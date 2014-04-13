/* vim: set expandtab ts=4 sw=4: */
/*
 * You may redistribute this program and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "Processor.h"

#include <linux/seccomp.h>
#include <linux/filter.h>
#include <sys/prctl.h>
#include <sys/mman.h>
#include <stddef.h> // offsetof
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

void SeccompRules_install(unsigned char* unprotectLoc,
                          unsigned char* secZone,
                          unsigned long secZoneLen)
{
    unsigned long unprotectSecZone = (PROT_READ | PROT_EXEC);
    unsigned long protectSecZone = PROT_NONE;

    #define SECCOMP_FILTER_FAIL SECCOMP_RET_TRAP

    #define JMPINS(type, not, input, num) \
        BPF_JUMP(BPF_JMP+(type)+BPF_K, (input), (!(not) ? (num) : 0), ((not) ? (num) : 0) )

    #define LOAD(offset)    BPF_STMT(BPF_LD+BPF_W+BPF_ABS, (offset))
    #define RET(val)        BPF_STMT(BPF_RET+BPF_K, (val))

    #define JEQ(input, num) JMPINS(BPF_JEQ, 0, (input), (num))
    #define JNE(input, num) JMPINS(BPF_JEQ, 1, (input), (num))

    #define JGT(input, num) JMPINS(BPF_JGT, 0, (input), (num))
    #define JGE(input, num) JMPINS(BPF_JGE, 0, (input), (num))
    #define JLT(input, num) JMPINS(BPF_JGE, 1, (input), (num))
    #define JLE(input, num) JMPINS(BPF_JGT, 1, (input), (num))

    #define FNE(input) JEQ(input, 1), RET(SECCOMP_FILTER_FAIL)
    #define SEQ(input) JNE(input, 1), RET(SECCOMP_RET_ALLOW)
    #define FGT(input) JLE(input, 1), RET(SECCOMP_FILTER_FAIL)
    #define FLT(input) JGE(input, 1), RET(SECCOMP_FILTER_FAIL)

    struct sock_filter seccompFilter[] = {

        // verify the processor type is the same as what we're setup for.
        LOAD(offsetof(struct seccomp_data, arch)),
        FNE(Processor_SECCOMP_ARCH),

        LOAD(offsetof(struct seccomp_data, instruction_pointer)),
        JLT((long)secZone, 2),
        JGT(((long)secZone + secZoneLen), 1),
        RET(SECCOMP_RET_ALLOW),

        // protecting and unprotecting the secure space.
        LOAD(offsetof(struct seccomp_data, nr)),
        FNE(__NR_mprotect), // if (nr != __NR_mprotect) { fail }

        LOAD(offsetof(struct seccomp_data, args[0])),
        FNE((long)secZone), // if (args[0] != secZone) { fail }

        LOAD(offsetof(struct seccomp_data, args[1])),
        FNE(secZoneLen), // if (args[1] != secZoneEnd) { fail }

        LOAD(offsetof(struct seccomp_data, args[2])),
        SEQ(protectSecZone), // if (args[2] == protectSecZone) { success, anyone can do this }

        FNE(unprotectSecZone), // if (args[2] != unprotectSecZone) { fail }

        LOAD(offsetof(struct seccomp_data, instruction_pointer)),
        FNE((long)unprotectLoc), // if (ip != unprotectLoc) { fail }

        RET(SECCOMP_RET_ALLOW)
    };

    struct sock_fprog seccompProgram = {
        .len = (unsigned short)(sizeof(seccompFilter)/sizeof(seccompFilter[0])),
        .filter = seccompFilter,
    };

    #ifdef DEBUG
        printf("unprotectLoc [%lx] secZone [%lx] secZoneLen [%ld]\n",
               (unsigned long) unprotectLoc,
               (unsigned long) secZone,
               (unsigned long) secZoneLen);
    #endif

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == -1) {
        // don't worry about it.
        printf("prctl(PR_SET_NO_NEW_PRIVS) -> [%s]\n", strerror(errno));
    }
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &seccompProgram) == -1) {
        printf("prctl(PR_SET_SECCOMP) -> [%s]\n", strerror(errno));
        abort();
    }
}
