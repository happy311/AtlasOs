#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

/* Programs PIT channel 0 to fire IRQ0 `freq` times a second and
 * registers our handler for it. */
void timer_init(uint32_t freq);

uint32_t timer_get_ticks(void);
uint32_t timer_get_frequency(void);

/* Busy-waits (via hlt, so the CPU isn't spinning at 100%) until at
 * least `ticks` timer interrupts have fired. */
void timer_sleep(uint32_t ticks);

#endif /* TIMER_H */
