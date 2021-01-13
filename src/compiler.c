#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "scanner.h"
#include "object.h"
#include "value.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,    // =
    PREC_OR,            // or
    PREC_AND,           // and
    PREC_EQUALITY,      // == !=
    PREC_COMPARISON,    // < > <= >=
    PREC_TERM,          // + -
    PREC_FACTOR,        // * /
    PREC_UNARY,         // - !
    PREC_CALL,          // . ()
    PREC_PRIMARY,
} Precedence;

typedef void (*ParseFn)();

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

// forward declarations
static void expression();
static void statement();
static void declaration();
static ParseRule* get_rule(TokenType type);
static void parse_precedence(Precedence precedence);

typedef struct {
    Token previous;
    Token current;
    bool had_error;
    bool panic_mode;
} Parser;

Parser parser;  // singleton

Chunk* compiling_chunk;

static Chunk* current_chunk() {
    return compiling_chunk;
}

static void report_error(Token* token, const char* message) {
    if (parser.panic_mode) {
        return;
    }
    parser.panic_mode = true;
    fprintf(stderr, "[line %zu] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '%.*s'", (int)token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.had_error = true;
}

static void error(const char* message) {
    report_error(&parser.previous, message);
}

static void error_at_current(const char* message) {
    report_error(&parser.current, message);
}

static void advance() {
    parser.previous = parser.current;
    while (true) {
        parser.current = scan_token();
        if (parser.current.type != TOKEN_ERR) {
            break;
        }
        error_at_current(parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }
    error_at_current(message);
}

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    } else {
        return false;
    }
}

static uint8_t make_constant(Value value) {
    size_t idx = add_constant(current_chunk(), value);
    if (idx > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }
    return (uint8_t) idx;
}

static void emit_byte(uint8_t opcode) {
    chunk_write(current_chunk(), opcode, parser.previous.line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2) {
    emit_byte(byte1);
    emit_byte(byte2);
}

static void emit_constant(Value value) {
    emit_bytes(OP_CONSTANT, make_constant(value));
}

static void emit_return() {
    emit_byte(OP_RETURN);
}

static void end_compiler() {
    emit_return();
#ifdef DEBUG_PRINT_CODE
    if (!parser.had_error) {
        disassemble_chunk(current_chunk(), "code");
    }
#endif
}

static void binary() {
    TokenType op_type = parser.previous.type;

    ParseRule* rule = get_rule(op_type);
    parse_precedence((Precedence)(rule->precedence + 1));

    switch (op_type) {
        case TOKEN_NE:      emit_bytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EE:      emit_byte(OP_EQUAL); break;
        case TOKEN_LT:      emit_byte(OP_LESS); break;
        case TOKEN_LE:      emit_bytes(OP_GREATER, OP_NOT); break;
        case TOKEN_GT:      emit_byte(OP_GREATER); break;
        case TOKEN_GE:      emit_bytes(OP_LESS, OP_NOT); break;
        case TOKEN_PLUS:    emit_byte(OP_ADD); break;
        case TOKEN_MINUS:   emit_byte(OP_SUBTRACT); break;
        case TOKEN_STAR:    emit_byte(OP_MULTIPLY); break;
        case TOKEN_SLASH:   emit_byte(OP_DIVIDE); break;
        default:
            return; // Unreachable.
    }
}

static void unary() {
    TokenType op_type = parser.previous.type;

    parse_precedence(PREC_UNARY);

    switch (op_type) {
        case TOKEN_BANG:  emit_byte(OP_NOT); break;
        case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
        default:
            return; // Unreachable.
    }
}

static void number() {
    double value = strtod(parser.previous.start, NULL);
    emit_constant(BOX_NUMBER(value));
}

static void string() {
    size_t length = parser.previous.length - 2;
    emit_constant(BOX_OBJ(copy_string(parser.previous.start + 1, length)));
}

static void literal() {
    switch (parser.previous.type) {
        case TOKEN_NIL:   emit_byte(OP_NIL); break;
        case TOKEN_TRUE:  emit_byte(OP_TRUE); break;
        case TOKEN_FALSE: emit_byte(OP_FALSE); break;
        default:
            return; // Unreachable.
    }
}

static void grouping() {
    expression();
    consume(TOKEN_RPAREN, "Expect ')' after expression.");
}

static void expression() {
    parse_precedence(PREC_ASSIGNMENT);
}

static void expression_statement() {
    expression();
    consume(TOKEN_SEMI, "Expect ';' after expression.");
    emit_byte(OP_POP);
}

static void print_statement() {
    expression();
    consume(TOKEN_SEMI, "Expect ';' after value.");
    emit_byte(OP_PRINT);
}

static void synchronize() {
    parser.panic_mode = false;
    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMI) {
            return;
        }
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default:
                ; // nothing to do
        }
        advance();
    }
}

