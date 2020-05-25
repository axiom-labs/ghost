#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../include/ghost.h"
#include "assert.h"
#include "../vm.h"

static Value
assertIsTrue(GhostVM *vm, int argCount, Value *args)
{
    if (argCount == 0)
    {
        runtimeError(vm, "Assert.isTrue() expects at least one argument.");
        return NULL_VAL;
    }

    if (isFalsey(args[0]))
    {
        if (argCount == 2)
        {
            char message[1024];
            sprintf(message, "Failed asserting that %s", AS_CSTRING(args[1]));
            runtimeError(vm, message);
        }
        else
        {
            runtimeError(vm, "Assert.isTrue() failed.");
        }

        exit(70);
    }

    return NULL_VAL;
}

static Value
assertIsFalse(GhostVM *vm, int argCount, Value *args)
{
    if (argCount == 0)
    {
        runtimeError(vm, "Assert.isFalse() expects at least one argument.");
        return NULL_VAL;
    }

    if (!isFalsey(args[0]))
    {
        if (argCount == 2)
        {
            char message[1024];
            sprintf(message, "Failed asserting that %s", AS_CSTRING(args[1]));
            runtimeError(vm, message);
        }
        else
        {
            runtimeError(vm, "Assert.isFalse() failed.");
        }

        exit(70);
    }

    return NULL_VAL;
}

static Value
assertEquals(GhostVM *vm, int argCount, Value *args)
{
    if (argCount < 2)
    {
        runtimeError(vm, "Assert.equals() expects at least two arguments.");
        return NULL_VAL;
    }

    Value a = args[0];
    Value b = args[1];
    Value result = BOOL_VAL(valuesEqual(a, b));

    if (isFalsey(result))
    {
        if (argCount == 3)
        {
            char message[1024];
            sprintf(message, "Failed asserting that %s", AS_CSTRING(args[2]));
            runtimeError(vm, message);
        }
        else
        {
            runtimeError(vm, "Assert.equals() failed.");
        }

        exit(70);
    }

    return NULL_VAL;
}

void registerAssertModule(GhostVM *vm)
{
    ObjString *name = copyString(vm, "Assert", 6);
    push(vm, OBJ_VAL(name));
    ObjNativeClass *klass = newNativeClass(vm, name);
    push(vm, OBJ_VAL(klass));

    defineNativeMethod(vm, klass, "isTrue", assertIsTrue);
    defineNativeMethod(vm, klass, "isFalse", assertIsFalse);
    defineNativeMethod(vm, klass, "equals", assertEquals);

    tableSet(vm, &vm->globals, name, OBJ_VAL(klass));
    pop(vm);
    pop(vm);
}