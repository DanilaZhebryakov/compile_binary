#include <assert.h>

#include "compiler_out_lang/exec_output.h"

struct InstructionBinCode {
    size_t size;
    const char* data;
};

const InstructionBinCode ginstrBinCodes[] = {
    {1, "\xF1"}, //invalid
    {1, "\x90"}, //nop
    {6, "\x5a\x58\x48\x01\xd0\x50"}, //pop rdx, pop rax, add, push rax
    {6, "\x5a\x58\x48\x01\xd0\x50"}, //pop rdx, pop rax, sub, push rax
    {9, "\x59\x58\x48\x31\xd2\x48\xf7\xe1\x50"}, //pop, pop, xor, mul, push
    {9, "\x59\x58\x48\x31\xd2\x48\xf7\xf1\x50"}, //pop, pop, xor, mul, push
    {12,"\x48\x89\xEC\x5d\xb8\x3c\x00\x00\x00\x5f\x0f\x05"}, //exit (pop)
    {3, "\xff\x34\x24"}, //push qword(rsp)
    {4, "\x48\x83\xC4\x08"}, // add rsp,8
    {22,"\x6A\x00\xb8\0\0\0\0\xbf\0\0\0\0\x48\x89\xE6\xBA\x01\0\0\0\x0F\x05"}, // in
    {25,"\x50\xB8\x01\0\0\0\xBF\x01\0\0\0\x48\x89\xE6\xBA\x01\0\0\0\x0F\x05\x48\x83\xC4\x08"}, //out
    {1 ,"\xCC"} //int 3 (trap)
};
const int g_instr_cnt = 12;

static bool translateGenericInstr(ExecOutput* out, void* instr_ptr) {
    CompilerInstrHeader* header = ((CompilerInstrHeader*)instr_ptr);
    assert (header->type == COUT_TYPE_GENERIC);
    instr_ptr = ((char*)instr_ptr) + sizeof(CompilerInstrHeader);

    compilerGenericInstr_t instr = *((compilerGenericInstr_t*)instr_ptr);
    
    if (instr >= g_instr_cnt) {
        printf("Unknown instr %d", instr);
        return false;
    }
    return execOutPutData(out, ginstrBinCodes[instr].data, ginstrBinCodes[instr].size);
}

struct x86MemArgBytes {
    char modrm_mod:2;
    char modrm_reg:3;
    char modrm_rm:3;
    char sib_scl:2;
    char sib_ind:3;
    char sib_base:3;
};

static bool translateMemInstr(ExecOutput* out, const void* arg_ptr, char opcode, char modrm_reg) {
    CompilerMemArgAttr attr = *(CompilerMemArgAttr*)arg_ptr;
    arg_ptr = ((char*)arg_ptr) + sizeof(attr);

    if (attr.mod_arg) {
        if (!execOutPutData(out, "\x5a", 1)) //pop rdx if arg needed
            return false;
    }

    char rex_prefix = 0x48;
    x86MemArgBytes arg_bytes = {};
    arg_bytes.modrm_reg = modrm_reg; //this has nothing to do with memory, it is part of instr, or its other arg
    arg_bytes.modrm_mod = attr.mod_bp ? 0b10 : 0b00; // is base needed?
    arg_bytes.modrm_rm  = 0b10; //32 bit offset every for now
    arg_bytes.sib_scl   = 0b00; // For now, multiply here is unused
    arg_bytes.sib_ind   = attr.mod_arg ? 0b010 : 0b100; // if argument modifier is required, it is in rbx (for now)
    arg_bytes.sib_base  = 0b101; // use BP as base. Luckily, same bits also mean 'no base' if mod=0

    if (!(
            execOutPutData(out, &rex_prefix, 1) 
         && execOutPutData(out, &opcode    , 1)
         && execOutPutData(out, &arg_bytes , 2) )) {    
        return false;
    }
    
    if (attr.lbl) {
        return execOutPutSym(out, (const char*)arg_ptr, 4); //iiuc displ32 is valid here, not 64
    }
    else {
        return execOutPutData(out, arg_ptr, 4);
    }
}

static bool translateLoadInstr(ExecOutput* out, void* instr_ptr) {
    CompilerInstrHeader* header = ((CompilerInstrHeader*)instr_ptr);
    assert(header->type == COUT_TYPE_LOAD);
    instr_ptr = ((char*)instr_ptr) + sizeof(CompilerInstrHeader);

    CompilerMemArgAttr m_arg = *(CompilerMemArgAttr*)instr_ptr;
    instr_ptr = ((char*)instr_ptr) + sizeof(m_arg);

    if (!translateMemInstr(out, instr_ptr, 0x8b , 0)) // mov rax, [MEM]
        return false;
    return execOutPutData(out, "\x50", 1); // push rax
}

