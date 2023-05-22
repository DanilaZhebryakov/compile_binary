#include <stdlib.h>
#include <stdint.h>

#include "compiler_out_lang.h"

#define G_INSTR_DEF(id, enum, name, arg, ret)\
{name, id, arg, ret},

const GenericInstructionInfo g_instr_info[] = {
    #include "generic_instruction_defines.h"
};
#undef G_INSTR_DEF

const CompilerFlagConditionInfo flag_cond_info[] = {
{"NVR", COUT_FLAGS_NVR   },
{"ALW", COUT_FLAGS_ALW   },
{"EQ" , COUT_FLAGS_EQ    },
{"NE" , COUT_FLAGS_NE    },
{"GT" , COUT_FLAGS_GT    },
{"LE" , COUT_FLAGS_LE    },
{"GE" , COUT_FLAGS_GE    },
{"LT" , COUT_FLAGS_LT    },
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
        case COUT_TYPE_SPECIAL:
            return "Special ";
        default:
            return "UNDEFINED";
    }
}

compilerFlagCondition_t invertFlagCondition(compilerFlagCondition_t jmp_type) {
    return (compilerFlagCondition_t)(jmp_type ^ 1);
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
    free(out->data);
    #ifndef NDEBUG
        out->capacity = -1;
        out->size = 0;
        out->data = nullptr;
    #endif
}

bool resizeCompilationOutput(CompilationOutput* out, size_t new_size){
    void* new_data = realloc(out->data, new_size);
    if (!new_data){
        return false;
    }
    out->capacity = new_size;
    out->data = new_data;
    return true;
}

bool expandCompilationOutput(CompilationOutput* out, const void* elem, size_t elem_size) {
    if (out->capacity < out->size + elem_size){
        if (!resizeCompilationOutput(out, (out->capacity + elem_size) * 2)) {
           return false; 
        }
    }
    memcpy(((char*)out->data) + out->size, elem, elem_size);
    out->size += elem_size;
    return true;
}

