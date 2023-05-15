#ifndef RELOCATION_TABLE_H_INCLUDED
#define RELOCATION_TABLE_H_INCLUDED

#include <stddef.h>

const int min_reltable_cap = 100;

struct  RelocationEntry {
    size_t sect_id;
    size_t sect_offset;
    size_t relative_to;
    int size;
};

struct RelocationTable {
    size_t size;
    size_t capacity;
    RelocationEntry* data;
};

bool relocationTableCtor(RelocationTable* table);

void relocationTableDtor(RelocationTable* table);

bool relocationTableAdd(RelocationTable* table, RelocationEntry* value);

bool relocationEntryApply(RelocationEntry* entry, void* sect_start, long long reloc_offset);

#endif
