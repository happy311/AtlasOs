#include "vga.h"
#include "string.h"

#define VGA_MEMORY   ((volatile uint16_t *)0xB8000)
#define VGA_WIDTH    80
#define VGA_HEIGHT   25
#define DEFAULT_COLOR 0x0F   /* white text on black background */

static int cursor_row = 0;
static int cursor_col = 0;
static uint8_t current_color = DEFAULT_COLOR;

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

void vga_set_color(uint8_t color) {
    current_color = color;
}

void vga_set_cursor_pos(int row, int col) {
    cursor_row = row;
    cursor_col = col;
}

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = vga_entry(' ', current_color);
    }
    cursor_row = 0;
    cursor_col = 0;
}

/* Real scrolling: shift every line up by one row and blank the
 * new bottom line, rather than wrapping back to the top (which
 * would silently overwrite the top of the screen). This is the
 * one piece of "terminal" behaviour a text-mode kernel needs to
 * feel usable once you're typing shell commands into it. */
static void vga_scroll(void) {
    for (int row = 1; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            VGA_MEMORY[(row - 1) * VGA_WIDTH + col] = VGA_MEMORY[row * VGA_WIDTH + col];
        }
    }
    for (int col = 0; col < VGA_WIDTH; col++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = vga_entry(' ', current_color);
    }
    cursor_row = VGA_HEIGHT - 1;
}

static void vga_newline(void) {
    cursor_col = 0;
    cursor_row++;
    if (cursor_row >= VGA_HEIGHT) {
        vga_scroll();
    }
}

void vga_putc_colored(char c, uint8_t color) {
    if (c == '\n') {
        vga_newline();
        return;
    }
    if (c == '\r') {
        cursor_col = 0;
        return;
    }
    int pos = cursor_row * VGA_WIDTH + cursor_col;
    VGA_MEMORY[pos] = vga_entry(c, color);
    cursor_col++;
    if (cursor_col >= VGA_WIDTH) {
        vga_newline();
    }
}

void vga_putc(char c) {
    vga_putc_colored(c, current_color);
}

/* Moves the cursor back one cell and blanks it - used by the shell
 * line editor when the user presses Backspace. Deliberately does
 * not walk back across a line boundary; the shell only calls this
 * while there is still typed input on the current line. */
void vga_backspace(void) {
    if (cursor_col > 0) {
        cursor_col--;
        int pos = cursor_row * VGA_WIDTH + cursor_col;
        VGA_MEMORY[pos] = vga_entry(' ', current_color);
    }
}

void vga_print_colored(const char *str, uint8_t color) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        vga_putc_colored(str[i], color);
    }
}

void vga_print(const char *str) {
    vga_print_colored(str, current_color);
}
