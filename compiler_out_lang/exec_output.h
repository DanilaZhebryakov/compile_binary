#ifndef EXEC_OUTPUT_H_INCLUDED
#define EXEC_OUTPUT_H_INCLUDED

#include <stddef.h>

#include "relocation_table.h"
#include "lib/UStack.h"
#include "compiler_out_lang/compiler_out_lang.h"
#include "compiler_out_lang/out_sym_table.h"

const size_t init_sect_size = 100;

enum execOutContextType_t {
    EOUT_CONTEXT_ROOT   = 0,
    EOUT_CONTEXT_SECT   = 1,
    EOUT_CONTEXT_CB     = 2,
    EOUT_CONTEXT_NONLIN = 4,
    EOUT_CONTEXT_SF     = 5,

    EOUT_CONTEXT_INVALID = 0xFF
};

struct ExecOutContext {
    execOutContextType_t type;
    size_t sect;
    size_t stk_size_beg;
    size_t stk_size_curr;
};

struct ExecOutSection {
    const char* name;
    BinSectionAttr attr;
    bool in_use;
    size_t size;
    size_t capacity;
    void* data;
};

struct ExecOutput{
    RelocationTable relt;
    RelocationTable sym_pre_tbl;
    UStack          sym_pre_data;

    CmpoutSymTable  syms;
    UStack context_stk;

    ExecOutSection* sects;
    
    ExecOutContext* curr_context;
    size_t sect_count;
    size_t lvl;
};

bool execOutPushContext(ExecOutput* out, ExecOutContext* context);

bool execOutPopContext(ExecOutput* out);

bool execOutCtor(ExecOutput* out);

void execOutDtor(ExecOutput* out);

size_t execOutFindSect(ExecOutput* out, const char* name);

bool execOutWriteSection(ExecOutSection* sect, size_t dt_size, const void* dt_val, size_t val_size);

bool execoutAddSection(ExecOutput* out, const char* name, BinSectionAttr attr, size_t size = init_sect_size);

bool execOutHandleTableInstr(ExecOutput* out, const void* instr_ptr);

bool execOutHandleStructInstr(ExecOutput* out, const void* instr_ptr);

bool execOutPutSym(ExecOutput* out, const char* name, int size, bool relative = false);

bool execOutPutOffsSym(ExecOutput* out, const char* name, void* offset_ptr, int size, bool relative = false);

bool execOutPutData(ExecOutput* out, const void* data, size_t size);

bool execOutPrepareCode(ExecOutput* out, const void* prefix, size_t prefsize, const void* suffix, size_t sufsize);

bool execOutApplyPreLbls(ExecOutput* out);

#endif