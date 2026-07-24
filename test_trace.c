/* test_trace.c - deterministic smoke test.
 *
 * Replays one small hand-written trace of malloc/free/realloc operations and
 * asserts alignment and payload preservation after each. Fixed and readable by
 * design: when this fails the failing operation is obvious, which makes it the
 * first thing to run before the randomized test in test_random.c.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "memlib.h"
#include "mm.h"

#define MAX_PTRS 1024

/* track logical id -> real pointer + size.
 * so we can free() by id and verify contents on realloc */
static void *ptrs[MAX_PTRS];
static size_t sizes[MAX_PTRS];

typedef enum {OP_MALLOC, OP_FREE, OP_REALLOC} op_t;

typedef struct {
    op_t op;
    int id; /* which logical pointer this slot refers to */
    size_t size; /* for MALLOC / REALLOC */
} trace_op_t;

static trace_op_t trace[] = {
    {OP_MALLOC, 0, 16},
    {OP_MALLOC, 1, 543},
    {OP_MALLOC, 2, 12},
    {OP_FREE, 1, 0},
    {OP_FREE, 0, 0},
    {OP_MALLOC, 0, 100},
    {OP_REALLOC, 0, 50},
    {OP_FREE, 0, 0},
    {OP_FREE, 2, 0},
    {OP_REALLOC, 3, 23}, // id 3 never malloc'd -> ptrs[3] == NULL, tests realloc(NUll, n) == maclloc(n)
    {OP_REALLOC, 3, 67},
    {OP_REALLOC, 3, 0}
};


int main(void) {
    mem_init();

    if (mm_init() != 0) {
        fprintf(stderr, "mm_init failed\n");
        return 1;
    }

    int n = sizeof(trace) / sizeof(trace[0]);

    for (int i = 0; i < n; i++) {
        trace_op_t t = trace[i];
        switch (t.op) {
            case OP_MALLOC:{
                void *ptr = mm_malloc(t.size);
                memset(ptr, t.id & 0xFF, t.size); /* label the payload to  */
                assert(ptr != NULL);
                assert((size_t)ptr % 8 == 0);

                sizes[t.id] = t.size;
                ptrs[t.id] = ptr;

                break;
            }
            case OP_FREE: {
                mm_free(ptrs[t.id]);
                ptrs[t.id] = NULL;
                sizes[t.id] = 0;
                break;
            }
            case OP_REALLOC: {
                size_t old_size = sizes[t.id];
                size_t new_size = t.size;

                void *old_ptr = ptrs[t.id];

                void *new_ptr = mm_realloc(old_ptr, new_size);

                if (new_size == 0) { //free
                    assert(new_ptr == NULL);
                    ptrs[t.id] = NULL;
                    sizes[t.id] = 0;
                } else {
                    assert(new_ptr != NULL);
                    assert(((size_t)new_ptr % 8) == 0);

                    /* verify old contents were preserved up to min(old_size, new_size) */

                    size_t copy_size = old_size < new_size ? old_size : new_size;
                    unsigned char expected = (unsigned char)(t.id & 0xFF);
                    for (size_t j = 0; j < copy_size; j++) {
                        assert(((unsigned char *)new_ptr)[j] == expected);
                    }
                    /* fill the block with the pattern again,
                     * in case new_size > old_size
                     */
                    memset(new_ptr, expected, new_size);
                    ptrs[t.id] = new_ptr;
                    sizes[t.id] = new_size;
                }
                break;
            }
        }
    }

    mem_deinit();
    printf("done\n");
    return 0;
}
