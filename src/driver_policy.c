/* SPDX-License-Identifier: Apache-2.0 */

#include "driver_policy.h"

#include "ast.h"
#include "rcc.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#define DRIVER_TYPE_DEPTH_LIMIT 128u

static bool driver_type_uses_fpu_inner(Type* type, Type** active,
                                       size_t active_count)
{
    TypeField* field;
    TypeParam* parameter;
    size_t index;

    if (!type) return false;
    if (type->kind == TYPE_FLOAT || type->kind == TYPE_DOUBLE) return true;
    for (index = 0u; index < active_count; ++index) {
        if (active[index] == type) return false;
    }
    if (active_count == DRIVER_TYPE_DEPTH_LIMIT) {
        /* A type graph beyond the bounded audit cannot be proven safe. */
        return true;
    }
    active[active_count++] = type;

    switch (type->kind) {
        case TYPE_PTR:
        case TYPE_ARRAY:
            return driver_type_uses_fpu_inner(type->base, active, active_count);
        case TYPE_FUNC:
            if (driver_type_uses_fpu_inner(type->ret_type, active,
                                           active_count)) return true;
            for (parameter = type->params; parameter; parameter = parameter->next) {
                if (driver_type_uses_fpu_inner(parameter->type, active,
                                               active_count)) return true;
            }
            return false;
        case TYPE_STRUCT:
        case TYPE_UNION:
            for (field = type->fields; field; field = field->next) {
                if (driver_type_uses_fpu_inner(field->type, active,
                                               active_count)) return true;
            }
            return false;
        default:
            return false;
    }
}

static bool driver_type_uses_fpu(Type* type)
{
    Type* active[DRIVER_TYPE_DEPTH_LIMIT];
    return driver_type_uses_fpu_inner(type, active, 0u);
}

static bool driver_reject_type(Type* type, SourceLoc location)
{
    if (!driver_type_uses_fpu(type)) return false;
    rcc_error(location,
              "driver mode forbids floating-point types and FPU/SIMD ABI state");
    return true;
}

static bool driver_register_uses_fpu(const char* name);

static bool driver_constraint_uses_fpu(const char* constraint)
{
    const unsigned char* cursor = (const unsigned char*)constraint;
    if (!cursor) return false;
    for (; *cursor; ++cursor) {
        if (*cursor == '{') {
            char reg[32];
            size_t length = 0u;
            ++cursor;
            while (*cursor && *cursor != '}' && length + 1u < sizeof(reg)) {
                reg[length++] = (char)*cursor++;
            }
            reg[length] = '\0';
            if (*cursor != '}' || driver_register_uses_fpu(reg)) return true;
            continue;
        }
        switch (*cursor) {
            case 'f': case 't': case 'u':
            case 'k': case 'v': case 'w': case 'x': case 'y':
            case 'F': case 'T': case 'U':
            case 'V': case 'W': case 'Y':
                return true;
            default:
                break;
        }
    }
    return false;
}

static bool driver_register_uses_fpu(const char* name)
{
    char normalized[32];
    size_t input = 0u;
    size_t output = 0u;

    if (!name) return false;
    while (name[input] == '%' || name[input] == '{' ||
           isspace((unsigned char)name[input])) ++input;
    while (name[input] && name[input] != '}' &&
           !isspace((unsigned char)name[input]) &&
           output + 1u < sizeof(normalized)) {
        normalized[output++] = (char)tolower((unsigned char)name[input++]);
    }
    normalized[output] = '\0';

    return strncmp(normalized, "st", 2u) == 0 ||
           strncmp(normalized, "fp", 2u) == 0 ||
           strncmp(normalized, "mm", 2u) == 0 ||
           strncmp(normalized, "xmm", 3u) == 0 ||
           strncmp(normalized, "ymm", 3u) == 0 ||
           strncmp(normalized, "zmm", 3u) == 0 ||
           strcmp(normalized, "mxcsr") == 0 ||
           (normalized[0] == 'k' && normalized[1] >= '0' &&
            normalized[1] <= '7' && normalized[2] == '\0');
}

