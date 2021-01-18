#pragma once

#include "common.h"
#include "value.h"

typedef enum {
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_POP,
    OP_DEFINE_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_LOCAL,
    OP_GET_LOCAL,
    OP_EQUAL,
    OP_LESS,
    OP_GREATER,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NEGATE,
    OP_NOT,
    OP_PRINT,
    OP_LOOP,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_RETURN,
} OpCode;

typedef struct {
    size_t count;
    size_t capacity;
    uint8_t* code;
    size_t* lines;
    ValueArray constants;
} Chunk;

void init_chunk(Chunk* chunk);
void free_chunk(Chunk* chunk);
void chunk_write(Chunk* chunk, uint8_t byte, size_t line);

size_t add_constant(Chunk* chunk, Value value);
