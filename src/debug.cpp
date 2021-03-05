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
        case OP_RETURN:
            return simpleInstruction("OP_RETURN", offset);
        default:
            std::cout << "Unknown opcode " << instruction << std::endl;
            return offset + 1;
    }
}

} // namespace lox
