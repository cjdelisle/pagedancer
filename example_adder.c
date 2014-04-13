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
#include <assert.h>
#define __USE_MISC // MAP_ANONYMOUS
#include <sys/mman.h>

struct ChildContext
{
    int argc;
    char** argv;
    int result;
    int done;
};

struct Context
{
    struct ChildContext* cc;
};

static void sandboxed(void* context, struct PageDancer* pd)
{
    struct ChildContext* ctx = (struct ChildContext*) context;
    // pd->syscall(__NR_write, 1, "hax\n", 5); // segmentation fault
    // printf("hax\n"); // SIGSYS Bad System Call
    // callPrivileged(); // lose control of the execution.
    if (ctx->argc < 3) {
        ctx->result = -1;
        return;
    }
    int a = atoi(ctx->argv[1]);
    int b = atoi(ctx->argv[2]);
    ctx->result = a + b;
    ctx->done = 1;
}

static void privilegedMain(void* context, struct PageDancer* pd)
{
    struct Context* ctx = (struct Context*) context;

    if (ctx->cc->done) {
        char buff[32] = {0};
        snprintf(buff, 31, "Result is: %d\n", ctx->cc->result);
        pd->syscall(__NR_write, 1, buff, strlen(buff)+1);
        pd->syscall(__NR_exit, 0);
    }
    pd->nextCall = sandboxed;
    pd->nextCallContext = ctx->cc;
    return;
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        if (argc < 1) { return 100; }
        printf("usage: %s <number> <number>\n"
               "Adds the numbers together in the sandbox then returns to the\n"
               "privileged code and prints the result.\n",
               argv[0]);
        return 0;
    }
    struct Context* ctx = mmap(NULL,
                               sizeof(struct Context),
                               PROT_READ | PROT_WRITE,
                               MAP_PRIVATE | MAP_ANONYMOUS,
                               -1,
                               0);
    assert(ctx != MAP_FAILED);

    struct ChildContext* cc = mmap(NULL,
                                   sizeof(struct ChildContext),
                                   PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANONYMOUS,
                                   -1,
                                   0);
    assert(cc != MAP_FAILED);

    cc->argc = argc;
    cc->argv = argv;
    ctx->cc = cc;

    PageDancer_begin(privilegedMain, ctx, sizeof(struct Context));
}
