#pragma once

#include "common.h"
#include "object.h"
#include "value.h"

#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, ptr) \
    reallocate(ptr, sizeof(type), 0);

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, prev_ptr, old_count, new_count) \
    (type*)reallocate(prev_ptr, sizeof(type) * (old_count), sizeof(type) * (new_count))

#define FREE_ARRAY(type, ptr, old_count) \
    reallocate(ptr, sizeof(type) * (old_count), 0)

void* reallocate(void* prev_ptr, size_t old_size, size_t new_size);
void free_objects();
void gc_collect();
void gc_mark_value(Value value);
void gc_mark_object(Obj* object);
