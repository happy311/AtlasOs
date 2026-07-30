#ifndef SERIAL_H
#define SERIAL_H

/* Polled (non-interrupt-driven) 16550 UART driver on COM1. Useful
 * for two things a text-mode VGA console can't do: logging boot
 * progress somewhere that survives a screen full of shell output,
 * and driving the kernel headlessly (QEMU's -serial stdio) for
 * scripted testing, the same role port 0xE9 plays for the boot
 * stages in boot1.asm/stage2.asm. */
void serial_init(void);
void serial_write_char(char c);
void serial_write_string(const char *s);
int  serial_received(void);
char serial_read_char(void);

#endif /* SERIAL_H */