static void statement() {
    if (match(TOKEN_PRINT)) {
        print_statement();
    } else {
        expression_statement();
    }
}

static void declaration() {
    statement();
    if (parser.panic_mode) {
        synchronize();
    }
}

static void parse_precedence(Precedence precedence) {
    advance();
    ParseFn prefix_rule = get_rule(parser.previous.type)->prefix;
    if (prefix_rule == NULL) {
        error("Expect expression.");
        return;
    }

    prefix_rule();

    while (precedence <= get_rule(parser.current.type)->precedence) {
        advance();
        ParseFn infix_rule = get_rule(parser.previous.type)->infix;
        infix_rule();
    }
}

ParseRule rules[] = {
    // token_type  | prefix   | infix | precedence
    [TOKEN_LPAREN] = {grouping, NULL,   PREC_NONE},
    [TOKEN_RPAREN] = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LBRACE] = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RBRACE] = {NULL,     NULL,   PREC_NONE},
    [TOKEN_COMMA]  = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SEMI]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_DOT]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_BANG]   = {unary,    NULL,   PREC_NONE},
    [TOKEN_EQUAL]  = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NE]     = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_EE]     = {NULL,     binary, PREC_EQUALITY},
    [TOKEN_LT]     = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_LE]     = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_GT]     = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_GE]     = {NULL,     binary, PREC_COMPARISON},
    [TOKEN_PLUS]   = {NULL,     binary, PREC_TERM},
    [TOKEN_MINUS]  = {unary,    binary, PREC_TERM},
    [TOKEN_STAR]   = {NULL,     binary, PREC_FACTOR},
    [TOKEN_SLASH]  = {NULL,     binary, PREC_FACTOR},
    [TOKEN_IDENT]  = {NULL,     NULL,   PREC_NONE},
    [TOKEN_STR]    = {string,   NULL,   PREC_NONE},
    [TOKEN_NUM]    = {number,   NULL,   PREC_NONE},
    [TOKEN_AND]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_CLASS]  = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ELSE]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]  = {literal,  NULL,   PREC_NONE},
    [TOKEN_FOR]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FUN]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IF]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]    = {literal,  NULL,   PREC_NONE},
    [TOKEN_OR]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_PRINT]  = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RETURN] = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SUPER]  = {NULL,     NULL,   PREC_NONE},
    [TOKEN_THIS]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_TRUE]   = {literal,  NULL,   PREC_NONE},
    [TOKEN_VAR]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_WHILE]  = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ERR]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EOF]    = {NULL,     NULL,   PREC_NONE},
};

static ParseRule* get_rule(TokenType type) {
    return &rules[type];
}

bool compile(const char* source, Chunk* chunk) {
    init_scanner(source);
    compiling_chunk = chunk;
    parser.had_error = false;
    parser.panic_mode = false;

    advance();
    while (!match(TOKEN_EOF)) {
        declaration();
    }
    consume(TOKEN_EOF, "Expect end of expression.");
    end_compiler();

    return !parser.had_error;
}
