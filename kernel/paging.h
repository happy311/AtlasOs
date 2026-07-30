#ifndef PAGING_H
#define PAGING_H

/* Enables paging with a flat identity map (virtual address ==
 * physical address for the whole 4 GB space), using 4 MB pages
 * (PSE) so a single, statically-allocated page directory is
 * enough - no page tables, no allocator dependency, no dynamic
 * setup. This intentionally does *not* give the kernel any real
 * memory protection or virtual-address independence; it exists to
 * demonstrate turning paging on correctly (CR4.PSE, CR3, CR0.PG)
 * as a foundation, not as a full virtual memory manager. */
void paging_init(void);

#endif /* PAGING_H */
