#ifndef SHELL_H
#define SHELL_H

/* Simple line-oriented command shell. Never returns - it's the
 * kernel's main loop once boot/init is done. */
void shell_run(void);

#endif /* SHELL_H */
