#include <stdio.h>

#include "compiler.h"
#include "scanner.h"

void compile(const char* source) {
    scanner_init(source);
#ifdef DEBUG_TRACE_EXECUTION
    size_t line = 0;
    while (true) {
        Token token = scan_token();
        if (token.line != line) {
            printf("%4zu ", token.line);
            line = token.line;
        } else {
            printf("   | ");
        }
        printf("%2d '%.*s'\n", token.type, (int) token.length, token.start);
        if (token.type == TOKEN_ERR || token.type == TOKEN_EOF) {
            break;
        }
    }
#endif
}
