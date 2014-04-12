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

struct PageDancer;

typedef long (* PageDancer_Syscall)(long num, ...);

typedef void (* PageDancer_Function)(void* context, struct PageDancer* pd);

struct PageDancer {
    PageDancer_Function nextCall;
    void* nextCallContext;
    int isInSandbox;
    PageDancer_Syscall syscall;
};

__attribute__((noreturn))
void PageDancer_begin(PageDancer_Function privilegedMain, void* privilegedContext);

#endif