static bool translateStoreInstr(ExecOutput* out, void* instr_ptr) {
    CompilerInstrHeader* header = ((CompilerInstrHeader*)instr_ptr);
    assert(header->type == COUT_TYPE_STORE);

    instr_ptr = ((char*)instr_ptr) + sizeof(CompilerInstrHeader);

    if (!execOutPutData(out, "\x50", 1)) // push rax  
        return false;

    CompilerMemArgAttr m_arg = *(CompilerMemArgAttr*)instr_ptr;
    instr_ptr = ((char*)instr_ptr) + sizeof(m_arg);

    return translateMemInstr(out, instr_ptr, 0x89 , 0); // mov [MEM], rax
}

static bool translateConstInstr(ExecOutput* out, COMPILER_NATIVE_TYPE value){
    if (value < (1L << 7)){ //imm8 enough
        return execOutPutData(out, "\x6A", 1) && execOutPutData(out, &value, 1);
    }

    if (value < (1L << 31)){ //imm32 enough
        return execOutPutData(out, "\x68", 1) && execOutPutData(out, &value, 4);
    }
    return execOutPutData(out, "\xB8", 1) // mov rax, imm64
        && execOutPutData(out, &value , 8) // that imm64
        && execOutPutData(out, "\x50", 1); // push rax
}

static bool translateConstInstr(ExecOutput* out, void* instr_ptr) {
    CompilerInstrHeader* header = ((CompilerInstrHeader*)instr_ptr);
    assert(header->type == COUT_TYPE_CONST);

    instr_ptr = ((char*)instr_ptr) + sizeof(CompilerInstrHeader);

    return translateConstInstr(out, *(COMPILER_NATIVE_TYPE*)instr_ptr);
}

static bool translateSymConstInstr(ExecOutput* out, void* instr_ptr) {
    CompilerInstrHeader* header = ((CompilerInstrHeader*)instr_ptr);
    assert(header->type != COUT_TYPE_CONST);

    instr_ptr = ((char*)instr_ptr) + sizeof(CompilerInstrHeader);
    CmpoutSymEntry* sym = cmpoutSymTableGet(&(out->syms), (const char*)instr_ptr);

    if(sym->value.attr.relocatable) {
        return execOutPutData(out, "\xB8", 1) // mov rax, imm64
        && execOutPutSym(out, (const char*)instr_ptr, sizeof(COMPILER_NATIVE_TYPE)) // that imm64
        && execOutPutData(out, "\x50", 1); // push rax
    }
    else {
        return translateConstInstr(out, sym->value.value);
    }
}

static bool translateJumpInstr(ExecOutput* out, void* instr_ptr) {
    CompilerInstrHeader* header = ((CompilerInstrHeader*)instr_ptr);
    assert(header->type == COUT_TYPE_CONST);

    instr_ptr = ((char*)instr_ptr) + sizeof(CompilerInstrHeader);
    assert(0); //NYI
}

bool translateInstruction_x86_64(ExecOutput* out, void* instr_ptr) {
    CompilerInstrHeader* header = ((CompilerInstrHeader*)instr_ptr);
    switch (header->type){
        case COUT_TYPE_GENERIC:
            return translateGenericInstr(out, instr_ptr);
        case COUT_TYPE_CONST:
            return translateConstInstr(out, instr_ptr);
        case COUT_TYPE_LCONST:
            return translateSymConstInstr(out, instr_ptr);
        case COUT_TYPE_LOAD:
            return translateLoadInstr(out, instr_ptr);
        case COUT_TYPE_STORE:
            return translateStoreInstr(out, instr_ptr);
        case COUT_TYPE_JMP:
            return translateJumpInstr(out, instr_ptr);
        case COUT_TYPE_TABLE:
            return execOutHandleTableInstr(out, instr_ptr);
        case COUT_TYPE_STRUCT:
            return execOutHandleStructInstr(out, instr_ptr);
        case COUT_TYPE_SPECIAL:
            assert(0); //NYI
        default:
            error_log("Bad instruction type%d\n", header->type);
            return false;
    }
    assert(0);
}

bool translateCompilerOutput_x86_64(ExecOutput* out, CompilationOutput* in) {
    void* next_instr_ptr = in->data;
    void* data_end = ((char*)in->data) + in->size;

    while (next_instr_ptr < data_end) {
        if (!translateInstruction_x86_64(out, next_instr_ptr))
            return false;
        next_instr_ptr = ((char*)next_instr_ptr) + ((CompilerInstrHeader*)next_instr_ptr)->size;
    }
    if (next_instr_ptr != data_end){
        error_log("Data ended incorrectly: %ld bytes past the end\n", ((char*)next_instr_ptr) - ((char*)data_end));
        return true;
    }
    return true;
}