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

/* A bounded fixed point is used only to resolve declaration-order
 * dependencies between the conservative pure-scalar inline candidates.
 * Recursive, aggregate, exception, and whole-program cost-based inline forms
 * remain outside this pass; local expression expansion has an explicit node
 * budget below. */
static bool optimize_inline_changed;
static AST* optimize_inline_ast;

typedef struct ConstantState ConstantState;

static bool expression_has_side_effect(const Expr* expression);
static bool expression_mentions_decl(const Expr* expression,
                                     const Decl* declaration);
static bool expression_modifies_decl(const Expr* expression,
                                     const Decl* declaration);
static bool statement_modifies_decl(const Stmt* statement,
                                    const Decl* declaration);
static bool integer_literal(const Expr* expression, int64_t* value);
static bool constant_integer_expression(const Expr* expression,
                                        int64_t* value);
static bool float_literal(const Expr* expression, double* value);
static bool floating_to_integer_literal(double value, const Type* type,
                                        int64_t* result);
static bool constant_scalar_truth(const Expr* expression, bool* value);
static int integer_width(const Type* type);
static void replace_integer(Expr* expression, int64_t value);
static void replace_float(Expr* expression, double value);
static uint64_t integer_mask(const Type* type);
static int64_t integer_bits_to_value(uint64_t bits);
static uint64_t integer_unsigned_value(int64_t value, const Type* type);
static int64_t integer_signed_value(int64_t value, const Type* type);
static bool signed_type_limits(const Type* type, int64_t* minimum,
                               int64_t* maximum);
static Expr* clone_inline_pure_scalar_expression(const Expr* expression);
static size_t inline_pure_scalar_expression_cost(const Expr* expression);

static bool statement_contains_loop_transfer(const Stmt* statement);
static bool statement_contains_declaration(const Stmt* statement);
static bool statement_contains_unroll_unsafe_declaration(
    const Stmt* statement);
static bool statement_is_unroll_safe_shape(const Stmt* statement);
static int unit_for_step(const Expr* increment, const Decl* induction);
static bool for_initializer(const Stmt* initializer, Decl** induction,
                            const Expr** initial_value);
static bool for_condition_matches_step(const Expr* condition, int step);
static bool constant_for_iteration_count(const Stmt* statement,
                                         unsigned* count);
static Expr* clone_unrolled_expr(const Expr* expression);
static Stmt* clone_unrolled_stmt(const Stmt* statement);
static StmtList** append_unrolled_stmt(StmtList** tail, Stmt* statement);
static bool unroll_constant_for(Stmt* statement, unsigned count);
static bool unroll_single_iteration_for(Stmt* statement);
static bool constant_while_iteration_count(const Stmt* statement,
                                           const ConstantState* state,
                                           unsigned* count);
static bool constant_do_iteration_count(const Stmt* statement,
                                        const ConstantState* state,
                                        unsigned* count);
static bool while_body_constant_step(const Stmt* body, const Decl* induction,
                                     int* step);
static bool unroll_constant_loop(Stmt* statement, unsigned count);
static bool fold_constant_switch(Stmt* statement);

