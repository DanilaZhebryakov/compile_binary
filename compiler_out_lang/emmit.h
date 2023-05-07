#ifndef CMPOUT_EMMIT_H_INCLUDED
#define CMPOUT_EMMIT_H_INCLUDED

#include "compiler_out_lang.h"


bool emmitGenericInstruction(CompilationOutput* out, compilerGenericInstr_t instr);

bool emmitJumpInstruction_l(CompilationOutput* out, compilerJumpType_t jump, CompilerMemArgAttr attr, char* lbl);

bool emmitMemInstruction_l(CompilationOutput* out, CompilerMemArgAttr attr, bool store, char* lbl);
bool emmitMemInstruction(CompilationOutput* out, CompilerMemArgAttr attr, bool store, COMPILER_NATIVE_TYPE addr);

bool emmitConstInstruction_l(CompilationOutput* out, char* lbl);
bool emmitConstInstruction(CompilationOutput* out, COMPILER_NATIVE_TYPE val);

bool emmitStructuralInstruction(CompilationOutput* out, compilerStructureInstr_t instr);
bool emmitSetSection(CompilationOutput* out, char* name);

bool emmitTableSomething(CompilationOutput* out, compilerTableInstr_t instr, char* name, size_t addsize);
bool emmitTableDefault(CompilationOutput* out, compilerTableInstr_t instr, char* name, COMPILER_NATIVE_TYPE val);
bool emmitTableConstant(CompilationOutput* out, char* name, COMPILER_NATIVE_TYPE val);
bool emmitTableStackVar(CompilationOutput* out, char* name, COMPILER_NATIVE_TYPE vsize);
bool emmitTablePrototype(CompilationOutput* out, char* name);
bool emmitTableLbl(CompilationOutput* out, char* name);
bool emmitTableSectVar(CompilationOutput* out, char* name, size_t vsize, char* sect_name, size_t set_size, void* set_data);

bool emmitAddSection(CompilationOutput* out, char* name, BinSectionAttr attr);

#endif
