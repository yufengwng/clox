#include <stdio.h>

#include "debug.h"
#include "vm.h"

VM vm;  // singleton

void vm_init() {
}

void vm_free() {
}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

    while (true) {
#ifdef DEBUG_TRACE_EXECUTION
        disassemble_instruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif
        uint8_t instruction = READ_BYTE();
        switch (instruction) {
            case OP_RETURN: {
                return INTERPRET_OK;
            }
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                print_value(constant);
                printf("\n");
                break;
            }
        }
    }

#undef READ_CONSTANT
#undef READ_BYTE
}

InterpretResult vm_interpret(Chunk* chunk) {
    vm.chunk = chunk;
    vm.ip = vm.chunk->code;
    return run();
}
