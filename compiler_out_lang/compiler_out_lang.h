#ifndef COMPILER_OUT_LANG_INCLUDED
#define COMPILER_OUT_LANG_INCLUDED

#include <stdint.h>
#include <string.h>

#define COMPILER_NATIVE_TYPE size_t

enum compilerInstrType_t{  //___compiles__|_args_|_ret_|_addinfo___
    COUT_TYPE_INVALID = 0, //             |      |     |
    COUT_TYPE_GENERIC = 1, //   something | any  | any | type

    COUT_TYPE_CONST   = 2, //   values    | 0    |  1  | val
    COUT_TYPE_LCONST  = 3, //   values    | ^    |  ^  | name

    COUT_TYPE_LOAD    = 4, //     mov ()  | 0/1  |  1  | MemArgAttr (+...
    COUT_TYPE_STORE   = 5, //     mov ()  | 1/2  |  0  | MemArgAttr (+...

    COUT_TYPE_JMP     = 6, //     jmp     | 0/1+F|  0  | type + MemArgAttr (+...

    COUT_TYPE_TABLE   = 7, //  only lbls  |  0   |  0  | type + name + ...       
    COUT_TYPE_STRUCT  = 8, // compile info|  0   |  ?  | type
    COUT_TYPE_SPECIAL = 9  // unusual things like Ret/Call
};

struct GenericInstructionInfo {
    const char* name; 
    int code;
    int argc;
    int retc;
};

#define G_INSTR_DEF(id, enum, name, arg, ret) \
COUT_GINSTR_ ## enum = id,

enum compilerGenericInstr_t {
    #include "generic_instruction_defines.h"
};
#undef G_INSTR_DEF

extern const GenericInstructionInfo g_instr_info[];

enum compilerTableInstr_t {//_what___________|_add data_______
    COUT_TABLE_NULL   = 0, // unused         | 
    COUT_TABLE_SIMPLE = 1, // constant       | value(native type)
    COUT_TABLE_PRE    = 2, // prototype      | actual def offset(size_t)
    COUT_TABLE_HERE   = 3, // label          | none
    COUT_TABLE_STACK  = 4, // alloc on stack | size(native type)
    COUT_TABLE_SECT   = 5, // alloc in sect  | name + size(native type)
    COUT_TABLE_ADDSECT = 6 // add section    | BinSectionAttr
};

enum compilerStructureInstr_t {
    COUT_STRUCT_NULL = 0,
    COUT_STRUCT_SECT_B = 1,
    COUT_STRUCT_SECT_E = 2,

    COUT_STRUCT_ADDSF = 3,
    COUT_STRUCT_RMSF = 4,

    COUT_STRUCT_NCB_B = 5,
    COUT_STRUCT_RCB_B = 6,
    COUT_STRUCT_CB_E  = 7,
    
    COUT_STRUCT_BC_B  = 8,
    COUT_STRUCT_BC_E  = 9
};

enum compilerSpecialInstr_t {
    COUT_SPEC_RETCB = 1,
    COUT_SPEC_RETF  = 2
};

struct CompilerMemArgAttr {
    bool store:1;
    bool mem_acc:1;
    bool mod_bp:1;
    bool mod_arg:1;

    bool volat:1;
    bool const_:1;
    bool lbl:1;
};

struct CompilerJumpTypeInfo {
    const char* name;
    int code;
};

enum compilerJumpType_t {
    COUT_JUMP_NVR = 0,
    COUT_JUMP_ALW = 1,
    COUT_JUMP_CALL = 2,

    COUT_JUMP_EQ  = 3,
    COUT_JUMP_NE  = 4,
    COUT_JUMP_GT  = 5,
    COUT_JUMP_LT  = 6,
    COUT_JUMP_GE  = 7,
    COUT_JUMP_LE  = 8    
};

extern const CompilerJumpTypeInfo jmp_type_info[];

enum binSectionFillType_t {
    BIN_SECTION_ZEROED = 0,
    BIN_SECTION_INITIALISED = 1,
    BIN_SECTION_UNINITIALISED = 2,
    BIN_SECTION_ONE_ED = 3
};

struct BinSectionAttr {
    binSectionFillType_t fill_type:2;
    bool entry_point:1;
    bool readable:1;
    bool writable:1;
    bool executable:1;
};

const BinSectionAttr BIN_SECTION_PRESET_MAIN  = {BIN_SECTION_INITIALISED, 1, 0, 0, 1};
const BinSectionAttr BIN_SECTION_PRESET_FUNC  = {BIN_SECTION_INITIALISED, 0, 0, 0, 1};
const BinSectionAttr BIN_SECTION_PRESET_DATA  = {BIN_SECTION_INITIALISED, 0, 1, 1, 0};
const BinSectionAttr BIN_SECTION_PRESET_CONST = {BIN_SECTION_INITIALISED, 0, 1, 0, 0};

struct CompilerInstrHeader {
    size_t size;
    int argc;
    bool join;
    compilerInstrType_t type;
};

struct CompilationOutput {
    void* data;
    size_t size;
    size_t capacity;
};

bool compilationOutputCtor(CompilationOutput* out);
void compilationOutputDtor(CompilationOutput* out);

bool resizeCompilationOutput(CompilationOutput* out, size_t size);

bool expandCompilationOutput(CompilationOutput* out, const void* elem, size_t elem_size);

#define EXPAND_COMPILATION_OUTPUT(out, elem) \
expandCompilationOutput(out, (elem), sizeof(*(elem)))

int getMemArgArgc(CompilerMemArgAttr attr);

const char* getInstrTypeName(compilerInstrType_t type);

#endif