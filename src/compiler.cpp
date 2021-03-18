#include "compiler.h"

#include <string.h>

#include <unordered_map>

#include "memory.h"
#include "object.h"
#include "scanner.h"
#include "vm.h"
#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

#define LOCAL_DECLARE_UNINITIALIZED -1

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

static void error(const char* message)
{
    parser.errorAt(&parser.previous(), message);
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

Compiler* current = NULL;

Chunk* compilingChunk;

static Chunk* currentChunk()
{
    return &current->function_->chunk;
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

static bool identifiersEqual(const Token& a, const Token& b)
{
    if (a.length != b.length) return false;
    return memcmp(a.start, b.start, a.length) == 0;
}

void Compiler::init(FunctionType type)
{
    enclosing_ = current;
    // function_ = NULL;
    type_ = type;
    localCount_ = 0;
    scopeDepth_ = 0;
    function_ = newFunction();
    current = this;

    if (type != TYPE_SCRIPT)
        current->function_->name =
          copyString(parser.previous().start, parser.previous().length);

    Local* local = &locals_[localCount_++];
    local->depth = 0;
    local->isCaptured = false;
    local->name.start = "";
    local->name.length = 0;
}

void Compiler::beginScope()
{
    scopeDepth_++;
}

void Compiler::endScope()
{
    scopeDepth_--;

    while (current->localCount_ > 0 &&
           current->locals_[current->localCount_ - 1].depth >
             current->scopeDepth_)
    {
        if (current->locals_[current->localCount_ - 1].isCaptured)
            emitByte(OP_CLOSE_UPVALUE);
        else
            emitByte(OP_POP);

        current->localCount_--;
    }
}

int Compiler::resolveLocal(const Token& name)
{
    for (int i = localCount_ - 1; i >= 0; i--)
    {
        Local* local = &locals_[i];
        if (identifiersEqual(name, local->name))
        {
            if (local->depth == LOCAL_DECLARE_UNINITIALIZED)
                error("Can't read local variable in its own initializer.");
            return i;
        }
    }
    return -1;
}

int Compiler::addUpvalue(uint8_t index, bool isLocal)
{
    int upvalueCount = function_->upvalueCount;

    //  first check to see if the function already has the same upvalue
    for (int i = 0; i < upvalueCount; i++)
    {
        Upvalue* upvalue = &upvalues_[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) return i;
    }

    if (upvalueCount == UINT8_COUNT)
    {
        error("Too many closure variables in function.");
        return 0;
    }

    upvalues_[upvalueCount].isLocal = isLocal;
    upvalues_[upvalueCount].index = index;
    return function_->upvalueCount++;
}

int Compiler::resolveUpvalue(const Token& name)
{
    if (enclosing_ == NULL) return -1;

    int local = enclosing_->resolveLocal(name);
    if (local != -1)
    {
        enclosing_->locals_[local].isCaptured = true;
        return addUpvalue((uint8_t)local, true);
    }

    int upvalue = enclosing_->resolveUpvalue(name);
    if (upvalue != -1) return addUpvalue((uint8_t)upvalue, false);

    return -1;
}

static void emitReturn()
{
    emitByte(OP_NIL);
    emitByte(OP_RETURN);
}

static ObjFunction* endCompiler()
{
    emitReturn();
    ObjFunction* function = current->function_;

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError())
    {
        disassembleChunk(currentChunk(), function->name != NULL
                                           ? function->name->chars
                                           : "<script>");
    }
#endif

    current = current->enclosing_;
    return function;
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

static void patchJump(int offset)
{
    int jump = currentChunk()->count() - offset - 2;

    if (jump > UINT16_MAX) error("Too much code to jump over.");

    currentChunk()->code()[offset] = (jump >> 8) & 0xff;
    currentChunk()->code()[offset + 1] = jump & 0xff;
}

static int emitJump(uint8_t instruction)
{
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count() - 2;
}

static void emitLoop(int loopStart)
{
    emitByte(OP_LOOP);

    int offset = currentChunk()->count() - loopStart + 2;
    if (offset > UINT16_MAX) error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
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
static void and_(bool canAssign);
static void or_(bool canAssign);
static void call(bool canAssign);
static void dot(bool canAssign);

static std::unordered_map<int, ParseRule> rules = {
  {TOKEN_LEFT_PAREN, {grouping, call, PREC_CALL}},
  {TOKEN_RIGHT_PAREN, {NULL, NULL, PREC_NONE}},
  {TOKEN_LEFT_BRACE, {NULL, NULL, PREC_NONE}},
  {TOKEN_RIGHT_BRACE, {NULL, NULL, PREC_NONE}},
  {TOKEN_COMMA, {NULL, NULL, PREC_NONE}},
  {TOKEN_DOT, {NULL, dot, PREC_CALL}},
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
  {TOKEN_AND, {NULL, and_, PREC_AND}},
  {TOKEN_CLASS, {NULL, NULL, PREC_NONE}},
  {TOKEN_ELSE, {NULL, NULL, PREC_NONE}},
  {TOKEN_FALSE, {literal, NULL, PREC_NONE}},
  {TOKEN_FOR, {NULL, NULL, PREC_NONE}},
  {TOKEN_FUN, {NULL, NULL, PREC_NONE}},
  {TOKEN_IF, {NULL, NULL, PREC_NONE}},
  {TOKEN_NIL, {literal, NULL, PREC_NONE}},
  {TOKEN_OR, {NULL, or_, PREC_OR}},
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

static void addLocal(const Token& name)
{
    if (current->localCount_ == UINT8_COUNT)
    {
        error("Too many local variables in function.");
        return;
    }

    Local* local = &current->locals_[current->localCount_++];
    local->name = name;
    local->depth = LOCAL_DECLARE_UNINITIALIZED;
    local->isCaptured = false;
}

static void declareVariable()
{
    if (current->scopeDepth_ == 0) return;

    Token& name = parser.previous();

    for (int i = current->localCount_ - 1; i >= 0; i--)
    {
        Local* local = &current->locals_[i];
        if (local->depth != -1 && local->depth < current->scopeDepth_) break;

        if (identifiersEqual(name, local->name))
            error("Already variable with this name in this scope.");
    }

    addLocal(name);
}

static uint8_t parseVariable(const char* errorMessage)
{
    parser.consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    if (current->scopeDepth_ > 0)
        return 0; // In local var, return a dummy table index

    return identifierConstant(parser.previous());
}

static void markInitialized()
{
    if (current->scopeDepth_ == 0) return;

    current->locals_[current->localCount_ - 1].depth = current->scopeDepth_;
}

static void defineVariable(uint8_t global)
{
    if (current->scopeDepth_ > 0)
    {
        markInitialized();
        return;
    }

    emitBytes(OP_DEFINE_GLOBAL, global);
}

static void namedVariable(const Token& name, bool canAssign)
{
    uint8_t getOp, setOp;
    int arg = current->resolveLocal(name);
    if (arg != -1)
    {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    }
    else if ((arg = current->resolveUpvalue(name)) != -1)
    {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    }
    else
    {
        arg = identifierConstant(name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && parser.match(TOKEN_EQUAL))
    {
        expression();
        emitBytes(setOp, (uint8_t)arg);
    }
    else
    {
        emitBytes(getOp, (uint8_t)arg);
    }
}

static uint8_t argumentList()
{
    uint8_t argCount = 0;
    if (!parser.check(TOKEN_RIGHT_PAREN))
    {
        do {
            expression();
            if (argCount == 255) error("Can't have more than 255 arguments.");

            argCount++;
        } while (parser.match(TOKEN_COMMA));
    }

    parser.consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

/* expressions */
static void dot(bool canAssign)
{
    parser.consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint8_t name = identifierConstant(parser.previous());

    if (canAssign && parser.match(TOKEN_EQUAL))
    {
        expression();
        emitBytes(OP_SET_PROPERTY, name);
    }
    else
    {
        emitBytes(OP_GET_PROPERTY, name);
    }
}

static void call(bool canAssign)
{
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

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

static void and_(bool canAssign)
{
    int endJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);
    parsePrecedence(PREC_AND);

    patchJump(endJump);
}

static void or_(bool canAssign)
{
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);

    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

/* statements */

static void varDeclaration();
static void printStatement();
static void expressionStatement();
static void block();
static void statement();
static void declaration();

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

static void ifStatement()
{
    parser.consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    parser.consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int thenJumpOffset = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // Clean up the condition value (as each statement is
                      // required to have zero stack effect).
    statement();

    int elseJumpOffset = emitJump(OP_JUMP);

    patchJump(thenJumpOffset);
    emitByte(OP_POP);

    if (parser.match(TOKEN_ELSE)) statement();
    patchJump(elseJumpOffset);
}

static void whileStatement()
{
    int loopStart = currentChunk()->count();

    parser.consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    parser.consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);
    statement();

    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);
}

static void forStatement()
{
    current->beginScope();

    parser.consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

    /* initializer */
    if (parser.match(TOKEN_SEMICOLON))
    {
        // No initializer.
    }
    else if (parser.match(TOKEN_VAR))
        varDeclaration();
    else
        expressionStatement();

    int loopStart = currentChunk()->count();

    /* condition */
    int exitJump = -1;
    if (!parser.match(TOKEN_SEMICOLON))
    {
        expression();
        parser.consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of the loop if the condition is false.
        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP); // Condition.
    }

    /* increment */
    if (!parser.match(TOKEN_RIGHT_PAREN))
    {
        int bodyJump = emitJump(OP_JUMP);

        int incrementStart = currentChunk()->count();
        expression();
        emitByte(OP_POP);
        parser.consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        emitLoop(loopStart);
        loopStart = incrementStart;
        patchJump(bodyJump);
    }

    statement();

    emitLoop(loopStart);

    if (exitJump != -1)
    {
        patchJump(exitJump);
        emitByte(OP_POP); // Condition.
    }

    current->endScope();
}

static void returnStatement()
{
    if (current->type_ == TYPE_SCRIPT)
    {
        error("Can't return from top-level code.");
    }

    if (parser.match(TOKEN_SEMICOLON))
        emitReturn();
    else
    {
        expression();
        parser.consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
}

static void statement()
{
    if (parser.match(TOKEN_PRINT))
        printStatement();
    else if (parser.match(TOKEN_FOR))
        forStatement();
    else if (parser.match(TOKEN_IF))
        ifStatement();
    else if (parser.match(TOKEN_RETURN))
        returnStatement();
    else if (parser.match(TOKEN_WHILE))
        whileStatement();
    else if (parser.match(TOKEN_PRINT))
    {
        current->beginScope();
        block();
        current->endScope();
    }
    else
        expressionStatement();
}

static void function(FunctionType type)
{
    Compiler compiler;
    compiler.init(type);
    compiler.beginScope();

    // Compile the parameter list.
    parser.consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!parser.check(TOKEN_RIGHT_PAREN))
    {
        do {
            current->function_->arity++;
            if (current->function_->arity > 255)
            {
                parser.errorAtCurrent("Can't have more than 255 parameters.");
            }

            uint8_t paramConstant = parseVariable("Expect parameter name.");
            defineVariable(paramConstant);
        } while (parser.match(TOKEN_COMMA));
    }
    parser.consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

    // The body.
    parser.consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    // Create the function object.
    ObjFunction* function = endCompiler();
    emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    for (int i = 0; i < function->upvalueCount; i++)
    {
        emitByte(compiler.upvalues_[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues_[i].index);
    }
}

static void funDeclaration()
{
    uint8_t global = parseVariable("Expect function name.");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
}

static void classDeclaration()
{
    parser.consume(TOKEN_IDENTIFIER, "Expect class name.");
    uint8_t nameConstant = identifierConstant(parser.previous());
    declareVariable();

    emitBytes(OP_CLASS, nameConstant);
    defineVariable(nameConstant);

    parser.consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    parser.consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
}

static void declaration()
{
    if (parser.match(TOKEN_CLASS))
        classDeclaration();
    else if (parser.match(TOKEN_FUN))
        funDeclaration();
    else if (parser.match(TOKEN_VAR))
        varDeclaration();
    else
        statement();

    if (parser.panicMode()) parser.synchronize();
}

static void block()
{
    while (!parser.check(TOKEN_RIGHT_BRACE) && !parser.check(TOKEN_EOF))
        declaration();

    parser.consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

ObjFunction* compile(const char* source)
{
    scanner.init(source);

    Compiler compiler;
    compiler.init(TYPE_SCRIPT);

    parser.clearError();

    parser.advance();

    while (!parser.match(TOKEN_EOF)) declaration();

    ObjFunction* function = endCompiler();
    return parser.hadError() ? NULL : function;
}

void markCompilerRoots()
{
    Compiler* compiler = current;
    while (compiler != NULL)
    {
        markObject((Obj*)compiler->function_);
        compiler = compiler->enclosing_;
    }
}

} // namespace lox
