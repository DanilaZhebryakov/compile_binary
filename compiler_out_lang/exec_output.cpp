#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "lib/logging.h"
#include "compiler_out_lang/exec_output.h"

#include "bool_check_define.h"

bool execOutPushContext(ExecOutput* out, ExecOutContext* context) {
    CHECK_BOOL(ustackPush(&(out->context_stk), context) == VAR_NOERROR);
    out->curr_context = (ExecOutContext*)ustackTopPtr(&(out->context_stk));
    return true;
}

bool execOutPopContext(ExecOutput* out) {
    CHECK_BOOL(ustackPop(&(out->context_stk), nullptr) == VAR_NOERROR);
    out->curr_context = (ExecOutContext*)ustackTopPtr(&(out->context_stk));
    return true;
}


bool execOutCtor(ExecOutput* out){
    CHECK_BOOL(relocationTableCtor(&(out->relt)));
    cmpoutSymTableCtor(&(out->syms));
    ustackCtor(&(out->context_stk), sizeof(*(out->curr_context)));

    ExecOutContext context = {EOUT_CONTEXT_ROOT, (size_t)-1, 0, 0};
    CHECK_BOOL(execOutPushContext(out, &context));


    out->sect_count = 0;
    out->lvl        = 0;
    out->sects = nullptr;
    return true;
}

void execOutDtor(ExecOutput* out){
    relocationTableDtor(&(out->relt));
    cmpoutSymTableDtor(&(out->syms));
    ustackDtor(&(out->context_stk));

    if (out->sect_count > 0){
        for (ExecOutSection* sect = out->sects; sect < out->sects + out->sect_count; sect++){
            if (sect->data != nullptr)
                free(sect->data);
        }
        free(out->sects);
    }
    #ifndef NDEBUG
        out->curr_context = nullptr;
        out->sect_count = 0;
        out->sects = nullptr;
    #endif
}


const char* execOutContextTypeString(execOutContextType_t type) {
    switch (type){
        case EOUT_CONTEXT_ROOT:
            return "Root";
        case EOUT_CONTEXT_SECT:
            return "Section";
        case EOUT_CONTEXT_CB:
            return "Code block";
        case EOUT_CONTEXT_NONLIN:
            return "Non-linear";
        case EOUT_CONTEXT_SF:
            return "Stack frame";
        default:
            return "Invalid";
    }
}

inline static ExecOutSection* getCurrSect(ExecOutput* out) {
    return out->sects + out->curr_context->sect;
}

size_t execOutFindSect(ExecOutput* out, const char* name) {
    ExecOutSection* prev_res = nullptr;
    for (ExecOutSection* sect = out->sects + out->sect_count-1; sect >= out->sects; sect--){
        if (strcmp(sect->name, name) == 0) {
            if (!(sect->in_use)){
                return (size_t)(sect - out->sects);
            }
            else {
                prev_res = sect;
            }
        }
    }
    if (prev_res == nullptr)
        return (size_t)-1;
    if (!execoutAddSection(out, prev_res->name, prev_res->attr))
        return (size_t)-1;
    return out->sect_count-1; // this newly added duplicate section
}

bool execOutWriteSection(ExecOutSection* sect, size_t dt_size, const void* dt_val, size_t val_size){
    if (val_size > dt_size){
        error_log("Error: write more bytes than allocate (%ld vs %ld) (sect %s)\n", val_size, dt_size, sect->name);
        return false;
    }
    if (sect->attr.fill_type != BIN_SECTION_INITIALISED) {
        if (val_size > 0) {
            warn_log("Warning: data write to uninitialised section (%s). Ignoring.", sect->name);
        }
        sect->size += dt_size;
        return true;
    }
    if (sect->size + dt_size > sect->capacity) {
        void* new_data = realloc(sect->data, (sect->capacity + dt_size)*2);
        CHECK_BOOL(new_data);
        sect->data = new_data;
        sect->capacity = (sect->capacity + dt_size)*2;
    }
    memcpy(((char*)sect->data) + sect->size           , dt_val, val_size);
    memset(((char*)sect->data) + sect->size + val_size, 0, dt_size - val_size);
    sect->size += dt_size;
    return true;
}

