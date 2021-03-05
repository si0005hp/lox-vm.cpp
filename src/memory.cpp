#include "memory.h"

#include <stdlib.h>

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
    if (result == NULL)
        exit(1);
    return result;
}

} // namespace lox
