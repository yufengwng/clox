#include <stdlib.h>
#include <string.h>

#include "table.h"
#include "memory.h"

#define TABLE_MAX_LOAD 0.75

void init_table(Table* table) {
    table->count = 0;
    table->capacity_mask = 0;
    table->entries = NULL;
}

void free_table(Table* table) {
    size_t capacity = table_current_capacity(table);
    FREE_ARRAY(Entry, table->entries, capacity);
    init_table(table);
}

size_t table_current_capacity(Table* table) {
    // We always start at 8 and grow by 2x, so use zero as check for empty.
    return table->capacity_mask == 0 ? 0 : table->capacity_mask + 1;
}

static Entry* find_entry(Entry* entries, size_t capacity_mask, ObjString* key) {
    size_t idx = key->hash & capacity_mask;
    Entry* tombstone = NULL;
    while (true) {
        Entry* entry = &entries[idx];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                return tombstone != NULL ? tombstone : entry;
            } else if (tombstone == NULL) {
                tombstone = entry;
            }
        } else if (entry->key == key) {
            return entry;
        }
        idx = (idx + 1) & capacity_mask;
    }
}

static void adjust_capacity(Table* table, size_t capacity_mask) {
    Entry* entries = ALLOCATE(Entry, capacity_mask + 1);
    for (size_t i = 0; i <= capacity_mask; i++) {
        entries[i].key = NULL;
        entries[i].value = BOX_NIL;
    }

    table->count = 0;
    size_t current_capacity = table_current_capacity(table);
    for (size_t i = 0; i < current_capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) {
            continue;
        }
        Entry* dest = find_entry(entries, capacity_mask, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, current_capacity);
    table->entries = entries;
    table->capacity_mask = capacity_mask;
}

bool table_get(Table* table, ObjString* key, Value* value) {
    if (table->count == 0) {
        return false;
    }

    Entry* entry = find_entry(table->entries, table->capacity_mask, key);
    if (entry->key == NULL) {
        return false;
    }

    *value = entry->value;
    return true;
}

bool table_set(Table* table, ObjString* key, Value value) {
    size_t capacity = table_current_capacity(table);
    if (table->count + 1 > capacity * TABLE_MAX_LOAD) {
        size_t capacity_mask = GROW_CAPACITY(capacity) - 1;
        adjust_capacity(table, capacity_mask);
    }

    Entry* entry = find_entry(table->entries, table->capacity_mask, key);
    bool is_new_key = (entry->key == NULL);
    if (is_new_key && IS_NIL(entry->value)) {
        table->count++;
    }

    entry->key = key;
    entry->value = value;
    return is_new_key;
}

bool table_delete(Table* table, ObjString* key) {
    if (table->count == 0) {
        return false;
    }

    Entry* entry = find_entry(table->entries, table->capacity_mask, key);
    if (entry->key == NULL) {
        return false;
    }

    // a tombstone entry
    entry->key = NULL;
    entry->value = BOX_BOOL(true);

    return true;
}

void table_add_all(Table* from, Table* to) {
    size_t from_capacity = table_current_capacity(from);
    for (size_t i = 0; i < from_capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key != NULL) {
            table_set(to, entry->key, entry->value);
        }
    }
}

ObjString* table_find_string(Table* table, const char* chars, size_t length, size_t hash) {
    if (table->count == 0) {
        return NULL;
    }

    size_t idx = hash & table->capacity_mask;
    while (true) {
        Entry* entry = &table->entries[idx];
        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                return NULL;
            }
        } else if (entry->key->length == length
                && entry->key->hash == hash
                && memcmp(entry->key->chars, chars, length) == 0) {
            return entry->key;
        }
        idx = (idx + 1) & table->capacity_mask;
    }
}

void table_mark_reachable(Table* table) {
    size_t capacity = table_current_capacity(table);
    for (size_t i = 0; i < capacity; i++) {
        Entry* entry = &table->entries[i];
        gc_mark_object((Obj*)entry->key);
        gc_mark_value(entry->value);
    }
}

void table_remove_unreachable(Table* table) {
    size_t capacity = table_current_capacity(table);
    for (size_t i = 0; i < capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->obj.is_marked) {
            table_delete(table, entry->key);
        }
    }
}
