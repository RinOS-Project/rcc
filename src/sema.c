/*
 * RCC - RinOS C Compiler
 * Semantic Analysis
 */

#include "rcc.h"
#include "ast.h"
#include "symtab.h"
#include <limits.h>

/* Current function return type */
static Type* current_func_ret = NULL;
static bool current_func_variadic = false;
static Decl* current_func_last_param = NULL;

typedef struct SemaSwitchValue {
    uint64_t bits;
    struct SemaSwitchValue* next;
} SemaSwitchValue;

typedef struct SemaSwitchContext {
    Type* control_type;
    SemaSwitchValue* values;
    bool has_default;
    struct SemaSwitchContext* previous;
} SemaSwitchContext;

static SemaSwitchContext* current_switch = NULL;
static int loop_depth = 0;

/* Forward declarations */
static void sema_stmt(Stmt* stmt);
static Type* sema_expr(Expr* expr);
static void sema_decl(Decl* decl);
static void sema_initializer(Type* type, Expr* initializer);
static bool sema_atomic_builtin_call(Expr* expr);

static Type* sema_switch_control_type(Type* type) {
    if (!type || type->kind == TYPE_ENUM || type->kind < TYPE_INT) {
        return type_int;
    }
    return type;
}

static uint64_t sema_switch_value_bits(int64_t value, Type* control_type) {
    unsigned width = control_type && control_type->size > 0
        ? (unsigned)control_type->size * 8u : 32u;
    uint64_t bits = (uint64_t)value;
    if (width < 64u) bits &= (UINT64_C(1) << width) - 1u;
    return bits;
}

static void sema_switch_release_values(SemaSwitchValue* value) {
    while (value) {
        SemaSwitchValue* next = value->next;
        rcc_free(value);
        value = next;
    }
}

static Expr* sema_call_argument(Expr* call, int index) {
    ExprList* argument = call ? call->call_args : NULL;
    while (argument && index-- > 0) argument = argument->next;
    return argument ? argument->expr : NULL;
}

static bool sema_atomic_order(Expr* call, const char* name, int index,
                              int64_t* value, bool* is_constant) {
    Expr* order = sema_call_argument(call, index);
    int64_t evaluated;
    if (is_constant) *is_constant = false;
    if (!order) return false;
    if (!order->type ||
        (!type_is_integer(order->type) && order->type->kind != TYPE_ENUM)) {
        rcc_error(order->loc, "%s memory order must have integer type", name);
        return false;
    }
    if (!expr_eval_integer_constant(order, &evaluated)) return true;
    if (is_constant) *is_constant = true;
    if (value) *value = evaluated;
    if (evaluated < 0 || evaluated > 5) {
        rcc_error(order->loc, "%s memory order is outside the range 0..5",
                  name);
        return false;
    }
    return true;
}

static bool atomic_failure_order_allowed(int64_t success, int64_t failure) {
    if (failure == 3 || failure == 4) return false;
    switch (success) {
        case 0: return failure == 0;
        case 1: return failure == 0 || failure == 1;
        case 2: return failure == 0 || failure == 1 || failure == 2;
        case 3: return failure == 0;
        case 4: return failure == 0 || failure == 1 || failure == 2;
        case 5: return failure == 0 || failure == 1 || failure == 2 ||
                       failure == 5;
        default: return false;
    }
}

static bool atomic_allows_pointer_value(const char* name) {
    return strcmp(name, "__atomic_load_n") == 0 ||
           strcmp(name, "__atomic_store_n") == 0 ||
           strcmp(name, "__atomic_exchange_n") == 0 ||
           strcmp(name, "__atomic_compare_exchange_n") == 0;
}

/* ═══════════════════════════════════════
 * Type Checking Helpers
 * ═══════════════════════════════════════ */

static bool is_lvalue(Expr* e) {
    switch (e->kind) {
        case EXPR_IDENT:
        case EXPR_DEREF:
        case EXPR_INDEX:
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
        case EXPR_COMPOUND:
            return true;
        case EXPR_CALL:
            return e->call_method && e->call_method->return_type &&
                   e->call_method->return_type->is_reference;
        default:
            return false;
    }
}

static Type* get_pointer_base(Type* t) {
    if (t->kind == TYPE_PTR) return t->base;
    if (t->kind == TYPE_ARRAY) return t->base;
    return NULL;
}

static bool is_pointer_arithmetic_type(Type* type) {
    Type* base = get_pointer_base(type);
    return base && base->kind != TYPE_VOID && base->kind != TYPE_FUNC &&
           base->size > 0;
}

static Type* generic_selection_type(Type* type) {
    if (!type) return NULL;
    if (type->kind == TYPE_ARRAY) return type_ptr(type->base);
    if (type->kind == TYPE_FUNC) return type_ptr(type);
    return type;
}

static bool sema_is_integer_type(Type* type) {
    return type && (type_is_integer(type) || type->kind == TYPE_ENUM);
}

static Type* sema_integer_promotion(Type* type) {
    if (!type || type->kind == TYPE_ENUM || type->kind < TYPE_INT) {
        return type_int;
    }
    return type;
}

static Type* implicit_cast(Expr* e, Type* target) {
    if (!e->type || !target) return NULL;

    if (target->is_reference) {
        Type* referred = target->base;
        /* Reference arguments are passed as addresses by the backend, so the
         * supported subset deliberately requires addressable expressions. */
        if (!referred || !is_lvalue(e)) return NULL;
        if (e->type->is_const && !referred->is_const) return NULL;
        return type_is_compatible(e->type, referred) ? target : NULL;
    }

    /* Same type */
    if (e->type == target) return target;

    if ((target->kind == TYPE_STRUCT || target->kind == TYPE_UNION) &&
        type_is_compatible(e->type, target)) {
        return target;
    }

    /* Integer promotions */
    if ((type_is_integer(e->type) || e->type->kind == TYPE_ENUM) &&
        (type_is_integer(target) || target->kind == TYPE_ENUM)) {
        return target;
    }

    if (type_is_arithmetic(e->type) && type_is_arithmetic(target)) {
        return target;
    }

    /* Pointer to/from integer */
    if (type_is_pointer(e->type) && type_is_integer(target)) {
        return target;
    }
    if (type_is_integer(e->type) && type_is_pointer(target)) {
        return target;
    }

    /* Array to pointer decay */
    if (type_is_array(e->type) && type_is_pointer(target)) {
        if ((target->base && target->base->kind == TYPE_VOID) ||
            type_is_compatible(e->type->base, target->base)) {
            return target;
        }
    }
    if (type_is_function(e->type) && type_is_pointer(target) &&
        type_is_compatible(e->type, target->base)) {
        return target;
    }

    /* void* conversions */
    if (type_is_pointer(e->type) && type_is_pointer(target)) {
        if ((e->type->base && e->type->base->kind == TYPE_VOID) ||
            (target->base && target->base->kind == TYPE_VOID)) {
            return target;
        }
        if (type_is_compatible(e->type->base, target->base)) {
            return target;
        }
    }

    return NULL;
}

static TypeMethod* sema_find_inline_method(Type* aggregate,
                                           const char* name) {
    TypeMethod* method;
    if (!aggregate || !name ||
        (aggregate->kind != TYPE_STRUCT && aggregate->kind != TYPE_UNION)) {
        return NULL;
    }
    for (method = aggregate->methods; method; method = method->next) {
        if (method->name && strcmp(method->name, name) == 0) return method;
    }
    return NULL;
}

static bool cxx_same_parameter_type(Type* source, Type* target,
                                    bool top_level) {
    if (!source || !target || source->kind != target->kind) return false;
    if (source->is_reference != target->is_reference ||
        source->is_rvalue_reference != target->is_rvalue_reference) {
        return false;
    }
    if (!top_level && source->is_const != target->is_const) return false;
    if (type_is_integer(source) &&
        source->is_unsigned != target->is_unsigned) {
        return false;
    }
    if (source->kind == TYPE_PTR) {
        return cxx_same_parameter_type(source->base, target->base, false);
    }
    if (source->kind == TYPE_ARRAY) {
        return (source->array_len < 0 || target->array_len < 0 ||
                source->array_len == target->array_len) &&
               cxx_same_parameter_type(source->base, target->base, false);
    }
    return type_is_compatible(source, target);
}

