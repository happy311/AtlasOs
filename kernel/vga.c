#include "vga.h"

#define VGA_MEMORY   ((volatile uint16_t *)0xB8000)
#define VGA_WIDTH    80
#define VGA_HEIGHT   25
#define DEFAULT_COLOR 0x0F   /* white text on black background */

static int cursor_row = 0;
static int cursor_col = 0;

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

void vga_set_cursor_pos(int row, int col) {
    cursor_row = row;
    cursor_col = col;
}

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = vga_entry(' ', DEFAULT_COLOR);
    }
    cursor_row = 0;
    cursor_col = 0;
}

static void vga_newline(void) {
    cursor_col = 0;
    cursor_row++;
    if (cursor_row >= VGA_HEIGHT) {
        cursor_row = 0; /* no scrolling implemented - simple wrap for this demo kernel */
    }
}

void vga_print_colored(const char *str, uint8_t color) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n') {
            vga_newline();
            continue;
        }
        int pos = cursor_row * VGA_WIDTH + cursor_col;
        VGA_MEMORY[pos] = vga_entry(str[i], color);
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            vga_newline();
        }
    }
}

void vga_print(const char *str) {
    vga_print_colored(str, DEFAULT_COLOR);
}
