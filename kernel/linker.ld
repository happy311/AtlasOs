/* =============================================================
 *  linker.ld  -  Kernel linker script
 * =============================================================
 *  Places the kernel at 0x10000, which is exactly where Stage 2
 *  loads it in memory (see boot/stage2.asm, dap_kernel) and
 *  exactly where Stage 2 jumps after enabling protected mode.
 *  Flat binary output (no ELF headers) via objcopy -O binary.
 * ============================================================= */

ENTRY(_start)

SECTIONS
{
    . = 0x10000;

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
        *(COMMON)
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame)
    }
}
