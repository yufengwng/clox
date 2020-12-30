#pragma once

#include "common.h"
#include "value.h"

#define MAX_CONSTANTS 256

typedef enum {
    OP_CONSTANT,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NEGATE,
    OP_RETURN,
} OpCode;

typedef struct {
    uint8_t* code;
    size_t* lines;
    size_t capacity;
    size_t count;
    ValueArray constants;
} Chunk;

void chunk_init(Chunk* chunk);
void chunk_free(Chunk* chunk);
void chunk_write(Chunk* chunk, uint8_t byte, size_t line);

uint8_t add_constant(Chunk* chunk, Value value);
