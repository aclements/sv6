#include "mmu.h"
#include "asmdefines.h"

#define ENTRY(name) .globl name ; .balign 8; name :

// We aren't allowed to touch rbx,rsp,rbp,r12-r15
        
.code64
// rdi user src
// rsi kernel dst
ENTRY(__uaccess_int64)
        lfence
        push    %rbp            // For stack traces
        mov     %rsp, %rbp

        mov     %gs:GS_PROC, %r11
        movl    $1, PROC_UACCESS(%r11)
        mov     (%rdi), %r10
        mov     %r10, (%rsi)
        mov     $0, %rax
        jmp     __uaccess_end

// rdi dst
// rsi src
// rdx dst len
ENTRY(__uaccess_str)
        lfence
        push    %rbp            // For stack traces
        mov     %rsp, %rbp

        mov     %gs:GS_PROC, %r11
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

// rdi user src
// rsi max len
// Returns pointer to NUL or (char*)-1 on failure
ENTRY(__uaccess_strend)
        lfence
        push    %rbp            // For stack traces
        mov     %rsp, %rbp

        mov     %gs:GS_PROC, %r11
        movl    $1, PROC_UACCESS(%r11)

        // %rcx limits scas
        mov     %rsi, %rcx
        // Search for NUL
        xor     %rax, %rax
        // Scan string
        repne   scasb
        jne     2f
        // RDI points one past the NUL
        mov     %rdi, %rax
        dec     %rax
        jmp     __uaccess_end
2:
        // No NUL found
        movq    $-1, %rax
        jmp     __uaccess_end

// rdi dst
// rsi src
// rdx len
ENTRY(__uaccess_mem)
        lfence
        push    %rbp            // For stack traces
        mov     %rsp, %rbp

        mov     %gs:GS_PROC, %r11
        movl    $1, PROC_UACCESS(%r11)

        // %rcx is rep counter
        mov     %rdx, %rcx
        xor     %rax, %rax

        // Copy
        rep movsb

        // Done
        jmp     __uaccess_end
        
.globl __uaccess_end
.balign 8
__uaccess_end:
        movl $0, PROC_UACCESS(%r11)
        pop     %rbp
        ret
