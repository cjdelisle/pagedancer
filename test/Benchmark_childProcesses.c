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
#include <sys/types.h>
#include <sys/wait.h>
#define __USE_MISC
#include <sys/mman.h>

struct Page
{
    uint8_t bytes[4096];
};

static void usage(char* appName)
{
    printf("Usage: %s <childCount> <pagesPerChild> <cycles>\n"
           "          Create <childCount> child processes\n"
           "          and for each child, allocate <pagesPerChild> pages of memory\n"
           "          and begin attempting to read from a pipe.\n"
           "          The parent will write to the first pipe to awaken the first child\n"
           "          who will access all of his pages and then write to the next pipe\n"
           "          to awaken his sibling. This will go around in a loop until it\n"
           "          reaches the parent again who will repeat the process <cycles> times.\n",
           appName);
}

static void child(int readFrom, int writeTo, int pagesPerChild, int cycles)
{
    struct Page** page = malloc(sizeof(uintptr_t) * pagesPerChild);
    assert(page);
    for (int i = 0; i < pagesPerChild; i++) {
        page[i] = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(page[i] != MAP_FAILED);

        // Place a guard page in the middle to discourage the kernel from merging
        // all of the pages into a hugepage.
        mmap(((char*)page[i]) + 4096, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        //if (readFrom == 3) { printf("%lx\n", (unsigned long)page[i]); }
    }

    for (int i = 0; i < cycles; i++) {
        unsigned char seed = 0;
        int ret;
        while ((ret = read(readFrom, &seed, 1)) == 0) ;
        assert(ret == 1);
        for (int j = 0; j < pagesPerChild; j++) {
            seed ^= ((unsigned char*) page[j])[0];
            ((unsigned char*) page[j])[0] += seed;
        }
        while (write(writeTo, &seed, 1) == 0) ;
        assert(ret == 1);
    }
}

static void closePipes(int* pipes, int pipeCount, int readFrom, int writeTo)
{
    for (int j = 0; j < pipeCount; j++) {
        if (pipes[j] != readFrom && pipes[j] != writeTo) { close(pipes[j]); }
    }
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

    int* pipes = malloc(sizeof(int) * childCount * 2);
    for (unsigned long i = 0; i < childCount * 2; i+= 2) {
        assert(!pipe(&pipes[i]));
    }

    int pid = 0;
    unsigned long readFromIdx = 0;
    unsigned long writeToIdx = (childCount * 2) - 1;
    for (unsigned long i = 0; i < childCount; i++) {

        int writeTo = pipes[writeToIdx];
        int readFrom = pipes[readFromIdx];

        pid = fork();
        assert(pid >= 0);
        if (!pid) {
            closePipes(pipes, childCount * 2, readFrom, writeTo);
            child(readFrom, writeTo, pagesPerChild, cycles);
            return 0;
        }
        readFromIdx += 2;
        writeToIdx += 2;
        if (writeToIdx > (childCount * 2)) { writeToIdx = 1; }
    }

    // parent only
    unsigned long seed = getpid();
    write(pipes[1], &seed, 1);

    waitpid(pid, NULL, 0);
}
