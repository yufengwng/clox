#include <stdarg.h>
#include <stdio.h>

#include "compiler.h"
#include "debug.h"
#include "vm.h"

VM vm;  // singleton

void stack_push(Value value) {
    *vm.stack_top = value;
    vm.stack_top++;
}

Value stack_pop() {
    vm.stack_top--;
    return *vm.stack_top;
}

static void stack_reset() {
    vm.stack_top = vm.stack;
}

static Value stack_peek(size_t distance) {
    return vm.stack_top[-1 - distance];
}

static void runtime_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    size_t line = vm.chunk->lines[instruction];
    fprintf(stderr, "[line %zu] in script\n", line);

    stack_reset();
}

#ifdef DEBUG_TRACE_EXECUTION
static void print_stack() {
    if (vm.stack_top == vm.stack) {
        printf("[ ]");
    } else {
        for (Value* curr = vm.stack; curr < vm.stack_top; curr++) {
            printf("[ ");
            print_value(*curr);
            printf(" ]");
        }
    }
    printf("\n");
}
#endif

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(value_type, op) \
    do { \
        if (!IS_NUMBER(stack_peek(0)) || !IS_NUMBER(stack_peek(1))) { \
            runtime_error("Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = RAW_NUMBER(stack_pop()); \
        double a = RAW_NUMBER(stack_pop()); \
        stack_push(value_type(a op b)); \
    } while (false)

    while (true) {
#ifdef DEBUG_TRACE_EXECUTION
        print_stack();
        disassemble_instruction(vm.chunk, (size_t)(vm.ip - vm.chunk->code));
#endif
        uint8_t instruction = READ_BYTE();
        switch (instruction) {
            case OP_RETURN: {
                print_value(stack_pop());
                printf("\n");
                return INTERPRET_OK;
            }
            case OP_ADD:        BINARY_OP(BOX_NUMBER, +); break;
            case OP_SUBTRACT:   BINARY_OP(BOX_NUMBER, -); break;
            case OP_MULTIPLY:   BINARY_OP(BOX_NUMBER, *); break;
            case OP_DIVIDE:     BINARY_OP(BOX_NUMBER, /); break;
            case OP_NEGATE:
                if (!IS_NUMBER(stack_peek(0))) {
                    runtime_error("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                stack_push(BOX_NUMBER(-RAW_NUMBER(stack_pop())));
                break;
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                stack_push(constant);
                break;
            }
        }
    }

#undef BINARY_OP
#undef READ_CONSTANT
#undef READ_BYTE
}

void vm_init() {
    stack_reset();
}

void vm_free() {
}

InterpretResult vm_interpret(const char* source) {
    Chunk chunk;
    chunk_init(&chunk);

    if (!compile(source, &chunk)) {
        chunk_free(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;
    InterpretResult result = run();

    chunk_free(&chunk);
    return result;
}
