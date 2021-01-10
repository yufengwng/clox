#pragma once

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

typedef enum {
    VAL_NIL,
    VAL_BOOL,
    VAL_NUMBER,
    VAL_OBJ,
} ValueType;

typedef struct {
    ValueType type;
    union {
        bool boolean;
        double number;
        Obj* obj;
    } as;
} Value;

#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
#define IS_OBJ(value)     ((value).type == VAL_OBJ)

#define BOX_NIL           ((Value){VAL_NIL, {.number = 0}})
#define BOX_BOOL(value)   ((Value){VAL_BOOL, {.boolean = value}})
#define BOX_NUMBER(value) ((Value){VAL_NUMBER, {.number = value}})
#define BOX_OBJ(object)   ((Value){VAL_OBJ, {.obj = (Obj*)object}})

#define RAW_BOOL(value)   ((value).as.boolean)
#define RAW_NUMBER(value) ((value).as.number)
#define RAW_OBJ(value)    ((value).as.obj)

typedef struct {
    Value* values;
    size_t capacity;
    size_t count;
} ValueArray;

void varr_init(ValueArray* array);
void varr_free(ValueArray* array);
void varr_write(ValueArray* array, Value value);

bool values_equal(Value a, Value b);
void print_value(Value value);
