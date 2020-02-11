// x86-64 bootstrap, assuming load by MultiBoot-compliant loader.
// The MutliBoot specification is at:
// http://www.gnu.org/software/grub/manual/multiboot/multiboot.html
// GRUB is a MultiBoot loader, as is qemu's -kernel option.

#include "mmu.h"

// KCODE is the kernel virtual address of the first byte of physical memory.
// The linker loads the executable as if starting at KCODE+2MB, but we
// ask the loader to load the kernel at physical 2MB and then set up the
// necessary memory mapping to switch to the higher address.
// The value of KCODE must match the definitions in kernel.h and kernel.ld.
#define KCODE 0xFFFFFFFFC0000000

// PADDR(x) is the physical memory address corresponding to x.
// Until we set up the memory map, fairly late in this file, we have to
// refer to PADDR(symbol) instead of symbol, so that we use the
// physical address.
#define PADDR(x) ((x) - KCODE)

// STACK is the size of the bootstrap stack.
#define STACK KSTACKSIZE

// MultiBoot header.
// http://www.gnu.org/software/grub/manual/multiboot/multiboot.html#Header-layout
.balign 4
.text
.globl multiboot_header
multiboot_header:
    #define magic 0x1badb002
    #define flags (1<<16 | 1<<2 | 1<<0)
    .long magic
    .long flags
    .long (- magic - flags)        # checksum
    .long PADDR(multiboot_header)  # header address
    .long PADDR(multiboot_header)  # load address
    .long PADDR(edata)             # load end address
    .long PADDR(end)               # bss end address
    .long PADDR(start)             # entry address
	.long 0                        # graphics mode
	.long 800                      # x-resolution
	.long 600                      # y-resolution
	.long 32                       # bit depth

.balign 8
.text
.globl multiboot2_header
multiboot2_header:
    #define magic2         0xE85250D6
    #define architecture  0
    #define header_length (mb2_end - multiboot2_header)
    .long magic2
    .long 0
    .long header_length
    .long (- magic2 - architecture - header_length) // checksum

    .word 2                        // type = address
    .word 1                        // flags = optional
    .long 24                       // size
    .long PADDR(multiboot2_header) // header address
    .long PADDR(multiboot2_header) // load address
    .long PADDR(edata)             // load end address
    .long PADDR(end)               // bss end address

    .word 3                        // type = entry
    .word 1                        // flags = optional
    .long 12                       // size
    .long PADDR(start)             // entry address
    .long 0                        // padding

    .word 5                        // type = framebuffer
    .word 1                        // flags = optional
    .long 20                       // size
    .long 800                      // width
    .long 600                      // height
    .long 32                       // depth
    .long 0                        // padding

    .word 7                        // type = boot services tag
    .word 1
    .long 8

    .word 9
    .word 1
    .long 12
    .long PADDR(efi_main)
    .long 0

    .word 0                        // type = end
    .word 0                        // flags
    .long 8                        // size

mb2_end:

// Entry point jumped to by boot loader.  Running in 32-bit mode.
// http://www.gnu.org/software/grub/manual/multiboot/multiboot.html#Machine-state
//
//    EAX = 0x2badb002
//    EBX = address of multiboot information structure
//    CS = 32-bit read/execute code segment with identity map
//    DS, ES, FS, GS, SS = 32-bit read/write data segment with identity map
//    A20 gate = enabled
//    CR0 = PE set, PG clear
//    EFLAGS = VM clear, IF clear
.code32
.globl start
start:
    // Tell BIOS to do "warm reboot" when we shut down.
    movw $0x1234, 0x472

    // Set up multiboot arguments for cmain.
    movl %eax, %edi
    movl %ebx, %esi

    // Initialize stack.
    movl $PADDR(stack+STACK), %esp

    call loadgdt

    // Enter new 32-bit code segment (already in 32-bit mode).
    ljmp $KCSEG32, $PADDR(start32)  // code32 segment selector

start32:
    // Initialize page table.
    call init32e

    // Enter 64-bit mode.
    movl $PADDR(start64), %eax
    ljmp $KCSEG, $PADDR(tramp64)  // code64 segment selector

.code64
start64:
    // Load VA stack pointer
    movabsq $(stack+STACK), %rsp
    // Clear frame pointer for stack walks, and call into C code.
    movq $0, %rbp
    call cmain
1:  jmp 1b

