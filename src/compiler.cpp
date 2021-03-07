#include "compiler.h"

#include <unordered_map>

#include "scanner.h"
#include "vm.h"
#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

namespace lox
{

void Parser::clearError()
{
    hadError_ = false;
    panicMode_ = false;
}

void Parser::advance()
{
    previous_ = current_;

    for (;;)
    {
        current_ = scanner.scanToken();
        if (current_.type != TOKEN_ERROR) break;

        errorAtCurrent(current_.start);
    }
}

void Parser::errorAtCurrent(const char* message)
{
    errorAt(&current_, message);
}

void Parser::errorAt(Token* token, const char* message)
{
    if (panicMode_) return;
    panicMode_ = true;

    std::cerr << "[line " << token->line << "] Error";

    if (token->type == TOKEN_EOF)
        std::cerr << " at end";
    else if (token->type == TOKEN_ERROR)
        ; // Nothing todo.
    else
        std::cerr << " at " << token->length << " " << token->start;

    std::cerr << ": " << message << std::endl;
    hadError_ = true;
}

void Parser::consume(TokenType type, const char* message)
{
    if (current_.type == type)
    {
        advance();
        return;
    }
    errorAtCurrent(message);
}

/* code gen */

Chunk* compilingChunk;

static Chunk* currentChunk()
{
    return compilingChunk;
}

static void emitByte(uint8_t byte)
{
    currentChunk()->write(byte, parser.previous().line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2)
{
    emitByte(byte1);
    emitByte(byte2);
}

static void emitReturn()
{
    emitByte(OP_RETURN);
}

static void endCompiler()
{
    emitReturn();
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError()) { disassembleChunk(currentChunk(), "code"); }
#endif
}

static void error(const char* message)
{
    parser.errorAt(&parser.previous(), message);
}

static uint8_t makeConstant(Value value)
{
    int constant = currentChunk()->addConstant(value);
    if (constant > UINT8_MAX)
    {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static void emitConstant(Value value)
{
    emitBytes(OP_CONSTANT, makeConstant(value));
}

/* forward decl */
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);
static void expression();
static void number();
static void unary();
static void binary();
static void grouping();

static void number()
{
    double value = strtod(parser.previous().start, NULL);
    emitConstant(value);
}

static void expression()
{
    parsePrecedence(PREC_ASSIGNMENT);
}

static void grouping()
{
    expression();
    parser.consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary()
{
    TokenType operatorType = parser.previous().type;

    // Compile the operand.
    parsePrecedence(PREC_UNARY);

    // Emit the operator instruction.
    switch (operatorType)
    {
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        default: return; // Unreachable.
    }
}

static void binary()
{
    // Remember the operator.
    TokenType operatorType = parser.previous().type;

    // Compile the right operand.
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    // Emit the operator instruction.
    switch (operatorType)
    {
        case TOKEN_PLUS: emitByte(OP_ADD); break;
        case TOKEN_MINUS: emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR: emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH: emitByte(OP_DIVIDE); break;
        default: return; // Unreachable.
    }
}

static void parsePrecedence(Precedence precedence)
{
    parser.advance();
    ParseFn prefixRule = getRule(parser.previous().type)->prefix;
    if (prefixRule == NULL)
    {
        error("Expect expression.");
        return;
    }
    prefixRule();

    while (precedence <= getRule(parser.current().type)->precedence)
    {
        parser.advance();
        ParseFn infixRule = getRule(parser.previous().type)->infix;
        infixRule();
    }
}

static std::unordered_map<int, ParseRule> rules = {
  {TOKEN_LEFT_PAREN, {grouping, NULL, PREC_NONE}},
  {TOKEN_RIGHT_PAREN, {NULL, NULL, PREC_NONE}},
  {TOKEN_LEFT_BRACE, {NULL, NULL, PREC_NONE}},
  {TOKEN_RIGHT_BRACE, {NULL, NULL, PREC_NONE}},
  {TOKEN_COMMA, {NULL, NULL, PREC_NONE}},
  {TOKEN_DOT, {NULL, NULL, PREC_NONE}},
  {TOKEN_MINUS, {unary, binary, PREC_TERM}},
  {TOKEN_PLUS, {NULL, binary, PREC_TERM}},
  {TOKEN_SEMICOLON, {NULL, NULL, PREC_NONE}},
  {TOKEN_SLASH, {NULL, binary, PREC_FACTOR}},
  {TOKEN_STAR, {NULL, binary, PREC_FACTOR}},
  {TOKEN_BANG, {NULL, NULL, PREC_NONE}},
  {TOKEN_BANG_EQUAL, {NULL, NULL, PREC_NONE}},
  {TOKEN_EQUAL, {NULL, NULL, PREC_NONE}},
  {TOKEN_EQUAL_EQUAL, {NULL, NULL, PREC_NONE}},
  {TOKEN_GREATER, {NULL, NULL, PREC_NONE}},
  {TOKEN_GREATER_EQUAL, {NULL, NULL, PREC_NONE}},
  {TOKEN_LESS, {NULL, NULL, PREC_NONE}},
  {TOKEN_LESS_EQUAL, {NULL, NULL, PREC_NONE}},
  {TOKEN_IDENTIFIER, {NULL, NULL, PREC_NONE}},
  {TOKEN_STRING, {NULL, NULL, PREC_NONE}},
  {TOKEN_NUMBER, {number, NULL, PREC_NONE}},
  {TOKEN_AND, {NULL, NULL, PREC_NONE}},
  {TOKEN_CLASS, {NULL, NULL, PREC_NONE}},
  {TOKEN_ELSE, {NULL, NULL, PREC_NONE}},
  {TOKEN_FALSE, {NULL, NULL, PREC_NONE}},
  {TOKEN_FOR, {NULL, NULL, PREC_NONE}},
  {TOKEN_FUN, {NULL, NULL, PREC_NONE}},
  {TOKEN_IF, {NULL, NULL, PREC_NONE}},
  {TOKEN_NIL, {NULL, NULL, PREC_NONE}},
  {TOKEN_OR, {NULL, NULL, PREC_NONE}},
  {TOKEN_PRINT, {NULL, NULL, PREC_NONE}},
  {TOKEN_RETURN, {NULL, NULL, PREC_NONE}},
  {TOKEN_SUPER, {NULL, NULL, PREC_NONE}},
  {TOKEN_THIS, {NULL, NULL, PREC_NONE}},
  {TOKEN_TRUE, {NULL, NULL, PREC_NONE}},
  {TOKEN_VAR, {NULL, NULL, PREC_NONE}},
  {TOKEN_WHILE, {NULL, NULL, PREC_NONE}},
  {TOKEN_ERROR, {NULL, NULL, PREC_NONE}},
  {TOKEN_EOF, {NULL, NULL, PREC_NONE}},
};

static ParseRule* getRule(TokenType type)
{
    return &rules.at(type);
}

bool compile(const char* source, Chunk* chunk)
{
    scanner.init(source);
    compilingChunk = chunk;

    parser.clearError();

    parser.advance();

    expression();

    parser.consume(TOKEN_EOF, "Expect end of expression.");
    endCompiler();
    return !parser.hadError();
}

} // namespace lox
