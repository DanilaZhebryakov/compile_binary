#include <stdio.h>

#include "compiler_out_lang.h"

#define SIZE_LEFT (((char*)end_ptr) - ((char*)instr_ptr))
#define SIZE_LEFT_I ((int)SIZE_LEFT)

#define CHK_SIZE_CORRECT() \
{                        \
    if (SIZE_LEFT < 0) { \
        fprintf(file, "<unexpected end>"); \
        return;          \
    }                    \
}

#define CHK_SIZE_ENOUGH(val)    { \
    if (SIZE_LEFT < (long int)sizeof(val)) { \
        fprintf(file, "<unexpected end>"); \
        return; \
    } \
}

const char* getStructInstrString(compilerStructureInstr_t instr){
    switch(instr){
    case COUT_STRUCT_SECT_B:
        return "In section:";
    case COUT_STRUCT_SECT_E:
        return "End of section";
    case COUT_STRUCT_ADDSF:
        return "Add stack frame";
    case COUT_STRUCT_RMSF:
        return "Remove stack frame";

    case COUT_STRUCT_NCB_B:
        return "Non-returnable code block";
    case COUT_STRUCT_RCB_B:
        return "Returnable code block";
    case COUT_STRUCT_CB_E:
        return "End of code block";

    case COUT_STRUCT_NONLIN_B:
        return "Branching code";
    case COUT_STRUCT_NONLIN_E:
        return "End of branching code";

    default:
        return "UNDEFINED";
    }
}

void printGenericInstruction(FILE* file, void* instr_ptr, void* end_ptr){
    CHK_SIZE_ENOUGH(compilerGenericInstr_t)
    fprintf(file, "%s", g_instr_info[*((compilerGenericInstr_t*)instr_ptr)].name);
}
void printInstrMemArg(FILE* file, void* instr_ptr, void* end_ptr){
    CHK_SIZE_ENOUGH(CompilerMemArgAttr)
    CompilerMemArgAttr* attr = (CompilerMemArgAttr*) instr_ptr;
    instr_ptr = ((char*) instr_ptr) + sizeof(*attr);
    fputc((attr->store)   ? '>'   : '<'  , file);
    fputc((attr->volat)   ? '!'   : '-'  , file);
    fputs((attr->mem_acc) ? "M@(" : "--(", file);
    if (attr->lbl) {
        fputs((char*)instr_ptr, file);
    }
    else {
        fprintf(file, "%ld", *((size_t*)instr_ptr));
    }
    fputs(")", file);

    fputs((attr->mod_bp)  ? "+bp" : "", file);
    fputs((attr->mod_arg) ? "+arg" : "", file);
}

void printTableInstr(FILE* file, void* instr_ptr, void* end_ptr){
    CHK_SIZE_ENOUGH(compilerTableInstr_t)
    compilerTableInstr_t* instr = (compilerTableInstr_t*)instr_ptr;
    instr_ptr = ((char*)instr_ptr) + sizeof(*instr);
    fprintf(file, " %.*s ", SIZE_LEFT_I, (char*)instr_ptr);
    instr_ptr = ((char*) instr_ptr) + strnlen((char*)instr_ptr, SIZE_LEFT)+1;
    CHK_SIZE_CORRECT()

    switch (*instr){
        case COUT_TABLE_SIMPLE:
            CHK_SIZE_ENOUGH(COMPILER_NATIVE_TYPE)
            fprintf(file, "=%lu", *((COMPILER_NATIVE_TYPE*)instr_ptr));
            break;
        case COUT_TABLE_STACK:
            CHK_SIZE_ENOUGH(size_t)
            fprintf(file, "=%lu@<stk>", *((size_t*)instr_ptr));
            break;
        case COUT_TABLE_SECT:
            CHK_SIZE_ENOUGH(size_t)
            fprintf(file, "=%lu@%.*s", *(size_t*)( ((char*)instr_ptr) + strnlen((char*)instr_ptr, SIZE_LEFT - 1 - sizeof(size_t))+1), SIZE_LEFT_I, (char*)instr_ptr);
            break;
        case COUT_TABLE_HERE:
            fprintf(file, "=<here>");
            break;
        case COUT_TABLE_PRE:
            fprintf(file, "<pre>");
            break;
        case COUT_TABLE_ADDSECT:
            fputs("<new section>[", file);
            CHK_SIZE_ENOUGH(BinSectionAttr)
            fputc(((BinSectionAttr*)instr_ptr)->readable   ? 'R' : '.', file);
            fputc(((BinSectionAttr*)instr_ptr)->writable   ? 'W' : '.', file);
            fputc(((BinSectionAttr*)instr_ptr)->executable ? 'X' : '.', file);
            fputc(((BinSectionAttr*)instr_ptr)->entry_point ?'E' : '.', file);
            fprintf(file, "f%d", ((BinSectionAttr*)instr_ptr)->fill_type);
            fputc(']', file);
            break;
        default:
            fprintf(file, "UNDEFINED");
    }
}

