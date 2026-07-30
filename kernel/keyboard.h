#ifndef KEYBOARD_H
#define KEYBOARD_H

void keyboard_init(void);

/* Blocks (via hlt, not a busy spin) until a key has been typed,
 * then returns its translated ASCII value. Backspace is returned
 * as '\b' and Enter as '\n'; shift is handled internally and never
 * surfaces as its own character. */
char keyboard_getchar(void);

/* Non-blocking: returns 1 and writes *out if a character is
 * waiting, otherwise returns 0 immediately. */
int keyboard_try_getchar(char *out);

#endif /* KEYBOARD_H */
