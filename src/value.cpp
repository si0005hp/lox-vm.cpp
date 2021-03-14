#include "value.h"

#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"

namespace lox
{

bool valuesEqual(Value a, Value b)
{
    /* cannot simply memcmp here as C has no rule regarding padding value */

    if (a.type != b.type) return false;

    switch (a.type)
    {
        case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NIL: return true;
        case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJ: return AS_OBJ(a) == AS_OBJ(b);
        default: return false; // Unreachable.
    }
}

void printValue(Value value)
{
    switch (value.type)
    {
        case VAL_BOOL: printf(AS_BOOL(value) ? "true" : "false"); break;
        case VAL_NIL: printf("nil"); break;
        case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
        case VAL_OBJ: printObject(value); break;
    }
}

} // namespace lox
