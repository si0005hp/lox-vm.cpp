#include "vm.h"

#include <stdarg.h>
#include <string.h>
#include <time.h>

#include <cstdlib>

#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "value.h"

namespace lox
{

/* Native functions */
static Value clockNative(int argCount, Value *args)
{
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}
static Value getEnvNative(int argCount, Value *args)
{
    ObjString *envKey = AS_STRING(args[0]);

    char *envVal = std::getenv(envKey->chars);
    ObjString *result = takeString(envVal, strlen(envVal));

    return OBJ_VAL(result);
}
static Value sumNative(int argCount, Value *args)
{
    double a = AS_NUMBER(args[0]);
    double b = AS_NUMBER(args[1]);

    return NUMBER_VAL(a + b);
}
static Value helloworldNative(int argCount, Value *args)
{
    return OBJ_VAL(takeString((char *)"Hello world!", 13));
}

VM::VM()
  : frameCount_(0),
    stackTop_(stack_),
    objects_(NULL),
    openUpvalues_(NULL),
    initString_(NULL), // For GC, first need to NULL
    grayCount_(0),
    grayCapacity_(0),
    grayStack_(NULL),
    bytesAllocated_(0),
    nextGC_(1024 * 1024)
{
    initTable(&globals_);
    initTable(&strings_);
    initString_ = copyString("init", 4);

    defineNative("clock", clockNative);
    defineNative("getEnv", getEnvNative);
    defineNative("sum", sumNative);
    defineNative("helloworld", helloworldNative);
}

void VM::resetStack()
{
    stackTop_ = stack_;
    frameCount_ = 0;
    openUpvalues_ = NULL;
}

InterpretResult VM::interpret(const char *source)
{
    ObjFunction *function = compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));
    ObjClosure *closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    callValue(OBJ_VAL(closure), 0);

    return run();
}

void VM::free()
{
    freeTable(&globals_);
    freeTable(&strings_);
    initString_ = NULL;
    freeObjects();
}

