#pragma once

#include "common.h"
#include "value.h"

typedef enum {
    OBJ_STRING,
} ObjType;

struct Obj {
    ObjType type;
    struct Obj* next;
};

struct ObjString {
    Obj obj;
    size_t length;
    char* chars;
};

#define OBJ_TYPE(value)    (RAW_OBJ(value)->type)
#define IS_STRING(value)   is_obj_type(value, OBJ_STRING)

#define RAW_STRING(value)  ((ObjString*)RAW_OBJ(value))
#define RAW_CSTRING(value) (((ObjString*)RAW_OBJ(value))->chars)

static inline bool is_obj_type(Value value, ObjType type) {
    return IS_OBJ(value) && RAW_OBJ(value)->type == type;
}

ObjString* copy_string(const char* chars, size_t length);
ObjString* take_string(char* chars, size_t length);

void print_object(Value value);
