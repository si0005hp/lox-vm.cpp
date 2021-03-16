#pragma once

#include <iostream>

#include "chunk.h"
#include "common.h"
#include "scanner.h"
#include "value.h"

namespace lox
{

typedef enum
{
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_TERM,       // + -
    PREC_FACTOR,     // * /
    PREC_UNARY,      // ! -
    PREC_CALL,       // . ()
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

struct ParseRule
{
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
};

struct Local
{
    Token name;
    int depth;
    bool isCaptured;
};

struct Upvalue
{
    uint8_t index;
    bool isLocal;
};

typedef enum
{
    TYPE_FUNCTION,
    TYPE_SCRIPT
} FunctionType;

class Compiler
{
  public:
    Compiler() : localCount_(0), scopeDepth_(0) {}
    void init(FunctionType type);

    void beginScope();
    void endScope();
    int resolveLocal(const Token& name);
    int resolveUpvalue(const Token& name);
    int addUpvalue(uint8_t index, bool isLocal);

    /* fields */
    struct Compiler* enclosing_;

    ObjFunction* function_;
    FunctionType type_;

    Local locals_[UINT8_COUNT];
    int localCount_;
    int scopeDepth_;
    Upvalue upvalues_[UINT8_COUNT];
};

ObjFunction* compile(const char* source);
void markCompilerRoots();

class Parser
{
  public:
    void advance();
    void errorAtCurrent(const char* message);
    void errorAt(Token* token, const char* message);
    void consume(TokenType type, const char* message);
    bool match(TokenType type);
    bool check(TokenType type);
    void synchronize();

    void clearError();

    Token& current() { return current_; }
    Token& previous() { return previous_; }
    bool hadError() const { return hadError_; }
    bool panicMode() const { return panicMode_; }

  private:
    Token current_;
    Token previous_;
    bool hadError_;
    bool panicMode_;
};

inline Parser parser;

} // namespace lox