static int cxx_conversion_rank(Expr* argument, Type* target) {
    Type* source;
    Type* source_base;
    Type* target_base;

    if (!argument || !argument->type || !target) return -1;
    source = argument->type;
    if (target->is_reference) {
        target_base = target->base;
        if (!target_base || !is_lvalue(argument)) return -1;
        if (source->is_const && !target_base->is_const) return -1;
        if (cxx_same_parameter_type(source, target_base, false)) return 0;
        return type_is_compatible(source, target_base) ? 1 : -1;
    }
    if (cxx_same_parameter_type(source, target, true)) return 0;

    if (source->kind == TYPE_ARRAY && target->kind == TYPE_PTR) {
        source_base = source->base;
        target_base = target->base;
        if (source_base && target_base && source_base->is_const &&
            !target_base->is_const) {
            return -1;
        }
        if ((target_base && target_base->kind == TYPE_VOID) ||
            type_is_compatible(source_base, target_base)) {
            return cxx_same_parameter_type(source_base, target_base, false)
                ? 1 : 2;
        }
        return -1;
    }

    if (source->kind == TYPE_PTR && target->kind == TYPE_PTR) {
        source_base = source->base;
        target_base = target->base;
        if (!source_base || !target_base) return -1;
        /* Standard qualification conversion may add, but never remove,
         * pointee constness. */
        if (source_base->is_const && !target_base->is_const) return -1;
        if (type_is_compatible(source_base, target_base)) return 1;
        if (source_base->kind == TYPE_VOID || target_base->kind == TYPE_VOID) {
            return 2;
        }
        return -1;
    }

    if ((type_is_integer(source) || source->kind == TYPE_ENUM) &&
        (type_is_integer(target) || target->kind == TYPE_ENUM)) {
        if ((source->kind == TYPE_ENUM || source->kind < TYPE_INT) &&
            target == type_int) {
            return 1;
        }
        return 2;
    }
    if (type_is_arithmetic(source) && type_is_arithmetic(target)) return 2;
    if ((target->kind == TYPE_STRUCT || target->kind == TYPE_UNION) &&
        type_is_compatible(source, target)) {
        return 0;
    }
    if (source->kind == TYPE_INT && type_is_pointer(target) &&
        argument->kind == EXPR_INT_LIT && argument->int_val == 0) {
        return 2;
    }
    return -1;
}

static bool cxx_same_function_parameters(Type* left, Type* right) {
    TypeParam* left_parameter;
    TypeParam* right_parameter;

    if (!left || !right || left->kind != TYPE_FUNC ||
        right->kind != TYPE_FUNC || left->variadic != right->variadic) {
        return false;
    }
    left_parameter = left->params;
    right_parameter = right->params;
    while (left_parameter && right_parameter) {
        if (!cxx_same_parameter_type(left_parameter->type,
                                     right_parameter->type, true)) {
            return false;
        }
        left_parameter = left_parameter->next;
        right_parameter = right_parameter->next;
    }
    return left_parameter == NULL && right_parameter == NULL;
}

static Decl* sema_select_cxx_overload(Expr* call) {
    Decl* candidate;
    Decl* best = NULL;
    int best_total = INT_MAX;
    int best_worst = INT_MAX;
    bool ambiguous = false;

    if (!call || !call->call_func ||
        call->call_func->kind != EXPR_IDENT ||
        !call->call_func->ident_decl) {
        return NULL;
    }
    candidate = call->call_func->ident_decl;
    for (; candidate; candidate = candidate->func_overload_next) {
        TypeParam* parameter;
        ExprList* argument;
        int total = 0;
        int worst = 0;
        bool viable = true;

        if (candidate->kind != DECL_FUNC || !candidate->type ||
            candidate->type->kind != TYPE_FUNC) {
            continue;
        }
        parameter = candidate->type->params;
        argument = call->call_args;
        while (argument && parameter) {
            int rank = cxx_conversion_rank(argument->expr, parameter->type);
            if (rank < 0) {
                viable = false;
                break;
            }
            total += rank;
            if (rank > worst) worst = rank;
            argument = argument->next;
            parameter = parameter->next;
        }
        if (!viable || parameter) continue;
        if (argument) {
            if (!candidate->type->variadic) continue;
            while (argument) {
                total += 8;
                worst = 8;
                argument = argument->next;
            }
        }
        if (!best || worst < best_worst ||
            (worst == best_worst && total < best_total)) {
            best = candidate;
            best_total = total;
            best_worst = worst;
            ambiguous = false;
        } else if (worst == best_worst && total == best_total) {
            ambiguous = true;
        }
    }
    if (!best) {
        rcc_error(call->loc, "no matching overload for '%s'",
                  call->call_func->ident_name);
        return NULL;
    }
    if (ambiguous) {
        rcc_error(call->loc, "ambiguous overload for '%s'",
                  call->call_func->ident_name);
        return NULL;
    }
    return best;
}

/* ═══════════════════════════════════════
 * Expression Semantic Analysis
 * ═══════════════════════════════════════ */

