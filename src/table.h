#pragma once

#include "common.h"
#include "value.h"

namespace lox
{

typedef struct
{
    ObjString* key;
    Value value;
} Entry;

struct Table
{
    Table() : count(0), capacity(0), entries(NULL) {}
    void init();
    void free();
    bool set(ObjString* key, Value value);
    bool get(ObjString* key, Value* value);
    bool delete_(Table* table, ObjString* key);

    Entry* findEntry(Entry* entries, int capacity, ObjString* key);
    void adjustCapacity(int capacity);
    void addAllTo(Table* to);

    ObjString* findString(const char* chars, int length, uint32_t hash);

    int count;
    int capacity;
    Entry* entries;
};

} // namespace lox
