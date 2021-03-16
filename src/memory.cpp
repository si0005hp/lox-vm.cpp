#include "memory.h"

#include <stdlib.h>

#include "compiler.h"
#include "object.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>

#include "debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

namespace lox
{

void* reallocate(void* pointer, size_t oldSize, size_t newSize)
{
    vm.bytesAllocated_ += newSize - oldSize;

    if (newSize > oldSize)
    {
#ifdef DEBUG_STRESS_GC
        collectGarbage();
#endif

        if (vm.bytesAllocated_ > vm.nextGC_) collectGarbage();
    }

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
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
#endif

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
    free(vm.grayStack_);
}

void markObject(Obj* object)
{
    if (object == NULL) return;
    if (object->isMarked) return; // To avoid infinite loop.

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    object->isMarked = true;

    if (vm.grayCapacity_ < vm.grayCount_ + 1)
    {
        vm.grayCapacity_ = GROW_CAPACITY(vm.grayCapacity_);
        // Use realloc because the memory for the gray stack itself is not
        // managed by the garbage collector.
        vm.grayStack_ =
          (Obj**)realloc(vm.grayStack_, sizeof(Obj*) * vm.grayCapacity_);

        if (vm.grayStack_ == NULL) exit(1);
    }
    vm.grayStack_[vm.grayCount_++] = object;
}

void markValue(Value value)
{
    if (!IS_OBJ(value)) return;
    markObject(AS_OBJ(value));
}

static void markRoots()
{
    for (Value* slot = vm.stack(); slot < vm.stackTop(); slot++)
        markValue(*slot);

    for (int i = 0; i < vm.frameCount(); i++)
        markObject((Obj*)vm.frames()[i].closure);

    for (ObjUpvalue* upvalue = vm.openUpvalues(); upvalue != NULL;
         upvalue = upvalue->next)
        markObject((Obj*)upvalue);

    markTable(vm.globals());
    markCompilerRoots();
}

static void markArray(ValueArray* array)
{
    for (int i = 0; i < array->count(); i++) { markValue(array->elems()[i]); }
}

// Note that we don’t set any state in the traversed object itself. There is no
// direct encoding of “black” in the object’s state. A black object is any
// object whose isMarked field is set and that is no longer in the gray stack.
static void blackenObject(Obj* object)
{
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif

    switch (object->type)
    {
        case OBJ_UPVALUE: markValue(((ObjUpvalue*)object)->closed); break;
        case OBJ_FUNCTION:
        {
            ObjFunction* function = (ObjFunction*)object;
            markObject((Obj*)function->name);
            markArray(function->chunk.constantsPtr());
            break;
        }
        case OBJ_CLOSURE:
        {
            ObjClosure* closure = (ObjClosure*)object;
            markObject((Obj*)closure->function);
            for (int i = 0; i < closure->upvalueCount; i++)
                markObject((Obj*)closure->upvalues[i]);
            break;
        }

        // strings and native function objects contain no outgoing references so
        // there is nothing to traverse. An easy optimization we could do in
        // markObject() is to skip adding strings and native functions to the
        // gray stack at all since we know they don’t need to be processed.
        // Instead, they could darken from white straight to black.
        case OBJ_NATIVE:
        case OBJ_STRING: break;
    }
}

static void traceReferences()
{
    while (vm.grayCount_ > 0)
    {
        Obj* object = vm.grayStack_[--vm.grayCount_];
        blackenObject(object);
    }
}

static void sweep()
{
    Obj* previous = NULL;
    Obj* object = vm.objects();
    while (object != NULL)
    {
        // If an object is marked (black), leave it alone and continue
        if (object->isMarked)
        {
            object->isMarked = false; // clear the bit for the next run
            previous = object;
            object = object->next;
        }
        else
        {
            //  If it is unmarked (white), unlink it from the list and free it.
            Obj* unreached = object;
            object = object->next;
            if (previous != NULL)
                previous->next = object;
            else
                vm.objects_ = object;

            freeObject(unreached);
        }
    }
}

void collectGarbage()
{
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm.bytesAllocated_;
#endif

    markRoots();
    traceReferences();
    tableRemoveWhite(vm.strings());
    sweep();

    vm.nextGC_ = vm.bytesAllocated_ * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
           before - vm.bytesAllocated_, before, vm.bytesAllocated_, vm.nextGC_);
#endif
}

} // namespace lox
