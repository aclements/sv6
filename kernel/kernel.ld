OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(xxx)

SECTIONS
{
    . = 0xFFFFFFFFC0200000;
    PROVIDE(text = .);
    .text : AT(0x200000) {
        *(.text .stub .text.* .gnu.linkonce.t.*)
        . = ALIGN(0x1000);
    }
    .rodata : {
        *(.rodata .rodata.* .gnu.linkonce.r.* .sflag)

        . = ALIGN(0x1000);
        PROVIDE(__hotpatch_start = .);
        *(.hotpatch)
        PROVIDE(__hotpatch_end = .);

        . = ALIGN(0x1000);
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP(*(.init_array))
        KEEP(*(.ctors));
        PROVIDE_HIDDEN(__init_array_end = .);

        PROVIDE_HIDDEN(__percpuinit_array_start = .);
        KEEP(*(.percpuinit_array))
        PROVIDE_HIDDEN(__percpuinit_array_end = .);
        *(.init_rodata)
        . = ALIGN(0x1000);
    }

    /* .stapsdt.base section might be added here, so force page alignment for kmeta */
    . = ALIGN(0x1000);
    .kmeta : {
        *(.kmeta)
        /* Pad section to end of hugepage */
        . = ALIGN(0x200000);
        ASSERT((. <= 0xFFFFFFFFC0400000), "RO sections > 2MB");
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
        . = ALIGN(0x1000);
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
}
