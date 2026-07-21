#include "mm.h"
#include "memlib.h"
#include <string.h>

#define WSIZE 4 /* size of one header / footer */
#define DSIZE 8  /* double word size */
#define CHUNKSIZE (1 << 12) /* amount to extend heap by when out of space */
#define MINBLOCK (4 * DSIZE) /* header + prev + next + footer = 32 */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* ------------- Alignment -------------- */
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

/* ---------- pack size + alloc bit into one word --------- */
#define PACK(size, alloc) ((size) | (alloc))

/* ----------- read/write a size_t at address p ------------*/
#define GET(p)      (*(size_t *)(p))
#define PUT(p, val) (*(size_t *)(p) = (size_t)(val))


/* ------------ extract size / alloc bit from a header or footer --------- */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* ------------ given block pointer bp, compute header and footer addresses ---------*/
#define HDRP(bp) ((char *)(bp) - DSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - 2 * DSIZE)

/* ----------- given block pointer bp, find adjacent PHYSICAL blocks --------*/
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - DSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - 2 * DSIZE)))

/* ----------- free-list pointers, stored inside a FREE block's payload -------- */
/* layout inside a free block's payload region: [prev (8B)][ next (8B)] */
#define GET_PREV(bp)        (*(void **)(bp))
#define GET_NEXT(bp)        (*(void **)((char *)(bp) + DSIZE))
#define SET_PREV(bp, val)   (*(void **)(bp) = (void *)(val))
#define SET_NEXT(bp, val)   (*(void **)((char *)(bp) + DSIZE) = (void *)(val))

int mm_init(void) {
    return 0;
}

void *mm_malloc(size_t size) {
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void*) -1) return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

void mm_free(void *ptr) {

}

void *mm_realloc(void* ptr, size_t size) {

    if (ptr == NULL) {
        return mm_malloc(size);
    }

    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    void *oldptr = ptr;
    void *newptr;
    size_t copysize;

    newptr = mm_malloc(size);
    if (newptr == NULL) return NULL;

    copysize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copysize) copysize = size;

    memcpy(newptr, oldptr, copysize);
    mm_free(oldptr);
    return newptr;


}