bool execoutAddSection(ExecOutput* out, const char* name, BinSectionAttr attr, size_t size ) {
    if (out->sect_count == 0){
        assert(out->sects == nullptr);
        out->sects = (ExecOutSection*)calloc(1, sizeof(ExecOutSection));
        CHECK_BOOL(out->sects);
    }
    else {
        void* new_sects = realloc(out->sects, (out->sect_count + 1)*sizeof(*(out->sects)));
        CHECK_BOOL(new_sects)
        out->sects = (ExecOutSection*)new_sects;
    }
    if (attr.fill_type == BIN_SECTION_INITIALISED) {
        out->sects[out->sect_count].data = calloc(init_sect_size, sizeof(char));
        CHECK_BOOL(out->sects[out->sect_count].data);
        out->sects[out->sect_count].capacity = init_sect_size;
    }
    else {
        out->sects[out->sect_count].capacity = (size_t)-1;
    }
    out->sects[out->sect_count].size = 0;
    out->sects[out->sect_count].attr = attr;
    out->sects[out->sect_count].name = name;
    out->sect_count++;
    return true;
}

bool execOutHandleTableInstr(ExecOutput* out, const void* instr_ptr) {
    CompilerInstrHeader* header = ((CompilerInstrHeader*)instr_ptr);
    CHECK_BOOL(header->type == COUT_TYPE_TABLE)

    instr_ptr = ((char*)instr_ptr) + sizeof(CompilerInstrHeader);

    compilerTableInstr_t instr = *((compilerTableInstr_t*)instr_ptr);
    instr_ptr = ((char*)instr_ptr) + sizeof(compilerTableInstr_t);
    const char* name_ptr = (char*)instr_ptr;
    instr_ptr = ((char*)instr_ptr) + strlen(name_ptr) + 1;

    if (instr == COUT_TABLE_SECT) {
        ExecOutSection* sect = out->sects + execOutFindSect(out, (char*)instr_ptr);
        CHECK_BOOL(sect)

        instr_ptr = ((char*)instr_ptr) + strlen((char*)instr_ptr) + 1;
        size_t dt_size = *((size_t*)instr_ptr);
        instr_ptr = ((char*)instr_ptr) + sizeof(dt_size);
        return cmpoutSymTablePut(&(out->syms), {{sect->size, (size_t)(sect - out->sects), {1, 0}}, name_ptr, out->lvl, 0})
            && execOutWriteSection(sect, dt_size, instr_ptr, header->size - (((char*)instr_ptr) - ((char*)header)));
    }


    switch (instr){
        case COUT_TABLE_STACK:
            if (out->curr_context->stk_size_beg != out->curr_context->stk_size_curr){
                warn_log("Stack var def over data present in stack. This is modt probably not ok\n");
            }
            if (out->curr_context->type != EOUT_CONTEXT_CB) {
                error_log("Stack vars have to be inside CB context\n");
                return false;
            }
            out->curr_context->stk_size_curr += *(size_t*)instr_ptr;
            CHECK_BOOL(cmpoutSymTablePut(&(out->syms), {{-sizeof(COMPILER_NATIVE_TYPE)*(out->curr_context->stk_size_curr) , (size_t)-1, 0}, name_ptr, out->lvl, 0}));
            
            return true;
        case COUT_TABLE_ADDSECT:
            return execoutAddSection(out, name_ptr, *((BinSectionAttr*)(instr_ptr)));
        case COUT_TABLE_SIMPLE:
            return cmpoutSymTablePut(&(out->syms), {{*((COMPILER_NATIVE_TYPE*)instr_ptr), (size_t)-1, 0}, name_ptr, out->lvl, 0});
        case COUT_TABLE_HERE:
            return cmpoutSymTablePut(&(out->syms), {{(out->sects + out->curr_context->sect)->size, out->curr_context->sect, {1,0}}, name_ptr, out->lvl, 0});
        case COUT_TABLE_PRE:
            error_log("NYI\n");
            return false;
        default:
            Error_log("Unknown table instr %d\n", instr);
            return false;
    }
    assert(0);
}

