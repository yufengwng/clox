#include <stdio.h>

#include "debug.h"
#include "vm.h"

VM vm;  // singleton

static void stack_reset() {
    vm.stack_top = vm.stack;
}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

    while (true) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("---- ");
        for (Value* curr = vm.stack; curr < vm.stack_top; curr++) {
            printf("[ ");
            print_value(*curr);
            printf(" ]");
        }
        printf("\n");
        disassemble_instruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif
        uint8_t instruction = READ_BYTE();
        switch (instruction) {
            case OP_RETURN: {
                print_value(stack_pop());
                printf("\n");
                return INTERPRET_OK;
            }
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                stack_push(constant);
                break;
            }
        }
    }

#undef READ_CONSTANT
#undef READ_BYTE
}

void vm_init() {
    stack_reset();
}

void vm_free() {
}

InterpretResult vm_interpret(Chunk* chunk) {
    vm.chunk = chunk;
    vm.ip = vm.chunk->code;
    return run();
}

void stack_push(Value value) {
    *vm.stack_top = value;
    vm.stack_top++;
}

Value stack_pop() {
    vm.stack_top--;
    return *vm.stack_top;
}
