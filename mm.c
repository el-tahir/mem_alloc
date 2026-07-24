/* mm.c - segregated free list allocator.
 *
 * Every block carries a matching header and footer holding its size plus an
 * allocated bit; those boundary tags let a block find its physical neighbours
 * in O(1), which is what makes immediate coalescing on free possible.
 *
 * Free blocks are additionally threaded onto one of NUM_CLASSES doubly linked
 * lists, bucketed by size, with the link pointers living inside the free
 * block's own payload (so they cost no extra space). Allocation searches the
 * class for the requested size and then larger classes, first fit within each.
 *
 * The heap is bracketed by an allocated prologue block and a zero-size
 * allocated epilogue header, which keep coalescing from walking off either end
 * without needing explicit boundary checks.
 */

#include "mm.h"
#include "memlib.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define WSIZE sizeof(size_t) /* size of one header / footer */
#define DSIZE (2 * WSIZE) /* double word size */
#define CHUNKSIZE (1UL << 12) /* amount to extend heap by when out of space */
#define MINBLOCK (4 * WSIZE) /* header + prev + next + footer = 32 */
#define NUM_CLASSES 9 /* number of size-bucketed free lists; last one is a catch-all */

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define MIN(x, y) ((x) < (y) ? (x) : (y))

/* ------------- Alignment -------------- */
#define ALIGN_MASK (ALIGNMENT - 1)
#define ALIGN(size) (((size) + ALIGN_MASK) & ~ALIGN_MASK)

/* ---------- pack size + alloc bit into one word --------- */
#define PACK(size, alloc) ((size) | (alloc))

/* ----------- read/write a size_t at address p ------------*/
#define GET(p)      (*(const size_t *)(p))
#define PUT(p, val) (*(size_t *)(p) = (size_t)(val))


/* ------------ extract size / alloc bit from a header or footer --------- */
#define GET_SIZE(p)  (GET(p) & ~(size_t)ALIGN_MASK)
#define GET_ALLOC(p) (GET(p) & (size_t)0x1)

/* ------------ given block pointer bp, compute header and footer addresses ---------*/
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* ----------- given block pointer bp, find adjacent PHYSICAL blocks --------*/
/* PREV_BLKP reads the PREVIOUS block's footer (the word before our header) to
 * learn its size - this is the whole reason blocks carry footers */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* ----------- free-list pointers, stored inside a FREE block's payload -------- */
/* layout inside a free block's payload region: [header][prev][next][footer] */
#define GET_PREV(bp)        (*(void **)(bp))
#define GET_NEXT(bp)        (*(void **)((char *)(bp) + WSIZE))
#define SET_PREV(bp, val)   (GET_PREV(bp) = (void *)(val))
#define SET_NEXT(bp, val)   (GET_NEXT(bp) = (void *)(val))

/* global variables */
static void *class_head[NUM_CLASSES];


static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static int  get_class_index(size_t size);
static void insert_node(void *bp);
static void remove_node(void *bp);


/* maps a block size to its free list, with class bounds doubling from MINBLOCK
 * upward; anything past the last bound lands in the final catch-all class */
static int get_class_index(size_t size) {
    int idx = 0;
    size_t upper = MINBLOCK;

    while (size > upper && idx < NUM_CLASSES - 1) {
        upper <<= 1;
        idx++;
    }

    return idx;
}

static void insert_node(void *bp) {
    size_t size = GET_SIZE(HDRP(bp));
    int idx = get_class_index(size);

    void *head = class_head[idx];

    SET_NEXT(bp, head);
    SET_PREV(bp, NULL);

    if (head != NULL) {
        SET_PREV(head, bp);
    }

    class_head[idx] = bp;
}

static void remove_node(void *bp) {

    size_t size = GET_SIZE(HDRP(bp));
    int idx = get_class_index(size);

    void *prev = GET_PREV(bp);
    void *next = GET_NEXT(bp);

    if (prev == NULL) {
        class_head[idx] = next;
        if (next != NULL) SET_PREV(next, NULL);
    } else {
        SET_NEXT(prev, next);
        if (next != NULL) SET_PREV(next, prev);
    }
}

/* merges bp with whichever physical neighbours are free, then inserts the
 * resulting block into a free list and returns it. callers must NOT insert bp
 * themselves; the merged block may start earlier than the bp passed in */
