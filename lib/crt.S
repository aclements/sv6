.code64
.align 8
.globl _start
_start:
        mov %r12, %rdi
        mov %r13, %rsi
        call usetup
        pop %rdi
        mov %rsp, %rsi
        call main
        call exit