bool execOutHandleStructInstr(ExecOutput* out, const void* instr_ptr){

    CompilerInstrHeader* header = ((CompilerInstrHeader*)instr_ptr);
    CHECK_BOOL(header->type == COUT_TYPE_STRUCT)

    instr_ptr = ((char*)instr_ptr) + sizeof(CompilerInstrHeader);

    compilerStructureInstr_t instr = *((compilerStructureInstr_t*)instr_ptr);
    instr_ptr = ((char*)instr_ptr) + sizeof(instr);

    if (instr & COUT_STRUCT_END_MASK) {
        execOutContextType_t req_context = EOUT_CONTEXT_INVALID;
        switch (instr) {
            case COUT_STRUCT_RMSF:
                req_context = EOUT_CONTEXT_SF;
                break; //no special actions required. Stack is cleaned 'automatically'
            case COUT_STRUCT_CB_E:
                req_context = EOUT_CONTEXT_CB;
                break;
            case COUT_STRUCT_SECT_E:
                req_context = EOUT_CONTEXT_SECT;
                if (out->curr_context->stk_size_curr != out->curr_context->stk_size_beg) {
                    warn_log("WARNING: section %s left trash on stack.\n", getCurrSect(out)->name);
                }
                out->sects[out->curr_context->sect].in_use = false;
                break;
            case COUT_STRUCT_NONLIN_E:
                req_context = EOUT_CONTEXT_NONLIN;
                if (out->curr_context->stk_size_curr != out->curr_context->stk_size_beg) {
                    warn_log("WARNING: nonlin left trash on stack.\n");
                }
                break;
            default:
                error_log("Unknown context end instr: %d", instr);
                return false;
        }

        if (req_context != out->curr_context->type) {
            error_log("Context type mismatch: %s ended as %s\n",
                        execOutContextTypeString(out->curr_context->type),
                        execOutContextTypeString(req_context));
            return false;
        }
        cmpoutSymTableDescendLvl(&(out->syms), out->lvl);
        out->lvl--;
        return execOutPopContext(out);
    }

    if (instr == COUT_STRUCT_NULL) {
        return true;
    }

    ExecOutContext new_context = {};
    new_context.sect = out->curr_context->sect;
    new_context.stk_size_curr = out->curr_context->stk_size_curr;
    new_context.stk_size_beg  = out->curr_context->stk_size_curr;
    
    switch (instr) {
        case COUT_STRUCT_ADDSF:
            new_context.type = EOUT_CONTEXT_SF;
            new_context.stk_size_beg  = 0;
            new_context.stk_size_curr = 0;
            break;
        case COUT_STRUCT_NCB_B:
        /* fallthrough */
        case COUT_STRUCT_RCB_B:
            new_context.type = EOUT_CONTEXT_CB;
            break;
        case COUT_STRUCT_SECT_B:
            new_context.type = EOUT_CONTEXT_SECT;
            new_context.sect = execOutFindSect(out, ((const char*)instr_ptr));
            new_context.stk_size_beg  = 0;
            new_context.stk_size_curr = 0;
            if (new_context.sect == (size_t)-1){
                error_log("Section %s not found", ((const char*)instr_ptr));
                return false;
            }
            out->sects[new_context.sect].in_use = true;
            break;
        case COUT_STRUCT_NONLIN_B:
            new_context.type = EOUT_CONTEXT_NONLIN;
            break;
        default:
            error_log("Undefined structural instruction %d\n", instr);
            return false;
    }

    out->lvl++;
    execOutPushContext(out, &new_context);

    return true;
}

