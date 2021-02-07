#pragma once

#include "common.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
    ObjClosure* closure;
    uint8_t* ip;
    Value* slots;
} CallFrame;

typedef struct {
    CallFrame frames[FRAMES_MAX];
    size_t frame_count;
    Value stack[STACK_MAX];
    Value* stack_top;
    ObjUpvalue* open_upvalues;
    Table globals;
    Table strings;
    Obj* objects;
    size_t gray_count;
    size_t gray_capacity;
    Obj** gray_stack;
    size_t bytes_allocated;
    size_t gc_threshold;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

extern VM vm;

void init_vm();
void free_vm();
InterpretResult vm_interpret(const char* source);

void stack_push(Value value);
Value stack_pop();
