#include "debug.h"

#include <iomanip>
#include <iostream>

#include "value.h"

namespace lox
{

static int simpleInstruction(const char* name, int offset)
{
    std::cout << name << std::endl;
    return offset + 1;
}

static int constantInstruction(const char* name, Chunk* chunk, int offset)
{
    uint8_t constant = chunk->code()[offset + 1];
    std::cout << std::setw(16) << std::left << name << " ";
    std::cout << std::setw(4) << std::right << std::to_string(constant) << " '";
    printValue(chunk->constants().elems()[constant]);
    std::cout << "'" << std::endl;
    return offset + 2;
}

static int byteInstruction(const char* name, Chunk* chunk, int offset)
{
    uint8_t slot = chunk->code()[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

void disassembleChunk(Chunk* chunk, const char* name)
{
    std::cout << "== " << name << " ==" << std::endl;

    for (int offset = 0; offset < chunk->count();)
        offset = disassembleInstruction(chunk, offset);
}

int disassembleInstruction(Chunk* chunk, int offset)
{
    std::cout << std::setw(4) << std::setfill('0') << offset
              << std::setfill(' ') << " ";

    if (offset > 0 && chunk->lines()[offset] == chunk->lines()[offset - 1])
        std::cout << "   | ";
    else
        std::cout << std::setw(4) << chunk->lines()[offset] << " ";

    uint8_t instruction = chunk->code()[offset];
    switch (instruction)
    {
        case OP_CONSTANT:
            return constantInstruction("OP_CONSTANT", chunk, offset);
        case OP_NIL: return simpleInstruction("OP_NIL", offset);
        case OP_TRUE: return simpleInstruction("OP_TRUE", offset);
        case OP_FALSE: return simpleInstruction("OP_FALSE", offset);
        case OP_EQUAL: return simpleInstruction("OP_EQUAL", offset);
        case OP_GREATER: return simpleInstruction("OP_GREATER", offset);
        case OP_LESS: return simpleInstruction("OP_LESS", offset);
        case OP_ADD: return simpleInstruction("OP_ADD", offset);
        case OP_SUBTRACT: return simpleInstruction("OP_SUBTRACT", offset);
        case OP_MULTIPLY: return simpleInstruction("OP_MULTIPLY", offset);
        case OP_DIVIDE: return simpleInstruction("OP_DIVIDE", offset);
        case OP_NOT: return simpleInstruction("OP_NOT", offset);
        case OP_NEGATE: return simpleInstruction("OP_NEGATE", offset);
        case OP_PRINT: return simpleInstruction("OP_PRINT", offset);
        case OP_POP: return simpleInstruction("OP_POP", offset);
        case OP_GET_GLOBAL:
            return constantInstruction("OP_GET_GLOBAL", chunk, offset);
        case OP_DEFINE_GLOBAL:
            return constantInstruction("OP_DEFINE_GLOBAL", chunk, offset);
        case OP_SET_GLOBAL:
            return constantInstruction("OP_SET_GLOBAL", chunk, offset);
        case OP_RETURN: return simpleInstruction("OP_RETURN", offset);
        case OP_GET_LOCAL:
            return byteInstruction("OP_GET_LOCAL", chunk, offset);
        case OP_SET_LOCAL:
            return byteInstruction("OP_SET_LOCAL", chunk, offset);
        default:
            std::cout << "Unknown opcode " << instruction << std::endl;
            return offset + 1;
    }
}

} // namespace lox
