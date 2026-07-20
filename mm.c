#include "mm.h"
#include "memlib.h"
#include <string.h>

#define ALIGNMENT 8
#define ALIGN(size) ((((size) + (ALIGNMENT - 1)) & ~0x7))
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

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
