#include <stdlib.h>
#include <string.h>

#include "table.h"
#include "memory.h"

#define TABLE_MAX_LOAD 0.75

void init_table(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void free_table(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    init_table(table);
}

static Entry* find_entry(Entry* entries, size_t capacity, ObjString* key) {
    size_t idx = key->hash % capacity;
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
        idx = (idx + 1) % capacity;
    }
}

static void adjust_capacity(Table* table, size_t capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);
    for (size_t i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = BOX_NIL;
    }

    table->count = 0;
    for (size_t i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) {
            continue;
        }
        Entry* dest = find_entry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

bool table_get(Table* table, ObjString* key, Value* value) {
    if (table->count == 0) {
        return false;
    }

    Entry* entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) {
        return false;
    }

    *value = entry->value;
    return true;
}

bool table_set(Table* table, ObjString* key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        size_t capacity = GROW_CAPACITY(table->capacity);
        adjust_capacity(table, capacity);
    }

    Entry* entry = find_entry(table->entries, table->capacity, key);
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

    Entry* entry = find_entry(table->entries, table->capacity, key);
    if (entry->key == NULL) {
        return false;
    }

    // a tombstone entry
    entry->key = NULL;
    entry->value = BOX_BOOL(true);

    return true;
}

void table_add_all(Table* from, Table* to) {
    for (size_t i = 0; i < from->capacity; i++) {
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

    size_t idx = hash % table->capacity;
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
        idx = (idx + 1) % table->capacity;
    }
}
