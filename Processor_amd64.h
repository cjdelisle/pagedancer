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
#ifndef PageDancer_amd64_H
#define PageDancer_amd64_H

// All processor specific code should be here.

#ifndef __ASSEMBLER__
    #include <sys/syscall.h>
    #include <linux/audit.h>
#endif

#define Processor_SECCOMP_ARCH AUDIT_ARCH_X86_64

// AMD64 syscall:
// syscall num and return value: %rax
// arguments: %rdi, %rsi, %rdx, %rcx, %r8, %r9
// return value in %rax
// %rcx and %r11 are trashed by the kernel.

// pet names for registers in inline ASM
#define Processor_RAX(input) "a"(input)
#define Processor_RBX(input) "b"(input)
#define Processor_RCX(input) "c"(input)
#define Processor_RDX(input) "d"(input)
#define Processor_RSI(input) "S"(input)
#define Processor_RDI(input) "D"(input)

#define Processor_INLINE_MEMPROTECT_MARKER "\x48\xb9\xbe\xba\xfe\xca\xef\xbe\xad\xde\x0f\x05"
#define Processor_INLINE_MEMPROTECT_MARKER_SZ (sizeof(Processor_INLINE_MEMPROTECT_MARKER)-1)

#define Processor_INLINE_MEMPROTECT(mem, len, prot, ret, memOut) \
    __asm__ (                                                    \
        "mov $0xdeadbeefcafebabe, %%rcx\n\t"                     \
        "syscall"                                                \
                                                                 \
        : "=" Processor_RAX(ret),                                \
          "=" Processor_RDI(memOut)                              \
                                                                 \
        : Processor_RAX(__NR_mprotect),                          \
          Processor_RDI(mem),                                    \
          Processor_RSI(len),                                    \
          Processor_RDX(prot)                                    \
                                                                 \
        : "%rcx",                                                \
          "%r11"                                                 \
    )

#endif

// The remainder of the file is meant to be called many times.

#ifdef Processor_ASM_INIT
    .section .text
    #undef Processor_ASM_INIT
#endif
#ifdef Processor_ASM_FINI
    #undef Processor_ASM_FINI
#endif


// Syscall.S:
// #define Processor_CREATE_SYSCALL_FUNC syscall
// #include "Processor.h"
//
//.p2align 5
#if defined(Processor_MKSYSCALL_NAME) && defined(Processor_MKSYSCALL_PARAMS)
    .globl Processor_MKSYSCALL_NAME
    .globl Processor_GLUE(Processor_MKSYSCALL_NAME, _end)
    Processor_MKSYSCALL_NAME:
        #ifndef Processor_MKSYSCALL_NR
            mov %rdi, %rax
        #endif
        #if (Processor_MKSYSCALL_PARAMS > 0)
            mov %rsi, %rdi
        #endif
        #if (Processor_MKSYSCALL_PARAMS > 1)
            mov %rdx, %rsi
        #endif
        #if (Processor_MKSYSCALL_PARAMS > 2)
            mov %rcx, %rdx
        #endif
        #if (Processor_MKSYSCALL_PARAMS > 3)
            mov %r8,  %r10
        #endif
        #if (Processor_MKSYSCALL_PARAMS > 4)
            mov %r9,  %r8
        #endif
        #if (Processor_MKSYSCALL_PARAMS > 5)
            mov 0x8(%rsp), %r9
        #endif
        #ifdef Processor_MKSYSCALL_NR
            mov $Processor_MKSYSCALL_NR, %rax
        #endif
        syscall
    retq
    Processor_GLUE(Processor_MKSYSCALL_NAME, _end):

    #undef Processor_MKSYSCALL_NAME
    #undef Processor_MKSYSCALL_NR
    #undef Processor_MKSYSCALL_PARAMS
#endif
