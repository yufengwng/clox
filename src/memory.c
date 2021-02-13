#include <stdio.h>
#include <stdlib.h>

#include "compiler.h"
#include "memory.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include "debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void* reallocate(void* prev_ptr, size_t old_size, size_t new_size) {
    vm.bytes_allocated += new_size - old_size;

    if (new_size > old_size) {
#ifdef DEBUG_STRESS_GC
        gc_collect();
#endif
        if (vm.bytes_allocated > vm.gc_threshold) {
            gc_collect();
        }
    }

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
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
#endif
    switch (object->type) {
        case OBJ_STRING: {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            free_chunk(&function->chunk);
            FREE(ObjFunction, object);
            break;
        }
        case OBJ_UPVALUE:
            FREE(ObjUpvalue, object);
            break;
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*) object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalue_count);
            FREE(ObjClosure, object);
            break;
        }
        case OBJ_NATIVE: {
            FREE(ObjNative, object);
            break;
        }
        case OBJ_CLASS: {
            FREE(ObjClass, object);
            break;
        }
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            free_table(&instance->fields);
            FREE(ObjInstance, object);
            break;
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
    free(vm.gray_stack);
}

void gc_mark_object(Obj* object) {
    if (object == NULL) {
        return;
    }
    if (object->is_marked) {
        return;
    }
#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    print_value(BOX_OBJ(object));
    printf("\n");
#endif
    object->is_marked = true;
    if (vm.gray_capacity < vm.gray_count + 1) {
        vm.gray_capacity = GROW_CAPACITY(vm.gray_capacity);
        vm.gray_stack = realloc(vm.gray_stack, sizeof(Obj*) * vm.gray_capacity);
        if (vm.gray_stack == NULL) {
            exit(1);
        }
    }
    vm.gray_stack[vm.gray_count++] = object;
}

void gc_mark_value(Value value) {
    if (!IS_OBJ(value)) {
        return;
    }
    gc_mark_object(RAW_OBJ(value));
}

static void gc_mark_array(ValueArray* array) {
    for (size_t i = 0; i < array->count; i++) {
        gc_mark_value(array->values[i]);
    }
}

static void gc_blacken_object(Obj* object) {
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    print_value(BOX_OBJ(object));
    printf("\n");
#endif
    switch (object->type) {
        case OBJ_INSTANCE: {
            ObjInstance* instance = (ObjInstance*)object;
            gc_mark_object((Obj*)instance->klass);
            table_mark_reachable(&instance->fields);
            break;
        }
        case OBJ_CLASS: {
            ObjClass* klass = (ObjClass*)object;
            gc_mark_object((Obj*)klass->name);
            break;
        }
        case OBJ_CLOSURE: {
            ObjClosure* closure = (ObjClosure*)object;
            gc_mark_object((Obj*)closure->function);
            for (size_t i = 0; i < closure->upvalue_count; i++) {
                gc_mark_object((Obj*)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_FUNCTION: {
            ObjFunction* function = (ObjFunction*)object;
            gc_mark_object((Obj*)function->name);
            gc_mark_array(&function->chunk.constants);
            break;
        }
        case OBJ_UPVALUE:
            gc_mark_value(((ObjUpvalue*)object)->closed);
            break;
        case OBJ_NATIVE:
        case OBJ_STRING:
            break;
    }
}

static void gc_mark_roots() {
    for (Value* slot = vm.stack; slot < vm.stack_top; slot++) {
        gc_mark_value(*slot);
    }
    for (size_t i = 0; i < vm.frame_count; i++) {
        gc_mark_object((Obj*)vm.frames[i].closure);
    }
    for (ObjUpvalue* upvalue = vm.open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
        gc_mark_object((Obj*)upvalue);
    }
    table_mark_reachable(&vm.globals);
    compiler_mark_roots();
}

static void gc_trace_references() {
    while (vm.gray_count > 0) {
        Obj* object = vm.gray_stack[--vm.gray_count];
        gc_blacken_object(object);
    }
}

static void gc_sweep() {
    Obj* previous = NULL;
    Obj* object = vm.objects;
    while (object != NULL) {
        if (object->is_marked) {
            object->is_marked = false;
            previous = object;
            object = object->next;
        } else {
            Obj* unreached = object;
            object = object->next;
            if (previous != NULL) {
                previous->next = object;
            } else {
                vm.objects = object;
            }
            free_object(unreached);
        }
    }
}

void gc_collect() {
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm.bytes_allocated;
#endif

    gc_mark_roots();
    gc_trace_references();
    gc_sweep();

    vm.gc_threshold = vm.bytes_allocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
            before - vm.bytes_allocated, before, vm.bytes_allocated, vm.gc_threshold);
#endif
}
