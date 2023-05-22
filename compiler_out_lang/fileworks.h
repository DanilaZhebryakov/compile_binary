#include "compiler_out_lang.h"
#include "exec_output.h"

bool writeCompileOutFile(CompilationOutput* out, FILE* file);

int createCompileOutFile(CompilationOutput* out, const char* filename);

int readCompileOutFile(CompilationOutput* out, FILE* file);

int loadCompileOutFile(CompilationOutput* out, const char* filename);