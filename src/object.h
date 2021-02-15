#pragma once

#include "common.h"
#include "chunk.h"
#include "table.h"
#include "value.h"

typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
    OBJ_UPVALUE,
    OBJ_CLOSURE,
    OBJ_NATIVE,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_BOUND_METHOD,
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

typedef struct ObjUpvalue {
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

typedef struct {
    Obj obj;
    ObjString* name;
    Table methods;
} ObjClass;

typedef struct {
    Obj obj;
    ObjClass* klass;
    Table fields;
} ObjInstance;

typedef struct {
    Obj obj;
    Value receiver;
    ObjClosure* method;
} ObjBoundMethod;

#define OBJ_TYPE(value)         (RAW_OBJ(value)->type)
#define IS_STRING(value)        is_obj_type(value, OBJ_STRING)
#define IS_FUNCTION(value)      is_obj_type(value, OBJ_FUNCTION)
#define IS_CLOSURE(value)       is_obj_type(value, OBJ_CLOSURE)
#define IS_NATIVE(value)        is_obj_type(value, OBJ_NATIVE)
#define IS_CLASS(value)         is_obj_type(value, OBJ_CLASS)
#define IS_INSTANCE(value)      is_obj_type(value, OBJ_INSTANCE)
#define IS_BOUND_METHOD(value)  is_obj_type(value, OBJ_BOUND_METHOD)

#define RAW_STRING(value)       ((ObjString*)RAW_OBJ(value))
#define RAW_CSTRING(value)      (((ObjString*)RAW_OBJ(value))->chars)
#define RAW_FUNCTION(value)     ((ObjFunction*)RAW_OBJ(value))
#define RAW_CLOSURE(value)      ((ObjClosure*)RAW_OBJ(value))
#define RAW_NATIVE(value)       (((ObjNative*)RAW_OBJ(value))->function)
#define RAW_CLASS(value)        ((ObjClass*)RAW_OBJ(value))
#define RAW_INSTANCE(value)     ((ObjInstance*)RAW_OBJ(value))
#define RAW_BOUND_METHOD(value) ((ObjBoundMethod*)RAW_OBJ(value))

static inline bool is_obj_type(Value value, ObjType type) {
    return IS_OBJ(value) && RAW_OBJ(value)->type == type;
}

ObjString* copy_string(const char* chars, size_t length);
ObjString* take_string(char* chars, size_t length);

ObjFunction* new_function();
ObjUpvalue* new_upvalue(Value* slot);
ObjClosure* new_closure(ObjFunction* function);
ObjNative* new_native(NativeFn function);
ObjClass* new_class(ObjString* name);
ObjInstance* new_instance(ObjClass* klass);
ObjBoundMethod* new_bound_method(Value receiver, ObjClosure* method);

void print_object(Value value);
