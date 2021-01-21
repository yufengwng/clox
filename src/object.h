#pragma once

#include "common.h"
#include "chunk.h"
#include "value.h"

typedef enum {
    OBJ_STRING,
    OBJ_FUNCTION,
} ObjType;

struct Obj {
    ObjType type;
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
    Chunk chunk;
    ObjString* name;
} ObjFunction;

#define OBJ_TYPE(value)     (RAW_OBJ(value)->type)
#define IS_STRING(value)    is_obj_type(value, OBJ_STRING)
#define IS_FUNCTION(value)  is_obj_type(value, OBJ_FUNCTION)

#define RAW_STRING(value)   ((ObjString*)RAW_OBJ(value))
#define RAW_CSTRING(value)  (((ObjString*)RAW_OBJ(value))->chars)
#define RAW_FUNCTION(value) ((ObjFunction*)RAW_OBJ(value))

static inline bool is_obj_type(Value value, ObjType type) {
    return IS_OBJ(value) && RAW_OBJ(value)->type == type;
}

ObjString* copy_string(const char* chars, size_t length);
ObjString* take_string(char* chars, size_t length);

ObjFunction* new_function();

void print_object(Value value);
