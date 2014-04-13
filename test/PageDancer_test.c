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
#include "test/Syscalls.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>

#define __USE_MISC
#include <sys/mman.h>

typedef void (* TestFunc)(void* context, struct PageDancer* pd);

enum Exit {
    Exit_SEGFAULT,
    Exit_SIGSYS,
    Exit_OK,
    Exit_OTHER,
    Exit_OTHERSIG,
    Exit_ERRCODE
};

struct TestCase
{
    TestFunc func;
    char* name;
    enum Exit expect;
};

struct ChildContext
{
    int retVal;
};

struct Context
{
    const struct TestCase* can;
    struct ChildContext* cc;
};

static void basic(void* context, struct PageDancer* pd)
{
    struct ChildContext* ctx = (struct ChildContext*) context;
    ctx->retVal = 0;
}

// Expect an error code return because this doesn't set retVal to 0.
static void doNothing(void* context, struct PageDancer* pd)
{
}

// sigsys
static void attemtDirectSyscall(void* context, struct PageDancer* pd)
{
    printf("hax\n");
}

// segfault
static void attemptPdSyscall(void* context, struct PageDancer* pd)
{
    pd->syscall(__NR_write, 1, "hax\n", 5);
}

static const struct TestCase TEST_CASES[] = {
    { .func = basic, .name = "basic", .expect = Exit_OK },
    { .func = doNothing, .name = "doNothing", .expect = Exit_ERRCODE },
    { .func = attemtDirectSyscall, .name = "attemtDirectSyscall", .expect = Exit_SIGSYS },
    { .func = attemptPdSyscall, .name = "attemptPdSyscall", .expect = Exit_SEGFAULT },
};

static enum Exit getExitCause(int status)
{
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status)) { return Exit_ERRCODE; }
        return Exit_OK;
    }
    if (WIFSIGNALED(status)) {
        if (WTERMSIG(status) == SIGSEGV) { return Exit_SEGFAULT; }
        if (WTERMSIG(status) == SIGSYS) { return Exit_SIGSYS; }
        return Exit_OTHERSIG;
    }
    return Exit_OTHER;
}

static const char* EXIT_CAUSE_STR[] = {
    "Exit_SEGFAULT",
    "Exit_SIGSYS",
    "Exit_OK",
    "Exit_OTHER",
    "Exit_OTHERSIG",
    "Exit_ERRCODE"
};

static void privilegedMain(void* context, struct PageDancer* pd)
{
    struct Context* ctx = (struct Context*) context;
    if (ctx->can) {
        Syscalls_exit(pd, ctx->cc->retVal);
    }
    for (int i = 0; i < (int)(sizeof(TEST_CASES) / sizeof(*TEST_CASES)); i++) {
        ctx->can = &TEST_CASES[i];
        long pid = pd->syscall(__NR_fork);
        if (pid < 0) {
            Syscalls_printf(pd, "fork() -> %d\n", pid);
            Syscalls_abort(pd);
        }
        if (!pid) {
            pd->nextCall = ctx->can->func;
            pd->nextCallContext = ctx->cc;
            return;
        } else {
            Syscalls_printf(pd, "[%d] %s running\n", pid, ctx->can->name);
            int status;
            int ret = Syscalls_wait4(pd, pid, &status, 0, NULL);
            if (ret < 0) {
                Syscalls_printf(pd, "wait4() -> %d\n", ret);
                Syscalls_abort(pd);
            }
            enum Exit ex = getExitCause(status);
            Syscalls_printf(pd, "[%d] %s PASS [%s]\n", pid, ctx->can->name, EXIT_CAUSE_STR[ex]);
            if (ex != ctx->can->expect) {
                Syscalls_printf(pd, "%s FAIL [%s] expected [%s]\n",
                        ctx->can->name, EXIT_CAUSE_STR[ex], EXIT_CAUSE_STR[ctx->can->expect]);
            }
        }
    }
    Syscalls_exit(pd, 0);
}

int main()
{
    struct Context* ctx = mmap(NULL,
                               sizeof(struct Context),
                               PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS,
                               -1,
                               0);
    assert(ctx != MAP_FAILED);

    struct ChildContext* childCtx = mmap(NULL,
                                         sizeof(struct ChildContext),
                                         PROT_READ | PROT_WRITE,
                                         MAP_PRIVATE | MAP_ANONYMOUS,
                                         -1,
                                         0);
    assert(childCtx != MAP_FAILED);
    childCtx->retVal = -1;
    ctx->cc = childCtx;

    PageDancer_begin(privilegedMain, ctx, sizeof(struct Context));
}
