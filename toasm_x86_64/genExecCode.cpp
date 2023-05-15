#include <assert.h>

#include "compiler_out_lang/exec_output.h"

#include "bool_check_define.h"

#define DEBUG_MODE

#ifdef DEBUG_MODE
#include "compiler_out_lang/compiler_out_dump.h"
#endif
struct InstructionBinCode {
    size_t size;
    const char* data;
};

static const InstructionBinCode ginstrBinCodes[] = {
    {1, "\xF1"}, //invalid
    {1, "\x90"}, //nop
    {6, "\x5a\x58\x48\x01\xd0\x50"}, //pop rdx, pop rax, add, push rax
    {6, "\x5a\x58\x48\x01\xd0\x50"}, //pop rdx, pop rax, sub, push rax
    {9, "\x59\x58\x48\x31\xd2\x48\xf7\xe1\x50"}, //pop, pop, xor, mul, push
    {9, "\x59\x58\x48\x31\xd2\x48\xf7\xf1\x50"}, //pop, pop, xor, mul, push
    {12,"\x48\x89\xEC\x5d\xb8\x3c\x00\x00\x00\x5f\x0f\x05"}, //exit (pop)
    {3, "\xff\x34\x24"}, //push qword(rsp)
    {4, "\x48\x83\xC4\x08"}, // add rsp,8
    {22,"\x6A\x00\xb8\0\0\0\0\xBF\0\0\0\0\x48\x89\xE6\xBA\x01\0\0\0\x0F\x05"}, // in
    {24,"\xB8\x01\0\0\0\xBF\x01\0\0\0\x48\x89\xE6\xBA\x01\0\0\0\x0F\x05\x48\x83\xC4\x08"}, //out
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

struct x86MemArgByteModrm {
    char rm:3;
    char reg:3;
    char mod:2;
};
struct x86MemArgByteSib {
    char base:3;
    char ind:3;
    char scl:2;
};

static bool translateMemInstr(ExecOutput* out, const void* arg_ptr, char opcode, char modrm_reg) {
    CompilerMemArgAttr attr = *(CompilerMemArgAttr*)arg_ptr;
    arg_ptr = ((char*)arg_ptr) + sizeof(attr);

    if (attr.mod_arg) {
        if (!execOutPutData(out, "\x5a", 1)) //pop rdx if arg needed
            return false;
    }

    char rex_prefix = 0x48;
    x86MemArgByteModrm arg_modrm = {};
    x86MemArgByteSib   arg_sib   = {};
    arg_modrm.reg = modrm_reg; //this has nothing to do with memory, it is part of instr, or its other arg
    arg_modrm.rm  = attr.mod_bp ? 0b100 : 0b000; // is base needed?
    arg_modrm.mod = 0b10; //32 bit offset every for now

    arg_sib.scl   = 0b00; // multiply here is unused
    arg_sib.ind   = attr.mod_arg ? 0b010 : 0b100; // if argument modifier is required, it is in rbx (for now)
    arg_sib.base  = 0b101; // use BP as base. Luckily, same bits also mean 'no base' if mod=0

    if (!(
            execOutPutData(out, &rex_prefix, 1) 
         && execOutPutData(out, &opcode    , 1)
         && execOutPutData(out, &arg_modrm , 1) 
         && execOutPutData(out, &arg_sib   , 1))) {    
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

    if (!translateMemInstr(out, instr_ptr, 0x8b , 0)) // mov rax, [MEM]
        return false;
    return execOutPutData(out, "\x50", 1); // push rax
}

static bool translateStoreInstr(ExecOutput* out, void* instr_ptr) {
    CompilerInstrHeader* header = ((CompilerInstrHeader*)instr_ptr);
    assert(header->type == COUT_TYPE_STORE);

    instr_ptr = ((char*)instr_ptr) + sizeof(CompilerInstrHeader);

    if (!execOutPutData(out, "\x58", 1)) // pop rax  
        return false;

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
    assert(header->type == COUT_TYPE_JMP);

    instr_ptr = ((char*)instr_ptr) + sizeof(CompilerInstrHeader);
    assert(0); //NYI
}

static bool addRspMoveInstr(ExecOutput* out, long int move){
    if (move == 0) {
        return true;
    }

    bool dir = move < 0;
    if (dir)
        move = -move;

    if (move >= 1L << 32) {
        error_log("Stack move value is too high (%ld). Probably error.\n", move);
        return false;
    }

    if (move < 256) {
        CHECK_BOOL(execOutPutData(out, dir ? "\x48\x83\xEC" : "\x48\x83\xC4", 3)); // (sub:add) rsp, imm8
        CHECK_BOOL(execOutPutData(out, &move, 1)); // big endian is good for this. Lower bytes are moved
        return true;
    }

    CHECK_BOOL(execOutPutData(out,  dir ? "\x48\x81\xEC" : "\x48\x81\xC4", 3)); // (sub:add) rsp, imm32
    CHECK_BOOL(execOutPutData(out, &move, 4)); // big endian is good for this. Lower bytes are moved
    return true;
}

static bool translateStructInstr(ExecOutput* out, void* instr_ptr) {
    CompilerInstrHeader* header = ((CompilerInstrHeader*)instr_ptr);
    compilerStructureInstr_t instr = *(compilerStructureInstr_t*)(((char*)instr_ptr) + sizeof(*header));

    assert(header->type == COUT_TYPE_STRUCT);

    assert(header->argc == 0);
    assert(header->retc == 0); //NYI


    if (instr & COUT_STRUCT_END_MASK) {
        if (instr != COUT_STRUCT_ADDSF) {
            long long clean = out->curr_context->stk_size_curr - out->curr_context->stk_size_beg;
            if (clean < 0) {
                error_log("Not enough elements on stack on sf end\n");
                return false;
            }
            CHECK_BOOL(addRspMoveInstr(out, clean*8));
        }
    }
    
    switch (instr){
        case COUT_STRUCT_ADDSF:
            CHECK_BOOL(execOutPutData(out, "\xc8\0\0\x01", 4)); //enter (0, 1)
            break;
        case COUT_STRUCT_RMSF:
            CHECK_BOOL(execOutPutData(out, "\xc9", 1)); //leave
            break;
        default:
            break;
    }

    return execOutHandleStructInstr(out, instr_ptr);
}

static bool translateTableInstr(ExecOutput* out, void* instr_ptr) {
    CompilerInstrHeader* header = ((CompilerInstrHeader*)instr_ptr);
    compilerTableInstr_t instr = *(compilerTableInstr_t*)(((char*)instr_ptr) + sizeof(*header));
    if (instr == COUT_TABLE_STACK) {
        void* instr_arg_ptr = ((char*)instr_ptr) + sizeof(*header) + sizeof(instr);
        instr_arg_ptr = ((char*)instr_arg_ptr) + strlen((char*)instr_arg_ptr) + 1;
        size_t var_size = *(size_t*)instr_arg_ptr;
        CHECK_BOOL(addRspMoveInstr(out, -8*(long int)var_size));
    }
    return execOutHandleTableInstr(out, instr_ptr);
}

bool translateInstruction_x86_64(ExecOutput* out, void* instr_ptr) {
    #ifdef DEBUG_MODE
    printCompilerInstruction(stderr, instr_ptr);
    #endif

    CompilerInstrHeader* header = ((CompilerInstrHeader*)instr_ptr);
    bool ret = false;
    if (out->curr_context->stk_size_curr < out->curr_context->stk_size_beg + header->argc) {
        error_log("not enough args on stack\n");
        return false;
    }

    #ifdef DEBUG_MODE
    if (out->curr_context->type != EOUT_CONTEXT_ROOT){
        CHECK_BOOL(execOutPutData(out, "\x90", 1));
    }
    #endif

    out->curr_context->stk_size_curr -= header->argc;
    switch (header->type){
        case COUT_TYPE_GENERIC:
            ret = translateGenericInstr(out, instr_ptr);
            break;
        case COUT_TYPE_CONST:
            ret = translateConstInstr(out, instr_ptr);
            break;
        case COUT_TYPE_LCONST:
            ret = translateSymConstInstr(out, instr_ptr);
            break;
        case COUT_TYPE_LOAD:
            ret = translateLoadInstr(out, instr_ptr);
            break;
        case COUT_TYPE_STORE:
            ret = translateStoreInstr(out, instr_ptr);
            break;
        case COUT_TYPE_JMP:
            ret = translateJumpInstr(out, instr_ptr);
            break;
        case COUT_TYPE_TABLE:
            ret = translateTableInstr(out, instr_ptr);
            break;
        case COUT_TYPE_STRUCT:
            ret = translateStructInstr(out, instr_ptr);
            break;
        case COUT_TYPE_SPECIAL:
            assert(0); //NYI
        default:
            error_log("Bad instruction type%d\n", header->type);
            return false;
    }
    out->curr_context->stk_size_curr += header->retc;
    return ret;
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