#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "lib/logging.h"
#include "lib/bintree.h"
#include "program_tables_v2.h"
#include "compiler_out_lang/emmit.h"

#define F_ARGS(...) out, __VA_ARGS__ , objs, pos, expr
#define F_DEF_ARGS CompilationOutput* out, BinTreeNode* expr , ProgramNameTable* objs, ProgramPosData* pos, BinTreeNode* oexpr, int req_val

#define CHECK_ERR(...) { \
    c_ret = __VA_ARGS__; \
    if (c_ret < 0)       \
        return c_ret;    \
    if (c_ret > 0)       \
        ret = c_ret;     \
}

#define COMPILATION_ERROR(...) \
{error_log(__VA_ARGS__);        \
printf_log("at:'");           \
printExprElem(stderr  , expr->data);\
printExprElem(_logfile, expr->data);\
printf_log("' (%s:%d:%d)\n", expr->data.file_name, expr->data.file_line, expr->data.line_pos);\
}

#define COMPILATION_WARN(...) \
{warn_log(__VA_ARGS__);        \
printf_log("at:'");           \
printExprElem(stderr  , expr->data);\
printExprElem(_logfile, expr->data);\
printf_log("' (%s:%d:%d)\n", expr->data.file_name, expr->data.file_line, expr->data.line_pos);\
}

static int compileCode(F_DEF_ARGS);

static int compileVarAcc(CompilationOutput* out, VarEntry* var, ProgramNameTable* objs, ProgramPosData* pos, BinTreeNode* expr, bool write){
    if (!var) {
        COMPILATION_ERROR("Var \"%s\" not found\n", expr->data.name)
        return 1;
    }

    CompilerMemArgAttr mem_acc_attr = {};
    mem_acc_attr.mod_bp = true;
    mem_acc_attr.volat  = var->value.attr.acc_type == VAL_ACC_VOLATILE;
    mem_acc_attr.const_ = var->value.attr.acc_type == VAL_ACC_CONST;

    if (!emmitMemInstruction_l(out, mem_acc_attr , write, var->value.lbl))
        return -1;

    return 0;
}

static int compileVarDef(F_DEF_ARGS){
    int ret = 0, c_ret = 0;
    if (!expr)
        return 0;
    if (expr->data.type == EXPR_OP){
        if (expr->data.op != EXPR_O_COMMA){
            COMPILATION_WARN("Unusual separator in var def\n")
        }
        CHECK_ERR(compileVarDef(F_ARGS(expr->left ), 0))
        CHECK_ERR(compileVarDef(F_ARGS(expr->right), 0))
        return ret;
    }

    VarData var_data = {};
    var_data.bsize = sizeof(COMPILER_NATIVE_TYPE);
    var_data.elcnt = 1;
    var_data.attr = {VAL_ACC_NORMAL, true, false, false};

    if (!programCreateVar(objs, pos, expr->data.name, &var_data, false))
        return -1;
    if (!emmitTableStackVar(out, var_data.lbl, var_data.bsize * var_data.elcnt))
        return -1;
    return ret;
}

static int compileCodeBlock(F_DEF_ARGS){
    if (expr == nullptr) {
        if (req_val == 0) {
            return 0;
        }
        else{
            expr = oexpr;
            COMPILATION_ERROR("Empty expression can not return any values\n");
            return 1;
        }
    }

    if (expr->data.type != EXPR_OP || expr->data.op != EXPR_O_ENDL){
        return compileCode(F_ARGS(expr), req_val);
    }

    int ret = 0, c_ret = 0;
    
    assert_ret(programAscendLvl(objs, pos), -1);
    pos->lvl++;

    if (req_val == 0){
        emmitStructuralInstruction(out, COUT_STRUCT_NCB_B);
    }
    else{
        emmitStructuralInstruction(out, COUT_STRUCT_RCB_B);
    }

    CHECK_ERR(compileCode(F_ARGS(expr), req_val))

    assert_ret(programDescendLvl(objs, pos),-1);
    pos->lvl--;
    
    emmitStructuralInstruction(out, COUT_STRUCT_CB_E);
    return ret;
}

