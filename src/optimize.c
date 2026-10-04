/*
 * RCC - Target-independent AST optimization passes
 */

#include "optimize.h"
#include "ir_lower.h"

#include <limits.h>

static void optimize_expr(Expr** expression);
static void optimize_stmt(Stmt* statement);
static void propagate_block_constants(Stmt* statement);
static void eliminate_block_dead_stores(Stmt* statement);

static bool expression_has_side_effect(const Expr* expression);
static bool expression_mentions_decl(const Expr* expression,
                                     const Decl* declaration);
static bool expression_modifies_decl(const Expr* expression,
                                     const Decl* declaration);
static bool statement_modifies_decl(const Stmt* statement,
                                    const Decl* declaration);
static bool integer_literal(const Expr* expression, int64_t* value);
static int integer_width(const Type* type);
static void replace_integer(Expr* expression, int64_t value);
static uint64_t integer_mask(const Type* type);
static uint64_t integer_unsigned_value(int64_t value, const Type* type);

static void replace_integer_with_side_effect(Expr** expression,
                                              Expr* side_effect) {
    Expr* value;
    Expr* zero;
    Expr* sequence;
    if (!expression || !*expression || !side_effect) return;
    value = *expression;
    zero = expr_int(0, value->loc);
    /* Preserve the original integer result type.  The semantic pass has
     * already assigned the usual arithmetic-conversion type by the time
     * optimization runs, while expr_int starts as plain int. */
    zero->type = value->type;
    sequence = expr_binary(EXPR_COMMA, side_effect, zero, value->loc);
    sequence->type = value->type;
    *expression = sequence;
}

static bool integer_expression_type_matches(const Expr* expression,
                                            const Type* type) {
    return expression && expression->type && type &&
           type_is_integer(expression->type) && type_is_integer((Type*)type) &&
           type_is_compatible(expression->type, (Type*)type);
}

static bool simplify_integer_identity(Expr** expression) {
    Expr* value;
    Expr* left;
    Expr* right;
    int64_t left_value;
    int64_t right_value;
    uint64_t mask;

    if (!expression || !*expression) return false;
    value = *expression;
    switch (value->kind) {
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
            break;
        default:
            return false;
    }
    left = value->binary_lhs;
    right = value->binary_rhs;
    if (!left || !right || !type_is_integer(value->type) ||
        !integer_expression_type_matches(left, value->type) ||
        !integer_expression_type_matches(right, value->type)) {
        return false;
    }
    mask = integer_mask(value->type);
    if (integer_literal(right, &right_value)) {
        uint64_t right_bits = integer_unsigned_value(right_value, value->type);
        if ((value->kind == EXPR_ADD || value->kind == EXPR_SUB ||
             value->kind == EXPR_BITOR || value->kind == EXPR_BITXOR ||
             value->kind == EXPR_LSHIFT || value->kind == EXPR_RSHIFT) &&
            right_bits == 0u) {
            *expression = left;
            return true;
        }
        if (value->kind == EXPR_MUL && right_bits == 1u) {
            *expression = left;
            return true;
        }
        if (value->kind == EXPR_DIV && right_bits == 1u) {
            *expression = left;
            return true;
        }
        if (value->kind == EXPR_BITAND && right_bits == mask) {
            *expression = left;
            return true;
        }
        if ((value->kind == EXPR_MUL || value->kind == EXPR_BITAND) &&
            right_bits == 0u) {
            if (expression_has_side_effect(left)) {
                replace_integer_with_side_effect(expression, left);
            } else {
                replace_integer(value, 0);
            }
            return true;
        }
    }
    if (integer_literal(left, &left_value)) {
        uint64_t left_bits = integer_unsigned_value(left_value, value->type);
        if ((value->kind == EXPR_ADD || value->kind == EXPR_BITOR ||
             value->kind == EXPR_BITXOR) && left_bits == 0u) {
            *expression = right;
            return true;
        }
        if (value->kind == EXPR_MUL && left_bits == 1u) {
            *expression = right;
            return true;
        }
        if (value->kind == EXPR_BITAND && left_bits == mask) {
            *expression = right;
            return true;
        }
        if ((value->kind == EXPR_MUL || value->kind == EXPR_BITAND) &&
            left_bits == 0u) {
            if (expression_has_side_effect(right)) {
                replace_integer_with_side_effect(expression, right);
            } else {
                replace_integer(value, 0);
            }
            return true;
        }
    }
    return false;
}

static bool simplify_unsigned_power_of_two(Expr** expression) {
    Expr* value;
    Expr* operand;
    Expr* shift;
    Expr* replacement;
    int64_t factor_value;
    uint64_t factor_bits;
    uint64_t shift_count = 0u;
    int width;

    if (!expression || !*expression || (*expression)->kind != EXPR_MUL) {
        return false;
    }
    value = *expression;
    if (!value->type || !type_is_integer(value->type) ||
        !value->type->is_unsigned || !value->binary_lhs ||
        !value->binary_rhs ||
        !integer_expression_type_matches(value->binary_lhs, value->type) ||
        !integer_expression_type_matches(value->binary_rhs, value->type)) {
        return false;
    }
    width = integer_width(value->type);
    if (width <= 0) return false;

    if (integer_literal(value->binary_rhs, &factor_value)) {
        operand = value->binary_lhs;
    } else if (integer_literal(value->binary_lhs, &factor_value)) {
        operand = value->binary_rhs;
    } else {
        return false;
    }
    (void)factor;
    factor_bits = integer_unsigned_value(factor_value, value->type);
    if (factor_bits == 0u || (factor_bits & (factor_bits - 1u)) != 0u) {
        return false;
    }
    while ((factor_bits >> shift_count) > 1u) ++shift_count;
    if (shift_count >= (uint64_t)width) return false;

    shift = expr_int((int64_t)shift_count, value->loc);
    shift->type = type_int;
    replacement = expr_binary(EXPR_LSHIFT, operand, shift, value->loc);
    replacement->type = value->type;
    *expression = replacement;
    return true;
}

static bool single_integer_return(const Stmt* statement, int64_t* value) {
    const StmtList* item;
    if (!statement || !value) return false;
    if (statement->kind == STMT_RETURN) {
        return integer_literal(statement->return_val, value);
    }
    if (statement->kind != STMT_BLOCK) return false;
    item = statement->block_stmts;
    if (!item || !item->stmt || item->next) return false;
    return single_integer_return(item->stmt, value);
}

static const Expr* single_return_expression(const Stmt* statement) {
    const StmtList* item;
    if (!statement) return NULL;
    if (statement->kind == STMT_RETURN) return statement->return_val;
    if (statement->kind != STMT_BLOCK) return NULL;
    item = statement->block_stmts;
    if (!item || !item->stmt || item->next) return NULL;
    return single_return_expression(item->stmt);
}

static bool inline_integer_expression_shape(const Expr* expression,
                                            const Decl* parameter,
                                            size_t* parameter_uses) {
    if (!expression || !parameter || !parameter_uses) return false;
    switch (expression->kind) {
        case EXPR_INT_LIT:
            return true;
        case EXPR_IDENT:
            if (expression->ident_decl != parameter) return false;
            ++*parameter_uses;
            return true;
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
            return inline_integer_expression_shape(expression->unary_operand,
                                                   parameter,
                                                   parameter_uses);
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
            return inline_integer_expression_shape(expression->binary_lhs,
                                                   parameter,
                                                   parameter_uses) &&
                   inline_integer_expression_shape(expression->binary_rhs,
                                                   parameter,
                                                   parameter_uses);
        default:
            return false;
    }
}

static Expr* clone_inline_integer_expression(const Expr* expression,
                                             const Decl* parameter,
                                             Expr* argument) {
    Expr* clone;
    if (!expression || !parameter || !argument) return NULL;
    if (expression->kind == EXPR_IDENT) {
        return expression->ident_decl == parameter ? argument : NULL;
    }
    if (expression->kind == EXPR_INT_LIT) return (Expr*)expression;
    switch (expression->kind) {
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
            clone = expr_unary(
                expression->kind,
                clone_inline_integer_expression(expression->unary_operand,
                                                parameter, argument),
                expression->loc);
            if (!clone->unary_operand) return NULL;
            clone->type = expression->type;
            return clone;
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
        case EXPR_OR: {
            Expr* left = clone_inline_integer_expression(
                expression->binary_lhs, parameter, argument);
            Expr* right = clone_inline_integer_expression(
                expression->binary_rhs, parameter, argument);
            if (!left || !right) return NULL;
            clone = expr_binary(expression->kind, left, right,
                                expression->loc);
            clone->type = expression->type;
            return clone;
        }
        default:
            return NULL;
    }
}

