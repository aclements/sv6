OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(xxx)

SECTIONS
{
    . = 0xFFFFFFFFC0200000;
    PROVIDE(text = .);
    .text : AT(0x200000) {
        *(.text .stub .text.* .gnu.linkonce.t.*)
    }
    .rodata : {
        *(.rodata .rodata.* .gnu.linkonce.r.* .sflag)
    }
    . = ALIGN(0x1000);
    .hotpatch : {
        PROVIDE(__hotpatch_start = .);
        *(.hotpatch)
        PROVIDE(__hotpatch_end = .);
    }
    . = ALIGN(0x1000);
    .init_array : {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP(*(.init_array))
        KEEP(*(.ctors));
        PROVIDE_HIDDEN(__init_array_end = .);
    }

    .percpuinit_array : {
        PROVIDE_HIDDEN(__percpuinit_array_start = .);
        KEEP(*(.percpuinit_array))
        PROVIDE_HIDDEN(__percpuinit_array_end = .);
    }

    /* Binary blobs from kernel/incbin.S */
    .init_rodata : {
        *(.init_rodata)

        /* Pad section to end of hugepage */
        . = ALIGN(0x200000);
    }

    .data : {
        *(.data .data.* .gnu.linkonce.d.*)
        *(.got) *(.igot)
        *(.got.plt)  *(.igot.plt)
        *(.eh_frame_hdr)
        *(.eh_frame)
        *(.eh_frame_end)
        *(.gcc_except_table .gcc_except_table.*)
        . = ALIGN(0x1000);
        PROVIDE(__qdata_start = .);
        *(.qdata)
        . = ALIGN(0x1000);
        PROVIDE(__qdata_end = .);
    }
    PROVIDE(edata = .);
    .bss : {
        *(.bss .bss.* .gnu.linkonce.b.*)
    }

    . = ALIGN(0x1000);
    .percpu : {
        PROVIDE(__percpu_start = .);
        PROVIDE(__qpercpu_start = .);
        *(.qpercpu)
        . = ALIGN(0x1000);
        PROVIDE(__qpercpu_end = .);
        *(.percpu)
        PROVIDE(__percpu_end = .);
    }

    /* 2MByte align, because we set NX on 2MByte super pages. */
    . = ALIGN(0x200000);
    PROVIDE(end = .);

    /*
     * Different versions of GCC seem to provide different sets of retpoline
     * thunks. Since the code itself can't detect whether a symbol exists, we
     * instead define any missing ones as having address zero. (The PROVIDE
     * function only defines a symbol if there is no definition of it in any
     * input file.)
     */
    PROVIDE(__x86_indirect_thunk_rax = 0);
    PROVIDE(__x86_indirect_thunk_rcx = 0);
    PROVIDE(__x86_indirect_thunk_rdx = 0);
    PROVIDE(__x86_indirect_thunk_rbx = 0);
    PROVIDE(__x86_indirect_thunk_rsp = 0);
    PROVIDE(__x86_indirect_thunk_rbp = 0);
    PROVIDE(__x86_indirect_thunk_rsi = 0);
    PROVIDE(__x86_indirect_thunk_rdi = 0);
    PROVIDE(__x86_indirect_thunk_r8 = 0);
    PROVIDE(__x86_indirect_thunk_r9 = 0);
    PROVIDE(__x86_indirect_thunk_r10 = 0);
    PROVIDE(__x86_indirect_thunk_r11 = 0);
    PROVIDE(__x86_indirect_thunk_r12 = 0);
    PROVIDE(__x86_indirect_thunk_r13 = 0);
    PROVIDE(__x86_indirect_thunk_r14 = 0);
    PROVIDE(__x86_indirect_thunk_r15 = 0);
}
