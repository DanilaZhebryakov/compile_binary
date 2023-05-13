#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "lib/logging.h"
#include "compiler_out_lang/exec_output.h"

bool execOutCtor(ExecOutput* out){
    if (!relocationTableCtor(&(out->relt)))
        return false;
    cmpoutSymTableCtor(&(out->syms));
    out->sect_count = 0;
    out->curr_sect  = -1;
    out->lvl        = 0;
    out->sects = nullptr;
    return true;
}

void execOutDtor(ExecOutput* out){
    relocationTableDtor(&(out->relt));
    cmpoutSymTableDtor(&(out->syms));
    if (out->sect_count > 0){
        for (ExecOutSection* sect = out->sects; sect < out->sects + out->sect_count; sect++){
            if (sect->data != nullptr)
                free(sect->data);
        }
        free(out->sects);
    }
    #ifndef NDEBUG
        out->sect_count = 0;
        out->sects = nullptr;
    #endif
}

ExecOutSection* execOutFindSect(ExecOutput* out, const char* name) {
    for (ExecOutSection* sect = out->sects + out->sect_count; sect > out->sects; sect--){
        if (strcmp(sect->name, name) == 0) {
            return sect;
        }
    }
    return nullptr;
}

bool execOutWriteSection(ExecOutSection* sect, size_t dt_size, const void* dt_val, size_t val_size){
    if (val_size > dt_size){
        error_log("Error: write more bytes than allocate (%ld vs %ld) (sect %s)\n", val_size, dt_size, sect->name);
        return false;
    }
    if (sect->attr.fill_type != BIN_SECTION_INITIALISED) {
        if (val_size > 0) {
            warn_log("Warning: data write to uninitialised section (%s). Ignoring.", sect->name);
        }
        sect->size += dt_size;
        return true;
    }
    if (sect->size + dt_size > sect->capacity) {
        void* new_data = realloc(sect->data, (sect->capacity + dt_size)*2);
        if (!new_data)
            return false;
        sect->data = new_data;
        sect->capacity = (sect->capacity + dt_size)*2;
    }
    memcpy(((char*)sect->data) + sect->size           , dt_val, val_size);
    memset(((char*)sect->data) + sect->size + val_size, 0, dt_size - val_size);
    sect->size += dt_size;
    return true;
}

bool execoutAddSection(ExecOutput* out, const char* name, BinSectionAttr attr, size_t size ) {
    if (out->sect_count == 0){
        assert(out->sects == nullptr);
        out->sects = (ExecOutSection*)calloc(1, sizeof(ExecOutSection));
        if (!out->sects)
            return false;
        out->sect_count = 1;
    }
    else {
        void* new_sects = realloc(out->sects, (out->sect_count + 1)*sizeof(*(out->sects)));
        if (!new_sects)
            return false;
        out->sects = (ExecOutSection*)new_sects;
        out->sect_count++;
    }
    if (attr.fill_type == BIN_SECTION_INITIALISED) {
        out->sects[out->sect_count].data = calloc(init_sect_size, sizeof(char));
        if (!(out->sects[out->sect_count].data)) {
            return false;
        }
        out->sects[out->sect_count].capacity = init_sect_size;
    }
    else {
        out->sects[out->sect_count].capacity = (size_t)-1;
    }
    out->sects[out->sect_count].size = 0;
    out->sects[out->sect_count].attr = attr;
    out->sects[out->sect_count].name = name;
    return true;
}

bool execOutHandleTableInstr(ExecOutput* out, const void* instr_ptr) {
    CompilerInstrHeader* header = ((CompilerInstrHeader*)instr_ptr);
    if (header->type != COUT_TYPE_TABLE)
        return false;
    instr_ptr = ((char*)instr_ptr) + sizeof(CompilerInstrHeader);

    compilerTableInstr_t instr = *((compilerTableInstr_t*)instr_ptr);
    instr_ptr = ((char*)instr_ptr) + sizeof(compilerTableInstr_t);
    const char* name_ptr = (char*)instr_ptr;
    instr_ptr = ((char*)instr_ptr) + strlen(name_ptr) + 1;

    if (instr == COUT_TABLE_SECT) {
        ExecOutSection* sect = execOutFindSect(out, (char*)instr_ptr);
        if (!sect)
            return false;
        instr_ptr = ((char*)instr_ptr) + strlen((char*)instr_ptr) + 1;
        size_t dt_size = *((size_t*)instr_ptr);
        instr_ptr = ((char*)instr_ptr) + sizeof(dt_size);
        return cmpoutSymTablePut(&(out->syms), {{sect->size, (size_t)(sect - out->sects), {1, 0}}, name_ptr, out->lvl, 0})
            && execOutWriteSection(sect, dt_size, instr_ptr, header->size - (((char*)instr_ptr) - ((char*)header)));
    }


    switch (instr){
        case COUT_TABLE_ADDSECT:
            return execoutAddSection(out, name_ptr, *((BinSectionAttr*)(instr_ptr)));
        case COUT_TABLE_SIMPLE:
            return cmpoutSymTablePut(&(out->syms), {{*((COMPILER_NATIVE_TYPE*)instr_ptr), (size_t)-1, 0}, name_ptr, out->lvl, 0});
        case COUT_TABLE_HERE:
            return cmpoutSymTablePut(&(out->syms), {{(out->sects + out->curr_sect)->size, out->curr_sect, {1,0}}, name_ptr, out->lvl, 0});
        case COUT_TABLE_PRE:
            return execOutHandleTableInstr(out, ((const char*)instr_ptr) + *((size_t*)instr_ptr));
        default:
            Error_log("Unknown table instr %d\n", instr);
            return false;
    }
    assert(0);
}

