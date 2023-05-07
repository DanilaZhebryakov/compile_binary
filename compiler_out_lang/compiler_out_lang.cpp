#include <stdlib.h>
#include <stdint.h>

#include "compiler_out_lang.h"

const GenericInstructionInfo g_instr_info[] = {
{"bad", COUT_GINSTR_BAD, 0, 0},
{"nop", COUT_GINSTR_NOP, 0, 0},
{"add", COUT_GINSTR_ADD, 2, 1},
{"sub", COUT_GINSTR_SUB, 2, 1},
{"mul", COUT_GINSTR_MUL, 2, 1},
{"div", COUT_GINSTR_DIV, 2, 1},
{"end", COUT_GINSTR_END, 1, 0}
};

const CompilerJumpTypeInfo jmp_type_info[] = {
{"NVR", COUT_JUMP_NVR   },
{"ALW", COUT_JUMP_ALW   },
{"CALL", COUT_JUMP_CALL  },
{"EQ" , COUT_JUMP_EQ    },
{"NE" , COUT_JUMP_NE    },
{"GT" , COUT_JUMP_GT    },
{"LT" , COUT_JUMP_LT    },
{"GE" , COUT_JUMP_GE    },
{"LE" , COUT_JUMP_LE    },
};

int getMemArgArgc(CompilerMemArgAttr attr){
    return attr.mod_arg;
}

const char* getInstrTypeName(compilerInstrType_t type){
    switch(type){
        case COUT_TYPE_INVALID:
            return "INVALID";
        case COUT_TYPE_GENERIC:
            return "Generic";
        case COUT_TYPE_CONST:
            return "Const  ";
        case COUT_TYPE_LCONST:
            return "LConst ";
        case COUT_TYPE_LOAD:
            return "Load   ";
        case COUT_TYPE_STORE:
            return "Store  ";
        case COUT_TYPE_JMP:
            return "Jump   ";
        case COUT_TYPE_TABLE:
            return "Table  ";
        case COUT_TYPE_STRUCT:
            return "Struct ";
        default:
            return "UNDEFINED";
    }
}

bool compilationOutputCtor(CompilationOutput* out){
    const int cmpout_min_cap = 100;
    out->data = calloc(cmpout_min_cap, sizeof(char));
    if (!out->data) 
        return false;
    out->capacity = cmpout_min_cap;
    out->size = 0;
    return true;
}

void compilationOutputDtor(CompilationOutput* out){
    out->capacity = -1;
    out->size = 0;
    free(out->data);
}

bool resizeCompilationOutput(CompilationOutput* out, size_t new_size){
    void* new_data = realloc(out->data, new_size);
    if (!new_data){
        return false;
    }
    out->capacity = new_size;
    return true;
}

bool expandCompilationOutput(CompilationOutput* out, void* elem, size_t elem_size) {
    if (out->capacity < out->size + elem_size){
        if (!resizeCompilationOutput(out, (out->capacity + elem_size) * 2)) {
           return false; 
        }
    }
    memcpy(((char*)out->data) + out->size, elem, elem_size);
    return true;
}