static bool driver_mnemonic_uses_fpu(const char* mnemonic)
{
    static const char* const prefixes[] = {
        "addp", "adds", "aes", "blendp", "cmpp", "cmps", "comis",
        "cvt", "divp", "divs", "dpp", "emms", "gf2p8", "haddp",
        "hsubp", "ldmxcsr", "maxp", "maxs", "minp", "mins", "movap",
        "movdq", "movdqa", "movdqu", "movmsk", "movnt", "movup",
        "mulp", "muls", "pack", "pabs", "padd", "pand", "pavg",
        "pblend", "pclmul", "pcmp", "pextr", "phadd", "phsub",
        "pinsr", "pmax", "pmin", "pmov", "pmul", "por", "psad",
        "pshuf", "psign", "psll", "psra", "psrl", "psub", "ptest",
        "punpck", "pxor", "rcpp", "rcps", "roundp", "rounds", "rsqrt",
        "sha1", "sha256", "shufp", "sqrtp", "sqrts", "stmxcsr", "subp",
        "subs", "ucomis", "unpck", "xgetbv", "xrstor", "xsave", "xsetbv"
    };
    size_t index;

    if (!mnemonic || !mnemonic[0]) return false;
    if (mnemonic[0] == '.' || mnemonic[0] == 'f' || mnemonic[0] == 'v') {
        return true;
    }
    for (index = 0u; index < sizeof(prefixes) / sizeof(prefixes[0]); ++index) {
        size_t length = strlen(prefixes[index]);
        if (strncmp(mnemonic, prefixes[index], length) == 0) return true;
    }
    return false;
}

static bool driver_asm_template_uses_fpu(const char* text)
{
    const char* cursor = text;

    if (!cursor) return false;
    while (*cursor) {
        char mnemonic[32];
        size_t length = 0u;

        while (*cursor && (isspace((unsigned char)*cursor) ||
                           *cursor == ';')) ++cursor;
        if (!*cursor) break;

        while (cursor[length] && !isspace((unsigned char)cursor[length]) &&
               cursor[length] != ';' && cursor[length] != ',' &&
               length + 1u < sizeof(mnemonic)) {
            mnemonic[length] =
                (char)tolower((unsigned char)cursor[length]);
            ++length;
        }
        mnemonic[length] = '\0';
        if (driver_mnemonic_uses_fpu(mnemonic)) return true;

        while (*cursor && *cursor != ';' && *cursor != '\n' &&
               *cursor != '\r') {
            if (*cursor == '%') {
                char reg[32];
                size_t reg_length = 0u;
                const char* reg_start = cursor;
                while (cursor[reg_length] && cursor[reg_length] != ',' &&
                       cursor[reg_length] != ';' &&
                       !isspace((unsigned char)cursor[reg_length]) &&
                       reg_length + 1u < sizeof(reg)) ++reg_length;
                memcpy(reg, reg_start, reg_length);
                reg[reg_length] = '\0';
                if (driver_register_uses_fpu(reg)) return true;
                cursor += reg_length;
            } else {
                ++cursor;
            }
        }
    }
    return false;
}

static bool driver_validate_expr(Expr* expression);
static bool driver_validate_stmt(Stmt* statement);
static bool driver_validate_decl(Decl* declaration);

static bool driver_validate_expr_list(ExprList* list)
{
    for (; list; list = list->next) {
        if (!driver_validate_expr(list->expr)) return false;
    }
    return true;
}

