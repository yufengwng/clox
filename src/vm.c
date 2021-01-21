#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "vm.h"
#include "compiler.h"
#include "memory.h"
#include "object.h"

#ifdef DEBUG_TRACE_EXECUTION
#include "debug.h"
#endif

VM vm;  // singleton

void stack_push(Value value) {
    *vm.stack_top = value;
    vm.stack_top++;
}

Value stack_pop() {
    vm.stack_top--;
    return *vm.stack_top;
}

static void stack_reset() {
    vm.stack_top = vm.stack;
    vm.frame_count = 0;
}

static Value stack_peek(size_t distance) {
    return vm.stack_top[-1 - distance];
}

static bool is_falsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !RAW_BOOL(value));
}

static void concatenate() {
    ObjString* b = RAW_STRING(stack_pop());
    ObjString* a = RAW_STRING(stack_pop());

    size_t length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = take_string(chars, length);
    stack_push(BOX_OBJ(result));
}

static void runtime_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    CallFrame* frame = &vm.frames[vm.frame_count - 1];
    size_t instruction = frame->ip - frame->function->chunk.code - 1;
    size_t line = frame->function->chunk.lines[instruction];
    fprintf(stderr, "[line %zu] in script\n", line);

    stack_reset();
}

#ifdef DEBUG_TRACE_EXECUTION
static void print_stack() {
    if (vm.stack_top == vm.stack) {
        printf("[ ]");
    } else {
        for (Value* curr = vm.stack; curr < vm.stack_top; curr++) {
            printf("[ ");
            print_value(*curr);
            printf(" ]");
        }
    }
    printf("\n");
}
#endif

static InterpretResult run() {
    CallFrame* frame = &vm.frames[vm.frame_count - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() RAW_STRING(READ_CONSTANT())
#define READ_SHORT() \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define BINARY_OP(value_type, op) \
    do { \
        if (!IS_NUMBER(stack_peek(0)) || !IS_NUMBER(stack_peek(1))) { \
            runtime_error("Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        double b = RAW_NUMBER(stack_pop()); \
        double a = RAW_NUMBER(stack_pop()); \
        stack_push(value_type(a op b)); \
    } while (false)

    while (true) {
#ifdef DEBUG_TRACE_EXECUTION
        print_stack();
        disassemble_instruction(&frame->function->chunk,
                (size_t)(frame->ip - frame->function->chunk.code));
#endif
        uint8_t instruction = READ_BYTE();
        switch (instruction) {
            case OP_CONSTANT: {
                Value constant = READ_CONSTANT();
                stack_push(constant);
                break;
            }
            case OP_NIL:    stack_push(BOX_NIL); break;
            case OP_TRUE:   stack_push(BOX_BOOL(true)); break;
            case OP_FALSE:  stack_push(BOX_BOOL(false)); break;
            case OP_POP:    stack_pop(); break;
            case OP_DEFINE_GLOBAL: {
                ObjString* name = READ_STRING();
                table_set(&vm.globals, name, stack_peek(0));
                stack_pop();
                break;
            }
            case OP_SET_GLOBAL: {
                ObjString* name = READ_STRING();
                if (table_set(&vm.globals, name, stack_peek(0))) {
                    table_delete(&vm.globals, name);
                    runtime_error("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_GET_GLOBAL: {
                ObjString* name = READ_STRING();
                Value value;
                if (!table_get(&vm.globals, name, &value)) {
                    runtime_error("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                stack_push(value);
                break;
            }
            case OP_SET_LOCAL: {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = stack_peek(0);
                break;
            }
            case OP_GET_LOCAL: {
                uint8_t slot = READ_BYTE();
                stack_push(frame->slots[slot]);
                break;
            }
            case OP_EQUAL: {
                Value b = stack_pop();
                Value a = stack_pop();
                stack_push(BOX_BOOL(values_equal(a, b)));
                break;
            }
            case OP_LESS:       BINARY_OP(BOX_BOOL, <); break;
            case OP_GREATER:    BINARY_OP(BOX_BOOL, >); break;
            case OP_ADD: {
                Value peek_b = stack_peek(0);
                Value peek_a = stack_peek(1);
                if (IS_STRING(peek_b) && IS_STRING(peek_a)) {
                    concatenate();
                } else if (IS_NUMBER(peek_b) && IS_NUMBER(peek_a)) {
                    double b = RAW_NUMBER(stack_pop());
                    double a = RAW_NUMBER(stack_pop());
                    stack_push(BOX_NUMBER(a + b));
                } else {
                    runtime_error("Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUBTRACT:   BINARY_OP(BOX_NUMBER, -); break;
            case OP_MULTIPLY:   BINARY_OP(BOX_NUMBER, *); break;
            case OP_DIVIDE:     BINARY_OP(BOX_NUMBER, /); break;
            case OP_NEGATE:
                if (!IS_NUMBER(stack_peek(0))) {
                    runtime_error("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                stack_push(BOX_NUMBER(-RAW_NUMBER(stack_pop())));
                break;
            case OP_NOT:
                stack_push(BOX_BOOL(is_falsey(stack_pop())));
                break;
            case OP_PRINT: {
                print_value(stack_pop());
                printf("\n");
                break;
            }
            case OP_LOOP: {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_JUMP: {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = READ_SHORT();
                if (is_falsey(stack_peek(0))) {
                    frame->ip += offset;
                }
                break;
            }
            case OP_RETURN: {
                return INTERPRET_OK;
            }
        }
    }

#undef BINARY_OP
#undef READ_SHORT
#undef READ_STRING
#undef READ_CONSTANT
#undef READ_BYTE
}

void init_vm() {
    stack_reset();
    vm.objects = NULL;
    init_table(&vm.strings);
    init_table(&vm.globals);
}

void free_vm() {
    free_table(&vm.globals);
    free_table(&vm.strings);
    free_objects();
}

InterpretResult vm_interpret(const char* source) {
    ObjFunction* function = compile(source);
    if (function == NULL) {
        return INTERPRET_COMPILE_ERROR;
    }

    stack_push(BOX_OBJ(function));
    CallFrame* frame = &vm.frames[vm.frame_count++];
    frame->function = function;
    frame->ip = function->chunk.code;
    frame->slots = vm.stack;

    return run();
}