static Type* sema_expr(Expr* expr) {
    if (!expr) return NULL;

    switch (expr->kind) {
        case EXPR_INT_LIT:
            if (!expr->type) expr->type = type_int;
            break;

        case EXPR_FLOAT_LIT:
            expr->type = type_double;
            break;

        case EXPR_CHAR_LIT:
            expr->type = type_int;
            break;

        case EXPR_STRING_LIT:
            expr->type = type_array(type_char,
                                    (int)strlen(expr->str_val) + 1);
            break;

        case EXPR_IDENT: {
            Symbol* sym = symtab_lookup(g_symtab, expr->ident_name);
            if (!sym) {
                rcc_error(expr->loc, "undefined identifier '%s'", expr->ident_name);
                expr->type = type_int;
            } else {
                expr->ident_decl = sym->decl;
                expr->type = sym->type && sym->type->is_reference
                    ? sym->type->base : sym->type;
                if (sym->kind == SYM_FUNC && sym->decl &&
                    sym->decl->func_overload_next) {
                    rcc_error(expr->loc,
                              "overloaded function '%s' requires call context",
                              expr->ident_name);
                }
            }
            break;
        }

        case EXPR_NEG: {
            Type* t = sema_expr(expr->unary_operand);
            if (!type_is_arithmetic(t) && t->kind != TYPE_ENUM) {
                rcc_error(expr->loc, "invalid operand type for unary operator");
            }
            expr->type = sema_is_integer_type(t)
                ? sema_integer_promotion(t) : t;
            break;
        }

        case EXPR_BITNOT: {
            Type* t = sema_expr(expr->unary_operand);
            if (!sema_is_integer_type(t)) {
                rcc_error(expr->loc,
                          "bitwise complement requires integer operand");
            }
            expr->type = sema_integer_promotion(t);
            break;
        }

        case EXPR_NOT: {
            Type* t = sema_expr(expr->unary_operand);
            if (!type_is_scalar(t) && t->kind != TYPE_ENUM &&
                t->kind != TYPE_ARRAY && t->kind != TYPE_FUNC) {
                rcc_error(expr->loc, "logical not requires scalar operand");
            }
            expr->type = type_int;
            break;
        }

        case EXPR_ADDR: {
            Type* t = sema_expr(expr->unary_operand);
            if (!is_lvalue(expr->unary_operand)) {
                rcc_error(expr->loc, "cannot take address of rvalue");
            }
            expr->type = type_ptr(t);
            break;
        }

        case EXPR_DEREF: {
            Type* t = sema_expr(expr->unary_operand);
            Type* base = get_pointer_base(t);
            if (!base) {
                rcc_error(expr->loc, "cannot dereference non-pointer");
                expr->type = type_int;
            } else {
                expr->type = base;
            }
            break;
        }

        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC: {
            Type* t = sema_expr(expr->unary_operand);
            if (!is_lvalue(expr->unary_operand)) {
                rcc_error(expr->loc, "increment/decrement requires lvalue");
            }
            if (!type_is_arithmetic(t) &&
                !(type_is_pointer(t) && is_pointer_arithmetic_type(t))) {
                rcc_error(expr->loc,
                          "increment/decrement requires arithmetic or object pointer type");
            }
            expr->type = t;
            break;
        }

        case EXPR_SIZEOF: {
            if (expr->sizeof_type) {
                expr->type = type_uint;
            } else {
                sema_expr(expr->unary_operand);
                expr->type = type_uint;
            }
            break;
        }

        case EXPR_CAST: {
            sema_expr(expr->cast_expr);
            expr->type = expr->cast_type;
            break;
        }

        case EXPR_ALIGNOF:
            expr->type = type_uint;
            break;

        case EXPR_GENERIC: {
            Type* control = generic_selection_type(
                sema_expr(expr->generic_control));
            GenericAssociation* selected = NULL;
            GenericAssociation* fallback = NULL;
            for (GenericAssociation* association =
                     expr->generic_associations;
                 association; association = association->next) {
                sema_expr(association->expr);
                if (!association->type) {
                    fallback = association;
                } else if (control &&
                           type_is_compatible(control,
                                              association->type)) {
                    if (selected) {
                        rcc_error(association->loc,
                                  "generic selection matches more than one association");
                    } else {
                        selected = association;
                    }
                }
            }
            if (!selected) selected = fallback;
            if (!selected) {
                rcc_error(expr->loc,
                          "generic selection has no compatible association");
                expr->type = type_int;
            } else {
                Expr replacement = *selected->expr;
                *expr = replacement;
            }
            break;
        }

        case EXPR_ADD:
        case EXPR_SUB: {
            Type* lt = sema_expr(expr->binary_lhs);
            Type* rt = sema_expr(expr->binary_rhs);
            bool left_pointer = type_is_pointer(lt) || type_is_array(lt);
            bool right_pointer = type_is_pointer(rt) || type_is_array(rt);
            Type* left_result = type_is_array(lt) ? type_ptr(lt->base) : lt;
            Type* right_result = type_is_array(rt) ? type_ptr(rt->base) : rt;

            /* Pointer arithmetic */
            if (left_pointer && type_is_integer(rt)) {
                if (!is_pointer_arithmetic_type(lt)) {
                    rcc_error(expr->loc,
                              "pointer arithmetic requires a complete object type");
                }
                expr->type = left_result;
            } else if (type_is_integer(lt) && right_pointer &&
                       expr->kind == EXPR_ADD) {
                if (!is_pointer_arithmetic_type(rt)) {
                    rcc_error(expr->loc,
                              "pointer arithmetic requires a complete object type");
                }
                expr->type = right_result;
            } else if (left_pointer && right_pointer &&
                       expr->kind == EXPR_SUB) {
                Type* left_base = get_pointer_base(lt);
                Type* right_base = get_pointer_base(rt);
                if (!is_pointer_arithmetic_type(lt) ||
                    !is_pointer_arithmetic_type(rt) ||
                    !type_is_compatible(left_base, right_base)) {
                    rcc_error(expr->loc,
                              "pointer subtraction requires compatible complete object types");
                }
                expr->type = type_long;  /* ptrdiff_t */
            } else if (type_is_arithmetic(lt) && type_is_arithmetic(rt)) {
                expr->type = type_common(lt, rt);
            } else {
                rcc_error(expr->loc, "invalid operands to binary +/-");
                expr->type = type_int;
            }
            break;
        }

        case EXPR_MUL:
        case EXPR_DIV: {
            Type* lt = sema_expr(expr->binary_lhs);
            Type* rt = sema_expr(expr->binary_rhs);
            if (!type_is_arithmetic(lt) || !type_is_arithmetic(rt)) {
                rcc_error(expr->loc, "invalid operands to binary operator");
            }
            expr->type = type_common(lt, rt);
            break;
        }

        case EXPR_VA_START: {
            Type* list_type = sema_expr(expr->va_list_operand);
            sema_expr(expr->va_second_operand);
            if (!current_func_variadic) {
                rcc_error(expr->loc,
                          "va_start is only valid in a variadic function");
            }
            if (!list_type || (list_type->kind != TYPE_ARRAY &&
                               list_type->kind != TYPE_PTR)) {
                rcc_error(expr->loc, "va_start requires a va_list object");
            }
            if (!expr->va_second_operand ||
                expr->va_second_operand->kind != EXPR_IDENT ||
                !expr->va_second_operand->ident_decl ||
                expr->va_second_operand->ident_decl->kind != DECL_PARAM ||
                expr->va_second_operand->ident_decl !=
                    current_func_last_param) {
                rcc_error(expr->loc,
                          "va_start requires the final named parameter");
            }
            expr->type = type_void;
            break;
        }

        case EXPR_VA_END: {
            Type* list_type = sema_expr(expr->va_list_operand);
            if (!list_type || (list_type->kind != TYPE_ARRAY &&
                               list_type->kind != TYPE_PTR)) {
                rcc_error(expr->loc, "va_end requires a va_list object");
            }
            expr->type = type_void;
            break;
        }

        case EXPR_VA_COPY: {
            Type* destination = sema_expr(expr->va_list_operand);
            Type* source = sema_expr(expr->va_second_operand);
            if (!destination || !source ||
                (destination->kind != TYPE_ARRAY &&
                 destination->kind != TYPE_PTR) ||
                (source->kind != TYPE_ARRAY && source->kind != TYPE_PTR)) {
                rcc_error(expr->loc,
                          "va_copy requires two va_list objects");
            }
            expr->type = type_void;
            break;
        }

        case EXPR_VA_ARG: {
            Type* list_type = sema_expr(expr->va_list_operand);
            if (!list_type || (list_type->kind != TYPE_ARRAY &&
                               list_type->kind != TYPE_PTR)) {
                rcc_error(expr->loc, "va_arg requires a va_list object");
            }
            if (!expr->va_arg_type ||
                !(type_is_integer(expr->va_arg_type) ||
                  expr->va_arg_type->kind == TYPE_ENUM ||
                  expr->va_arg_type->kind == TYPE_PTR) ||
                expr->va_arg_type->size <= 0 ||
                expr->va_arg_type->size > 8) {
                rcc_error(expr->loc,
                          "va_arg currently supports integer and pointer scalars up to 64 bits");
                expr->va_arg_type = type_int;
            }
            expr->type = expr->va_arg_type;
            break;
        }

        case EXPR_COMPOUND:
            if (!expr->compound_type ||
                !type_is_complete(expr->compound_type) ||
                expr->compound_type->kind == TYPE_FUNC ||
                expr->compound_type->kind == TYPE_VOID) {
                rcc_error(expr->loc,
                          "compound literal requires a complete object type");
                expr->type = type_int;
            } else {
                expr->type = expr->compound_type;
                sema_initializer(expr->compound_type, expr);
            }
            break;

        case EXPR_MOD: {
            Type* lt = sema_expr(expr->binary_lhs);
            Type* rt = sema_expr(expr->binary_rhs);
            if (!sema_is_integer_type(lt) || !sema_is_integer_type(rt)) {
                rcc_error(expr->loc, "remainder operator requires integer operands");
            }
            expr->type = type_common(sema_integer_promotion(lt),
                                     sema_integer_promotion(rt));
            break;
        }

        case EXPR_BITAND:
        case EXPR_BITOR:
        case EXPR_BITXOR: {
            Type* lt = sema_expr(expr->binary_lhs);
            Type* rt = sema_expr(expr->binary_rhs);
            if (!sema_is_integer_type(lt) || !sema_is_integer_type(rt)) {
                rcc_error(expr->loc, "bitwise operator requires integer operands");
            }
            expr->type = type_common(sema_integer_promotion(lt),
                                     sema_integer_promotion(rt));
            break;
        }

        case EXPR_LSHIFT:
        case EXPR_RSHIFT: {
            Type* lt = sema_expr(expr->binary_lhs);
            Type* rt = sema_expr(expr->binary_rhs);
            if (!sema_is_integer_type(lt) || !sema_is_integer_type(rt)) {
                rcc_error(expr->loc, "shift operator requires integer operands");
            }
            /* C17 6.5.7 promotes each operand independently; unlike most
             * binary operators, the right operand never changes the result
             * type or the signedness of right shift. */
            expr->type = sema_integer_promotion(lt);
            break;
        }

        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_GT:
        case EXPR_LE:
        case EXPR_GE: {
            Type* left = sema_expr(expr->binary_lhs);
            Type* right = sema_expr(expr->binary_rhs);
            Type* left_value = generic_selection_type(left);
            Type* right_value = generic_selection_type(right);
            bool arithmetic = (type_is_arithmetic(left_value) ||
                               left_value->kind == TYPE_ENUM) &&
                              (type_is_arithmetic(right_value) ||
                               right_value->kind == TYPE_ENUM);
            bool pointers = type_is_pointer(left_value) &&
                            type_is_pointer(right_value);
            int64_t null_value = 1;
            bool pointer_null = (expr->kind == EXPR_EQ ||
                                 expr->kind == EXPR_NE) &&
                ((type_is_pointer(left_value) &&
                  sema_is_integer_type(right_value) &&
                  expr_eval_integer_constant(expr->binary_rhs, &null_value) &&
                  null_value == 0) ||
                 (type_is_pointer(right_value) &&
                  sema_is_integer_type(left_value) &&
                  expr_eval_integer_constant(expr->binary_lhs, &null_value) &&
                  null_value == 0));
            if (!arithmetic && !pointers && !pointer_null) {
                rcc_error(expr->loc,
                          "comparison requires arithmetic or pointer operands");
            }
            expr->type = type_int;
            break;
        }

        case EXPR_AND:
        case EXPR_OR: {
            Type* left = sema_expr(expr->binary_lhs);
            Type* right = sema_expr(expr->binary_rhs);
            Type* left_value = generic_selection_type(left);
            Type* right_value = generic_selection_type(right);
            if ((!type_is_scalar(left_value) &&
                 left_value->kind != TYPE_ENUM) ||
                (!type_is_scalar(right_value) &&
                 right_value->kind != TYPE_ENUM)) {
                rcc_error(expr->loc,
                          "logical operator requires scalar operands");
            }
            expr->type = type_int;
            break;
        }

        case EXPR_ADD_ASSIGN:
        case EXPR_SUB_ASSIGN: {
            Type* lt = sema_expr(expr->binary_lhs);
            Type* rt = sema_expr(expr->binary_rhs);
            if (!is_lvalue(expr->binary_lhs)) {
                rcc_error(expr->loc, "assignment requires lvalue");
            }
            if (!((type_is_pointer(lt) && is_pointer_arithmetic_type(lt) &&
                   type_is_integer(rt)) ||
                  (type_is_arithmetic(lt) && type_is_arithmetic(rt)))) {
                rcc_error(expr->loc,
                          "invalid operands to compound pointer arithmetic");
            }
            expr->type = lt;
            break;
        }

        case EXPR_MUL_ASSIGN:
        case EXPR_DIV_ASSIGN: {
            Type* lt = sema_expr(expr->binary_lhs);
            Type* rt = sema_expr(expr->binary_rhs);
            if (!is_lvalue(expr->binary_lhs)) {
                rcc_error(expr->loc, "assignment requires lvalue");
            }
            if (!type_is_arithmetic(lt) || !type_is_arithmetic(rt)) {
                rcc_error(expr->loc,
                          "multiplicative compound assignment requires arithmetic operands");
            }
            expr->type = lt;
            break;
        }

        case EXPR_MOD_ASSIGN:
        case EXPR_AND_ASSIGN:
        case EXPR_OR_ASSIGN:
        case EXPR_XOR_ASSIGN:
        case EXPR_LSHIFT_ASSIGN:
        case EXPR_RSHIFT_ASSIGN: {
            Type* lt = sema_expr(expr->binary_lhs);
            Type* rt = sema_expr(expr->binary_rhs);
            if (!is_lvalue(expr->binary_lhs)) {
                rcc_error(expr->loc, "assignment requires lvalue");
            }
            if (!type_is_integer(lt) || !type_is_integer(rt)) {
                rcc_error(expr->loc,
                          "integer compound assignment requires integer operands");
            }
            expr->type = lt;
            break;
        }

        case EXPR_ASSIGN: {
            Type* lt = sema_expr(expr->binary_lhs);
            sema_expr(expr->binary_rhs);
            if (!is_lvalue(expr->binary_lhs)) {
                rcc_error(expr->loc, "assignment requires lvalue");
            }
            expr->type = lt;
            break;
        }

        case EXPR_COND: {
            sema_expr(expr->cond_test);
            Type* tt = sema_expr(expr->cond_then);
            Type* et = sema_expr(expr->cond_else);
            expr->type = type_common(tt, et);
            break;
        }

        case EXPR_COMMA: {
            sema_expr(expr->binary_lhs);
            expr->type = sema_expr(expr->binary_rhs);
            break;
        }

        case EXPR_CALL: {
            Type* ft;
            TypeParam* parameter;
            ExprList* argument;
            Decl* selected_overload = NULL;
            int argument_index = 1;
            bool reported_too_many = false;
            bool arguments_analyzed = false;
            if (expr->call_func &&
                (expr->call_func->kind == EXPR_MEMBER ||
                 expr->call_func->kind == EXPR_PTR_MEMBER)) {
                Expr* member = expr->call_func;
                Type* owner = sema_expr(member->member_base);
                TypeMethod* method;
                if (member->kind == EXPR_PTR_MEMBER) {
                    owner = get_pointer_base(owner);
                }
                method = sema_find_inline_method(owner,
                                                 member->member_name);
                if (method) {
                    for (argument = expr->call_args; argument;
                         argument = argument->next) {
                        sema_expr(argument->expr);
                    }
                    if (expr->call_args) {
                        rcc_error(expr->loc,
                                  "inline accessor '%s' accepts no arguments",
                                  member->member_name);
                    }
                    if (method->cxx_access != 0u) {
                        rcc_error(expr->loc, "method '%s' is not accessible",
                                  member->member_name);
                    }
                    expr->call_method = method;
                    expr->type = method->return_type &&
                        method->return_type->is_reference
                        ? method->return_type->base
                        : method->return_type;
                    break;
                }
            }
            if (sema_atomic_builtin_call(expr)) break;
            if (expr->call_func->kind == EXPR_IDENT) {
                Symbol* overload = symtab_lookup(
                    g_symtab, expr->call_func->ident_name);
                if (overload && overload->kind == SYM_FUNC &&
                    overload->decl && overload->decl->func_has_cxx_linkage &&
                    overload->decl->func_overload_next) {
                    expr->call_func->ident_decl = overload->decl;
                    for (argument = expr->call_args; argument;
                         argument = argument->next) {
                        sema_expr(argument->expr);
                    }
                    arguments_analyzed = true;
                    selected_overload = sema_select_cxx_overload(expr);
                    if (!selected_overload) {
                        expr->type = type_int;
                        break;
                    }
                    expr->call_func->ident_decl = selected_overload;
                    expr->call_func->type = selected_overload->type;
                }
            }
            ft = selected_overload
                ? selected_overload->type : sema_expr(expr->call_func);
            if (!ft || ft->kind != TYPE_FUNC) {
                /* Could be pointer to function */
                if (ft && ft->kind == TYPE_PTR && ft->base && ft->base->kind == TYPE_FUNC) {
                    ft = ft->base;
                } else {
                    rcc_error(expr->loc, "called object is not a function");
                    expr->type = type_int;
                    break;
                }
            }

            parameter = ft->params;
            argument = expr->call_args;
            while (argument) {
                if (!arguments_analyzed) sema_expr(argument->expr);
                if (parameter) {
                    if (!implicit_cast(argument->expr, parameter->type)) {
                        const char* function_name =
                            expr->call_func->kind == EXPR_IDENT
                                ? expr->call_func->ident_name : "<function>";
                        rcc_error(argument->expr->loc,
                                  "incompatible type for argument %d to '%s'",
                                  argument_index, function_name);
                    }
                    parameter = parameter->next;
                } else if (ft->has_prototype && !ft->variadic &&
                           !reported_too_many) {
                    rcc_error(argument->expr->loc,
                              "too many arguments to function call");
                    reported_too_many = true;
                }
                argument = argument->next;
                ++argument_index;
            }
            if (parameter) {
                rcc_error(expr->loc, "too few arguments to function call");
            }

            expr->type = ft->ret_type;
            break;
        }

        case EXPR_INDEX: {
            Type* bt = sema_expr(expr->index_base);
            Type* it = sema_expr(expr->index_expr);

            Type* base = get_pointer_base(bt);
            if (!base) {
                rcc_error(expr->loc, "subscript requires array or pointer");
                expr->type = type_int;
            } else {
                if (!type_is_integer(it)) {
                    rcc_error(expr->loc, "array subscript must be integer");
                }
                expr->type = base;
            }
            break;
        }

        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER: {
            Type* bt = sema_expr(expr->member_base);

            if (expr->kind == EXPR_PTR_MEMBER) {
                bt = get_pointer_base(bt);
                if (!bt) {
                    rcc_error(expr->loc, "-> requires pointer to struct/union");
                    expr->type = type_int;
                    break;
                }
            }

            if (!bt || (bt->kind != TYPE_STRUCT && bt->kind != TYPE_UNION)) {
                rcc_error(expr->loc, "member access requires struct/union");
                expr->type = type_int;
                break;
            }

            /* Find member */
            TypeField* field = bt->fields;
            while (field) {
                if (strcmp(field->name, expr->member_name) == 0) {
                    expr->member_field = field;
                    expr->type = field->type;
                    if (field->cxx_access != 0u) {
                        rcc_error(expr->loc, "member '%s' is not accessible",
                                  expr->member_name);
                    }
                    break;
                }
                field = field->next;
            }

            if (!field) {
                rcc_error(expr->loc, "no member named '%s'", expr->member_name);
                expr->type = type_int;
            }
            break;
        }

        default:
            expr->type = type_int;
            break;
    }

    return expr->type;
}

