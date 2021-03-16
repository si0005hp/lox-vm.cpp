#include "chunk.h"

#include "memory.h"
#include "value.h"
#include "vm.h"

namespace lox
{

void Chunk::init()
{
    count_ = 0;
    capacity_ = 0;
    code_ = NULL;
    lines_ = NULL;
    constants_.init();
}

void Chunk::free()
{
    FREE_ARRAY(uint8_t, code_, capacity_);
    FREE_ARRAY(int, lines_, capacity_);
    constants_.free();
    init();
}

void Chunk::write(uint8_t byte, int line)
{
    if (capacity_ < count_ + 1)
    {
        int oldCapacity = capacity_;
        capacity_ = GROW_CAPACITY(oldCapacity);
        code_ = GROW_ARRAY(uint8_t, code_, oldCapacity, capacity_);
        lines_ = GROW_ARRAY(int, lines_, oldCapacity, capacity_);
    }

    code_[count_] = byte;
    lines_[count_] = line;
    count_++;
}

int Chunk::addConstant(Value value)
{
    vm.push(value); // For GC
    constants_.write(value);
    vm.pop();
    return constants_.count() - 1;
}

} // namespace lox
