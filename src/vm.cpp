#include "vm.h"

#include "compiler.h"
#include "debug.h"
#include "value.h"

namespace lox
{

InterpretResult VM::interpret(Chunk *chunk)
{
    chunk_ = chunk;
    ip_ = vm.chunk_->code();
    return run();
}

InterpretResult VM::interpret(const char *source)
{
    Chunk chunk;

    if (!compile(source, &chunk))
    {
        chunk.free();
        return INTERPRET_COMPILE_ERROR;
    }

    chunk_ = &chunk;
    ip_ = chunk_->code();

    InterpretResult result = run();

    chunk.free();
    return result;
}

void VM::free() {}

InterpretResult VM::run()
{
#define READ_BYTE() (*ip_++)
#define READ_CONSTANT() (chunk_->constants().elems()[READ_BYTE()])

#define BINARY_OP(op)     \
    do {                  \
        double b = pop(); \
        double a = pop(); \
        push(a op b);     \
    } while (false)

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
        disassembleInstruction(chunk_, (int)(ip_ - chunk_->code()));
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
            case OP_ADD: BINARY_OP(+); break;
            case OP_SUBTRACT: BINARY_OP(-); break;
            case OP_MULTIPLY: BINARY_OP(*); break;
            case OP_DIVIDE: BINARY_OP(/); break;
            case OP_NEGATE: push(-pop()); break;
            case OP_RETURN:
            {
                printValue(pop());
                std::cout << std::endl;
                return INTERPRET_OK;
            }
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
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

} // namespace lox
