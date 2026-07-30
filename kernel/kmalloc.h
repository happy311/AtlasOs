#ifndef KMALLOC_H
#define KMALLOC_H

#include <stddef.h>

/* A minimal first-fit, splitting/coalescing free-list allocator
 * over a fixed physical arena. This is deliberately not a slab or
 * buddy allocator - it's the smallest design that actually
 * demonstrates the core allocator concepts (headers, splitting,
 * coalescing, fragmentation) rather than hiding them behind a
 * library call. */
void kmalloc_init(void);
void *kmalloc(size_t size);
void kfree(void *ptr);

/* Reports current heap usage in bytes, for the shell's `mem`
 * command. */
void kmalloc_stats(size_t *used_bytes, size_t *free_bytes);

#endif /* KMALLOC_H */
