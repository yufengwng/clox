#pragma once

#include "common.h"
#include "chunk.h"
#include "value.h"

#define STACK_MAX 256

typedef struct {
    Chunk* chunk;
    uint8_t* ip;
    Value stack[STACK_MAX];
    Value* stack_top;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

void vm_init();
void vm_free();
InterpretResult vm_interpret(Chunk* chunk);

void stack_push(Value value);
Value stack_pop();
