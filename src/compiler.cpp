#include "compiler.h"

#include <unordered_map>

#include "object.h"
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

bool Parser::match(TokenType type)
{
    if (!check(type)) return false;
    advance();
    return true;
}

bool Parser::check(TokenType type)
{
    return current().type == type;
}

void Parser::synchronize()
{
    panicMode_ = false;

    while (current().type != TOKEN_EOF)
    {
        if (previous().type == TOKEN_SEMICOLON) return;

        switch (current().type)
        {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN: return;
            default:
              // Do nothing.
              ;
        }
        advance();
    }
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

static void variable(bool canAssign);
static void binary(bool canAssign);
static void literal(bool canAssign);
static void grouping(bool canAssign);
static void number(bool canAssign);
static void string(bool canAssign);
static void unary(bool canAssign);

static void statement();
static void declaration();
static void expressionStatement();
static void varDeclaration();

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
  {TOKEN_BANG, {unary, NULL, PREC_NONE}},
  {TOKEN_BANG_EQUAL, {NULL, binary, PREC_EQUALITY}},
  {TOKEN_EQUAL, {NULL, NULL, PREC_NONE}},
  {TOKEN_EQUAL_EQUAL, {NULL, binary, PREC_EQUALITY}},
  {TOKEN_GREATER, {NULL, binary, PREC_COMPARISON}},
  {TOKEN_GREATER_EQUAL, {NULL, binary, PREC_COMPARISON}},
  {TOKEN_LESS, {NULL, binary, PREC_COMPARISON}},
  {TOKEN_LESS_EQUAL, {NULL, binary, PREC_COMPARISON}},
  {TOKEN_IDENTIFIER, {variable, NULL, PREC_NONE}},
  {TOKEN_STRING, {string, NULL, PREC_NONE}},
  {TOKEN_NUMBER, {number, NULL, PREC_NONE}},
  {TOKEN_AND, {NULL, NULL, PREC_NONE}},
  {TOKEN_CLASS, {NULL, NULL, PREC_NONE}},
  {TOKEN_ELSE, {NULL, NULL, PREC_NONE}},
  {TOKEN_FALSE, {literal, NULL, PREC_NONE}},
  {TOKEN_FOR, {NULL, NULL, PREC_NONE}},
  {TOKEN_FUN, {NULL, NULL, PREC_NONE}},
  {TOKEN_IF, {NULL, NULL, PREC_NONE}},
  {TOKEN_NIL, {literal, NULL, PREC_NONE}},
  {TOKEN_OR, {NULL, NULL, PREC_NONE}},
  {TOKEN_PRINT, {NULL, NULL, PREC_NONE}},
  {TOKEN_RETURN, {NULL, NULL, PREC_NONE}},
  {TOKEN_SUPER, {NULL, NULL, PREC_NONE}},
  {TOKEN_THIS, {NULL, NULL, PREC_NONE}},
  {TOKEN_TRUE, {literal, NULL, PREC_NONE}},
  {TOKEN_VAR, {NULL, NULL, PREC_NONE}},
  {TOKEN_WHILE, {NULL, NULL, PREC_NONE}},
  {TOKEN_ERROR, {NULL, NULL, PREC_NONE}},
  {TOKEN_EOF, {NULL, NULL, PREC_NONE}},
};

static ParseRule* getRule(TokenType type)
{
    return &rules.at(type);
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

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= getRule(parser.current().type)->precedence)
    {
        parser.advance();
        ParseFn infixRule = getRule(parser.previous().type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && parser.match(TOKEN_EQUAL))
        error("Invalid assignment target.");
}

static uint8_t identifierConstant(const Token& name)
{
    return makeConstant(OBJ_VAL(copyString(name.start, name.length)));
}

static uint8_t parseVariable(const char* errorMessage)
{
    parser.consume(TOKEN_IDENTIFIER, errorMessage);
    return identifierConstant(parser.previous());
}

static void defineVariable(uint8_t global)
{
    emitBytes(OP_DEFINE_GLOBAL, global);
}

static void namedVariable(const Token& name, bool canAssign)
{
    uint8_t arg = identifierConstant(name);

    if (canAssign && parser.match(TOKEN_EQUAL))
    {
        expression();
        emitBytes(OP_SET_GLOBAL, arg);
    }
    else
    {
        emitBytes(OP_GET_GLOBAL, arg);
    }
}

/* expressions */
static void variable(bool canAssign)
{
    namedVariable(parser.previous(), canAssign);
}

static void literal(bool canAssign)
{
    switch (parser.previous().type)
    {
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_NIL: emitByte(OP_NIL); break;
        case TOKEN_TRUE: emitByte(OP_TRUE); break;
        default: return; // Unreachable.
    }
}

static void number(bool canAssign)
{
    double value = strtod(parser.previous().start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void string(bool canAssign)
{
    emitConstant(OBJ_VAL(
      copyString(parser.previous().start + 1, parser.previous().length - 2)));
}

static void expression()
{
    parsePrecedence(PREC_ASSIGNMENT);
}

static void grouping(bool canAssign)
{
    expression();
    parser.consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void unary(bool canAssign)
{
    TokenType operatorType = parser.previous().type;

    // Compile the operand.
    parsePrecedence(PREC_UNARY);

    // Emit the operator instruction.
    switch (operatorType)
    {
        case TOKEN_BANG: emitByte(OP_NOT); break;
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        default: return; // Unreachable.
    }
}

static void binary(bool canAssign)
{
    // Remember the operator.
    TokenType operatorType = parser.previous().type;

    // Compile the right operand.
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    // Emit the operator instruction.
    switch (operatorType)
    {
        case TOKEN_BANG_EQUAL: emitBytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL: emitByte(OP_EQUAL); break;
        case TOKEN_GREATER: emitByte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS: emitByte(OP_LESS); break;
        case TOKEN_LESS_EQUAL: emitBytes(OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS: emitByte(OP_ADD); break;
        case TOKEN_MINUS: emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR: emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH: emitByte(OP_DIVIDE); break;
        default: return; // Unreachable.
    }
}

/* statements */

static void declaration()
{
    if (parser.match(TOKEN_VAR))
        varDeclaration();
    else
        statement();

    if (parser.panicMode()) parser.synchronize();
}

static void varDeclaration()
{
    uint8_t global = parseVariable("Expect variable name.");

    if (parser.match(TOKEN_EQUAL))
        expression();
    else
        emitByte(OP_NIL);

    parser.consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(global);
}

static void printStatement()
{
    expression();
    parser.consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

static void expressionStatement()
{
    expression();
    parser.consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

static void statement()
{
    if (parser.match(TOKEN_PRINT))
        printStatement();
    else
        expressionStatement();
}

bool compile(const char* source, Chunk* chunk)
{
    scanner.init(source);
    compilingChunk = chunk;

    parser.clearError();

    parser.advance();

    while (!parser.match(TOKEN_EOF)) declaration();

    endCompiler();
    return !parser.hadError();
}

} // namespace lox
