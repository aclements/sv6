#include "asmdefines.h"
#include "memlayout.h"

# Context switch
#
#   void swtch(struct context **old, struct context *new);
# 
# Save current register context in old
# and then load register context from new.
# %rdi holds old, %rsi holds new.
.globl swtch
swtch:
        pushq %rbx
        pushq %rbp
        pushq %r12
        pushq %r13
        pushq %r14
        pushq %r15

        movq %rsp, (%rdi)
        movq %rsi, %rsp

        popq %r15
        popq %r14
        popq %r13
        popq %r12
        popq %rbp
        popq %rbx
        ret

.globl threadstub
threadstub:
        movq %r12, %rdi
        movq %r13, %rsi
        jmp threadhelper

.globl switch_to_kstack
switch_to_kstack:
		// %rdx = myproc()->qstack+(KSTACKSIZE-PGSIZE)
		movq %gs:GS_PROC_OFFSET, %rdx
		movq PROC_QSTACK_OFFSET(%rdx), %rdx
		addq $(KSTACKSIZE), %rdx
		subq %rsp, %rdx
		cmpq $(KSTACKSIZE), %rdx
		ja 1f

		movq %rsp, %rsi
		movabsq $(KBASE-QSTACKBASE), %rdi
		addq %rdi, %rsi

		movq %gs:GS_PROC_OFFSET, %rdi  // myproc()
		movq PROC_KSTACK_OFFSET(%rdi), %rdi
		addq $(KSTACKSIZE), %rdi
		subq %rdx, %rdi

		subq $8, %rdx
		addq $8, %rsi
		addq $8, %rdi

		call memcpy  // memcpy(dest=rdi, src=rsi, size=rdx);
		movq %rax, %rsp
1:		ret

