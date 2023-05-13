#ifndef GEN_EXEC_CODE_H_INCLUDED
#define GEN_EXEC_CODE_H_INCLUDED

#include "compiler_out_lang/compiler_out_lang.h"
#include "compiler_out_lang/exec_output.h"

const unsigned char x86_jit_prefix[] = {0xCC};
const unsigned char x86_jit_suffix[] = {0xF1};

bool translateInstruction_x86_64(ExecOutput* out, void* instr_ptr);

bool translateCompilerOutput_x86_64(ExecOutput* out, CompilationOutput* in);

#endif