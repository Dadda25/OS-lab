OUTPUT_ARCH(riscv)
ENTRY(_entry)

SECTIONS
{
    /*
     * QEMU virt machine places RAM at 0x80000000
     */
    . = 0x80000000;

    .text : {
        *(.text .text.*)
    }

    .rodata : {
        . = ALIGN(16);
        *(.rodata .rodata.*)
    }

    .data : {
        . = ALIGN(16);
        *(.data .data.*)
    }

    .bss : {
        . = ALIGN(16);
        _bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        _bss_end = .;
    }

    _end = .;

    /DISCARD/ : {
        *(.comment)
        *(.gnu*)
        *(.note*)
        *(.eh_frame*)
    }
}