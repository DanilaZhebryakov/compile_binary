#include <stdio.h>
#include <stdlib.h>

#include "tree/compile_backend.h"
#include "compiler_out_lang/compiler_out_dump.h"
#include "compiler_out_lang/jit_run.h"
#include "compiler_out_lang/createElfFile.h"
#include "compiler_out_lang/fileworks.h"
#include "toasm_x86_64/genExecCode.h"

#include "expr/formule_utils.h"
#include "lib/parseArg.h"
#include "lib/cmd_tool_utils.h"

#include "misc_default_vals.h"

const char* const help_str = 
"The ;⸵⁉ compiler binary back-end utility\n"
"Options:\n"
"-i <filename> : specify input file (compiler out lang). Multiple files can be specified\n"
"If no output filename is specified, it is the same as input, but with \"" LANG_TREE_PRE_FILE_EXTENSION "\" extension\n"
"If no files are specified, defaults to \"" LANG_TREE_PRE_DEFAULT_FILE "\" \n"
"-l ; -elf : Generate ELF output file\n"
"-r ; -run : Run current output using JIT\n"
DEFAULT_ARG_DESCRIPTION
;

int main(int argc, const char* argv[]) {
    ExecOutput out = {};
    if (!execOutCtor(&out)) {
        return 1;
    }

    for (int i = 0; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "-i")) {
                CompilationOutput cmpout = {};
            }
        }
    }

}