#include "memory.h"

#include <stdlib.h>

#include "object.h"
#include "vm.h"

namespace lox
{

void* reallocate(void* pointer, size_t oldSize, size_t newSize)
{
    if (newSize == 0)
    {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, newSize);
    if (result == NULL) exit(1);
    return result;
}

static void freeObject(Obj* object)
{
    switch (object->type)
    {
        case OBJ_STRING:
        {
            ObjString* string = (ObjString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(ObjString, object);
            break;
        }
        case OBJ_FUNCTION:
        {
            ObjFunction* function = (ObjFunction*)object;
            function->chunk.free();
            FREE(ObjFunction, object);
            // function’s name will be managed by GC
            break;
        }
        case OBJ_CLOSURE:
        {
            // Free only the ObjClosure itself and ObjUpvalue 'arrays', not the
            // ObjFunction and actual ObjUpvalue objects. Because there may be
            // multiple closures referencing the same function. None of them
            // claims any special privilege over it.
            ObjClosure* closure = (ObjClosure*)object;
            FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
            FREE(ObjClosure, object);
            break;
        }
        case OBJ_UPVALUE:
        {
            // Similar to the case of OBJ_CLOSURE, not own the variable it
            // references and free only ObjUpvalue as multiple closures can
            // close over the same variable.
            FREE(ObjUpvalue, object);
            break;
        }
        case OBJ_NATIVE: FREE(ObjNative, object); break;
    }
}

void freeObjects()
{
    Obj* object = vm.objects();
    while (object != NULL)
    {
        Obj* next = object->next;
        freeObject(object);
        object = next;
    }
}

} // namespace lox