enum {
    INLINE_PURE_SCALAR_EXPANSION_LIMIT = 64,
    INLINE_SCALAR_BINDING_LIMIT = 16
};

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
        if (!value->type->is_unsigned && right_bits == mask) {
            if (value->kind == EXPR_MUL || value->kind == EXPR_DIV) {
                Expr* replacement =
                    expr_unary(EXPR_NEG, left, value->loc);
                replacement->type = value->type;
                *expression = replacement;
                return true;
            }
            if (value->kind == EXPR_MOD) {
                if (expression_has_side_effect(left)) {
                    replace_integer_with_side_effect(expression, left);
                } else {
                    replace_integer(value, 0);
                }
                return true;
            }
        }
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
        if (value->kind == EXPR_MOD && right_bits == 1u) {
            if (expression_has_side_effect(left)) {
                replace_integer_with_side_effect(expression, left);
            } else {
                replace_integer(value, 0);
            }
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
        if (!value->type->is_unsigned && value->kind == EXPR_MUL &&
            left_bits == mask) {
            Expr* replacement = expr_unary(EXPR_NEG, right, value->loc);
            replacement->type = value->type;
            *expression = replacement;
            return true;
        }
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

    if (!expression || !*expression ||
        ((*expression)->kind != EXPR_MUL &&
         (*expression)->kind != EXPR_DIV &&
         (*expression)->kind != EXPR_MOD)) {
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

    if (value->kind == EXPR_MUL) {
        if (integer_literal(value->binary_rhs, &factor_value)) {
            operand = value->binary_lhs;
        } else if (integer_literal(value->binary_lhs, &factor_value)) {
            operand = value->binary_rhs;
        } else {
            return false;
        }
    } else {
        if (!integer_literal(value->binary_rhs, &factor_value)) {
            return false;
        }
        operand = value->binary_lhs;
    }
    factor_bits = integer_unsigned_value(factor_value, value->type);
    if (factor_bits == 0u || (factor_bits & (factor_bits - 1u)) != 0u) {
        return false;
    }
    while ((factor_bits >> shift_count) > 1u) ++shift_count;
    if (shift_count >= (uint64_t)width) return false;

    if (value->kind == EXPR_MOD) {
        Expr* mask = expr_int((int64_t)(factor_bits - 1u), value->loc);
        mask->type = value->type;
        replacement = expr_binary(EXPR_BITAND, operand, mask, value->loc);
    } else {
        shift = expr_int((int64_t)shift_count, value->loc);
        shift->type = type_int;
        replacement = expr_binary(value->kind == EXPR_MUL
                                      ? EXPR_LSHIFT : EXPR_RSHIFT,
                                  operand, shift, value->loc);
    }
    replacement->type = value->type;
    *expression = replacement;
    return true;
}

static Expr* make_signed_power_of_two_div(
    const Expr* operand, unsigned shift, SourceLoc loc, Type* type) {
    Expr* count;
    Expr* shifted;
    Expr* negative_zero;
    Expr* negative;
    Expr* mask;
    Expr* remainder;
    Expr* remainder_zero;
    Expr* has_remainder;
    Expr* correction;
    Expr* one;
    Expr* zero;
    Expr* replacement;
    uint64_t mask_value;

    if (!operand || !type || shift == 0u || shift >= 63u) return NULL;
    mask_value = (UINT64_C(1) << shift) - 1u;
    count = expr_int((int64_t)shift, loc);
    count->type = type_int;
    shifted = expr_binary(
        EXPR_RSHIFT, clone_inline_pure_scalar_expression(operand), count,
        loc);
    if (!shifted || !shifted->binary_lhs) return NULL;
    shifted->type = type;

    negative_zero = expr_int(0, loc);
    negative_zero->type = type;
    negative = expr_binary(
        EXPR_LT, clone_inline_pure_scalar_expression(operand),
        negative_zero, loc);
    if (!negative || !negative->binary_lhs) return NULL;
    negative->type = type_int;

    mask = expr_int((int64_t)mask_value, loc);
    mask->type = type;
    remainder = expr_binary(
        EXPR_BITAND, clone_inline_pure_scalar_expression(operand), mask,
        loc);
    if (!remainder || !remainder->binary_lhs) return NULL;
    remainder->type = type;
    remainder_zero = expr_int(0, loc);
    remainder_zero->type = type;
    has_remainder = expr_binary(
        EXPR_NE, remainder, remainder_zero, loc);
    if (!has_remainder || !has_remainder->binary_lhs) return NULL;
    has_remainder->type = type_int;

    correction = expr_binary(EXPR_AND, negative, has_remainder, loc);
    if (!correction || !correction->binary_lhs) return NULL;
    correction->type = type_int;
    one = expr_int(1, loc);
    one->type = type;
    zero = expr_int(0, loc);
    zero->type = type;
    replacement = expr_cond(correction, one, zero, loc);
    if (!replacement) return NULL;
    replacement->type = type;
    return expr_binary(EXPR_ADD, shifted, replacement, loc);
}

static bool simplify_signed_power_of_two(Expr** expression) {
    Expr* value;
    Expr* operand;
    Expr* replacement;
    int64_t factor_value;
    uint64_t factor_bits;
    unsigned shift_count = 0u;
    int width;

    if (!expression || !*expression ||
        (*expression)->kind != EXPR_DIV) return false;
    value = *expression;
    if (!value->type || !type_is_integer(value->type) ||
        value->type->is_unsigned || !value->binary_lhs ||
        !value->binary_rhs ||
        !integer_expression_type_matches(value->binary_lhs, value->type) ||
        !integer_expression_type_matches(value->binary_rhs, value->type) ||
        !integer_literal(value->binary_rhs, &factor_value)) {
        return false;
    }
    operand = value->binary_lhs;
    if (expression_has_side_effect(operand)) return false;
    width = integer_width(value->type);
    if (width <= 1) return false;
    factor_bits = integer_unsigned_value(factor_value, value->type);
    if (factor_value <= 0 || factor_bits < 2u ||
        factor_bits > (uint64_t)INT64_MAX ||
        (factor_bits & (factor_bits - 1u)) != 0u) return false;
    while ((factor_bits >> shift_count) > 1u) ++shift_count;
    if (shift_count == 0u || shift_count >= (unsigned)(width - 1)) {
        return false;
    }
    replacement = make_signed_power_of_two_div(
        operand, shift_count, value->loc, value->type);
    if (!replacement || !replacement->binary_lhs) return false;
    replacement->type = value->type;
    *expression = replacement;
    return true;
}

static Expr* make_signed_power_of_two_mod(
    const Expr* operand, unsigned shift, SourceLoc loc, Type* type) {
    Expr* mask_for_check;
    Expr* remainder_for_check;
    Expr* zero_for_check;
    Expr* has_remainder;
    Expr* negative_zero;
    Expr* negative;
    Expr* correction;
    Expr* mask_for_adjust;
    Expr* remainder_for_adjust;
    Expr* divisor;
    Expr* adjusted;
    Expr* mask_for_result;
    Expr* positive_remainder;
    Expr* replacement;
    uint64_t mask_value;

    if (!operand || !type || shift == 0u || shift >= 63u) return NULL;
    mask_value = (UINT64_C(1) << shift) - 1u;

    mask_for_check = expr_int((int64_t)mask_value, loc);
    mask_for_check->type = type;
    remainder_for_check = expr_binary(
        EXPR_BITAND, clone_inline_pure_scalar_expression(operand),
        mask_for_check, loc);
    if (!remainder_for_check || !remainder_for_check->binary_lhs) {
        return NULL;
    }
    remainder_for_check->type = type;
    zero_for_check = expr_int(0, loc);
    zero_for_check->type = type;
    has_remainder = expr_binary(
        EXPR_NE, remainder_for_check, zero_for_check, loc);
    if (!has_remainder || !has_remainder->binary_lhs) return NULL;
    has_remainder->type = type_int;

    negative_zero = expr_int(0, loc);
    negative_zero->type = type;
    negative = expr_binary(
        EXPR_LT, clone_inline_pure_scalar_expression(operand),
        negative_zero, loc);
    if (!negative || !negative->binary_lhs) return NULL;
    negative->type = type_int;
    correction = expr_binary(EXPR_AND, negative, has_remainder, loc);
    if (!correction || !correction->binary_lhs) return NULL;
    correction->type = type_int;

    mask_for_adjust = expr_int((int64_t)mask_value, loc);
    mask_for_adjust->type = type;
    remainder_for_adjust = expr_binary(
        EXPR_BITAND, clone_inline_pure_scalar_expression(operand),
        mask_for_adjust, loc);
    if (!remainder_for_adjust || !remainder_for_adjust->binary_lhs) {
        return NULL;
    }
    remainder_for_adjust->type = type;
    divisor = expr_int((int64_t)(mask_value + 1u), loc);
    divisor->type = type;
    adjusted = expr_binary(
        EXPR_SUB, remainder_for_adjust, divisor, loc);
    if (!adjusted || !adjusted->binary_lhs) return NULL;
    adjusted->type = type;

    mask_for_result = expr_int((int64_t)mask_value, loc);
    mask_for_result->type = type;
    positive_remainder = expr_binary(
        EXPR_BITAND, clone_inline_pure_scalar_expression(operand),
        mask_for_result, loc);
    if (!positive_remainder || !positive_remainder->binary_lhs) {
        return NULL;
    }
    positive_remainder->type = type;
    replacement = expr_cond(
        correction, adjusted, positive_remainder, loc);
    if (!replacement) return NULL;
    replacement->type = type;
    return replacement;
}

static bool simplify_signed_power_of_two_remainder(Expr** expression) {
    Expr* value;
    Expr* replacement;
    int64_t factor_value;
    uint64_t factor_bits;
    unsigned shift_count = 0u;
    int width;

    if (!expression || !*expression ||
        (*expression)->kind != EXPR_MOD) return false;
    value = *expression;
    if (!value->type || !type_is_integer(value->type) ||
        value->type->is_unsigned || !value->binary_lhs ||
        !value->binary_rhs ||
        !integer_expression_type_matches(value->binary_lhs, value->type) ||
        !integer_expression_type_matches(value->binary_rhs, value->type) ||
        !integer_literal(value->binary_rhs, &factor_value) ||
        factor_value <= 0 || expression_has_side_effect(value->binary_lhs)) {
        return false;
    }
    width = integer_width(value->type);
    if (width <= 1) return false;
    factor_bits = integer_unsigned_value(factor_value, value->type);
    if (factor_bits < 2u || factor_bits > (uint64_t)INT64_MAX ||
        (factor_bits & (factor_bits - 1u)) != 0u) return false;
    while ((factor_bits >> shift_count) > 1u) ++shift_count;
    if (shift_count == 0u || shift_count >= (unsigned)(width - 1)) {
        return false;
    }
    replacement = make_signed_power_of_two_mod(
        value->binary_lhs, shift_count, value->loc, value->type);
    if (!replacement) return false;
    *expression = replacement;
    return true;
}

static Expr* make_unsigned_shift(const Expr* operand, unsigned shift,
                                 SourceLoc loc, Type* type) {
    Expr* count;
    Expr* shifted;
    if (!operand || !type) return NULL;
    count = expr_int((int64_t)shift, loc);
    count->type = type_int;
    shifted = expr_binary(
        EXPR_LSHIFT, clone_inline_pure_scalar_expression(operand), count,
        loc);
    if (!shifted || !shifted->binary_lhs) return NULL;
    shifted->type = type;
    return shifted;
}

static Expr* make_unsigned_shift_add(const Expr* operand, unsigned factor,
                                     SourceLoc loc, Type* type) {
    Expr* result = NULL;
    unsigned shift;

    if (!operand || !type) return NULL;
    for (shift = 0u; shift < 7u; ++shift) {
        Expr* term;
        if ((factor & (1u << shift)) == 0u) continue;
        term = shift == 0u
            ? clone_inline_pure_scalar_expression(operand)
            : make_unsigned_shift(operand, shift, loc, type);
        if (!term) return NULL;
        if (!result) {
            result = term;
        } else {
            result = expr_binary(EXPR_ADD, result, term, loc);
            if (!result) return NULL;
            result->type = type;
        }
    }
    return result;
}

static bool simplify_unsigned_small_multiply(Expr** expression) {
    Expr* value;
    Expr* operand;
    Expr* replacement;
    Expr* left;
    Expr* right;
    int64_t factor_value;
    uint64_t factor;
    unsigned shift;

    if (!expression || !*expression ||
        (*expression)->kind != EXPR_MUL) return false;
    value = *expression;
    if (!value->type || !type_is_integer(value->type) ||
        !value->type->is_unsigned || !value->binary_lhs ||
        !value->binary_rhs) return false;
    if (integer_literal(value->binary_lhs, &factor_value)) {
        operand = value->binary_rhs;
    } else if (integer_literal(value->binary_rhs, &factor_value)) {
        operand = value->binary_lhs;
    } else {
        return false;
    }
    if (!integer_expression_type_matches(operand, value->type) ||
        expression_has_side_effect(operand)) return false;
    factor = integer_unsigned_value(factor_value, value->type);
    if (factor < 3u || factor > 127u || factor == 4u || factor == 8u ||
        factor == 16u) {
        return false;
    }

    switch (factor) {
        case 3u:
            shift = 1u;
            left = make_unsigned_shift(operand, shift, value->loc,
                                        value->type);
            right = clone_inline_pure_scalar_expression(operand);
            replacement = left && right
                ? expr_binary(EXPR_ADD, left, right, value->loc) : NULL;
            break;
        case 5u:
            left = make_unsigned_shift(operand, 2u, value->loc,
                                       value->type);
            right = clone_inline_pure_scalar_expression(operand);
            replacement = left && right
                ? expr_binary(EXPR_ADD, left, right, value->loc) : NULL;
            break;
        case 6u:
            left = make_unsigned_shift(operand, 2u, value->loc,
                                       value->type);
            right = make_unsigned_shift(operand, 1u, value->loc,
                                        value->type);
            replacement = left && right
                ? expr_binary(EXPR_ADD, left, right, value->loc) : NULL;
            break;
        case 7u:
            left = make_unsigned_shift(operand, 3u, value->loc,
                                       value->type);
            right = clone_inline_pure_scalar_expression(operand);
            replacement = left && right
                ? expr_binary(EXPR_SUB, left, right, value->loc) : NULL;
            break;
        default:
            if (factor < 9u || factor > 127u) return false;
            replacement = make_unsigned_shift_add(
                operand, (unsigned)factor, value->loc, value->type);
            break;
    }
    if (!replacement) return false;
    replacement->type = value->type;
    *expression = replacement;
    return true;
}

static bool float_literal(const Expr* expression, double* value) {
    if (!expression || expression->kind != EXPR_FLOAT_LIT ||
        !expression->type || !type_is_floating(expression->type) || !value) {
        return false;
    }
    *value = expression->type->kind == TYPE_FLOAT
        ? (double)(float)expression->float_val : expression->float_val;
    return true;
}

static bool floating_to_integer_literal(double value, const Type* type,
                                        int64_t* result) {
    int bits;
    double upper;
    uint64_t unsigned_result;
    int64_t minimum;
    int64_t maximum;

    if (!type || !result || !type_is_integer((Type*)type) ||
        value != value) {
        return false;
    }
    bits = integer_width(type);
    if (bits <= 0) return false;
    if (type->is_unsigned) {
        /* Use an exclusive power-of-two bound.  In particular, converting
         * 2^32 to uint32_t or 2^64 to uint64_t is not a representable C
         * conversion and must remain in the backend. */
        upper = bits == 64 ? 18446744073709551616.0
                           : (double)(UINT64_C(1) << bits);
        if (value <= -1.0 || value >= upper) return false;
        if (value < 0.0) {
            unsigned_result = 0u;
        } else if (bits == 64 && value >= 9223372036854775808.0) {
            /* Keep the host conversion below INT64_MAX so this remains
             * well-defined even on hosts whose unsigned conversion follows
             * the signed range for floating-point operands. */
            unsigned_result = (uint64_t)(value - 9223372036854775808.0) |
                UINT64_C(0x8000000000000000);
        } else {
            unsigned_result = (uint64_t)value;
        }
        *result = integer_bits_to_value(unsigned_result & integer_mask(type));
        return true;
    }
    if (bits == 64) {
        /* (double)INT64_MAX rounds to 2^63, so the upper bound is exclusive. */
        if (value < -9223372036854775808.0 ||
            value >= 9223372036854775808.0) {
            return false;
        }
        *result = value <= -9223372036854775808.0
            ? INT64_MIN : (int64_t)value;
        return true;
    }
    if (!signed_type_limits(type, &minimum, &maximum) ||
        value <= (double)minimum - 1.0 ||
        value >= (double)maximum + 1.0) {
        return false;
    }
    *result = (int64_t)value;
    return true;
}

static bool constant_scalar_truth(const Expr* expression, bool* value) {
    int64_t integer;
    double floating;
    if (!value) return false;
    if (integer_literal(expression, &integer)) {
        *value = integer != 0;
        return true;
    }
    if (float_literal(expression, &floating)) {
        /* NaNs are true in a C scalar context; this comparison also keeps
         * negative zero false without inspecting the host representation. */
        *value = floating != 0.0;
        return true;
    }
    return false;
}

static void replace_float(Expr* expression, double value) {
    Type* type;
    SourceLoc loc;
    if (!expression) return;
    type = expression->type;
    loc = expression->loc;
    expression->kind = EXPR_FLOAT_LIT;
    expression->float_val = type && type->kind == TYPE_FLOAT
        ? (double)(float)value : value;
    expression->type = type;
    expression->loc = loc;
}

static bool fold_float_literals(Expr* expression) {
    double left;
    double right;
    double result;
    int64_t left_int;
    bool comparison;
    bool left_truth;
    bool right_truth;
    float left_float;
    float right_float;
    float float_result;
    if (!expression) return false;
    if (expression->kind == EXPR_CAST && expression->cast_expr) {
        if (float_literal(expression->cast_expr, &left)) {
            if (expression->type && type_is_floating(expression->type)) {
                replace_float(expression, left);
                return true;
            }
            if (expression->type && expression->type->kind == TYPE_BOOL) {
                replace_integer(expression, left == 0.0 ? 0 : 1);
                return true;
            }
            if (expression->type && type_is_integer(expression->type) &&
                expression->type->kind != TYPE_BOOL &&
                floating_to_integer_literal(
                    left, expression->type, &left_int)) {
                replace_integer(expression, left_int);
                return true;
            }
        } else if (expression->type &&
                   type_is_floating(expression->type) &&
                   integer_literal(expression->cast_expr, &left_int)) {
            const Type* source_type = expression->cast_expr->type;
            uint64_t integer_bits = (uint64_t)left_int;
            if (!source_type || !type_is_integer((Type*)source_type)) {
                return false;
            }
            if (source_type && source_type->is_unsigned) {
                integer_bits = integer_unsigned_value(left_int, source_type);
                replace_float(expression, (double)integer_bits);
            } else {
                replace_float(expression,
                              (double)integer_signed_value(left_int,
                                                           source_type));
            }
            return true;
        }
        return false;
    }
    if (expression->kind == EXPR_NEG &&
        float_literal(expression->unary_operand, &left)) {
        replace_float(expression, -left);
        return true;
    }
    if (expression->kind == EXPR_NOT &&
        float_literal(expression->unary_operand, &left)) {
        replace_integer(expression, left == 0.0 ? 1 : 0);
        return true;
    }
    if ((expression->kind == EXPR_AND || expression->kind == EXPR_OR) &&
        float_literal(expression->binary_lhs, &left) &&
        float_literal(expression->binary_rhs, &right)) {
        left_truth = left != 0.0;
        right_truth = right != 0.0;
        replace_integer(expression, expression->kind == EXPR_AND
            ? (left_truth && right_truth) : (left_truth || right_truth));
        return true;
    }
    switch (expression->kind) {
        case EXPR_ADD:
        case EXPR_SUB:
        case EXPR_MUL:
        case EXPR_DIV:
        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_GT:
        case EXPR_LE:
        case EXPR_GE:
            break;
        default:
            return false;
    }
    if (!float_literal(expression->binary_lhs, &left) ||
        !float_literal(expression->binary_rhs, &right)) {
        return false;
    }
    switch (expression->kind) {
        case EXPR_ADD:
        case EXPR_SUB:
        case EXPR_MUL:
            if (!expression->type || !type_is_floating(expression->type)) {
                return false;
            }
            if (expression->type->kind == TYPE_FLOAT) {
                left_float = (float)left;
                right_float = (float)right;
                float_result = expression->kind == EXPR_ADD
                    ? left_float + right_float
                    : expression->kind == EXPR_SUB
                        ? left_float - right_float
                        : left_float * right_float;
                replace_float(expression, (double)float_result);
            } else {
                result = expression->kind == EXPR_ADD
                    ? left + right
                    : expression->kind == EXPR_SUB
                        ? left - right : left * right;
                replace_float(expression, result);
            }
            return true;
        case EXPR_DIV:
            /* Keep floating division by zero in the backend: its result and
             * floating-environment behavior are target/runtime semantics. */
            if (right == 0.0 || !expression->type ||
                !type_is_floating(expression->type)) return false;
            if (expression->type->kind == TYPE_FLOAT) {
                float_result = (float)left / (float)right;
                replace_float(expression, (double)float_result);
            } else {
                replace_float(expression, left / right);
            }
            return true;
        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_GT:
        case EXPR_LE:
        case EXPR_GE:
            comparison = expression->kind == EXPR_EQ ? left == right
                : expression->kind == EXPR_NE ? left != right
                : expression->kind == EXPR_LT ? left < right
                : expression->kind == EXPR_GT ? left > right
                : expression->kind == EXPR_LE ? left <= right
                : left >= right;
            replace_integer(expression, comparison ? 1 : 0);
            return true;
        default:
            return false;
    }
}

typedef struct InlineScalarBinding {
    const Decl* parameter;
    Expr* argument;
    size_t uses;
} InlineScalarBinding;

typedef enum InlineScalarOperationKind {
    INLINE_SCALAR_LOCAL_INITIALIZER,
    INLINE_SCALAR_LOCAL_ASSIGNMENT
} InlineScalarOperationKind;

typedef struct InlineScalarOperation {
    InlineScalarOperationKind kind;
    const Decl* declaration;
    const Expr* expression;
} InlineScalarOperation;

/* Keep the multi-statement inline shape deliberately narrow.  A block may
 * contain only scalar, non-volatile automatic declarations with pure
 * initializers, direct side-effect-free assignments to those locals, and one
 * final return.  This lets small wrappers such as
 * `int f(int x) { int y = x + 1; y = y * 2; return y; }` be expanded without
 * pretending that arbitrary control flow, cleanup, or lifetime-sensitive
 * objects are safe to clone into the caller. */
static bool collect_inline_scalar_body(
    const Stmt* body, InlineScalarOperation* operations,
    size_t* operation_count, const Expr** returned) {
    const StmtList* item;
    if (!operations || !operation_count || !returned || !body) {
        return false;
    }
    *operation_count = 0u;
    *returned = NULL;
    if (body->kind == STMT_RETURN) {
        *returned = body->return_val;
        return *returned != NULL;
    }
    if (body->kind != STMT_BLOCK) return false;
    for (item = body->block_stmts; item; item = item->next) {
        const Stmt* statement = item->stmt;
        if (!statement) return false;
        if (statement->kind == STMT_DECL) {
            const Decl* declaration = statement->decl;
            if (!declaration || declaration->kind != DECL_VAR ||
                *returned != NULL ||
                declaration->var_is_global ||
                declaration->var_is_static_local ||
                declaration->var_is_thread_local || declaration->var_is_vla ||
                !declaration->name || declaration->name[0] == '\0' ||
                !declaration->type || declaration->type->is_volatile ||
                !type_is_scalar(declaration->type) ||
                !declaration->var_init || declaration->var_cleanup ||
                declaration->var_cleanups ||
                *operation_count >= INLINE_SCALAR_BINDING_LIMIT) {
                return false;
            }
            operations[*operation_count].kind =
                INLINE_SCALAR_LOCAL_INITIALIZER;
            operations[*operation_count].declaration = declaration;
            operations[*operation_count].expression = declaration->var_init;
            ++*operation_count;
            continue;
        }
        if (statement->kind == STMT_EXPR && statement->expr &&
            statement->expr->kind == EXPR_ASSIGN &&
            statement->expr->binary_lhs &&
            statement->expr->binary_lhs->kind == EXPR_IDENT &&
            statement->expr->binary_lhs->ident_decl &&
            statement->expr->binary_rhs &&
            *returned == NULL &&
            *operation_count < INLINE_SCALAR_BINDING_LIMIT) {
            const Decl* declaration =
                statement->expr->binary_lhs->ident_decl;
            bool is_prior_local = false;
            for (size_t index = 0u; index < *operation_count; ++index) {
                if (operations[index].kind ==
                        INLINE_SCALAR_LOCAL_INITIALIZER &&
                    operations[index].declaration == declaration) {
                    is_prior_local = true;
                }
            }
            if (!is_prior_local || !declaration->type ||
                declaration->type->is_volatile ||
                !type_is_scalar(declaration->type) ||
                !type_is_compatible(declaration->type,
                                    statement->expr->binary_rhs->type) ||
                expression_has_side_effect(statement->expr->binary_rhs)) {
                return false;
            }
            operations[*operation_count].kind =
                INLINE_SCALAR_LOCAL_ASSIGNMENT;
            operations[*operation_count].declaration = declaration;
            operations[*operation_count].expression =
                statement->expr->binary_rhs;
            ++*operation_count;
            continue;
        }
        if (statement->kind == STMT_RETURN) {
            if (*returned != NULL || !statement->return_val) return false;
            *returned = statement->return_val;
            continue;
        }
        return false;
    }
    return *returned != NULL;
}

static size_t inline_scalar_binding_index(
    const Decl* declaration, const InlineScalarBinding* bindings,
    size_t binding_count) {
    size_t index;
    if (!declaration || !bindings) return binding_count;
    for (index = 0u; index < binding_count; ++index) {
        if (bindings[index].parameter == declaration) return index;
    }
    return binding_count;
}

static bool inline_scalar_expression_shape(
    const Expr* expression, InlineScalarBinding* bindings,
    size_t binding_count) {
    size_t binding_index;
    if (!expression || (!bindings && binding_count != 0u)) return false;
    switch (expression->kind) {
        case EXPR_INT_LIT:
        case EXPR_FLOAT_LIT:
        case EXPR_CHAR_LIT:
        case EXPR_STRING_LIT:
        case EXPR_SIZEOF:
        case EXPR_ALIGNOF:
        case EXPR_NOEXCEPT:
            return true;
        case EXPR_IDENT:
            binding_index = inline_scalar_binding_index(
                expression->ident_decl, bindings, binding_count);
            if (binding_index >= binding_count) return false;
            ++bindings[binding_index].uses;
            return true;
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
        case EXPR_ADDR:
        case EXPR_DEREF:
            return inline_scalar_expression_shape(expression->unary_operand,
                                                   bindings, binding_count);
        case EXPR_CAST:
            return inline_scalar_expression_shape(expression->cast_expr,
                                                   bindings, binding_count);
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
            return inline_scalar_expression_shape(expression->binary_lhs,
                                                   bindings, binding_count) &&
                   inline_scalar_expression_shape(expression->binary_rhs,
                                                   bindings, binding_count);
        case EXPR_INDEX:
            return inline_scalar_expression_shape(expression->index_base,
                                                   bindings, binding_count) &&
                   inline_scalar_expression_shape(expression->index_expr,
                                                   bindings, binding_count);
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            return expression->member_name && expression->member_field &&
                   inline_scalar_expression_shape(expression->member_base,
                                                  bindings, binding_count);
        case EXPR_COND:
            return inline_scalar_expression_shape(expression->cond_test,
                                                   bindings, binding_count) &&
                   inline_scalar_expression_shape(expression->cond_then,
                                                   bindings, binding_count) &&
                   inline_scalar_expression_shape(expression->cond_else,
                                                   bindings, binding_count);
        default:
            return false;
    }
}

static Expr* clone_inline_scalar_expression(
    const Expr* expression, const InlineScalarBinding* bindings,
    size_t binding_count) {
    Expr* clone;
    size_t binding_index;
    if (!expression || !bindings) return NULL;
    if (expression->kind == EXPR_IDENT) {
        binding_index = inline_scalar_binding_index(
            expression->ident_decl, bindings, binding_count);
        /* An identifier outside the callee binding set belongs to the
         * caller's expression argument.  Keep it as a pure leaf while
         * recursively expanding callee-local bindings.  The body and return
         * expression are shape-checked before this helper is called, so an
         * unbound identifier cannot silently escape from the callee itself. */
        if (binding_index >= binding_count) {
            return clone_inline_pure_scalar_expression(expression);
        }
        if (bindings[binding_index].argument->kind == EXPR_IDENT ||
            bindings[binding_index].argument->kind == EXPR_INT_LIT ||
            bindings[binding_index].argument->kind == EXPR_FLOAT_LIT) {
            return bindings[binding_index].argument;
        }
        /* Local initializers may depend on an earlier local.  Clone through
         * the same binding table so `shifted = sum << 1` substitutes the
         * already-bound `sum` expression instead of leaving a dead stack
         * reference in the caller. */
        return clone_inline_scalar_expression(bindings[binding_index].argument,
                                              bindings, binding_count);
    }
    if (expression->kind == EXPR_INT_LIT ||
        expression->kind == EXPR_FLOAT_LIT ||
        expression->kind == EXPR_CHAR_LIT ||
        expression->kind == EXPR_STRING_LIT ||
        expression->kind == EXPR_SIZEOF ||
        expression->kind == EXPR_ALIGNOF ||
        expression->kind == EXPR_NOEXCEPT) {
        return (Expr*)expression;
    }
    switch (expression->kind) {
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
        case EXPR_ADDR:
        case EXPR_DEREF:
            clone = expr_unary(
                expression->kind,
                clone_inline_scalar_expression(expression->unary_operand,
                                                bindings, binding_count),
                expression->loc);
            if (!clone->unary_operand) return NULL;
            clone->type = expression->type;
            return clone;
        case EXPR_INDEX: {
            Expr* base = clone_inline_scalar_expression(
                expression->index_base, bindings, binding_count);
            Expr* index = clone_inline_scalar_expression(
                expression->index_expr, bindings, binding_count);
            if (!base || !index) return NULL;
            clone = expr_index(base, index, expression->loc);
            clone->type = expression->type;
            return clone;
        }
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            clone = expr_member(
                clone_inline_scalar_expression(expression->member_base,
                                                bindings, binding_count),
                expression->member_name, expression->loc);
            if (!clone->member_base) return NULL;
            clone->kind = expression->kind;
            clone->member_field = expression->member_field;
            clone->type = expression->type;
            return clone;
        case EXPR_CAST:
            clone = expr_cast(
                expression->cast_type,
                clone_inline_scalar_expression(expression->cast_expr,
                                                bindings, binding_count),
                expression->loc);
            if (!clone->cast_expr) return NULL;
            clone->type = expression->type;
            clone->cxx_cast_kind = expression->cxx_cast_kind;
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
        case EXPR_OR:
        case EXPR_COMMA: {
            Expr* left = clone_inline_scalar_expression(
                expression->binary_lhs, bindings, binding_count);
            Expr* right = clone_inline_scalar_expression(
                expression->binary_rhs, bindings, binding_count);
            if (!left || !right) return NULL;
            clone = expr_binary(expression->kind, left, right,
                                expression->loc);
            clone->type = expression->type;
            return clone;
        }
        case EXPR_COND:
            clone = expr_cond(
                clone_inline_scalar_expression(expression->cond_test,
                                                bindings, binding_count),
                clone_inline_scalar_expression(expression->cond_then,
                                                bindings, binding_count),
                clone_inline_scalar_expression(expression->cond_else,
                                                bindings, binding_count),
                expression->loc);
            if (!clone->cond_test || !clone->cond_then ||
                !clone->cond_else) return NULL;
            clone->type = expression->type;
            return clone;
        default:
            return NULL;
    }
}

/* Repeatedly substituting a complex argument must not attach one AST node to
 * multiple parents.  Keep this clone deliberately narrower than the whole
 * expression language: only side-effect-free scalar expression forms which
 * the inline body already understands may be copied. */
static Expr* clone_inline_pure_scalar_expression(const Expr* expression) {
    Expr* clone;
    if (!expression) return NULL;
    switch (expression->kind) {
        case EXPR_INT_LIT:
        case EXPR_FLOAT_LIT:
        case EXPR_CHAR_LIT:
        case EXPR_STRING_LIT:
        case EXPR_IDENT:
        case EXPR_SIZEOF:
        case EXPR_ALIGNOF:
        case EXPR_NOEXCEPT:
            return (Expr*)expression;
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
        case EXPR_ADDR:
        case EXPR_DEREF:
            clone = expr_unary(
                expression->kind,
                clone_inline_pure_scalar_expression(
                    expression->unary_operand), expression->loc);
            if (!clone->unary_operand) return NULL;
            clone->type = expression->type;
            return clone;
        case EXPR_INDEX: {
            Expr* base = clone_inline_pure_scalar_expression(
                expression->index_base);
            Expr* index = clone_inline_pure_scalar_expression(
                expression->index_expr);
            if (!base || !index) return NULL;
            clone = expr_index(base, index, expression->loc);
            clone->type = expression->type;
            return clone;
        }
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            clone = expr_member(
                clone_inline_pure_scalar_expression(expression->member_base),
                expression->member_name, expression->loc);
            if (!clone->member_base) return NULL;
            clone->kind = expression->kind;
            clone->member_field = expression->member_field;
            clone->type = expression->type;
            return clone;
        case EXPR_CAST:
            clone = expr_cast(
                expression->cast_type,
                clone_inline_pure_scalar_expression(expression->cast_expr),
                expression->loc);
            if (!clone->cast_expr) return NULL;
            clone->type = expression->type;
            clone->cxx_cast_kind = expression->cxx_cast_kind;
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
        case EXPR_OR:
        case EXPR_COMMA:
            clone = expr_binary(
                expression->kind,
                clone_inline_pure_scalar_expression(
                    expression->binary_lhs),
                clone_inline_pure_scalar_expression(
                    expression->binary_rhs), expression->loc);
            if (!clone->binary_lhs || !clone->binary_rhs) return NULL;
            clone->type = expression->type;
            return clone;
        case EXPR_COND:
            clone = expr_cond(
                clone_inline_pure_scalar_expression(expression->cond_test),
                clone_inline_pure_scalar_expression(expression->cond_then),
                clone_inline_pure_scalar_expression(expression->cond_else),
                expression->loc);
            if (!clone->cond_test || !clone->cond_then ||
                !clone->cond_else) return NULL;
            clone->type = expression->type;
            return clone;
        default:
            return NULL;
    }
}

static size_t inline_pure_scalar_expression_cost(const Expr* expression) {
    size_t left;
    size_t right;
    if (!expression) return 0u;
    switch (expression->kind) {
        case EXPR_INT_LIT:
        case EXPR_FLOAT_LIT:
        case EXPR_CHAR_LIT:
        case EXPR_STRING_LIT:
        case EXPR_IDENT:
        case EXPR_SIZEOF:
        case EXPR_ALIGNOF:
        case EXPR_NOEXCEPT:
            return 1u;
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
        case EXPR_ADDR:
        case EXPR_DEREF:
            left = inline_pure_scalar_expression_cost(
                expression->unary_operand);
            return left == (size_t)-1 || left == (size_t)-1 - 1u
                       ? (size_t)-1 : left + 1u;
        case EXPR_INDEX:
            left = inline_pure_scalar_expression_cost(
                expression->index_base);
            right = inline_pure_scalar_expression_cost(
                expression->index_expr);
            if (left == (size_t)-1 || right == (size_t)-1 ||
                left > (size_t)-1 - right - 1u) return (size_t)-1;
            return left + right + 1u;
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            left = inline_pure_scalar_expression_cost(
                expression->member_base);
            return left == (size_t)-1 || left == (size_t)-1 - 1u
                       ? (size_t)-1 : left + 1u;
        case EXPR_CAST:
            left = inline_pure_scalar_expression_cost(
                expression->cast_expr);
            return left == (size_t)-1 || left == (size_t)-1 - 1u
                       ? (size_t)-1 : left + 1u;
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
            left = inline_pure_scalar_expression_cost(
                expression->binary_lhs);
            right = inline_pure_scalar_expression_cost(
                expression->binary_rhs);
            if (left == (size_t)-1 || right == (size_t)-1 ||
                left > (size_t)-1 - right - 1u) return (size_t)-1;
            return left + right + 1u;
        case EXPR_COND:
            left = inline_pure_scalar_expression_cost(
                expression->cond_test);
            right = inline_pure_scalar_expression_cost(
                expression->cond_then);
            if (left == (size_t)-1 || right == (size_t)-1 ||
                left > (size_t)-1 - right - 1u) return (size_t)-1;
            left += right + 1u;
            right = inline_pure_scalar_expression_cost(
                expression->cond_else);
            if (right == (size_t)-1 || left > (size_t)-1 - right) {
                return (size_t)-1;
            }
            return left + right;
        default:
            return (size_t)-1;
    }
}

static Decl* resolve_inline_function_definition(Decl* function) {
    if (!function || function->func_body || !optimize_inline_ast ||
        !function->name || !function->type) {
        return function;
    }
    for (DeclList* item = optimize_inline_ast->decls; item;
         item = item->next) {
        Decl* candidate = item->decl;
        if (!candidate || candidate == function ||
            candidate->kind != DECL_FUNC || !candidate->func_body ||
            !candidate->name ||
            strcmp(candidate->name, function->name) != 0 ||
            candidate->func_has_cxx_linkage != function->func_has_cxx_linkage ||
            !type_is_compatible(candidate->type, function->type)) {
            continue;
        }
        return candidate;
    }
    return function;
}

static bool inline_side_effect_free_scalar_call(Expr** expression_out) {
    Expr* expression;
    Decl* function;
    DeclList* parameters;
    ExprList* arguments;
    const Expr* returned;
    InlineScalarOperation operations[INLINE_SCALAR_BINDING_LIMIT];
    InlineScalarBinding bindings[INLINE_SCALAR_BINDING_LIMIT];
    size_t binding_count = 0u;
    size_t parameter_count = 0u;
    size_t operation_count = 0u;
    size_t index;
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
    function = resolve_inline_function_definition(function);
    if (function->kind != DECL_FUNC || !function->func_body ||
        function->func_this_param != NULL || !type_is_scalar(expression->type)) {
        return false;
    }
    if (!collect_inline_scalar_body(function->func_body, operations,
                                    &operation_count, &returned)) {
        return false;
    }
    parameters = function->func_params;
    arguments = expression->call_args;
    for (DeclList* parameter = parameters; parameter;
         parameter = parameter->next) {
        ++parameter_count;
    }
    if (parameter_count > INLINE_SCALAR_BINDING_LIMIT) return false;
    if (parameter_count == 0u) {
        if (arguments != NULL) return false;
    } else {
        for (index = 0u; index < parameter_count; ++index) {
            DeclList* parameter = parameters;
            ExprList* argument = arguments;
            size_t offset;
            for (offset = 0u; offset < index; ++offset) {
                parameter = parameter->next;
                argument = argument ? argument->next : NULL;
            }
            if (!parameter || !parameter->decl || !argument ||
                !argument->expr || parameter->decl->kind != DECL_PARAM ||
                !type_is_scalar(parameter->decl->type) ||
                !type_is_compatible(parameter->decl->type,
                                    argument->expr->type) ||
                expression_has_side_effect(argument->expr)) {
                return false;
            }
            bindings[binding_count].parameter = parameter->decl;
            bindings[binding_count].argument = argument->expr;
            bindings[binding_count].uses = 0u;
            ++binding_count;
        }
    }
    if (parameter_count != 0u) {
        ExprList* extra_argument = arguments;
        for (index = 0u; index < parameter_count && extra_argument;
             ++index, extra_argument = extra_argument->next) {
        }
        if (extra_argument) return false;
    }
    for (index = 0u; index < operation_count; ++index) {
        const InlineScalarOperation* operation = &operations[index];
        size_t local_binding;
        Expr* value;
        if (!operation->declaration || !operation->expression ||
            !type_is_compatible(operation->declaration->type,
                                operation->expression->type) ||
            expression_has_side_effect(operation->expression) ||
            !inline_scalar_expression_shape(operation->expression, bindings,
                                             binding_count)) {
            return false;
        }
        value = clone_inline_scalar_expression(
            operation->expression, bindings, binding_count);
        if (!value) return false;
        value->type = operation->declaration->type;
        if (operation->kind == INLINE_SCALAR_LOCAL_INITIALIZER) {
            if (binding_count >= INLINE_SCALAR_BINDING_LIMIT) return false;
            bindings[binding_count].parameter = operation->declaration;
            bindings[binding_count].argument = value;
            bindings[binding_count].uses = 0u;
            ++binding_count;
            continue;
        }
        local_binding = inline_scalar_binding_index(
            operation->declaration, bindings, binding_count);
        if (local_binding < parameter_count || local_binding >= binding_count) {
            return false;
        }
        bindings[local_binding].argument = value;
        bindings[local_binding].uses = 0u;
    }
    if (!returned || !type_is_scalar(returned->type) ||
        !type_is_compatible(returned->type, expression->type) ||
        !inline_scalar_expression_shape(returned, bindings, binding_count) ||
        expression_has_side_effect(returned)) {
        return false;
    }
    for (index = 0u; index < binding_count; ++index) {
        size_t cost;
        /* Repeated pure expressions are safe only when they can be cloned
         * and the resulting expansion stays bounded.  This is deliberately
         * a local cost guard, not a promise of whole-program inlining. */
        if (bindings[index].uses <= 1u ||
            bindings[index].argument->kind == EXPR_IDENT ||
            (bindings[index].argument->kind == EXPR_INT_LIT ||
             bindings[index].argument->kind == EXPR_FLOAT_LIT)) {
            continue;
        }
        cost = inline_pure_scalar_expression_cost(bindings[index].argument);
        if (cost == (size_t)-1 ||
            cost > 16u / bindings[index].uses) {
            return false;
        }
    }
    {
        size_t expansion_cost = inline_pure_scalar_expression_cost(returned);
        if (expansion_cost == (size_t)-1 ||
            expansion_cost > INLINE_PURE_SCALAR_EXPANSION_LIMIT) {
            return false;
        }
        /* The return expression cost counts each parameter identifier as one
         * node.  Charge the additional nodes introduced by each substituted
         * pure argument so a large expression cannot bypass the per-argument
         * repeated-use guard merely by using many distinct parameters. */
        for (index = 0u; index < binding_count; ++index) {
            size_t uses = bindings[index].uses;
            size_t cost = inline_pure_scalar_expression_cost(
                bindings[index].argument);
            size_t additional;
            if (uses == 0u || cost <= 1u) continue;
            if (cost == (size_t)-1 ||
                uses > ((size_t)-1) / (cost - 1u)) return false;
            additional = uses * (cost - 1u);
            if (expansion_cost > (size_t)-1 - additional ||
                expansion_cost + additional >
                    INLINE_PURE_SCALAR_EXPANSION_LIMIT) {
                return false;
            }
            expansion_cost += additional;
        }
    }
    {
        Expr* clone = binding_count == 0u
            ? clone_inline_pure_scalar_expression(returned)
            : clone_inline_scalar_expression(returned, bindings,
                                             binding_count);
        if (!clone) return false;
        clone->type = expression->type;
        *expression_out = clone;
    }
    optimize_inline_changed = true;
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
        case EXPR_CXX_TYPEID:
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
        case EXPR_CXX_TYPEID:
            return false;
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
        case EXPR_CXX_TYPEID:
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

static bool statement_contains_loop_transfer(const Stmt* statement) {
    const StmtList* item;
    if (!statement) return false;
    switch (statement->kind) {
        case STMT_BREAK:
        case STMT_CONTINUE:
        case STMT_GOTO:
            return true;
        case STMT_BLOCK:
            for (item = statement->block_stmts; item; item = item->next) {
                if (statement_contains_loop_transfer(item->stmt)) return true;
            }
            return false;
        case STMT_IF:
            return statement_contains_loop_transfer(statement->if_then) ||
                statement_contains_loop_transfer(statement->if_else);
        case STMT_WHILE:
        case STMT_DO:
            return statement_contains_loop_transfer(statement->while_body);
        case STMT_FOR:
            return statement_contains_loop_transfer(statement->for_init) ||
                statement_contains_loop_transfer(statement->for_body);
        case STMT_SWITCH:
            return statement_contains_loop_transfer(statement->switch_body);
        case STMT_CASE:
            return statement_contains_loop_transfer(statement->case_stmt);
        case STMT_DEFAULT:
            return statement_contains_loop_transfer(statement->default_stmt);
        case STMT_LABEL:
            return statement_contains_loop_transfer(statement->label_stmt);
        case STMT_TRY:
            if (statement_contains_loop_transfer(statement->try_body)) {
                return true;
            }
            for (CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                if (statement_contains_loop_transfer(handler->body)) {
                    return true;
                }
            }
            return false;
        default:
            return false;
    }
}

static bool statement_contains_declaration(const Stmt* statement) {
    const StmtList* item;
    if (!statement) return false;
    switch (statement->kind) {
        case STMT_DECL:
            return true;
        case STMT_BLOCK:
            for (item = statement->block_stmts; item; item = item->next) {
                if (statement_contains_declaration(item->stmt)) return true;
            }
            return false;
        case STMT_IF:
            return statement_contains_declaration(statement->if_then) ||
                statement_contains_declaration(statement->if_else);
        case STMT_WHILE:
        case STMT_DO:
            return statement_contains_declaration(statement->while_body);
        case STMT_FOR:
            return statement_contains_declaration(statement->for_init) ||
                statement_contains_declaration(statement->for_body);
        case STMT_SWITCH:
            return statement_contains_declaration(statement->switch_body);
        case STMT_CASE:
            return statement_contains_declaration(statement->case_stmt);
        case STMT_DEFAULT:
            return statement_contains_declaration(statement->default_stmt);
        case STMT_LABEL:
            return statement_contains_declaration(statement->label_stmt);
        case STMT_TRY:
            if (statement_contains_declaration(statement->try_body)) {
                return true;
            }
            for (CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                if (statement_contains_declaration(handler->body)) {
                    return true;
                }
            }
            return false;
        default:
            return false;
    }
}

static bool declaration_is_safe_to_unroll(const Decl* declaration) {
    return declaration && declaration->kind == DECL_VAR &&
        !declaration->var_is_global && !declaration->var_is_static_local &&
        !declaration->var_is_thread_local && !declaration->var_is_vla &&
        declaration->storage != STORAGE_EXTERN &&
        declaration->storage != STORAGE_STATIC && declaration->type &&
        type_is_scalar(declaration->type) && !declaration->type->is_volatile &&
        !declaration->type->is_reference && !declaration->var_is_auto &&
        !declaration->var_is_decltype_auto &&
        !declaration->var_is_auto_reference &&
        !declaration->var_is_auto_rvalue_reference &&
        !declaration->var_cleanup && !declaration->var_cleanups;
}

static bool statement_contains_unroll_unsafe_declaration(
    const Stmt* statement) {
    const StmtList* item;
    if (!statement) return false;
    switch (statement->kind) {
        case STMT_DECL:
            return !declaration_is_safe_to_unroll(statement->decl);
        case STMT_BLOCK:
            for (item = statement->block_stmts; item; item = item->next) {
                if (statement_contains_unroll_unsafe_declaration(item->stmt)) {
                    return true;
                }
            }
            return false;
        case STMT_IF:
            return statement_contains_unroll_unsafe_declaration(
                       statement->if_then) ||
                statement_contains_unroll_unsafe_declaration(
                    statement->if_else);
        case STMT_WHILE:
        case STMT_DO:
            return statement_contains_unroll_unsafe_declaration(
                statement->while_body);
        case STMT_FOR:
            return statement_contains_unroll_unsafe_declaration(
                       statement->for_init) ||
                statement_contains_unroll_unsafe_declaration(
                    statement->for_body);
        case STMT_SWITCH:
            return statement_contains_unroll_unsafe_declaration(
                statement->switch_body);
        case STMT_CASE:
            return statement_contains_unroll_unsafe_declaration(
                statement->case_stmt);
        case STMT_DEFAULT:
            return statement_contains_unroll_unsafe_declaration(
                statement->default_stmt);
        case STMT_LABEL:
            return statement_contains_unroll_unsafe_declaration(
                statement->label_stmt);
        case STMT_TRY:
            if (statement_contains_unroll_unsafe_declaration(
                    statement->try_body)) {
                return true;
            }
            for (const CxxCatch* handler = statement->try_catches;
                 handler; handler = handler->next) {
                if (statement_contains_unroll_unsafe_declaration(
                        handler->body)) {
                    return true;
                }
            }
            return false;
        default:
            return false;
    }
}

/* Bounded while/do expansion may clone only the statement forms handled by
 * clone_unrolled_stmt.  Keep this predicate structural: induction-variable
 * writes, labels, loop transfers, and cleanup-sensitive declarations are
 * checked separately by while_body_constant_step. */
static bool statement_is_unroll_safe_shape(const Stmt* statement) {
    const StmtList* item;
    if (!statement) return false;
    switch (statement->kind) {
        case STMT_NULL:
        case STMT_EXPR:
            return true;
        case STMT_DECL:
            return declaration_is_safe_to_unroll(statement->decl);
        case STMT_BLOCK:
            for (item = statement->block_stmts; item; item = item->next) {
                if (!statement_is_unroll_safe_shape(item->stmt)) return false;
            }
            return true;
        case STMT_IF:
            return statement_is_unroll_safe_shape(statement->if_then) &&
                (!statement->if_else ||
                 statement_is_unroll_safe_shape(statement->if_else));
        default:
            return false;
    }
}

static int unit_for_step(const Expr* increment, const Decl* induction) {
    const Expr* lhs;
    const Expr* rhs;
    int64_t value;
    if (!increment || !induction) return 0;
    if ((increment->kind == EXPR_PREINC ||
         increment->kind == EXPR_POSTINC) &&
        increment->unary_operand &&
        increment->unary_operand->kind == EXPR_IDENT &&
        increment->unary_operand->ident_decl == induction) {
        return 1;
    }
    if ((increment->kind == EXPR_PREDEC ||
         increment->kind == EXPR_POSTDEC) &&
        increment->unary_operand &&
        increment->unary_operand->kind == EXPR_IDENT &&
        increment->unary_operand->ident_decl == induction) {
        return -1;
    }
    if (increment->kind == EXPR_ADD_ASSIGN &&
        increment->binary_lhs && increment->binary_rhs &&
        increment->binary_lhs->kind == EXPR_IDENT &&
        increment->binary_lhs->ident_decl == induction &&
        constant_integer_expression(increment->binary_rhs, &value)) {
        return value == 1 ? 1 : value == -1 ? -1 : 0;
    }
    if (increment->kind == EXPR_SUB_ASSIGN &&
        increment->binary_lhs && increment->binary_rhs &&
        increment->binary_lhs->kind == EXPR_IDENT &&
        increment->binary_lhs->ident_decl == induction &&
        constant_integer_expression(increment->binary_rhs, &value)) {
        return value == 1 ? -1 : 0;
    }
    if (increment->kind != EXPR_ASSIGN || !increment->binary_lhs ||
        !increment->binary_rhs || increment->binary_lhs->kind != EXPR_IDENT ||
        increment->binary_lhs->ident_decl != induction ||
        increment->binary_rhs->kind != EXPR_ADD) {
        if (increment->kind != EXPR_ASSIGN || !increment->binary_lhs ||
            !increment->binary_rhs ||
            increment->binary_lhs->kind != EXPR_IDENT ||
            increment->binary_lhs->ident_decl != induction ||
            increment->binary_rhs->kind != EXPR_SUB) {
            return 0;
        }
        lhs = increment->binary_rhs->binary_lhs;
        rhs = increment->binary_rhs->binary_rhs;
        if (lhs && lhs->kind == EXPR_IDENT &&
            lhs->ident_decl == induction &&
            constant_integer_expression(rhs, &value)) {
            return value == 1 ? -1 : 0;
        }
        return 0;
    }
    lhs = increment->binary_rhs->binary_lhs;
    rhs = increment->binary_rhs->binary_rhs;
    if (lhs && lhs->kind == EXPR_IDENT &&
        lhs->ident_decl == induction &&
        constant_integer_expression(rhs, &value)) {
        return value == 1 ? 1 : 0;
    }
    if (rhs && rhs->kind == EXPR_IDENT &&
        rhs->ident_decl == induction &&
        constant_integer_expression(lhs, &value)) {
        return value == 1 ? 1 : 0;
    }
    return 0;
}

static int bounded_loop_step_value(int64_t value) {
    return value >= -4 && value <= 4 && value != 0 ? (int)value : 0;
}

static int bounded_loop_step(const Expr* increment, const Decl* induction) {
    const Expr* left;
    const Expr* right;
    int64_t value;
    int step = unit_for_step(increment, induction);
    if (step != 0 || !increment || !induction) return step;
    if (increment->kind == EXPR_ADD_ASSIGN && increment->binary_lhs &&
        increment->binary_rhs && increment->binary_lhs->kind == EXPR_IDENT &&
        increment->binary_lhs->ident_decl == induction &&
        constant_integer_expression(increment->binary_rhs, &value)) {
        return bounded_loop_step_value(value);
    }
    if (increment->kind == EXPR_SUB_ASSIGN && increment->binary_lhs &&
        increment->binary_rhs && increment->binary_lhs->kind == EXPR_IDENT &&
        increment->binary_lhs->ident_decl == induction &&
        constant_integer_expression(increment->binary_rhs, &value)) {
        step = bounded_loop_step_value(value);
        return step == 0 ? 0 : -step;
    }
    if (increment->kind != EXPR_ASSIGN || !increment->binary_lhs ||
        !increment->binary_rhs || increment->binary_lhs->kind != EXPR_IDENT ||
        increment->binary_lhs->ident_decl != induction) {
        return 0;
    }
    right = increment->binary_rhs;
    if (right->kind != EXPR_ADD && right->kind != EXPR_SUB) return 0;
    left = right->binary_lhs;
    if (left && left->kind == EXPR_IDENT &&
        left->ident_decl == induction &&
        constant_integer_expression(right->binary_rhs, &value)) {
        step = bounded_loop_step_value(value);
        return right->kind == EXPR_ADD || step == 0 ? step : -step;
    }
    if (right->kind == EXPR_ADD && right->binary_rhs &&
        right->binary_rhs->kind == EXPR_IDENT &&
        right->binary_rhs->ident_decl == induction &&
        constant_integer_expression(right->binary_lhs, &value)) {
        return bounded_loop_step_value(value);
    }
    return 0;
}

static bool for_initializer(const Stmt* initializer, Decl** induction,
                            const Expr** initial_value) {
    const Expr* expression;
    const Expr* left;
    const Expr* right;
    int64_t literal_value;
    if (!initializer || !induction || !initial_value) return false;
    if (initializer->kind == STMT_DECL && initializer->decl &&
        initializer->decl->kind == DECL_VAR &&
        initializer->decl->var_init) {
        *induction = initializer->decl;
        *initial_value = initializer->decl->var_init;
        return true;
    }
    if (initializer->kind != STMT_EXPR || !initializer->expr) return false;
    expression = initializer->expr;
    if (expression->kind != EXPR_ASSIGN) return false;
    left = expression->binary_lhs;
    right = expression->binary_rhs;
    if (!left || left->kind != EXPR_IDENT || !left->ident_decl || !right ||
        !constant_integer_expression(right, &literal_value)) {
        return false;
    }
    *induction = left->ident_decl;
    *initial_value = right;
    return true;
}

static bool for_condition_matches_step(const Expr* condition, int step) {
    if (!condition || step == 0) return false;
    if (condition->kind == EXPR_NE) return true;
    if (step > 0) {
        return condition->kind == EXPR_LT || condition->kind == EXPR_LE;
    }
    return condition->kind == EXPR_GT || condition->kind == EXPR_GE;
}

static bool eliminate_zero_condition_do(Stmt* statement) {
    int64_t condition;
    Stmt* body;
    if (!statement || statement->kind != STMT_DO ||
        !statement->while_body || !statement->while_cond ||
        !constant_integer_expression(statement->while_cond, &condition) ||
        condition != 0 ||
        statement_contains_loop_transfer(statement->while_body)) {
        return false;
    }
    body = statement->while_body;
    *statement = *body;
    return true;
}

static bool eliminate_zero_iteration_for(Stmt* statement) {
    Stmt* initializer;
    Decl* induction;
    const Expr* initial_expression;
    Expr* condition;
    Expr* increment;
    int64_t initial_value;
    int64_t bound_value;
    int step;
    bool zero_iterations;
    StmtList* only;
    if (!statement || statement->kind != STMT_FOR ||
        !statement->for_init || !statement->for_cond ||
        !statement->for_inc || !statement->for_body) {
        return false;
    }
    initializer = statement->for_init;
    if (!for_initializer(initializer, &induction, &initial_expression)) {
        return false;
    }
    condition = statement->for_cond;
    increment = statement->for_inc;
    step = bounded_loop_step(increment, induction);
    if (!induction->type || induction->type->is_volatile ||
        !type_is_integer(induction->type) ||
        !constant_integer_expression(initial_expression, &initial_value) ||
        !for_condition_matches_step(condition, step) ||
        !condition->binary_lhs || condition->binary_lhs->kind != EXPR_IDENT ||
        condition->binary_lhs->ident_decl != induction ||
        !constant_integer_expression(condition->binary_rhs, &bound_value) ||
        step == 0 ||
        statement_contains_label(statement->for_body)) {
        return false;
    }
    if (induction->type->is_unsigned) {
        uint64_t initial_bits = integer_unsigned_value(
            initial_value, induction->type);
        uint64_t bound_bits = integer_unsigned_value(
            bound_value, induction->type);
        if (condition->kind == EXPR_NE) {
            zero_iterations = initial_bits == bound_bits;
        } else if (step > 0) {
            zero_iterations = condition->kind == EXPR_LT
                ? initial_bits >= bound_bits : initial_bits > bound_bits;
        } else {
            zero_iterations = condition->kind == EXPR_GT
                ? initial_bits <= bound_bits : initial_bits < bound_bits;
        }
    } else {
        if (condition->kind == EXPR_NE) {
            zero_iterations = initial_value == bound_value;
        } else if (step > 0) {
            zero_iterations = condition->kind == EXPR_LT
                ? initial_value >= bound_value : initial_value > bound_value;
        } else {
            zero_iterations = condition->kind == EXPR_GT
                ? initial_value <= bound_value : initial_value < bound_value;
        }
    }
    if (!zero_iterations) return false;
    only = ast_arena_alloc(sizeof(*only));
    only->stmt = initializer;
    only->next = NULL;
    statement->kind = STMT_BLOCK;
    statement->block_stmts = only;
    return true;
}

static bool constant_for_iteration_count(const Stmt* statement,
                                         unsigned* count) {
    const Stmt* initializer;
    Decl* induction_decl;
    const Decl* induction;
    const Expr* initial_expression;
    const Expr* condition;
    const Expr* increment;
    int64_t initial_value;
    int64_t bound_value;
    int step;
    unsigned iterations;
    if (!statement || !count || statement->kind != STMT_FOR ||
        !statement->for_init || !statement->for_cond ||
        !statement->for_inc || !statement->for_body ||
        statement_contains_loop_transfer(statement->for_body) ||
        statement_contains_label(statement->for_body) ||
        statement_contains_unroll_unsafe_declaration(statement->for_body)) {
        return false;
    }
    initializer = statement->for_init;
    if (!for_initializer(initializer, &induction_decl,
                         &initial_expression)) {
        return false;
    }
    induction = induction_decl;
    condition = statement->for_cond;
    increment = statement->for_inc;
    step = bounded_loop_step(increment, induction);
    if (!induction->type || induction->type->is_volatile ||
        !type_is_integer(induction->type) ||
        !constant_integer_expression(initial_expression, &initial_value) ||
        !for_condition_matches_step(condition, step) ||
        !condition->binary_lhs || condition->binary_lhs->kind != EXPR_IDENT ||
        condition->binary_lhs->ident_decl != induction ||
        !constant_integer_expression(condition->binary_rhs, &bound_value) ||
        step == 0) {
        return false;
    }
    iterations = 0u;
    if (induction->type->is_unsigned) {
        uint64_t current = integer_unsigned_value(
            initial_value, induction->type);
        uint64_t bound = integer_unsigned_value(
            bound_value, induction->type);
        uint64_t mask = integer_mask(induction->type);
        uint64_t magnitude = (uint64_t)(step > 0 ? step : -step);
        if (condition->kind == EXPR_NE &&
            ((step > 0 && current > bound) ||
             (step < 0 && current < bound))) {
            return false;
        }
        while (iterations <= 4u) {
            bool runs;
            if (condition->kind == EXPR_NE) {
                runs = current != bound;
            } else if (step > 0) {
                runs = condition->kind == EXPR_LT
                    ? current < bound : current <= bound;
            } else {
                runs = condition->kind == EXPR_GT
                    ? current > bound : current >= bound;
            }
            if (!runs) {
                *count = iterations;
                return iterations >= 1u;
            }
            if ((step > 0 && current > mask - magnitude) ||
                (step < 0 && current < magnitude)) return false;
            current = step > 0
                ? (current + magnitude) & mask
                : (current - magnitude) & mask;
            ++iterations;
        }
    } else {
        int64_t current = integer_signed_value(
            initial_value, induction->type);
        int64_t bound = integer_signed_value(
            bound_value, induction->type);
        int64_t minimum;
        int64_t maximum;
        if (!signed_type_limits(induction->type, &minimum, &maximum)) {
            return false;
        }
        if (condition->kind == EXPR_NE &&
            ((step > 0 && current > bound) ||
             (step < 0 && current < bound))) {
            return false;
        }
        while (iterations <= 4u) {
            bool runs;
            if (condition->kind == EXPR_NE) {
                runs = current != bound;
            } else if (step > 0) {
                runs = condition->kind == EXPR_LT
                    ? current < bound : current <= bound;
            } else {
                runs = condition->kind == EXPR_GT
                    ? current > bound : current >= bound;
            }
            if (!runs) {
                *count = iterations;
                return iterations >= 1u;
            }
            if ((step > 0 && current > maximum - step) ||
                (step < 0 && current < minimum - step)) return false;
            current = current + step;
            ++iterations;
        }
    }
    return false;
}

static ExprList* clone_unrolled_expr_list(const ExprList* list) {
    ExprList* result = NULL;
    ExprList** tail = &result;
    for (; list; list = list->next) {
        ExprList* copy = ast_arena_alloc(sizeof(*copy));
        *copy = *list;
        copy->expr = clone_unrolled_expr(list->expr);
        copy->next = NULL;
        *tail = copy;
        tail = &copy->next;
    }
    return result;
}

static GenericAssociation* clone_unrolled_generic_associations(
    const GenericAssociation* list) {
    GenericAssociation* result = NULL;
    GenericAssociation** tail = &result;
    for (; list; list = list->next) {
        GenericAssociation* copy = ast_arena_alloc(sizeof(*copy));
        *copy = *list;
        copy->expr = clone_unrolled_expr(list->expr);
        copy->next = NULL;
        *tail = copy;
        tail = &copy->next;
    }
    return result;
}

static Expr* clone_unrolled_expr(const Expr* expression) {
    Expr* copy;
    if (!expression) return NULL;
    copy = ast_arena_alloc(sizeof(*copy));
    *copy = *expression;
    switch (expression->kind) {
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
            copy->unary_operand = clone_unrolled_expr(
                expression->unary_operand);
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
            copy->binary_lhs = clone_unrolled_expr(expression->binary_lhs);
            copy->binary_rhs = clone_unrolled_expr(expression->binary_rhs);
            break;
        case EXPR_COND:
            copy->cond_test = clone_unrolled_expr(expression->cond_test);
            copy->cond_then = clone_unrolled_expr(expression->cond_then);
            copy->cond_else = clone_unrolled_expr(expression->cond_else);
            break;
        case EXPR_CALL:
            copy->call_func = clone_unrolled_expr(expression->call_func);
            copy->call_args = clone_unrolled_expr_list(expression->call_args);
            copy->call_new_count = clone_unrolled_expr(
                expression->call_new_count);
            copy->call_new_args = clone_unrolled_expr_list(
                expression->call_new_args);
            break;
        case EXPR_INDEX:
            copy->index_base = clone_unrolled_expr(expression->index_base);
            copy->index_expr = clone_unrolled_expr(expression->index_expr);
            break;
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            copy->member_base = clone_unrolled_expr(expression->member_base);
            break;
        case EXPR_CAST:
            copy->cast_expr = clone_unrolled_expr(expression->cast_expr);
            break;
        case EXPR_COMPOUND:
            copy->compound_init = clone_unrolled_expr_list(
                expression->compound_init);
            break;
        case EXPR_GENERIC:
            copy->generic_control = clone_unrolled_expr(
                expression->generic_control);
            copy->generic_associations =
                clone_unrolled_generic_associations(
                    expression->generic_associations);
            break;
        case EXPR_CXX_TYPEID:
            copy->cxx_typeid_operand = clone_unrolled_expr(
                expression->cxx_typeid_operand);
            break;
        case EXPR_VA_START:
        case EXPR_VA_END:
        case EXPR_VA_COPY:
        case EXPR_VA_ARG:
            copy->va_list_operand = clone_unrolled_expr(
                expression->va_list_operand);
            copy->va_second_operand = clone_unrolled_expr(
                expression->va_second_operand);
            break;
        default:
            break;
    }
    return copy;
}

static StmtList* clone_unrolled_stmt_list(const StmtList* list) {
    StmtList* result = NULL;
    StmtList** tail = &result;
    for (; list; list = list->next) {
        StmtList* copy = ast_arena_alloc(sizeof(*copy));
        copy->stmt = clone_unrolled_stmt(list->stmt);
        if (list->stmt && !copy->stmt) return NULL;
        copy->next = NULL;
        *tail = copy;
        tail = &copy->next;
    }
    return result;
}

static Stmt* clone_unrolled_stmt(const Stmt* statement) {
    Stmt* copy;
    if (!statement) return NULL;
    copy = ast_arena_alloc(sizeof(*copy));
    *copy = *statement;
    switch (statement->kind) {
        case STMT_EXPR:
            copy->expr = clone_unrolled_expr(statement->expr);
            break;
        case STMT_BLOCK:
            copy->block_stmts = clone_unrolled_stmt_list(
                statement->block_stmts);
            if (statement->block_stmts && !copy->block_stmts) return NULL;
            break;
        case STMT_DECL:
            if (!declaration_is_safe_to_unroll(statement->decl)) return NULL;
            break;
        case STMT_IF:
            copy->if_cond = clone_unrolled_expr(statement->if_cond);
            copy->if_then = clone_unrolled_stmt(statement->if_then);
            copy->if_else = clone_unrolled_stmt(statement->if_else);
            if ((statement->if_then && !copy->if_then) ||
                (statement->if_else && !copy->if_else)) return NULL;
            break;
        case STMT_RETURN:
            copy->return_val = clone_unrolled_expr(statement->return_val);
            break;
        case STMT_NULL:
            break;
        default:
            return NULL;
    }
    return copy;
}

static StmtList** append_unrolled_stmt(StmtList** tail, Stmt* statement) {
    if (!tail || !statement) return tail;
    if (statement->kind == STMT_BLOCK &&
        !statement_contains_declaration(statement)) {
        StmtList* item = statement->block_stmts;
        while (item) {
            *tail = item;
            tail = &item->next;
            item = item->next;
        }
        return tail;
    }
    {
        StmtList* item = ast_arena_alloc(sizeof(*item));
        item->stmt = statement;
        item->next = NULL;
        *tail = item;
        return &item->next;
    }
}

static bool unroll_constant_for(Stmt* statement, unsigned count) {
    StmtList* head;
    StmtList** tail;
    if (!statement || statement->kind != STMT_FOR || count < 1u ||
        count > 4u || !statement->for_init || !statement->for_body ||
        !statement->for_inc || !clone_unrolled_stmt(statement->for_body)) {
        return false;
    }
    head = ast_arena_alloc(sizeof(*head));
    head->stmt = statement->for_init;
    head->next = NULL;
    tail = &head->next;
    for (unsigned index = 0u; index < count; ++index) {
        Stmt* body = clone_unrolled_stmt(statement->for_body);
        tail = append_unrolled_stmt(tail, body);
        /* The increment also runs after the final completed iteration.  It
         * cannot be dropped merely because the loop condition is false
         * afterwards: a pre-existing induction declaration remains visible
         * after the loop and its final value is observable. */
        {
            StmtList* increment = ast_arena_alloc(sizeof(*increment));
            increment->stmt = stmt_expr(
                index == 0u
                    ? statement->for_inc
                    : clone_unrolled_expr(statement->for_inc),
                statement->for_inc->loc);
            increment->next = NULL;
            *tail = increment;
            tail = &increment->next;
        }
    }
    statement->kind = STMT_BLOCK;
    statement->block_stmts = head;
    return true;
}

static bool unroll_constant_loop(Stmt* statement, unsigned count) {
    StmtList* head = NULL;
    StmtList** tail = &head;
    if (!statement || (statement->kind != STMT_WHILE &&
                       statement->kind != STMT_DO) || count < 1u ||
        count > 4u || !statement->while_body ||
        !clone_unrolled_stmt(statement->while_body)) {
        return false;
    }
    for (unsigned index = 0u; index < count; ++index) {
        Stmt* body = clone_unrolled_stmt(statement->while_body);
        *tail = NULL;
        tail = append_unrolled_stmt(tail, body);
    }
    statement->kind = STMT_BLOCK;
    statement->block_stmts = head;
    return true;
}

static bool unroll_single_iteration_for(Stmt* statement) {
    Stmt* initializer;
    Decl* induction;
    const Expr* initial_expression;
    Expr* condition;
    Expr* increment;
    int64_t initial_value;
    int64_t bound_value;
    int step;
    StmtList* first;
    StmtList** tail;
    if (!statement || statement->kind != STMT_FOR ||
        !statement->for_init || !statement->for_cond ||
        !statement->for_inc || !statement->for_body) {
        return false;
    }
    initializer = statement->for_init;
    if (!for_initializer(initializer, &induction, &initial_expression)) {
        return false;
    }
    condition = statement->for_cond;
    increment = statement->for_inc;
    step = unit_for_step(increment, induction);
    if (!induction->type || induction->type->is_volatile ||
        !type_is_integer(induction->type) ||
        !constant_integer_expression(initial_expression, &initial_value) ||
        !for_condition_matches_step(condition, step) ||
        !condition->binary_lhs || condition->binary_lhs->kind != EXPR_IDENT ||
        condition->binary_lhs->ident_decl != induction ||
        !constant_integer_expression(condition->binary_rhs, &bound_value) ||
        step == 0 ||
        statement_contains_loop_transfer(statement->for_body) ||
        statement_contains_label(statement->for_body) ||
        statement_contains_unroll_unsafe_declaration(statement->for_body)) {
        return false;
    }
    if (induction->type->is_unsigned) {
        uint64_t initial_bits = integer_unsigned_value(
            initial_value, induction->type);
        uint64_t bound_bits = integer_unsigned_value(
            bound_value, induction->type);
        uint64_t mask = integer_mask(induction->type);
        uint64_t expected = step > 0
            ? (initial_bits + 1u) & mask
            : (initial_bits - 1u) & mask;
        if ((step > 0 && condition->kind == EXPR_NE &&
             (initial_bits == mask || bound_bits != expected)) ||
            (step < 0 && condition->kind == EXPR_NE &&
             (initial_bits == 0u || bound_bits != expected)) ||
            (step > 0 && condition->kind == EXPR_LT &&
             (initial_bits == mask || bound_bits != expected)) ||
            (step > 0 && condition->kind == EXPR_LE &&
             bound_bits != initial_bits) ||
            (step < 0 && condition->kind == EXPR_GT &&
             (initial_bits == 0u || bound_bits != expected)) ||
            (step < 0 && condition->kind == EXPR_GE &&
             bound_bits != initial_bits)) {
            return false;
        }
    } else if ((step > 0 && condition->kind == EXPR_NE &&
                (initial_value == INT64_MAX ||
                 bound_value != initial_value + 1)) ||
               (step < 0 && condition->kind == EXPR_NE &&
                (initial_value == INT64_MIN ||
                 bound_value != initial_value - 1)) ||
               (step > 0 && condition->kind == EXPR_LT &&
                (initial_value == INT64_MAX ||
                 bound_value != initial_value + 1)) ||
               (step > 0 && condition->kind == EXPR_LE &&
                bound_value != initial_value) ||
               (step < 0 && condition->kind == EXPR_GT &&
                (initial_value == INT64_MIN ||
                 bound_value != initial_value - 1)) ||
               (step < 0 && condition->kind == EXPR_GE &&
                bound_value != initial_value)) {
        return false;
    }
    first = ast_arena_alloc(sizeof(*first));
    first->stmt = statement->for_init;
    first->next = NULL;
    tail = &first->next;
    tail = append_unrolled_stmt(tail, statement->for_body);
    {
        StmtList* increment = ast_arena_alloc(sizeof(*increment));
        increment->stmt = stmt_expr(statement->for_inc,
                                    statement->for_inc->loc);
        increment->next = NULL;
        *tail = increment;
    }
    statement->kind = STMT_BLOCK;
    statement->block_stmts = first;
    return true;
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
            if (value->kind == EXPR_AND || value->kind == EXPR_OR) {
                bool left_truth;
                if (constant_scalar_truth(value->binary_lhs, &left_truth) &&
                    ((value->kind == EXPR_AND && !left_truth) ||
                     (value->kind == EXPR_OR && left_truth))) {
                    replace_integer(value, value->kind == EXPR_OR);
                    return;
                }
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
            {
                bool condition;
                if (constant_scalar_truth(value->cond_test, &condition)) {
                    Expr** selected = condition
                        ? &value->cond_then : &value->cond_else;
                    optimize_expr(selected);
                    *expression = *selected;
                    return;
                }
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
            if (inline_side_effect_free_scalar_call(expression)) return;
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

    /* The parser's integer-constant evaluator covers typed C17/C++20
     * expressions that are not represented by literal AST nodes, including
     * sizeof/alignof, selected generic associations, and pure conditional or
     * comma expressions.  Fold only expressions accepted by that evaluator;
     * assignments, calls, volatile accesses, and other side-effecting forms
     * therefore remain untouched. */
    if (fold_float_literals(value)) return;

    if (value->kind != EXPR_INT_LIT && value->kind != EXPR_CHAR_LIT &&
        expr_eval_integer_constant(value, &result)) {
        replace_integer(value, result);
        return;
    }

    if (simplify_integer_identity(expression)) return;
    if (simplify_unsigned_power_of_two(expression)) return;
    if (simplify_signed_power_of_two(expression)) return;
    if (simplify_signed_power_of_two_remainder(expression)) return;
    if (simplify_unsigned_small_multiply(expression)) return;

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

struct ConstantState {
    LocalConstant* bindings;
};

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

static bool while_body_constant_step(const Stmt* body, const Decl* induction,
                                     int* step) {
    const Stmt* increment_statement;
    const StmtList* item;
    if (!body || !induction || !step) return false;
    if (body->kind == STMT_BLOCK) {
        item = body->block_stmts;
        if (!item) return false;
        while (item->next) item = item->next;
        increment_statement = item->stmt;
        if (!increment_statement || increment_statement->kind != STMT_EXPR) {
            return false;
        }
        for (item = body->block_stmts; item && item->stmt != increment_statement;
             item = item->next) {
            if (!item->stmt ||
                !statement_is_unroll_safe_shape(item->stmt) ||
                statement_contains_loop_transfer(item->stmt) ||
                statement_transfers_control(item->stmt) ||
                statement_modifies_decl(item->stmt, induction)) {
                return false;
            }
            /* The structural predicate above also admits nested blocks and
             * side-effect-free control selection; the final item remains the
             * induction update handled below. */
        }
    } else {
        if (body->kind != STMT_EXPR) return false;
        increment_statement = body;
    }
    if (statement_contains_label(body) ||
        statement_contains_unroll_unsafe_declaration(body) ||
        statement_contains_loop_transfer(body) ||
        statement_transfers_control(body)) {
        return false;
    }
    *step = bounded_loop_step(increment_statement->expr, induction);
    return *step != 0;
}

static bool constant_loop_condition_holds(const Expr* condition,
                                          const Type* type,
                                          int64_t signed_current,
                                          uint64_t unsigned_current,
                                          int step, int64_t bound_value) {
    if (type->is_unsigned) {
        uint64_t bound = integer_unsigned_value(bound_value, type);
        if (condition->kind == EXPR_NE) return unsigned_current != bound;
        if (step > 0) {
            return condition->kind == EXPR_LT
                ? unsigned_current < bound : unsigned_current <= bound;
        }
        return condition->kind == EXPR_GT
            ? unsigned_current > bound : unsigned_current >= bound;
    }
    {
        int64_t bound = integer_signed_value(bound_value, type);
        if (condition->kind == EXPR_NE) return signed_current != bound;
        if (step > 0) {
            return condition->kind == EXPR_LT
                ? signed_current < bound : signed_current <= bound;
        }
        return condition->kind == EXPR_GT
            ? signed_current > bound : signed_current >= bound;
    }
}

static bool constant_loop_ne_direction_valid(const Expr* condition,
                                             const Type* type,
                                             int64_t signed_current,
                                             uint64_t unsigned_current,
                                             int step, int64_t bound_value) {
    if (condition->kind != EXPR_NE) return true;
    if (type->is_unsigned) {
        uint64_t bound = integer_unsigned_value(bound_value, type);
        return step > 0 ? unsigned_current <= bound
                        : unsigned_current >= bound;
    }
    {
        int64_t bound = integer_signed_value(bound_value, type);
        return step > 0 ? signed_current <= bound
                        : signed_current >= bound;
    }
}

static bool advance_constant_loop_value(const Type* type, int step,
                                        int64_t* signed_current,
                                        uint64_t* unsigned_current) {
    uint64_t distance = (uint64_t)(step > 0 ? step : -step);
    if (type->is_unsigned) {
        uint64_t mask = integer_mask(type);
        if (step > 0) {
            if (distance > mask || *unsigned_current > mask - distance) {
                return false;
            }
            *unsigned_current = (*unsigned_current + distance) & mask;
        } else {
            if (*unsigned_current < distance) return false;
            *unsigned_current = (*unsigned_current - distance) & mask;
        }
        return true;
    }
    {
        int64_t minimum;
        int64_t maximum;
        if (!signed_type_limits(type, &minimum, &maximum) ||
            (step > 0 &&
             *signed_current > maximum - (int64_t)distance) ||
            (step < 0 &&
             *signed_current < minimum + (int64_t)distance)) {
            return false;
        }
        *signed_current = step > 0
            ? *signed_current + (int64_t)distance
            : *signed_current - (int64_t)distance;
        return true;
    }
}

static bool constant_loop_iteration_count(const Stmt* statement,
                                          const ConstantState* state,
                                          bool do_first, unsigned* count) {
    const Expr* condition;
    const Expr* left;
    const LocalConstant* binding;
    const Decl* induction;
    int64_t bound_value;
    int step;
    unsigned iterations;
    int64_t signed_current = 0;
    uint64_t unsigned_current = 0u;
    if (!statement || !state || !count ||
        (statement->kind != STMT_WHILE && statement->kind != STMT_DO) ||
        !statement->while_cond || !statement->while_body) {
        return false;
    }
    condition = statement->while_cond;
    if (!condition->binary_lhs || !condition->binary_rhs ||
        condition->binary_lhs->kind != EXPR_IDENT ||
        !condition->binary_lhs->ident_decl ||
        !constant_integer_expression(condition->binary_rhs, &bound_value)) {
        return false;
    }
    left = condition->binary_lhs;
    induction = left->ident_decl;
    if (!induction->type || induction->type->is_volatile ||
        !type_is_integer(induction->type) ||
        (!for_condition_matches_step(condition, 1) &&
         !for_condition_matches_step(condition, -1))) {
        return false;
    }
    binding = find_local_constant((ConstantState*)state, induction);
    if (!binding || !binding->known ||
        !while_body_constant_step(statement->while_body, induction, &step)) {
        return false;
    }
    if (!for_condition_matches_step(condition, step)) return false;
    if (induction->type->is_unsigned) {
        unsigned_current = integer_unsigned_value(
            binding->value, induction->type);
    } else {
        signed_current = integer_signed_value(
            binding->value, induction->type);
    }
    if (!do_first && !constant_loop_ne_direction_valid(
            condition, induction->type, signed_current, unsigned_current,
            step, bound_value)) {
        return false;
    }
    iterations = 0u;
    while (iterations < 4u) {
        if (!do_first && !constant_loop_condition_holds(
                condition, induction->type, signed_current, unsigned_current,
                step, bound_value)) {
            *count = iterations;
            return iterations >= 1u;
        }
        if (!advance_constant_loop_value(induction->type, step,
                                         &signed_current,
                                         &unsigned_current)) {
            return false;
        }
        ++iterations;
        if (!constant_loop_ne_direction_valid(
                condition, induction->type, signed_current, unsigned_current,
                step, bound_value)) {
            return false;
        }
        if (!constant_loop_condition_holds(
                condition, induction->type, signed_current, unsigned_current,
                step, bound_value)) {
            *count = iterations;
            return true;
        }
    }
    return false;
}

static bool constant_while_iteration_count(const Stmt* statement,
                                           const ConstantState* state,
                                           unsigned* count) {
    return constant_loop_iteration_count(statement, state, false, count);
}

static bool constant_do_iteration_count(const Stmt* statement,
                                        const ConstantState* state,
                                        unsigned* count) {
    return constant_loop_iteration_count(statement, state, true, count);
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
        case EXPR_CXX_TYPEID:
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
                {
                    unsigned iteration_count = 0u;
                    if (constant_while_iteration_count(
                            current, &state, &iteration_count) &&
                        unroll_constant_loop(current, iteration_count)) {
                        optimize_block(current);
                        clear_local_constants(&state);
                        break;
                    }
                }
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
                {
                    unsigned iteration_count = 0u;
                    if (constant_do_iteration_count(
                            current, &state, &iteration_count) &&
                        unroll_constant_loop(current, iteration_count)) {
                        optimize_block(current);
                        clear_local_constants(&state);
                        break;
                    }
                }
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
        case EXPR_CXX_TYPEID:
            /* Non-polymorphic typeid operands are unevaluated in this
             * bounded implementation and carry only a static identity. */
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
        case EXPR_CXX_TYPEID:
            /* Static typeid has no evaluated operand and no local read. */
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

/* A constant switch can be reduced to the statements reached by its selected
 * label, but only when the selected path has no control-flow construct whose
 * target would change after the switch wrapper disappears.  In particular,
 * a break is accepted only as the direct statement attached to a case label;
 * a nested break would become an outer-loop break after this rewrite. */
static bool constant_switch_statement_safe(const Stmt* statement,
                                           bool direct_case_statement) {
    if (!statement) return true;
    switch (statement->kind) {
        case STMT_BLOCK:
            for (const StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                if (!constant_switch_statement_safe(item->stmt, false)) {
                    return false;
                }
            }
            return true;
        case STMT_EXPR:
        case STMT_NULL:
        case STMT_ASM:
        case STMT_RETURN:
        case STMT_THROW:
            return true;
        case STMT_BREAK:
            return direct_case_statement;
        case STMT_DECL:
            return statement->decl && statement->decl->kind == DECL_VAR &&
                !statement->decl->var_cleanup &&
                !statement->decl->var_cleanups &&
                (!statement->decl->type ||
                 !statement->decl->type->cleanup_function);
        default:
            return false;
    }
}

static uint64_t constant_switch_bits(int64_t value, const Type* type) {
    unsigned width = type && type->size > 0 ? (unsigned)type->size * 8u : 64u;
    uint64_t bits = (uint64_t)value;
    if (width < 64u) bits &= (UINT64_C(1) << width) - 1u;
    return bits;
}

static bool constant_switch_label_matches(const Stmt* label,
                                          const Expr* switch_expression,
                                          uint64_t selector) {
    int64_t value;
    if (!label || label->kind != STMT_CASE || !label->case_val ||
        !expr_eval_integer_constant(label->case_val, &value)) {
        return false;
    }
    return constant_switch_bits(value, switch_expression->type) == selector;
}

static bool constant_integer_expression(const Expr* expression,
                                        int64_t* value) {
    return expression && value &&
        expr_eval_integer_constant((Expr*)expression, value);
}

static bool fold_constant_switch(Stmt* statement) {
    const Type* switch_type;
    StmtList* selected = NULL;
    StmtList* fallback = NULL;
    StmtList* first = NULL;
    StmtList** tail = &first;
    uint64_t selector;
    int64_t selector_value;
    bool started = false;

    if (!statement || statement->kind != STMT_SWITCH ||
        !statement->switch_expr || !statement->switch_body ||
        statement->switch_body->kind != STMT_BLOCK ||
        !constant_integer_expression(statement->switch_expr,
                                     &selector_value)) {
        return false;
    }
    switch_type = statement->switch_expr->type;
    if (!switch_type || (!type_is_integer((Type*)switch_type) &&
                         switch_type->kind != TYPE_ENUM)) {
        return false;
    }
    selector = constant_switch_bits(selector_value, switch_type);

    /* Validate the complete switch before selecting a path.  This keeps the
     * optimization conservative when another arm contains a label, a loop,
     * cleanup, or any other construct whose reachability is non-local. */
    for (StmtList* item = statement->switch_body->block_stmts; item;
         item = item->next) {
        Stmt* current = item->stmt;
        if (!current) return false;
        if (current->kind == STMT_CASE) {
            if (!constant_switch_statement_safe(current->case_stmt, true)) {
                return false;
            }
            if (!selected && constant_switch_label_matches(
                    current, statement->switch_expr, selector)) {
                selected = item;
            }
        } else if (current->kind == STMT_DEFAULT) {
            if (!constant_switch_statement_safe(current->default_stmt, true)) {
                return false;
            }
            if (!fallback) fallback = item;
        } else if (!constant_switch_statement_safe(current, true)) {
            return false;
        }
    }
    if (!selected) selected = fallback;
    if (!selected) {
        statement->kind = STMT_NULL;
        statement->switch_expr = NULL;
        statement->switch_body = NULL;
        return true;
    }

    for (StmtList* item = selected; item; item = item->next) {
        Stmt* current = item->stmt;
        Stmt* body;
        StmtList* copy;
        if (!current) return false;
        if (current->kind == STMT_CASE) {
            body = current->case_stmt;
        } else if (current->kind == STMT_DEFAULT) {
            body = current->default_stmt;
        } else {
            body = current;
        }
        if (!body) return false;
        if (body->kind == STMT_BREAK) break;
        copy = rcc_alloc(sizeof(*copy));
        copy->stmt = body;
        copy->next = NULL;
        *tail = copy;
        tail = &copy->next;
        started = true;
        if (body->kind == STMT_RETURN || body->kind == STMT_THROW ||
            statement_transfers_control(body)) {
            break;
        }
    }
    if (!started) {
        statement->kind = STMT_NULL;
        statement->switch_expr = NULL;
        statement->switch_body = NULL;
        return true;
    }
    statement->kind = STMT_BLOCK;
    statement->block_no_scope = false;
    statement->block_stmts = first;
    return true;
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
                if (constant_integer_expression(statement->if_cond,
                                                 &condition)) {
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
                if (constant_integer_expression(statement->while_cond,
                                                 &condition) &&
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
            eliminate_zero_condition_do(statement);
            break;
        case STMT_FOR:
            optimize_stmt(statement->for_init);
            optimize_expr(&statement->for_cond);
            {
                int64_t condition;
                if (constant_integer_expression(statement->for_cond,
                                                 &condition) &&
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
            if (eliminate_zero_iteration_for(statement)) {
                optimize_block(statement);
            } else {
                unsigned iteration_count = 0u;
                if (constant_for_iteration_count(
                        statement, &iteration_count) &&
                    unroll_constant_for(statement, iteration_count)) {
                    optimize_block(statement);
                } else if (unroll_single_iteration_for(statement)) {
                    optimize_block(statement);
                } else {
                    optimize_stmt(statement->for_body);
                }
            }
            break;
        case STMT_SWITCH:
            optimize_expr(&statement->switch_expr);
            optimize_stmt(statement->switch_body);
            (void)fold_constant_switch(statement);
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
    enum { OPTIMIZE_INLINE_PASSES = 8 };
    unsigned pass;
    if (!ast || g_opts.opt_level <= 0) return;
    optimize_inline_ast = ast;
    for (pass = 0u; pass < OPTIMIZE_INLINE_PASSES; ++pass) {
        optimize_inline_changed = false;
        for (DeclList* item = ast->decls; item; item = item->next) {
            Decl* declaration = item->decl;
            if (!declaration) continue;
            if (declaration->kind == DECL_VAR) {
                optimize_expr(&declaration->var_init);
            } else if (declaration->kind == DECL_FUNC) {
                optimize_stmt(declaration->func_body);
            }
        }
        if (!optimize_inline_changed) break;
    }
    optimize_inline_ast = NULL;
    if (!rcc_ir_verify_ast_subset(ast, &lowered_functions, ir_error,
                                  sizeof(ir_error))) {
        rcc_fatal("typed SSA lowering failed: %s", ir_error);
    }
    if (g_opts.verbose) {
        printf("Typed SSA shadow verification: %zu function(s)\n",
               lowered_functions);
    }
}
