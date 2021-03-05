#include <iostream>

#include "chunk.h"
#include "common.h"
#include "debug.h"

using namespace lox;

int main(int argc, char const *argv[])
{
    Chunk chunk;

    int constant = chunk.addConstant(1.2);
    chunk.write(OP_CONSTANT, 123);
    chunk.write(constant, 123);

    chunk.write(OP_RETURN, 123);

    disassembleChunk(&chunk, "test chunk");
    chunk.free();
    return 0;
}
