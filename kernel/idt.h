#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/* One 32-bit interrupt-gate descriptor. The IDT holds 256 of these;
 * the CPU indexes into it directly with the interrupt/exception
 * number, so the layout below is dictated by the hardware, not by
 * us - this has to match the Intel SDM's gate-descriptor format
 * exactly or the CPU will triple-fault the moment it's loaded. */
typedef struct __attribute__((packed)) {
    uint16_t base_low;   /* handler address bits 0-15 */
    uint16_t sel;        /* code segment selector (our GDT code seg) */
    uint8_t  always0;    /* reserved, must be 0 */
    uint8_t  flags;      /* present | DPL | gate type */
    uint16_t base_high;  /* handler address bits 16-31 */
} idt_entry_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint32_t base;
} idt_ptr_t;

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
void idt_init(void);

#endif /* IDT_H */
