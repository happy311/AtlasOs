#ifndef PIC_H
#define PIC_H

#include <stdint.h>

/* Remaps the two legacy 8259 PICs so hardware IRQs 0-15 land on
 * IDT vectors offset1..offset1+7 and offset2..offset2+7. This has
 * to happen before we ever unmask an IRQ: by default the BIOS
 * leaves IRQ0-7 mapped to interrupt vectors 8-15, which collide
 * head-on with CPU exceptions like #DF (8) and #GP (13). */
void pic_remap(uint8_t offset1, uint8_t offset2);

/* Every IRQ handler must call this before returning, or the PIC
 * will assume the interrupt is still being serviced and never
 * deliver another one on that line (or any lower-priority line). */
void pic_send_eoi(uint8_t irq);

/* Masks (disables) / unmasks (enables) a single IRQ line. */
void pic_set_mask(uint8_t irq);
void pic_clear_mask(uint8_t irq);

#endif /* PIC_H */