void printStructInstr(FILE* file, void* instr_ptr, void* end_ptr){
    CHK_SIZE_ENOUGH(compilerStructureInstr_t)
    compilerStructureInstr_t instr = *((compilerStructureInstr_t*)instr_ptr);
    instr_ptr = ((char*)instr_ptr) + sizeof(instr);
    
    fprintf(file, "%s", getStructInstrString(instr));
    if (instr == COUT_STRUCT_SECT_B) {
        fprintf(file, " %.*s", SIZE_LEFT_I, (char*) instr_ptr);
    }
}

void printSpecialInstr(FILE* file, void* instr_ptr, void* end_ptr){
    CHK_SIZE_ENOUGH(compilerSpecialInstr_t)
    compilerSpecialInstr_t instr = *((compilerSpecialInstr_t*)instr_ptr);
    instr_ptr = ((char*)instr_ptr) + sizeof(instr);
    switch (instr){
        case COUT_SPEC_RETCB:
            fputs("RetCB", file);
            break;
        case COUT_SPEC_RETF:
            fputs("RetFunc", file); 
            break;
        default:
            fputs("UNDEFINED", file);
            break;
    }
}


void printCompilerInstruction(FILE* file, void* instr_ptr){
    CompilerInstrHeader* header = (CompilerInstrHeader*)instr_ptr;
    void* end_ptr = ((char*)header) + header->size;
    instr_ptr = ((char*)instr_ptr) + sizeof(*header);

    fprintf(file, "[%3lu] %s (%d>%d)", header->size, getInstrTypeName(header->type), header->argc, header->retc);


    switch (header->type){
        case COUT_TYPE_GENERIC:
            printGenericInstruction(file, instr_ptr, end_ptr);
            break;
        case COUT_TYPE_CONST:
            CHK_SIZE_ENOUGH(COMPILER_NATIVE_TYPE)
            fprintf(file, "%ld", *((COMPILER_NATIVE_TYPE*) instr_ptr));
            break;
        case COUT_TYPE_LCONST:
            fprintf(file, "%.*s", SIZE_LEFT_I, (char*)instr_ptr);
            break;
        case COUT_TYPE_JMP:
            CHK_SIZE_ENOUGH(compilerFlagCondition_t)
            fprintf(file, "%s ", flag_cond_info[*((compilerFlagCondition_t*)instr_ptr)].name);
            /* FALLTHRU */
        case COUT_TYPE_LOAD:
            /* FALLTHRU */
        case COUT_TYPE_STORE:
            printInstrMemArg(file, instr_ptr, end_ptr);
            break;
        case COUT_TYPE_TABLE:
            printTableInstr(file, instr_ptr, end_ptr);
            break;
        case COUT_TYPE_STRUCT:
            printStructInstr(file, instr_ptr, end_ptr);
            break;
        case COUT_TYPE_SPECIAL:
            printSpecialInstr(file, instr_ptr, end_ptr);
            break;
        default:
            break;
    }
    fprintf(file, "\n");
}

void printCompilerOutput(FILE* file, CompilationOutput* out){
    void* curr_instr_ptr = out->data;
    void* next_instr_ptr = ((char*)curr_instr_ptr) + ((CompilerInstrHeader*)curr_instr_ptr)->size;
    void* data_end = ((char*)out->data) + out->size;

    while (next_instr_ptr <= data_end) {
        printCompilerInstruction(file, curr_instr_ptr);
        curr_instr_ptr = next_instr_ptr;
        if (curr_instr_ptr >= data_end) {
            break;
        }
        next_instr_ptr = ((char*)curr_instr_ptr) + ((CompilerInstrHeader*)curr_instr_ptr)->size;
    }
    if (next_instr_ptr != curr_instr_ptr){
        fprintf(file, "Data ended incorrectly: instruction at (%ld/%ld) bytes over the end\n", (char*)curr_instr_ptr - ((char*)data_end), (char*)curr_instr_ptr - ((char*)data_end));
    }
}