/* ═══════════════════════════════════════
 * Statement Semantic Analysis
 * ═══════════════════════════════════════ */

static void sema_stmt(Stmt* stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case STMT_EXPR:
            if (stmt->expr) {
                sema_expr(stmt->expr);
            }
            break;

        case STMT_BLOCK:
            symtab_enter_scope(g_symtab);
            for (StmtList* s = stmt->block_stmts; s; s = s->next) {
                sema_stmt(s->stmt);
            }
            symtab_leave_scope(g_symtab);
            break;

        case STMT_IF:
            sema_expr(stmt->if_cond);
            sema_stmt(stmt->if_then);
            if (stmt->if_else) {
                sema_stmt(stmt->if_else);
            }
            break;

        case STMT_WHILE:
            sema_expr(stmt->while_cond);
            ++loop_depth;
            sema_stmt(stmt->while_body);
            --loop_depth;
            break;

        case STMT_DO:
            ++loop_depth;
            sema_stmt(stmt->while_body);
            --loop_depth;
            sema_expr(stmt->while_cond);
            break;

        case STMT_FOR:
            symtab_enter_scope(g_symtab);
            if (stmt->for_init) {
                sema_stmt(stmt->for_init);
            }
            if (stmt->for_cond) {
                sema_expr(stmt->for_cond);
            }
            if (stmt->for_inc) {
                sema_expr(stmt->for_inc);
            }
            ++loop_depth;
            sema_stmt(stmt->for_body);
            --loop_depth;
            symtab_leave_scope(g_symtab);
            break;

        case STMT_SWITCH:
        {
            Type* control = sema_expr(stmt->switch_expr);
            SemaSwitchContext context = {0};
            if (!control ||
                (!type_is_integer(control) && control->kind != TYPE_ENUM)) {
                rcc_error(stmt->switch_expr->loc,
                          "switch controlling expression must have integer type");
            }
            context.control_type = sema_switch_control_type(control);
            context.previous = current_switch;
            current_switch = &context;
            sema_stmt(stmt->switch_body);
            current_switch = context.previous;
            sema_switch_release_values(context.values);
            break;
        }

        case STMT_CASE:
        {
            Type* case_type = sema_expr(stmt->case_val);
            int64_t evaluated = 0;
            if (!current_switch) {
                rcc_error(stmt->loc, "case label is not within a switch");
            } else if (!case_type ||
                       (!type_is_integer(case_type) &&
                        case_type->kind != TYPE_ENUM) ||
                       !expr_eval_integer_constant(stmt->case_val,
                                                   &evaluated)) {
                rcc_error(stmt->case_val->loc,
                          "case label must be an integer constant expression");
            } else {
                uint64_t bits = sema_switch_value_bits(
                    evaluated, current_switch->control_type);
                SemaSwitchValue* existing = current_switch->values;
                while (existing && existing->bits != bits) {
                    existing = existing->next;
                }
                if (existing) {
                    rcc_error(stmt->case_val->loc,
                              "duplicate case value after conversion to switch type");
                } else {
                    SemaSwitchValue* value = rcc_alloc(sizeof(*value));
                    value->bits = bits;
                    value->next = current_switch->values;
                    current_switch->values = value;
                }
            }
            sema_stmt(stmt->case_stmt);
            break;
        }

        case STMT_DEFAULT:
            if (!current_switch) {
                rcc_error(stmt->loc, "default label is not within a switch");
            } else if (current_switch->has_default) {
                rcc_error(stmt->loc, "multiple default labels in one switch");
            } else {
                current_switch->has_default = true;
            }
            sema_stmt(stmt->default_stmt);
            break;

        case STMT_RETURN:
            if (stmt->return_val) {
                if (stmt->return_val->kind == EXPR_COMPOUND &&
                    !stmt->return_val->compound_type && current_func_ret &&
                    current_func_ret != type_void) {
                    stmt->return_val->compound_type = current_func_ret;
                    rcc_parser_validate_cxx_constructor_initializer(
                        current_func_ret, stmt->return_val);
                }
                sema_expr(stmt->return_val);
                if (current_func_ret &&
                    current_func_ret->cleanup_function) {
                    rcc_error(stmt->loc,
                              "returning a C++ scope-cleanup type is not "
                              "supported yet");
                }
                if (current_func_ret && current_func_ret != type_void) {
                    if (!implicit_cast(stmt->return_val, current_func_ret)) {
                        rcc_warning(stmt->loc, "incompatible return type");
                    }
                }
            }
            break;

        case STMT_GOTO: {
            Symbol* label = symtab_lookup_label(g_symtab, stmt->goto_label);
            if (!label) {
                /* Forward reference - create placeholder */
                label = rcc_alloc(sizeof(Symbol));
                label->name = stmt->goto_label;
                label->kind = SYM_LABEL;
                label->is_defined = false;
                label->next = g_symtab->labels;
                g_symtab->labels = label;
            }
            break;
        }

        case STMT_LABEL:
            symtab_define_label(g_symtab, stmt->label_name, stmt->loc);
            sema_stmt(stmt->label_stmt);
            break;

        case STMT_DECL:
            sema_decl(stmt->decl);
            break;

        case STMT_BREAK:
            if (loop_depth == 0 && !current_switch) {
                rcc_error(stmt->loc,
                          "break statement is not within a loop or switch");
            }
            break;

        case STMT_CONTINUE:
            if (loop_depth == 0) {
                rcc_error(stmt->loc,
                          "continue statement is not within a loop");
            }
            break;

        case STMT_NULL:
            /* Nothing to check */
            break;

        case STMT_ASM:
            /* Analyze input/output expressions */
            for (AsmOperand* op = stmt->asm_outputs; op; op = op->next) {
                if (op->expr) {
                    sema_expr(op->expr);
                }
            }
            for (AsmOperand* op = stmt->asm_inputs; op; op = op->next) {
                if (op->expr) {
                    sema_expr(op->expr);
                }
            }
            break;
    }
}

