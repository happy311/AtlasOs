#include "kmalloc.h"
#include <stdint.h>

/* The heap lives at a fixed physical address well above the kernel
 * (loaded+linked at 0x10000, plus its 16 KB stack - see entry.asm)
 * and well below the 32-bit address space ceiling. It is *not*
 * part of the kernel image: nothing here is emitted into kernel.bin,
 * these are just addresses of ordinary RAM that the allocator takes
 * ownership of at runtime. QEMU's default RAM (128 MB) comfortably
 * covers it. */
#define HEAP_START 0x00400000u   /* 4 MB */
#define HEAP_SIZE  0x00100000u   /* 1 MB arena */

typedef struct block_header {
    size_t size;                 /* usable bytes in this block (excludes header) */
    int free;
    struct block_header *next;
} block_header_t;

#define HEADER_SIZE ((size_t)sizeof(block_header_t))
#define MIN_SPLIT_REMAINDER 16   /* don't split off slivers smaller than this */

static block_header_t *heap_head = 0;

void kmalloc_init(void) {
    heap_head = (block_header_t *)HEAP_START;
    heap_head->size = HEAP_SIZE - HEADER_SIZE;
    heap_head->free = 1;
    heap_head->next = 0;
}

void *kmalloc(size_t size) {
    if (size == 0) return 0;

    /* keep every returned pointer at least word-aligned */
    size = (size + 3) & ~((size_t)3);

    block_header_t *cur = heap_head;
    while (cur) {
        if (cur->free && cur->size >= size) {
            size_t remainder = cur->size - size;
            if (remainder >= HEADER_SIZE + MIN_SPLIT_REMAINDER) {
                block_header_t *split =
                    (block_header_t *)((uint8_t *)cur + HEADER_SIZE + size);
                split->size = remainder - HEADER_SIZE;
                split->free = 1;
                split->next = cur->next;

                cur->size = size;
                cur->next = split;
            }
            cur->free = 0;
            return (uint8_t *)cur + HEADER_SIZE;
        }
        cur = cur->next;
    }
    return 0;   /* out of memory */
}

void kfree(void *ptr) {
    if (!ptr) return;

    block_header_t *blk = (block_header_t *)((uint8_t *)ptr - HEADER_SIZE);
    blk->free = 1;

    /* single pass, merge every run of adjacent free blocks - simple
     * and correct since the list is kept in address order by
     * construction (kmalloc only ever splits forward). */
    block_header_t *cur = heap_head;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            cur->size += HEADER_SIZE + cur->next->size;
            cur->next = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

void kmalloc_stats(size_t *used_bytes, size_t *free_bytes) {
    size_t used = 0, freeb = 0;
    block_header_t *cur = heap_head;
    while (cur) {
        if (cur->free) freeb += cur->size;
        else            used  += cur->size;
        cur = cur->next;
    }
    if (used_bytes) *used_bytes = used;
    if (free_bytes) *free_bytes = freeb;
}
