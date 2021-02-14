#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "scanner.h"
#include "memory.h"
#include "chunk.h"
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

typedef void (*ParseFn)(bool can_assign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token previous;
    Token current;
    bool had_error;
    bool panic_mode;
} Parser;

typedef struct {
    Token name;
    int depth;
    bool is_captured;
} Local;

typedef struct {
    uint8_t index;
    bool is_local;
} Upvalue;

typedef enum {
    TYPE_FUNCTION,
    TYPE_INITIALIZER,
    TYPE_METHOD,
    TYPE_SCRIPT,
} FunctionType;

typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType type;
    Upvalue upvalues[UINT8_COUNT];
    Local locals[UINT8_COUNT];
    size_t local_count;
    size_t scope_depth;
} Compiler;

typedef struct {
    struct ClassCompiler* enclosing;
    Token name;
} ClassCompiler;

Parser parser;  // singleton
Compiler* current = NULL;
ClassCompiler* current_class = NULL;

// forward declarations
static void expression();
static void statement();
static void declaration();
static void var_declaration();
static ParseRule* get_rule(TokenType type);
static void parse_precedence(Precedence precedence);

static Chunk* current_chunk() {
    return &current->function->chunk;
}

static void init_compiler(Compiler* compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->local_count = 0;
    compiler->scope_depth = 0;
    compiler->function = new_function();
    current = compiler;

    if (type != TYPE_SCRIPT) {
        current->function->name = copy_string(parser.previous.start, parser.previous.length);
    }

    Local* local = &current->locals[current->local_count++];
    local->depth = 0;
    local->is_captured = false;
    if (type != TYPE_FUNCTION) {
        local->name.start = "this";
        local->name.length = 4;
    } else {
        local->name.start = "";
        local->name.length = 0;
    }
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
    if (current->type == TYPE_INITIALIZER) {
        emit_bytes(OP_GET_LOCAL, 0);
    } else {
        emit_byte(OP_NIL);
    }
    emit_byte(OP_RETURN);
}

static void emit_loop(size_t loop_start) {
    emit_byte(OP_LOOP);

    size_t offset = current_chunk()->count - loop_start + 2;
    if (offset > UINT16_MAX) {
        error("Loop body too large.");
    }

    emit_byte((offset >> 8) & 0xff);
    emit_byte(offset & 0xff);
}

static size_t emit_jump(uint8_t instruction) {
    emit_byte(instruction);
    emit_byte(0xff);
    emit_byte(0xff);
    return current_chunk()->count - 2;
}

static void patch_jump(size_t offset) {
    size_t jump = current_chunk()->count - offset - 2;
    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }
    current_chunk()->code[offset] = (jump >> 8) & 0xff;
    current_chunk()->code[offset + 1] = jump & 0xff;
}

static void begin_scope() {
    current->scope_depth++;
}

static void end_scope() {
    current->scope_depth--;
    while (current->local_count > 0 &&
            current->locals[current->local_count - 1].depth > current->scope_depth) {
        if (current->locals[current->local_count - 1].is_captured) {
            emit_byte(OP_CLOSE_UPVALUE);
        } else {
            emit_byte(OP_POP);
        }
        current->local_count--;
    }
}

static ObjFunction* end_compiler() {
    emit_return();
    ObjFunction* function = current->function;
#ifdef DEBUG_PRINT_CODE
    if (!parser.had_error) {
        disassemble_chunk(current_chunk(),
                function->name != NULL ? function->name->chars : "<script>");
    }
#endif
    current = current->enclosing;
    return function;
}

static void binary(bool can_assign) {
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

static void unary(bool can_assign) {
    TokenType op_type = parser.previous.type;

    parse_precedence(PREC_UNARY);

    switch (op_type) {
        case TOKEN_BANG:  emit_byte(OP_NOT); break;
        case TOKEN_MINUS: emit_byte(OP_NEGATE); break;
        default:
            return; // Unreachable.
    }
}

static void number(bool can_assign) {
    double value = strtod(parser.previous.start, NULL);
    emit_constant(BOX_NUMBER(value));
}

static void string(bool can_assign) {
    size_t length = parser.previous.length - 2;
    emit_constant(BOX_OBJ(copy_string(parser.previous.start + 1, length)));
}

static uint8_t identifier_constant(Token* name) {
    return make_constant(BOX_OBJ(copy_string(name->start, name->length)));
}

static bool identifier_equals(Token* a, Token* b) {
    if (a->length != b->length) {
        return false;
    }
    return memcmp(a->start, b->start, a->length) == 0;
}

static void add_local(Token name) {
    if (current->local_count == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }
    Local* local = &current->locals[current->local_count++];
    local->name = name;
    local->depth = -1;
    local->is_captured = false;
}

static void mark_initialized() {
    if (current->scope_depth == 0) {
        return;
    }
    current->locals[current->local_count - 1].depth = current->scope_depth;
}

static int resolve_local(Compiler* compiler, Token* name) {
    for (int i = compiler->local_count - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifier_equals(name, &local->name)) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }
    return -1;
}

