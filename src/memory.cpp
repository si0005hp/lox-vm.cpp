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
