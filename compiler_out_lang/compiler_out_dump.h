#ifndef COMPILER_OUT_DUMP_H_INCLUDED
#define COMPILER_OUT_DUMP_H_INCLUDED

#include <stdio.h>

#include "compiler_out_lang.h"

void printCompilerInstruction(FILE* file, void* instr_ptr);

void printCompilerOutput(FILE* file, CompilationOutput* out);

#endif