#include <stdlib.h>
#include <string.h>

#include "lib/memmath.h"
#include "lib/logging.h"

#include "relocation_table.h"

bool relocationTableCtor(RelocationTable* table) {
    table->data = (RelocationEntry*)calloc(min_reltable_cap, sizeof(*(table->data)));
    if (!(table->data)) 
        return false;

    table->size = 0;
    table->capacity = min_reltable_cap;
    return true;
}

void relocationTableDtor(RelocationTable* table) {
    free(table->data);
    #ifndef NDEBUG
        table->capacity = -1;
        table->size = 0;
        table->data = nullptr;
    #endif
}

bool relocationTableAdd(RelocationTable* table, RelocationEntry* value){
    if (table->size+1 > table->capacity) {
        size_t new_cap = (table->capacity + 1)*2;
        void* new_data = realloc(table->data, new_cap * sizeof(*(table->data)));
        if (!new_data)
            return false;
        table->data = (RelocationEntry*)new_data;
    }

    memcpy(table->data + table->size, value, sizeof(*value));
    table->size++;
    return true;
}

bool relocationEntryApply(RelocationEntry* entry, void* sect_start, long long reloc_offset){
    if (entry->sub){
        reloc_offset = -reloc_offset;
    }
    if ((unsigned int)entry->size > sizeof(reloc_offset)) {
        error_log("Relocation failed: too large relocation size (%d).\n", entry->size);
    }

    long long t = reloc_offset >> ((sizeof(long long) - entry->size)*8 -1);
    if ((t!=0) && (t!=-1)) { // check that reloc_offset actually does fit in entry->size bytes 
        error_log("Relocation failed: overflow detected\n");
        return false;
    }

    void* rel_ptr = (((char*)sect_start) + entry->sect_offset);

    if (memadd(rel_ptr, &reloc_offset, entry->size)){
        error_log("Relocation failed: overflow detected\n");
        return false;
    }
    return true;
}