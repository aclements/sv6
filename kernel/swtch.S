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

// Switch to executing on the current proc's kstack.
//
// Preconditions:
//  	1. Kernel GS in use
//      2. User page tables
.globl switch_to_kstack
switch_to_kstack:
		// Install kernel page tables
		movq    %cr3, %rax
	    andq    $(~0x1001), %rax
	    movq    %rax, %cr3

		// Load pointers to the top of the kstack and qstack.
		movq %gs:GS_PROC, %rax
		movq PROC_KSTACK(%rax), %rdi
		movq PROC_QSTACK(%rax), %rsi
		addq $(KSTACKSIZE), %rdi
		addq $(KSTACKSIZE), %rsi

		// Compute how much has been pushed onto the stack.
		movq %rsi, %rdx
		subq %rsp, %rdx

		// Jump to return if we are on the double-fault stack or NMI stack.
		cmpq $(KSTACKSIZE), %rdx
		ja 1f

		// Point both pointers to current position on the stack.
		subq %rdx, %rdi
		subq %rdx, %rsi

		// Page table swap means that the qstack virtual address range no longer
		// points to the stack that was previously in use. Adjust the pointer to
		// be into the direct map region associated with its physical memory.
		movabsq $(KBASE-QSTACKBASE), %rax
		addq %rax, %rsi

		call memcpy  // memcpy(dest=rdi, src=rsi, size=rdx);
		movq %rax, %rsp
1:		ret

