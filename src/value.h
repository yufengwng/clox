#pragma once

#include <string.h>

#include "common.h"

typedef struct Obj Obj;
typedef struct ObjString ObjString;

#ifdef NAN_BOXING

typedef uint64_t Value;

#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define QNAN     ((uint64_t)0x7ffc000000000000)

#define TAG_NIL   1 // 0b01
#define TAG_FALSE 2 // 0b10
#define TAG_TRUE  3 // 0b11

#define IS_NIL(value)     ((value) == BOX_NIL)
#define IS_BOOL(value)    (((value) | 1) == BOX_TRUE)
#define IS_NUMBER(value)  (((value) & QNAN) != QNAN)
#define IS_OBJ(value) \
    (((value) & (SIGN_BIT | QNAN)) == (SIGN_BIT | QNAN))

#define BOX_NIL           ((Value)(uint64_t)(QNAN | TAG_NIL))
#define BOX_FALSE         ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define BOX_TRUE          ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define BOX_BOOL(b)       ((b) ? BOX_TRUE : BOX_FALSE)
#define BOX_NUMBER(num)   num_to_value(num)
#define BOX_OBJ(obj) \
    (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

#define RAW_BOOL(value)   ((value) == BOX_TRUE)
#define RAW_NUMBER(value) value_to_num(value)
#define RAW_OBJ(value) \
    ((Obj*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

static inline Value num_to_value(double num) {
    Value value;
    memcpy(&value, &num, sizeof(double));
    return value;
}

static inline double value_to_num(Value value) {
    double num;
    memcpy(&num, &value, sizeof(Value));
    return num;
}

#else

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

#endif

typedef struct {
    size_t count;
    size_t capacity;
    Value* values;
} ValueArray;

void init_varr(ValueArray* array);
void free_varr(ValueArray* array);
void varr_write(ValueArray* array, Value value);

bool values_equal(Value a, Value b);
void print_value(Value value);
