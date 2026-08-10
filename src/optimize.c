/*
 * RCC - Target-independent AST optimization passes
 */

#include "optimize.h"

#include <limits.h>

static void optimize_expr(Expr** expression);
static void optimize_stmt(Stmt* statement);

static bool integer_literal(const Expr* expression, int64_t* value) {
    if (!expression || !value) return false;
    if (expression->kind == EXPR_INT_LIT) {
        *value = expression->int_val;
        return true;
    }
    if (expression->kind == EXPR_CHAR_LIT) {
        *value = (unsigned char)expression->char_val;
        return true;
    }
    return false;
}

static bool signed_result_fits(const Expr* expression, int64_t value) {
    Type* type = expression ? expression->type : NULL;
    int bits;
    int64_t minimum;
    int64_t maximum;
    if (!type || !type_is_integer(type) || type->is_unsigned) return false;
    if (type->kind == TYPE_BOOL) return value == 0 || value == 1;
    bits = type->size * 8;
    if (bits <= 0 || bits > 64) return false;
    if (bits == 64) return true;
    minimum = -(INT64_C(1) << (bits - 1));
    maximum = (INT64_C(1) << (bits - 1)) - 1;
    return value >= minimum && value <= maximum;
}

static bool add_signed(int64_t left, int64_t right, int64_t* result) {
    if ((right > 0 && left > INT64_MAX - right) ||
        (right < 0 && left < INT64_MIN - right)) {
        return false;
    }
    *result = left + right;
    return true;
}

static bool subtract_signed(int64_t left, int64_t right, int64_t* result) {
    if ((right < 0 && left > INT64_MAX + right) ||
        (right > 0 && left < INT64_MIN + right)) {
        return false;
    }
    *result = left - right;
    return true;
}

static bool multiply_signed(int64_t left, int64_t right, int64_t* result) {
    if (left == 0 || right == 0) {
        *result = 0;
        return true;
    }
    if ((left == -1 && right == INT64_MIN) ||
        (right == -1 && left == INT64_MIN)) {
        return false;
    }
    if (left > 0) {
        if ((right > 0 && left > INT64_MAX / right) ||
            (right < 0 && right < INT64_MIN / left)) {
            return false;
        }
    } else if ((right > 0 && left < INT64_MIN / right) ||
               (right < 0 && left < INT64_MAX / right)) {
        return false;
    }
    *result = left * right;
    return true;
}

static void replace_integer(Expr* expression, int64_t value) {
    Type* type = expression->type;
    SourceLoc loc = expression->loc;
    expression->kind = EXPR_INT_LIT;
    expression->int_val = value;
    expression->type = type;
    expression->loc = loc;
}

static bool fold_binary(Expr* expression, int64_t left, int64_t right,
                        int64_t* result) {
    bool signed_operands = expression->binary_lhs &&
        expression->binary_rhs && expression->binary_lhs->type &&
        expression->binary_rhs->type &&
        !expression->binary_lhs->type->is_unsigned &&
        !expression->binary_rhs->type->is_unsigned;
    switch (expression->kind) {
        case EXPR_ADD:
            return signed_operands && add_signed(left, right, result) &&
                   signed_result_fits(expression, *result);
        case EXPR_SUB:
            return signed_operands && subtract_signed(left, right, result) &&
                   signed_result_fits(expression, *result);
        case EXPR_MUL:
            return signed_operands && multiply_signed(left, right, result) &&
                   signed_result_fits(expression, *result);
        case EXPR_DIV:
            if (!signed_operands || right == 0 ||
                (left == INT64_MIN && right == -1)) return false;
            *result = left / right;
            return signed_result_fits(expression, *result);
        case EXPR_MOD:
            if (!signed_operands || right == 0 ||
                (left == INT64_MIN && right == -1)) return false;
            *result = left % right;
            return signed_result_fits(expression, *result);
        case EXPR_BITAND:
            *result = left & right;
            return signed_operands && signed_result_fits(expression, *result);
        case EXPR_BITOR:
            *result = left | right;
            return signed_operands && signed_result_fits(expression, *result);
        case EXPR_BITXOR:
            *result = left ^ right;
            return signed_operands && signed_result_fits(expression, *result);
        case EXPR_LSHIFT:
            if (!signed_operands || left < 0 || right < 0 || right >= 63 ||
                left > (INT64_MAX >> right)) return false;
            *result = left << right;
            return signed_result_fits(expression, *result);
        case EXPR_RSHIFT:
            if (!signed_operands || left < 0 || right < 0 || right >= 63) {
                return false;
            }
            *result = left >> right;
            return signed_result_fits(expression, *result);
        case EXPR_EQ: *result = left == right; return true;
        case EXPR_NE: *result = left != right; return true;
        case EXPR_LT:
            if (!signed_operands) return false;
            *result = left < right;
            return true;
        case EXPR_GT:
            if (!signed_operands) return false;
            *result = left > right;
            return true;
        case EXPR_LE:
            if (!signed_operands) return false;
            *result = left <= right;
            return true;
        case EXPR_GE:
            if (!signed_operands) return false;
            *result = left >= right;
            return true;
        case EXPR_AND: *result = left != 0 && right != 0; return true;
        case EXPR_OR: *result = left != 0 || right != 0; return true;
        default: return false;
    }
}

