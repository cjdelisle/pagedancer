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
#include "PageDancer.h"
#include "Processor.h"
#include "Syscall.h"
#include "SeccompRules.h"

#define _GNU_SOURCE // memmem()
#include <string.h>

#include <stdio.h>
#include <sys/mman.h>
#include <assert.h>

struct PageDancer_Secure
{
    PageDancer_Function privilegedMain;
    void* mainContext;
    PageDancer_Syscall syscall;
};

struct PageDancer_pvt {
    struct PageDancer pub;
    unsigned long secZoneLen;
    struct PageDancer_Secure* secPd;
};

typedef void (* CallPrivileged)(struct PageDancer_pvt* pd);
static void callPrivileged(struct PageDancer_pvt* pd)
{
    long ret;
    struct PageDancer_Secure* secPd;

    // This is merely informational.
    pd->pub.isInSandbox = 0;

    Processor_INLINE_MEMPROTECT(pd->secPd,
                                pd->secZoneLen,
                                (PROT_READ | PROT_EXEC),
                                ret,
                                secPd);

    // note we use the returned secPd rather than the evil one passed to us!
    if (!ret) {
        pd->pub.syscall = secPd->syscall;
        secPd->privilegedMain(secPd->mainContext, &pd->pub);
    }

    for (;;) {

        // pd could be invalid, in which case this syscall will fail...
        Processor_INLINE_MEMPROTECT(secPd, pd->secZoneLen, PROT_NONE, ret, secPd);

        if (!ret) {
            pd->pub.isInSandbox = 1;
            return;
        }

        assert(0);
    }
}

#define MAX_SEARCH 2048

#define PageDancer_SEC_ZONE_SZ 2048

__attribute__((noreturn))
void PageDancer_begin(PageDancer_Function privilegedMain, void* privilegedContext)
{
    unsigned char* ptr = mmap(NULL,
                              PageDancer_SEC_ZONE_SZ,
                              PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | MAP_ANONYMOUS,
                              -1, 0);
    assert(ptr != MAP_FAILED);

    struct PageDancer_Secure* secContext = (struct PageDancer_Secure*) ptr;
    secContext->privilegedMain = privilegedMain;
    secContext->mainContext = privilegedContext;

    ptr = (unsigned char*) (&secContext[1]);

    memcpy(ptr, &Syscall_make, &Syscall_make_end - &Syscall_make);

    // can't cast between data and function pointers...
    union { PageDancer_Syscall func; void* data; } hax = { .data = ptr };
    secContext->syscall = hax.func;

    mprotect(ptr, PageDancer_SEC_ZONE_SZ, PROT_NONE);

    // This context is *NOT* protected, it can be arbitrarily modified!
    struct PageDancer_pvt pdp = {
        .secZoneLen = PageDancer_SEC_ZONE_SZ,
        .secPd = secContext
    };

    // warning: ISO C forbids ... between function pointer and ‘void *’ [-Wpedantic]
    union { CallPrivileged func; void* cast; } monkey = { .func = callPrivileged };

    unsigned char* unprotectLoc = memmem(monkey.cast,
                                         MAX_SEARCH,
                                         Processor_INLINE_MEMPROTECT_MARKER,
                                         Processor_INLINE_MEMPROTECT_MARKER_SZ);
    assert(unprotectLoc);
    unprotectLoc += Processor_INLINE_MEMPROTECT_MARKER_SZ;

    SeccompRules_install(unprotectLoc, (unsigned char*)secContext, PageDancer_SEC_ZONE_SZ);

    for (;;) {
        callPrivileged(&pdp);
        if (pdp.pub.nextCall) {
            pdp.pub.nextCall(pdp.pub.nextCallContext, &pdp.pub);
            pdp.pub.nextCall = NULL;
            pdp.pub.nextCallContext = NULL;
        }
    }
}
