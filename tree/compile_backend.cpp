#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "lib/logging.h"
#include "lib/bintree.h"
#include "program_tables_v2.h"
#include "compiler_out_lang/emmit.h"

#define F_ARGS(...) out, __VA_ARGS__ , objs, pos, expr
#define F_DEF_ARGS_BASE CompilationOutput* out, BinTreeNode* expr , ProgramNameTable* objs, ProgramPosData* pos, BinTreeNode* oexpr
#define F_DEF_ARGS F_DEF_ARGS_BASE, int req_val

#define CHECK_ERR(...) { \
    c_ret = __VA_ARGS__; \
    if (c_ret < 0)       \
        return c_ret;    \
    if (c_ret > 0)       \
        ret = c_ret;     \
}

#define CHECK_BOOL(...) { \
    if(!(__VA_ARGS__)) {          \
        error_log("Fail: %d %s", __LINE__, #__VA_ARGS__);\
        return -1;        \
    }                     \
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
    mem_acc_attr.mod_bp  = true;
    mem_acc_attr.mem_acc = true;
    mem_acc_attr.mod_arg = false;

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
    var_data.bsize = 1;
    var_data.elcnt = 1;
    var_data.attr = {VAL_ACC_NORMAL, true, false, false};

    if (!programCreateVar(objs, pos, expr->data.name, &var_data, false))
        return -1;
    if (!emmitTableStackVar(out, var_data.lbl, var_data.bsize * var_data.elcnt))
        return -1;
    return ret;
}

static int compileCodeBlock(F_DEF_ARGS, bool force = false){
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

    if (!force && (expr->data.type != EXPR_OP || expr->data.op != EXPR_O_ENDL)){
        return compileCode(F_ARGS(expr), req_val);
    }

    int ret = 0, c_ret = 0;
    
    assert_ret(programAscendLvl(objs, pos), -1);
    pos->lvl++;

    if (req_val == 0){
        CHECK_BOOL(emmitStructuralInstruction(out, COUT_STRUCT_NCB_B));
    }
    else{
        CHECK_BOOL(emmitStructuralInstruction(out, COUT_STRUCT_RCB_B));
    }

    CHECK_ERR(compileCode(F_ARGS(expr), req_val))

    assert_ret(programDescendLvl(objs, pos),-1);
    pos->lvl--;
    
    CHECK_BOOL(emmitStructuralInstruction(out, COUT_STRUCT_CB_E, 0, req_val));
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
            CHECK_BOOL(emmitGenericInstruction(out, COUT_GINSTR_OUT));;
        }
        for (int i = 0; i < req_val; i++){
            CHECK_BOOL(emmitGenericInstruction(out, COUT_GINSTR_IN));
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
            CHECK_BOOL(emmitGenericInstruction(out, COUT_GINSTR_IN));
        }
        return ret;
    }
    if (expr->data.kword == EXPR_KW_TRAP){
        if (write_c != req_val){
            COMPILATION_ERROR("Bad use of trap\n");
            return 1;
        }
        CHECK_BOOL(emmitGenericInstruction(out, COUT_GINSTR_TRAP));
        return ret;
    }
    if (expr->data.kword == EXPR_KW_BAD){
        CHECK_BOOL(emmitGenericInstruction(out, COUT_GINSTR_BAD));
        return ret;
    }

    COMPILATION_ERROR("Bad KW\n")
    return 1;
}