efi_main:
    movl %eax, %edi
    movl %ebx, %esi
    movq $PADDR(stack+STACK), %rsp
    call efi_cmain

.code32
.globl apstart
apstart:
    call loadgdt
    ljmp $KCSEG32, $PADDR(apstart32)  // code32 segment selector

apstart32:
    call init32e
    movl $PADDR(apstart64), %eax
    ljmp $KCSEG, $PADDR(tramp64)  // code64 segment selector

.code64
apstart64:
    # Remember (from bootothers), that our kernel stack pointer is
    # at the top of our temporary stack.
    popq %rax
    movq %rax, %rsp
    movq $0, %rbp
    call mpboot
1:  jmp 1b

.code64
tramp64:
    // The linker thinks we are running at tramp64, but we're actually
    // running at PADDR(tramp64), so use an explicit calculation to
    // load and jump to the correct address.  %rax should hold the
    // physical address of the jmp target.
    movq $KCODE, %r11
    addq %r11, %rax
    jmp *%rax

// Initialize IA-32e mode.  See section 9.8.5 of 253668.pdf.
.code32
init32e:
    // Set CR4.PAE = CR4.PSE = CR4.OSFXSR = CR4.OSXMMEXCPT = 1.
    movl %cr4, %eax
    orl $0x630, %eax
    movl %eax, %cr4

    // Load CR3 with physical base address of level 4 page table.
    movl $PADDR(kpml4), %eax
    movl %eax, %cr3

    // Enable IA-32e mode by setting IA32_EFER.LME = 1.
    // Also turn on IA32_EFER.SCE (syscall enable) and
    // IA32_EFER.NXE (no-execute enable).
    movl $0xc0000080, %ecx
    rdmsr
    orl $((1<<8)|(1<<0)|(1<<11)), %eax
    wrmsr

    // Enable paging by setting CR0.PG = 1.
    movl %cr0, %eax
    orl $0x80000000, %eax
    movl %eax, %cr0
    nop
    nop
    ret

loadgdt:
    subl $8, %esp
    movl $PADDR(bootgdt), 4(%esp)
    movw $(8*NSEGS-1), 2(%esp)
    lgdt 2(%esp)
    addl $8, %esp

    movl $KDSEG, %eax  // data segment selector
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    movl $0, %eax  // null segment selector
    movw %ax, %fs
    movw %ax, %gs
    ret

// Initial stack
.comm stack, STACK

// Page tables.  See section 4.5 of 253668.pdf.
// We map the first GB of physical memory at 0, KBASE, KCODE.  At boot
// time we are using the mapping at 0 but during ordinary execution we
// use the high mappings.
// The intent is that after bootstrap the kernel can expand this mapping
// to cover all the available physical memory.
// This would be easier if we could use the PS bit to create GB-sized entries
// and skip the pdt table, but not all chips support it, and QEMU doesn't.
// This depends on pdtcode and pdtbase which are defined/initialized in hwvm.cc
// using constexpr functions.
.section .data
.balign 4096
.global kpml4
kpml4:
    .quad PADDR(pdpt0) + (1<<0) + (1<<1)  // present, read/write
    .space 4096 - 3*8
    // We can't just use pdpt0 because we mark pdpt0 no-execute
    // in initpg.
    .quad PADDR(pdptbase) + (1<<0) + (1<<1)  // present, read/write
    .quad PADDR(pdptcode) + (1<<0) + (1<<1)  // present, read/write
.balign 4096
pdpt0:
    .quad PADDR(pdtcode) + (1<<0) + (1<<1)  // present, read/write
    .space 4096 - 8
.balign 4096
pdptbase:
    .quad PADDR(pdtbase) + (1<<0) + (1<<1)         // present, read/write
    .quad PADDR(pdtbase+0x1000) + (1<<0) + (1<<1)  // present, read/write
    .quad PADDR(pdtbase+0x2000) + (1<<0) + (1<<1)  // present, read/write
    .quad PADDR(pdtbase+0x3000) + (1<<0) + (1<<1)  // present, read/write
    .space 4096 - 32
.balign 4096
pdptcode:
    .space 4096 - 8
    .quad PADDR(pdtcode) + (1<<0) + (1<<1)  // present, read/write

.section .eh_frame
.globl __EH_FRAME_BEGIN__
__EH_FRAME_BEGIN__:

.section .eh_frame_end
.globl __EH_FRAME_END__
__EH_FRAME_END__:
    .long 0     // see gcc/crtstuff.c __FRAME_END__

