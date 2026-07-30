#include "shell.h"
#include "vga.h"
#include "keyboard.h"
#include "serial.h"
#include "kmalloc.h"
#include "timer.h"
#include "string.h"
#include "io.h"
#include <stddef.h>
#include <stdint.h>

#define LINE_MAX 128

static void print_prompt(void) {
    vga_print_colored("atlas> ", 0x0A);
}

static void read_line(char *buf, int max_len) {
    int len = 0;
    for (;;) {
        char c = keyboard_getchar();

        if (c == '\n') {
            vga_putc('\n');
            buf[len] = '\0';
            return;
        }
        if (c == '\b') {
            if (len > 0) {
                len--;
                vga_backspace();
            }
            continue;
        }
        if (len < max_len - 1) {
            buf[len++] = c;
            vga_putc(c);
        }
    }
}

/* Splits `line` in place at the first space; *cmd points at the
 * command word, *arg at whatever follows it (or an empty string if
 * there's nothing else). */
static void split_command(char *line, char **cmd, char **arg) {
    *cmd = line;
    char *p = line;
    while (*p && *p != ' ') p++;
    if (*p == ' ') {
        *p = '\0';
        p++;
        while (*p == ' ') p++;
    }
    *arg = p;
}

static void print_number(uint32_t n) {
    char buf[16];
    utoa(n, buf, 10);
    vga_print(buf);
}

static void cmd_help(void) {
    vga_print(
        "Commands:\n"
        "  help              show this list\n"
        "  clear             clear the screen\n"
        "  echo <text>       print text back\n"
        "  mem               heap allocator stats (kmalloc arena)\n"
        "  uptime            seconds since boot (PIT timer)\n"
        "  trap              raise int3 to exercise the IDT/ISR path\n"
        "  serial <text>     write text out over COM1\n"
        "  color <0-15>      change the shell's text color\n"
        "  reboot            pulse the keyboard controller's reset line\n"
    );
}

static void cmd_mem(void) {
    size_t used = 0, freeb = 0;
    kmalloc_stats(&used, &freeb);
    vga_print("kmalloc arena: ");
    print_number((uint32_t)used);
    vga_print(" bytes used, ");
    print_number((uint32_t)freeb);
    vga_print(" bytes free\n");
}

static void cmd_uptime(void) {
    uint32_t ticks = timer_get_ticks();
    uint32_t freq  = timer_get_frequency();
    vga_print("uptime: ");
    print_number(freq ? ticks / freq : 0);
    vga_print(" s (");
    print_number(ticks);
    vga_print(" timer ticks)\n");
}

static void cmd_reboot(void) {
    vga_print("Rebooting...\n");
    /* The standard "8042 keyboard controller pulse reset line"
     * trick: bit 1 of the controller's output port is wired to the
     * CPU's RESET pin on real hardware, and QEMU emulates this. */
    uint8_t status;
    do {
        status = inb(0x64);
    } while (status & 0x02);
    outb(0x64, 0xFE);
    for (;;) {
        __asm__ volatile ("hlt");
    }
}

static void cmd_color(const char *arg) {
    int color = 0;
    while (*arg >= '0' && *arg <= '9') {
        color = color * 10 + (*arg - '0');
        arg++;
    }
    if (color < 0 || color > 15) {
        vga_print("usage: color <0-15>\n");
        return;
    }
    vga_set_color((uint8_t)color);
    vga_print("color changed.\n");
}

static void dispatch(char *line) {
    char *cmd, *arg;
    split_command(line, &cmd, &arg);

    if (cmd[0] == '\0') {
        return;
    } else if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "clear") == 0) {
        vga_clear();
    } else if (strcmp(cmd, "echo") == 0) {
        vga_print(arg);
        vga_putc('\n');
    } else if (strcmp(cmd, "mem") == 0) {
        cmd_mem();
    } else if (strcmp(cmd, "uptime") == 0) {
        cmd_uptime();
    } else if (strcmp(cmd, "trap") == 0) {
        __asm__ volatile ("int3");
    } else if (strcmp(cmd, "serial") == 0) {
        serial_write_string(arg);
        serial_write_string("\n");
        vga_print("sent over COM1.\n");
    } else if (strcmp(cmd, "color") == 0) {
        cmd_color(arg);
    } else if (strcmp(cmd, "reboot") == 0) {
        cmd_reboot();
    } else {
        vga_print_colored("Unknown command: ", 0x0C);
        vga_print_colored(cmd, 0x0C);
        vga_print_colored(" (try 'help')\n", 0x0C);
    }
}

void shell_run(void) {
    char line[LINE_MAX];

    vga_print_colored("\nType 'help' for a list of commands.\n\n", 0x0E);

    for (;;) {
        print_prompt();
        read_line(line, LINE_MAX);
        dispatch(line);
    }
}