static void *coalesce(void *bp) {

    int prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    int next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        /* nothing to merge */

    } else if (prev_alloc && !next_alloc) {

        remove_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

    } else if (!prev_alloc && next_alloc) {

        remove_node(PREV_BLKP(bp));
        size += GET_SIZE(FTRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));

    } else {
        remove_node(PREV_BLKP(bp));
        remove_node(NEXT_BLKP(bp));
        size += GET_SIZE(FTRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    insert_node(bp);

    return bp;

}

/* grows the heap by words words (not bytes), reusing the old epilogue slot as
 * the new block's header and writing a fresh epilogue past it */
static void *extend_heap(size_t words) {

    size_t size = ALIGN(words * WSIZE);
    if (size < MINBLOCK) size = MINBLOCK;

    void *bp = mem_sbrk((int)size);
    if (bp == (void *)-1) return NULL;

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);

}

/* first fit within a class, escalating to larger classes until something fits */
static void *find_fit(size_t asize) {

    for (int idx = get_class_index(asize); idx < NUM_CLASSES; idx++) {
        for (void *bp = class_head[idx]; bp != NULL; bp = GET_NEXT(bp)) {
            if (GET_SIZE(HDRP(bp)) >= asize)
                return bp;
        }
    }

    return NULL;
}

/* marks free block bp allocated at asize bytes, splitting off the leftover as
 * a new free block when the remainder is big enough to stand on its own */
static void place(void *bp, size_t asize) {

    size_t current_size = GET_SIZE(HDRP(bp));
    size_t split_size = current_size - asize;
    remove_node(bp);

    if ((split_size) >= MINBLOCK) {

        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(split_size, 0));
        PUT(FTRP(bp), PACK(split_size, 0));
        coalesce(bp);
    } else {

        PUT(HDRP(bp), PACK(current_size, 1));
        PUT(FTRP(bp), PACK(current_size, 1));

    }

}


int mm_init(void) {

    for (int idx = 0; idx < NUM_CLASSES; idx++) {
        class_head[idx] = NULL;
    }

    char *heap_listp = mem_sbrk(4 * WSIZE);
    if (heap_listp == (void *)-1) return -1;

    PUT(heap_listp, 0); /* padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* prologue header*/
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1)); /* epilogue */

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    return 0;
}

void *mm_malloc(size_t size) {
    if (size == 0)
        return NULL;

    size_t asize = MAX(MINBLOCK, ALIGN(size + DSIZE));

    void *bp = find_fit(asize);
    if (bp == NULL) {
        size_t extend_size = MAX(asize, CHUNKSIZE);
        bp = extend_heap(extend_size / WSIZE); // extend_size is always a multiple of 8
        if (bp == NULL)
            return NULL;
    }

    place(bp, asize);
    return bp;

}

void mm_free(void *ptr) {

    if (ptr == NULL)
        return;

    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    coalesce(ptr);


}

void *mm_realloc(void *ptr, size_t size) {

    if (ptr == NULL)
        return mm_malloc(size);

    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    size_t old_size = GET_SIZE(HDRP(ptr));
    size_t asize = MAX(MINBLOCK, ALIGN(size + DSIZE));

    if (old_size >= asize)
        return ptr;

    /* see if we can grow into the next block if its free */
    void *next = NEXT_BLKP(ptr);
    if (!GET_ALLOC(HDRP(next)) && old_size + GET_SIZE(HDRP(next)) >= asize) {
        remove_node(next);
        size_t total = old_size + GET_SIZE(HDRP(next));

        if (total - asize >= MINBLOCK) {
            PUT(HDRP(ptr), PACK(asize, 1));
            PUT(FTRP(ptr), PACK(asize, 1));
            void *remainder = NEXT_BLKP(ptr);
            PUT(HDRP(remainder), PACK(total - asize, 0));
            PUT(FTRP(remainder), PACK(total - asize, 0));
            insert_node(remainder);
        } else {
            PUT(HDRP(ptr), PACK(total, 1));
            PUT(FTRP(ptr), PACK(total, 1));
        }
        return ptr;
    }

    /* otherwise just get a new block and copy */

    void *new_ptr = mm_malloc(size);
    if (new_ptr == NULL)
        return NULL;

    size_t copy_size = old_size - DSIZE;
    copy_size = MIN(copy_size, size);

    memcpy(new_ptr, ptr, copy_size);
    mm_free(ptr);

    return new_ptr;
}

/* two independent walks - once over physical blocks, once over the free lists -
 * and then a cross-check that both agree on how many free blocks exist, which
 * is what catches a block that was dropped from or double-linked into a list */
