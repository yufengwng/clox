#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef double Value;

typedef struct {
    Value* values;
    int capacity;
    int count;
} ValueArray;

void value_array_init(ValueArray* array);
void value_array_free(ValueArray* array);
void value_array_write(ValueArray* array, Value value);

void print_value(Value value);

#endif