static bool driver_validate_expr(Expr* expression)
{
    if (!expression) return true;
    if (driver_reject_type(expression->type, expression->loc)) return false;

    switch (expression->kind) {
        case EXPR_INT_LIT:
        case EXPR_CHAR_LIT:
        case EXPR_STRING_LIT:
        case EXPR_IDENT:
            return true;
        case EXPR_FLOAT_LIT:
            rcc_error(expression->loc,
                      "driver mode forbids floating-point literals");
            return false;
        case EXPR_NEG: case EXPR_NOT: case EXPR_BITNOT: case EXPR_ADDR:
        case EXPR_DEREF: case EXPR_PREINC: case EXPR_PREDEC:
        case EXPR_POSTINC: case EXPR_POSTDEC:
            return driver_validate_expr(expression->unary_operand);
        case EXPR_SIZEOF:
        case EXPR_ALIGNOF:
            if (driver_reject_type(expression->sizeof_type, expression->loc)) {
                return false;
            }
            return driver_validate_expr(expression->unary_operand);
        case EXPR_CAST:
            if (driver_reject_type(expression->cast_type, expression->loc)) {
                return false;
            }
            return driver_validate_expr(expression->cast_expr);
        case EXPR_ADD: case EXPR_SUB: case EXPR_MUL: case EXPR_DIV:
        case EXPR_MOD: case EXPR_BITAND: case EXPR_BITOR: case EXPR_BITXOR:
        case EXPR_LSHIFT: case EXPR_RSHIFT: case EXPR_EQ: case EXPR_NE:
        case EXPR_LT: case EXPR_GT: case EXPR_LE: case EXPR_GE:
        case EXPR_AND: case EXPR_OR: case EXPR_ASSIGN: case EXPR_ADD_ASSIGN:
        case EXPR_SUB_ASSIGN: case EXPR_MUL_ASSIGN: case EXPR_DIV_ASSIGN:
        case EXPR_MOD_ASSIGN: case EXPR_AND_ASSIGN: case EXPR_OR_ASSIGN:
        case EXPR_XOR_ASSIGN: case EXPR_LSHIFT_ASSIGN: case EXPR_RSHIFT_ASSIGN:
        case EXPR_COMMA: {
            bool valid = driver_validate_expr(expression->binary_lhs) &&
                         driver_validate_expr(expression->binary_rhs);
            if (valid && expression->kind == EXPR_ASSIGN &&
                expression->cxx_move_assignment) {
                valid = driver_validate_expr(
                            expression->cxx_move_assignment->cleanup) &&
                        driver_validate_expr(
                            expression->cxx_move_assignment->release);
            }
            return valid;
        }
        case EXPR_COND:
            return driver_validate_expr(expression->cond_test) &&
                   driver_validate_expr(expression->cond_then) &&
                   driver_validate_expr(expression->cond_else);
        case EXPR_CALL: {
            bool valid = driver_validate_expr(expression->call_func) &&
                         driver_validate_expr_list(expression->call_args);
            if (valid && expression->cxx_close_call) {
                valid = driver_validate_expr(
                    expression->cxx_close_call->cleanup);
            }
            return valid;
        }
        case EXPR_INDEX:
            return driver_validate_expr(expression->index_base) &&
                   driver_validate_expr(expression->index_expr);
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            return driver_validate_expr(expression->member_base);
        case EXPR_COMPOUND:
            if (driver_reject_type(expression->compound_type,
                                   expression->loc)) return false;
            return driver_validate_expr_list(expression->compound_init);
        case EXPR_GENERIC: {
            GenericAssociation* association;
            if (!driver_validate_expr(expression->generic_control)) {
                return false;
            }
            for (association = expression->generic_associations; association;
                 association = association->next) {
                if (driver_reject_type(association->type,
                                       association->loc) ||
                    !driver_validate_expr(association->expr)) return false;
            }
            return true;
        }
        case EXPR_VA_START:
        case EXPR_VA_COPY:
            return driver_validate_expr(expression->va_list_operand) &&
                   driver_validate_expr(expression->va_second_operand);
        case EXPR_VA_END:
            return driver_validate_expr(expression->va_list_operand);
        case EXPR_VA_ARG:
            return !driver_reject_type(expression->va_arg_type,
                                       expression->loc) &&
                   driver_validate_expr(expression->va_list_operand);
    }
    return true;
}

