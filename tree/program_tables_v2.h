#ifndef PROGRAM_TABLES_V2_H_INCLUDED
#define PROGRAM_TABLES_V2_H_INCLUDED

#include "var_table.h"
#include "lib/bintree.h"
#include "program_structure.h"

enum tableType_t {
    TABLE_T_NULL = 0,
    TABLE_T_VAR  = 1,
    TABLE_T_FUNC = 2
};

enum valueAccessType_t{
    VAL_ACC_VOLATILE = 0, // each read/write/call matters. Default for functions
    VAL_ACC_SOLID    = 1, // two sequential reads/calls are the same. Default for Vfunc
    VAL_ACC_LAZY     = 2, // give same args -> get same result. Only write changes read. Only last write matters. Default for variables.
    VAL_ACC_CONST    = 3  // always the same. Writes do error. Default for nothing.
};

enum FuncValCnt_t{
    VF_VAL_NONE   = 0,
    VF_VAL_SINGLE = 1,
    VF_VAL_MULTI  = 2,
    VF_VAL_COUNT  = 3
};

struct FuncAttr{
    FuncValCnt_t arg_cnt:2;
    FuncValCnt_t ret_cnt:2;
    valueAccessType_t acc_type:2;
    bool varfunc:1;
};

struct VarAttr{
    valueAccessType_t acc_type:2;
    bool addressable:1;
    bool addressed:1;
    bool array:1;
};

struct FuncCallInfo{
    char* name;
    int argc;
    int retc;
    bool varfunc;
};

struct FuncData{
    char* lbl;
    char* bplbl;
    FuncAttr attr;
};

struct VarData{
    char* lbl;
    int bsize;
    size_t addr;
    size_t elcnt;
    VarAttr attr;
};

INC_VAR_TABLE(Var  , var  , VarData )
INC_VAR_TABLE(Func , func , FuncData)

struct ProgramNameTable{
    VarTable* vars;
    FuncTable* funcs;
};

#endif