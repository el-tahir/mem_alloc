/* memlib.h - interface to the simulated heap implemented in memlib.c.
 *
 * Models the kernel's sbrk() over a fixed private region so the allocator can
 * be exercised without touching the real process heap. Test drivers call
 * mem_init() once at startup; mm.c calls mem_sbrk() and the bounds queries.
 */

#ifndef MEMLIB_H
#define MEMLIB_H

#include <stddef.h>

void mem_init(void);
void mem_deinit(void);
/* rewinds the break to the start of the region, discarding every block, so one
 * process can run repeated tests against a fresh heap */
void mem_reset_brk(void);
/* grows the heap by incr bytes and returns the OLD break (start of the new
 * area), or (void *)-1 on failure */
void *mem_sbrk(int incr);
void *mem_heap_lo(void);
/* NOTE: inclusive - returns the last valid heap byte, not one past it */
void *mem_heap_hi(void);
size_t mem_heapsize(void);
size_t mem_pagesize(void);

#endif /* MEMLIB_H */
