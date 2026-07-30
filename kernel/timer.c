#include "timer.h"
#include "isr.h"
#include "io.h"

#define PIT_CHANNEL0_DATA 0x40
#define PIT_COMMAND       0x43
#define PIT_BASE_FREQ     1193182u   /* PIT's fixed input clock, in Hz */

static volatile uint32_t tick_count = 0;
static uint32_t configured_freq = 0;

static void timer_callback(registers_t *regs) {
    (void)regs;
    tick_count++;
}

void timer_init(uint32_t freq) {
    configured_freq = freq;
    irq_install_handler(0, timer_callback);

    uint32_t divisor = PIT_BASE_FREQ / freq;

    outb(PIT_COMMAND, 0x36);   /* channel 0, lobyte/hibyte, mode 3 (square wave) */
    outb(PIT_CHANNEL0_DATA, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0_DATA, (uint8_t)((divisor >> 8) & 0xFF));
}

uint32_t timer_get_ticks(void) {
    return tick_count;
}

uint32_t timer_get_frequency(void) {
    return configured_freq;
}

void timer_sleep(uint32_t ticks) {
    uint32_t target = tick_count + ticks;
    while (tick_count < target) {
        __asm__ volatile ("sti; hlt");
    }
}
