#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"

void init_varr(ValueArray* array) {
    array->count = 0;
    array->capacity = 0;
    array->values = NULL;
}

void free_varr(ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    init_varr(array);
}

void varr_write(ValueArray* array, Value value) {
    if (array->capacity < array->count + 1) {
        size_t old_capacity = array->capacity;
        array->capacity = GROW_CAPACITY(old_capacity);
        array->values = GROW_ARRAY(Value, array->values, old_capacity, array->capacity);
    }
    array->values[array->count] = value;
    array->count++;
}

bool values_equal(Value a, Value b) {
    if (a.type != b.type) {
        return false;
    }
    switch (a.type) {
        case VAL_NIL:       return true;
        case VAL_BOOL:      return RAW_BOOL(a) == RAW_BOOL(b);
        case VAL_NUMBER:    return RAW_NUMBER(a) == RAW_NUMBER(b);
        case VAL_OBJ:       return RAW_OBJ(a) == RAW_OBJ(b);
        default:            return false; // Unreachable.
    }
}

void print_value(Value value) {
    switch (value.type) {
        case VAL_NIL:
            printf("nil");
            break;
        case VAL_BOOL:
            printf(RAW_BOOL(value) ? "true" : "false");
            break;
        case VAL_NUMBER:
            printf("%g", RAW_NUMBER(value));
            break;
        case VAL_OBJ:
            print_object(value);
            break;
    }
}