/* ═══════════════════════════════════════
 * Declaration Semantic Analysis
 * ═══════════════════════════════════════ */

static Expr* initializer_character_string(Type* type, Expr* initializer) {
    if (!type || type->kind != TYPE_ARRAY || !type->base ||
        type->base->kind != TYPE_CHAR || !initializer) {
        return NULL;
    }
    if (initializer->kind == EXPR_STRING_LIT) return initializer;
    if (initializer->kind == EXPR_COMPOUND && initializer->compound_init &&
        !initializer->compound_init->next &&
        initializer->compound_init->designator_kind == INIT_DESIGNATOR_NONE &&
        initializer->compound_init->expr &&
        initializer->compound_init->expr->kind == EXPR_STRING_LIT) {
        return initializer->compound_init->expr;
    }
    return NULL;
}

static bool sema_atomic_builtin_call(Expr* expr) {
    Expr* function = expr->call_func;
    ExprList* argument;
    const char* name;
    int argument_count = 0;
    int expected_count;
    bool requires_pointer = true;
    bool requires_expected_pointer = false;
    bool returns_void = false;
    bool returns_bool = false;
    Type* pointer_type = NULL;
    int64_t success_order = 0;
    int64_t failure_order = 0;
    bool success_constant = false;
    bool failure_constant = false;

    if (!function || function->kind != EXPR_IDENT) return false;
    name = function->ident_name;
    if (strcmp(name, "__atomic_load_n") == 0) {
        expected_count = 2;
    } else if (strcmp(name, "__atomic_store_n") == 0) {
        expected_count = 3;
        returns_void = true;
    } else if (strcmp(name, "__atomic_exchange_n") == 0 ||
               strcmp(name, "__atomic_fetch_add") == 0 ||
               strcmp(name, "__atomic_fetch_sub") == 0 ||
               strcmp(name, "__atomic_fetch_and") == 0 ||
               strcmp(name, "__atomic_fetch_or") == 0 ||
               strcmp(name, "__atomic_fetch_xor") == 0 ||
               strcmp(name, "__atomic_fetch_nand") == 0 ||
               strcmp(name, "__atomic_add_fetch") == 0 ||
               strcmp(name, "__atomic_sub_fetch") == 0 ||
               strcmp(name, "__atomic_and_fetch") == 0 ||
               strcmp(name, "__atomic_or_fetch") == 0 ||
               strcmp(name, "__atomic_xor_fetch") == 0 ||
               strcmp(name, "__atomic_nand_fetch") == 0) {
        expected_count = 3;
    } else if (strcmp(name, "__atomic_compare_exchange_n") == 0) {
        expected_count = 6;
        requires_expected_pointer = true;
        returns_bool = true;
    } else if (strcmp(name, "__sync_bool_compare_and_swap") == 0) {
        expected_count = 3;
        returns_bool = true;
    } else if (strcmp(name, "__sync_val_compare_and_swap") == 0) {
        expected_count = 3;
    } else if (strcmp(name, "__sync_lock_test_and_set") == 0 ||
               strcmp(name, "__sync_fetch_and_add") == 0 ||
               strcmp(name, "__sync_fetch_and_sub") == 0 ||
               strcmp(name, "__sync_fetch_and_and") == 0 ||
               strcmp(name, "__sync_fetch_and_or") == 0 ||
               strcmp(name, "__sync_fetch_and_xor") == 0 ||
               strcmp(name, "__sync_fetch_and_nand") == 0 ||
               strcmp(name, "__sync_add_and_fetch") == 0 ||
               strcmp(name, "__sync_sub_and_fetch") == 0 ||
               strcmp(name, "__sync_and_and_fetch") == 0 ||
               strcmp(name, "__sync_or_and_fetch") == 0 ||
               strcmp(name, "__sync_xor_and_fetch") == 0 ||
               strcmp(name, "__sync_nand_and_fetch") == 0) {
        expected_count = 2;
    } else if (strcmp(name, "__sync_lock_release") == 0) {
        expected_count = 1;
        returns_void = true;
    } else if (strcmp(name, "__atomic_thread_fence") == 0) {
        expected_count = 1;
        requires_pointer = false;
        returns_void = true;
    } else if (strcmp(name, "__sync_synchronize") == 0) {
        expected_count = 0;
        requires_pointer = false;
        returns_void = true;
    } else {
        return false;
    }

    for (argument = expr->call_args; argument; argument = argument->next) {
        sema_expr(argument->expr);
        ++argument_count;
    }
    if (argument_count != expected_count) {
        rcc_error(expr->loc, "%s expects %d arguments, got %d",
                  name, expected_count, argument_count);
    }
    if (requires_pointer) {
        bool pointer_value;
        bool integer_value;
        pointer_type = expr->call_args ? expr->call_args->expr->type : NULL;
        pointer_value = pointer_type && pointer_type->kind == TYPE_PTR &&
            pointer_type->base && pointer_type->base->kind == TYPE_PTR &&
            atomic_allows_pointer_value(name);
        integer_value = pointer_type && pointer_type->kind == TYPE_PTR &&
            pointer_type->base && type_is_integer(pointer_type->base) &&
            (pointer_type->base->size == 1u ||
             pointer_type->base->size == 2u ||
             pointer_type->base->size == 4u ||
             pointer_type->base->size == 8u);
        if (!pointer_type || pointer_type->kind != TYPE_PTR ||
            (!integer_value && !pointer_value)) {
            rcc_error(expr->loc,
                      "%s requires a supported lock-free object pointer",
                      name);
        }
    }
    if (requires_expected_pointer) {
        argument = expr->call_args ? expr->call_args->next : NULL;
        if (!argument || !argument->expr->type ||
            argument->expr->type->kind != TYPE_PTR ||
            !argument->expr->type->base ||
            !pointer_type || !pointer_type->base ||
            argument->expr->type->base->size != pointer_type->base->size ||
            !type_is_compatible(argument->expr->type->base,
                                pointer_type->base)) {
            rcc_error(expr->loc,
                      "%s expected-value pointer must match the object type",
                      name);
        }
    }
    if (strcmp(name, "__atomic_load_n") == 0) {
        if (sema_atomic_order(expr, name, 1, &success_order,
                              &success_constant) && success_constant &&
            (success_order == 3 || success_order == 4)) {
            rcc_error(sema_call_argument(expr, 1)->loc,
                      "%s does not accept release or acq_rel order", name);
        }
    } else if (strcmp(name, "__atomic_store_n") == 0) {
        if (sema_atomic_order(expr, name, 2, &success_order,
                              &success_constant) && success_constant &&
            (success_order == 1 || success_order == 2 ||
             success_order == 4)) {
            rcc_error(sema_call_argument(expr, 2)->loc,
                      "%s accepts only relaxed, release, or seq_cst order",
                      name);
        }
    } else if (strcmp(name, "__atomic_compare_exchange_n") == 0) {
        Expr* weak = sema_call_argument(expr, 3);
        int64_t weak_value;
        bool weak_constant = weak &&
            expr_eval_integer_constant(weak, &weak_value);
        if (weak && (!weak->type ||
            (!type_is_integer(weak->type) && weak->type->kind != TYPE_ENUM))) {
            rcc_error(weak->loc, "%s weak flag must have integer type", name);
        } else if (weak_constant && weak_value != 0 && weak_value != 1) {
            rcc_error(weak->loc, "%s weak flag must be zero or one", name);
        }
        sema_atomic_order(expr, name, 4, &success_order,
                          &success_constant);
        sema_atomic_order(expr, name, 5, &failure_order,
                          &failure_constant);
        if (success_constant && failure_constant &&
            success_order >= 0 && success_order <= 5 &&
            failure_order >= 0 && failure_order <= 5 &&
            !atomic_failure_order_allowed(success_order, failure_order)) {
            rcc_error(sema_call_argument(expr, 5)->loc,
                      "%s failure order is invalid or stronger than success",
                      name);
        }
    } else if (strncmp(name, "__atomic_", 9) == 0) {
        int order_index = strcmp(name, "__atomic_thread_fence") == 0
            ? 0 : 2;
        sema_atomic_order(expr, name, order_index, &success_order,
                          &success_constant);
    }

    function->type = type_ptr(type_void);
    if (returns_void) {
        expr->type = type_void;
    } else if (returns_bool) {
        expr->type = type_int;
    } else {
        expr->type = pointer_type && pointer_type->base
            ? pointer_type->base : type_uint;
    }
    return true;
}

