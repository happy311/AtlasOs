#ifndef IO_H
#define IO_H

#include <stdint.h>

/* Thin wrappers around the x86 IN/OUT instructions. Every driver
 * (VGA, PIC, PIT, keyboard, serial) talks to its hardware through
 * these two primitives - there is no other way to reach I/O port
 * space from C. */

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Writing to port 0x80 (an unused POST-code diagnostic port on
 * real hardware) takes long enough to act as a small delay - the
 * standard trick for giving slow 8-bit hardware (the PICs) time
 * to react between successive out's during initialization. */
static inline void io_wait(void) {
    outb(0x80, 0);
}

#endif /* IO_H */
