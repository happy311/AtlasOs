#include "isr.h"
#include "pic.h"
#include "vga.h"
#include "string.h"

static const char *exception_messages[32] = {
    "Divide error",              "Debug",                     "Non-maskable interrupt",   "Breakpoint",
    "Overflow",                  "Bound range exceeded",      "Invalid opcode",           "Device not available",
    "Double fault",              "Coprocessor segment overrun","Invalid TSS",              "Segment not present",
    "Stack-segment fault",       "General protection fault",  "Page fault",                "Reserved",
    "x87 FPU error",             "Alignment check",           "Machine check",             "SIMD FP exception",
    "Reserved",                  "Reserved",                  "Reserved",                  "Reserved",
    "Reserved",                  "Reserved",                  "Reserved",                  "Reserved",
    "Reserved",                  "Reserved",                  "Reserved",                  "Reserved",
};

static irq_handler_t irq_routines[16] = { 0 };

void irq_install_handler(int irq, irq_handler_t handler) {
    irq_routines[irq] = handler;
}

void irq_uninstall_handler(int irq) {
    irq_routines[irq] = 0;
}

/* Dispatches CPU exceptions (vectors 0-31). Interrupt 3 (#BP, the
 * breakpoint trap raised by `int3`) is used by the shell's `trap`
 * command as a safe, resumable demonstration that the IDT works -
 * it prints and returns. Every other exception is treated as fatal:
 * there's no process/segment to kill and resume here, so we report
 * what happened and halt rather than limping on in an undefined
 * state. */
void isr_handler(registers_t regs) {
    if (regs.int_no == 3) {
        vga_print_colored("[isr] Breakpoint (int3) caught - resuming.\n", 0x0E);
        return;
    }

    char numbuf[16];
    vga_print_colored("\n[isr] Unhandled CPU exception: ", 0x0C);
    vga_print_colored(exception_messages[regs.int_no], 0x0C);
    vga_print_colored(" (vector ", 0x0C);
    itoa((int)regs.int_no, numbuf, 10);
    vga_print_colored(numbuf, 0x0C);
    vga_print_colored(", error code 0x", 0x0C);
    utoa(regs.err_code, numbuf, 16);
    vga_print_colored(numbuf, 0x0C);
    vga_print_colored(")\n[isr] System halted.\n", 0x0C);

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

/* Dispatches hardware IRQs (vectors 32-47 -> lines 0-15). Looks up
 * whatever driver registered itself for this line, runs it, then
 * always sends the EOI - even if no handler is registered, so a
 * stray/unmasked line can't stall the PIC. */
void irq_handler(registers_t regs) {
    int irq = (int)(regs.int_no - 32);

    if (irq_routines[irq] != 0) {
        irq_routines[irq](&regs);
    }

    pic_send_eoi((uint8_t)irq);
}
