#include <stdio.h>
#include "compiler_out_lang.h"
#include "exec_output.h"

bool writeCompileOutFile(CompilationOutput* out, FILE* file) {
    if (fwrite(out, sizeof(*out), 1, file) != 1) {
        return false;
    }
    if (fwrite(out, out->size, 1, file) != 1) {
        return false;
    }
    return true;
}

int createCompileOutFile(CompilationOutput* out, const char* filename) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        return 1;
    }
    if (writeCompileOutFile(out, file)) {
        fclose(file);
        return -2;
    }
    fclose(file);
    return 0;
}

int readCompileOutFile(CompilationOutput* out, FILE* file) {
    if (fread(out, sizeof(*out), 1, file) != 1) {
        return -2;
    }
    out->capacity = out->size;
    out->data = calloc(out->size, sizeof(char));
    if (out->data == nullptr) {
        return -1;
    }

    if (fread(out, out->size, 1, file) != 1) {
        return -2;
    }
    return 0;
}

int loadCompileOutFile(CompilationOutput* out, const char* filename) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        return 1;
    }
    int ret = readCompileOutFile(out, file);
    fclose(file);
    return ret;
}