static void optimize_expr_list(ExprList* list) {
    for (ExprList* item = list; item; item = item->next) {
        optimize_expr(&item->expr);
    }
}

static void optimize_expr(Expr** expression) {
    Expr* value;
    int64_t left;
    int64_t right;
    int64_t result;
    if (!expression || !*expression) return;
    value = *expression;
    switch (value->kind) {
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
        case EXPR_ADDR:
        case EXPR_DEREF:
        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC:
            optimize_expr(&value->unary_operand);
            break;
        case EXPR_ADD:
        case EXPR_SUB:
        case EXPR_MUL:
        case EXPR_DIV:
        case EXPR_MOD:
        case EXPR_BITAND:
        case EXPR_BITOR:
        case EXPR_BITXOR:
        case EXPR_LSHIFT:
        case EXPR_RSHIFT:
        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_GT:
        case EXPR_LE:
        case EXPR_GE:
        case EXPR_AND:
        case EXPR_OR:
        case EXPR_ASSIGN:
        case EXPR_ADD_ASSIGN:
        case EXPR_SUB_ASSIGN:
        case EXPR_MUL_ASSIGN:
        case EXPR_DIV_ASSIGN:
        case EXPR_MOD_ASSIGN:
        case EXPR_AND_ASSIGN:
        case EXPR_OR_ASSIGN:
        case EXPR_XOR_ASSIGN:
        case EXPR_LSHIFT_ASSIGN:
        case EXPR_RSHIFT_ASSIGN:
        case EXPR_COMMA:
            optimize_expr(&value->binary_lhs);
            if ((value->kind == EXPR_AND || value->kind == EXPR_OR) &&
                integer_literal(value->binary_lhs, &left) &&
                ((value->kind == EXPR_AND && left == 0) ||
                 (value->kind == EXPR_OR && left != 0))) {
                replace_integer(value, value->kind == EXPR_OR);
                return;
            }
            optimize_expr(&value->binary_rhs);
            break;
        case EXPR_COND:
            optimize_expr(&value->cond_test);
            if (integer_literal(value->cond_test, &left)) {
                Expr** selected = left != 0
                    ? &value->cond_then : &value->cond_else;
                optimize_expr(selected);
                *expression = *selected;
                return;
            }
            optimize_expr(&value->cond_then);
            optimize_expr(&value->cond_else);
            break;
        case EXPR_CALL:
            optimize_expr(&value->call_func);
            optimize_expr_list(value->call_args);
            break;
        case EXPR_INDEX:
            optimize_expr(&value->index_base);
            optimize_expr(&value->index_expr);
            break;
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            optimize_expr(&value->member_base);
            break;
        case EXPR_CAST:
            optimize_expr(&value->cast_expr);
            break;
        case EXPR_COMPOUND:
            optimize_expr_list(value->compound_init);
            break;
        default:
            break;
    }

    switch (value->kind) {
        case EXPR_NEG:
            if (integer_literal(value->unary_operand, &left) &&
                left != INT64_MIN) {
                result = -left;
                if (signed_result_fits(value, result)) {
                    replace_integer(value, result);
                }
            }
            return;
        case EXPR_NOT:
            if (integer_literal(value->unary_operand, &left)) {
                replace_integer(value, left == 0);
            }
            return;
        case EXPR_BITNOT:
            if (integer_literal(value->unary_operand, &left)) {
                result = ~left;
                if (signed_result_fits(value, result)) {
                    replace_integer(value, result);
                }
            }
            return;
        case EXPR_ADD:
        case EXPR_SUB:
        case EXPR_MUL:
        case EXPR_DIV:
        case EXPR_MOD:
        case EXPR_BITAND:
        case EXPR_BITOR:
        case EXPR_BITXOR:
        case EXPR_LSHIFT:
        case EXPR_RSHIFT:
        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_GT:
        case EXPR_LE:
        case EXPR_GE:
        case EXPR_AND:
        case EXPR_OR:
            if (integer_literal(value->binary_lhs, &left) &&
                integer_literal(value->binary_rhs, &right) &&
                fold_binary(value, left, right, &result)) {
                replace_integer(value, result);
            }
            return;
        case EXPR_COMMA:
            if (integer_literal(value->binary_lhs, &left)) {
                *expression = value->binary_rhs;
            }
            return;
        default:
            return;
    }
}

