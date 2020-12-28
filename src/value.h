#pragma once

#include "common.h"

typedef double Value;

typedef struct {
    Value* values;
    size_t capacity;
    size_t count;
} ValueArray;

void varr_init(ValueArray* array);
void varr_free(ValueArray* array);
void varr_write(ValueArray* array, Value value);

void print_value(Value value);
