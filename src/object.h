#pragma once

#include "common.h"
#include "chunk.h"
#include "value.h"

typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_UPVALUE,
    OBJ_CLOSURE,
    OBJ_NATIVE,
} ObjType;

struct Obj {
    ObjType type;
    bool is_marked;
    struct Obj* next;
};

struct ObjString {
    Obj obj;
    size_t length;
    char* chars;
    size_t hash;
};

typedef struct {
    Obj obj;
    size_t arity;
    size_t upvalue_count;
    Chunk chunk;
    ObjString* name;
} ObjFunction;

typedef struct {
    Obj obj;
    Value* location;
    Value closed;
    struct ObjUpvalue* next;
} ObjUpvalue;

typedef struct {
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    size_t upvalue_count;
} ObjClosure;

typedef Value (*NativeFn)(size_t arg_count, Value* args);

typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

#define OBJ_TYPE(value)     (RAW_OBJ(value)->type)
#define IS_STRING(value)    is_obj_type(value, OBJ_STRING)
#define IS_FUNCTION(value)  is_obj_type(value, OBJ_FUNCTION)
#define IS_CLOSURE(value)   is_obj_type(value, OBJ_CLOSURE)
#define IS_NATIVE(value)    is_obj_type(value, OBJ_NATIVE)

#define RAW_STRING(value)   ((ObjString*)RAW_OBJ(value))
#define RAW_CSTRING(value)  (((ObjString*)RAW_OBJ(value))->chars)
#define RAW_FUNCTION(value) ((ObjFunction*)RAW_OBJ(value))
#define RAW_CLOSURE(value)  ((ObjClosure*)RAW_OBJ(value))
#define RAW_NATIVE(value)   (((ObjNative*)RAW_OBJ(value))->function)

static inline bool is_obj_type(Value value, ObjType type) {
    return IS_OBJ(value) && RAW_OBJ(value)->type == type;
}

ObjString* copy_string(const char* chars, size_t length);
ObjString* take_string(char* chars, size_t length);

ObjFunction* new_function();
ObjUpvalue* new_upvalue(Value* slot);
ObjClosure* new_closure(ObjFunction* function);
ObjNative* new_native(NativeFn function);

void print_object(Value value);