static void sema_infer_initializer_type(Type* type, Expr* initializer) {
    Expr* string;
    int64_t cursor = 0;
    int64_t maximum = -1;
    if (!type || !initializer) return;
    string = initializer_character_string(type, initializer);
    if (string) {
        size_t characters = strlen(string->str_val);
        size_t storage = characters + 1u;
        if (type->array_len < 0) {
            if (storage > INT_MAX || type->base->size <= 0 ||
                storage > (size_t)INT_MAX / (size_t)type->base->size) {
                rcc_error(initializer->loc,
                          "character array initializer is too large");
            } else {
                type->array_len = (int)storage;
                type->size = (int)storage * type->base->size;
            }
        } else if ((size_t)type->array_len < characters) {
            rcc_error(initializer->loc,
                      "initializer string is too long for character array");
        }
        return;
    }
    if (type->kind != TYPE_ARRAY || initializer->kind != EXPR_COMPOUND) {
        return;
    }
    for (ExprList* item = initializer->compound_init; item;
         item = item->next) {
        if (item->designator_kind == INIT_DESIGNATOR_INDEX) {
            cursor = item->designator_index;
        }
        if (item->designator_kind != INIT_DESIGNATOR_FIELD &&
            cursor > maximum) {
            maximum = cursor;
        }
        if (cursor < INT64_MAX) ++cursor;
    }
    if (type->array_len < 0) {
        int64_t length = maximum >= 0 && maximum < INT_MAX
            ? maximum + 1 : 0;
        if (length <= 0 || !type->base ||
            type->base->size <= 0 ||
            length > INT_MAX / type->base->size) {
            rcc_error(initializer->loc,
                      "array initializer cannot determine a valid bound");
        } else {
            type->array_len = (int)length;
            type->size = (int)length * type->base->size;
        }
    }
}

static TypeField* initializer_field(Type* type, const char* name) {
    if (!type || !name) return NULL;
    for (TypeField* field = type->fields; field; field = field->next) {
        if (strcmp(field->name, name) == 0) return field;
    }
    return NULL;
}

static bool initializer_is_aggregate_zero(Type* type, Expr* initializer) {
    ExprList* item;
    int64_t value;
    if (!type || !initializer || initializer->kind != EXPR_COMPOUND ||
        (type->kind != TYPE_ARRAY && type->kind != TYPE_STRUCT &&
         type->kind != TYPE_UNION)) {
        return false;
    }
    item = initializer->compound_init;
    return item && !item->next &&
           item->designator_kind == INIT_DESIGNATOR_NONE && item->expr &&
           expr_eval_integer_constant(item->expr, &value) && value == 0;
}