static void optimize_stmt(Stmt* statement) {
    if (!statement) return;
    switch (statement->kind) {
        case STMT_EXPR:
            optimize_expr(&statement->expr);
            break;
        case STMT_BLOCK:
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                optimize_stmt(item->stmt);
            }
            break;
        case STMT_IF:
            optimize_expr(&statement->if_cond);
            optimize_stmt(statement->if_then);
            optimize_stmt(statement->if_else);
            break;
        case STMT_WHILE:
        case STMT_DO:
            optimize_expr(&statement->while_cond);
            optimize_stmt(statement->while_body);
            break;
        case STMT_FOR:
            optimize_stmt(statement->for_init);
            optimize_expr(&statement->for_cond);
            optimize_expr(&statement->for_inc);
            optimize_stmt(statement->for_body);
            break;
        case STMT_SWITCH:
            optimize_expr(&statement->switch_expr);
            optimize_stmt(statement->switch_body);
            break;
        case STMT_CASE:
            optimize_expr(&statement->case_val);
            optimize_stmt(statement->case_stmt);
            break;
        case STMT_DEFAULT:
            optimize_stmt(statement->default_stmt);
            break;
        case STMT_RETURN:
            optimize_expr(&statement->return_val);
            break;
        case STMT_LABEL:
            optimize_stmt(statement->label_stmt);
            break;
        case STMT_DECL:
            if (statement->decl && statement->decl->kind == DECL_VAR) {
                optimize_expr(&statement->decl->var_init);
            }
            break;
        case STMT_ASM:
            for (AsmOperand* operand = statement->asm_outputs; operand;
                 operand = operand->next) {
                optimize_expr(&operand->expr);
            }
            for (AsmOperand* operand = statement->asm_inputs; operand;
                 operand = operand->next) {
                optimize_expr(&operand->expr);
            }
            break;
        default:
            break;
    }
}

void rcc_optimize(AST* ast) {
    if (!ast || g_opts.opt_level <= 0) return;
    for (DeclList* item = ast->decls; item; item = item->next) {
        Decl* declaration = item->decl;
        if (!declaration) continue;
        if (declaration->kind == DECL_VAR) {
            optimize_expr(&declaration->var_init);
        } else if (declaration->kind == DECL_FUNC) {
            optimize_stmt(declaration->func_body);
        }
    }
}
