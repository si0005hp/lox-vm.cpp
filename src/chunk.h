#pragma once

#include "array.h"
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
    const uint8_t* code() const { return code_; };
    const int* lines() const { return lines_; };

    const ValueArray& constants() const { return constants_; }

  private:
    int count_;
    int capacity_;
    uint8_t* code_;

    int* lines_;
    ValueArray constants_;
};

} // namespace lox
