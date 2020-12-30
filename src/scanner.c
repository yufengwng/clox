#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

typedef struct {
    const char* start;
    const char* current;
    size_t line;
} Scanner;

Scanner scanner;    // singleton

void scanner_init(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}