static size_t add_upvalue(Compiler* compiler, uint8_t index, bool is_local) {
    size_t upvalue_count = compiler->function->upvalue_count;

    for (size_t i = 0; i < upvalue_count; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->is_local == is_local) {
            return i;
        }
    }

    if (upvalue_count == UINT8_COUNT) {
        error("Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalue_count].is_local = is_local;
    compiler->upvalues[upvalue_count].index = index;
    return compiler->function->upvalue_count++;
}

static int resolve_upvalue(Compiler* compiler, Token* name) {
    if (compiler->enclosing == NULL) {
        return -1;
    }

    int local = resolve_local(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].is_captured = true;
        return add_upvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = resolve_upvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return add_upvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

static void declare_variable() {
    if (current->scope_depth == 0) {
        return;
    }
    Token* name = &parser.previous;
    for (int i = current->local_count - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scope_depth) {
            break;
        }
        if (identifier_equals(name, &local->name)) {
            error("Already variable with this name in this scope.");
        }
    }
    add_local(*name);
}

static void define_variable(uint8_t global) {
    if (current->scope_depth > 0) {
        mark_initialized();
        return;
    }
    emit_bytes(OP_DEFINE_GLOBAL, global);
}

static uint8_t parse_variable(const char* error_message) {
    consume(TOKEN_IDENT, error_message);
    declare_variable();
    if (current->scope_depth > 0) {
        return 0;
    }
    return identifier_constant(&parser.previous);
}

static void named_variable(Token name, can_assign) {
    uint8_t get_op, set_op;
    int arg = resolve_local(current, &name);
    if (arg != -1) {
        get_op = OP_GET_LOCAL;
        set_op = OP_SET_LOCAL;
    } else if ((arg = resolve_upvalue(current, &name)) != -1) {
        get_op = OP_GET_UPVALUE;
        set_op = OP_SET_UPVALUE;
    } else {
        arg = identifier_constant(&name);
        get_op = OP_GET_GLOBAL;
        set_op = OP_SET_GLOBAL;
    }

    if (can_assign && match(TOKEN_EQUAL)) {
        expression();
        emit_bytes(set_op, (uint8_t)arg);
    } else {
        emit_bytes(get_op, (uint8_t)arg);
    }
}

static void variable(bool can_assign) {
    named_variable(parser.previous, can_assign);
}

static void this_(bool can_assign) {
    if (current_class == NULL) {
        error("Can't use 'this' outside of a class.");
        return;
    }
    variable(false);
}

static void literal(bool can_assign) {
    switch (parser.previous.type) {
        case TOKEN_NIL:   emit_byte(OP_NIL); break;
        case TOKEN_TRUE:  emit_byte(OP_TRUE); break;
        case TOKEN_FALSE: emit_byte(OP_FALSE); break;
        default:
            return; // Unreachable.
    }
}

static void log_and(bool can_assign) {
    size_t end_jump = emit_jump(OP_JUMP_IF_FALSE);

    emit_byte(OP_POP);
    parse_precedence(PREC_AND);

    patch_jump(end_jump);
}

static void log_or(bool can_assign) {
    size_t else_jump = emit_jump(OP_JUMP_IF_FALSE);
    size_t end_jump = emit_jump(OP_JUMP);

    patch_jump(else_jump);
    emit_byte(OP_POP);
    parse_precedence(PREC_OR);

    patch_jump(end_jump);
}

static void grouping(bool can_assign) {
    expression();
    consume(TOKEN_RPAREN, "Expect ')' after expression.");
}

static void expression() {
    parse_precedence(PREC_ASSIGNMENT);
}

static uint8_t argument_list() {
    uint8_t arg_count = 0;
    if (!check(TOKEN_RPAREN)) {
        do {
            expression();
            if (arg_count == 255) {
                error("Can't have more than 255 arguments.");
            }
            arg_count++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RPAREN, "Expet ')' after arguments.");
    return arg_count;
}

static void call(bool can_assign) {
    uint8_t arg_count = argument_list();
    emit_bytes(OP_CALL, arg_count);
}

static void dot(bool can_assign) {
    consume(TOKEN_IDENT, "Expect property name after '.'.");
    uint8_t name_idx = identifier_constant(&parser.previous);

    if (can_assign && match(TOKEN_EQUAL)) {
        expression();
        emit_bytes(OP_SET_PROPERTY, name_idx);
    } else if (match(TOKEN_LPAREN)) {
        uint8_t arg_count = argument_list();
        emit_bytes(OP_INVOKE, name_idx);
        emit_byte(arg_count);
    } else {
        emit_bytes(OP_GET_PROPERTY, name_idx);
    }
}

static void block() {
    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        declaration();
    }
    consume(TOKEN_RBRACE, "Expect '}' after block.");
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

static void if_statement() {
    consume(TOKEN_LPAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RPAREN, "Expect ')' after condition.");

    size_t then_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();
    size_t else_jump = emit_jump(OP_JUMP);
    patch_jump(then_jump);

    emit_byte(OP_POP);
    if (match(TOKEN_ELSE)) {
        statement();
    }
    patch_jump(else_jump);
}

static void while_statement() {
    size_t loop_start = current_chunk()->count;

    consume(TOKEN_LPAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RPAREN, "Expect ')' after condition.");

    size_t exit_jump = emit_jump(OP_JUMP_IF_FALSE);
    emit_byte(OP_POP);
    statement();
    emit_loop(loop_start);

    patch_jump(exit_jump);
    emit_byte(OP_POP);
}

static void for_statement() {
    begin_scope();

    consume(TOKEN_LPAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_SEMI)) {
        // no initializer
    } else if (match(TOKEN_VAR)) {
        var_declaration();
    } else {
        expression_statement();
    }

    size_t loop_start = current_chunk()->count;
    int exit_jump = -1;
    if (!match(TOKEN_SEMI)) {
        expression();
        consume(TOKEN_SEMI, "Expect ';' after loop condition.");
        exit_jump = emit_jump(OP_JUMP_IF_FALSE);
        emit_byte(OP_POP);
    }

    if (!match(TOKEN_RPAREN)) {
        size_t body_jump = emit_jump(OP_JUMP);
        size_t increment_start = current_chunk()->count;

        expression();
        emit_byte(OP_POP);
        consume(TOKEN_RPAREN, "Expect ')' after for clauses.");

        emit_loop(loop_start);
        loop_start = increment_start;
        patch_jump(body_jump);
    }

    statement();
    emit_loop(loop_start);
    if (exit_jump != -1) {
        patch_jump(exit_jump);
        emit_byte(OP_POP);
    }

    end_scope();
}

static void return_statement() {
    if (current->type == TYPE_SCRIPT) {
        error("Can't return from top-level code.");
    }

    if (match(TOKEN_SEMI)) {
        emit_return();
    } else {
        if (current->type == TYPE_INITIALIZER) {
            error("Can't return a value from an initializer.");
        }
        expression();
        consume(TOKEN_SEMI, "Expect ';' after return value.");
        emit_byte(OP_RETURN);
    }
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
    } else if (match(TOKEN_IF)) {
        if_statement();
    } else if (match(TOKEN_WHILE)) {
        while_statement();
    } else if (match(TOKEN_FOR)) {
        for_statement();
    } else if (match(TOKEN_RETURN)) {
        return_statement();
    } else if (match(TOKEN_LBRACE)) {
        begin_scope();
        block();
        end_scope();
    } else {
        expression_statement();
    }
}

