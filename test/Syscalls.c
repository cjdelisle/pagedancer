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

#include <sys/syscall.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

void Syscalls_printf(struct PageDancer* pd, const char* format, ...)
{
    char buff[1024] = {0};
    va_list args;
    va_start(args, format);
    vsnprintf(buff, 1023, format, args);
    va_end(args);
    pd->syscall(__NR_write, 2, buff, strlen(buff)+1);
}

void Syscalls_exit(struct PageDancer* pd, int num)
{
    pd->syscall(__NR_exit, num);
}

__attribute__((noreturn))
void Syscalls_abort(struct PageDancer* pd)
{
    long pid = pd->syscall(__NR_gettid);
    pd->syscall(__NR_tgkill, pid, pid, SIGABRT);
    for (;;) {
        ((char*)0)[0] = 0;
    }
}

int Syscalls_wait4(struct PageDancer* pd, int pid, int* status, int options, void* rusage)
{
    return pd->syscall(__NR_wait4, pid, status, options, rusage);
}