InterpretResult VM::run()
{
    // storing the frame in a local variable encourages the C compiler to keep
    // that pointer in a register. (no guarantee , but there???s a good chance it
    // will.)
    CallFrame *frame = &frames_[frameCount_ - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() \
    (frame->closure->function->chunk.constants().elems()[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())

#define BINARY_OP(valueType, op)                        \
    do {                                                \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) \
        {                                               \
            runtimeError("Operands must be numbers.");  \
            return INTERPRET_RUNTIME_ERROR;             \
        }                                               \
        double b = AS_NUMBER(pop());                    \
        double a = AS_NUMBER(pop());                    \
        push(valueType(a op b));                        \
    } while (false)

#ifdef DEBUG_TRACE_EXECUTION
    std::cout << "********** TRACE EXECUTION **********" << std::endl;
#endif
    for (;;)
    {

#ifdef DEBUG_TRACE_EXECUTION
        std::cout << "          ";
        for (Value *slot = stack_; slot < stackTop_; slot++)
        {
            std::cout << "[ ";
            printValue(*slot);
            std::cout << " ]";
        }
        std::cout << std::endl;
        disassembleInstruction(
          &frame->closure->function->chunk,
          (int)(frame->ip - frame->closure->function->chunk.code()));
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE())
        {
            case OP_CONSTANT:
            {
                Value constant = READ_CONSTANT();
                push(constant);
                break;
            }
            case OP_NIL: push(NIL_VAL); break;
            case OP_TRUE: push(BOOL_VAL(true)); break;
            case OP_FALSE: push(BOOL_VAL(false)); break;
            case OP_EQUAL:
            {
                Value b = pop();
                Value a = pop();
                push(BOOL_VAL(valuesEqual(a, b)));
                break;
            }
            case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
            case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
            case OP_ADD:
                if (IS_STRING(peek(0)) && IS_STRING(peek(1)))
                    concatenate();
                else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1)))
                {
                    double b = AS_NUMBER(pop());
                    double a = AS_NUMBER(pop());
                    push(NUMBER_VAL(a + b));
                }
                else
                {
                    runtimeError(
                      "Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
            case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
            case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
            case OP_NOT: push(BOOL_VAL(isFalsey(pop()))); break;
            case OP_NEGATE:
                if (!IS_NUMBER(peek(0)))
                {
                    runtimeError("Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(-AS_NUMBER(pop())));
                break;

            case OP_PRINT:
                printValue(pop());
                std::cout << std::endl;
                break;
            case OP_POP: pop(); break;
            case OP_GET_GLOBAL:
            {
                ObjString *name = READ_STRING();
                Value value;
                if (!tableGet(&globals_, name, &value))
                {
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(value);
                break;
            }
            case OP_SET_GLOBAL:
            {
                ObjString *name = READ_STRING();
                if (tableSet(&globals_, name, peek(0)))
                {
                    tableDelete(&globals_, name); // [delete]
                    runtimeError("Undefined variable '%s'.", name->chars);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_GET_LOCAL:
            {
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
                break;
            }
            case OP_SET_LOCAL:
            {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
                break;
            }
            case OP_DEFINE_GLOBAL:
            {
                ObjString *name = READ_STRING();
                tableSet(&globals_, name, peek(0));
                pop();
                break;
            }
            case OP_JUMP_IF_FALSE:
            {
                uint16_t offset = READ_SHORT();
                if (isFalsey(peek(0))) frame->ip += offset;
                break;
            }
            case OP_JUMP:
            {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }
            case OP_LOOP:
            {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }
            case OP_CALL:
            {
                int argCount = READ_BYTE();
                if (!callValue(peek(argCount), argCount))
                    return INTERPRET_RUNTIME_ERROR;

                frame = &frames_[frameCount_ - 1];
                break;
            }

            case OP_CLOSURE:
            {
                ObjFunction *function = AS_FUNCTION(READ_CONSTANT());
                ObjClosure *closure = newClosure(function);
                push(OBJ_VAL(closure));

                for (int i = 0; i < closure->upvalueCount; i++)
                {
                    uint8_t isLocal = READ_BYTE();
                    uint8_t index = READ_BYTE();

                    closure->upvalues[i] =
                      isLocal ? captureUpvalue(frame->slots + index)
                              : frame->closure->upvalues[index];
                }
                break;
            }

            case OP_GET_UPVALUE:
            {
                uint8_t slot = READ_BYTE();
                push(*frame->closure->upvalues[slot]->location);
                break;
            }
            case OP_SET_UPVALUE:
            {
                uint8_t slot = READ_BYTE();
                *frame->closure->upvalues[slot]->location = peek(0);
                break;
            }

            case OP_CLOSE_UPVALUE:
                closeUpvalues(stackTop_ - 1);
                pop();
                break;

            case OP_CLASS: push(OBJ_VAL(newClass(READ_STRING()))); break;

            case OP_GET_PROPERTY:
            {
                if (!IS_INSTANCE(peek(0)))
                {
                    runtimeError("Only instances have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjInstance *instance = AS_INSTANCE(peek(0));
                ObjString *name = READ_STRING();

                Value value;
                if (tableGet(&instance->fields, name, &value))
                {
                    pop(); // Instance.
                    push(value);
                    break;
                }

                if (!bindMethod(instance->klass, name))
                    return INTERPRET_RUNTIME_ERROR;
                break;
            }
            case OP_SET_PROPERTY:
            {
                if (!IS_INSTANCE(peek(1)))
                {
                    runtimeError("Only instances have fields.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjInstance *instance = AS_INSTANCE(peek(1));
                tableSet(&instance->fields, READ_STRING(), peek(0));

                Value value = pop();
                pop();
                push(value);
                break;
            }
            case OP_METHOD: defineMethod(READ_STRING()); break;

            case OP_INVOKE:
            {
                ObjString *method = READ_STRING();
                int argCount = READ_BYTE();
                if (!invoke(method, argCount))
                {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames()[vm.frameCount_ - 1];
                break;
            }

            case OP_INHERIT:
            {
                Value superclass = peek(1);
                if (!IS_CLASS(superclass))
                {
                    runtimeError("Superclass must be a class.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                ObjClass *subclass = AS_CLASS(peek(0));
                tableAddAll(&AS_CLASS(superclass)->methods, &subclass->methods);
                pop(); // Subclass.
                break;
            }

            case OP_GET_SUPER:
            {
                ObjString *name = READ_STRING();
                ObjClass *superclass = AS_CLASS(pop());
                if (!bindMethod(superclass, name))
                {
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            case OP_SUPER_INVOKE:
            {
                ObjString *method = READ_STRING();
                int argCount = READ_BYTE();
                ObjClass *superclass = AS_CLASS(pop());
                if (!invokeFromClass(superclass, method, argCount))
                {
                    return INTERPRET_RUNTIME_ERROR;
                }
                frame = &vm.frames()[vm.frameCount_ - 1];
                break;
            }

            case OP_RETURN:
            {
                Value result = pop();

                closeUpvalues(frame->slots);

                frameCount_--;
                if (frameCount_ == 0)
                {
                    // Finished executing the top-level code.
                    // Pop the main script function from the stack and exit the
                    // interpreter.
                    pop();
                    return INTERPRET_OK;
                }

                stackTop_ = frame->slots;
                push(result);

                frame = &frames_[frameCount_ - 1];
                break;
            }
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
#undef READ_STRING
}

bool VM::invokeFromClass(ObjClass *klass, ObjString *name, int argCount)
{
    Value method;
    if (!tableGet(&klass->methods, name, &method))
    {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }
    return call(AS_CLOSURE(method), argCount);
}

bool VM::invoke(ObjString *name, int argCount)
{
    Value receiver = peek(argCount);
    if (!IS_INSTANCE(receiver))
    {
        runtimeError("Only instances have methods.");
        return false;
    }

    ObjInstance *instance = AS_INSTANCE(receiver);

    Value value;
    if (tableGet(&instance->fields, name, &value))
    {
        vm.stackTop_[-argCount - 1] = value;
        return callValue(value, argCount);
    }

    return invokeFromClass(instance->klass, name, argCount);
}

bool VM::bindMethod(ObjClass *klass, ObjString *name)
{
    Value method;
    if (!tableGet(&klass->methods, name, &method))
    {
        runtimeError("Undefined property '%s'.", name->chars);
        return false;
    }

    ObjBoundMethod *bound = newBoundMethod(peek(0), AS_CLOSURE(method));
    pop();
    push(OBJ_VAL(bound));
    return true;
}

void VM::defineMethod(ObjString *name)
{
    Value method = peek(0);
    ObjClass *klass = AS_CLASS(peek(1));
    tableSet(&klass->methods, name, method);
    pop();
}

void VM::closeUpvalues(Value *last)
{
    while (openUpvalues_ != NULL && openUpvalues_->location >= last)
    {
        ObjUpvalue *upvalue = openUpvalues_;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        openUpvalues_ = upvalue->next;
    }
}

ObjUpvalue *VM::captureUpvalue(Value *local)
{
    ObjUpvalue *prevUpvalue = NULL;
    ObjUpvalue *upvalue = vm.openUpvalues_;

    while (upvalue != NULL && upvalue->location > local)
    {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }
    if (upvalue != NULL && upvalue->location == local) return upvalue;

    ObjUpvalue *createdUpvalue = newUpvalue(local);
    createdUpvalue->next = upvalue;

    if (prevUpvalue == NULL)
        openUpvalues_ = createdUpvalue;
    else
        prevUpvalue->next = createdUpvalue;

    return createdUpvalue;
}

bool VM::isFalsey(Value value) const
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

void VM::push(Value value)
{
    *stackTop_ = value;
    stackTop_++;
}

Value VM::pop()
{
    stackTop_--;
    return *stackTop_;
}

Value VM::peek(int distance) const
{
    return stackTop_[-1 - distance];
}

bool VM::call(ObjClosure *closure, int argCount)
{
    if (argCount != closure->function->arity)
    {
        runtimeError("Expected %d arguments but got %d.",
                     closure->function->arity, argCount);
        return false;
    }
    if (frameCount_ == FRAMES_MAX)
    {
        runtimeError("Stack overflow.");
        return false;
    }

    CallFrame *frame = &frames_[frameCount_++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code();

    frame->slots = stackTop_ - argCount - 1;
    return true;
}

bool VM::callValue(Value callee, int argCount)
{
    if (IS_OBJ(callee))
    {
        switch (OBJ_TYPE(callee))
        {
            case OBJ_CLOSURE: return call(AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE:
            {
                NativeFn native = AS_NATIVE(callee);
                Value result = native(argCount, stackTop_ - argCount);
                stackTop_ -= argCount + 1;
                push(result);
                return true;
            }
            case OBJ_CLASS:
            {
                ObjClass *klass = AS_CLASS(callee);
                vm.stackTop_[-argCount - 1] = OBJ_VAL(newInstance(klass));
                Value initializer;
                if (tableGet(&klass->methods, vm.initString_, &initializer))
                {
                    return call(AS_CLOSURE(initializer), argCount);
                }
                else if (argCount != 0)
                {
                    runtimeError("Expected 0 arguments but got %d.", argCount);
                    return false;
                }
                return true;
            }
            case OBJ_BOUND_METHOD:
            {
                ObjBoundMethod *bound = AS_BOUND_METHOD(callee);
                vm.stackTop_[-argCount - 1] = bound->receiver;
                return call(bound->method, argCount);
            }
            default: break; // Non-callable object type.
        }
    }

    runtimeError("Can only call functions and classes.");
    return false;
}

void VM::concatenate()
{
    // ObjString *b = AS_STRING(pop());
    // ObjString *a = AS_STRING(pop());
    ObjString *b = AS_STRING(peek(0)); // For GC
    ObjString *a = AS_STRING(peek(1));

    int length = a->length + b->length;
    char *chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString *result = takeString(chars, length);
    pop();
    pop();

    push(OBJ_VAL(result));
}

void VM::runtimeError(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    CallFrame *frame = &frames_[frameCount_ - 1];
    size_t instruction = frame->ip - frame->closure->function->chunk.code() - 1;
    int line = frame->closure->function->chunk.lines()[instruction];
    fprintf(stderr, "[line %d] in script\n", line);

    for (int i = frameCount_ - 1; i >= 0; i--)
    {
        CallFrame *frame = &frames_[i];
        ObjFunction *function = frame->closure->function;
        // -1 because the IP is sitting on the next instruction to be
        // executed.
        size_t instruction = frame->ip - function->chunk.code() - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines()[instruction]);
        if (function->name == NULL)
            fprintf(stderr, "script\n");
        else
            fprintf(stderr, "%s()\n", function->name->chars);
    }

    resetStack();
}

void VM::defineNative(const char *name, NativeFn function)
{
    // Push name and ObjFunction to ensure the GC collector knows we???re not done
    // with these so that it doesn???t free them out.
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&globals_, AS_STRING(stack_[0]), stack_[1]);
    pop();
    pop();
}

} // namespace lox
