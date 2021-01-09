#include <stdio.h>

#include "compiler.h"
#include "debug.h"
#include "vm.h"

VM vm;  // singleton

static void stack_reset() {
    vm.stack_top = vm.stack;
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
#define BINARY_OP(op) \
    do { \
        Value b = stack_pop(); \
        Value a = stack_pop(); \
        stack_push(a op b); \
    } while (false)

    while (true) {
#ifdef DEBUG_TRACE_EXECUTION
        print_stack();
        disassemble_instruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif
        uint8_t instruction = READ_BYTE();
        switch (instruction) {
            case OP_RETURN: {
                print_value(stack_pop());
                printf("\n");
                return INTERPRET_OK;
            }
            case OP_ADD:        BINARY_OP(+); break;
            case OP_SUBTRACT:   BINARY_OP(-); break;
            case OP_MULTIPLY:   BINARY_OP(*); break;
            case OP_DIVIDE:     BINARY_OP(/); break;
            case OP_NEGATE:     stack_push(-stack_pop()); break;
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

void stack_push(Value value) {
    *vm.stack_top = value;
    vm.stack_top++;
}

Value stack_pop() {
    vm.stack_top--;
    return *vm.stack_top;
}
