; =============================================================
;  isr.asm  -  Interrupt/exception entry stubs
; =============================================================
;  The IDT can only point at raw code addresses, and the CPU
;  jumps straight there with no C-friendly calling convention -
;  no argument, no guarantee our C code's registers survive. So
;  every vector gets a tiny asm stub whose only job is:
;    1. push a fake error code if the CPU didn't push a real one
;       (only exceptions 8, 10-14, 17 push one), so every vector
;       leaves the stack in the *same* shape.
;    2. push the vector number, so the C handler knows what fired.
;    3. jump into one shared "common stub" that saves the general
;       purpose registers, calls the C handler, restores them, and
;       irets back to whatever was interrupted.
;  This is the standard technique used by essentially every x86
;  hobby kernel - duplicating 48 near-identical trampolines by
;  hand isn't the interesting part, so we generate them with a
;  couple of NASM macros instead.
; =============================================================

[BITS 32]

extern isr_handler
extern irq_handler

global idt_flush
idt_flush:
    mov eax, [esp + 4]
    lidt [eax]
    ret

; -------------------------------------------------------------
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    cli
    push dword 0        ; dummy error code, keeps stack layout uniform
    push dword %1        ; interrupt number
    jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    cli
    push dword %1        ; CPU already pushed the real error code
    jmp isr_common_stub
%endmacro

%macro IRQ 2
global irq%1
irq%1:
    cli
    push dword 0
    push dword %2
    jmp irq_common_stub
%endmacro

; CPU exceptions 0-31. Only 8, 10, 11, 12, 13, 14, 17 push a real
; error code on the stack themselves (Intel SDM Vol. 3A, ch. 6).
ISR_NOERRCODE 0    ; #DE  Divide error
ISR_NOERRCODE 1    ; #DB  Debug
ISR_NOERRCODE 2    ; NMI
ISR_NOERRCODE 3    ; #BP  Breakpoint (int3)
ISR_NOERRCODE 4    ; #OF  Overflow
ISR_NOERRCODE 5    ; #BR  Bound range exceeded
ISR_NOERRCODE 6    ; #UD  Invalid opcode
ISR_NOERRCODE 7    ; #NM  Device not available
ISR_ERRCODE   8    ; #DF  Double fault
ISR_NOERRCODE 9    ; Coprocessor segment overrun (legacy)
ISR_ERRCODE   10   ; #TS  Invalid TSS
ISR_ERRCODE   11   ; #NP  Segment not present
ISR_ERRCODE   12   ; #SS  Stack-segment fault
ISR_ERRCODE   13   ; #GP  General protection fault
ISR_ERRCODE   14   ; #PF  Page fault
ISR_NOERRCODE 15   ; Reserved
ISR_NOERRCODE 16   ; #MF  x87 FPU error
ISR_ERRCODE   17   ; #AC  Alignment check
ISR_NOERRCODE 18   ; #MC  Machine check
ISR_NOERRCODE 19   ; #XM  SIMD FP exception
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

; Hardware IRQs 0-15, remapped by pic.c to vectors 32-47 so they
; don't collide with the CPU exceptions above.
IRQ 0,  32   ; PIT timer
IRQ 1,  33   ; PS/2 keyboard
IRQ 2,  34   ; cascade (never fires directly)
IRQ 3,  35   ; COM2
IRQ 4,  36   ; COM1
IRQ 5,  37
IRQ 6,  38
IRQ 7,  39
IRQ 8,  40
IRQ 9,  41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

; -------------------------------------------------------------
; Shared trampoline for CPU exceptions (0-31).
isr_common_stub:
    pusha                ; edi, esi, ebp, esp, ebx, edx, ecx, eax

    mov ax, ds
    push eax              ; save the data segment

    mov ax, 0x10           ; kernel data selector, from gdt.inc
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call isr_handler

    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8             ; drop int_no + err_code pushed by the stub
    sti
    iret

; Shared trampoline for hardware IRQs (32-47).
irq_common_stub:
    pusha

    mov ax, ds
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call irq_handler

    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8
    sti
    iret
