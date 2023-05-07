#include <stdio.h>

#include "compiler_out_lang.h"

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

    case COUT_STRUCT_BC_B:
        return "Branching code";
    case COUT_STRUCT_BC_E:
        return "End of branching code";

    default:
        return "UNDEFINED";
    }
}

void printGenericInstruction(FILE* file, void* instr_ptr){
    fprintf(file, "%s", g_instr_info[*((compilerGenericInstr_t*)instr_ptr)].name);
}
void printInstrMemArg(FILE* file, void* instr_ptr){
    CompilerMemArgAttr* attr = (CompilerMemArgAttr*) instr_ptr;
    instr_ptr = ((char*) instr_ptr) + sizeof(*attr);
    fputc((attr->store)   ? '>'   : '<'  , file);
    fputc((attr->volat)   ? '!'   : '.'  , file);
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

void printTableInstr(FILE* file, void* instr_ptr){
    compilerTableInstr_t* instr = (compilerTableInstr_t*)instr_ptr;
    instr_ptr = ((char*)instr_ptr) + sizeof(*instr);
    fprintf(file, " %s ", (char*)instr_ptr);
    instr_ptr = ((char*) instr_ptr) + strlen((char*)instr_ptr);
    switch (*instr){
        case COUT_TABLE_SIMPLE:
            fprintf(file, "=%ld", *((COMPILER_NATIVE_TYPE*)instr_ptr));
            break;
        case COUT_TABLE_STACK:
            fprintf(file, "=%ld@<stk>", *((COMPILER_NATIVE_TYPE*)instr_ptr));
            break;
        case COUT_TABLE_SECT:
            fprintf(file, "=%ld@%s", *(COMPILER_NATIVE_TYPE*)( ((char*)instr_ptr) + strlen((char*)instr_ptr)), (char*)instr_ptr);
            break;
        case COUT_TABLE_HERE:
            fprintf(file, "=<here>");
            break;
        case COUT_TABLE_PRE:
            fprintf(file, "<pre>");
            break;
        case COUT_TABLE_ADDSECT:
            fprintf(file, "<new section>[");
            fputc(((BinSectionAttr*)instr_ptr)->readable   ? 'R' : '.', file);
            fputc(((BinSectionAttr*)instr_ptr)->writable   ? 'W' : '.', file);
            fputc(((BinSectionAttr*)instr_ptr)->executable ? 'X' : '.', file);
            fputc(((BinSectionAttr*)instr_ptr)->entry_point ?'E' : '.', file);
            fprintf(file, "f%2.2b]", ((BinSectionAttr*)instr_ptr)->fill_type);
            break;
        default:
            fprintf(file, "UNDEFINED");
    }
}

void printStructInstr(FILE* file, void* instr_ptr){
    compilerStructureInstr_t instr = *((compilerStructureInstr_t*)instr_ptr);
    instr_ptr = ((char*)instr_ptr) + sizeof(instr);
    printf("%s", getStructInstrString(instr));
    if (instr == COUT_STRUCT_SECT_B) {
        printf(" %s", (char*) instr_ptr);
    }
}

void printSpecialInstr(FILE* file, void* instr_ptr){
    compilerSpecialInstr_t instr = *((compilerSpecialInstr_t*)instr_ptr);
    instr_ptr = ((char*)instr_ptr) + sizeof(instr);
    switch (instr){
        case COUT_SPEC_RETCB:
            fputs("RetCB", file);
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
    instr_ptr = ((char*)instr_ptr) + sizeof(*header);

    fprintf(file, "[%3d] %s (%d)", header->size, getInstrTypeName(header->type), header->argc);

    switch (header->type){
        case COUT_TYPE_GENERIC:
            printGenericInstruction(file, instr_ptr);
            break;
        case COUT_TYPE_CONST:
            fprintf(file, "%ld", *((COMPILER_NATIVE_TYPE*) instr_ptr));
            break;
        case COUT_TYPE_LCONST:
            fprintf(file, "%s", (char*)instr_ptr);
            break;
        case COUT_TYPE_JMP:
            fprintf(file, "%s ", jmp_type_info[*((compilerJumpType_t*)instr_ptr)].name);
        case COUT_TYPE_LOAD:
        case COUT_TYPE_STORE:
            printInstrMemArg(file, instr_ptr);
            break;
        case COUT_TYPE_TABLE:
            printTableInstr(file, instr_ptr);
            break;
        case COUT_TYPE_STRUCT:
            printStructInstr(file, instr_ptr);
            break;
        case COUT_TYPE_SPECIAL:
            printSpecialInstr(file, instr_ptr);
            break;
        default:
            break;
    }
    fprintf(file, "\n");
}

void printCompilerOutput(FILE* file, CompilationOutput* out){
    void* next_instr_ptr = out->data;
    void* data_end = ((char*)out->data) + out->size;

    while (next_instr_ptr < data_end) {
        printCompilerInstruction(file, next_instr_ptr);
        next_instr_ptr = ((char*)next_instr_ptr) + ((CompilerInstrHeader*)next_instr_ptr)->size;
    }
    if (next_instr_ptr != data_end){
        fprintf(file, "Data ended incorrectly: %d bytes past the end\n", ((char*)next_instr_ptr) - ((char*)data_end));
    }
}