static bool driver_validate_asm(Stmt* statement)
{
    AsmOperand* operand;
    AsmClobber* clobber;

    if (driver_asm_template_uses_fpu(statement->asm_template)) {
        rcc_error(statement->loc,
                  "driver mode forbids FPU/SIMD instructions and raw asm data");
        return false;
    }
    for (operand = statement->asm_outputs; operand; operand = operand->next) {
        if (driver_constraint_uses_fpu(operand->constraint)) {
            rcc_error(statement->loc,
                      "driver mode forbids FPU/SIMD asm constraints");
            return false;
        }
        if (!driver_validate_expr(operand->expr)) return false;
    }
    for (operand = statement->asm_inputs; operand; operand = operand->next) {
        if (driver_constraint_uses_fpu(operand->constraint)) {
            rcc_error(statement->loc,
                      "driver mode forbids FPU/SIMD asm constraints");
            return false;
        }
        if (!driver_validate_expr(operand->expr)) return false;
    }
    for (clobber = statement->asm_clobbers; clobber; clobber = clobber->next) {
        if (driver_register_uses_fpu(clobber->reg)) {
            rcc_error(statement->loc,
                      "driver mode forbids FPU/SIMD asm clobbers");
            return false;
        }
    }
    return true;
}

static bool driver_validate_stmt_list(StmtList* list)
{
    for (; list; list = list->next) {
        if (!driver_validate_stmt(list->stmt)) return false;
    }
    return true;
}

static bool driver_validate_stmt(Stmt* statement)
{
    if (!statement) return true;
    switch (statement->kind) {
        case STMT_EXPR:
            return driver_validate_expr(statement->expr);
        case STMT_BLOCK:
            return driver_validate_stmt_list(statement->block_stmts);
        case STMT_IF:
            return driver_validate_expr(statement->if_cond) &&
                   driver_validate_stmt(statement->if_then) &&
                   driver_validate_stmt(statement->if_else);
        case STMT_WHILE:
        case STMT_DO:
            return driver_validate_expr(statement->while_cond) &&
                   driver_validate_stmt(statement->while_body);
        case STMT_FOR:
            return driver_validate_stmt(statement->for_init) &&
                   driver_validate_expr(statement->for_cond) &&
                   driver_validate_expr(statement->for_inc) &&
                   driver_validate_stmt(statement->for_body);
        case STMT_SWITCH:
            return driver_validate_expr(statement->switch_expr) &&
                   driver_validate_stmt(statement->switch_body);
        case STMT_CASE:
            return driver_validate_expr(statement->case_val) &&
                   driver_validate_stmt(statement->case_stmt);
        case STMT_DEFAULT:
            return driver_validate_stmt(statement->default_stmt);
        case STMT_RETURN:
            return driver_validate_expr(statement->return_val);
        case STMT_LABEL:
            return driver_validate_stmt(statement->label_stmt);
        case STMT_DECL:
            return driver_validate_decl(statement->decl);
        case STMT_ASM:
            return driver_validate_asm(statement);
        case STMT_BREAK:
        case STMT_CONTINUE:
        case STMT_GOTO:
        case STMT_NULL:
            return true;
    }
    return true;
}

static bool driver_validate_decl_list(DeclList* list)
{
    for (; list; list = list->next) {
        if (!driver_validate_decl(list->decl)) return false;
    }
    return true;
}

static bool driver_validate_decl(Decl* declaration)
{
    if (!declaration) return true;
    if (driver_reject_type(declaration->type, declaration->loc)) return false;

    switch (declaration->kind) {
        case DECL_VAR:
            if (declaration->var_is_thread_local) {
                rcc_error(declaration->loc,
                          "driver mode forbids thread-local storage");
                return false;
            }
            return driver_validate_expr(declaration->var_init);
        case DECL_FUNC:
            return driver_validate_decl_list(declaration->func_params) &&
                   driver_validate_stmt(declaration->func_body);
        case DECL_TYPEDEF:
            return !driver_reject_type(declaration->typedef_type,
                                       declaration->loc);
        case DECL_STRUCT:
        case DECL_UNION:
            return driver_validate_decl_list(declaration->struct_fields);
        case DECL_ENUM:
            return driver_validate_decl_list(declaration->enum_consts);
        case DECL_PARAM:
        case DECL_ENUM_CONST:
            return true;
    }
    return true;
}

bool rcc_validate_driver_policy(AST* ast)
{
    if (!ast) {
        rcc_error((SourceLoc){"<driver-policy>", 0, 0},
                  "driver policy requires a parsed translation unit");
        return false;
    }
    return driver_validate_decl_list(ast->decls);
}
