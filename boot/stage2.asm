; =============================================================
;  stage2.asm  -  Stage 2 Bootloader
; =============================================================
;  Responsibilities:
;    1. Load the kernel image and the pre-computed checksum
;       from disk (still in 16-bit real mode).
;    2. Verify kernel integrity using a checksum that was
;       *seeded* by the Quantum Security Module at build time
;       (see tools/quantum_key_gen.py + tools/gen_checksum.py).
;    3. Enable the A20 line.
;    4. Load the GDT and switch the CPU into 32-bit protected
;       mode (set CR0.PE, far jump to flush the prefetch queue).
;    5. Jump to the kernel's entry point.
;
;  Loaded by boot1.asm at physical address 0x8000 (ORG matches).
; =============================================================

[BITS 16]
[ORG 0x8000]

; SEED_CONST is generated at build time by tools/gen_checksum.py
; from the quantum-derived key (see docs/DOCUMENTATION.md, section
; "Quantum Security Module"). It is NOT computed at boot time -
; no quantum hardware/simulator is available at boot.
%include "stage2_seed.inc"

KERNEL_SECTORS equ 64                     ; reserved kernel size on disk
KERNEL_BYTES   equ KERNEL_SECTORS * 512   ; = 32768 bytes (32 KB budget)

stage2_start:
    mov [boot_drive], dl

    mov si, msg_stage2
    call print_string16

    ; ---- Load checksum.bin (1 sector) to 0x0000:0x9000 ----
    mov si, dap_checksum
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    ; ---- Load kernel.bin (KERNEL_SECTORS) to 0x1000:0x0000 (phys 0x10000) ----
    mov si, dap_kernel
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    mov si, msg_verify
    call print_string16

    call verify_checksum
    cmp ax, 1
    jne integrity_fail

    mov si, msg_ok
    call print_string16

    call enable_a20

    cli
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 1           ; set PE (Protection Enable) bit
    mov cr0, eax

    jmp CODE_SEG:protected_mode_entry   ; far jump flushes prefetch queue

; -------------------------------------------------------------
integrity_fail:
    mov si, msg_fail
    call print_string16
    jmp $

disk_error:
    mov si, msg_diskerr
    call print_string16
    jmp $

; =============================================================
; 16-bit helper routines
; =============================================================

print_string16:
    pusha
.loop:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    mov dx, 0xE9        ; echo to QEMU debug console (port 0xE9) for
    out dx, al          ; scripted/headless testing; inert on real hardware
    jmp .loop
.done:
    popa
    ret

enable_a20:
    in al, 0x92
    or al, 2
    out 0x92, al
    ret

; -------------------------------------------------------------
; verify_checksum
;   Recomputes a rolling XOR/rotate checksum over the freshly
;   loaded kernel image (at physical 0x10000, KERNEL_BYTES long),
;   seeded with SEED_CONST (derived from the quantum module).
;   Compares the result against the 4-byte value loaded from
;   checksum.bin (physical 0x9000).
;
;   Algorithm (must match tools/gen_checksum.py exactly):
;       acc = SEED_CONST
;       for each little-endian dword d in kernel image:
;           acc = rol32(acc XOR d, 1)
;
;   Returns: AX = 1 if match, AX = 0 if mismatch.
; -------------------------------------------------------------
verify_checksum:
    push es
    mov ax, 0x1000
    mov es, ax                  ; ES:EDI walks the kernel image
    xor edi, edi
    mov eax, SEED_CONST
    mov ecx, KERNEL_BYTES / 4
.calc_loop:
    mov edx, [es:edi]
    xor eax, edx
    rol eax, 1
    add edi, 4
    loop .calc_loop
    pop es

    ; EAX now holds the freshly computed checksum. Compare it to
    ; the expected value stored at 0x0000:0x9000 by the build tool.
    push es
    xor bx, bx
    mov es, bx
    mov ebx, [es:0x9000]
    cmp eax, ebx
    je .match
    xor ax, ax
    jmp .done
.match:
    mov ax, 1
.done:
    pop es
    ret

; -------------------------------------------------------------
boot_drive: db 0

msg_stage2:  db "[stage2] Loading kernel image + checksum...", 13, 10, 0
msg_verify:  db "[stage2] Verifying kernel integrity (quantum-seeded checksum)...", 13, 10, 0
msg_ok:      db "[stage2] Integrity OK. Entering protected mode...", 13, 10, 0
msg_fail:    db "[stage2] INTEGRITY CHECK FAILED - kernel image rejected. Halting.", 13, 10, 0
msg_diskerr: db "[stage2] Disk read error in Stage 2!", 13, 10, 0

; Disk Address Packets (INT13h/AH=42h format, see boot1.asm)
dap_checksum:
    db 0x10
    db 0
    dw 1              ; 1 sector holds the 4-byte checksum
    dw 0x9000
    dw 0x0000
    dq 9              ; LBA 9 (after boot1[1 sector] + stage2[8 sectors])

dap_kernel:
    db 0x10
    db 0
    dw KERNEL_SECTORS
    dw 0x0000
    dw 0x1000
    dq 10             ; LBA 10 (right after the checksum sector)

%include "gdt.inc"

; =============================================================
; 32-bit protected mode entry point
; =============================================================
[BITS 32]
protected_mode_entry:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000        ; plenty of stack space below the kernel load area

    jmp 0x10000             ; jump straight to the kernel's entry point

; -------------------------------------------------------------
; Pad Stage 2 out to exactly 8 sectors (4096 bytes) so the disk
; layout in the Makefile / build script stays lined up.
times 4096-($-$$) db 0
