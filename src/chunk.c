#include <stdio.h>
#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

void chunk_init(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    varr_init(&chunk->constants);
}

void chunk_free(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(size_t, chunk->lines, chunk->capacity);
    varr_free(&chunk->constants);
    chunk_init(chunk);
}

void chunk_write(Chunk* chunk, uint8_t byte, size_t line) {
    if (chunk->capacity < chunk->count + 1) {
        size_t old_capacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(old_capacity);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(size_t, chunk->lines, old_capacity, chunk->capacity);
    }
    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

size_t add_constant(Chunk* chunk, Value value) {
    varr_write(&chunk->constants, value);
    return chunk->constants.count - 1;
}
