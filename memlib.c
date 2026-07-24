/* memlib.c - simulated heap emulation.
 *
 * Reserves one fixed-size private region up front and hands it out through a
 * model of sbrk(), so mm.c can be tested against a heap with known bounds and
 * no interference from the real allocator. Failures here are the emulator's
 * own limits (out of region, bad argument), never allocator bugs.
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "memlib.h"

#define MAX_HEAP (100 * (1 << 20)) /* 100 mb */

/* simulated heap state */
static char *mem_start; /* first legal heap byte */
static char *mem_brk; /* current end of allocated heap (exclusive) */
static char *mem_max; /* one past lsat legal heap byte */


void mem_init(void) {
    mem_start = mmap(NULL, MAX_HEAP, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (mem_start == MAP_FAILED) {
        fprintf(stderr, "mem_init: mmap failed: %s\n", strerror(errno));
        exit(1);
    }

    mem_brk = mem_start;
    mem_max = mem_start + MAX_HEAP;
}

void mem_deinit(void) {
    if (mem_start != NULL) {
        munmap(mem_start, MAX_HEAP);
        mem_start = NULL;
    }
}

/* reset brk to start of region without unmapping - to rerun a fresh "heap"
 * many times in one process for testing */

void mem_reset_brk(void) {
    mem_brk = mem_start;
}

/* simple model of the sbrk() syscall. extends the heap by incr bytes
 * and returns the start address of the new area.
 * negative incr not supported
 */
void *mem_sbrk(int incr) {
    char *old_brk = mem_brk;

    if (incr < 0) {
        fprintf(stderr, "mem_sbrk: negative incr not supported\n");
        errno = EINVAL;
        return (void *)-1;
    }

    if (mem_brk + incr > mem_max) {
        fprintf(stderr, "mem_sbrk: out of memory\n");
        errno = ENOMEM;
        return (void *)-1;
    }

    mem_brk += incr;

    return old_brk;

}

void *mem_heap_lo(void) {
    return mem_start;
}

void *mem_heap_hi(void) {
    return mem_brk - 1;
}

size_t mem_heapsize(void) {
    return (size_t)(mem_brk - mem_start);
}

size_t mem_pagesize(void) {
    return (size_t) getpagesize();
}
