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
"-f <filename> [out filename] : specify a file. Multiple files can be specified\n"
"If no output filename is specified, it is the same as input, but with \"" LANG_TREE_PRE_FILE_EXTENSION "\" extension\n"
"If no files are specified, defaults to \"" LANG_TREE_PRE_DEFAULT_FILE "\" \n"
"-D  : Dump output. Print output in text format to stdout. Useful for debugging\n"
"-c  : Compile to compiler out lang only. Do not translate.\n"
"-S  : use stdin and stdout as in and out files \n"
DEFAULT_ARG_DESCRIPTION
;

struct BackendRunFlags{
    bool out_dump:1;
    bool compile_only:1;
};

int processFile(FILE* tree_file, FILE* out_file, BackendRunFlags flags){
    BinTreeNode* prog = binTreeReadFromFile(tree_file);

    if(!prog){
        IF_NQ(fprintf(stderr,"Error while reading tree from input file\n");)
        return 2;
    }
    binTreeDump(prog);

    CompilationOutput cmp_out;
    compilationOutputCtor(&cmp_out);
    int cmp_ret = compileProgram(&cmp_out, prog);
    if (cmp_ret != 0){
        IF_NQ(fprintf(stderr,"Compilation errors occured. Code:%d\n", cmp_ret);)
    }
    if (flags.out_dump){
        printf("Dump:\n");
        printCompilerOutput(stdout, &cmp_out);
        printf("\n\n");
    }

    if (flags.compile_only) {
        if (writeCompileOutFile(&cmp_out, out_file) != 0) {
            printf("Compile output write fail\n");
        }
    }
    else{
        ExecOutput exec = {};
        execOutCtor(&exec);
        if (translateCompilerOutput_x86_64(&exec, &cmp_out)) {
            execOutApplyPreLbls(&exec);
            if (execOutPrepareCode(&exec, "", 0, x86_jit_suffix, sizeof(x86_jit_suffix))) {
                //execOutputJitRun(&exec);
                if (writeElfFile(&exec, out_file) != 0) {
                    printf("ELF fail\n");
                }
            }
            else{
                printf("code prepare failed\n");
            }
        }
        else {
            printf("translation failed\n");
        }
        execOutDtor(&exec);
    }
    compilationOutputDtor(&cmp_out);

    binTreeDtor(prog);
    return 0;
}

int processFile(const char* input_filename, const char* out_filename, BackendRunFlags flags) {
    IF_VB(fprintf(stderr, "Input file: %s ", input_filename);)

    bool alloc_str = false;
    if (!out_filename){
        alloc_str = true;
        const char* result_extension = flags.compile_only ? LANG_CMPOUT_FILE_EXTENSION : "";
        out_filename = repl_file_extension(input_filename, result_extension);
    }
    IF_VB(fprintf(stderr, "Output: %s\n", out_filename);)

    FILE* tree_file = fopen(input_filename, "r");
    FILE* out_file = tree_file ? fopen(out_filename, "w") : nullptr;

    int ret = -2;
    if (tree_file && out_file){
        ret = processFile(tree_file, out_file, flags);
    }
    else{
        ret = 1;
    }
    if (tree_file){
        fclose(tree_file);
        if (out_file){
            fclose(out_file);
        }
        else{
            IF_NQ(fprintf(stderr, "Can not open output file (%s)\n", out_filename);)
        }
    }
    else{
        IF_NQ(fprintf(stderr, "Can not open input file (%s). Probably it does not exist\n", input_filename);)
    }
    
    if (alloc_str) {
        free((void*)out_filename);
    }
    return ret;
}

int main(int argc, const char* argv[]){
    HANDLE_DEFAULT_ARGS

    BackendRunFlags flags = {};

    flags.out_dump     = parseArgBegin(argc, argv, "-D") >= 0;
    flags.compile_only = parseArgBegin(argc, argv, "-c") >= 0;
    
    int file_counter = 0;

    if (parseArg(argc, argv, "-S") >= 0) {
        int ret = processFile(stdin, stdout, flags);
        file_counter++;
        if(ret != 0){
            return ret;
        }
    }

    for (int arg_i = 0; arg_i < argc; arg_i++){
        if (strcmp(argv[arg_i], "-f") == 0) {
            if (arg_i == argc-1) {
                IF_NQ(fprintf(stderr, "-f: expected a filename\n");)
                return 1;
            }
            const char* inp_filename = argv[arg_i+1];
            const char* out_filename = nullptr;
            arg_i++;
            if (arg_i < argc-1 && argv[arg_i + 1][0] != '-') {
                out_filename = argv[arg_i + 1];
                arg_i++;
            }
            int ret = processFile(inp_filename, out_filename, flags);
            file_counter++;
            if (ret != 0) {
                return ret;
            }
        }
    }
    if (file_counter == 0) {
        IF_NQ(fprintf(stderr, "Warning: no files specified. Using default. (run with --help for help)\n");)

        int ret = processFile(LANG_TREE_BCK_DEFAULT_FILE, LANG_CMPOUT_DEFAULT_FILE, flags);
        file_counter++;
        if (ret != 0) {
            return ret;
        }
    }    

    IF_VB(fprintf(stderr, "Succcessfully processed %d files\n", file_counter);)
    return 0;
}
