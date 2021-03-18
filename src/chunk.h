#pragma once

#include "common.h"
#include "value.h"

namespace lox
{

typedef enum
{
    OP_RETURN,
    OP_NOT,
    OP_NEGATE,
    OP_CONSTANT,
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,
    OP_PRINT,
    OP_POP,
    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_JUMP_IF_FALSE,
    OP_JUMP,
    OP_LOOP,
    OP_CALL,
    OP_CLOSURE,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_CLOSE_UPVALUE,
    OP_CLASS,
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,
    OP_METHOD,
    OP_INVOKE,
} OpCode;

class Chunk
{
  public:
    Chunk() : count_(0), capacity_(0), code_(NULL), lines_(NULL) {}

    void init();
    void free();
    void write(uint8_t byte, int line);
    int addConstant(Value value);

    int count() const { return count_; };
    int capacity() const { return capacity_; };
    uint8_t* code() const { return code_; };
    const int* lines() const { return lines_; };

    const ValueArray& constants() const { return constants_; }
    ValueArray* constantsPtr() { return &constants_; }

  private:
    int count_;
    int capacity_;
    uint8_t* code_;

    int* lines_;
    ValueArray constants_;
};

} // namespace lox
