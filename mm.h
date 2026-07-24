#include <stddef.h>

#define ALIGNMENT 8UL

int mm_init(void);

void *mm_malloc(size_t size);

void mm_free(void *ptr);

void *mm_realloc(void* ptr, size_t size);

int mm_checkheap(int verbose);
