#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "memlib.h"
#include "mm.h"

#define MAXLIVE 1024
#define NOPS 100000

typedef struct {
    void *ptr;
    size_t size;
    unsigned char tag;
} live_entry_t;

live_entry_t live[MAXLIVE];

void do_malloc(int slot, size_t size) {

    void *ptr = mm_malloc(size);
    assert(ptr != NULL);
    assert((uintptr_t)ptr % ALIGNMENT == 0);
    unsigned char tag = (unsigned char)slot;
    memset(ptr, tag, size);
    live[slot] = (live_entry_t){ptr, size, tag};

}

void do_free(int slot) {

    size_t size = live[slot].size;

    for (size_t i = 0; i < size; i++) {
        assert(*((unsigned char *)live[slot].ptr + i) == live[slot].tag);
    }

    mm_free(live[slot].ptr);
    live[slot].ptr = NULL;
    live[slot].size = 0;

}

void do_realloc(int slot, size_t newsize) {

    // newsize will be > 0, to avoid realloc == free case

    live_entry_t old = live[slot];
    void *new_ptr = mm_realloc(old.ptr, newsize);

    assert(new_ptr != NULL);
    assert((uintptr_t)new_ptr % ALIGNMENT == 0);
    size_t check = old.size < newsize ? old.size : newsize;
    for (size_t i = 0; i < check; i++) {
        assert(*((unsigned char *)new_ptr + i) == old.tag);
    }
    memset(new_ptr, old.tag, newsize);
    live[slot] = (live_entry_t){new_ptr, newsize, old.tag};


}

int pick_slot(int want_live) {
 // want_live == 1 -> random live spot, want_live == 0 -> random free spot

 int candidates[MAXLIVE];
 int n = 0;
    for (int i = 0; i < MAXLIVE; i++) {
        int is_live = (live[i].ptr != NULL);
        if (is_live == want_live)
            candidates[n++] = i;
    }
    if (n == 0) return -1;
    return candidates[rand() % n];
}


size_t random_size(void) {
    if (rand() % 10 == 0) return 1 + rand() % 8000; // ~10% large
    return 1 + rand() % 256;                        // ~90% small
}


int main(int argc, char **argv) {

    int seed = argc > 1 ? atoi(argv[1]) : 0;

    srand(seed);
    mem_init();

    if (mm_init() != 0) {
        fprintf(stderr, "mem_init failed\n");
        return -1;
    }

    memset(live, 0, sizeof(live)); // all slots free
    int n_live = 0;

    for (int i = 0; i < NOPS; i++) {

        double r = (double)rand() / RAND_MAX;
        double threshold = (double)n_live / MAXLIVE;

        if (r < threshold) { // randomly favor free/realloc as more slots become live
            // free-or-realloc path
            int slot = pick_slot(1);
            if (slot < 0) continue;
            if (rand() % 2) {
                do_free(slot);
                n_live--;
            } else {
                size_t sz = random_size(); // > 0 always
                do_realloc(slot, sz);
            }
        } else {
            // malloc path
            int slot = pick_slot(0);
            if (slot < 0) continue;
            do_malloc(slot, random_size());
            n_live++;
        }

        if (!mm_checkheap(1)) {
            fprintf(stderr, "checkheap failed after op %d (seed %d)\n", i, seed);
            return 1;
        }
    }

    // drain everything still live
    for (int s = 0; s < MAXLIVE; s++) {
        if (live[s].ptr != NULL) do_free(s);
    }

    if (!mm_checkheap(1)) {
        fprintf(stderr, "checkheap failed after drain\n");
        return 1;
    }

    printf("ok: %d ops, seed %d\n", NOPS, seed);
    return 0;
}
