.code64
.align 8
.globl _start
_start:
        mov %r12, %rdi
        mov %r13, %rsi
        call usetup
        pop %rdi
        mov %rsp, %rsi
        // 16-byte align stack.  GCC requires this for SSE instructions.
        andq $~15, %rsp
        call main
        call exit