static bool inline_side_effect_free_integer_call(Expr** expression_out) {
    Expr* expression;
    Decl* function;
    Decl* parameter;
    DeclList* parameters;
    ExprList* arguments;
    const Expr* returned;
    size_t parameter_uses = 0u;
    int64_t value;
    if (!expression_out || !*expression_out) return false;
    expression = *expression_out;
    if (expression->kind != EXPR_CALL ||
        !expression->call_func ||
        expression->call_func->kind != EXPR_IDENT ||
        expression->call_func->ident_decl == NULL ||
        expression->call_new_args != NULL ||
        expression->call_new_count != NULL || expression->call_is_new ||
        expression->call_is_delete || expression->call_is_virtual ||
        expression->cxx_close_call != NULL) {
        return false;
    }
    function = expression->call_func->ident_decl;
    if (function->kind != DECL_FUNC || !function->func_body ||
        function->func_this_param != NULL || !type_is_integer(expression->type)) {
        return false;
    }
    parameters = function->func_params;
    arguments = expression->call_args;
    if (!parameters) {
        if (arguments != NULL ||
            !single_integer_return(function->func_body, &value)) {
            return false;
        }
        replace_integer(expression, value);
        return true;
    }
    if (parameters->next || !parameters->decl || !arguments ||
        arguments->next || !arguments->expr ||
        parameters->decl->kind != DECL_PARAM ||
        !type_is_integer(parameters->decl->type) ||
        !type_is_compatible(parameters->decl->type, arguments->expr->type) ||
        expression_has_side_effect(arguments->expr)) {
        return false;
    }
    parameter = parameters->decl;
    returned = single_return_expression(function->func_body);
    if (!returned || !type_is_integer(returned->type) ||
        !type_is_compatible(returned->type, expression->type) ||
        !inline_integer_expression_shape(returned, parameter,
                                         &parameter_uses) ||
        parameter_uses > 1u) {
        return false;
    }
    {
        Expr* clone = clone_inline_integer_expression(
            returned, parameter, arguments->expr);
        if (!clone) return false;
        clone->type = expression->type;
        *expression_out = clone;
    }
    return true;
}

static bool expression_list_mentions_decl(const ExprList* list,
                                          const Decl* declaration) {
    for (; list; list = list->next) {
        if (expression_mentions_decl(list->expr, declaration)) return true;
    }
    return false;
}

static bool expression_mentions_decl(const Expr* expression,
                                     const Decl* declaration) {
    if (!expression || !declaration) return false;
    switch (expression->kind) {
        case EXPR_IDENT:
            return expression->ident_decl == declaration;
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
        case EXPR_ADDR:
        case EXPR_DEREF:
        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC:
        case EXPR_SIZEOF:
        case EXPR_ALIGNOF:
        case EXPR_NOEXCEPT:
            return expression_mentions_decl(expression->unary_operand,
                                            declaration);
        case EXPR_CAST:
            return expression_mentions_decl(expression->cast_expr,
                                            declaration);
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
        case EXPR_SPACESHIP:
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
            return expression_mentions_decl(expression->binary_lhs,
                                            declaration) ||
                   expression_mentions_decl(expression->binary_rhs,
                                            declaration);
        case EXPR_COND:
            return expression_mentions_decl(expression->cond_test,
                                            declaration) ||
                   expression_mentions_decl(expression->cond_then,
                                            declaration) ||
                   expression_mentions_decl(expression->cond_else,
                                            declaration);
        case EXPR_CALL:
            return expression_mentions_decl(expression->call_func,
                                            declaration) ||
                   expression_list_mentions_decl(expression->call_args,
                                                 declaration) ||
                   expression_mentions_decl(expression->call_new_count,
                                            declaration) ||
                   expression_list_mentions_decl(expression->call_new_args,
                                                 declaration);
        case EXPR_INDEX:
            return expression_mentions_decl(expression->index_base,
                                            declaration) ||
                   expression_mentions_decl(expression->index_expr,
                                            declaration);
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            return expression_mentions_decl(expression->member_base,
                                            declaration);
        case EXPR_COMPOUND:
            return expression_list_mentions_decl(expression->compound_init,
                                                 declaration);
        case EXPR_GENERIC:
            if (expression_mentions_decl(expression->generic_control,
                                          declaration)) {
                return true;
            }
            for (const GenericAssociation* association =
                     expression->generic_associations;
                 association; association = association->next) {
                if (expression_mentions_decl(association->expr,
                                              declaration)) return true;
            }
            return false;
        case EXPR_CXX_FOLD:
            return expression_mentions_decl(expression->cxx_fold_init,
                                            declaration) ||
                   expression_mentions_decl(expression->cxx_fold_pattern,
                                            declaration);
        case EXPR_CXX_REQUIRES:
            if (expression_list_mentions_decl(
                    expression->cxx_requires_items, declaration) ||
                expression_list_mentions_decl(
                    expression->cxx_requires_nested, declaration)) {
                return true;
            }
            for (const CxxCompoundRequirement* requirement =
                     expression->cxx_requires_compound;
                 requirement; requirement = requirement->next) {
                if (expression_mentions_decl(requirement->expr,
                                              declaration)) return true;
            }
            return false;
        case EXPR_VA_START:
        case EXPR_VA_END:
        case EXPR_VA_COPY:
        case EXPR_VA_ARG:
            return expression_mentions_decl(expression->va_list_operand,
                                            declaration) ||
                   expression_mentions_decl(expression->va_second_operand,
                                            declaration);
        case EXPR_INT_LIT:
        case EXPR_FLOAT_LIT:
        case EXPR_CHAR_LIT:
        case EXPR_STRING_LIT:
        case EXPR_CXX_THIS:
            return false;
    }
    return false;
}

static bool expression_modifies_decl(const Expr* expression,
                                     const Decl* declaration) {
    if (!expression || !declaration) return false;
    switch (expression->kind) {
        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC:
            return expression_mentions_decl(expression->unary_operand,
                                            declaration) ||
                   expression_modifies_decl(expression->unary_operand,
                                            declaration);
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
            return expression_mentions_decl(expression->binary_lhs,
                                            declaration) ||
                   expression_modifies_decl(expression->binary_lhs,
                                            declaration) ||
                   expression_modifies_decl(expression->binary_rhs,
                                            declaration);
        case EXPR_CALL:
            /* A call can mutate a local through a pointer/reference argument.
             * Treat any mentioned local as an escape and therefore block
             * loop-invariant propagation. */
            return expression_mentions_decl(expression, declaration);
        case EXPR_ADDR:
            return expression_mentions_decl(expression->unary_operand,
                                            declaration);
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
        case EXPR_DEREF:
        case EXPR_SIZEOF:
        case EXPR_ALIGNOF:
        case EXPR_NOEXCEPT:
            return expression_modifies_decl(expression->unary_operand,
                                            declaration);
        case EXPR_CAST:
            return expression_modifies_decl(expression->cast_expr,
                                            declaration);
        case EXPR_COND:
            return expression_modifies_decl(expression->cond_test,
                                            declaration) ||
                   expression_modifies_decl(expression->cond_then,
                                            declaration) ||
                   expression_modifies_decl(expression->cond_else,
                                            declaration);
        case EXPR_INDEX:
            return expression_modifies_decl(expression->index_base,
                                            declaration) ||
                   expression_modifies_decl(expression->index_expr,
                                            declaration);
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            return expression_modifies_decl(expression->member_base,
                                            declaration);
        case EXPR_COMPOUND:
            for (const ExprList* item = expression->compound_init; item;
                 item = item->next) {
                if (expression_modifies_decl(item->expr, declaration)) {
                    return true;
                }
            }
            return false;
        case EXPR_GENERIC:
            if (expression_modifies_decl(expression->generic_control,
                                          declaration)) return true;
            for (const GenericAssociation* association =
                     expression->generic_associations;
                 association; association = association->next) {
                if (expression_modifies_decl(association->expr,
                                              declaration)) return true;
            }
            return false;
        case EXPR_CXX_FOLD:
            return expression_modifies_decl(expression->cxx_fold_init,
                                            declaration) ||
                   expression_modifies_decl(expression->cxx_fold_pattern,
                                            declaration);
        case EXPR_CXX_REQUIRES:
            return expression_list_mentions_decl(
                       expression->cxx_requires_items, declaration) ||
                   expression_list_mentions_decl(
                       expression->cxx_requires_nested, declaration);
        case EXPR_VA_START:
        case EXPR_VA_END:
        case EXPR_VA_COPY:
        case EXPR_VA_ARG:
            return expression_mentions_decl(expression, declaration);
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
        case EXPR_SPACESHIP:
        case EXPR_AND:
        case EXPR_OR:
        case EXPR_COMMA:
            return expression_modifies_decl(expression->binary_lhs,
                                            declaration) ||
                   expression_modifies_decl(expression->binary_rhs,
                                            declaration);
        case EXPR_INT_LIT:
        case EXPR_FLOAT_LIT:
        case EXPR_CHAR_LIT:
        case EXPR_STRING_LIT:
        case EXPR_IDENT:
        case EXPR_CXX_THIS:
            return false;
    }
    return false;
}

