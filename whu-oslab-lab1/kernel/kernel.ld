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
        etext = .;     /* 代码段结束 */
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

    /*
     * trampoline 代码段（放在最后，页表切换时使用）
     */
    .trampoline : {
        . = ALIGN(4096);   /* 页面对齐 */
        trampoline = .;
        *(.trampoline)
    }

    /*
     * 内核结束地址
     */
    end = .;

    /DISCARD/ : {
        *(.comment)
        *(.gnu*)
        *(.note*)
        *(.eh_frame*)
    }
}
