#include <stdio.h>
#include <stdlib.h>

#include "memory.h"
#include "object.h"
#include "vm.h"

void* reallocate(void* prev_ptr, size_t old_size, size_t new_size) {
    if (new_size == 0) {
        free(prev_ptr);
        return NULL;
    }
    void* ptr = realloc(prev_ptr, new_size);
    if (ptr == NULL) {
        fprintf(stderr, "[lox] error: failed to reallocate memory\n");
        exit(1);
    }
    return ptr;
}

static void free_object(Obj* object) {
    switch (object->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
        }
    }
}

void free_objects() {
    Obj* object = vm.objects;
    while (object != NULL) {
        Obj* next = object->next;
        free_object(object);
        object = next;
    }
}
