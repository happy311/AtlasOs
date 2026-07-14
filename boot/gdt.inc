; =============================================================
;  gdt.inc  -  Global Descriptor Table (flat memory model)
; =============================================================
;  Defines exactly two usable descriptors (plus the mandatory
;  null descriptor) so that Stage 2 can enter 32-bit protected
;  mode with a simple flat 4GB address space:
;    - gdt_code : base=0, limit=4GB, executable, ring 0
;    - gdt_data : base=0, limit=4GB, writable,   ring 0
;  No paging, no TSS, no LDT - deliberately minimal.
; =============================================================

gdt_start:

gdt_null:
    dq 0x0000000000000000

gdt_code:                  ; offset 0x08
    dw 0xFFFF               ; limit  0-15
    dw 0x0000                ; base   0-15
    db 0x00                   ; base   16-23
    db 10011010b               ; access: present,ring0,code,exec/read
    db 11001111b                ; flags(4-bit)+limit(16-19): 4K gran,32-bit
    db 0x00                       ; base 24-31

gdt_data:                  ; offset 0x10
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b            ; access: present,ring0,data,read/write
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1   ; GDT limit (size - 1)
    dd gdt_start                 ; GDT linear base address

CODE_SEG equ gdt_code - gdt_start   ; = 0x08
DATA_SEG equ gdt_data - gdt_start   ; = 0x10
