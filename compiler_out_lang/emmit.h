#ifndef CMPOUT_EMMIT_H_INCLUDED
#define CMPOUT_EMMIT_H_INCLUDED

#include "compiler_out_lang.h"

bool emitGenericInstruction(CompilationOutput* out, compilerGenericInstr_t instr);

bool emitJumpInstruction_l(CompilationOutput* out, compilerFlagCondition_t jump, CompilerMemArgAttr attr, const char* lbl);

bool emitMemInstruction_l(CompilationOutput* out, CompilerMemArgAttr attr, bool store, const char* lbl);
bool emitMemInstruction(CompilationOutput* out, CompilerMemArgAttr attr, bool store, COMPILER_NATIVE_TYPE addr);

bool emitConstInstruction_l(CompilationOutput* out, const char* lbl);
bool emitConstInstruction(CompilationOutput* out, COMPILER_NATIVE_TYPE val);

bool emitStructuralInstruction(CompilationOutput* out, compilerStructureInstr_t instr, int arg = 0, int ret = 0);
bool emitSetSection(CompilationOutput* out, const char* name);

bool emitTableSomething(CompilationOutput* out, compilerTableInstr_t instr, const char* name, size_t addsize);
bool emitTableDefault(CompilationOutput* out, compilerTableInstr_t instr, const char* name, COMPILER_NATIVE_TYPE val);
bool emitTableConstant(CompilationOutput* out, const char* name, COMPILER_NATIVE_TYPE val);
bool emitTableStackVar(CompilationOutput* out, const char* name, COMPILER_NATIVE_TYPE vsize);
bool emitTablePrototype(CompilationOutput* out, const char* name);
bool emitTableLbl(CompilationOutput* out, const char* name);
bool emitTableSectVar(CompilationOutput* out, const char* name, size_t vsize, const char* sect_name, size_t set_size, void* set_data);

bool emitAddSection(CompilationOutput* out, const char* name, BinSectionAttr attr);

bool emitRetInstruction(CompilationOutput* out, int retc, bool retf);
bool emitCallInstruction(CompilationOutput* out, int argc, int retc, const char* name);

#endif
