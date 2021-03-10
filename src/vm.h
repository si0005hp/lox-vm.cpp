#pragma once

#include "chunk.h"
#include "table.h"
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
    VM();

    void resetStack();
    InterpretResult interpret(Chunk *chunk);
    InterpretResult interpret(const char *source);
    void free();
    void push(Value value);
    bool isFalsey(Value value) const;
    Value pop();
    Value peek(int distance) const;
    void concatenate();

    void runtimeError(const char *format, ...);

    Obj *objects() const { return objects_; }
    void setObjects(Obj *objects) { objects = objects_; }
    Table *strings() { return &strings_; }

  private:
    InterpretResult run();

    Chunk *chunk_;
    const uint8_t *ip_;
    Value stack_[STACK_MAX];
    Value *stackTop_;
    Table strings_; // intern

    Obj *objects_;
};

inline VM vm;

} // namespace lox
