#include "mmu.h"
#include "asmdefines.h"
#include "memlayout.h"

#define NOEC pushq $0
#define EC

#define _TRAP(x, code) \
        .globl trap ## x; \
        .balign 8; \
        trap ## x: \
                code; \
                pushq $x; \
                jmp trapcommon; \
        .data; \
        .quad trap ## x; \
        .previous

#define TRAP(x) _TRAP(x, NOEC)
#define TRAPCODE(x) _TRAP(x, EC)

#define _OPTIONAL(option, value, opcode) \
    .section .rodata; \
91:  .asciz option; \
92:  .asciz value; \
    .previous; \
    .section .hotpatch, "a"; \
    .quad opcode, 91b, 92b, 93f; \
    .previous; \
    93:

#define OPTIONAL(option, value) \
    _OPTIONAL(option, value, 0x3)
#define OPTIONAL_KTEXT(option, value) \
    _OPTIONAL(option, value, 0x1)
#define OPTIONAL_QTEXT(option, value) \
    _OPTIONAL(option, value, 0x2)

#define OPTIONAL_OR_NOPS(option) \
    .section .hotpatch, "a"; \
    .quad 0x4, 0, 91f, 0; \
    .previous; \
    91:

#define OPTIONAL_OR_CALL(option, alternative) \
    .section .hotpatch, "a"; \
    .quad 0x5, alternative, 91f, 0; \
    .previous; \
    91:

.section .rodata
mds_clear_cpu_buffers_ds:
.long KDSEG
.text

/* Calling convention:
 *
 * Syscall #:  %rax
 * Arguments:  %rdi, %rsi, %rdx, %r10 (*), %r8, %r9
 * Return RIP: %rcx (from syscall instruction)
 * RFLAGS:     %r11 (from syscall instruction)
 *
 * None of the above registers are preserved across function calls in
 * the AMD64 ABI.  This means user space doesn't need to save any
 * registers across a syscall and we're free to clobber them.
 *
 * (*) This argument register differs from the regular AMD64 ABI.
 * Normally, the fourth argument is in %rcx, but this is clobbered by
 * syscall.  %r10 is cheap to use because it is caller-save.
 */
.code64
.globl sysentry
.balign 8
sysentry:
        swapgs
        movq    %rsp, %gs:GS_SCRATCH

OPTIONAL("lazy_barrier", "no")
        lfence
		movq %cr3, %rsp
	    andq $(~0x1001), %rsp
	    andq %gs:GS_CR3_MASK, %rsp
	    movq %rsp, %cr3
OPTIONAL_OR_NOPS("lazy_barrier")

        // %rsp and set it to point to the kstack
        movq    %gs:GS_PROC, %rsp  // myproc()
        movq    PROC_KSTACK(%rsp), %rsp
        addq    $(KSTACKSIZE-TRAPFRAME_SIZE), %rsp

        // save all registers we're not allowed to clobber
        movq    %r15, 0x10(%rsp)
        movq    $0, %r15
        movq    %r14, 0x18(%rsp)
        movq    $0, %r14
        movq    %r13, 0x20(%rsp)
        movq    $0, %r13
        movq    %r12, 0x28(%rsp)
        movq    $0, %r12
        movq    %rbp, 0x30(%rsp)
        movq    $0, %rbp
        movq    %rbx, 0x38(%rsp)
        movq    $0, %rbx
        movq    %rcx, 0x98(%rsp)  // rip saved by syscall
        movq    $0, %rcx
        movq    %r11, 0xa8(%rsp)  // eflags saved by syscall
        movq    $0, %r11
        movq    %r10, %rcx
        movq    $0, %r10

        // move saved %rsp to trapframe
        movq    %gs:GS_SCRATCH, %r15
        movq    $0, %gs:GS_SCRATCH
        movq    %r15, 0xb0(%rsp) // rsp saved earlier
        movq    $0, %r15

        // Push a fake activation record so we can stack unwind
        pushq   0x98(%rsp)  // Return RIP
        pushq   0x30(%rsp)  // User RBP
        mov     %rsp, %rbp

        pushq   %rax // syscall#

        movq $text, %rax

OPTIONAL("fsgsbase", "yes")
        wrfsbase %rax
