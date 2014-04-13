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
#include <stdint.h>
#include <assert.h>
#include <unistd.h>
#define __USE_MISC
#include <sys/mman.h>

struct Page
{
    uint8_t bytes[4096];
};

struct Child
{
    unsigned long seed;

    long pageCount;

    // This runs off the end of the structure.
    struct Page* pages[1];
};

struct Context {
    unsigned long childCount;
    unsigned long pagesPerChild;
    unsigned long cycles;

    unsigned long childNum;
    unsigned long cycle;

    /** Does not need to be random but cannot be a compile time constant. */
    unsigned long seed;

    // This runs off the end of the struct
    struct Child* children[4];
};

static void sandboxed(void* vchild, struct PageDancer* pd)
{
    struct Child* child = (struct Child*) vchild;
    unsigned long seed = child->seed;
    for (int i = 0; i < child->pageCount; i++) {
        seed ^= ((long*) child->pages[i])[0];
        ((long*) child->pages[i])[0] += seed;
    }
}

static void unlockPage(struct PageDancer* pd, struct Page* page)
{
    if (pd->syscall(__NR_mprotect, page, 4096, (PROT_READ | PROT_WRITE))) {
        Syscalls_abort(pd);
    }
}

static void lockPage(struct PageDancer* pd, struct Page* page)
{
    if (pd->syscall(__NR_mprotect, page, 4096, PROT_NONE)) {
        Syscalls_abort(pd);
    }
}

// Here's how it works:
// When we wake up a child, we give it two numbers, the first number is the index where he should
// *write* a value to each page, the second number is an index where he should read.
// This should foil any attempts by the compiler to "optimize" the benchmark, breaking it.
static void privilegedMain(void* context, struct PageDancer* pd)
{
    struct Context* ctx = (struct Context*) context;

    if (ctx->cycle >= ctx->cycles) {
        Syscalls_exit(pd, 0);
    }

    ctx->seed += ctx->children[ctx->childNum]->seed;

    // lock the previous child
    for (unsigned long i = 0; i < ctx->pagesPerChild; i++) {
        lockPage(pd, ctx->children[ctx->childNum]->pages[i]);
    }
    lockPage(pd, (struct Page*)ctx->children[ctx->childNum]);

    ctx->childNum = (ctx->childNum + 1) % ctx->childCount;
    if (!ctx->childNum) {
        ctx->cycle++;
        //Syscalls_printf(pd, "cycle [%d]\n", ctx->cycle);
    }

    // unlock the next one
    unlockPage(pd, (struct Page*)ctx->children[ctx->childNum]);
    for (unsigned long i = 0; i < ctx->pagesPerChild; i++) {
        unlockPage(pd, ctx->children[ctx->childNum]->pages[i]);
    }

    ctx->children[ctx->childNum]->seed = ctx->seed;

    pd->nextCall = sandboxed;
    pd->nextCallContext = ctx->children[ctx->childNum];
    return;
}

static void usage(char* appName)
{
    printf("Usage: %s <childCount> <pagesPerChild> <cycles>\n"
           "          Create <childCount> sandboxed children in the process\n"
           "          and for each child, allocate <pagesPerChild> pages of memory\n"
           "          then switch to each child in turn and access each page in the\n"
           "          child's page set before proceeding to the next child\n"
           "          Repeat for every child going around in a circle <cycles> times.\n",
           appName);
}

int main(int argc, char** argv)
{
    unsigned int childCount = 100;
    unsigned int pagesPerChild = 100;
    unsigned int cycles = 100;

    if (argc < 4) {
        usage(argv[0]);
        printf("\nUsing defaults\n");
    } else {
        childCount = atoi(argv[1]);
        pagesPerChild = atoi(argv[2]);
        cycles = atoi(argv[3]);
    }

    if (!(childCount && pagesPerChild && cycles)) {
        usage(argv[0]);
        return 100;
    }

    printf("[%d] children -- [%d] pages per child -- [%d] cycles\n",
           childCount, pagesPerChild, cycles);

    unsigned long ctxSize = sizeof(struct Context) + (sizeof(uintptr_t) * childCount);
    struct Context* ctx =
        mmap(NULL, ctxSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(ctx != MAP_FAILED);

    ctx->pagesPerChild = pagesPerChild;
    ctx->childCount = childCount;
    ctx->cycles = cycles;
    ctx->seed = getpid();

    for (uintptr_t i = 0; i < pagesPerChild+1; i++) {
        for (uintptr_t j = 0; j < childCount; j++) {
            struct Page* page =
                mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            assert(page != MAP_FAILED);
            if (i == 0) {
                ctx->children[j] = (struct Child*) page;
                ctx->children[j]->pageCount = pagesPerChild;
            } else {
                ctx->children[j]->pages[i-1] = page;
                // protect the page, skip protecting for the first child.
                assert(!j || !mprotect(page, 4096, PROT_NONE));
            }
            if (i == pagesPerChild) {
                // protect the bottom page.
                assert(!j || !mprotect(ctx->children[j], 4096, PROT_NONE));
            }
        }
    }

    PageDancer_begin(privilegedMain, ctx, ctxSize);
}
