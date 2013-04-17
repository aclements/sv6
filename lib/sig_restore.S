.globl  sig_restore
sig_restore:
        add     $0xe, %rsp    // padding
        popw    %ax
        movw    %ax, %ds
        movw    %ax, %es

        pop     %r15
        pop     %r14
        pop     %r13
        pop     %r12
        pop     %rbp
        pop     %rbx
        pop     %r11
        pop     %r10
        pop     %r9
        pop     %r8
        pop     %rax
        pop     %rcx
        pop     %rdx
        pop     %rsi
        pop     %rdi

        add     $0x10, %rsp   // trapno, err, padding
        iretq

