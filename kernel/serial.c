#include "serial.h"
#include "io.h"

#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00);   /* disable all UART interrupts - we poll */
    outb(COM1 + 3, 0x80);   /* enable DLAB to set the baud rate divisor */
    outb(COM1 + 0, 0x03);   /* divisor low byte  -> 38400 baud */
    outb(COM1 + 1, 0x00);   /* divisor high byte */
    outb(COM1 + 3, 0x03);   /* 8 bits, no parity, 1 stop bit; DLAB off */
    outb(COM1 + 2, 0xC7);   /* enable FIFO, clear it, 14-byte threshold */
    outb(COM1 + 4, 0x0B);   /* IRQs enabled (unused here), RTS/DSR set */
}

static int transmit_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

void serial_write_char(char c) {
    while (!transmit_empty()) { }
    outb(COM1, (uint8_t)c);
}

void serial_write_string(const char *s) {
    while (*s) {
        if (*s == '\n') serial_write_char('\r');
        serial_write_char(*s++);
    }
}

int serial_received(void) {
    return inb(COM1 + 5) & 0x01;
}

char serial_read_char(void) {
    while (!serial_received()) { }
    return (char)inb(COM1);
}
