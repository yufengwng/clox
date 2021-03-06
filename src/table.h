#pragma once

#include "common.h"
#include "value.h"

typedef struct {
    ObjString* key;
    Value value;
} Entry;

typedef struct {
    size_t count;
    size_t capacity_mask;
    Entry* entries;
} Table;

void init_table(Table* table);
void free_table(Table* table);
size_t table_current_capacity(Table* table);

bool table_get(Table* table, ObjString* key, Value* value);
bool table_set(Table* table, ObjString* key, Value value);
bool table_delete(Table* table, ObjString* key);
void table_add_all(Table* from, Table* to);

ObjString* table_find_string(Table* table, const char* chars, size_t length, size_t hash);
void table_mark_reachable(Table* table);
void table_remove_unreachable(Table* table);