bool execOutHandleStructInstr(ExecOutput* out, const void* instr_ptr){
    CompilerInstrHeader* header = ((CompilerInstrHeader*)instr_ptr);
    if (header->type != COUT_TYPE_STRUCT)
        return false;
    instr_ptr = ((char*)instr_ptr) + sizeof(CompilerInstrHeader);

    compilerStructureInstr_t instr = *((compilerStructureInstr_t*)instr_ptr);
    instr_ptr = ((char*)instr_ptr) + sizeof(instr);

    switch (instr) {
        case COUT_STRUCT_ADDSF:
            return true;
        case COUT_STRUCT_RMSF:
            return true;
        case COUT_STRUCT_NCB_B:
            out->lvl++;
            return true;
        case COUT_STRUCT_RCB_B:
            out->lvl++;
            return true;
        case COUT_STRUCT_CB_E:
            cmpoutSymTableDescendLvl(&(out->syms), out->lvl);
            out->lvl--;
            return true;
        case COUT_STRUCT_SECT_B:
            out->curr_sect = execOutFindSect(out, ((const char*)instr_ptr))-(out->sects);
            return out->curr_sect != (size_t)(((ExecOutSection*)nullptr) - (out->sects));
        case COUT_STRUCT_SECT_E:
            return true;
        default:
            error_log("Undefined structural instruction %d\n", instr);
            return false;
    }
    assert(0);
}

bool execOutPutSym(ExecOutput* out, const char* name, int size) {
    if (out->curr_sect == (size_t)-1){
        error_log("Attempt to write data to no section\n");
        return 0;
    }

    CmpoutSymEntry* sym = cmpoutSymTableGet(&(out->syms), name);
    if (sym->value.attr.relocatable) {
        RelocationEntry entry = {out->curr_sect, (out->sects + out->curr_sect)->size, sym->value.section_id, size};
        if (!relocationTableAdd(&(out->relt), &entry)) {
            return false;
        }
    }
    return (execOutWriteSection(out->sects + out->curr_sect, size, &(sym->value.value), size));
}

bool execOutPutData(ExecOutput* out, const void* data, size_t size){
    if (out->curr_sect == (size_t)-1){
        error_log("Attempt to write data to no section\n");
        return 0;
    }
    return execOutWriteSection(out->sects + out->curr_sect, size, data, size);
}

bool execOutPrepareCode(ExecOutput* out, const void* prefix, size_t prefsize, const void* suffix, size_t sufsize) {
    size_t all_size = prefsize;
    BinSectionAttr common_attr = {BIN_SECTION_INITIALISED,1,0,0,1};
    printf("%d\n", out->sect_count);
    size_t* sect_reloc_offs = (size_t*)calloc(out->sect_count, sizeof(*sect_reloc_offs));
    ExecOutSection* replaced_sect = nullptr;

    for (ExecOutSection* sect = out->sects; sect < out->sects + out->sect_count; sect++){
        if (sect->attr.entry_point) {
            common_attr.readable |= sect->attr.readable;
            common_attr.writable |= sect->attr.writable;
            sect_reloc_offs[sect-out->sects] = all_size;
            all_size += sect->size;
            replaced_sect = sect;
        }
    }

    all_size += sufsize;
    
    ExecOutSection new_sect;
    new_sect.data = calloc(all_size, sizeof(char));
    if (!new_sect.data)
        return false;
    new_sect.name = "_ALL_CODE_";
    new_sect.capacity = all_size;
    new_sect.attr = common_attr;

    memcpy(new_sect.data, prefix, prefsize);
    new_sect.size = prefsize;

    for (ExecOutSection* sect = out->sects; sect < out->sects + out->sect_count-1; sect++){
        if (sect->attr.entry_point) {
            memcpy(((char*)new_sect.data) + new_sect.size, sect->data, sect->size);
            new_sect.size += sect->size;
            sect->size = 0;
            sect->capacity = 0;
            free(sect->data);
            sect->data = nullptr;
        }
    }
    size_t repl_sect_id = replaced_sect - out->sects;
    memcpy(((char*)new_sect.data) + new_sect.size, suffix, sufsize);

    for (RelocationEntry* rel = out->relt.data + out->relt.size; rel >= out->relt.data; rel--){
        if (out->sects[rel->sect_id].attr.entry_point){
            rel->sect_id = repl_sect_id;
            rel->sect_offset += sect_reloc_offs[rel->sect_id];
            out->sects[rel->sect_id].attr.entry_point = 0;
        }
        if (out->sects[rel->rel_to_sect].attr.entry_point){
            assert(rel->size > 8);
            size_t* rel_ptr = (size_t*)(((char*)out->sects[rel->sect_id].data) + rel->sect_offset);
            *rel_ptr = ((*rel_ptr + sect_reloc_offs[rel->rel_to_sect]) & (-1L >> (8*(8-rel->size))))
                    | ((*rel_ptr) & (-1L >> rel->size));
        }
    }
    memcpy(replaced_sect, &new_sect, sizeof(new_sect));
    return true;
}