static void sema_initializer(Type* type, Expr* initializer) {
    Expr* string;
    if (!type || !initializer) return;
    string = initializer_character_string(type, initializer);
    if (string) {
        sema_expr(string);
        initializer->type = type;
        return;
    }
    if (initializer->kind != EXPR_COMPOUND) {
        sema_expr(initializer);
        if (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) {
            if (!type_is_compatible(type, initializer->type)) {
                rcc_error(initializer->loc,
                          "incompatible aggregate copy initialization");
            }
            return;
        }
        if (type->kind == TYPE_ARRAY) {
            rcc_error(initializer->loc,
                      "array copy initialization is not valid C17");
            return;
        }
        if (!implicit_cast(initializer, type)) {
            rcc_warning(initializer->loc,
                        "incompatible types in initialization");
        }
        return;
    }
    initializer->type = type;
    if (initializer_is_aggregate_zero(type, initializer)) {
        sema_expr(initializer->compound_init->expr);
        if (!type_is_integer(initializer->compound_init->expr->type)) {
            rcc_error(initializer->compound_init->expr->loc,
                      "aggregate zero initializer requires an integer zero");
        }
        return;
    }
    if (type->kind == TYPE_ARRAY) {
        int64_t cursor = 0;
        for (ExprList* item = initializer->compound_init; item;
             item = item->next) {
            if (item->designator_kind == INIT_DESIGNATOR_FIELD) {
                rcc_error(item->expr->loc,
                          "field designator cannot initialize an array");
                continue;
            }
            if (item->designator_kind == INIT_DESIGNATOR_INDEX) {
                cursor = item->designator_index;
            }
            if (cursor < 0 || cursor >= type->array_len) {
                rcc_error(item->expr->loc,
                          "array initializer index is out of bounds");
            } else {
                sema_initializer(type->base, item->expr);
            }
            if (cursor < INT64_MAX) ++cursor;
        }
        return;
    }
    if (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) {
        TypeField* cursor = type->fields;
        int initialized = 0;
        if (!type->is_complete) {
            rcc_error(initializer->loc,
                      "initializer requires a complete aggregate type");
            return;
        }
        for (ExprList* item = initializer->compound_init; item;
             item = item->next) {
            TypeField* field = cursor;
            if (item->designator_kind == INIT_DESIGNATOR_INDEX) {
                rcc_error(item->expr->loc,
                          "array designator cannot initialize a struct or union");
                continue;
            }
            if (item->designator_kind == INIT_DESIGNATOR_FIELD) {
                field = initializer_field(type, item->designator_field);
                if (!field) {
                    rcc_error(item->expr->loc,
                              "no member named '%s' in initializer",
                              item->designator_field ?
                                  item->designator_field : "");
                    continue;
                }
            }
            if (!field || (type->kind == TYPE_UNION && initialized != 0 &&
                           item->designator_kind == INIT_DESIGNATOR_NONE)) {
                rcc_error(item->expr->loc,
                          "too many initializers for aggregate");
                continue;
            }
            sema_initializer(field->type, item->expr);
            cursor = field->next;
            ++initialized;
        }
        return;
    }
    if (!initializer->compound_init || initializer->compound_init->next ||
        initializer->compound_init->designator_kind != INIT_DESIGNATOR_NONE) {
        rcc_error(initializer->loc,
                  "scalar initializer list requires exactly one value");
        return;
    }
    sema_initializer(type, initializer->compound_init->expr);
}

static Type* sema_deduce_auto_type(Decl* declaration) {
    Type* deduced;
    if (!declaration) return type_int;
    if (!declaration->var_init) {
        rcc_error(declaration->loc, "auto variable requires an initializer");
        return type_int;
    }
    deduced = declaration->var_init->type;
    if (!deduced) deduced = sema_expr(declaration->var_init);
    if (deduced && deduced->is_reference && deduced->kind == TYPE_PTR) {
        deduced = deduced->base;
    } else if (deduced && deduced->kind == TYPE_ARRAY) {
        deduced = type_ptr(deduced->base);
    } else if (deduced && deduced->kind == TYPE_FUNC) {
        deduced = type_ptr(deduced);
    }
    if (!deduced || deduced->kind == TYPE_VOID ||
        !type_is_complete(deduced)) {
        rcc_error(declaration->loc,
                  "auto initializer does not have a complete object type");
        return type_int;
    }
    if (deduced->is_const || deduced->is_volatile) {
        Type* unqualified = ast_arena_alloc(sizeof(*unqualified));
        *unqualified = *deduced;
        unqualified->is_const = false;
        unqualified->is_volatile = false;
        deduced = unqualified;
    }
    return deduced;
}

static Expr* sema_cleanup_member_expression(Decl* declaration,
                                            TypeField* field) {
    Expr* object = expr_ident(declaration->name, declaration->loc);
    Expr* member = expr_member(object, field->name, declaration->loc);
    object->ident_decl = declaration;
    object->type = declaration->type;
    member->member_field = field;
    member->type = field->type;
    return member;
}

static void sema_prepare_variable_cleanup(Decl* declaration,
                                          bool is_global) {
    Symbol* symbol;
    Decl* function;
    TypeParam* parameter;
    TypeField* field;
    Expr* condition;
    Expr* function_expression;
    Expr* argument;
    Expr* call;
    Expr* cleanup;
    ExprList* arguments = NULL;
    if (!declaration || !declaration->type ||
        !declaration->type->cleanup_function ||
        !declaration->type->cleanup_field) {
        return;
    }
    if (is_global) {
        rcc_error(declaration->loc,
                  "C++ cleanup for static storage is not supported yet");
        return;
    }
    if (!declaration->var_init ||
        declaration->var_init->kind != EXPR_COMPOUND ||
        declaration->var_init->compound_type != declaration->type) {
        rcc_error(declaration->loc,
                  "C++ scope-cleanup object requires a validated direct "
                  "constructor");
        return;
    }
    field = declaration->type->cleanup_field;
    symbol = symtab_lookup(g_symtab, declaration->type->cleanup_function);
    function = symbol && symbol->kind == SYM_FUNC ? symbol->decl : NULL;
    if (!function || !function->type || function->type->kind != TYPE_FUNC) {
        rcc_error(declaration->loc,
                  "C++ cleanup function '%s' is not declared",
                  declaration->type->cleanup_function);
        return;
    }
    parameter = function->type->params;
    if (!parameter || parameter->next ||
        !type_is_compatible(parameter->type, field->type)) {
        rcc_error(declaration->loc,
                  "C++ cleanup function '%s' has an incompatible signature",
                  declaration->type->cleanup_function);
        return;
    }

    condition = expr_binary(
        EXPR_NE,
        sema_cleanup_member_expression(declaration, field),
        expr_int(declaration->type->cleanup_invalid, declaration->loc),
        declaration->loc);
    condition->type = type_int;
    function_expression = expr_ident(function->name, declaration->loc);
    function_expression->ident_decl = function;
    function_expression->type = function->type;
    argument = sema_cleanup_member_expression(declaration, field);
    exprlist_append(&arguments, argument);
    call = expr_call(function_expression, arguments, declaration->loc);
    call->type = function->type->ret_type;
    cleanup = expr_cond(condition, call,
                        expr_int(0, declaration->loc), declaration->loc);
    cleanup->type = call->type && call->type->kind != TYPE_VOID
        ? call->type : type_int;
    declaration->var_cleanup = cleanup;
}

static bool sema_statement_has_cleanup(Stmt* statement) {
    if (!statement) return false;
    switch (statement->kind) {
        case STMT_BLOCK:
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                if (sema_statement_has_cleanup(item->stmt)) return true;
            }
            return false;
        case STMT_IF:
            return sema_statement_has_cleanup(statement->if_then) ||
                   sema_statement_has_cleanup(statement->if_else);
        case STMT_WHILE:
        case STMT_DO:
            return sema_statement_has_cleanup(statement->while_body);
        case STMT_FOR:
            return sema_statement_has_cleanup(statement->for_init) ||
                   sema_statement_has_cleanup(statement->for_body);
        case STMT_SWITCH:
            return sema_statement_has_cleanup(statement->switch_body);
        case STMT_CASE:
            return sema_statement_has_cleanup(statement->case_stmt);
        case STMT_DEFAULT:
            return sema_statement_has_cleanup(statement->default_stmt);
        case STMT_LABEL:
            return sema_statement_has_cleanup(statement->label_stmt);
        case STMT_DECL:
            return statement->decl && statement->decl->var_cleanup;
        default:
            return false;
    }
}

