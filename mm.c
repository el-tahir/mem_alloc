#include "mm.h"
#include "memlib.h"
#include <string.h>

#define WSIZE sizeof(size_t) /* size of one header / footer */
#define DSIZE (2 * WSIZE) /* double word size */
#define CHUNKSIZE (1UL << 12) /* amount to extend heap by when out of space */
#define MINBLOCK (4 * WSIZE) /* header + prev + next + footer = 32 */

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* ------------- Alignment -------------- */
#define ALIGNMENT 8UL
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
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* ----------- free-list pointers, stored inside a FREE block's payload -------- */
/* layout inside a free block's payload region: [prev (8B)][ next (8B)] */
#define GET_PREV(bp)        (*(void **)(bp))
#define GET_NEXT(bp)        (*(void **)((char *)(bp) + WSIZE))
#define SET_PREV(bp, val)   (GET_PREV(bp) = (void *)(val))
#define SET_NEXT(bp, val)   (GET_NEXT(bp) = (void *)(val))


static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static int  get_class_index(size_t asize);
static void insert_node(void *bp);
static void remove_node(void *bp);