static int compileConstNode(F_DEF_ARGS) {
    assert(expr);
    assert(expr->data.type == EXPR_CONST);
    if (req_val == 0){
        return 0;
    }
    CHECK_BOOL(emmitConstInstruction(out, expr->data.val));
    for (int i = 1; i < req_val; i++) {
        CHECK_BOOL(emmitGenericInstruction(out, COUT_GINSTR_DUP));
    }
    return 0;
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

static int compileGenericOpArgs(F_DEF_ARGS_BASE){
    assert(expr);
    assert(expr->data.type == EXPR_OP);

    int ret = 0, c_ret = 0;
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

    return ret;
}

static int compileMathOp(F_DEF_ARGS){
    int ret = 0, c_ret = 0;
    if (!req_val){
        COMPILATION_WARN("Math op without requested value will be ignored")
        return 0;
    }

    CHECK_ERR(compileGenericOpArgs(F_ARGS(expr)));

    switch(expr->data.op){
        case EXPR_MO_ADD:
            CHECK_BOOL(emmitGenericInstruction(out, COUT_GINSTR_ADD));
            break;
        case EXPR_MO_SUB:
            CHECK_BOOL(emmitGenericInstruction(out, COUT_GINSTR_SUB));
            break;
        case EXPR_MO_MUL:
            CHECK_BOOL(emmitGenericInstruction(out, COUT_GINSTR_MUL));
            break;
        case EXPR_MO_DIV:
            CHECK_BOOL(emmitGenericInstruction(out, COUT_GINSTR_DIV));
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

static int compileLogicalExpr(F_DEF_ARGS_BASE, compilerFlagCondition_t* cond) {
    int ret = 0, c_ret = 0;
    if (!expr) {
        COMPILATION_WARN("Requesting logical value of nothingness. False by default\n");
        *cond = COUT_FLAGS_NVR;
    }
    if (expr->data.type != EXPR_OP || !isLogicalOp(expr->data.op)) {
        if (expr->data.type == EXPR_CONST){
            *cond = (expr->data.val != 0) ? COUT_FLAGS_ALW : COUT_FLAGS_NVR;
            return ret;
        }
        if (expr->data.type == EXPR_OP && expr->data.op == EXPR_MO_BAND) {
            CHECK_ERR(compileGenericOpArgs(F_ARGS(expr)));
            CHECK_BOOL(emmitGenericInstruction(out, COUT_GINSTR_TST));
            *cond = COUT_FLAGS_NE;
            return ret;
        }

        CHECK_ERR(compileCodeBlock(F_ARGS(expr),1));
        CHECK_BOOL(emmitGenericInstruction(out, COUT_GINSTR_DUP));
        CHECK_BOOL(emmitGenericInstruction(out, COUT_GINSTR_TST));
        return ret;
    }

    if (expr->data.op == EXPR_MO_BOOL) {
        return compileLogicalExpr(F_ARGS(expr->right), cond);
    }

    CHECK_ERR(compileGenericOpArgs(F_ARGS(expr)));

    switch (expr->data.op){
        case EXPR_MO_CEQ:
            CHECK_BOOL(emmitGenericInstruction(out, COUT_GINSTR_CMP));
            *cond = COUT_FLAGS_EQ;
            break;
        case EXPR_MO_CNE:
            CHECK_BOOL(emmitGenericInstruction(out, COUT_GINSTR_CMP));
            *cond = COUT_FLAGS_NE;
            break;
        case EXPR_MO_CGT:
            CHECK_BOOL(emmitGenericInstruction(out, COUT_GINSTR_CMP));
            *cond = COUT_FLAGS_GT;
            break;
        case EXPR_MO_CLE:
            CHECK_BOOL(emmitGenericInstruction(out, COUT_GINSTR_CMP));
            *cond = COUT_FLAGS_LE;
            break;
        case EXPR_MO_CLT:
            CHECK_BOOL(emmitGenericInstruction(out, COUT_GINSTR_CMP));
            *cond = COUT_FLAGS_LT;
            break;
        case EXPR_MO_CGE:
            CHECK_BOOL(emmitGenericInstruction(out, COUT_GINSTR_CMP));
            *cond = COUT_FLAGS_GE;
            break;
        default:
            COMPILATION_ERROR("Unknown logical op: %s", exprOpName(expr->data.op));
            return 1;
    }
    return ret;
}

static int compileMiscOp(F_DEF_ARGS){
    assert(expr);
    assert(expr->data.type == EXPR_OP);
    int ret = 0, c_ret = 0;

    if (expr->data.op == EXPR_O_IF) {
        char* if_end_lbl = (char*)calloc(strlen("IF_AABBCCDDEEFFGGHH_END."), sizeof(char));
        sprintf(if_end_lbl, "IF_%lX_END", pos->lbl_id);
        compilerFlagCondition_t flags = COUT_FLAGS_NVR;
        CHECK_ERR (compileLogicalExpr(F_ARGS(expr->left), &flags));

        CHECK_BOOL(emmitStructuralInstruction(out, COUT_STRUCT_NONLIN_B));
        CHECK_BOOL(emmitTablePrototype(out, if_end_lbl));
        CHECK_BOOL(emmitJumpInstruction_l(out, invertFlagCondition(flags), {}, if_end_lbl));
        CHECK_ERR (compileCodeBlock(F_ARGS(expr->right), 0, true))
        CHECK_BOOL(emmitTableLbl(out, if_end_lbl));
        CHECK_BOOL(emmitStructuralInstruction(out, COUT_STRUCT_NONLIN_E));

        return ret;
    }

    COMPILATION_ERROR("Unknown misc op for code: %s\n", exprOpName(expr->data.op));
    return false;
}

static int compileCode(F_DEF_ARGS){
    int ret = 0, c_ret = 0;
    if (!expr) {
        if(req_val == 0)
            return 0;
        else{
            COMPILATION_ERROR("Empty expression can not return any values\n");
            return 1;
        }
    }

    if (expr->data.type == EXPR_CONST)
        return compileConstNode(F_ARGS(expr), req_val);

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
                return compileMiscOp(F_ARGS(expr), req_val);
        }
    }    

    COMPILATION_ERROR("Bad elem for code\n")
    return 1;
}

int compileProgram(CompilationOutput* out, BinTreeNode* code) {
    int ret = 0, c_ret = 0;
    ProgramNameTable objs = {};
    ProgramPosData pos = {};
    programNameTableCtor(&objs);
    programPosDataCtor(&pos);

    CHECK_BOOL(emmitAddSection(out, "main", BIN_SECTION_PRESET_MAIN)); // register main code section
    CHECK_BOOL(emmitSetSection(out, "main"));                          // set as current
    CHECK_BOOL(emmitStructuralInstruction(out, COUT_STRUCT_ADDSF));    // set stack frame

    CHECK_ERR(compileCodeBlock(out, code, &objs, &pos, code, 0));

    CHECK_BOOL(emmitStructuralInstruction(out, COUT_STRUCT_RMSF  ));    // end stack frame
    CHECK_BOOL(emmitStructuralInstruction(out, COUT_STRUCT_SECT_E));    // end of main

    programPosDataDtor(&pos);
    programNameTableDtor(&objs);
    return ret;
}