static bool statement_modifies_decl(const Stmt* statement,
                                    const Decl* declaration) {
    if (!statement || !declaration) return false;
    switch (statement->kind) {
        case STMT_EXPR:
            return expression_modifies_decl(statement->expr, declaration);
        case STMT_BLOCK:
            for (const StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                if (statement_modifies_decl(item->stmt, declaration)) {
                    return true;
                }
            }
            return false;
        case STMT_IF:
            return expression_modifies_decl(statement->if_cond, declaration) ||
                   statement_modifies_decl(statement->if_then, declaration) ||
                   statement_modifies_decl(statement->if_else, declaration);
        case STMT_WHILE:
        case STMT_DO:
            return expression_modifies_decl(statement->while_cond,
                                            declaration) ||
                   statement_modifies_decl(statement->while_body,
                                            declaration);
        case STMT_FOR:
            return statement_modifies_decl(statement->for_init, declaration) ||
                   expression_modifies_decl(statement->for_cond, declaration) ||
                   expression_modifies_decl(statement->for_inc, declaration) ||
                   statement_modifies_decl(statement->for_body, declaration);
        case STMT_SWITCH:
            return expression_modifies_decl(statement->switch_expr,
                                            declaration) ||
                   statement_modifies_decl(statement->switch_body,
                                            declaration);
        case STMT_CASE:
            return expression_modifies_decl(statement->case_val, declaration) ||
                   statement_modifies_decl(statement->case_stmt, declaration);
        case STMT_DEFAULT:
            return statement_modifies_decl(statement->default_stmt,
                                            declaration);
        case STMT_RETURN:
            return expression_modifies_decl(statement->return_val,
                                            declaration);
        case STMT_DECL:
            return statement->decl && statement->decl != declaration &&
                   expression_modifies_decl(statement->decl->var_init,
                                            declaration);
        case STMT_ASM:
            return true;
        case STMT_TRY:
            if (statement_modifies_decl(statement->try_body, declaration)) {
                return true;
            }
            for (const CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                if (statement_modifies_decl(handler->body, declaration)) {
                    return true;
                }
            }
            return false;
        case STMT_THROW:
            return expression_modifies_decl(statement->throw_expr,
                                            declaration);
        case STMT_BREAK:
        case STMT_CONTINUE:
        case STMT_GOTO:
        case STMT_LABEL:
        case STMT_NULL:
            return statement->kind == STMT_LABEL &&
                   statement_modifies_decl(statement->label_stmt,
                                            declaration);
    }
    return false;
}

static bool expression_list_has_side_effect(const ExprList* list) {
    for (const ExprList* item = list; item; item = item->next) {
        if (expression_has_side_effect(item->expr)) return true;
    }
    return false;
}

static bool expression_has_side_effect(const Expr* expression) {
    if (!expression) return false;

    /* These lowerings own cleanup/release actions that cannot be discarded. */
    if (expression->cxx_move_assignment || expression->cxx_close_call ||
        (expression->type && expression->type->cleanup_function)) {
        return true;
    }

    switch (expression->kind) {
        case EXPR_INT_LIT:
        case EXPR_FLOAT_LIT:
        case EXPR_CHAR_LIT:
        case EXPR_STRING_LIT:
        case EXPR_SIZEOF:
        case EXPR_ALIGNOF:
        case EXPR_NOEXCEPT:
        case EXPR_CXX_THIS:
            return false;
        case EXPR_IDENT:
            return expression->type && expression->type->is_volatile;
        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC:
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
        case EXPR_CALL:
        case EXPR_VA_START:
        case EXPR_VA_END:
        case EXPR_VA_COPY:
        case EXPR_VA_ARG:
            return true;
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
        case EXPR_ADDR:
        case EXPR_CAST:
            return expression_has_side_effect(expression->unary_operand);
        case EXPR_DEREF:
            return (expression->type && expression->type->is_volatile) ||
                   expression_has_side_effect(expression->unary_operand);
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
        case EXPR_SPACESHIP:
        case EXPR_AND:
        case EXPR_OR:
        case EXPR_COMMA:
            return expression_has_side_effect(expression->binary_lhs) ||
                   expression_has_side_effect(expression->binary_rhs);
        case EXPR_COND:
            return expression_has_side_effect(expression->cond_test) ||
                   expression_has_side_effect(expression->cond_then) ||
                   expression_has_side_effect(expression->cond_else);
        case EXPR_INDEX:
            return (expression->type && expression->type->is_volatile) ||
                   expression_has_side_effect(expression->index_base) ||
                   expression_has_side_effect(expression->index_expr);
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            return (expression->type && expression->type->is_volatile) ||
                   expression_has_side_effect(expression->member_base);
        case EXPR_COMPOUND:
            return expression_list_has_side_effect(expression->compound_init);
        case EXPR_GENERIC:
            /* Sema replaces valid generic selections before optimization. */
            return true;
        case EXPR_CXX_FOLD:
            /* A fold is invalid until template instantiation expands it. */
            return true;
        case EXPR_CXX_REQUIRES:
            /* Sema replaces requires-expressions with a bool literal. */
            return false;
    }
    return true;
}

static bool statement_contains_label(const Stmt* statement) {
    if (!statement) return false;
    switch (statement->kind) {
        case STMT_LABEL:
        case STMT_CASE:
        case STMT_DEFAULT:
            return true;
        case STMT_BLOCK:
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                if (statement_contains_label(item->stmt)) return true;
            }
            return false;
        case STMT_IF:
            return statement_contains_label(statement->if_then) ||
                   statement_contains_label(statement->if_else);
        case STMT_WHILE:
        case STMT_DO:
            return statement_contains_label(statement->while_body);
        case STMT_FOR:
            return statement_contains_label(statement->for_init) ||
                   statement_contains_label(statement->for_body);
        case STMT_SWITCH:
            return statement_contains_label(statement->switch_body);
        default:
            return false;
    }
}

static bool statement_transfers_control(const Stmt* statement) {
    const StmtList* item;
    if (!statement) return false;
    switch (statement->kind) {
        case STMT_RETURN:
        case STMT_GOTO:
        case STMT_BREAK:
        case STMT_CONTINUE:
            return true;
        case STMT_BLOCK:
            item = statement->block_stmts;
            if (!item) return false;
            while (item->next) item = item->next;
            return statement_transfers_control(item->stmt);
        case STMT_IF:
            return statement->if_else &&
                   statement_transfers_control(statement->if_then) &&
                   statement_transfers_control(statement->if_else);
        case STMT_LABEL:
            return statement_transfers_control(statement->label_stmt);
        case STMT_CASE:
            return statement_transfers_control(statement->case_stmt);
        case STMT_DEFAULT:
            return statement_transfers_control(statement->default_stmt);
        default:
            return false;
    }
}

static void optimize_block(Stmt* statement) {
    StmtList** link;
    bool reachable = true;
    if (!statement || statement->kind != STMT_BLOCK) return;
    link = &statement->block_stmts;
    while (*link) {
        StmtList* item = *link;
        if (!reachable && !statement_contains_label(item->stmt)) {
            *link = item->next;
            continue;
        }
        if (!reachable) reachable = true;
        optimize_stmt(item->stmt);
        if (statement_transfers_control(item->stmt)) reachable = false;
        link = &item->next;
    }
    propagate_block_constants(statement);

    /* Propagation may turn a conditional into an unconditional transfer. */
    reachable = true;
    link = &statement->block_stmts;
    while (*link) {
        StmtList* item = *link;
        if (!reachable && !statement_contains_label(item->stmt)) {
            *link = item->next;
            continue;
        }
        if (!reachable) reachable = true;
        if (statement_transfers_control(item->stmt)) reachable = false;
        link = &item->next;
    }
    eliminate_block_dead_stores(statement);
}

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

static int integer_width(const Type* type) {
    int bits;
    if (!type || !type_is_integer((Type*)type)) return 0;
    bits = type->size * 8;
    return bits > 0 && bits <= 64 ? bits : 0;
}

static uint64_t integer_mask(const Type* type) {
    int bits = integer_width(type);
    if (bits == 64) return UINT64_MAX;
    return bits > 0 ? (UINT64_C(1) << bits) - 1u : 0u;
}

static int64_t integer_bits_to_value(uint64_t bits) {
    if (bits <= (uint64_t)INT64_MAX) return (int64_t)bits;
    return -1 - (int64_t)(UINT64_MAX - bits);
}

static uint64_t integer_unsigned_value(int64_t value, const Type* type) {
    return (uint64_t)value & integer_mask(type);
}

static int64_t integer_signed_value(int64_t value, const Type* type) {
    int bits = integer_width(type);
    uint64_t mask = integer_mask(type);
    uint64_t result = (uint64_t)value & mask;
    if (bits > 0 && bits < 64 &&
        (result & (UINT64_C(1) << (bits - 1))) != 0u) {
        result |= ~mask;
    }
    return integer_bits_to_value(result);
}

static bool signed_type_limits(const Type* type, int64_t* minimum,
                               int64_t* maximum) {
    int bits;
    if (!type || !minimum || !maximum || type->is_unsigned) return false;
    bits = integer_width(type);
    if (bits == 0) return false;
    if (bits == 64) {
        *minimum = INT64_MIN;
        *maximum = INT64_MAX;
    } else {
        *minimum = -(INT64_C(1) << (bits - 1));
        *maximum = (INT64_C(1) << (bits - 1)) - 1;
    }
    return true;
}

static bool signed_value_fits(const Type* type, int64_t value) {
    int64_t minimum;
    int64_t maximum;
    if (!type || !type_is_integer((Type*)type) || type->is_unsigned) {
        return false;
    }
    if (type->kind == TYPE_BOOL) return value == 0 || value == 1;
    return signed_type_limits(type, &minimum, &maximum) &&
           value >= minimum && value <= maximum;
}