bool execOutPutSym(ExecOutput* out, const char* name, int size) {
    if (out->curr_context->sect == (size_t)-1){
        error_log("Attempt to write data to no section\n");
        return false;
    }

    CmpoutSymEntry* sym = cmpoutSymTableGet(&(out->syms), name);

    if (!sym) {
        error_log("Symbol %s undefined\n", name);
        return false;
    }

    if (sym->value.attr.relocatable) {
        RelocationEntry entry = {out->curr_context->sect, (out->sects + out->curr_context->sect)->size, sym->value.section_id, size};
        CHECK_BOOL(relocationTableAdd(&(out->relt), &entry))
    }
    return (execOutWriteSection(out->sects + out->curr_context->sect, size, &(sym->value.value), size));
}

bool execOutPutData(ExecOutput* out, const void* data, size_t size){
    if (out->curr_context->sect == (size_t)-1){
        error_log("Attempt to write data to no section\n");
        return false;
    }
    return execOutWriteSection(out->sects + out->curr_context->sect, size, data, size);
}

bool execOutPrepareCode(ExecOutput* out, const void* prefix, size_t prefsize, const void* suffix, size_t sufsize) {
    size_t all_size = prefsize;
    BinSectionAttr common_attr = {BIN_SECTION_INITIALISED,1,0,0,1};
    printf("%lu\n", out->sect_count);
    size_t* sect_reloc_offs = (size_t*)calloc(out->sect_count, sizeof(*sect_reloc_offs));
    ExecOutSection* replaced_sect = nullptr;

    for (ExecOutSection* sect = out->sects; sect < out->sects + out->sect_count; sect++){
        if (sect->attr.entry_point) {
            common_attr.readable |= sect->attr.readable;
            common_attr.writable |= sect->attr.writable;
            sect_reloc_offs[sect-out->sects] = all_size;
            all_size += sect->size;
            replaced_sect = sect;
        }
    }

    all_size += sufsize;
    
    ExecOutSection new_sect;
    new_sect.data = calloc(all_size, sizeof(char));
    CHECK_BOOL(new_sect.data);
    new_sect.name = "_ALL_CODE_";
    new_sect.capacity = all_size;
    new_sect.attr = common_attr;

    memcpy(new_sect.data, prefix, prefsize);
    new_sect.size = prefsize;

    for (ExecOutSection* sect = out->sects; sect < out->sects + out->sect_count; sect++){
        if (sect->attr.entry_point) {
            memcpy(((char*)new_sect.data) + new_sect.size, sect->data, sect->size);
            new_sect.size += sect->size;
            sect->size = 0;
            sect->capacity = 0;
            free(sect->data);
            sect->data = nullptr;
        }
    }
    size_t repl_sect_id = replaced_sect - out->sects;
    memcpy(((char*)new_sect.data) + new_sect.size, suffix, sufsize);
    new_sect.size += sufsize;

    for (RelocationEntry* rel = out->relt.data + out->relt.size; rel >= out->relt.data; rel--){
        if (out->sects[rel->sect_id].attr.entry_point){
            rel->sect_id = repl_sect_id;
            rel->sect_offset += sect_reloc_offs[rel->sect_id];
            out->sects[rel->sect_id].attr.entry_point = 0;
        }
        if (out->sects[rel->rel_to_sect].attr.entry_point){
            assert(rel->size <= 8);
            size_t* rel_ptr = (size_t*)(((char*)out->sects[rel->sect_id].data) + rel->sect_offset);
            *rel_ptr = ((*rel_ptr + sect_reloc_offs[rel->rel_to_sect]) & (-1L >> (8*(8-rel->size))))
                    | ((*rel_ptr) & (-1L >> rel->size));
        }
    }
    memcpy(replaced_sect, &new_sect, sizeof(new_sect));
    return true;
}