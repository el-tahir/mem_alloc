/* mm.h - public interface to the allocator implemented in mm.c.
 *
 * Exposes the four malloc-style entry points plus a heap consistency checker.
 * Everything else (block layout, free lists, coalescing) is private to mm.c.
 * The allocator runs on the simulated heap from memlib.h, not the system heap.
 */

#ifndef MM_H
#define MM_H

#include <stddef.h>

/* alignment guarantee, in bytes, for every pointer mm_malloc/mm_realloc return */
#define ALIGNMENT 8UL

/* builds the initial empty heap; returns 0 on success, -1 if the heap can't grow.
 * must be called after mem_init() and before any other mm_ call */
int mm_init(void);

void *mm_malloc(size_t size);

void mm_free(void *ptr);

void *mm_realloc(void *ptr, size_t size);

/* audits every heap invariant; returns 1 if the heap is consistent, 0 if not.
 * verbose != 0 prints the first violation found to stderr */
int mm_checkheap(int verbose);

#endif /* MM_H */
