.code64
.align 8
.globl _start
_start:
        pop %rdi                // argc
        mov %rsp, %rsi          // argv
        mov %r12, %rdx          // phdr
        mov %r13, %rcx          // phnum
        // 16-byte align stack.  GCC requires this for SSE instructions.
        andq $~15, %rsp
        call __crt_main
        // __crt_main shouldn't exit, but just in case
        call exit
