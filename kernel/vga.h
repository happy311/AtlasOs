#ifndef VGA_H
#define VGA_H

#include <stdint.h>

/* Minimal VGA text-mode (0xB8000) driver. Kept in its own file so
 * the "hardware access" concern is separated from kernel logic. */

void vga_clear(void);
void vga_print(const char *str);
void vga_print_colored(const char *str, uint8_t color);
void vga_putc(char c);
void vga_putc_colored(char c, uint8_t color);
void vga_backspace(void);
void vga_set_color(uint8_t color);
void vga_set_cursor_pos(int row, int col);

#endif /* VGA_H */
