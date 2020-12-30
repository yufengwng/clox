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

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_alphascore(char c) {
    return c == '_'
        || (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z');
}

static bool is_alphanumscore(char c) {
    return is_alphascore(c) || is_digit(c);
}

static char advance() {
    scanner.current++;
    return scanner.current[-1];
}

static char peek() {
    return *scanner.current;
}

static char peek_next() {
    if (is_at_end()) {
        return '\0';
    }
    return scanner.current[1];
}

static bool match(char expected) {
    if (is_at_end()) {
        return false;
    }
    if (*scanner.current != expected) {
        return false;
    }
    scanner.current++;
    return true;
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

static void skip_whitespace() {
    while (true) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;
            case '/':
                if (peek_next() != '/') {
                    return;
                }
                while (peek() != '\n' && !is_at_end()) {
                    advance();
                }
                break;
            default:
                return;
        }
    }
}

static TokenType check_keyword(size_t start, size_t length, const char* rest, TokenType type) {
    bool same_len = ((size_t)(scanner.current - scanner.start) == start + length);
    if (same_len && memcmp(scanner.start + start, rest, length) == 0) {
        return type;
    }
    return TOKEN_IDENT;
}

static TokenType identifier_type() {
    switch (scanner.start[0]) {
        case 'a': return check_keyword(1, 2, "nd", TOKEN_AND);
        case 'c': return check_keyword(1, 4, "lass", TOKEN_CLASS);
        case 'e': return check_keyword(1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a': return check_keyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o': return check_keyword(2, 1, "r", TOKEN_FOR);
                    case 'u': return check_keyword(2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 'i': return check_keyword(1, 1, "f", TOKEN_IF);
        case 'n': return check_keyword(1, 2, "il", TOKEN_NIL);
        case 'o': return check_keyword(1, 1, "r", TOKEN_OR);
        case 'p': return check_keyword(1, 4, "rint", TOKEN_PRINT);
        case 'r': return check_keyword(1, 5, "eturn", TOKEN_RETURN);
        case 's': return check_keyword(1, 4, "uper", TOKEN_SUPER);
        case 't':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'h': return check_keyword(2, 2, "is", TOKEN_THIS);
                    case 'r': return check_keyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'v': return check_keyword(1, 2, "ar", TOKEN_VAR);
        case 'w': return check_keyword(1, 4, "hile", TOKEN_WHILE);
    }
    return TOKEN_IDENT;
}

static Token finish_identifier() {
    while (is_alphanumscore(peek())) {
        advance();
    }
    return make_token(identifier_type());
}

static Token finish_string() {
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') {
            scanner.line++;
        }
        advance();
    }

    if (is_at_end()) {
        return error_token("Unterminated string.");
    }

    advance();  // consume closing quote
    return make_token(TOKEN_STR);
}

static Token finish_number() {
    while (is_digit(peek())) {
        advance();
    }

    if (peek() == '.' && is_digit(peek_next())) {
        advance();  // consume decimal point
        while (is_digit(peek())) {
            advance();
        }
    }

    return make_token(TOKEN_NUM);
}

Token scan_token() {
    skip_whitespace();
    scanner.start = scanner.current;

    if (is_at_end()) {
        return make_token(TOKEN_EOF);
    }

    char c = advance();
    if (is_digit(c)) {
        return finish_number();
    }
    if (is_alphascore(c)) {
        return finish_identifier();
    }

    switch (c) {
        case '(': return make_token(TOKEN_LPAREN);
        case ')': return make_token(TOKEN_RPAREN);
        case '{': return make_token(TOKEN_LBRACE);
        case '}': return make_token(TOKEN_RBRACE);
        case ';': return make_token(TOKEN_SEMI);
        case ',': return make_token(TOKEN_COMMA);
        case '.': return make_token(TOKEN_DOT);
        case '+': return make_token(TOKEN_PLUS);
        case '-': return make_token(TOKEN_MINUS);
        case '*': return make_token(TOKEN_STAR);
        case '/': return make_token(TOKEN_SLASH);
        case '!':
            return make_token(match('=') ? TOKEN_NE : TOKEN_BANG);
        case '=':
            return make_token(match('=') ? TOKEN_EE : TOKEN_EQUAL);
        case '<':
            return make_token(match('=') ? TOKEN_LE : TOKEN_LT);
        case '>':
            return make_token(match('=') ? TOKEN_GE : TOKEN_GT);
        case '"':
            return finish_string();
    }

    return error_token("Unexpected character.");
}
