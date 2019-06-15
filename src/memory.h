#ifndef clox_memory_h
#define clox_memory_h

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, previous, old_count, count) \
    (type*) reallocate(previous, sizeof(type) * (old_count), sizeof(type) * (count))

#define FREE_ARRAY(type, pointer, old_count) \
    reallocate(pointer, sizeof(type) * (old_count), 0)

void* reallocate(void* previous, size_t old_size, size_t new_size);

#endif
