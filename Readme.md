# PageDancer

A SECCOMP PageTable Dance Move for running trusted and untrusted code in the same process.


## Background

If you're a computer program, the only way you can affect the world around you is using what
is known as a *system call*. When you need to put some data on the disk drive or talk to a
computer on the internet or even find out what time it is, you have to do this ritual.
The SYSCALL instruction is like a Jekell and Hyde button, when you push it, everything goes
blank and you are transformed into the all powerful Kernel but with no memory of what you
were doing or why you had pushed it in the first place.

To resolve the superman with amnesia problem, the people who work on the Kernel made up a rule:
programs who want to make a syscall need to write a little note and leaves it in a specific
place before firing the SYSCALL instruction, the Kernel may not know where it is or why but
it's programmed so that when it awakes, it immedietly starts looking for a note.

The problem with syscalls is they allow you to do more or less anything. Some of the most common
and dangerous security exploits are based on tricking a program into executing a tiny piece
of code which execute a syscall which "opens the gates of Troy" so to speak. A program that can't
make syscalls is quite secure but not useful. It's like being paralized, blind deaf and dumb with
only the ability to think.


### SECCOMP

At some point, somebody decided there should be a way for programs to make rules for themselves
by declaring that there were certain syscalls they would never need, this way if they were
exploited the damage would be limited. What resulted was a sort of programmable firewall for
syscalls, allowing some and even limiting them based on the syscall parameters (what was written
in the note to the kernel). This firewall was of course loaded by ... a syscall :)

Unfortunately it was limited because while it's technically legal to change the rules after
they've been implemented, leaving the necessary syscall unblocked was a huge security hole
because untrusted code could use it to remove the rules altogether.


### Memory Protection

Since long long ago when computers lived at universities and were shared by many people,
microprocessors have had memory protection. Memory management in the processor deals with memory
in blocks, known as Pages. When a program tries to read or write memory inside of a Page which
the processor doesn't recognize, the processor triggers a fault. The fault handler is the Kernel
(remember him?) and he again wakes up, unsure of whether the fault was because of the process
doing something wrong or just because the processor has forgotten about the page, processors
have a short memory. If the page is **protected** the Kernel will review his notes and rather than
telling the processor that it does indeed exist, he will kill off the process with the dreaded
Segmentation Fault (the bane of the C programming apprentice).


## The Dance Move

The SECCOMP filter allows filtering based not only on the syscall parameters but also the
instruction pointer at the time of the syscall, IE where in the code the syscall was made from.
At first blush, this seems like a dream come true, "only this code can make a syscall".. But there
is a catch. Because of the way syscalls are made at the binary level, I can write my own
"note to the kernel" and then use a *jmp* instruction to jump directly on to your trusted syscall,
skipping the part where you wrote the correct note! However, when that syscall returns, it will
return into *your code*.

Furthermore, if the trusted SYSCALL instruction is in a **protected** memory page, I can't even
jump on top of it without triggering a hardware level fault and crashing (but you can't either).

PageDancer uses *two* special syscall locations, one of them is in a protected memory page and
the other is in a spot which is permitted to do one thing, unprotect the memory page. The code
following the syscall which unprotects the protected memory page calls the privileged section of
the program and and then re-protects the memory page before returning.

The privileged section of the code can use any syscall but only through the provided syscall
function, libc functions like `printf()` will trigger Bad System Call errors. Currently the
un-privileged code is not allowed to make *any* syscalls except one which re-protects the
protected memory page (not very interesting). Attempting to use the provided syscall function
will result in a Segmentation fault.


## Caviats

* The sandboxed code can crash the program (this should be obvious).
* Global variables are evil, any memory space which the privileged code relies on, it must
protect before entering the sandbox. Don't come crying to me when the sandboxed code replaces
`errno` with a mean tiger and it eats you.
* The sandboxed code can access the stack, and trash it. This is why the privileged code
*returns* into the sandbox instead of *calling* into it. If you're in the habit of writing
secrets on the stack, you're wize to clear them before entering sandboxed code.
* This code does not attempt to handle infinite loops in the sandbox. Your code will hang.
* This is binary baby! If the processor has a vulnerability, the sandboxed code will be able
to exploit it.


## Performance

Bad. A benchmark was developed using a number of pages which would be touched by each
sandboxed child before it returned, the pages were protected and the next child's pages would
be un-protected. This would run around a circle of *n* children *x* times. Compared to running
each child in it's own child process with it's own memory space, the performance was 21 times
worse using PageDancer.

```
user@toshitba:~/wrk/play/PageDancer$ time ./build_linux/PageDancer_benchmark 100 100 1000
[100] children -- [100] pages per child -- [1000] cycles

real	0m44.921s
user	0m6.523s
sys	0m38.383s
user@toshitba:~/wrk/play/PageDancer$ time ./build_linux/Benchmark_childProcesses 100 100 1000
[100] children -- [100] pages per child -- [1000] cycles

real	0m2.124s
user	0m0.014s
sys	0m0.028s
user@toshitba:~/wrk/play/PageDancer$ 
```

With protecting and un-protecting of the child pages disabled, the performance is comparable
to full process context switches. The reason for this is not entirely clear, `mprotect()`
syscall uses the `flush_tlb_range()` function which is intended to flush only the affected
page entries but if the processor implements `flush_tlb_range()` by flushing everything,
each `mprotect()` is as costly as a context switch.


```
user@toshitba:~/wrk/play/PageDancer$ time ./build_linux/Benchmark_childProcesses 100 100 1000
[100] children -- [100] pages per child -- [1000] cycles

real	0m2.118s
user	0m0.005s
sys	0m0.035s
```


