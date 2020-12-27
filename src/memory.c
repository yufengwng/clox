#include <stdlib.h>

#include "common.h"
#include "memory.h"

void* reallocate(void* prev_ptr, size_t old_size, size_t new_size) {
    if (new_size == 0) {
        free(prev_ptr);
        return NULL;
    }
    return realloc(prev_ptr, new_size);
}
