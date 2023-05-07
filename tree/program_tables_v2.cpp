#include "program_tables_v2.h"

const int tbl_entry_format_version = 1;

DEF_VAR_TABLE(Var  , var  , VarData )
DEF_VAR_TABLE(Func , func , FuncData)

int varTableGetNewAddr(VarTable* stk){
    if (stk->size == 0){
        return 1;
    }
    VarData* last_var = &(varTableGetLast(stk)->value);
    return last_var->addr + (last_var->bsize * last_var->elcnt);
}

void programNameTableCtor(ProgramNameTable* table){
    table->vars   = (VarTable*)malloc(sizeof(*table->vars));
    table->funcs  = (VarTable*)malloc(sizeof(*table->funcs));

    varTableCtor(table->vars);
    varTableCtor(table->funcs);
}

void programNameTableDtor(ProgramNameTable* table){
    varTableDtor(table->vars);
    varTableDtor(table->funcs);

    free(table->vars);
    free(table->funcs);
}

void programDescendLvl(ProgramNameTable* objs, ProgramPosData* pos, int lvl){
    varTableDescendLvl  (objs->vars   , lvl);
    pos->rbp_offset = varTableGetLast(objs->vars)->value.addr;
    ustackPop(objs->vars, nullptr);
    varTableDescendLvl  (objs->funcs  , lvl);
}


void varEntryToTxt(FILE* file, VarEntry* var) {
    fprintf(file, "VAR %s v%d :%s d%df%d %lux%X ", var->name, tbl_entry_format_version, var->value.lbl, var->depth, var->fdepth, var->value.elcnt, var->value.bsize);

    fprintf(file, "a$%X&%X@%X", var->value.attr.acc_type, var->value.attr.addressable, var->value.attr.array);
}

void funcEntryToTxt(FILE* file, FuncEntry* func) {
    fprintf(file, "FUNC %s v%d c:%s b:%s d%df%d ", func->name, tbl_entry_format_version, func->value.lbl, func->value.bplbl, func->fdepth, func->fdepth);

    fprintf(file, "a$%XA%XR%X ", func->value.attr.acc_type, func->value.attr.arg_cnt, func->value.attr.ret_cnt);
}

char* addNameLbl(ProgramPosData* pos, const char* name, const char* prefix, bool global){
    char* lbl = (char*)calloc(strlen(name) + strlen(prefix) + strlen("_AABBCCDDEEFFGGHH"), sizeof(char));
    if (!lbl)
        return nullptr;
    programAddNewMem(pos, lbl);
    sprintf(lbl, "%s%s_%p", prefix, name, pos->lbl_id);
    pos->lbl_id++;
    return lbl;
}

bool programCreateVar(ProgramNameTable* objs, ProgramPosData* pos, char* name, VarData* var, bool global){
    var->addr = pos->rbp_offset;
    pos->rbp_offset += var->bsize * var->elcnt;

    var->lbl = addNameLbl(pos, name, "var_", global);
    if(!(var->lbl))
        return false;

    bool ret = varTablePut(objs->vars, {*var, name, pos->lvl, pos->flvl});

    #ifdef LOG_VARS
    printf_log("Var:%s Level:%d Flevel:%d Addr:%d\n", name, pos->lvl, pos->flvl, pos->rbp_offset);
    #endif
    return ret;
}

bool programCreateFunc(ProgramNameTable* objs, ProgramPosData* pos, char* name, FuncData* func, bool global){
    func->lbl = addNameLbl(pos, name, func->attr.varfunc ? "vfunc_" : "func_", global);
    if(!(func->lbl))
        return false;

    bool ret = funcTablePut(objs->funcs, {*func, name, pos->lvl, pos->flvl});

    #ifdef LOG_VARS
    printf_log("Func:%s Level:%d Flevel:%d Addr:%d\n", name, pos->lvl, pos->flvl, pos->rbp_offset);
    #endif
    return ret;
}




