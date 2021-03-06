#pragma once

#include "chunk.h"
#include "value.h"

#define STACK_MAX 256

namespace lox
{

typedef enum
{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

class VM
{
  public:
    VM() : stackTop_(stack_) {}

    InterpretResult interpret(Chunk *chunk);
    InterpretResult interpret(const char *source);
    void free();

    void push(Value value);
    Value pop();

  private:
    InterpretResult run();

    Chunk *chunk_;
    const uint8_t *ip_;
    Value stack_[STACK_MAX];
    Value *stackTop_;
};

inline VM vm;

} // namespace lox
