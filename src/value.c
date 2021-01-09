#include <stdio.h>

#include "memory.h"
#include "value.h"

void varr_init(ValueArray* array) {
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void varr_free(ValueArray* array) {
    FREE_ARRAY(Value, array->values, array->capacity);
    varr_init(array);
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

void print_value(Value value) {
    printf("%g", RAW_NUMBER(value));
}
