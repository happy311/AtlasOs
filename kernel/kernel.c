#include "vga.h"

/* =============================================================
 *  kernel.c  -  Tiny Operating System entry point
 * =============================================================
 *  This is intentionally NOT a full kernel. It exists to prove
 *  that the boot chain (BIOS -> Stage1 -> Stage2 -> Protected
 *  Mode -> here) works end to end. It does the minimum needed
 *  to demonstrate a working 32-bit protected-mode environment:
 *    - writes directly to VGA text video memory
 *    - halts the CPU cleanly instead of running off into garbage
 *
 *  Deliberately NOT implemented (out of scope for this project):
 *    paging, interrupts/IDT, a scheduler, drivers beyond VGA,
 *    a filesystem, user mode / syscalls.
 * ============================================================= */

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

    vga_print_colored("Tiny OS\n", 0x0A);            /* light green */
    vga_print_colored("========\n\n", 0x0A);

    vga_print("Kernel loaded successfully by the\n");
    vga_print("Quantum-Assisted Secure Bootloader.\n\n");

    vga_print_colored("[OK] Protected mode active (32-bit)\n", 0x0F);
    vga_print_colored("[OK] Kernel integrity verified before load\n", 0x0F);
    vga_print_colored("[OK] Checksum seed derived from a BB84 QKD\n", 0x0F);
    vga_print_colored("     simulation (Qiskit research module)\n", 0x0F);

    vga_print("\nSystem halted.\n");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
