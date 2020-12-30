#pragma once

#include "common.h"
#include "chunk.h"

typedef struct {
    Chunk* chunk;
    uint8_t* ip;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

void vm_init();
void vm_free();
InterpretResult vm_interpret(Chunk* chunk);
