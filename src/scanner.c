#include <stdio.h>
#include <string.h>

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

static bool is_at_end() {
    return *scanner.current == '\0';
}

static Token make_token(TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (size_t) (scanner.current - scanner.start);
    token.line = scanner.line;
    return token;
}

static Token error_token(const char* message) {
    Token token;
    token.type = TOKEN_ERR;
    token.start = message;
    token.length = (size_t) strlen(message);
    token.line = scanner.line;
    return token;
}

Token scan_token() {
    scanner.start = scanner.current;

    if (is_at_end()) {
        return make_token(TOKEN_EOF);
    }

    return error_token("Unexpected character.");
}