int mm_checkheap(int verbose) {
    // walk physical blocks
    void *bp = (char *)mem_heap_lo() + (4 * WSIZE); // first payload

    void *prologue_hdr = (char *)mem_heap_lo() + WSIZE;
    if (GET_SIZE(prologue_hdr) != DSIZE || !GET_ALLOC(prologue_hdr)) {
        if (verbose) fprintf(stderr, "error: bad prologue header at %p (size: %lu, alloc: %d)\n",
            prologue_hdr, (unsigned long)GET_SIZE(prologue_hdr), (int)GET_ALLOC(prologue_hdr));
        return 0;
    }

    void *prologue_ftr = (char *)mem_heap_lo() + (2 * WSIZE);
    if (GET_SIZE(prologue_ftr) != DSIZE || !GET_ALLOC(prologue_ftr)) {
        if (verbose) fprintf(stderr, "error: bad prologue footer at %p (size: %lu, alloc: %d)\n",
            prologue_ftr, (unsigned long)GET_SIZE(prologue_ftr), (int)GET_ALLOC(prologue_ftr));
        return 0;
    }

    int prev_free = 0;
    int free_blocks_physical_walk = 0;


    while (GET_SIZE(HDRP(bp)) > 0) {

        if ((GET_SIZE(HDRP(bp)) != GET_SIZE(FTRP(bp))) ||
            (GET_ALLOC(HDRP(bp)) != GET_ALLOC(FTRP(bp)))) {
            if (verbose) fprintf(stderr, "error: header and footer mismatch at block %p "
                "(header: size %lu, alloc %d | footer: size %lu, alloc %d)\n",
                bp,
                (unsigned long)GET_SIZE(HDRP(bp)), (int)GET_ALLOC(HDRP(bp)),
                (unsigned long)GET_SIZE(FTRP(bp)), (int)GET_ALLOC(FTRP(bp)));
            return 0;
        }

        size_t size = GET_SIZE(HDRP(bp));

        if (size % ALIGNMENT != 0) {
            if (verbose) fprintf(stderr, "error: size is not aligned at block %p (size: %lu)\n",
                bp, (unsigned long)size);
            return 0;
        }

        if ((uintptr_t)bp % ALIGNMENT != 0) {
            if (verbose) fprintf(stderr, "error: payload pointer is not aligned at %p\n", bp);
            return 0;
        }

        if (size < MINBLOCK) {
            if (verbose) fprintf(stderr, "error: size is less than MINBLOCK at block %p (size: %lu, MINBLOCK: %lu)\n",
                bp, (unsigned long)size, (unsigned long)MINBLOCK);
            return 0;
        }

        if (bp < mem_heap_lo() || bp > mem_heap_hi()) {
            if (verbose) fprintf(stderr, "error: block %p is outside of valid heap range [%p, %p]\n",
                bp, mem_heap_lo(), mem_heap_hi());
            return 0;
        }

        int current_free = !GET_ALLOC(HDRP(bp));

        if (current_free && prev_free) {
            if (verbose) fprintf(stderr, "error: consecutive free blocks found at or near block %p\n", bp);
            return 0;
        }

        free_blocks_physical_walk += current_free;
        prev_free = current_free;

        bp = NEXT_BLKP(bp);
    }

    if (GET_SIZE(HDRP(bp)) != 0 || !GET_ALLOC(HDRP(bp))) {
        if (verbose) fprintf(stderr, "error: bad epilogue header at %p (size: %lu, alloc: %d)\n",
            bp, (unsigned long)GET_SIZE(HDRP(bp)), (int)GET_ALLOC(HDRP(bp)));
        return 0;
    }


    // walk free lists
    int free_blocks_list_walk = 0;

    for (size_t idx = 0; idx < NUM_CLASSES; idx++) {
        void *fbp = class_head[idx];

        while (fbp != NULL) {

            size_t size = GET_SIZE(HDRP(fbp));
            void *next = GET_NEXT(fbp);
            void *prev = GET_PREV(fbp);

            if ((int)idx != get_class_index(size)) {
                if (verbose) fprintf(stderr, "error: block in wrong bin\n");
                return 0;
            }

            if (GET_ALLOC(HDRP(fbp)) != 0) {
                if (verbose) fprintf(stderr, "error: allocated block in free list\n");
                return 0;
            }

            if ((prev == NULL && class_head[idx] != fbp) ||
                (prev != NULL && GET_NEXT(prev) != fbp) ||
                (next != NULL && GET_PREV(next) != fbp)) {
                if (verbose) fprintf(stderr, "error: wrong links\n");
                return 0;
            }

            free_blocks_list_walk++;

            fbp = next;
        }

    }

    if (free_blocks_physical_walk > free_blocks_list_walk) {
        if (verbose) fprintf(stderr, "one or more free blocks not in list\n");
        return 0;
    } else if (free_blocks_physical_walk < free_blocks_list_walk) {
        if (verbose) fprintf(stderr, "one or more free blocks in one or more lists\n");
        return 0;
    }

    return 1;

}
