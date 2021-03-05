#pragma once

#include <iostream>

#include "array.h"
#include "common.h"

namespace lox
{

typedef double Value;

class ValueArray : public Array<Value>
{
};

inline void printValue(Value value)
{
    std::cout << value;
}

} // namespace lox
