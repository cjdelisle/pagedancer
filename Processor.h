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
#ifndef Processor_GLUE
    #define Processor_GLUE(a,b) Processor__GLUE(a,b)
    #define Processor__GLUE(a,b) Processor___GLUE(a,b)
    #define Processor___GLUE(a,b) a ## b
#endif

#if defined(__amd64__) || \
    defined(__x86_64__) || \
    defined(__AMD64__) || \
    defined(_M_X64) || \
    defined(__amd64)

    #include "Processor_amd64.h"

#elif defined(__i386__) || \
      defined(__x86__) || \
      defined(__X86__) || \
      defined(_M_IX86) || \
      defined(__i386)

    #error i386 is not supported

#elif defined(__ARM_EABI__)

    #error ARM EABI is not supported

#elif defined(__arm__)

    #error ARM OABI is not supported

#elif defined(__mips__) || defined(__mips) || defined(__MIPS__)
    #if defined(_ABIO32)
        #error mipso32 is not supported
    #elif defined(_ABIN32)
        #error mips32 is not supported
    #else
        #error mips64 is not supported
    #endif
#else

    #error "no supporting Processor.h for your processor architecture"

#endif