static int compileVarNode(F_DEF_ARGS, int write_c){
    int ret = 0, c_ret = 0;
    VarEntry* var = varTableGet(objs->vars, expr->data.name);
    FuncEntry* func = programGetFunc(objs, pos, {expr->data.name, write_c, req_val, true});

    if (!var && !func){
        COMPILATION_ERROR("No suitable var or Vfunc found\n")
    }

    if (var && func){
        if(var->depth >= func->depth)
            func = nullptr;
    }

    if (func){
        COMPILATION_ERROR("NYI\n")
        return 1;
    }
    else{
        if (var->value.attr.acc_type == VAL_ACC_CONST && write_c > 0){
            COMPILATION_ERROR("Var %s is constant\n", var->name)
        }

        for (int i = 0; i < write_c; i++){
            CHECK_ERR(compileVarAcc(out, var, objs, pos, expr, true));
        }
        for (int i = 0; i < req_val; i++){
            CHECK_ERR(compileVarAcc(out, var, objs, pos, expr, false));
        }
        return ret;
    }
}

static int compileKVarNode(F_DEF_ARGS, int write_c) {
    int ret = 0;
    if (expr->data.kword == EXPR_KW_CIO) {
        for (int i = 0; i < write_c; i++){
            if (!emmitGenericInstruction(out, COUT_GINSTR_OUT))
                return -1;
        }
        for (int i = 0; i < req_val; i++){
            if (!emmitGenericInstruction(out, COUT_GINSTR_IN))
                return -1;
        }
        return ret;
    }
    if (expr->data.kword == EXPR_KW_TEMP)
        return ret;
    if (expr->data.kword == EXPR_KW_NULL){
        if (req_val > 0){
            COMPILATION_ERROR("Nothing to read\n")
        }
        for (int i = 0; i < write_c; i++){
            if (!emmitGenericInstruction(out, COUT_GINSTR_IN))
                return -1;
        }
        return ret;
    }
    if (expr->data.kword == EXPR_KW_TRAP){
        if (write_c != req_val){
            COMPILATION_ERROR("Bad use of trap\n");
            return 1;
        }
        if (!emmitGenericInstruction(out, COUT_GINSTR_TRAP))
            return -1;
        return ret;
    }
    if (expr->data.kword == EXPR_KW_BAD){
        if (!emmitGenericInstruction(out, COUT_GINSTR_BAD))
            return -1;
        return ret;
    }

    COMPILATION_ERROR("Bad KW\n")
    return 1;
}

static int compileSetDst(F_DEF_ARGS, int write_c){
    int ret = 0, c_ret = 0;
    if (!expr) {
        COMPILATION_ERROR("Empty expression is invalid assignment target\n")
        return 1;
    }

    if (expr->data.type == EXPR_OP) {
        switch (expr->data.op){
            case EXPR_O_CER:
            case EXPR_O_COMMA:
                if (write_c > 1){
                    COMPILATION_ERROR("NYI\n")
                    return 1;
                }
                if (!emmitGenericInstruction(out, COUT_GINSTR_DUP))
                    return -1;
                CHECK_ERR(compileSetDst(F_ARGS(expr->left), 0, write_c))
                CHECK_ERR(compileSetDst(F_ARGS(expr->left), req_val, write_c))
                break;
            case EXPR_O_CEL:
                if (write_c > 1){
                    COMPILATION_ERROR("NYI\n")
                    return 1;
                }
                if (!emmitGenericInstruction(out, COUT_GINSTR_DUP))
                    return -1;
                CHECK_ERR(compileSetDst(F_ARGS(expr->left), req_val, write_c))
                CHECK_ERR(compileSetDst(F_ARGS(expr->left), 0      , write_c))
                break;
            default:
            COMPILATION_ERROR("Incorrect op for assignment target\n")
            return 1;
        }
        return ret;
    }

    if (expr->data.type == EXPR_VAR) {
        return compileVarNode(F_ARGS(expr), req_val, write_c);
    }
    if (expr->data.type == EXPR_KVAR) {
        return compileKVarNode(F_ARGS(expr), req_val, write_c);
    }
    if (expr->data.type == EXPR_CONST) {
        COMPILATION_ERROR("Constant is constant\n");
        return 1;
    }

    COMPILATION_ERROR("Bad set target\n");
    return 1;
}

