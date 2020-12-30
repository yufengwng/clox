#include <stdio.h>

#include "debug.h"
#include "value.h"

static int constant_instruction(const char* name, Chunk* chunk, size_t offset) {
    uint8_t idx = chunk->code[offset + 1];
    printf("%-16s %4d '", name, idx);
    print_value(chunk->constants.values[idx]);
    printf("'\n");
    return offset + 2;
}

static int simple_instruction(const char* name, size_t offset) {
    printf("%s\n", name);
    return offset + 1;
}

int disassemble_instruction(Chunk* chunk, size_t offset) {
    printf("%04zu ", offset);
    if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
        printf("   | ");
    } else {
        printf("%4zu ", chunk->lines[offset]);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction) {
        case OP_CONSTANT:
            return constant_instruction("OP_CONSTANT", chunk, offset);
        case OP_RETURN:
            return simple_instruction("OP_RETURN", offset);
        default:
            printf("unknown opcode %d\n", instruction);
            return offset + 1;
    }
}

void disassemble_chunk(Chunk* chunk, const char* name) {
    printf("=== %s ===\n", name);

    size_t offset = 0;
    while (offset < chunk->count) {
        offset = disassemble_instruction(chunk, offset);
    }
}
