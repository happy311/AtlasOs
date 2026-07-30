#include "idt.h"
#include "isr.h"
#include "string.h"

#define IDT_ENTRIES 256

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t   idt_ptr;

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low  = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel        = sel;
    idt[num].always0    = 0;
    idt[num].flags       = flags;
}

/* 0x8E = present(1) | DPL(00, ring 0) | S(0, system segment) |
 * type(1110, 32-bit interrupt gate). Interrupt gates (as opposed
 * to trap gates) clear IF on entry, which is what we want - our
 * asm stubs re-enable interrupts explicitly with STI right before
 * IRET once it's safe to do so. */
#define GATE_FLAGS 0x8E
#define KERNEL_CODE_SEG 0x08   /* CODE_SEG from boot/gdt.inc */

void idt_init(void) {
    idt_ptr.limit = sizeof(idt_entry_t) * IDT_ENTRIES - 1;
    idt_ptr.base  = (uint32_t)&idt;

    memset(&idt, 0, sizeof(idt));

    idt_set_gate(0,  (uint32_t)isr0,  KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(1,  (uint32_t)isr1,  KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(2,  (uint32_t)isr2,  KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(3,  (uint32_t)isr3,  KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(4,  (uint32_t)isr4,  KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(5,  (uint32_t)isr5,  KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(6,  (uint32_t)isr6,  KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(7,  (uint32_t)isr7,  KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(8,  (uint32_t)isr8,  KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(9,  (uint32_t)isr9,  KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(10, (uint32_t)isr10, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(11, (uint32_t)isr11, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(12, (uint32_t)isr12, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(13, (uint32_t)isr13, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(14, (uint32_t)isr14, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(15, (uint32_t)isr15, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(16, (uint32_t)isr16, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(17, (uint32_t)isr17, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(18, (uint32_t)isr18, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(19, (uint32_t)isr19, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(20, (uint32_t)isr20, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(21, (uint32_t)isr21, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(22, (uint32_t)isr22, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(23, (uint32_t)isr23, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(24, (uint32_t)isr24, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(25, (uint32_t)isr25, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(26, (uint32_t)isr26, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(27, (uint32_t)isr27, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(28, (uint32_t)isr28, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(29, (uint32_t)isr29, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(30, (uint32_t)isr30, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(31, (uint32_t)isr31, KERNEL_CODE_SEG, GATE_FLAGS);

    idt_set_gate(32, (uint32_t)irq0,  KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(33, (uint32_t)irq1,  KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(34, (uint32_t)irq2,  KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(35, (uint32_t)irq3,  KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(36, (uint32_t)irq4,  KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(37, (uint32_t)irq5,  KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(38, (uint32_t)irq6,  KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(39, (uint32_t)irq7,  KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(40, (uint32_t)irq8,  KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(41, (uint32_t)irq9,  KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(42, (uint32_t)irq10, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(43, (uint32_t)irq11, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(44, (uint32_t)irq12, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(45, (uint32_t)irq13, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(46, (uint32_t)irq14, KERNEL_CODE_SEG, GATE_FLAGS);
    idt_set_gate(47, (uint32_t)irq15, KERNEL_CODE_SEG, GATE_FLAGS);

    idt_flush((uint32_t)&idt_ptr);
}