static void function(FunctionType type) {
    Compiler compiler;
    init_compiler(&compiler, type);
    begin_scope();

    consume(TOKEN_LPAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RPAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                error_at_current("Can't have more than 255 parameters.");
            }
            uint8_t param_idx = parse_variable("Expect parameter name.");
            define_variable(param_idx);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RPAREN, "Expect ')' after parameters.");

    consume(TOKEN_LBRACE, "Expect '{' before function body.");
    block();

    ObjFunction* function = end_compiler();
    emit_bytes(OP_CLOSURE, make_constant(BOX_OBJ(function)));

    for (size_t i = 0; i < function->upvalue_count; i++) {
        emit_byte(compiler.upvalues[i].is_local ? 1 : 0);
        emit_byte(compiler.upvalues[i].index);
    }
}

static void method() {
    consume(TOKEN_IDENT, "Expect method name.");
    uint8_t idx = identifier_constant(&parser.previous);

    FunctionType type = TYPE_METHOD;
    if (parser.previous.length == 4 && memcmp(parser.previous.start, "init", 4) == 0) {
        type = TYPE_INITIALIZER;
    }
    function(type);

    emit_bytes(OP_METHOD, idx);
}

static void var_declaration() {
    uint8_t idx = parse_variable("Expect variable name.");
    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emit_byte(OP_NIL);
    }
    consume(TOKEN_SEMI, "Expect ';' after variable declaration.");
    define_variable(idx);
}

