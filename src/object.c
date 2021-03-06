#include <stdio.h>
#include <string.h>

#include "include/ghost.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(vm, type, objectType) \
    (type*)allocateObject(vm, sizeof(type), objectType)

static Obj* allocateObject(GhostVM *vm, size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(vm, NULL, 0, size);
    object->type = type;
    object->isMarked = false;

    object->next = vm->objects;
    vm->objects = object;

    #if DEBUG_LOG_GC
        printf("%p allocate %ld for %d\n", (void*)object, size, type);
    #endif

    return object;
}

ObjBoundMethod* newBoundMethod(GhostVM *vm, Value receiver, ObjClosure* method) {
    ObjBoundMethod* bound = ALLOCATE_OBJ(vm, ObjBoundMethod, OBJ_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;
    return bound;
}

ObjClass* newClass(GhostVM *vm, ObjString* name) {
    // "class" is a reserved word in C++, which
    // ghost can be compiled in. Thus, "klass".
    ObjClass* klass = ALLOCATE_OBJ(vm, ObjClass, OBJ_CLASS);
    klass->name = name;
    initTable(&klass->methods);
    return klass;
}

ObjNativeClass* newNativeClass(GhostVM *vm, ObjString *name) {
    ObjNativeClass* klass = ALLOCATE_OBJ(vm, ObjNativeClass, OBJ_NATIVE_CLASS);
    klass->name = name;
    initTable(&klass->methods);
    return klass;
}

ObjClosure* newClosure(GhostVM *vm, ObjFunction* function) {
    ObjUpvalue** upvalues = ALLOCATE(vm, ObjUpvalue*, function->upvalueCount);
    for (int i = 0; i < function->upvalueCount; i++) {
        upvalues[i] = NULL;
    }

    ObjClosure* closure = ALLOCATE_OBJ(vm, ObjClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

ObjFunction* newFunction(GhostVM *vm) {
    ObjFunction* function = ALLOCATE_OBJ(vm, ObjFunction, OBJ_FUNCTION);

    function->arity = 0;
    function->upvalueCount = 0;
    function->name = NULL;
    initChunk(&function->chunk);

    return function;
}

ObjInstance* newInstance(GhostVM *vm, ObjClass* klass) {
    ObjInstance* instance = ALLOCATE_OBJ(vm, ObjInstance, OBJ_INSTANCE);
    instance->klass = klass;
    initTable(&instance->fields);
    return instance;
}

ObjNative* newNative(GhostVM *vm, NativeFn function) {
    ObjNative* native = ALLOCATE_OBJ(vm, ObjNative, OBJ_NATIVE);
    native->function = function;

    return native;
}

ObjList* newList(GhostVM *vm) {
    ObjList* list = ALLOCATE_OBJ(vm, ObjList, OBJ_LIST);
    initValueArray(&list->values);

    return list;
}

static ObjString* allocateString(GhostVM *vm, char* chars, int length, uint32_t hash) {
    ObjString* string = ALLOCATE_OBJ(vm, ObjString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    push(vm, OBJ_VAL(string));
    tableSet(vm, &vm->strings, string, NULL_VAL);
    pop(vm);

    return string;
}

// The hashString function utilizes the FNV-1a algorithm to
// create a unique and reproducable hash of the given key
// with the given length. "FNV" stands for "Fowler/Noll/Vo",
// named after the creators of the algorithm.
static uint32_t hashString(const char* key, int length) {
    // we are implementing a 32bit hash, so the hash and prime
    // values are set appropriately for this size.
    uint32_t hash = 2166136261u;
    uint32_t prime = 16777619u;

    for (int i = 0; i < length; i++) {
        hash ^= key[i];
        hash *= prime;
    }

    return hash;
}

ObjString* takeString(GhostVM *vm, char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm->strings, chars, length, hash);

    if (interned != NULL) {
        FREE_ARRAY(vm, char, chars, length + 1);
        return interned;
    }

    return allocateString(vm, chars, length, hash);
}

ObjString* copyString(GhostVM *vm, const char* chars, int length) {
    uint32_t hash = hashString(chars, length);
    ObjString* interned = tableFindString(&vm->strings, chars, length, hash);

    if (interned != NULL) return interned;

    char* heapChars = ALLOCATE(vm, char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return allocateString(vm, heapChars, length, hash);
}

ObjUpvalue* newUpvalue(GhostVM *vm, Value* slot) {
    ObjUpvalue* upvalue = ALLOCATE_OBJ(vm, ObjUpvalue, OBJ_UPVALUE);
    upvalue->closed = NULL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

void printFunction(ObjFunction* function) {
    if (function->name == NULL) {
        printf("<script>");
        return;
    }

    printf("<fn %s>", function->name->chars);
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_CLASS:
        case OBJ_NATIVE_CLASS:
            printf("%s", AS_CLASS(value)->name->chars);
            break;
        case OBJ_BOUND_METHOD:
            printFunction(AS_BOUND_METHOD(value)->method->function);
            break;
        case OBJ_CLOSURE:
            printFunction(AS_CLOSURE(value)->function);
            break;
        case OBJ_FUNCTION:
            printFunction(AS_FUNCTION(value));
            break;
        case OBJ_INSTANCE:
            // TO-DO
            // Implement a "toString()" method that lets classes
            // specify how its instances are converted to a string and
            // printed here.
            printf("%s instance", AS_INSTANCE(value)->klass->name->chars);
            break;
        case OBJ_NATIVE:
            printf("<native fn>");
            break;
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;

        case OBJ_LIST: {
            ObjList* list = AS_LIST(value);
            printf("[");

            for (int i = 0; i < list->values.count; ++i) {
                printValue(list->values.values[i]);

                if (i != list->values.count - 1) {
                    printf(", ");
                }
            }

            printf("]");
            break;
        }

        case OBJ_UPVALUE:
            printf("upvalue");
            break;
    }
}