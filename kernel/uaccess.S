#include "mmu.h"
#include "asmdefines.h"

#define ENTRY(name) .globl name ; .align 8; name :

// We aren't allowed to touch rbx,rsp,rbp,r12-r15
        
.code64
// rdi user src
// rsi kernel dst
ENTRY(__uaccess_int64)
        mov     %gs:0x8, %r11
        movl    $1, PROC_UACCESS(%r11)
        mov     (%rdi), %r10
        mov     %r10, (%rsi)
        mov     $0, %rax
        jmp     __uaccess_end

// rdi dst
// rsi src
// rdx dst len
ENTRY(__uaccess_str)
        mov     %gs:0x8, %r11
        movl    $1, PROC_UACCESS(%r11)

        // %rcx is loop instruction counter
        mov     %rdx, %rcx
        xor     %rax, %rax
1:   
        movb    (%rsi), %r10b
        movb    %r10b, (%rdi)
        // Check for NULL
        cmp     $0, %r10b
        je      2f
        inc     %rdi
        inc     %rsi
        loop    1b
        // Error
        movq    $-1, %rax
2:      // Done
        jmp     __uaccess_end

// rdi dst
// rsi src
// rdx len
ENTRY(__uaccess_mem)
        mov     %gs:0x8, %r11
        movl    $1, PROC_UACCESS(%r11)

        // %rcx is loop instruction counter
        mov     %rdx, %rcx
        xor     %rax, %rax
1:   
        movb    (%rsi), %r10b
        movb    %r10b, (%rdi)
        inc     %rdi
        inc     %rsi
        loop    1b
        // Done
        jmp     __uaccess_end
        
.globl __uaccess_end
.align 8
__uaccess_end:
        movl $0, PROC_UACCESS(%r11)
        ret

.globl zpage
.align 8
zpage:
        mov     $4096/0x40, %rcx
        mov     $0, %rax
1:
        mov     %rax, (%rdi)
        mov     %rax, 0x8(%rdi)
        mov     %rax, 0x10(%rdi)
        mov     %rax, 0x18(%rdi)
        mov     %rax, 0x20(%rdi)
        mov     %rax, 0x28(%rdi)
        mov     %rax, 0x30(%rdi)
        mov     %rax, 0x38(%rdi)
        lea     0x40(%rdi), %rdi
        loop    1b
        ret

.globl zpage_nc
.align 8
zpage_nc:
        mov     $4096/0x40, %rcx
        mov     $0, %rax
1:
        movnti  %rax, (%rdi)
        movnti  %rax, 0x8(%rdi)
        movnti  %rax, 0x10(%rdi)
        movnti  %rax, 0x18(%rdi)
        movnti  %rax, 0x20(%rdi)
        movnti  %rax, 0x28(%rdi)
        movnti  %rax, 0x30(%rdi)
        movnti  %rax, 0x38(%rdi)
        lea     0x40(%rdi), %rdi
        loop    1b
        sfence
        ret
