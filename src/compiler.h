#pragma once

#include <iostream>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
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

typedef void (*ParseFn)();

struct ParseRule
{
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
};

bool compile(const char* source, Chunk* chunk);

class Parser
{
  public:
    void advance();
    void errorAtCurrent(const char* message);
    void errorAt(Token* token, const char* message);
    void consume(TokenType type, const char* message);

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
