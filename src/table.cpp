#include "table.h"

#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

namespace lox
{

void Table::init()
{
    count = 0;
    capacity = 0;
    entries = NULL;
}

void Table::free()
{
    FREE_ARRAY(Entry, entries, capacity);
    init();
}

bool Table::set(ObjString* key, Value value)
{
    if (count + 1 > capacity * TABLE_MAX_LOAD)
    {
        int _capacity = GROW_CAPACITY(capacity);
        adjustCapacity(_capacity);
    }

    Entry* entry = findEntry(entries, capacity, key);

    bool isNewKey = entry->key == NULL;
    if (isNewKey && IS_NIL(entry->value)) count++;

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool Table::get(ObjString* key, Value* value)
{
    if (count == 0) return false;

    Entry* entry = findEntry(entries, capacity, key);
    if (entry->key == NULL) return false;

    *value = entry->value;
    return true;
}

bool Table::delete_(Table* table, ObjString* key)
{
    if (count == 0) return false;

    Entry* entry = findEntry(entries, capacity, key);
    if (entry->key == NULL) return false;

    // Place a tombstone in the entry.
    entry->key = NULL;
    entry->value = BOOL_VAL(true);

    return true;
}

Entry* Table::findEntry(Entry* entries, int capacity, ObjString* key)
{
    uint32_t index = key->hash % capacity;
    Entry* tombstone = NULL;

    for (;;)
    {
        Entry* entry = &entries[index];

        if (entry->key == NULL)
        {
            if (IS_NIL(entry->value))
                // Empty entry.
                return tombstone != NULL ? tombstone : entry;
            else
              // Found a tombstone.
              if (tombstone == NULL)
                tombstone = entry;
        }
        else if (entry->key == key)
        {
            return entry; // Found the key.
        }

        index = (index + 1) % capacity;
    }
}

void Table::adjustCapacity(int _capacity)
{
    Entry* _entries = ALLOCATE(Entry, _capacity);
    for (int i = 0; i < _capacity; i++)
    {
        _entries[i].key = NULL;
        _entries[i].value = NIL_VAL;
    }

    count = 0;
    for (int i = 0; i < capacity; i++)
    {
        Entry* entry = &_entries[i];
        if (entry->key == NULL) continue;

        Entry* dest = findEntry(_entries, _capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        count++;
    }

    FREE_ARRAY(Entry, entries, capacity);
    entries = _entries;
    capacity = _capacity;
}

void Table::addAllTo(Table* to)
{
    for (int i = 0; i < capacity; i++)
    {
        Entry* entry = &entries[i];
        if (entry->key != NULL) to->set(entry->key, entry->value);
    }
}

ObjString* Table::findString(const char* chars, int length, uint32_t hash)
{
    if (count == 0) return NULL;

    uint32_t index = hash % capacity;

    for (;;)
    {
        Entry* entry = &entries[index];

        if (entry->key == NULL)
        {
            // Stop if we find an empty non-tombstone entry.
            if (IS_NIL(entry->value)) return NULL;
        }
        else if (entry->key->length == length && entry->key->hash == hash &&
                 memcmp(entry->key->chars, chars, length) == 0)
        {
            // Found it.
            return entry->key;
        }

        index = (index + 1) % capacity;
    }
}

} // namespace lox
