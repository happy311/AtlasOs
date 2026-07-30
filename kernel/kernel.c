#include "vga.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"
#include "serial.h"
#include "kmalloc.h"
#include "paging.h"
#include "shell.h"

/* Mirrors the bootloader's use of QEMU's debug console (port 0xE9) so
 * the whole boot chain can be tested headlessly / in CI. Inert on
 * real hardware. */
static inline void debug_char(char c) {
    __asm__ volatile ("outb %0, %1" : : "a"(c), "Nd"((unsigned short)0xE9));
}
static void debug_str(const char *s) {
    while (*s) debug_char(*s++);
}

void kernel_main(void) {
    debug_str("[kernel] kernel_main() reached.\n");

    vga_clear();
    vga_print_colored("AtlasOS\n", 0x0A);
    vga_print_colored("=======\n\n", 0x0A);
    vga_print("Loaded by a custom two-stage bootloader.\n");
    vga_print_colored("[OK] Protected mode active (32-bit)\n", 0x0F);
    vga_print_colored("[OK] Kernel integrity verified before load\n", 0x0F);

    /* Bring-up order matters:
     *   1. IDT before PIC - so that even if something goes wrong
     *      during PIC remapping, a stray interrupt lands on a
     *      handler instead of a triple fault.
     *   2. PIC remap before anything unmasks an IRQ - otherwise a
     *      timer/keyboard interrupt would vector into a CPU
     *      exception slot (see pic.c).
     *   3. Drivers that install IRQ handlers (timer, keyboard)
     *      before we globally enable interrupts with STI.
     *   4. kmalloc before paging - paging.c doesn't need the heap,
     *      but a real allocator (page tables, VMM structures) would;
     *      keeping this order documents the dependency for later.
     */
    idt_init();
    pic_remap(0x20, 0x28);

    timer_init(100);        /* 100 Hz tick */
    keyboard_init();
    serial_init();
    kmalloc_init();
    paging_init();

    vga_print_colored("[OK] IDT + PIC + PIT timer (IRQ0)\n", 0x0F);
    vga_print_colored("[OK] PS/2 keyboard driver (IRQ1)\n", 0x0F);
    vga_print_colored("[OK] Serial (COM1) driver\n", 0x0F);
    vga_print_colored("[OK] Heap allocator (kmalloc) online\n", 0x0F);
    vga_print_colored("[OK] Paging enabled (identity-mapped)\n", 0x0F);

    debug_str("[kernel] All subsystems initialized. Enabling interrupts.\n");
    __asm__ volatile ("sti");

    shell_run();   /* never returns */
}
