#include "compiler_out_lang.h"

bool emmitGenericInstruction(CompilationOutput* out, compilerGenericInstr_t instr){
    CompilerInstrHeader header = {sizeof(header) + sizeof(instr), g_instr_info[instr].argc, g_instr_info[instr].retc,  false, COUT_TYPE_GENERIC};

    if (!EXPAND_COMPILATION_OUTPUT(out, &header))
        return false;

    return EXPAND_COMPILATION_OUTPUT(out, &instr);
}

bool emmitJumpInstruction_l(CompilationOutput* out, compilerJumpType_t jump, CompilerMemArgAttr attr, const char* lbl){
    attr.lbl = true;
    int lbl_len = strlen(lbl);
    CompilerInstrHeader header = {sizeof(header) + sizeof(jump) + sizeof(attr) + lbl_len + 1, getMemArgArgc(attr), 0, false, COUT_TYPE_JMP};
    if (!EXPAND_COMPILATION_OUTPUT(out, &header))
        return false;
    if (!EXPAND_COMPILATION_OUTPUT(out, &jump))
        return false;
    if (!EXPAND_COMPILATION_OUTPUT(out, &attr))
        return false;
    return expandCompilationOutput(out, lbl, lbl_len + 1);
}


bool emmitMemInstruction_l(CompilationOutput* out, CompilerMemArgAttr attr, bool store, const char* lbl) {
    attr.lbl = true;
    int lbl_len = strlen(lbl);
    CompilerInstrHeader header = {sizeof(header) + sizeof(attr) + lbl_len + 1, getMemArgArgc(attr) + store, !store, false, store ? COUT_TYPE_STORE : COUT_TYPE_LOAD};
    if (!EXPAND_COMPILATION_OUTPUT(out, &header))
        return false;
    if (!EXPAND_COMPILATION_OUTPUT(out, &attr))
        return false;
    return expandCompilationOutput(out, lbl, lbl_len + 1);
}
bool emmitMemInstruction(CompilationOutput* out, CompilerMemArgAttr attr, bool store, COMPILER_NATIVE_TYPE addr) {
    attr.lbl = false;
    CompilerInstrHeader header = {sizeof(header) + sizeof(attr) + sizeof(addr), getMemArgArgc(attr) + store , !store, false, store ? COUT_TYPE_STORE : COUT_TYPE_LOAD};
    if (!EXPAND_COMPILATION_OUTPUT(out, &header))
        return false;
    if (!EXPAND_COMPILATION_OUTPUT(out, &attr))
        return false;
    return EXPAND_COMPILATION_OUTPUT(out, &addr);
}

bool emmitConstInstruction_l(CompilationOutput* out, const char* lbl){
    int lbl_len = strlen(lbl);
    CompilerInstrHeader header = {sizeof(header) + lbl_len + 1, 0, 1, false, COUT_TYPE_LCONST};
    if (!EXPAND_COMPILATION_OUTPUT(out, &header))
        return false;
    return expandCompilationOutput(out, lbl, lbl_len + 1);
}

bool emmitConstInstruction(CompilationOutput* out, COMPILER_NATIVE_TYPE val){
    CompilerInstrHeader header = {sizeof(header) + sizeof(val), 0, 1, false, COUT_TYPE_CONST};
    if (!EXPAND_COMPILATION_OUTPUT(out, &header))
        return false;
    return EXPAND_COMPILATION_OUTPUT(out, &val);
}

bool emmitStructuralInstruction(CompilationOutput* out, compilerStructureInstr_t instr, int arg, int ret){
    CompilerInstrHeader header = {sizeof(header) + sizeof(instr), arg, ret, false, COUT_TYPE_STRUCT};
    if (!EXPAND_COMPILATION_OUTPUT(out, &header))
        return false;
    return EXPAND_COMPILATION_OUTPUT(out, &instr);
}

bool emmitSetSection(CompilationOutput* out, const char* name){
    int name_len = strlen(name);
    compilerStructureInstr_t instr = COUT_STRUCT_SECT_B;
    CompilerInstrHeader header = {sizeof(header) + sizeof(instr) + name_len + 1, 0, 0, false, COUT_TYPE_STRUCT};
    if (!EXPAND_COMPILATION_OUTPUT(out, &header))
        return false;
    if (!EXPAND_COMPILATION_OUTPUT(out, &instr))
        return false;
    return expandCompilationOutput(out, name, name_len + 1);
}

bool emmitTableSomething(CompilationOutput* out, compilerTableInstr_t instr, const char* name, size_t addsize) {
    int name_len = strlen(name);
    CompilerInstrHeader header = {sizeof(header) + sizeof(instr) + name_len + 1 + addsize, 0, 0, false, COUT_TYPE_TABLE};
    if (!EXPAND_COMPILATION_OUTPUT(out, &header))
        return false;
    if (!EXPAND_COMPILATION_OUTPUT(out, &instr))
        return false;
    return expandCompilationOutput(out, name, name_len + 1);
}

bool emmitTableDefault(CompilationOutput* out, compilerTableInstr_t instr, const char* name, COMPILER_NATIVE_TYPE val){
    if (!emmitTableSomething(out, instr, name, sizeof(val)))
        return false;
    return EXPAND_COMPILATION_OUTPUT(out, &val);
}

bool emmitTableConstant(CompilationOutput* out, const char* name, COMPILER_NATIVE_TYPE val){
    return emmitTableDefault(out, COUT_TABLE_SIMPLE, name, val);
}
bool emmitTableStackVar(CompilationOutput* out, const char* name, size_t vsize){
    return emmitTableDefault(out, COUT_TABLE_STACK, name, vsize);
}
bool emmitTablePrototype(CompilationOutput* out, const char* name){
    return emmitTableDefault(out, COUT_TABLE_PRE, name, 0);
}
bool emmitTableLbl(CompilationOutput* out, const char* name){
    return emmitTableSomething(out, COUT_TABLE_HERE, name, 0);
}
bool emmitTableSectVar(CompilationOutput* out, const char* name, size_t vsize, const char* sect_name, size_t set_size, void* set_data){
    int sect_name_len = strlen(sect_name);
    if (!emmitTableSomething(out, COUT_TABLE_SECT, name, sect_name_len + 1 + set_size))
        return false;
    if (!expandCompilationOutput(out, sect_name, sect_name_len + 1))
        return false;
    if (!EXPAND_COMPILATION_OUTPUT(out, &vsize))
        return false;
    return expandCompilationOutput(out, set_data, set_size);
}
bool emmitAddSection(CompilationOutput* out, const char* name, BinSectionAttr attr) {
    if (!emmitTableSomething(out, COUT_TABLE_ADDSECT, name, sizeof(attr)))
        return false;
    return EXPAND_COMPILATION_OUTPUT(out, &attr);
}