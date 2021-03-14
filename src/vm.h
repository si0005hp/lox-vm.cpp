#pragma once

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

namespace lox
{

typedef enum
{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

struct CallFrame
{
    ObjClosure *closure;
    uint8_t *ip;
    Value *slots;
};

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
    bool callValue(Value callee, int argCount);
    bool call(ObjClosure *closure, int argCount);
    ObjUpvalue *captureUpvalue(Value *local);
    void closeUpvalues(Value *last);

    void runtimeError(const char *format, ...);
    void defineNative(const char *name, NativeFn function);

    Obj *objects() const { return objects_; }
    void setObjects(Obj *objects) { objects = objects_; }
    Table *strings() { return &strings_; }

  private:
    InterpretResult run();

    CallFrame frames_[FRAMES_MAX];
    int frameCount_;
    Value stack_[STACK_MAX];
    Value *stackTop_;
    Table strings_; // intern
    Table globals_; // global variables

    Obj *objects_;
    ObjUpvalue *openUpvalues_;
};

inline VM vm;

} // namespace lox
