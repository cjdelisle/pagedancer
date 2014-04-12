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

struct Context
{
    int argc;
    char** argv;
    int result;
    int done;
};

static void sandboxed(void* context, struct PageDancer* pd)
{
    struct Context* ctx = (struct Context*) context;
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

    if (ctx->done) {
        char buff[32] = {0};
        snprintf(buff, 31, "Result is: %d\n", ctx->result);
        pd->syscall(__NR_write, 1, buff, strlen(buff)+1);
        pd->syscall(__NR_exit, 0);
    }
    pd->nextCall = sandboxed;
    pd->nextCallContext = ctx;
    return;
}

int main(int argc, char** argv)
{
    struct Context ctx = {
        .argc = argc,
        .argv = argv
    };
    PageDancer_begin(privilegedMain, &ctx);
}
