#include "keyboard.h"
#include "isr.h"
#include "io.h"

#define KBD_DATA_PORT 0x60
#define BUFFER_SIZE   256

/* US QWERTY, PS/2 scancode set 1, unshifted. Index = make-code.
 * 0 means "no printable ASCII for this key" (function keys, caps
 * lock, arrows in their basic form, etc. - not needed for a shell). */
static const char scancode_ascii[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'','`',
    0,  '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0,  ' ', 0,
    /* rest: F-keys, numpad, etc. - unused by this driver */
};

static const char scancode_ascii_shift[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t','Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0,  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,  '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,  ' ', 0,
};

#define LSHIFT_MAKE  0x2A
#define RSHIFT_MAKE  0x36
#define LSHIFT_BREAK 0xAA
#define RSHIFT_BREAK 0xB6
#define BREAK_BIT    0x80

static volatile char buffer[BUFFER_SIZE];
static volatile int  buf_head = 0;   /* next slot to write */
static volatile int  buf_tail = 0;   /* next slot to read */
static int shift_held = 0;

static void buffer_push(char c) {
    int next = (buf_head + 1) % BUFFER_SIZE;
    if (next == buf_tail) return;   /* buffer full - drop the keystroke */
    buffer[buf_head] = c;
    buf_head = next;
}

static int buffer_pop(char *out) {
    if (buf_tail == buf_head) return 0;   /* empty */
    *out = buffer[buf_tail];
    buf_tail = (buf_tail + 1) % BUFFER_SIZE;
    return 1;
}

static void keyboard_callback(registers_t *regs) {
    (void)regs;
    uint8_t scancode = inb(KBD_DATA_PORT);

    if (scancode == LSHIFT_MAKE || scancode == RSHIFT_MAKE) {
        shift_held = 1;
        return;
    }
    if (scancode == LSHIFT_BREAK || scancode == RSHIFT_BREAK) {
        shift_held = 0;
        return;
    }
    if (scancode & BREAK_BIT) {
        return;   /* ignore all other key-release events */
    }
    if (scancode >= 128) {
        return;   /* extended (0xE0-prefixed) codes - not handled here */
    }

    char c = shift_held ? scancode_ascii_shift[scancode] : scancode_ascii[scancode];
    if (c != 0) {
        buffer_push(c);
    }
}

void keyboard_init(void) {
    irq_install_handler(1, keyboard_callback);
}

int keyboard_try_getchar(char *out) {
    return buffer_pop(out);
}

char keyboard_getchar(void) {
    char c;
    while (!buffer_pop(&c)) {
        __asm__ volatile ("sti; hlt");   /* sleep until the next interrupt */
    }
    return c;
}
