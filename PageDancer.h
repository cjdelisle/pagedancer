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
#ifndef PageDancer_H
#define PageDancer_H

#include "cjd/util/Linker.h"
Linker_require("PageDancer.c")

struct PageDancer;

typedef long (* PageDancer_Syscall)(long num, ...);

typedef void (* PageDancer_Function)(void* context, struct PageDancer* pd);

struct PageDancer {
    PageDancer_Function nextCall;
    void* nextCallContext;
    int isInSandbox;
    PageDancer_Syscall syscall;
};

/**
 * Begin the dance.
 *
 * Never returns, the stack is not safe after it has been exposed to the untrusteds code.
 *
 * @param privilegedMain This will be called immedietly and passed the mainContext
 *                       and the PageDancer object which provides access to the privileged
 *                       syscall function.
 * @param mainContext The context which will be supplied back to the privilegedMain function.
 *                    The PageDancer must be able to protect this context.
 *                    If you pass a value on the stack, hilariously undefined behavior will result!
 */
__attribute__((noreturn))
void PageDancer_begin(PageDancer_Function privilegedMain,
                      void* mainContext,
                      unsigned long mainContextLength);

#endif