static bool signed_result_fits(const Expr* expression, int64_t value) {
    return expression && signed_value_fits(expression->type, value);
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

static void replace_unsigned_integer(Expr* expression, uint64_t value,
                                     const Type* type) {
    replace_integer(expression,
                    integer_bits_to_value(value & integer_mask(type)));
}

static Type* fold_binary_value_type(const Expr* expression) {
    if (!expression) return NULL;
    switch (expression->kind) {
        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_GT:
        case EXPR_LE:
        case EXPR_GE:
            if (expression->binary_lhs && expression->binary_rhs &&
                expression->binary_lhs->type &&
                expression->binary_rhs->type &&
                type_is_integer(expression->binary_lhs->type) &&
                type_is_integer(expression->binary_rhs->type)) {
                return type_common(expression->binary_lhs->type,
                                   expression->binary_rhs->type);
            }
            return NULL;
        default:
            return expression->type;
    }
}

static bool fold_binary(Expr* expression, int64_t left, int64_t right,
                        int64_t* result) {
    Type* value_type;
    uint64_t left_unsigned;
    uint64_t right_unsigned;
    uint64_t unsigned_result;
    uint64_t shift;
    uint64_t mask;
    int64_t left_signed;
    int64_t right_signed;
    int64_t minimum;
    int64_t maximum;
    int bits;

    if (!expression || !result) return false;
    if (expression->kind == EXPR_AND || expression->kind == EXPR_OR) {
        *result = expression->kind == EXPR_AND
            ? (left != 0 && right != 0) : (left != 0 || right != 0);
        return true;
    }
    value_type = fold_binary_value_type(expression);
    bits = integer_width(value_type);
    if (bits == 0) return false;
    mask = integer_mask(value_type);
    left_unsigned = integer_unsigned_value(left, value_type);
    right_unsigned = integer_unsigned_value(right, value_type);
    left_signed = integer_signed_value(left, value_type);
    right_signed = integer_signed_value(right, value_type);

    switch (expression->kind) {
        case EXPR_ADD:
            if (value_type->is_unsigned) {
                *result = integer_bits_to_value(
                    (left_unsigned + right_unsigned) & mask);
                return true;
            }
            return add_signed(left_signed, right_signed, result) &&
                   signed_value_fits(value_type, *result);
        case EXPR_SUB:
            if (value_type->is_unsigned) {
                *result = integer_bits_to_value(
                    (left_unsigned - right_unsigned) & mask);
                return true;
            }
            return subtract_signed(left_signed, right_signed, result) &&
                   signed_value_fits(value_type, *result);
        case EXPR_MUL:
            if (value_type->is_unsigned) {
                *result = integer_bits_to_value(
                    (left_unsigned * right_unsigned) & mask);
                return true;
            }
            return multiply_signed(left_signed, right_signed, result) &&
                   signed_value_fits(value_type, *result);
        case EXPR_DIV:
            if (value_type->is_unsigned) {
                if (right_unsigned == 0u) return false;
                *result = integer_bits_to_value(
                    (left_unsigned / right_unsigned) & mask);
                return true;
            }
            if (right_signed == 0 ||
                !signed_type_limits(value_type, &minimum, &maximum) ||
                (left_signed == minimum && right_signed == -1)) {
                return false;
            }
            *result = left_signed / right_signed;
            return signed_value_fits(value_type, *result);
        case EXPR_MOD:
            if (value_type->is_unsigned) {
                if (right_unsigned == 0u) return false;
                *result = integer_bits_to_value(
                    (left_unsigned % right_unsigned) & mask);
                return true;
            }
            if (right_signed == 0 ||
                !signed_type_limits(value_type, &minimum, &maximum) ||
                (left_signed == minimum && right_signed == -1)) {
                return false;
            }
            *result = left_signed % right_signed;
            return signed_value_fits(value_type, *result);
        case EXPR_BITAND:
            unsigned_result = left_unsigned & right_unsigned;
            *result = value_type->is_unsigned
                ? integer_bits_to_value(unsigned_result & mask)
                : integer_signed_value(
                      integer_bits_to_value(unsigned_result), value_type);
            return true;
        case EXPR_BITOR:
            unsigned_result = left_unsigned | right_unsigned;
            *result = value_type->is_unsigned
                ? integer_bits_to_value(unsigned_result & mask)
                : integer_signed_value(
                      integer_bits_to_value(unsigned_result), value_type);
            return true;
        case EXPR_BITXOR:
            unsigned_result = left_unsigned ^ right_unsigned;
            *result = value_type->is_unsigned
                ? integer_bits_to_value(unsigned_result & mask)
                : integer_signed_value(
                      integer_bits_to_value(unsigned_result), value_type);
            return true;
        case EXPR_LSHIFT:
            shift = (uint64_t)right;
            if (shift >= (uint64_t)bits) return false;
            if (value_type->is_unsigned) {
                *result = integer_bits_to_value(
                    (left_unsigned << shift) & mask);
                return true;
            }
            if (left_signed < 0 ||
                !signed_type_limits(value_type, &minimum, &maximum) ||
                left_signed > (maximum >> shift)) {
                return false;
            }
            *result = integer_bits_to_value(
                ((uint64_t)left_signed << shift) & mask);
            return signed_value_fits(value_type, *result);
        case EXPR_RSHIFT:
            shift = (uint64_t)right;
            if (shift >= (uint64_t)bits) return false;
            if (value_type->is_unsigned) {
                *result = integer_bits_to_value(left_unsigned >> shift);
                return true;
            }
            if (left_signed < 0) return false;
            *result = left_signed >> shift;
            return signed_value_fits(value_type, *result);
        case EXPR_EQ:
            *result = value_type->is_unsigned
                ? left_unsigned == right_unsigned
                : left_signed == right_signed;
            return true;
        case EXPR_NE:
            *result = value_type->is_unsigned
                ? left_unsigned != right_unsigned
                : left_signed != right_signed;
            return true;
        case EXPR_LT:
            *result = value_type->is_unsigned
                ? left_unsigned < right_unsigned
                : left_signed < right_signed;
            return true;
        case EXPR_GT:
            *result = value_type->is_unsigned
                ? left_unsigned > right_unsigned
                : left_signed > right_signed;
            return true;
        case EXPR_LE:
            *result = value_type->is_unsigned
                ? left_unsigned <= right_unsigned
                : left_signed <= right_signed;
            return true;
        case EXPR_GE:
            *result = value_type->is_unsigned
                ? left_unsigned >= right_unsigned
                : left_signed >= right_signed;
            return true;
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
        case EXPR_NOEXCEPT:
            if (value->cxx_noexcept_value_valid) {
                replace_integer(value, value->cxx_noexcept_value ? 1 : 0);
            }
            return;
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
        case EXPR_SPACESHIP:
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
            if (value->kind == EXPR_ASSIGN &&
                value->cxx_move_assignment) {
                optimize_expr(&value->cxx_move_assignment->cleanup);
                optimize_expr(&value->cxx_move_assignment->release);
            }
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
            if (value->cxx_close_call) {
                optimize_expr(&value->cxx_close_call->cleanup);
            }
            if (inline_side_effect_free_integer_call(expression)) return;
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
        case EXPR_VA_START:
        case EXPR_VA_COPY:
            optimize_expr(&value->va_list_operand);
            optimize_expr(&value->va_second_operand);
            break;
        case EXPR_VA_END:
        case EXPR_VA_ARG:
            optimize_expr(&value->va_list_operand);
            break;
        default:
            break;
    }

    if (simplify_integer_identity(expression)) return;
    if (simplify_unsigned_power_of_two(expression)) return;

    switch (value->kind) {
        case EXPR_NEG:
            if (integer_literal(value->unary_operand, &left) &&
                integer_width(value->type) != 0) {
                if (value->type->is_unsigned) {
                    replace_unsigned_integer(
                        value,
                        UINT64_C(0) -
                            integer_unsigned_value(left, value->type),
                        value->type);
                } else {
                    int64_t minimum;
                    int64_t maximum;
                    left = integer_signed_value(left, value->type);
                    if (signed_type_limits(value->type, &minimum, &maximum) &&
                        left != minimum) {
                        result = -left;
                        if (signed_result_fits(value, result)) {
                            replace_integer(value, result);
                        }
                    }
                }
            }
            return;
        case EXPR_NOT:
            if (integer_literal(value->unary_operand, &left)) {
                replace_integer(value, left == 0);
            }
            return;
        case EXPR_BITNOT:
            if (integer_literal(value->unary_operand, &left) &&
                integer_width(value->type) != 0) {
                uint64_t bits =
                    ~integer_unsigned_value(left, value->type) &
                    integer_mask(value->type);
                if (value->type->is_unsigned) {
                    replace_unsigned_integer(value, bits, value->type);
                } else {
                    replace_integer(
                        value,
                        integer_signed_value(integer_bits_to_value(bits),
                                             value->type));
                }
            }
            return;
        case EXPR_CAST:
            if (integer_literal(value->cast_expr, &left) &&
                integer_width(value->type) != 0) {
                if (value->type->kind == TYPE_BOOL) {
                    Type* source_type = value->cast_expr->type;
                    result = source_type && integer_width(source_type) != 0
                        ? integer_unsigned_value(left, source_type) != 0u
                        : left != 0;
                    replace_integer(value, result);
                } else if (value->type->is_unsigned) {
                    replace_unsigned_integer(value, (uint64_t)left,
                                             value->type);
                } else {
                    replace_integer(value,
                                    integer_signed_value(left, value->type));
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

typedef struct LocalConstant {
    Decl* declaration;
    int64_t value;
    bool known;
    struct LocalConstant* next;
} LocalConstant;

typedef struct {
    LocalConstant* bindings;
} ConstantState;

static bool loop_preserves_known_constants(
    const ConstantState* state, const Stmt* init, const Expr* condition,
    const Expr* increment, const Stmt* body) {
    for (const LocalConstant* binding = state ? state->bindings : NULL;
         binding; binding = binding->next) {
        if (!binding->known) continue;
        if (expression_modifies_decl(condition, binding->declaration) ||
            expression_modifies_decl(increment, binding->declaration) ||
            statement_modifies_decl(init, binding->declaration) ||
            statement_modifies_decl(body, binding->declaration)) {
            return false;
        }
    }
    return true;
}

static LocalConstant* find_local_constant(ConstantState* state,
                                          const Decl* declaration) {
    LocalConstant* binding = state ? state->bindings : NULL;
    while (binding && binding->declaration != declaration) {
        binding = binding->next;
    }
    return binding;
}

static void clear_local_constants(ConstantState* state) {
    if (!state) return;
    for (LocalConstant* binding = state->bindings; binding;
         binding = binding->next) {
        binding->known = false;
    }
}

static int64_t normalize_local_constant(Type* type, int64_t value) {
    if (!type || integer_width(type) == 0) return value;
    if (type->kind == TYPE_BOOL) return value != 0;
    if (type->is_unsigned) {
        return integer_bits_to_value(integer_unsigned_value(value, type));
    }
    return integer_signed_value(value, type);
}

static void set_local_constant(ConstantState* state, Decl* declaration,
                               int64_t value) {
    LocalConstant* binding = find_local_constant(state, declaration);
    if (!binding || !declaration || !declaration->type) return;
    binding->value = normalize_local_constant(declaration->type, value);
    binding->known = true;
}

static void invalidate_local_constant(ConstantState* state,
                                      const Decl* declaration) {
    LocalConstant* binding = find_local_constant(state, declaration);
    if (binding) binding->known = false;
}

static ExprKind compound_binary_kind(ExprKind kind) {
    switch (kind) {
        case EXPR_ADD_ASSIGN: return EXPR_ADD;
        case EXPR_SUB_ASSIGN: return EXPR_SUB;
        case EXPR_MUL_ASSIGN: return EXPR_MUL;
        case EXPR_DIV_ASSIGN: return EXPR_DIV;
        case EXPR_MOD_ASSIGN: return EXPR_MOD;
        case EXPR_AND_ASSIGN: return EXPR_BITAND;
        case EXPR_OR_ASSIGN: return EXPR_BITOR;
        case EXPR_XOR_ASSIGN: return EXPR_BITXOR;
        case EXPR_LSHIFT_ASSIGN: return EXPR_LSHIFT;
        case EXPR_RSHIFT_ASSIGN: return EXPR_RSHIFT;
        default: return kind;
    }
}

static bool evaluate_local_update(ExprKind kind, Type* left_type,
                                  Type* right_type, int64_t left,
                                  int64_t right, int64_t* result) {
    Expr operation = {0};
    if (!left_type || !right_type || !result) return false;
    operation.kind = compound_binary_kind(kind);
    operation.type = operation.kind == EXPR_LSHIFT ||
            operation.kind == EXPR_RSHIFT
        ? type_common(left_type, type_int)
        : type_common(left_type, right_type);
    return fold_binary(&operation, left, right, result);
}

static void declare_local_constant(ConstantState* state, Decl* declaration) {
    LocalConstant* binding;
    int64_t value;
    if (!state || !declaration || declaration->kind != DECL_VAR ||
        declaration->var_is_global || declaration->var_is_thread_local ||
        declaration->storage == STORAGE_EXTERN ||
        declaration->storage == STORAGE_STATIC || !declaration->type ||
        !type_is_integer(declaration->type) ||
        declaration->type->is_volatile) {
        return;
    }
    binding = ast_arena_alloc(sizeof(*binding));
    binding->declaration = declaration;
    binding->known = false;
    binding->next = state->bindings;
    state->bindings = binding;
    if (integer_literal(declaration->var_init, &value)) {
        set_local_constant(state, declaration, value);
    }
}

static void propagate_constant_expr(Expr** expression, ConstantState* state);

static void propagate_constant_lvalue(Expr* expression,
                                      ConstantState* state) {
    if (!expression) return;
    switch (expression->kind) {
        case EXPR_IDENT:
            return;
        case EXPR_DEREF:
            propagate_constant_expr(&expression->unary_operand, state);
            return;
        case EXPR_INDEX:
            propagate_constant_expr(&expression->index_base, state);
            propagate_constant_expr(&expression->index_expr, state);
            return;
        case EXPR_MEMBER:
            propagate_constant_lvalue(expression->member_base, state);
            return;
        case EXPR_PTR_MEMBER:
            propagate_constant_expr(&expression->member_base, state);
            return;
        default:
            return;
    }
}

static void propagate_constant_expr_list(ExprList* list,
                                         ConstantState* state) {
    for (ExprList* item = list; item; item = item->next) {
        propagate_constant_expr(&item->expr, state);
    }
}

static void propagate_constant_expr(Expr** expression, ConstantState* state) {
    Expr* value;
    LocalConstant* binding;
    int64_t condition;
    bool arguments_have_side_effect = false;
    if (!expression || !*expression || !state) return;
    value = *expression;

    switch (value->kind) {
        case EXPR_IDENT:
            binding = find_local_constant(state, value->ident_decl);
            if (binding && binding->known && value->type &&
                !value->type->is_volatile) {
                replace_integer(value, binding->value);
            }
            return;
        case EXPR_CXX_THIS:
            return;
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
        case EXPR_DEREF:
        case EXPR_NOEXCEPT:
            propagate_constant_expr(&value->unary_operand, state);
            optimize_expr(expression);
            return;
        case EXPR_CAST:
            propagate_constant_expr(&value->cast_expr, state);
            optimize_expr(expression);
            return;
        case EXPR_ADDR:
            if (value->unary_operand &&
                value->unary_operand->kind == EXPR_IDENT) {
                invalidate_local_constant(
                    state, value->unary_operand->ident_decl);
            }
            propagate_constant_lvalue(value->unary_operand, state);
            return;
        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC:
            propagate_constant_lvalue(value->unary_operand, state);
            if (value->unary_operand &&
                value->unary_operand->kind == EXPR_IDENT) {
                Decl* declaration = value->unary_operand->ident_decl;
                int64_t updated;
                binding = find_local_constant(state, declaration);
                if (binding && binding->known &&
                    evaluate_local_update(
                        value->kind == EXPR_PREINC ||
                                value->kind == EXPR_POSTINC
                            ? EXPR_ADD : EXPR_SUB,
                        declaration->type, type_int, binding->value, 1,
                        &updated)) {
                    set_local_constant(state, declaration, updated);
                } else {
                    invalidate_local_constant(state, declaration);
                }
            } else {
                clear_local_constants(state);
            }
            return;
        case EXPR_ASSIGN:
            propagate_constant_lvalue(value->binary_lhs, state);
            propagate_constant_expr(&value->binary_rhs, state);
            optimize_expr(&value->binary_rhs);
            if (value->cxx_move_assignment) {
                clear_local_constants(state);
            } else if (value->binary_lhs &&
                       value->binary_lhs->kind == EXPR_IDENT) {
                int64_t assigned;
                binding = find_local_constant(
                    state, value->binary_lhs->ident_decl);
                if (binding && integer_literal(value->binary_rhs, &assigned)) {
                    set_local_constant(state,
                                       value->binary_lhs->ident_decl,
                                       assigned);
                } else if (binding) {
                    binding->known = false;
                }
            } else {
                clear_local_constants(state);
            }
            return;
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
            propagate_constant_lvalue(value->binary_lhs, state);
            propagate_constant_expr(&value->binary_rhs, state);
            if (value->binary_lhs &&
                value->binary_lhs->kind == EXPR_IDENT) {
                Decl* declaration = value->binary_lhs->ident_decl;
                int64_t right;
                int64_t updated;
                binding = find_local_constant(state, declaration);
                if (binding && binding->known &&
                    integer_literal(value->binary_rhs, &right) &&
                    evaluate_local_update(value->kind, declaration->type,
                                          value->binary_rhs->type,
                                          binding->value, right, &updated)) {
                    set_local_constant(state, declaration, updated);
                } else {
                    invalidate_local_constant(state, declaration);
                }
            } else {
                clear_local_constants(state);
            }
            return;
        case EXPR_AND:
        case EXPR_OR:
            propagate_constant_expr(&value->binary_lhs, state);
            optimize_expr(&value->binary_lhs);
            if (integer_literal(value->binary_lhs, &condition) &&
                ((value->kind == EXPR_AND && condition == 0) ||
                 (value->kind == EXPR_OR && condition != 0))) {
                optimize_expr(expression);
                return;
            }
            propagate_constant_expr(&value->binary_rhs, state);
            clear_local_constants(state);
            optimize_expr(expression);
            return;
        case EXPR_COND:
            propagate_constant_expr(&value->cond_test, state);
            optimize_expr(&value->cond_test);
            if (integer_literal(value->cond_test, &condition)) {
                Expr** selected = condition != 0
                    ? &value->cond_then : &value->cond_else;
                propagate_constant_expr(selected, state);
                optimize_expr(expression);
                return;
            }
            clear_local_constants(state);
            return;
        case EXPR_COMMA:
            propagate_constant_expr(&value->binary_lhs, state);
            propagate_constant_expr(&value->binary_rhs, state);
            optimize_expr(expression);
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
        case EXPR_SPACESHIP:
            propagate_constant_expr(&value->binary_lhs, state);
            propagate_constant_expr(&value->binary_rhs, state);
            optimize_expr(expression);
            return;
        case EXPR_CALL:
            for (ExprList* item = value->call_args; item; item = item->next) {
                if (expression_has_side_effect(item->expr)) {
                    arguments_have_side_effect = true;
                    break;
                }
            }
            if (!arguments_have_side_effect) {
                Type* function_type = value->call_func
                    ? value->call_func->type : NULL;
                TypeParam* parameter;
                ExprList* argument;
                if (function_type && function_type->kind == TYPE_PTR) {
                    function_type = function_type->base;
                }
                parameter = function_type && function_type->kind == TYPE_FUNC
                    ? function_type->params : NULL;
                for (argument = value->call_args; argument;
                     argument = argument->next) {
                    /* Constant propagation must preserve the address of an
                     * argument bound to T&, const T&, or T&&.  Replacing
                     * that lvalue with its known scalar value would make a
                     * later backend diagnose a valid reference call as a
                     * non-lvalue (and would change the ABI argument). */
                    if (parameter && parameter->type &&
                        parameter->type->is_reference) {
                        propagate_constant_lvalue(argument->expr, state);
                    } else {
                        propagate_constant_expr(&argument->expr, state);
                    }
                    if (parameter) parameter = parameter->next;
                }
            }
            clear_local_constants(state);
            return;
        case EXPR_INDEX:
            propagate_constant_expr(&value->index_base, state);
            propagate_constant_expr(&value->index_expr, state);
            return;
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            propagate_constant_expr(&value->member_base, state);
            return;
        case EXPR_COMPOUND:
            propagate_constant_expr_list(value->compound_init, state);
            return;
        case EXPR_VA_START:
        case EXPR_VA_END:
        case EXPR_VA_COPY:
        case EXPR_VA_ARG:
            clear_local_constants(state);
            return;
        case EXPR_INT_LIT:
        case EXPR_FLOAT_LIT:
        case EXPR_CHAR_LIT:
        case EXPR_STRING_LIT:
        case EXPR_SIZEOF:
        case EXPR_ALIGNOF:
        case EXPR_GENERIC:
        case EXPR_CXX_FOLD:
        case EXPR_CXX_REQUIRES:
            return;
    }
}

static void propagate_block_constants(Stmt* statement) {
    ConstantState state = {0};
    if (!statement || statement->kind != STMT_BLOCK) return;
    for (StmtList* item = statement->block_stmts; item; item = item->next) {
        Stmt* current = item->stmt;
        if (!current) continue;
        switch (current->kind) {
            case STMT_DECL:
                if (current->decl && current->decl->kind == DECL_VAR) {
                    if (current->decl->type &&
                        current->decl->type->is_reference) {
                        /* A reference initializer is an address-bearing
                         * lvalue.  Propagating the pointee's known integer
                         * value would turn a valid binding into a literal and
                         * lose the target ABI address. */
                        propagate_constant_lvalue(
                            current->decl->var_init, &state);
                    } else {
                        propagate_constant_expr(&current->decl->var_init,
                                                &state);
                    }
                    optimize_expr(&current->decl->var_init);
                    declare_local_constant(&state, current->decl);
                }
                break;
            case STMT_EXPR:
                propagate_constant_expr(&current->expr, &state);
                optimize_expr(&current->expr);
                if (!expression_has_side_effect(current->expr)) {
                    current->kind = STMT_NULL;
                    current->expr = NULL;
                }
                break;
            case STMT_RETURN:
                propagate_constant_expr(&current->return_val, &state);
                optimize_expr(&current->return_val);
                break;
            case STMT_IF:
                propagate_constant_expr(&current->if_cond, &state);
                optimize_stmt(current);
                clear_local_constants(&state);
                break;
            case STMT_WHILE:
                if (loop_preserves_known_constants(
                        &state, NULL, current->while_cond, NULL,
                        current->while_body)) {
                    propagate_constant_expr(&current->while_cond, &state);
                    optimize_expr(&current->while_cond);
                }
                optimize_stmt(current);
                clear_local_constants(&state);
                break;
            case STMT_DO:
                if (loop_preserves_known_constants(
                        &state, NULL, current->while_cond, NULL,
                        current->while_body)) {
                    propagate_constant_expr(&current->while_cond, &state);
                    optimize_expr(&current->while_cond);
                }
                optimize_stmt(current);
                clear_local_constants(&state);
                break;
            case STMT_FOR:
                if (loop_preserves_known_constants(
                        &state, current->for_init, current->for_cond,
                        current->for_inc, current->for_body)) {
                    propagate_constant_expr(&current->for_cond, &state);
                    optimize_expr(&current->for_cond);
                }
                optimize_stmt(current);
                clear_local_constants(&state);
                break;
            case STMT_SWITCH:
                propagate_constant_expr(&current->switch_expr, &state);
                optimize_stmt(current);
                clear_local_constants(&state);
                break;
            case STMT_TRY:
                optimize_stmt(current);
                clear_local_constants(&state);
                break;
            case STMT_THROW:
                optimize_expr(&current->throw_expr);
                clear_local_constants(&state);
                break;
            case STMT_BLOCK:
            case STMT_CASE:
            case STMT_DEFAULT:
            case STMT_GOTO:
            case STMT_LABEL:
            case STMT_ASM:
                clear_local_constants(&state);
                break;
            case STMT_BREAK:
            case STMT_CONTINUE:
            case STMT_NULL:
                break;
        }
    }
}

typedef struct DeadStoreLocal {
    Decl* declaration;
    bool escaped;
    bool live;
    struct DeadStoreLocal* next;
} DeadStoreLocal;

static DeadStoreLocal* find_dead_store_local(DeadStoreLocal* locals,
                                              const Decl* declaration) {
    while (locals && locals->declaration != declaration) {
        locals = locals->next;
    }
    return locals;
}

static bool dead_store_candidate(const Decl* declaration) {
    return declaration && declaration->kind == DECL_VAR &&
        !declaration->var_is_global &&
        !declaration->var_is_thread_local &&
        declaration->storage != STORAGE_EXTERN &&
        declaration->storage != STORAGE_STATIC && declaration->type &&
        type_is_integer(declaration->type) &&
        !declaration->type->is_volatile && !declaration->var_cleanup;
}

static DeadStoreLocal* collect_dead_store_locals(const Stmt* block) {
    DeadStoreLocal* locals = NULL;
    if (!block || block->kind != STMT_BLOCK) return NULL;
    for (const StmtList* item = block->block_stmts; item;
         item = item->next) {
        Decl* declaration = item->stmt && item->stmt->kind == STMT_DECL
            ? item->stmt->decl : NULL;
        if (dead_store_candidate(declaration)) {
            DeadStoreLocal* local = ast_arena_alloc(sizeof(*local));
            local->declaration = declaration;
            local->escaped = false;
            local->live = false;
            local->next = locals;
            locals = local;
        }
    }
    return locals;
}

static void mark_address_escapes_expr(const Expr* expression,
                                      DeadStoreLocal* locals);

static void mark_address_escapes_expr_list(const ExprList* list,
                                           DeadStoreLocal* locals) {
    for (; list; list = list->next) {
        mark_address_escapes_expr(list->expr, locals);
    }
}

static void mark_reference_escape(const Expr* expression,
                                  DeadStoreLocal* locals) {
    DeadStoreLocal* local;
    while (expression && expression->kind == EXPR_CAST) {
        expression = expression->cast_expr;
    }
    if (!expression || expression->kind != EXPR_IDENT) return;
    local = find_dead_store_local(locals, expression->ident_decl);
    if (local) local->escaped = true;
}

static void mark_address_escapes_expr(const Expr* expression,
                                      DeadStoreLocal* locals) {
    if (!expression) return;
    if (expression->cxx_move_assignment) {
        mark_address_escapes_expr(expression->cxx_move_assignment->source,
                                  locals);
        mark_address_escapes_expr(expression->cxx_move_assignment->cleanup,
                                  locals);
        mark_address_escapes_expr(expression->cxx_move_assignment->release,
                                  locals);
    }
    if (expression->cxx_close_call) {
        mark_address_escapes_expr(expression->cxx_close_call->object,
                                  locals);
        mark_address_escapes_expr(expression->cxx_close_call->handle,
                                  locals);
        mark_address_escapes_expr(expression->cxx_close_call->cleanup,
                                  locals);
    }
    switch (expression->kind) {
        case EXPR_CXX_THIS:
            return;
        case EXPR_NOEXCEPT:
            /* Its operand is unevaluated and cannot make a local address
             * escape from the containing expression. */
            return;
        case EXPR_CXX_FOLD:
            return;
        case EXPR_ADDR:
            mark_reference_escape(expression->unary_operand, locals);
            mark_address_escapes_expr(expression->unary_operand, locals);
            return;
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
        case EXPR_DEREF:
        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC:
            mark_address_escapes_expr(expression->unary_operand, locals);
            return;
        case EXPR_CAST:
            mark_address_escapes_expr(expression->cast_expr, locals);
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
        case EXPR_SPACESHIP:
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
            mark_address_escapes_expr(expression->binary_lhs, locals);
            mark_address_escapes_expr(expression->binary_rhs, locals);
            return;
        case EXPR_COND:
            mark_address_escapes_expr(expression->cond_test, locals);
            mark_address_escapes_expr(expression->cond_then, locals);
            mark_address_escapes_expr(expression->cond_else, locals);
            return;
        case EXPR_CALL: {
            Type* function_type = expression->call_func
                ? expression->call_func->type : NULL;
            TypeParam* parameter;
            const ExprList* argument;
            if (function_type && function_type->kind == TYPE_PTR) {
                function_type = function_type->base;
            }
            parameter = function_type && function_type->kind == TYPE_FUNC
                ? function_type->params : NULL;
            argument = expression->call_args;
            while (argument) {
                if (parameter && parameter->type &&
                    parameter->type->is_reference) {
                    mark_reference_escape(argument->expr, locals);
                }
                mark_address_escapes_expr(argument->expr, locals);
                argument = argument->next;
                if (parameter) parameter = parameter->next;
            }
            mark_address_escapes_expr(expression->call_func, locals);
            return;
        }
        case EXPR_INDEX:
            mark_address_escapes_expr(expression->index_base, locals);
            mark_address_escapes_expr(expression->index_expr, locals);
            return;
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            mark_address_escapes_expr(expression->member_base, locals);
            return;
        case EXPR_COMPOUND:
            mark_address_escapes_expr_list(expression->compound_init, locals);
            return;
        case EXPR_GENERIC:
            mark_address_escapes_expr(expression->generic_control, locals);
            for (const GenericAssociation* association =
                     expression->generic_associations;
                 association; association = association->next) {
                mark_address_escapes_expr(association->expr, locals);
            }
            return;
        case EXPR_CXX_REQUIRES:
            return;
        case EXPR_VA_START:
        case EXPR_VA_COPY:
            mark_address_escapes_expr(expression->va_list_operand, locals);
            mark_address_escapes_expr(expression->va_second_operand, locals);
            return;
        case EXPR_VA_END:
        case EXPR_VA_ARG:
            mark_address_escapes_expr(expression->va_list_operand, locals);
            return;
        case EXPR_IDENT:
        case EXPR_INT_LIT:
        case EXPR_FLOAT_LIT:
        case EXPR_CHAR_LIT:
        case EXPR_STRING_LIT:
        case EXPR_SIZEOF:
        case EXPR_ALIGNOF:
            return;
    }
}

static void mark_address_escapes_stmt(const Stmt* statement,
                                      DeadStoreLocal* locals) {
    if (!statement) return;
    switch (statement->kind) {
        case STMT_EXPR:
            mark_address_escapes_expr(statement->expr, locals);
            return;
        case STMT_BLOCK:
            for (const StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                mark_address_escapes_stmt(item->stmt, locals);
            }
            return;
        case STMT_IF:
            mark_address_escapes_expr(statement->if_cond, locals);
            mark_address_escapes_stmt(statement->if_then, locals);
            mark_address_escapes_stmt(statement->if_else, locals);
            return;
        case STMT_WHILE:
        case STMT_DO:
            mark_address_escapes_expr(statement->while_cond, locals);
            mark_address_escapes_stmt(statement->while_body, locals);
            return;
        case STMT_FOR:
            mark_address_escapes_stmt(statement->for_init, locals);
            mark_address_escapes_expr(statement->for_cond, locals);
            mark_address_escapes_expr(statement->for_inc, locals);
            mark_address_escapes_stmt(statement->for_body, locals);
            return;
        case STMT_SWITCH:
            mark_address_escapes_expr(statement->switch_expr, locals);
            mark_address_escapes_stmt(statement->switch_body, locals);
            return;
        case STMT_CASE:
            mark_address_escapes_expr(statement->case_val, locals);
            mark_address_escapes_stmt(statement->case_stmt, locals);
            return;
        case STMT_DEFAULT:
            mark_address_escapes_stmt(statement->default_stmt, locals);
            return;
        case STMT_RETURN:
            mark_address_escapes_expr(statement->return_val, locals);
            return;
        case STMT_TRY:
            mark_address_escapes_stmt(statement->try_body, locals);
            for (const CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                mark_address_escapes_stmt(handler->body, locals);
            }
            return;
        case STMT_THROW:
            mark_address_escapes_expr(statement->throw_expr, locals);
            return;
        case STMT_LABEL:
            mark_address_escapes_stmt(statement->label_stmt, locals);
            return;
        case STMT_DECL:
            if (statement->decl && statement->decl->kind == DECL_VAR) {
                if (statement->decl->type &&
                    statement->decl->type->is_reference) {
                    mark_reference_escape(statement->decl->var_init, locals);
                }
                mark_address_escapes_expr(statement->decl->var_init, locals);
                mark_address_escapes_expr(statement->decl->var_cleanup,
                                          locals);
                for (ExprList* item = statement->decl->var_cleanups;
                     item; item = item->next) {
                    mark_address_escapes_expr(item->expr, locals);
                }
            }
            return;
        case STMT_ASM:
            for (const AsmOperand* operand = statement->asm_outputs; operand;
                 operand = operand->next) {
                mark_address_escapes_expr(operand->expr, locals);
            }
            for (const AsmOperand* operand = statement->asm_inputs; operand;
                 operand = operand->next) {
                mark_address_escapes_expr(operand->expr, locals);
            }
            return;
        case STMT_BREAK:
        case STMT_CONTINUE:
        case STMT_GOTO:
        case STMT_NULL:
            return;
    }
}

static void mark_dead_store_reads(const Expr* expression,
                                  DeadStoreLocal* locals);

static void mark_dead_store_lvalue_reads(const Expr* expression,
                                         DeadStoreLocal* locals) {
    if (!expression) return;
    switch (expression->kind) {
        case EXPR_IDENT:
            return;
        case EXPR_DEREF:
            mark_dead_store_reads(expression->unary_operand, locals);
            return;
        case EXPR_INDEX:
            mark_dead_store_reads(expression->index_base, locals);
            mark_dead_store_reads(expression->index_expr, locals);
            return;
        case EXPR_MEMBER:
            mark_dead_store_lvalue_reads(expression->member_base, locals);
            return;
        case EXPR_PTR_MEMBER:
            mark_dead_store_reads(expression->member_base, locals);
            return;
        default:
            mark_dead_store_reads(expression, locals);
            return;
    }
}

static void mark_dead_store_expr_list_reads(const ExprList* list,
                                            DeadStoreLocal* locals) {
    for (; list; list = list->next) {
        mark_dead_store_reads(list->expr, locals);
    }
}

static void mark_dead_store_reads(const Expr* expression,
                                  DeadStoreLocal* locals) {
    DeadStoreLocal* local;
    if (!expression) return;
    if (expression->cxx_move_assignment || expression->cxx_close_call) {
        for (local = locals; local; local = local->next) local->live = true;
    }
    switch (expression->kind) {
        case EXPR_IDENT:
            local = find_dead_store_local(locals, expression->ident_decl);
            if (local) local->live = true;
            return;
        case EXPR_CXX_THIS:
            return;
        case EXPR_NOEXCEPT:
            /* The operand is unevaluated; it does not read local storage. */
            return;
        case EXPR_CXX_FOLD:
            return;
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
        case EXPR_DEREF:
        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC:
            mark_dead_store_reads(expression->unary_operand, locals);
            return;
        case EXPR_ADDR:
            mark_dead_store_lvalue_reads(expression->unary_operand, locals);
            return;
        case EXPR_CAST:
            mark_dead_store_reads(expression->cast_expr, locals);
            return;
        case EXPR_ASSIGN:
            mark_dead_store_lvalue_reads(expression->binary_lhs, locals);
            mark_dead_store_reads(expression->binary_rhs, locals);
            return;
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
            mark_dead_store_reads(expression->binary_lhs, locals);
            mark_dead_store_reads(expression->binary_rhs, locals);
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
        case EXPR_SPACESHIP:
        case EXPR_AND:
        case EXPR_OR:
        case EXPR_COMMA:
            mark_dead_store_reads(expression->binary_lhs, locals);
            mark_dead_store_reads(expression->binary_rhs, locals);
            return;
        case EXPR_COND:
            mark_dead_store_reads(expression->cond_test, locals);
            mark_dead_store_reads(expression->cond_then, locals);
            mark_dead_store_reads(expression->cond_else, locals);
            return;
        case EXPR_CALL:
            mark_dead_store_reads(expression->call_func, locals);
            mark_dead_store_expr_list_reads(expression->call_args, locals);
            return;
        case EXPR_INDEX:
            mark_dead_store_reads(expression->index_base, locals);
            mark_dead_store_reads(expression->index_expr, locals);
            return;
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            mark_dead_store_reads(expression->member_base, locals);
            return;
        case EXPR_COMPOUND:
            mark_dead_store_expr_list_reads(expression->compound_init, locals);
            return;
        case EXPR_GENERIC:
            mark_dead_store_reads(expression->generic_control, locals);
            for (const GenericAssociation* association =
                     expression->generic_associations;
                 association; association = association->next) {
                mark_dead_store_reads(association->expr, locals);
            }
            return;
        case EXPR_CXX_REQUIRES:
            return;
        case EXPR_VA_START:
        case EXPR_VA_COPY:
            mark_dead_store_reads(expression->va_list_operand, locals);
            mark_dead_store_reads(expression->va_second_operand, locals);
            return;
        case EXPR_VA_END:
        case EXPR_VA_ARG:
            mark_dead_store_reads(expression->va_list_operand, locals);
            return;
        case EXPR_INT_LIT:
        case EXPR_FLOAT_LIT:
        case EXPR_CHAR_LIT:
        case EXPR_STRING_LIT:
        case EXPR_SIZEOF:
        case EXPR_ALIGNOF:
            return;
    }
}

static void mark_all_dead_store_locals_live(DeadStoreLocal* locals) {
    for (; locals; locals = locals->next) locals->live = true;
}

static DeadStoreLocal* dead_store_target(const Expr* expression,
                                         DeadStoreLocal* locals) {
    const Expr* target = NULL;
    if (!expression || expression->cxx_move_assignment ||
        expression->cxx_close_call) {
        return NULL;
    }
    switch (expression->kind) {
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
            target = expression->binary_lhs;
            break;
        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC:
            target = expression->unary_operand;
            break;
        default:
            return NULL;
    }
    return target && target->kind == EXPR_IDENT
        ? find_dead_store_local(locals, target->ident_decl) : NULL;
}

static bool dead_store_has_integer_rhs(const Expr* expression) {
    if (!expression) return false;
    switch (expression->kind) {
        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC:
            return true;
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
            return expression->binary_rhs && expression->binary_rhs->type &&
                type_is_integer(expression->binary_rhs->type);
        default:
            return false;
    }
}

static void eliminate_dead_store_expression(Stmt* statement,
                                            DeadStoreLocal* local,
                                            DeadStoreLocal* locals) {
    Expr* expression = statement->expr;
    bool simple_assignment = expression->kind == EXPR_ASSIGN;
    bool increment = expression->kind == EXPR_PREINC ||
        expression->kind == EXPR_PREDEC ||
        expression->kind == EXPR_POSTINC ||
        expression->kind == EXPR_POSTDEC;
    if (!local->live && !local->escaped &&
        dead_store_has_integer_rhs(expression)) {
        Expr* preserved = increment ? NULL : expression->binary_rhs;
        if (!preserved || !expression_has_side_effect(preserved)) {
            statement->kind = STMT_NULL;
            statement->expr = NULL;
        } else {
            statement->expr = preserved;
            mark_dead_store_reads(preserved, locals);
        }
        local->live = false;
        return;
    }
    if (simple_assignment) {
        local->live = false;
        mark_dead_store_reads(expression->binary_rhs, locals);
    } else {
        local->live = true;
        if (!increment) {
            mark_dead_store_reads(expression->binary_rhs, locals);
        }
    }
}

static void eliminate_block_dead_stores(Stmt* statement) {
    DeadStoreLocal* locals;
    StmtList** items;
    size_t count = 0u;
    size_t index = 0u;
    if (!statement || statement->kind != STMT_BLOCK) return;
    locals = collect_dead_store_locals(statement);
    if (!locals) return;
    mark_address_escapes_stmt(statement, locals);
    for (StmtList* item = statement->block_stmts; item; item = item->next) {
        ++count;
    }
    items = ast_arena_alloc(count * sizeof(*items));
    for (StmtList* item = statement->block_stmts; item; item = item->next) {
        items[index++] = item;
    }
    while (index != 0u) {
        Stmt* current = items[--index]->stmt;
        DeadStoreLocal* local;
        if (!current) continue;
        switch (current->kind) {
            case STMT_EXPR:
                local = dead_store_target(current->expr, locals);
                if (local) {
                    eliminate_dead_store_expression(current, local, locals);
                } else {
                    mark_dead_store_reads(current->expr, locals);
                }
                break;
            case STMT_DECL:
                local = current->decl
                    ? find_dead_store_local(locals, current->decl) : NULL;
                if (local) {
                    bool initializer_needed = local->live;
                    local->live = false;
                    if (!initializer_needed && !local->escaped &&
                        current->decl->var_init &&
                        current->decl->var_init->type &&
                        type_is_integer(current->decl->var_init->type) &&
                        !expression_has_side_effect(
                            current->decl->var_init)) {
                        current->decl->var_init = NULL;
                    }
                    mark_dead_store_reads(current->decl->var_init, locals);
                } else if (current->decl &&
                           current->decl->kind == DECL_VAR) {
                    mark_dead_store_reads(current->decl->var_init, locals);
                    mark_dead_store_reads(current->decl->var_cleanup, locals);
                    for (ExprList* item = current->decl->var_cleanups;
                         item; item = item->next) {
                        mark_dead_store_reads(item->expr, locals);
                    }
                }
                break;
            case STMT_RETURN:
                mark_dead_store_reads(current->return_val, locals);
                break;
            case STMT_NULL:
                break;
            case STMT_BLOCK:
            case STMT_IF:
            case STMT_WHILE:
            case STMT_DO:
            case STMT_FOR:
            case STMT_SWITCH:
            case STMT_CASE:
            case STMT_DEFAULT:
            case STMT_TRY:
            case STMT_THROW:
            case STMT_BREAK:
            case STMT_CONTINUE:
            case STMT_GOTO:
            case STMT_LABEL:
            case STMT_ASM:
                mark_all_dead_store_locals_live(locals);
                break;
        }
    }
}

static void optimize_stmt(Stmt* statement) {
    if (!statement) return;
    switch (statement->kind) {
        case STMT_EXPR:
            optimize_expr(&statement->expr);
            if (!expression_has_side_effect(statement->expr)) {
                statement->kind = STMT_NULL;
                statement->expr = NULL;
            }
            break;
        case STMT_BLOCK:
            optimize_block(statement);
            break;
        case STMT_IF:
            optimize_expr(&statement->if_cond);
            {
                int64_t condition;
                if (integer_literal(statement->if_cond, &condition)) {
                    Stmt* selected = condition != 0
                        ? statement->if_then : statement->if_else;
                    Stmt* discarded = condition != 0
                        ? statement->if_else : statement->if_then;
                    /* A goto may enter the syntactically unreachable arm. */
                    if (!statement_contains_label(discarded)) {
                        if (selected) {
                            optimize_stmt(selected);
                            *statement = *selected;
                        } else {
                            statement->kind = STMT_NULL;
                        }
                        return;
                    }
                }
            }
            optimize_stmt(statement->if_then);
            optimize_stmt(statement->if_else);
            break;
        case STMT_WHILE:
            optimize_expr(&statement->while_cond);
            {
                int64_t condition;
                if (integer_literal(statement->while_cond, &condition) &&
                    condition == 0 &&
                    !statement_contains_label(statement->while_body)) {
                    statement->kind = STMT_NULL;
                    return;
                }
            }
            optimize_stmt(statement->while_body);
            break;
        case STMT_DO:
            optimize_expr(&statement->while_cond);
            optimize_stmt(statement->while_body);
            break;
        case STMT_FOR:
            optimize_stmt(statement->for_init);
            optimize_expr(&statement->for_cond);
            {
                int64_t condition;
                if (integer_literal(statement->for_cond, &condition) &&
                    condition == 0 &&
                    !statement_contains_label(statement->for_body)) {
                    Stmt* initializer = statement->for_init;
                    if (initializer) {
                        StmtList* only = ast_arena_alloc(sizeof(*only));
                        only->stmt = initializer;
                        only->next = NULL;
                        statement->kind = STMT_BLOCK;
                        statement->block_stmts = only;
                    } else {
                        statement->kind = STMT_NULL;
                    }
                    return;
                }
            }
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
        case STMT_TRY:
            optimize_stmt(statement->try_body);
            for (CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                optimize_stmt(handler->body);
            }
            break;
        case STMT_THROW:
            optimize_expr(&statement->throw_expr);
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
    char ir_error[256];
    size_t lowered_functions = 0u;
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
    if (!rcc_ir_verify_ast_subset(ast, &lowered_functions, ir_error,
                                  sizeof(ir_error))) {
        rcc_fatal("typed SSA lowering failed: %s", ir_error);
    }
    if (g_opts.verbose) {
        printf("Typed SSA shadow verification: %zu function(s)\n",
               lowered_functions);
    }
}
