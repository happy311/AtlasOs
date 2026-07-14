; =============================================================
;  entry.asm  -  Kernel entry stub
; =============================================================
;  The bootloader jumps here (physical/linear 0x10000) in 32-bit
;  protected mode. We set up a small stack area for the kernel
;  and hand off to the C entry point, kernel_main().
; =============================================================

[BITS 32]

global _start
extern kernel_main

_start:
    mov esp, kernel_stack_top   ; give the C kernel its own stack

    call kernel_main            ; should never return

.hang:
    cli
    hlt
    jmp .hang

section .bss
align 16
kernel_stack_bottom:
    resb 16384                  ; 16 KB kernel stack
kernel_stack_top:
