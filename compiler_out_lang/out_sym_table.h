#ifndef OUT_SYM_TABLE_H_INCLUDED
#define OUT_SYM_TABLE_H_INCLUDED

#include "compiler_out_lang.h"
#include "program/var_table.h"

struct CmpoutSymAttr {
    bool relocatable:1;
    bool global:1;
    bool prototype:1;
};

struct CmpoutSymData {
    COMPILER_NATIVE_TYPE value;
    size_t section_id;
    CmpoutSymAttr attr;
};

INC_VAR_TABLE(CmpoutSym, cmpoutSym, CmpoutSymData)

#endif