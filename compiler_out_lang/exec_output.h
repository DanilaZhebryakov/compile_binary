#ifndef EXEC_OUTPUT_H_INCLUDED
#define EXEC_OUTPUT_H_INCLUDED

#include <stddef.h>

#include "relocation_table.h"
#include "lib/UStack.h"
#include "compiler_out_lang/compiler_out_lang.h"
#include "compiler_out_lang/out_sym_table.h"

const size_t init_sect_size = 100;

struct ExecOutContext {
    size_t curr_sect;
    size_t stk_size;
};

struct ExecOutSection {
    const char* name;
    BinSectionAttr attr;
    size_t size;
    size_t capacity;
    void* data;
};

struct ExecOutput{
    RelocationTable relt;
    CmpoutSymTable  syms;

    ExecOutSection* sects;
    
    size_t sect_count;
    size_t curr_sect;
    size_t lvl;
};

bool execOutCtor(ExecOutput* out);

void execOutDtor(ExecOutput* out);

ExecOutSection* execOutFindSect(ExecOutput* out, const char* name);

bool execOutWriteSection(ExecOutSection* sect, size_t dt_size, const void* dt_val, size_t val_size);

bool execoutAddSection(ExecOutput* out, const char* name, BinSectionAttr attr, size_t size = init_sect_size);

bool execOutHandleTableInstr(ExecOutput* out, const void* instr_ptr);

bool execOutHandleStructInstr(ExecOutput* out, const void* instr_ptr);

bool execOutPutSym(ExecOutput* out, const char* name, int size);

bool execOutPutData(ExecOutput* out, const void* data, size_t size);

bool execOutPrepareCode(ExecOutput* out, const void* prefix, size_t prefsize, const void* suffix, size_t sufsize);

#endif