static bool sema_statement_has_unsupported_cleanup_flow(Stmt* statement) {
    if (!statement) return false;
    switch (statement->kind) {
        case STMT_GOTO:
        case STMT_SWITCH:
            return true;
        case STMT_BLOCK:
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                if (sema_statement_has_unsupported_cleanup_flow(item->stmt)) {
                    return true;
                }
            }
            return false;
        case STMT_IF:
            return sema_statement_has_unsupported_cleanup_flow(
                       statement->if_then) ||
                   sema_statement_has_unsupported_cleanup_flow(
                       statement->if_else);
        case STMT_WHILE:
        case STMT_DO:
            return sema_statement_has_unsupported_cleanup_flow(
                statement->while_body);
        case STMT_FOR:
            return sema_statement_has_unsupported_cleanup_flow(
                       statement->for_init) ||
                   sema_statement_has_unsupported_cleanup_flow(
                       statement->for_body);
        case STMT_CASE:
            return sema_statement_has_unsupported_cleanup_flow(
                statement->case_stmt);
        case STMT_DEFAULT:
            return sema_statement_has_unsupported_cleanup_flow(
                statement->default_stmt);
        case STMT_LABEL:
            return sema_statement_has_unsupported_cleanup_flow(
                statement->label_stmt);
        default:
            return false;
    }
}

static void sema_decl(Decl* decl) {
    if (!decl) return;

    switch (decl->kind) {
        case DECL_VAR: {
            bool is_global = g_symtab->current == g_symtab->global;
            if (decl->var_is_auto) {
                decl->type = sema_deduce_auto_type(decl);
            }
            if (decl->var_is_thread_local && !is_global) {
                rcc_error(decl->loc,
                          "block-scope thread-local variables are not supported yet");
            }
            if (decl->var_is_thread_local &&
                (decl->storage == STORAGE_AUTO ||
                 decl->storage == STORAGE_REGISTER)) {
                rcc_error(decl->loc,
                          "thread-local variable cannot use auto or register storage");
            }
            sema_infer_initializer_type(decl->type, decl->var_init);
            if (decl->type && decl->type->kind == TYPE_ARRAY &&
                       decl->type->array_len < 0 &&
                       !(decl->storage == STORAGE_EXTERN && !decl->var_init)) {
                rcc_error(decl->loc,
                          "incomplete array requires an initializer with known size");
            }
            Symbol* sym = is_global
                ? symtab_lookup_local(g_symtab, decl->name) : NULL;
            if (sym) {
                if (sym->kind != SYM_VAR ||
                    !type_is_compatible(sym->type, decl->type)) {
                    rcc_error(decl->loc,
                              "conflicting declaration of variable '%s'",
                              decl->name);
                } else if (sym->decl &&
                           sym->decl->var_is_thread_local !=
                               decl->var_is_thread_local) {
                    rcc_error(decl->loc,
                              "thread-local qualifier differs for variable '%s'",
                              decl->name);
                } else if (decl->var_init && sym->is_defined) {
                    rcc_error(decl->loc, "redefinition of variable '%s'",
                              decl->name);
                }
            } else {
                sym = symtab_define(g_symtab, decl->name, SYM_VAR,
                                    decl->type, decl->loc);
            }
            sym->decl = decl;
            if (!is_global || decl->var_init) sym->is_defined = true;
            decl->var_offset = sym->offset;
            decl->var_is_global = sym->is_global;

            if (decl->var_init) {
                sema_initializer(decl->type, decl->var_init);
            }
            sema_prepare_variable_cleanup(decl, is_global);
            break;
        }

        case DECL_FUNC: {
            Symbol* sym = symtab_lookup(g_symtab, decl->name);
            bool cxx_overload_set = false;
            if (sym && sym->kind == SYM_FUNC &&
                decl->func_has_cxx_linkage) {
                Decl** slot = &sym->decl;
                while (*slot) {
                    Decl* prior = *slot;
                    if (cxx_same_function_parameters(prior->type,
                                                     decl->type)) {
                        if (!type_is_compatible(prior->type->ret_type,
                                                decl->type->ret_type)) {
                            rcc_error(decl->loc,
                                      "overload '%s' differs only by return type",
                                      decl->name);
                        }
                        if (prior->func_body && decl->func_body) {
                            rcc_error(decl->loc,
                                      "redefinition of function '%s'",
                                      decl->name);
                        }
                        decl->func_overload_next =
                            prior->func_overload_next;
                        *slot = decl;
                        cxx_overload_set = true;
                        break;
                    }
                    slot = &prior->func_overload_next;
                }
                if (!cxx_overload_set) {
                    decl->func_overload_next = sym->decl;
                    sym->decl = decl;
                    cxx_overload_set = true;
                }
                sym->type = sym->decl->type;
            } else if (sym && sym->kind == SYM_FUNC) {
                /* Check for redefinition */
                if (sym->is_defined && decl->func_body) {
                    rcc_error(decl->loc, "redefinition of function '%s'", decl->name);
                }
            } else {
                sym = symtab_define(g_symtab, decl->name, SYM_FUNC, decl->type, decl->loc);
            }
            if (!cxx_overload_set) sym->decl = decl;

            if (decl->func_body) {
                sym->is_defined = true;

                /* Enter function scope */
                symtab_enter_function(g_symtab);
                current_func_ret = decl->type->ret_type;
                current_func_variadic = decl->type->variadic;
                current_func_last_param = NULL;

                /* Add parameters */
                int param_offset = 8;  /* After saved EBP and return address */
                if (g_opts.target_arch == ARCH_X86 &&
                    decl->type && decl->type->ret_type &&
                    (decl->type->ret_type->kind == TYPE_STRUCT ||
                     decl->type->ret_type->kind == TYPE_UNION)) {
                    param_offset += 4; /* Hidden aggregate-result pointer. */
                }
                for (DeclList* p = decl->func_params; p; p = p->next) {
                    current_func_last_param = p->decl;
                    Symbol* psym = symtab_define(g_symtab, p->decl->name, SYM_PARAM,
                                                  p->decl->type, p->decl->loc);
                    psym->decl = p->decl;
                    psym->offset = param_offset;
                    p->decl->var_offset = param_offset;
                    {
                        int parameter_size = p->decl->type &&
                            p->decl->type->size > 4
                            ? p->decl->type->size : 4;
                        param_offset += (parameter_size + 3) & ~3;
                    }
                }

                /* Analyze body */
                loop_depth = 0;
                current_switch = NULL;
                sema_stmt(decl->func_body);
                if (sema_statement_has_cleanup(decl->func_body) &&
                    sema_statement_has_unsupported_cleanup_flow(
                        decl->func_body)) {
                    rcc_error(decl->loc,
                              "goto and switch are not "
                              "supported with C++ scope cleanup yet");
                }

                /* Check for undefined labels */
                for (Symbol* label = g_symtab->labels; label; label = label->next) {
                    if (!label->is_defined) {
                        rcc_error(decl->loc, "undefined label '%s'", label->name);
                    }
                }

                symtab_leave_function(g_symtab);
                current_func_ret = NULL;
                current_func_variadic = false;
                current_func_last_param = NULL;
            }
            break;
        }

        case DECL_PARAM:
            /* Handled in DECL_FUNC */
            break;

        case DECL_TYPEDEF: {
            symtab_define(g_symtab, decl->name, SYM_TYPE, decl->typedef_type, decl->loc);
            break;
        }

        case DECL_STRUCT:
        case DECL_UNION: {
            SymKind k = (decl->kind == DECL_STRUCT) ? SYM_STRUCT : SYM_UNION;
            symtab_define(g_symtab, decl->name, k, decl->type, decl->loc);
            break;
        }

        case DECL_ENUM: {
            symtab_define(g_symtab, decl->name, SYM_ENUM, decl->type, decl->loc);
            /* Define enum constants */
            for (DeclList* c = decl->enum_consts; c; c = c->next) {
                Symbol* sym = symtab_define(g_symtab, c->decl->name, SYM_ENUM_CONST,
                                             type_int, c->decl->loc);
                sym->enum_val = c->decl->enum_val;
            }
            break;
        }

        case DECL_ENUM_CONST:
            /* Handled in DECL_ENUM */
            break;
    }
}

/* ═══════════════════════════════════════
 * Main Semantic Analysis
 * ═══════════════════════════════════════ */

bool rcc_sema(AST* ast) {
    bool valid;
    /* Create symbol table */
    g_symtab = symtab_new();

    /* Process all top-level declarations */
    for (DeclList* d = ast->decls; d; d = d->next) {
        sema_decl(d->decl);
    }

    valid = g_error_count == 0;
    symtab_free(g_symtab);
    g_symtab = NULL;
    return valid;
}
