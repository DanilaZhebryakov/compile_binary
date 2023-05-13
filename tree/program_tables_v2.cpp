#include "program_tables_v2.h"

const int tbl_entry_format_version = 1;

DEF_VAR_TABLE(Var  , var  , VarData )
DEF_VAR_TABLE(Func , func , FuncData)

bool valCntMatchesInt(funcValCnt_t val, int n){
    switch (val)
    {
        case VF_VAL_NONE:
            return n == 0;
        case VF_VAL_SINGLE:
            return n == 1;
        case VF_VAL_MULTI:
        case VF_VAL_COUNT:
            return true;    
        default:
            return false;
    }
}

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
    funcTableCtor(table->funcs);
    varTablePut (table->vars , {{"!", 0, 0}, "!0base!", 0, 0});
    funcTablePut(table->funcs, {{"!", "!"}, "!0base!", 0, 0});
}

void programNameTableDtor(ProgramNameTable* table){
    varTableDtor(table->vars);
    varTableDtor(table->funcs);

    free(table->vars);
    free(table->funcs);
}

bool programAscendLvl(ProgramNameTable* objs, ProgramPosData* pos){
    return varTablePut (objs->vars , {{"!",0, pos->rbp_offset}, "!base!", pos->lvl, pos->flvl});
}

bool programDescendLvl(ProgramNameTable* objs, ProgramPosData* pos){
    varTableDescendLvl  (objs->vars   , pos->lvl);
    pos->rbp_offset = varTableGetLast(objs->vars)->value.addr;
    if (ustackPop(objs->vars, nullptr) != VAR_NOERROR)
        return false;
    funcTableDescendLvl  (objs->funcs  , pos->lvl);
    return true;
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
    char* lbl = (char*)calloc(strlen(name) + strlen(prefix)+1 + strlen("_AABBCCDDEEFFGGHH")+1, sizeof(char));
    if (!lbl)
        return nullptr;
    programAddNewMem(pos, lbl);
    sprintf(lbl, "%s%s_%lX", prefix, name, pos->lbl_id);
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

FuncEntry* programGetFunc(ProgramNameTable* objs, ProgramPosData* pos, FuncCallInfo call){
    for (FuncEntry* i = ((FuncEntry*)objs->funcs->data) + objs->funcs->size; i >= objs->funcs->data; i--){
        if (strcmp(i->name, call.name) == 0 && i->value.attr.varfunc == call.varfunc 
            && valCntMatchesInt(i->value.attr.arg_cnt, call.argc) && valCntMatchesInt(i->value.attr.ret_cnt, call.retc)){
            return i;
        }
    }
    return nullptr;
}


