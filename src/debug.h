#pragma once

#include "chunk.h"

namespace lox
{

void disassembleChunk(Chunk* chunk, const char* name);
int disassembleInstruction(Chunk* chunk, int offset);

} // namespace lox
