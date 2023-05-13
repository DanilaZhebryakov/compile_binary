#include <stdlib.h>
#include <string.h>

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