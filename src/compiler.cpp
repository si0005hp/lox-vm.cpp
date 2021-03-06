#include "compiler.h"

#include "scanner.h"

namespace lox
{

void compile(const char* source)
{
    scanner.init(source);
}

} // namespace lox
