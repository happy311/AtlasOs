#ifndef ISR_H
#define ISR_H

#include <stdint.h>

/* Snapshot of the CPU state at the moment an interrupt fired.
 * The field order matches exactly what isr.asm pushes onto the
 * stack (pusha, then the saved data-segment selector, then the
 * int_no/err_code pair the stub pushed, then whatever the CPU
 * itself pushed before entering the handler). Because we never
 * change privilege level (everything runs in ring 0), the CPU
 * only pushes EIP/CS/EFLAGS - no stack switch, so no ESP/SS. */
typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;
} registers_t;

typedef void (*irq_handler_t)(registers_t *regs);

/* Registers a C handler for hardware IRQ `irq` (0-15). Called once
 * per driver during boot (timer.c registers IRQ0, keyboard.c IRQ1). */
void irq_install_handler(int irq, irq_handler_t handler);
void irq_uninstall_handler(int irq);

/* Called from isr.asm's common stubs - not meant to be called
 * directly from driver code. */
void isr_handler(registers_t regs);
void irq_handler(registers_t regs);

/* Raw entry points defined in isr.asm, one per CPU exception (0-31)
 * and one per remapped hardware IRQ (32-47). idt.c wires each of
 * these into the IDT. */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);  extern void isr3(void);
extern void isr4(void);  extern void isr5(void);  extern void isr6(void);  extern void isr7(void);
extern void isr8(void);  extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void); extern void isr15(void);
extern void isr16(void); extern void isr17(void); extern void isr18(void); extern void isr19(void);
extern void isr20(void); extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void); extern void isr27(void);
extern void isr28(void); extern void isr29(void); extern void isr30(void); extern void isr31(void);

extern void irq0(void);  extern void irq1(void);  extern void irq2(void);  extern void irq3(void);
extern void irq4(void);  extern void irq5(void);  extern void irq6(void);  extern void irq7(void);
extern void irq8(void);  extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void); extern void irq15(void);

extern void idt_flush(uint32_t idt_ptr_addr);

#endif /* ISR_H */
