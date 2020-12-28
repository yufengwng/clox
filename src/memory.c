#include <stdio.h>
#include <stdlib.h>

#include "memory.h"

void* reallocate(void* prev_ptr, size_t old_size, size_t new_size) {
    if (new_size == 0) {
        free(prev_ptr);
        return NULL;
    }
    void* ptr = realloc(prev_ptr, new_size);
    if (ptr == NULL) {
        fprintf(stderr, "[lox] error: failed to reallocate memory\n");
        exit(1);
    }
    return ptr;
}
