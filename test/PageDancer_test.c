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

#include <stdlib.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>

typedef void (* TestFunc)(void* context, struct PageDancer* pd);

struct TestCase
{
    TestFunc func;
    char* name;
};

struct Context
{
    const struct TestCase* can;
    int retVal;
};

static void basic(void* context, struct PageDancer* pd)
{
    struct Context* ctx = (struct Context*) context;
    ctx->retVal = 1;
}

static void attemtDirectSyscall(void* context, struct PageDancer* pd)
{
    printf("hax\n"); // sigsys
}

static void attemptPdSyscall(void* context, struct PageDancer* pd)
{
    pd->syscall(__NR_write, 1, "hax\n", 5); // segmentation fault
}

static const struct TestCase TEST_CASES[] = {
    { .func = basic, .name = "basic" },
    { .func = attemtDirectSyscall, .name = "attemtDirectSyscall" },
    { .func = attemptPdSyscall, .name = "attemptPdSyscall" },
};

static void xprintf(struct PageDancer* pd, const char* format, ...)
{
    char buff[1024] = {0};
    va_list args;
    va_start(args, format);
    vsnprintf(buff, 1023, format, args);
    va_end(args);
    pd->syscall(__NR_write, 1, buff, strlen(buff)+1);
}

static void xexit(struct PageDancer* pd, int num)
{
    pd->syscall(__NR_exit, num);
}

__attribute__((noreturn))
static void xabort(struct PageDancer* pd)
{
    long pid = pd->syscall(__NR_gettid);
    pd->syscall(__NR_tgkill, pid, pid, SIGABRT);
    for (;;) {
        ((char*)0)[0] = 0;
    }
}

static void privilegedMain(void* context, struct PageDancer* pd)
{
    struct Context* ctx = (struct Context*) context;
    if (ctx->can) {
        xprintf(pd, "%s %s\n", ctx->can->name, ctx->retVal ? "SUCCESS" : "FAIL");
        xexit(pd, 0);
    }
    for (int i = 0; i < (int)(sizeof(TEST_CASES) / sizeof(*TEST_CASES)); i++) {
        int pid = pd->syscall(__NR_fork);
        if (pid < 0) {
            xprintf(pd, "fork() -> %d\n", pid);
            xabort(pd);
        }
        if (!pid) {
            ctx->can = &TEST_CASES[i];
            pd->nextCall = TEST_CASES[i].func;
            xprintf(pd, "%s running\n", ctx->can->name);
            pd->nextCallContext = ctx;
            return;
        }
    }
    xexit(pd, 0);
}

int main()
{
    struct Context ctx = { .can = NULL };
    PageDancer_begin(privilegedMain, &ctx);
}