OPTIONAL_OR_CALL("fsgsbase", emulate_wrfsbase)

        sti
        call    sysentry_c
        cli

        push    %rax
        movq    %gs:GS_PROC, %rax
        movq    PROC_USER_FS(%rax), %rax

OPTIONAL("fsgsbase", "yes")
        wrfsbase %rax
OPTIONAL_OR_CALL("fsgsbase", emulate_wrfsbase)
        pop     %rax

        // return using SYSRET
        add     $(8*3), %rsp  // Discard syscall#, fake activation record
        movq    %rsp, %r11
        movq    0x10(%r11), %r15
        movq    0x18(%r11), %r14
        movq    0x20(%r11), %r13
        movq    0x28(%r11), %r12
        movq    0x30(%r11), %rbp
        movq    0x38(%r11), %rbx
        movq    0x98(%r11), %rcx    // rip to be restored by sysret
        movq    0xb0(%r11), %rsp
        movq    0xa8(%r11), %r11    // eflags to be restored by sysret

        // If we never mapped in secrets, just swapgs and return
        cmpb $0, (secrets_mapped)
        jz 1f

        movq    %rax, %gs:GS_SCRATCH
        movq    %cr3, %rax
        orq     $(0x1001), %rax
        andq    %gs:GS_CR3_MASK, %rax
        movq    %rax, %cr3  // swap page table back
        movq    %gs:GS_SCRATCH, %rax

OPTIONAL("mds", "yes")
        verw (mds_clear_cpu_buffers_ds)
OPTIONAL_OR_NOPS("mds")

1:      lfence
        swapgs
        sysretq

trapcommon:
        pushq %rdi
        movq  $0, %rdi
        pushq %rsi
        movq  $0, %rsi
        pushq %rdx
        movq  $0, %rdx
        pushq %rcx
        movq  $0, %rcx
        pushq %rax
        movq  $0, %rax
        pushq %r8
        movq  $0, %r8
        pushq %r9
        movq  $0, %r9
        pushq %r10
        movq  $0, %r10
        pushq %r11
        movq  $0, %r11
        pushq %rbx
        movq  $0, %rbx
        pushq %rbp
        movq  $0, %rbp
        pushq %r12
        movq  $0, %r12
        pushq %r13
        movq  $0, %r13
        pushq %r14
        movq  $0, %r14
        pushq %r15
        movq  $0, %r15
        subq  $0x10, %rsp

OPTIONAL("kvm_paravirt", "yes")
        // If single-step mode was enabled, re-enable it for us too (for gdb'ing qemu)
        testq $0x100, TF_RFLAGS(%rsp)
        jz 2f
        pushf
        orq   $0x100, (%rsp)
        popf
2:
OPTIONAL_OR_NOPS("kvm_paravirt")

        // If this trap came from kernel space then don't switch stacks or page tables.
        cmpw  $KCSEG, TF_CS(%rsp)
        jz 1f
        swapgs
1:      lfence

OPTIONAL("lazy_barrier", "no")
        call switch_to_kstack
OPTIONAL_OR_NOPS("lazy_barrier")

        movq %rsp, %rdi  // first argument to trap
        movq (secrets_mapped), %rsi  // second argument to trap
OPTIONAL("fsgsbase", "yes")
        rdfsbase %rax
OPTIONAL_OR_CALL("fsgsbase", emulate_rdfsbase)
        pushq %rax
        movq $text, %rax
OPTIONAL("fsgsbase", "yes")
        wrfsbase %rax
OPTIONAL_OR_CALL("fsgsbase", emulate_wrfsbase)
        call trap_c
        popq %rax
        // Fall through to trapret

.globl trapret
.balign 8
trapret:
        cli
OPTIONAL("fsgsbase", "yes")
        wrfsbase %rax