static int compileAssignment(F_DEF_ARGS){
    assert(expr);
    assert(expr->data.type == EXPR_OP);
    assert(isAssignOp(expr->data.op));
    int ret = 0, c_ret = 0;


    CHECK_ERR(compileCodeBlock(F_ARGS(expr->right), 1))

    if (expr->data.op == EXPR_O_EQARTL) {
        CHECK_ERR(compileCodeBlock(F_ARGS(expr->left), 1))
        if (!emmitGenericInstruction(out, COUT_GINSTR_ADD))
            return -1;
    }
    CHECK_ERR(compileSetDst(F_ARGS(expr->left), req_val, 1))
    return ret;
}

static int compileMathOp(F_DEF_ARGS){
    int ret = 0, c_ret = 0;
    if (!req_val){
        COMPILATION_WARN("Math op without requested value will be ignored")
        return 0;
    }
    
    if ((!canExprOpBeUnary(expr->data.op)) && (!expr->left)){
        COMPILATION_ERROR("Binary-only op used as unary");
        return 1;
    }
    if ((isExprOpUnary(expr->data.op)) && (expr->left)){
        COMPILATION_ERROR("Unary-only op used as binary");
        return 1;
    }

    if (expr->left){
        CHECK_ERR(compileCodeBlock(F_ARGS(expr->left), true))
    }
    CHECK_ERR(compileCodeBlock(F_ARGS(expr->right), true))

    switch(expr->data.op){
        case EXPR_MO_ADD:
            emmitGenericInstruction(out, COUT_GINSTR_ADD);
            break;
        case EXPR_MO_SUB:
            emmitGenericInstruction(out, COUT_GINSTR_SUB);
            break;
        case EXPR_MO_MUL:
            emmitGenericInstruction(out, COUT_GINSTR_MUL);
            break;
        case EXPR_MO_DIV:
            emmitGenericInstruction(out, COUT_GINSTR_DIV);
            break;
        /*
        case EXPR_MO_POW:
            break;
        case EXPR_MO_SQRT:
            break;
        case EXPR_MO_TANP:
            break;
        case EXPR_MO_BOOL:
            break;
        case EXPR_MO_BOR:
            break;
        case EXPR_MO_BAND:
            break;
        case EXPR_MO_BXOR:
            break;
        case EXPR_MO_BSL:
            break;
        case EXPR_MO_BSR:
            break;
        case EXPR_MO_UMIN:
            break;
        */
        default:
            COMPILATION_ERROR("Bad math op");
            return 1;
    }
    return ret;
}

int compileCode(F_DEF_ARGS){
    int ret = 0, c_ret = 0;
    if (!expr) {
        if(req_val == 0)
            return 0;
        else{
            COMPILATION_ERROR("Empty expression can not return any values\n");
            return 1;
        }
    }

    if (expr->data.type == EXPR_VAR)
        return compileVarNode(F_ARGS(expr), req_val, 0);

    if (expr->data.type == EXPR_KVAR)
        return compileKVarNode(F_ARGS(expr), req_val, 0);

    if (expr->data.type == EXPR_OP){
        if (isMathOp(expr->data.op))
            return compileMathOp(F_ARGS(expr), req_val);

        if (isAssignOp(expr->data.op))
            return compileAssignment(F_ARGS(expr), req_val);

        switch (expr->data.op) {
            case EXPR_O_ENDL:
                if (req_val > 0) {
                    COMPILATION_WARN("Strange behavior when compiling code. Non-block ';' val req\n")
                    return compileCodeBlock(F_ARGS(expr), req_val);
                }
                CHECK_ERR(compileCode     (F_ARGS(expr->left ), 0))
                CHECK_ERR(compileCodeBlock(F_ARGS(expr->right), 0))
                return ret;
            case EXPR_O_VDEF:
                return compileVarDef(F_ARGS(expr->right), 0);

            default:
                COMPILATION_ERROR("Unknown op for code\n")
                return 1;
        }
    }    

    COMPILATION_ERROR("Bad elem for code\n")
    return 1;
}

bool compileProgram(CompilationOutput* out, BinTreeNode* code) {
    bool ret = 0;
    ProgramNameTable objs = {};
    ProgramPosData pos = {};
    programNameTableCtor(&objs);
    programPosDataCtor(&pos);
    emmitAddSection(out, "main", BIN_SECTION_PRESET_MAIN); //register main code section
    emmitSetSection(out, "main");                          // set as current

    compileCodeBlock(out, code, &objs, &pos, code, 0);

    programPosDataDtor(&pos);
    programNameTableDtor(&objs);
    return ret;
}