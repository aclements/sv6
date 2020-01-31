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

.globl swtch_and_barrier
swtch_and_barrier:
        pushq %rbx
        pushq %rbp
        pushq %r12
        pushq %r13
        pushq %r14
        pushq %r15
        movq %rsp, (%rdi)

        movq %cr3, %rbx
        andq $(~0x1001), %rbx
        andq %gs:GS_CR3_MASK, %rbx
        movq %rbx, %cr3

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

// Switch to executing on the current proc's kstack.
//
// Preconditions:
//  	1. Kernel GS in use
//      2. User page tables
.globl switch_to_kstack
switch_to_kstack:
		cmpb $0, (secrets_mapped)
		jne 1f

		// Install kernel page tables
		movq %cr3, %rax
	    andq $(~0x1001), %rax
	    andq %gs:GS_CR3_MASK, %rax
	    movq %rax, %cr3

		// Load pointers to the top of the kstack and qstack.
		movq %gs:GS_PROC, %rax
		movq PROC_KSTACK(%rax), %rdi
		movq PROC_QSTACK(%rax), %rsi
		addq $(KSTACKSIZE), %rdi
		addq $(KSTACKSIZE), %rsi

		// Compute how much has been pushed onto the stack.
		movq %rdi, %rdx
		subq %rsp, %rdx

		// Jump to return if we are on the double-fault stack or NMI stack.
		// TODO: Shouldn't we still copy the stack for a NMI? Perhaps we need to copy the *entire* stack.
		cmpq $(KSTACKSIZE), %rdx
		ja 1f

		// Point both pointers to current position on the stack.
		subq %rdx, %rdi
		subq %rdx, %rsi

		call memcpy  // memcpy(dest=rdi, src=rsi, size=rdx);

1:		ret

