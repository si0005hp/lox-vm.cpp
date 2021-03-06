#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>

#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

using namespace lox;

static void repl()
{
    char line[1024];
    for (;;)
    {
        std::cout << "> ";
        if (!fgets(line, sizeof(line), stdin))
        {
            std::cout << std::endl;
            break;
        }
        vm.interpret(line);
    }
}

static char *readFile(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
    {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    char *buffer = (char *)malloc(fileSize + 1);
    if (buffer == NULL)
    {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize)
    {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }

    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

static void runFile(const char *path)
{
    char *source = readFile(path);
    InterpretResult result = vm.interpret(source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, char const *argv[])
{
    Chunk chunk;

    int constant = chunk.addConstant(1.2);
    chunk.write(OP_CONSTANT, 123);
    chunk.write(constant, 123);

    constant = chunk.addConstant(3.4);
    chunk.write(OP_CONSTANT, 123);
    chunk.write(constant, 123);

    chunk.write(OP_ADD, 123);

    constant = chunk.addConstant(5.6);
    chunk.write(OP_CONSTANT, 123);
    chunk.write(constant, 123);

    chunk.write(OP_DIVIDE, 123);

    chunk.write(OP_NEGATE, 123);
    chunk.write(OP_RETURN, 123);

    disassembleChunk(&chunk, "test chunk");

    vm.interpret(&chunk);
    vm.free();
    chunk.free();
    return 0;
}
