/*
 * RCC - Target-independent AST optimization passes
 */

#include "optimize.h"

#include <limits.h>

static void optimize_expr(Expr** expression);
static void optimize_stmt(Stmt* statement);
static void propagate_block_constants(Stmt* statement);

static bool expression_has_side_effect(const Expr* expression);

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
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
        case EXPR_DEREF:
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
                invalidate_local_constant(
                    state, value->unary_operand->ident_decl);
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
                invalidate_local_constant(
                    state, value->binary_lhs->ident_decl);
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
                propagate_constant_expr_list(value->call_args, state);
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
                    propagate_constant_expr(&current->decl->var_init, &state);
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
            case STMT_DO:
            case STMT_FOR:
                /* Loop conditions observe mutations from earlier iterations,
                 * so block-entry constants are not valid in the condition. */
                clear_local_constants(&state);
                break;
            case STMT_SWITCH:
                propagate_constant_expr(&current->switch_expr, &state);
                optimize_stmt(current);
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
