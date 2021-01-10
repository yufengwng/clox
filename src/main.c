#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

#define ERR_USAGE 64
#define ERR_DATAERR 65
#define ERR_SOFTWARE 70
#define ERR_IOERR 74

#define REPL_LINE_MAX 1024

static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "[lox] error: could not open file '%s'\n", path);
        exit(ERR_IOERR);
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(file_size + 1);
    if (buffer == NULL) {
        fprintf(stderr, "[lox] error: not enough memory to read '%s'\n", path);
        exit(ERR_IOERR);
    }
    size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
    if (bytes_read < file_size) {
        fprintf(stderr, "[lox] error: could not read file '%s'\n", path);
        exit(ERR_IOERR);
    }
    buffer[bytes_read] = '\0';

    fclose(file);
    return buffer;
}

static void run_file(const char* path) {
    char* source = read_file(path);
    InterpretResult result = vm_interpret(source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) {
        exit(ERR_DATAERR);
    }
    if (result == INTERPRET_RUNTIME_ERROR) {
        exit(ERR_SOFTWARE);
    }
}

static void run_repl() {
    char line[REPL_LINE_MAX];
    while (true) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }
        vm_interpret(line);
    }
}

int main(int argc, const char* argv[]) {
    init_vm();

    if (argc == 1) {
        run_repl();
    } else if (argc == 2) {
        run_file(argv[1]);
    } else {
        fprintf(stderr, "Usage: clox [path]\n");
        exit(ERR_USAGE);
    }

    free_vm();
    return 0;
}
