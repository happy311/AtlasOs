; =============================================================
;  boot1.asm  -  Stage 1 Bootloader (Master Boot Record)
; =============================================================
;  Responsibilities (and ONLY these - kept intentionally minimal):
;    1. Get loaded by the BIOS at 0x7C00 in 16-bit real mode.
;    2. Set up a known-good segment/stack environment.
;    3. Use BIOS INT 13h (AH=42h, LBA extended read) to load
;       Stage 2 from disk into memory at 0x0000:0x8000.
;    4. Jump to Stage 2.
;
;  This file must assemble to exactly 512 bytes, ending with the
;  boot signature 0x55AA, or BIOS will refuse to treat it as a
;  boot sector.
; =============================================================

[BITS 16]
[ORG 0x7C00]

start:
    cli                     ; no interrupts while we set up segments/stack
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00          ; stack grows down from just below us
    sti

    mov [boot_drive], dl    ; BIOS passes the boot drive number in DL

    mov si, msg_stage1
    call print_string

    ; ---- Load Stage 2 (8 sectors = 4096 bytes) to 0x0000:0x8000 ----
    mov si, dap_stage2
    mov ah, 0x42             ; INT 13h, AH=42h = Extended Read (LBA)
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    mov si, msg_jump
    call print_string

    mov dl, [boot_drive]      ; stage2 expects boot drive in DL
    jmp 0x0000:0x8000         ; hand off control to Stage 2

; -------------------------------------------------------------
disk_error:
    mov si, msg_error
    call print_string
    jmp $                     ; halt forever

; -------------------------------------------------------------
; print_string: prints a null-terminated string using BIOS
; teletype output (INT 10h, AH=0Eh). SI -> string.
print_string:
    pusha
.loop:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    mov dx, 0xE9        ; also echo to QEMU's debug console (port 0xE9) -
    out dx, al          ; makes headless/scripted testing possible; this
                        ; port doesn't exist on real hardware, so it's a
                        ; harmless no-op outside of QEMU/Bochs.
    jmp .loop
.done:
    popa
    ret

; -------------------------------------------------------------
boot_drive: db 0

msg_stage1: db "[boot1] Stage 1 OK. Loading Stage 2...", 13, 10, 0
msg_jump:   db "[boot1] Jumping to Stage 2...", 13, 10, 0
msg_error:  db "[boot1] DISK READ ERROR - halting.", 13, 10, 0

; Disk Address Packet (DAP) used by INT13h/AH=42h.
; Layout: size(1) reserved(1) sectorcount(2) offset(2) segment(2) LBA(8)
dap_stage2:
    db 0x10        ; size of this packet (16 bytes)
    db 0           ; reserved, must be 0
    dw 8           ; number of sectors to read (Stage 2 = 8 sectors)
    dw 0x8000      ; destination offset
    dw 0x0000      ; destination segment  -> 0x0000:0x8000 = physical 0x8000
    dq 1           ; starting LBA (sector 1, right after this MBR)

; -------------------------------------------------------------
; Pad to 510 bytes, then write the mandatory boot signature.
times 510-($-$$) db 0
dw 0xAA55
