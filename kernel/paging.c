#include "paging.h"
#include <stdint.h>

#define PAGE_PRESENT   0x01
#define PAGE_WRITABLE  0x02
#define PAGE_SIZE_4MB  0x80

/* Must be page-aligned (4 KB) - the low 12 bits of CR3 are ignored
 * by the CPU and reused for flags, so an unaligned directory would
 * silently corrupt whichever flag bits landed there. 4 KB of BSS,
 * so unlike the heap this one *is* part of the kernel image. */
static uint32_t page_directory[1024] __attribute__((aligned(4096)));

void paging_init(void) {
    for (int i = 0; i < 1024; i++) {
        uint32_t phys_base = (uint32_t)i * 0x400000u;   /* i * 4 MB */
        page_directory[i] = phys_base | PAGE_PRESENT | PAGE_WRITABLE | PAGE_SIZE_4MB;
    }

    uint32_t cr4;
    __asm__ volatile ("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= 0x00000010;   /* CR4.PSE - enable 4 MB pages */
    __asm__ volatile ("mov %0, %%cr4" : : "r"(cr4));

    __asm__ volatile ("mov %0, %%cr3" : : "r"(page_directory));

    uint32_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;   /* CR0.PG - enable paging */
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
}