OPTIONAL_OR_CALL("fsgsbase", emulate_wrfsbase)
        addq $0x10, %rsp
        popq %r15
        popq %r14
        popq %r13
        popq %r12
        popq %rbp
        popq %rbx
        popq %r11
        popq %r10
        popq %r9
        popq %r8
        popq %rax
        popq %rcx
        popq %rdx
        popq %rsi
        popq %rdi
        addq $0x10, %rsp  // trapno, err

        // If we are returning to kernel space, skip to the end of of trapret
        cmpw $KCSEG, 8(%rsp)
        jz 1f

        // If we never mapped in secrets, just swapgs and return
        cmpb $0, (secrets_mapped)
        jz 2f

        // At this point there are 40 bytes on the stack and we are returning to
        // userspace with secrets currently mapped. We need to apply mitigations
        // and move those items to the qstack.
        pushq %rbx
        pushq %rax
        movq %gs:GS_PROC, %rax
        movq PROC_QSTACK(%rax), %rax
        addq $(KSTACKSIZE-56), %rax
        movq 0(%rsp), %rbx
        movq %rbx, 0(%rax)
        movq 8(%rsp), %rbx
        movq %rbx, 8(%rax)
        movq 16(%rsp), %rbx
        movq %rbx, 16(%rax)
        movq 24(%rsp), %rbx
        movq %rbx, 24(%rax)
        movq 32(%rsp), %rbx
        movq %rbx, 32(%rax)
        movq 40(%rsp), %rbx
        movq %rbx, 40(%rax)
        movq 48(%rsp), %rbx
        movq %rbx, 48(%rax)
        movq %cr3, %rbx
        orq $(0x1001), %rbx
        andq %gs:GS_CR3_MASK, %rbx
        movq %rbx, %cr3  // swap page table back
        popq %rax
        popq %rbx

OPTIONAL("mds", "yes")
        verw (mds_clear_cpu_buffers_ds)
OPTIONAL_OR_NOPS("mds")

2:      swapgs
1:      lfence
        iretq

// This function does the same thing as `wrfsbase %rax`
.globl emulate_wrfsbase
emulate_wrfsbase:
    push %rdx
    push %rcx
    mov %rax, %rdx
    shrq $32, %rdx
    mov $0xc0000100, %rcx
    wrmsr
    pop %rcx
    pop %rdx
    ret

// This function does the same thing as `rdfsbase %rax`
.globl emulate_rdfsbase
emulate_rdfsbase:
    push %rdx
    push %rcx
    mov $0xc0000100, %rcx
    rdmsr
    shlq $32, %rdx
    or %rdx, %rax
    pop %rcx
    pop %rdx
    ret

.data
.balign 8
.globl trapentry
trapentry:

.text
    // See section 6.15 of Vol 3.
    TRAP(0)  // divide error
    TRAP(1)  // debug exception
    TRAP(2)  // NMI
    TRAP(3)  // breakpoint
    TRAP(4)  // overflow
    TRAP(5)  // bound
    TRAP(6)  // invalid opcode
    TRAP(7)  // device not available
    TRAPCODE(8)  // double fault - has error code
    TRAP(9)  // coprocessor overrun
    TRAPCODE(10)  // invalid TSS
    TRAPCODE(11)  // segment not present
    TRAPCODE(12)  // stack fault
    TRAPCODE(13)  // general protection fault
    TRAPCODE(14)  // page fault
    TRAP(15)  // not defined
    TRAP(16)  // fpu error
    TRAPCODE(17)  // alignment check
    TRAP(18)  // machine check
    TRAP(19)  // simd
    # Edit .+1,|seq 20 255 | sed 's/.*/    TRAP(&)/'
    TRAP(20)
    TRAP(21)
    TRAP(22)
    TRAP(23)
    TRAP(24)
    TRAP(25)
    TRAP(26)
    TRAP(27)
    TRAP(28)
    TRAP(29)
    TRAP(30)
    TRAP(31)
    TRAP(32)
    TRAP(33)
    TRAP(34)
    TRAP(35)
    TRAP(36)
    TRAP(37)
    TRAP(38)
    TRAP(39)
    TRAP(40)
    TRAP(41)
    TRAP(42)
    TRAP(43)
    TRAP(44)
    TRAP(45)
    TRAP(46)
    TRAP(47)
    TRAP(48)
    TRAP(49)
    TRAP(50)
    TRAP(51)
    TRAP(52)
    TRAP(53)
    TRAP(54)
    TRAP(55)
    TRAP(56)
    TRAP(57)
    TRAP(58)
    TRAP(59)
    TRAP(60)
    TRAP(61)
    TRAP(62)
    TRAP(63)
    TRAP(64)
    TRAP(65)
    TRAP(66)
    TRAP(67)
    TRAP(68)
    TRAP(69)
    TRAP(70)
    TRAP(71)
    TRAP(72)
    TRAP(73)
    TRAP(74)
    TRAP(75)
    TRAP(76)
    TRAP(77)
    TRAP(78)
    TRAP(79)
    TRAP(80)
    TRAP(81)
    TRAP(82)
    TRAP(83)
    TRAP(84)
    TRAP(85)
    TRAP(86)
    TRAP(87)
    TRAP(88)
    TRAP(89)
    TRAP(90)
    TRAP(91)
    TRAP(92)
    TRAP(93)
    TRAP(94)
    TRAP(95)
    TRAP(96)
    TRAP(97)
    TRAP(98)
    TRAP(99)
    TRAP(100)
    TRAP(101)
    TRAP(102)
    TRAP(103)
    TRAP(104)
    TRAP(105)
    TRAP(106)
    TRAP(107)
    TRAP(108)
    TRAP(109)
    TRAP(110)
    TRAP(111)
    TRAP(112)
    TRAP(113)
    TRAP(114)
    TRAP(115)
    TRAP(116)
    TRAP(117)
    TRAP(118)
    TRAP(119)
    TRAP(120)
    TRAP(121)
    TRAP(122)
    TRAP(123)
    TRAP(124)
    TRAP(125)
    TRAP(126)
    TRAP(127)
    TRAP(128)
    TRAP(129)
    TRAP(130)
    TRAP(131)
    TRAP(132)
    TRAP(133)
    TRAP(134)
    TRAP(135)
    TRAP(136)
    TRAP(137)
    TRAP(138)
    TRAP(139)
    TRAP(140)
    TRAP(141)
    TRAP(142)
    TRAP(143)
    TRAP(144)
    TRAP(145)
    TRAP(146)
    TRAP(147)
    TRAP(148)
    TRAP(149)
    TRAP(150)
    TRAP(151)
    TRAP(152)
    TRAP(153)
    TRAP(154)
    TRAP(155)
    TRAP(156)
    TRAP(157)
    TRAP(158)
    TRAP(159)
    TRAP(160)
    TRAP(161)
    TRAP(162)
    TRAP(163)
    TRAP(164)
    TRAP(165)
    TRAP(166)
    TRAP(167)
    TRAP(168)
    TRAP(169)
    TRAP(170)
    TRAP(171)
    TRAP(172)
    TRAP(173)
    TRAP(174)
    TRAP(175)
    TRAP(176)
    TRAP(177)
    TRAP(178)
    TRAP(179)
    TRAP(180)
    TRAP(181)
    TRAP(182)
    TRAP(183)
    TRAP(184)
    TRAP(185)
    TRAP(186)
    TRAP(187)
    TRAP(188)
    TRAP(189)
    TRAP(190)
    TRAP(191)
    TRAP(192)
    TRAP(193)
    TRAP(194)
    TRAP(195)
    TRAP(196)
    TRAP(197)
    TRAP(198)
    TRAP(199)
    TRAP(200)
    TRAP(201)
    TRAP(202)
    TRAP(203)
    TRAP(204)
    TRAP(205)
    TRAP(206)
    TRAP(207)
    TRAP(208)
    TRAP(209)
    TRAP(210)
    TRAP(211)
    TRAP(212)
    TRAP(213)
    TRAP(214)
    TRAP(215)
    TRAP(216)
    TRAP(217)
    TRAP(218)
    TRAP(219)
    TRAP(220)
    TRAP(221)
    TRAP(222)
    TRAP(223)
    TRAP(224)
    TRAP(225)
    TRAP(226)
    TRAP(227)
    TRAP(228)
    TRAP(229)
    TRAP(230)
    TRAP(231)
    TRAP(232)
    TRAP(233)
    TRAP(234)
    TRAP(235)
    TRAP(236)
    TRAP(237)
    TRAP(238)
    TRAP(239)
    TRAP(240)
    TRAP(241)
    TRAP(242)
    TRAP(243)
    TRAP(244)
    TRAP(245)
    TRAP(246)
    TRAP(247)
    TRAP(248)
    TRAP(249)
    TRAP(250)
    TRAP(251)
    TRAP(252)
    TRAP(253)
    TRAP(254)
    TRAP(255)
