.globl  setjmp
setjmp:
        pop     %rax    // %rip pushed by call
        mov     %rax, 0x00(%rdi)
        mov     %rsp, 0x08(%rdi)
        mov     %rbp, 0x10(%rdi)
        mov     %rbx, 0x18(%rdi)
        mov     %r12, 0x20(%rdi)
        mov     %r13, 0x28(%rdi)
        mov     %r14, 0x30(%rdi)
        mov     %r15, 0x38(%rdi)

        push    %rax
        mov     $0, %rax
        ret

.globl  longjmp
longjmp:
        mov     0x00(%rdi), %rax  // %rip
        mov     0x08(%rdi), %rsp
        mov     0x10(%rdi), %rbp
        mov     0x18(%rdi), %rbx
        mov     0x20(%rdi), %r12
        mov     0x28(%rdi), %r13
        mov     0x30(%rdi), %r14
        mov     0x38(%rdi), %r15

        push    %rax            // push %rip for ret
        mov     %rsi, %rax
        ret

.globl  sigsetjmp
sigsetjmp:
        jmp     setjmp

.globl  siglongjmp
siglongjmp:
        jmp     longjmp