static void fun_declaration() {
    uint8_t idx = parse_variable("Expect function name.");
    mark_initialized();
    function(TYPE_FUNCTION);
    define_variable(idx);
}

static void class_declaration() {
    consume(TOKEN_IDENT, "Expect class name.");
    Token class_name = parser.previous;
    uint8_t name_idx = identifier_constant(&parser.previous);
    declare_variable();

    emit_bytes(OP_CLASS, name_idx);
    define_variable(name_idx);

    ClassCompiler class_compiler;
    class_compiler.name = class_name;
    class_compiler.enclosing = current_class;
    current_class = &class_compiler;

    named_variable(class_name, false);
    consume(TOKEN_LBRACE, "Expect '{' before class body.");
    while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
        method();
    }
    consume(TOKEN_RBRACE, "Expect '}' after class body.");
    emit_byte(OP_POP);

    current_class = current_class->enclosing;
}

static void declaration() {
    if (match(TOKEN_CLASS)) {
        class_declaration();
    } else if (match(TOKEN_FUN)) {
        fun_declaration();
    } else if (match(TOKEN_VAR)) {
        var_declaration();
    } else {
        statement();
    }

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

    bool can_assign = precedence <= PREC_ASSIGNMENT;
    prefix_rule(can_assign);

    while (precedence <= get_rule(parser.current.type)->precedence) {
        advance();
        ParseFn infix_rule = get_rule(parser.previous.type)->infix;
        infix_rule(can_assign);
    }

    if (can_assign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}

ParseRule rules[] = {
    // token_type  | prefix   | infix | precedence
    [TOKEN_LPAREN] = {grouping, call,   PREC_CALL},
    [TOKEN_RPAREN] = {NULL,     NULL,   PREC_NONE},
    [TOKEN_LBRACE] = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RBRACE] = {NULL,     NULL,   PREC_NONE},
    [TOKEN_COMMA]  = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SEMI]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_DOT]    = {NULL,     dot,    PREC_CALL},
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
    [TOKEN_IDENT]  = {variable, NULL,   PREC_NONE},
    [TOKEN_STR]    = {string,   NULL,   PREC_NONE},
    [TOKEN_NUM]    = {number,   NULL,   PREC_NONE},
    [TOKEN_AND]    = {NULL,     log_and,PREC_AND},
    [TOKEN_CLASS]  = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ELSE]   = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FALSE]  = {literal,  NULL,   PREC_NONE},
    [TOKEN_FOR]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_FUN]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_IF]     = {NULL,     NULL,   PREC_NONE},
    [TOKEN_NIL]    = {literal,  NULL,   PREC_NONE},
    [TOKEN_OR]     = {NULL,     log_or, PREC_OR},
    [TOKEN_PRINT]  = {NULL,     NULL,   PREC_NONE},
    [TOKEN_RETURN] = {NULL,     NULL,   PREC_NONE},
    [TOKEN_SUPER]  = {NULL,     NULL,   PREC_NONE},
    [TOKEN_THIS]   = {this_,    NULL,   PREC_NONE},
    [TOKEN_TRUE]   = {literal,  NULL,   PREC_NONE},
    [TOKEN_VAR]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_WHILE]  = {NULL,     NULL,   PREC_NONE},
    [TOKEN_ERR]    = {NULL,     NULL,   PREC_NONE},
    [TOKEN_EOF]    = {NULL,     NULL,   PREC_NONE},
};

static ParseRule* get_rule(TokenType type) {
    return &rules[type];
}

ObjFunction* compile(const char* source) {
    Compiler compiler;
    init_scanner(source);
    init_compiler(&compiler, TYPE_SCRIPT);
    parser.had_error = false;
    parser.panic_mode = false;

    advance();
    while (!match(TOKEN_EOF)) {
        declaration();
    }

    ObjFunction* function = end_compiler();
    return parser.had_error ? NULL : function;
}

void compiler_mark_roots() {
    Compiler* compiler = current;
    while (compiler != NULL) {
        gc_mark_object((Obj*)compiler->function);
        compiler = compiler->enclosing;
    }
}
