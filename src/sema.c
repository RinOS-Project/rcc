/*
 * RCC - RinOS C Compiler
 * Semantic Analysis
 */

#include "rcc.h"
#include "ast.h"
#include "ast_cxx.h"
#include "symtab.h"
#include <limits.h>

/* The C compiler intentionally omits the C++ AST object.  Keep the namespace
 * lookup extension optional at this boundary so the C frontend remains a
 * standalone executable while rcc++ supplies the real implementation. */
#if defined(__GNUC__) || defined(__clang__)
extern CxxNamespace* g_global_namespace __attribute__((weak));
extern CxxNamespace* cxx_namespace_find(
    CxxNamespace*, const char*) __attribute__((weak));
extern CxxNamespace* cxx_namespace_for_decl_name(
    CxxNamespace*, const char*) __attribute__((weak));
extern const char* cxx_namespace_qualified_name(
    CxxNamespace*) __attribute__((weak));
#endif

/* Current function return type */
static Type* current_func_ret = NULL;
static bool current_func_variadic = false;
static Decl* current_func_last_param = NULL;
static Type* current_cxx_method_owner = NULL;
static Decl* current_cxx_this_param = NULL;
static CxxNamespace* current_cxx_namespace = NULL;

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
static void sema_vla_bounds(Type* type, SourceLoc loc);
static void sema_validate_array_parameter_type(Type* type, SourceLoc loc,
                                               bool is_parameter);

static bool sema_cxx_public_base(Type* derived, Type* target,
                                  int* adjustment, int depth) {
    CxxClass* cls;
    if (!derived || !target || depth > 32) return false;
    if (derived == target ||
        (derived->cxx_class && derived->cxx_class == target->cxx_class)) {
        if (adjustment) *adjustment = 0;
        return true;
    }
    cls = derived->cxx_class;
    if (!cls || !cls->base_offsets) return false;
    for (int index = 0; index < cls->base_count; ++index) {
        Type* base_type = cls->bases[index].base
            ? cls->bases[index].base->type : NULL;
        int nested_adjustment;
        if (cls->bases[index].is_virtual ||
            cls->bases[index].access != ACCESS_PUBLIC || !base_type ||
            cls->base_offsets[index] < 0) {
            continue;
        }
        if (sema_cxx_public_base(base_type, target,
                                 &nested_adjustment, depth + 1)) {
            if (adjustment) {
                *adjustment = cls->base_offsets[index] + nested_adjustment;
            }
            return true;
        }
    }
    return false;
}

static bool sema_cxx_pointer_conversion(Type* source, Type* target,
                                         int* adjustment) {
    if (!source || !target || source->kind != TYPE_PTR ||
        target->kind != TYPE_PTR || !source->base || !target->base) {
        return false;
    }
    return sema_cxx_public_base(source->base, target->base,
                                adjustment, 0);
}

/* C++ new/delete are language expressions, so they do not require a source
 * declaration for the RinOS allocation ABI.  Materialize the two C-linkage
 * declarations lazily in the semantic symbol table, while respecting a real
 * declaration if the translation unit or SDK headers provided one. */
static Symbol* sema_cxx_runtime_function(const char* name, SourceLoc loc) {
    Type* return_type;
    Type* parameter_type;
    TypeParam* type_parameter;
    Decl* parameter;
    DeclList* parameters;
    Type* function_type;
    Decl* declaration;
    Symbol* symbol;

    if (!rcc_parser_is_cxx_mode() ||
        !name || (strcmp(name, "rin_malloc") != 0 &&
                  strcmp(name, "rin_free") != 0)) {
        return NULL;
    }
    symbol = symtab_lookup(g_symtab, name);
    if (symbol) return symbol;

    return_type = strcmp(name, "rin_free") == 0
        ? type_void : type_ptr(type_void);
    parameter_type = strcmp(name, "rin_free") == 0
        ? type_ptr(type_void)
        : (g_opts.target_arch == ARCH_X64 ? type_ulong : type_uint);
    type_parameter = ast_arena_alloc(sizeof(*type_parameter));
    type_parameter->name = "value";
    type_parameter->type = parameter_type;
    type_parameter->cxx_access = 0u; /* ACCESS_PUBLIC without C++ header. */
    type_parameter->next = NULL;
    parameter = decl_param("value", parameter_type, 0, loc);
    parameters = ast_arena_alloc(sizeof(*parameters));
    parameters->decl = parameter;
    parameters->next = NULL;
    function_type = type_func(return_type, type_parameter, false);
    function_type->has_prototype = true;
    declaration = decl_func(name, function_type, parameters, NULL, loc);
    declaration->storage = STORAGE_EXTERN;
    declaration->func_has_cxx_linkage = false;
    declaration->link_name = rcc_intern(name);
    symbol = symtab_define(g_symtab, name, SYM_FUNC, function_type, loc);
    symbol->decl = declaration;
    return symbol;
}

static Symbol* sema_cxx_adl_lookup(const char* name, ExprList* arguments) {
    char qualified[512];
    if (!rcc_parser_is_cxx_mode() || !name) return NULL;
    for (ExprList* item = arguments; item; item = item->next) {
        Type* type = item->expr ? item->expr->type : NULL;
        while (type && type->kind == TYPE_PTR && !type->is_reference) {
            type = type->base;
        }
        if (!type || (type->kind != TYPE_STRUCT &&
                      type->kind != TYPE_UNION) ||
            !type->cxx_namespace ||
            strlen(type->cxx_namespace) + strlen(name) + 3u >=
                sizeof(qualified)) {
            continue;
        }
        strcpy(qualified, type->cxx_namespace);
        strcat(qualified, "::");
        strcat(qualified, name);
        Symbol* symbol = symtab_lookup(g_symtab, qualified);
        if (symbol && symbol->kind == SYM_FUNC && symbol->decl) {
            return symbol;
        }
    }
    return NULL;
}

static Symbol* sema_cxx_lookup_namespace(CxxNamespace* ns,
                                          const char* name,
                                          CxxNamespace** visited,
                                          int visited_count) {
    char qualified[512];
    Symbol* result = NULL;
    const char* namespace_name;

    if (!ns || !name || !*name || visited_count >= 32) return NULL;
    for (int index = 0; index < visited_count; ++index) {
        if (visited[index] == ns) return NULL;
    }
    visited[visited_count++] = ns;
    namespace_name = cxx_namespace_qualified_name(ns);
    if (namespace_name && *namespace_name) {
        if (strlen(namespace_name) + 2u + strlen(name) < sizeof(qualified)) {
            strcpy(qualified, namespace_name);
            strcat(qualified, "::");
            strcat(qualified, name);
            result = symtab_lookup(g_symtab, qualified);
        }
    } else {
        result = symtab_lookup(g_symtab, name);
    }
    if (result) return result;

    for (int index = 0; index < ns->using_declaration_count; ++index) {
        const char* target = ns->using_declarations[index];
        const char* final_component = strrchr(target, ':');
        final_component = final_component ? final_component + 1 : target;
        if (strcmp(final_component, name) != 0) continue;
        result = symtab_lookup(g_symtab, target);
        if (result) return result;
    }
    for (int index = 0; index < ns->using_namespace_count; ++index) {
        result = sema_cxx_lookup_namespace(ns->using_namespaces[index], name,
                                           visited, visited_count);
        if (result) return result;
    }
    return NULL;
}

static Symbol* sema_cxx_lookup_name(const char* name) {
    Symbol* symbol;
    CxxNamespace* visited[32] = { 0 };
    CxxNamespace* ns;

    if (!name) return NULL;
    symbol = symtab_lookup(g_symtab, name);
    if (symbol || !rcc_parser_is_cxx_mode() || strchr(name, ':')) {
        return symbol;
    }
    for (ns = current_cxx_namespace ? current_cxx_namespace
                                    : g_global_namespace;
         ns; ns = ns->parent) {
        symbol = sema_cxx_lookup_namespace(ns, name, visited, 0);
        if (symbol) return symbol;
    }
    return NULL;
}

static CxxNamespace* sema_decl_namespace(Decl* decl) {
    if (!g_global_namespace || !decl) return g_global_namespace;
    if (decl->func_method_owner && decl->func_method_owner->cxx_namespace) {
        CxxNamespace* owner_namespace = cxx_namespace_find(
            g_global_namespace, decl->func_method_owner->cxx_namespace);
        if (owner_namespace) return owner_namespace;
    }
    return cxx_namespace_for_decl_name(g_global_namespace, decl->name);
}

static bool sema_statement_has_current_switch_label(Stmt* statement) {
    if (!statement) return false;
    switch (statement->kind) {
        case STMT_SWITCH:
            /* Labels in a nested switch do not target the current switch. */
            return false;
        case STMT_CASE:
        case STMT_DEFAULT:
            return true;
        case STMT_BLOCK:
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                if (sema_statement_has_current_switch_label(item->stmt)) {
                    return true;
                }
            }
            return false;
        case STMT_IF:
            return sema_statement_has_current_switch_label(
                       statement->if_then) ||
                   sema_statement_has_current_switch_label(
                       statement->if_else);
        case STMT_WHILE:
        case STMT_DO:
            return sema_statement_has_current_switch_label(
                statement->while_body);
        case STMT_FOR:
            return sema_statement_has_current_switch_label(
                statement->for_body);
        case STMT_LABEL:
            return sema_statement_has_current_switch_label(
                statement->label_stmt);
        default:
            return false;
    }
}

static bool sema_switch_cleanup_scopes_safe(Stmt* statement,
                                            bool label_scope) {
    if (!statement) return true;
    switch (statement->kind) {
        case STMT_SWITCH:
            /* A nested switch is checked independently by sema_stmt(). */
            return true;
        case STMT_BLOCK: {
            bool block_label_scope =
                sema_statement_has_current_switch_label(statement);
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                if (!sema_switch_cleanup_scopes_safe(item->stmt,
                                                     block_label_scope)) {
                    return false;
                }
            }
            return true;
        }
        case STMT_DECL:
            return !label_scope || !statement->decl ||
                   !statement->decl->var_cleanup;
        case STMT_CASE:
            return sema_switch_cleanup_scopes_safe(statement->case_stmt,
                                                   label_scope);
        case STMT_DEFAULT:
            return sema_switch_cleanup_scopes_safe(statement->default_stmt,
                                                   label_scope);
        case STMT_IF:
            return sema_switch_cleanup_scopes_safe(statement->if_then,
                                                   label_scope) &&
                   sema_switch_cleanup_scopes_safe(statement->if_else,
                                                   label_scope);
        case STMT_WHILE:
        case STMT_DO:
            return sema_switch_cleanup_scopes_safe(statement->while_body,
                                                   label_scope);
        case STMT_FOR:
            return sema_switch_cleanup_scopes_safe(statement->for_init,
                                                   label_scope) &&
                   sema_switch_cleanup_scopes_safe(statement->for_body,
                                                   label_scope);
        case STMT_LABEL:
            return sema_switch_cleanup_scopes_safe(statement->label_stmt,
                                                   label_scope);
        default:
            return true;
    }
}

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

static bool is_modifiable_lvalue(Expr* expression) {
    Type* type;
    if (!expression || !is_lvalue(expression)) return false;
    type = expression->type;
    return type && !type->is_const && type->kind != TYPE_ARRAY &&
           type->kind != TYPE_FUNC;
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

static bool sema_is_cxx_nullptr_expr(const Expr* expression) {
    return expression && (expression->is_cxx_nullptr ||
        (expression->type && expression->type->kind == TYPE_NULLPTR));
}

static Type* sema_integer_promotion(Type* type) {
    if (!type || type->kind == TYPE_ENUM || type->kind < TYPE_INT) {
        return type_int;
    }
    return type;
}

static Type* implicit_cast(Expr* e, Type* target) {
    if (!e->type || !target) return NULL;

    /* nullptr has a zero machine representation, but it is not an integer.
     * Keep its standard null-pointer conversion separate from the legacy C
     * integer/pointer conversion paths below. */
    if (e->type->kind == TYPE_NULLPTR) {
        if (target->kind == TYPE_NULLPTR) return target;
        return !target->is_reference && type_is_pointer(target)
            ? target : NULL;
    }

    if (target->is_reference) {
        Type* referred = target->base;
        /* Reference arguments are passed as addresses by the backend, so the
         * supported subset deliberately requires addressable expressions. */
        if (!referred || !is_lvalue(e)) return NULL;
        if ((e->type->is_const && !referred->is_const) ||
            (e->type->is_volatile && !referred->is_volatile)) {
            return NULL;
        }
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
        if ((e->type->base->is_const && !target->base->is_const) ||
            (e->type->base->is_volatile && !target->base->is_volatile)) {
            return NULL;
        }
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
        if ((e->type->base->is_const && !target->base->is_const) ||
            (e->type->base->is_volatile && !target->base->is_volatile)) {
            return NULL;
        }
        if ((e->type->base && e->type->base->kind == TYPE_VOID) ||
            (target->base && target->base->kind == TYPE_VOID)) {
            return target;
        }
        if (type_is_compatible(e->type->base, target->base)) {
            return target;
        }
        {
            int adjustment;
            /* A zero-offset public base conversion has the same machine
             * representation and is safe through the existing pointer ABI.
             * Non-zero conversions require a dedicated expression lowering. */
            if (sema_cxx_pointer_conversion(e->type, target, &adjustment) &&
                adjustment == 0) {
                return target;
            }
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
        if (method->kind != TYPE_METHOD_FUNCTION && method->name &&
            strcmp(method->name, name) == 0) return method;
    }
    return NULL;
}

static TypeMethod* sema_find_function_method(Type* aggregate,
                                              const char* name) {
    TypeMethod* method;
    if (!aggregate || !name ||
        (aggregate->kind != TYPE_STRUCT && aggregate->kind != TYPE_UNION)) {
        return NULL;
    }
    for (method = aggregate->methods; method; method = method->next) {
        if (method->kind == TYPE_METHOD_FUNCTION && method->name &&
            method->function_decl && strcmp(method->name, name) == 0) {
            return method;
        }
    }
    return NULL;
}

static TypeMethod* sema_find_contextual_bool_method(Type* aggregate) {
    TypeMethod* method;
    if (!aggregate ||
        (aggregate->kind != TYPE_STRUCT && aggregate->kind != TYPE_UNION)) {
        return NULL;
    }
    for (method = aggregate->methods; method; method = method->next) {
        if (method->name &&
            strcmp(method->name, "operator conversion") == 0 &&
            method->return_type && method->return_type->kind == TYPE_BOOL &&
            method->field && method->cxx_access == 0u) {
            return method;
        }
    }
    return NULL;
}

/* C++ explicit operator bool participates in contextual conversions without
 * becoming a general implicit conversion.  Only the structurally validated
 * inline method subset is eligible, so arbitrary member bodies remain
 * fail-closed. */
static Expr* sema_contextual_bool(Expr* expression) {
    Type* type;
    Type* value_type;
    TypeMethod* method;
    Expr* member;
    Expr* call;
    if (!expression) return expression;
    type = sema_expr(expression);
    value_type = generic_selection_type(type);
    if (value_type &&
        (type_is_scalar(value_type) || value_type->kind == TYPE_ENUM)) {
        return expression;
    }
    method = sema_find_contextual_bool_method(value_type);
    if (!method) {
        rcc_error(expression->loc,
                  "condition requires scalar type or validated operator bool");
        return expression;
    }
    member = expr_member(expression, method->name, expression->loc);
    call = expr_call(member, NULL, expression->loc);
    call->call_method = method;
    call->type = method->return_type;
    return call;
}

static bool cxx_same_parameter_type(Type* source, Type* target,
                                    bool top_level) {
    if (!source || !target || source->kind != target->kind) return false;
    if (source->is_reference != target->is_reference ||
        source->is_rvalue_reference != target->is_rvalue_reference) {
        return false;
    }
    if (!top_level &&
        (source->is_const != target->is_const ||
         source->is_volatile != target->is_volatile)) {
        return false;
    }
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
    if (argument->type->kind == TYPE_NULLPTR) {
        if (target->kind == TYPE_NULLPTR) return 0;
        return !target->is_reference && type_is_pointer(target) ? 1 : -1;
    }
    if (target->is_reference) {
        target_base = target->base;
        if (!target_base || !is_lvalue(argument)) return -1;
        if ((source->is_const && !target_base->is_const) ||
            (source->is_volatile && !target_base->is_volatile)) {
            return -1;
        }
        if (cxx_same_parameter_type(source, target_base, false)) return 0;
        return type_is_compatible(source, target_base) ? 1 : -1;
    }
    if (cxx_same_parameter_type(source, target, true)) return 0;

    if (source->kind == TYPE_ARRAY && target->kind == TYPE_PTR) {
        source_base = source->base;
        target_base = target->base;
        if (source_base && target_base &&
            ((source_base->is_const && !target_base->is_const) ||
             (source_base->is_volatile && !target_base->is_volatile))) {
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
         * pointee cv-qualification. */
        if ((source_base->is_const && !target_base->is_const) ||
            (source_base->is_volatile && !target_base->is_volatile)) {
            return -1;
        }
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

static int sema_cxx_argument_count(ExprList* arguments) {
    int count = 0;
    for (; arguments; arguments = arguments->next) {
        if (count == INT_MAX) return INT_MAX;
        ++count;
    }
    return count;
}

static bool sema_cxx_new_storage_type(Type* type) {
    if (!type) return false;
    if (type->is_reference) return false;
    if (g_opts.target_arch == ARCH_X86 && type->size > 4) return false;
    return type_is_integer(type) || type->kind == TYPE_ENUM ||
           type->kind == TYPE_PTR || type->kind == TYPE_NULLPTR;
}

static bool sema_cxx_trivially_destructible(Type* type, int depth) {
    CxxClass* cls;
    if (!type || depth > 32) return false;
    if (type->kind != TYPE_STRUCT && type->kind != TYPE_UNION) return true;
    cls = type->cxx_class;
    if (cls) {
        for (struct CxxMember* member = cls->members;
             member; member = member->next) {
            if (member->method && member->method->is_destructor) {
                return false;
            }
        }
    }
    for (TypeField* field = type->fields; field; field = field->next) {
        if (!sema_cxx_trivially_destructible(field->type, depth + 1)) {
            return false;
        }
    }
    return true;
}

static Decl* sema_cxx_cleanup_function(Type* object_type, SourceLoc loc) {
    Symbol* symbol;
    Decl* function;
    TypeParam* parameter;
    Type* return_type;

    if (!object_type || !object_type->cleanup_function ||
        !object_type->cleanup_field) {
        return NULL;
    }
    symbol = symtab_lookup(g_symtab, object_type->cleanup_function);
    function = symbol && symbol->kind == SYM_FUNC ? symbol->decl : NULL;
    if (!function || !function->type || function->type->kind != TYPE_FUNC) {
        rcc_error(loc,
                  "C++ cleanup function '%s' is not declared",
                  object_type->cleanup_function);
        return NULL;
    }
    parameter = function->type->params;
    if (!parameter || parameter->next ||
        !type_is_compatible(parameter->type, object_type->cleanup_field->type)) {
        rcc_error(loc,
                  "C++ cleanup function '%s' has an incompatible signature",
                  object_type->cleanup_function);
        return NULL;
    }
    /* The delete lowering intentionally discards the cleanup result.  A
     * scalar/void result has no hidden sret storage, so the direct call below
     * remains ABI-complete on both supported targets. */
    return_type = function->type->ret_type;
    if (!return_type ||
        (return_type->kind != TYPE_VOID &&
         !type_is_integer(return_type) &&
         return_type->kind != TYPE_ENUM &&
         return_type->kind != TYPE_PTR &&
         return_type->kind != TYPE_NULLPTR)) {
        rcc_error(loc,
                  "C++ cleanup function '%s' has an unsupported delete result",
                  object_type->cleanup_function);
        return NULL;
    }
    return function;
}

static CxxConstructorInfo* sema_select_cxx_new_constructor(
    Type* object_type, ExprList* arguments, SourceLoc loc) {
    CxxClass* cls = object_type ? object_type->cxx_class : NULL;
    CxxConstructorInfo* candidate;
    CxxConstructorInfo* best = NULL;
    int argument_count = sema_cxx_argument_count(arguments);
    int best_total = INT_MAX;
    int best_worst = INT_MAX;
    bool ambiguous = false;
    uint32_t mask;

    if (!cls || argument_count < 0 || argument_count >= 32) return NULL;
    mask = rcc_parser_cxx_constructor_arity_mask(object_type);
    if ((mask & (UINT32_C(1) << (unsigned)argument_count)) == 0u) {
        return NULL;
    }
    for (candidate = cls->constructors; candidate;
         candidate = candidate->next) {
        TypeParam* parameter;
        ExprList* argument;
        int total = 0;
        int worst = 0;
        bool viable = true;
        if (!candidate->method || candidate->access != ACCESS_PUBLIC ||
            candidate->is_deleted || candidate->is_defaulted ||
            !candidate->initializers_are_supported ||
            !candidate->body_is_empty ||
            candidate->parameter_count != argument_count) {
            continue;
        }
        parameter = candidate->parameters;
        argument = arguments;
        while (parameter && argument) {
            int rank = cxx_conversion_rank(argument->expr, parameter->type);
            if (rank < 0) {
                viable = false;
                break;
            }
            total += rank;
            if (rank > worst) worst = rank;
            parameter = parameter->next;
            argument = argument->next;
        }
        if (!viable || parameter || argument) continue;
        if (!best || total < best_total ||
            (total == best_total && worst < best_worst)) {
            best = candidate;
            best_total = total;
            best_worst = worst;
            ambiguous = false;
        } else if (total == best_total && worst == best_worst) {
            ambiguous = true;
        }
    }
    if (ambiguous) {
        rcc_error(loc, "ambiguous constructor for C++ new expression");
        return NULL;
    }
    return best;
}

static bool sema_validate_cxx_new_arguments(Type* object_type,
                                            ExprList* arguments,
                                            CxxConstructorInfo* constructor) {
    TypeField* field;
    TypeParam* parameter;
    ExprList* argument;
    int count = sema_cxx_argument_count(arguments);
    if (!object_type || count < 0) return false;
    if (constructor) {
        parameter = constructor->parameters;
        argument = arguments;
        while (parameter && argument) {
            if (!sema_cxx_new_storage_type(parameter->type) ||
                cxx_conversion_rank(argument->expr, parameter->type) < 0) {
                return false;
            }
            parameter = parameter->next;
            argument = argument->next;
        }
        return parameter == NULL && argument == NULL;
    }
    if (object_type->kind != TYPE_STRUCT && object_type->kind != TYPE_UNION) {
        return count == 0 ||
               (count == 1 && sema_cxx_new_storage_type(object_type) &&
                arguments &&
                cxx_conversion_rank(arguments->expr, object_type) >= 0);
    }
    field = object_type->fields;
    argument = arguments;
    while (field && argument) {
        if (!sema_cxx_new_storage_type(field->type) ||
            cxx_conversion_rank(argument->expr, field->type) < 0) {
            return false;
        }
        field = field->next;
        argument = argument->next;
    }
    return argument == NULL;
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

static void sema_analyze_cxx_default_arguments(Decl* function) {
    for (DeclList* item = function ? function->func_params : NULL;
         item; item = item->next) {
        Decl* parameter = item->decl;
        if (!parameter || !parameter->param_default) continue;
        sema_expr(parameter->param_default);
        /* Default arguments follow C++ implicit-conversion rules.  In
         * particular, the C compatibility path in implicit_cast() permits
         * arbitrary integer-to-pointer conversions, while C++ permits only
         * a null pointer constant here. */
        if (cxx_conversion_rank(parameter->param_default,
                                parameter->type) < 0) {
            rcc_error(parameter->param_default->loc,
                      "default argument is incompatible with parameter %d",
                      parameter->param_index + 1);
        }
    }
}

static void sema_merge_cxx_default_arguments(Decl* prior, Decl* current) {
    DeclList* old_parameter = prior ? prior->func_params : NULL;
    DeclList* new_parameter = current ? current->func_params : NULL;
    for (; old_parameter && new_parameter;
         old_parameter = old_parameter->next,
         new_parameter = new_parameter->next) {
        Expr* old_default = old_parameter->decl
            ? old_parameter->decl->param_default : NULL;
        Expr* new_default = new_parameter->decl
            ? new_parameter->decl->param_default : NULL;
        if (old_default && new_default) {
            rcc_error(new_default->loc,
                      "redefinition of default argument for parameter %d",
                      new_parameter->decl->param_index + 1);
        } else if (old_default && new_parameter->decl) {
            new_parameter->decl->param_default = old_default;
        }
    }
}

static void sema_validate_cxx_default_suffix(Decl* function) {
    bool saw_default = false;
    for (DeclList* item = function ? function->func_params : NULL;
         item; item = item->next) {
        Decl* parameter = item->decl;
        if (parameter && parameter->param_default) {
            saw_default = true;
        } else if (saw_default) {
            if (parameter) {
                rcc_error(
                    parameter->loc,
                    "parameter without a default follows a default argument");
            } else {
                rcc_error(
                    function->loc,
                    "parameter without a default follows a default argument");
            }
        }
    }
}

static bool cxx_remaining_parameters_have_defaults(
    TypeParam* parameter, DeclList* declaration) {
    while (parameter && declaration) {
        if (!declaration->decl || !declaration->decl->param_default) {
            return false;
        }
        parameter = parameter->next;
        declaration = declaration->next;
    }
    return parameter == NULL;
}

static bool sema_append_cxx_default_arguments(
    Expr* call, Decl* function, TypeParam** remaining, int supplied_count) {
    DeclList* declaration;
    TypeParam* parameter;
    int skipped = 0;

    if (!call || !function || function->kind != DECL_FUNC || !remaining) {
        return false;
    }
    declaration = function->func_params;
    while (declaration && skipped < supplied_count) {
        declaration = declaration->next;
        ++skipped;
    }
    parameter = *remaining;
    if (skipped != supplied_count) return false;
    while (parameter && declaration) {
        if (!declaration->decl || !declaration->decl->param_default) {
            return false;
        }
        exprlist_append(&call->call_args,
                        declaration->decl->param_default);
        parameter = parameter->next;
        declaration = declaration->next;
    }
    if (parameter) return false;
    *remaining = NULL;
    return true;
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
        DeclList* declared_parameter;
        ExprList* argument;
        int total = 0;
        int worst = 0;
        bool viable = true;

        if (candidate->kind != DECL_FUNC || !candidate->type ||
            candidate->type->kind != TYPE_FUNC) {
            continue;
        }
        parameter = candidate->type->params;
        declared_parameter = candidate->func_params;
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
            if (declared_parameter) {
                declared_parameter = declared_parameter->next;
            }
        }
        if (!viable ||
            (parameter && !cxx_remaining_parameters_have_defaults(
                parameter, declared_parameter))) {
            continue;
        }
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

/* Member functions are kept on the owning TypeMethod list rather than in the
 * global symbol table because ordinary members use their ABI spelling as the
 * declaration key.  Apply the same conversion ranking used by free-function
 * overloads to the explicit arguments, skipping the implicit this parameter. */
static TypeMethod* sema_select_cxx_member_method(
    Expr* call, Type* aggregate, const char* name) {
    TypeMethod* method;
    TypeMethod* best = NULL;
    int best_total = INT_MAX;
    int best_worst = INT_MAX;
    bool ambiguous = false;

    if (!call || !aggregate || !name) return NULL;
    for (method = aggregate->methods; method; method = method->next) {
        Decl* function;
        TypeParam* parameter;
        DeclList* declared_parameter;
        ExprList* argument;
        int total = 0;
        int worst = 0;
        bool viable = true;

        if (method->kind != TYPE_METHOD_FUNCTION || !method->function_decl ||
            strcmp(method->name, name) != 0) {
            continue;
        }
        function = method->function_decl;
        parameter = function->type ? function->type->params : NULL;
        declared_parameter = function->func_params;
        if (function->func_this_param && parameter) parameter = parameter->next;
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
            if (declared_parameter) declared_parameter =
                declared_parameter->next;
        }
        if (!viable ||
            (parameter && !cxx_remaining_parameters_have_defaults(
                parameter, declared_parameter))) {
            continue;
        }
        if (argument) {
            if (!function->type->variadic) continue;
            while (argument) {
                total += 8;
                worst = 8;
                argument = argument->next;
            }
        }
        if (!best || worst < best_worst ||
            (worst == best_worst && total < best_total)) {
            best = method;
            best_total = total;
            best_worst = worst;
            ambiguous = false;
        } else if (worst == best_worst && total == best_total) {
            ambiguous = true;
        }
    }
    if (!best) {
        rcc_error(call->loc, "no matching member overload for '%s'", name);
        return NULL;
    }
    if (ambiguous) {
        rcc_error(call->loc, "ambiguous member overload for '%s'", name);
        return NULL;
    }
    return best;
}

static Expr* sema_cxx_move_member(Expr* object, TypeField* field) {
    Expr* member = expr_member(object, field->name, object->loc);
    member->member_field = field;
    member->type = field->type;
    return member;
}

/* Attach executable AST only after parser_cxx.c has proved the complete SDK
 * operator=, close, release, constructor, and destructor relationship.  The
 * backend consumes these expressions behind an address-equality guard. */
static bool sema_prepare_cxx_move_assignment(Expr* expression, Type* target) {
    Expr* rhs;
    Expr* source;
    Type* cast_type = NULL;
    TypeMethod* release;
    TypeField* field;
    Symbol* symbol;
    Decl* cleanup_function;
    TypeParam* cleanup_parameter;
    Expr* condition;
    Expr* function_expression;
    Expr* cleanup_argument;
    Expr* cleanup_call;
    Expr* cleanup;
    Expr* release_member;
    Expr* release_call;
    ExprList* cleanup_arguments = NULL;
    CxxMoveAssignment* lowering;
    bool names_move = false;
    if (!expression || !target ||
        (target->kind != TYPE_STRUCT && target->kind != TYPE_UNION)) {
        return false;
    }
    rhs = expression->binary_rhs;
    if (rhs && rhs->kind == EXPR_CAST) {
        cast_type = rhs->cast_type;
        names_move = cast_type && cast_type->kind == TYPE_PTR &&
            cast_type->is_reference && cast_type->is_rvalue_reference &&
            cast_type->base && type_is_compatible(cast_type->base, target);
    }
    if (!target->move_assignment_method && !names_move) {
        if (target->cleanup_function && target->cleanup_field) {
            rcc_error(expression->loc,
                      "C++ scope-cleanup object assignment requires a validated operator=");
        }
        return false;
    }
    if (!target->move_assignment_method || !names_move) {
        rcc_error(expression->loc,
                  "C++ ownership assignment requires a validated rvalue operator=");
        return false;
    }
    source = rhs->cast_expr;
    if (!source || expression->binary_lhs->kind != EXPR_IDENT ||
        source->kind != EXPR_IDENT || !is_lvalue(source)) {
        rcc_error(expression->loc,
                  "validated C++ ownership assignment requires named objects");
        return false;
    }
    release = target->move_assignment_method;
    field = release->field;
    if (!field || target->cleanup_field != field ||
        target->cleanup_invalid != release->constant ||
        !target->cleanup_function) {
        rcc_error(expression->loc,
                  "validated C++ ownership assignment metadata is inconsistent");
        return false;
    }
    symbol = symtab_lookup(g_symtab, target->cleanup_function);
    cleanup_function = symbol && symbol->kind == SYM_FUNC
        ? symbol->decl : NULL;
    if (!cleanup_function || !cleanup_function->type ||
        cleanup_function->type->kind != TYPE_FUNC) {
        rcc_error(expression->loc,
                  "C++ cleanup function '%s' is not declared",
                  target->cleanup_function);
        return false;
    }
    cleanup_parameter = cleanup_function->type->params;
    if (!cleanup_parameter || cleanup_parameter->next ||
        !type_is_compatible(cleanup_parameter->type, field->type)) {
        rcc_error(expression->loc,
                  "C++ cleanup function '%s' has an incompatible signature",
                  target->cleanup_function);
        return false;
    }

    condition = expr_binary(
        EXPR_NE,
        sema_cxx_move_member(expression->binary_lhs, field),
        expr_int(target->cleanup_invalid, expression->loc),
        expression->loc);
    condition->type = type_int;
    function_expression = expr_ident(cleanup_function->name,
                                     expression->loc);
    function_expression->ident_decl = cleanup_function;
    function_expression->type = cleanup_function->type;
    cleanup_argument = sema_cxx_move_member(expression->binary_lhs, field);
    exprlist_append(&cleanup_arguments, cleanup_argument);
    cleanup_call = expr_call(function_expression, cleanup_arguments,
                             expression->loc);
    cleanup_call->type = cleanup_function->type->ret_type;
    cleanup = expr_cond(condition, cleanup_call,
                        expr_int(0, expression->loc), expression->loc);
    cleanup->type = cleanup_call->type &&
        cleanup_call->type->kind != TYPE_VOID
        ? cleanup_call->type : type_int;

    release_member = expr_member(source, release->name, expression->loc);
    release_call = expr_call(release_member, NULL, expression->loc);
    release_call->call_method = release;
    release_call->type = release->return_type &&
        release->return_type->is_reference
        ? release->return_type->base : release->return_type;

    lowering = ast_arena_alloc(sizeof(*lowering));
    lowering->source = source;
    lowering->cleanup = cleanup;
    lowering->release = release_call;
    expression->cxx_move_assignment = lowering;
    return true;
}

static bool sema_prepare_cxx_close_call(Expr* expression,
                                        TypeMethod* method,
                                        Expr* object) {
    Symbol* symbol;
    Decl* cleanup_function;
    TypeParam* parameter;
    Expr* function_expression;
    Expr* argument;
    ExprList* arguments = NULL;
    CxxCloseCall* lowering;
    if (!expression || !method ||
        method->kind != TYPE_METHOD_FIELD_CLOSE || !object ||
        object->kind != EXPR_IDENT || !method->field ||
        !method->cleanup_function || !method->result_field) {
        if (expression) {
            rcc_error(expression->loc,
                      "validated C++ close requires a named ownership object");
        }
        return false;
    }
    symbol = symtab_lookup(g_symtab, method->cleanup_function);
    cleanup_function = symbol && symbol->kind == SYM_FUNC
        ? symbol->decl : NULL;
    if (!cleanup_function || !cleanup_function->type ||
        cleanup_function->type->kind != TYPE_FUNC) {
        rcc_error(expression->loc,
                  "C++ cleanup function '%s' is not declared",
                  method->cleanup_function);
        return false;
    }
    parameter = cleanup_function->type->params;
    if (!parameter || parameter->next ||
        !type_is_compatible(parameter->type, method->field->type) ||
        !type_is_compatible(cleanup_function->type->ret_type,
                            method->result_field->type)) {
        rcc_error(expression->loc,
                  "C++ cleanup function '%s' has an incompatible close signature",
                  method->cleanup_function);
        return false;
    }

    function_expression = expr_ident(cleanup_function->name,
                                     expression->loc);
    function_expression->ident_decl = cleanup_function;
    function_expression->type = cleanup_function->type;
    argument = sema_cxx_move_member(object, method->field);
    exprlist_append(&arguments, argument);

    lowering = ast_arena_alloc(sizeof(*lowering));
    lowering->object = object;
    lowering->handle = sema_cxx_move_member(object, method->field);
    lowering->cleanup = expr_call(function_expression, arguments,
                                  expression->loc);
    lowering->cleanup->type = cleanup_function->type->ret_type;
    expression->cxx_close_call = lowering;
    return true;
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
            if (!expr->type || !type_is_floating(expr->type)) {
                expr->type = type_double;
            }
            break;

        case EXPR_CHAR_LIT:
            expr->type = type_int;
            break;

        case EXPR_STRING_LIT:
            expr->type = type_array(type_char,
                                    (int)strlen(expr->str_val) + 1);
            break;

        case EXPR_IDENT: {
            if (expr->ident_decl && expr->ident_decl->kind == DECL_FUNC) {
                expr->type = expr->ident_decl->type;
                break;
            }
            if (expr->ident_decl &&
                (expr->ident_decl->kind == DECL_VAR ||
                 expr->ident_decl->kind == DECL_PARAM)) {
                expr->type = expr->ident_decl->type &&
                    expr->ident_decl->type->is_reference
                    ? expr->ident_decl->type->base
                    : expr->ident_decl->type;
                break;
            }
            Symbol* sym = sema_cxx_lookup_name(expr->ident_name);
            if (!sym && current_cxx_method_owner &&
                current_cxx_this_param && expr->ident_name) {
                TypeField* field;
                for (field = current_cxx_method_owner->fields; field;
                     field = field->next) {
                    if (field->name &&
                        strcmp(field->name, expr->ident_name) == 0) {
                        Expr* object = expr_ident("this", expr->loc);
                        Type* field_type = field->type;
                        object->ident_decl = current_cxx_this_param;
                        object->type = current_cxx_this_param->type;
                        expr->kind = EXPR_PTR_MEMBER;
                        expr->member_base = object;
                        expr->member_name = expr->ident_name;
                        expr->member_field = field;
                        if (current_cxx_method_owner->is_const &&
                            field_type && !field_type->is_const) {
                            Type* qualified = ast_arena_alloc(sizeof(*qualified));
                            *qualified = *field_type;
                            qualified->is_const = true;
                            field_type = qualified;
                        }
                        expr->type = field_type;
                        break;
                    }
                }
            }
            if (expr->kind != EXPR_IDENT) break;
            if (!sym) {
                sym = sema_cxx_runtime_function(expr->ident_name,
                                                 expr->loc);
            }
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
            if (sema_is_cxx_nullptr_expr(expr->unary_operand)) {
                rcc_error(expr->loc,
                          "nullptr does not support arithmetic operators");
            } else if (!type_is_arithmetic(t) && t->kind != TYPE_ENUM) {
                rcc_error(expr->loc, "invalid operand type for unary operator");
            }
            expr->type = sema_is_integer_type(t)
                ? sema_integer_promotion(t) : t;
            break;
        }

        case EXPR_BITNOT: {
            Type* t = sema_expr(expr->unary_operand);
            if (sema_is_cxx_nullptr_expr(expr->unary_operand)) {
                rcc_error(expr->loc,
                          "nullptr does not support integer operators");
            } else if (!sema_is_integer_type(t)) {
                rcc_error(expr->loc,
                          "bitwise complement requires integer operand");
            }
            expr->type = sema_integer_promotion(t);
            break;
        }

        case EXPR_NOT: {
            Type* t;
            expr->unary_operand = sema_contextual_bool(expr->unary_operand);
            t = expr->unary_operand->type;
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
            if (!is_modifiable_lvalue(expr->unary_operand)) {
                rcc_error(expr->loc,
                          "increment/decrement requires modifiable lvalue");
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
                sema_validate_array_parameter_type(expr->sizeof_type,
                                                   expr->loc, false);
                sema_vla_bounds(expr->sizeof_type, expr->loc);
                expr->type = type_uint;
            } else {
                sema_expr(expr->unary_operand);
                expr->type = type_uint;
            }
            break;
        }

        case EXPR_CAST: {
            sema_expr(expr->cast_expr);
            sema_validate_array_parameter_type(expr->cast_type,
                                               expr->loc, false);
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
            bool has_nullptr =
                sema_is_cxx_nullptr_expr(expr->binary_lhs) ||
                sema_is_cxx_nullptr_expr(expr->binary_rhs);
            bool left_pointer = type_is_pointer(lt) || type_is_array(lt);
            bool right_pointer = type_is_pointer(rt) || type_is_array(rt);
            Type* left_result = type_is_array(lt) ? type_ptr(lt->base) : lt;
            Type* right_result = type_is_array(rt) ? type_ptr(rt->base) : rt;

            /* Pointer arithmetic */
            if (has_nullptr) {
                rcc_error(expr->loc,
                          "nullptr does not support arithmetic operators");
                expr->type = type_int;
            } else if (left_pointer && type_is_integer(rt)) {
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
            if (sema_is_cxx_nullptr_expr(expr->binary_lhs) ||
                sema_is_cxx_nullptr_expr(expr->binary_rhs)) {
                rcc_error(expr->loc,
                          "nullptr does not support arithmetic operators");
            } else if (!type_is_arithmetic(lt) || !type_is_arithmetic(rt)) {
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
            Type* argument_type = expr->va_arg_type;
            if (!list_type || (list_type->kind != TYPE_ARRAY &&
                               list_type->kind != TYPE_PTR)) {
                rcc_error(expr->loc, "va_arg requires a va_list object");
            }
            if (!argument_type || !type_is_complete(argument_type) ||
                argument_type->size <= 0 ||
                (argument_type->kind != TYPE_STRUCT &&
                 argument_type->kind != TYPE_UNION &&
                 !(type_is_integer(argument_type) ||
                   argument_type->kind == TYPE_ENUM ||
                   argument_type->kind == TYPE_PTR ||
                   argument_type->kind == TYPE_FLOAT ||
                   argument_type->kind == TYPE_DOUBLE)) ||
                ((argument_type->kind != TYPE_STRUCT &&
                  argument_type->kind != TYPE_UNION) &&
                 argument_type->size > 8)) {
                rcc_error(expr->loc,
                          "va_arg requires a complete fixed scalar or aggregate object type");
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
            if (sema_is_cxx_nullptr_expr(expr->binary_lhs) ||
                sema_is_cxx_nullptr_expr(expr->binary_rhs)) {
                rcc_error(expr->loc,
                          "nullptr does not support integer operators");
            } else if (!sema_is_integer_type(lt) ||
                       !sema_is_integer_type(rt)) {
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
            if (sema_is_cxx_nullptr_expr(expr->binary_lhs) ||
                sema_is_cxx_nullptr_expr(expr->binary_rhs)) {
                rcc_error(expr->loc,
                          "nullptr does not support integer operators");
            } else if (!sema_is_integer_type(lt) ||
                       !sema_is_integer_type(rt)) {
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
            if (sema_is_cxx_nullptr_expr(expr->binary_lhs) ||
                sema_is_cxx_nullptr_expr(expr->binary_rhs)) {
                rcc_error(expr->loc,
                          "nullptr does not support integer operators");
            } else if (!sema_is_integer_type(lt) ||
                       !sema_is_integer_type(rt)) {
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
            bool equality = expr->kind == EXPR_EQ || expr->kind == EXPR_NE;
            bool left_nullptr =
                sema_is_cxx_nullptr_expr(expr->binary_lhs);
            bool right_nullptr =
                sema_is_cxx_nullptr_expr(expr->binary_rhs);
            bool arithmetic = !left_nullptr && !right_nullptr &&
                              (type_is_arithmetic(left_value) ||
                               left_value->kind == TYPE_ENUM) &&
                              (type_is_arithmetic(right_value) ||
                               right_value->kind == TYPE_ENUM);
            bool pointers = type_is_pointer(left_value) &&
                            type_is_pointer(right_value);
            int64_t left_constant = 1;
            int64_t right_constant = 1;
            bool left_zero = sema_is_integer_type(left_value) &&
                expr_eval_integer_constant(expr->binary_lhs,
                                           &left_constant) &&
                left_constant == 0;
            bool right_zero = sema_is_integer_type(right_value) &&
                expr_eval_integer_constant(expr->binary_rhs,
                                           &right_constant) &&
                right_constant == 0;
            bool nullptr_equality = equality &&
                ((left_nullptr && right_nullptr) ||
                 (left_nullptr && right_zero) ||
                 (right_nullptr && left_zero));
            bool pointer_null = equality &&
                ((type_is_pointer(left_value) &&
                  (right_nullptr || right_zero)) ||
                 (type_is_pointer(right_value) &&
                  (left_nullptr || left_zero)));
            if (!arithmetic && !pointers && !pointer_null &&
                !nullptr_equality) {
                rcc_error(expr->loc,
                          "comparison requires arithmetic or pointer operands");
            }
            expr->type = type_int;
            break;
        }

        case EXPR_AND:
        case EXPR_OR: {
            Type* left;
            Type* right;
            expr->binary_lhs = sema_contextual_bool(expr->binary_lhs);
            expr->binary_rhs = sema_contextual_bool(expr->binary_rhs);
            left = expr->binary_lhs->type;
            right = expr->binary_rhs->type;
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
            if (!is_modifiable_lvalue(expr->binary_lhs)) {
                rcc_error(expr->loc,
                          "assignment requires modifiable lvalue");
            }
            if (sema_is_cxx_nullptr_expr(expr->binary_rhs)) {
                rcc_error(expr->loc,
                          "nullptr does not support arithmetic operators");
            } else if (!((type_is_pointer(lt) &&
                          is_pointer_arithmetic_type(lt) &&
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
            if (!is_modifiable_lvalue(expr->binary_lhs)) {
                rcc_error(expr->loc,
                          "assignment requires modifiable lvalue");
            }
            if (sema_is_cxx_nullptr_expr(expr->binary_rhs)) {
                rcc_error(expr->loc,
                          "nullptr does not support arithmetic operators");
            } else if (!type_is_arithmetic(lt) ||
                       !type_is_arithmetic(rt)) {
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
            if (!is_modifiable_lvalue(expr->binary_lhs)) {
                rcc_error(expr->loc,
                          "assignment requires modifiable lvalue");
            }
            if (sema_is_cxx_nullptr_expr(expr->binary_rhs)) {
                rcc_error(expr->loc,
                          "nullptr does not support integer operators");
            } else if (!type_is_integer(lt) || !type_is_integer(rt)) {
                rcc_error(expr->loc,
                          "integer compound assignment requires integer operands");
            }
            expr->type = lt;
            break;
        }

        case EXPR_ASSIGN: {
            Type* lt = sema_expr(expr->binary_lhs);
            sema_expr(expr->binary_rhs);
            if (!is_modifiable_lvalue(expr->binary_lhs)) {
                rcc_error(expr->loc,
                          "assignment requires modifiable lvalue");
            }
            if (sema_is_cxx_nullptr_expr(expr->binary_rhs) &&
                !type_is_pointer(lt) && lt->kind != TYPE_NULLPTR) {
                rcc_error(expr->loc,
                          "nullptr can only be assigned to a pointer");
            }
            sema_prepare_cxx_move_assignment(expr, lt);
            expr->type = lt;
            break;
        }

        case EXPR_COND: {
            expr->cond_test = sema_contextual_bool(expr->cond_test);
            Type* tt = sema_expr(expr->cond_then);
            Type* et = sema_expr(expr->cond_else);
            Type* tv = generic_selection_type(tt);
            Type* ev = generic_selection_type(et);
            bool then_nullptr =
                sema_is_cxx_nullptr_expr(expr->cond_then);
            bool else_nullptr =
                sema_is_cxx_nullptr_expr(expr->cond_else);
            if (then_nullptr && else_nullptr) {
                expr->type = type_nullptr;
            } else if (then_nullptr && type_is_pointer(ev)) {
                expr->type = ev;
            } else if (else_nullptr && type_is_pointer(tv)) {
                expr->type = tv;
            } else {
                expr->type = type_common(tt, et);
            }
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
            Decl* call_declaration = NULL;
            int argument_index = 1;
            bool reported_too_many = false;
            bool arguments_analyzed = false;
            if (expr->call_is_delete) {
                Type* freed_type;
                Type* object_type;
                Decl* cleanup_function;

                sema_expr(expr->call_func);
                if (!expr->call_args || expr->call_args->next ||
                    !expr->call_args->expr) {
                    rcc_error(expr->loc,
                              "delete expression requires one pointer operand");
                    expr->type = type_void;
                    return expr->type;
                }
                freed_type = sema_expr(expr->call_args->expr);
                expr->type = type_void;
                if (!freed_type || freed_type->kind != TYPE_PTR ||
                    !freed_type->base) {
                    rcc_error(expr->loc,
                              "delete expression operand must be a pointer");
                    return expr->type;
                }
                object_type = freed_type->base;
                if (!type_is_complete(object_type)) {
                    rcc_error(expr->loc,
                              "delete expression requires a complete object type");
                    return expr->type;
                }
                if (object_type->cxx_nontrivial &&
                    !sema_cxx_trivially_destructible(object_type, 0)) {
                    if (expr->call_delete_is_array) {
                        rcc_error(expr->loc,
                                  "array delete requires element destructor lowering");
                        return expr->type;
                    }
                    if (!object_type->cleanup_function ||
                        !object_type->cleanup_field) {
                        rcc_error(expr->loc,
                                  "delete requires C++ destructor lowering for a non-trivial object");
                        return expr->type;
                    }
                    cleanup_function = sema_cxx_cleanup_function(
                        object_type, expr->loc);
                    if (cleanup_function) {
                        expr->call_delete_cleanup = cleanup_function;
                        expr->call_delete_cleanup_field =
                            object_type->cleanup_field;
                        expr->call_delete_cleanup_invalid =
                            object_type->cleanup_invalid;
                    }
                }
                return expr->type;
            }
            if (expr->call_is_new) {
                Type* object_type = expr->call_new_type;
                CxxClass* cls = object_type ? object_type->cxx_class : NULL;
                int argument_count;
                CxxConstructorInfo* constructor = NULL;

                /* Analyze the allocator size through the normal call path;
                 * this preserves the declared RinOS allocation ABI and also
                 * validates dynamic array bounds. */
                sema_expr(expr->call_func);
                for (argument = expr->call_args; argument;
                     argument = argument->next) {
                    sema_expr(argument->expr);
                }
                for (argument = expr->call_new_args; argument;
                     argument = argument->next) {
                    sema_expr(argument->expr);
                }
                expr->type = object_type ? type_ptr(object_type) : type_int;
                if (!object_type || object_type == type_void ||
                    object_type->kind == TYPE_FUNC ||
                    !type_is_complete(object_type)) {
                    return expr->type;
                }
                argument_count = sema_cxx_argument_count(expr->call_new_args);
                if (expr->call_new_is_array && expr->call_new_args) {
                    rcc_error(expr->loc,
                              "array new does not accept element initializers");
                }
                if (expr->call_new_is_array && object_type->cxx_nontrivial) {
                    rcc_error(expr->loc,
                              "array new requires element constructor and destructor lowering");
                    return expr->type;
                }
                if (object_type->cxx_nontrivial &&
                    (!cls || !rcc_parser_cxx_constructor_arity_mask(object_type))) {
                    rcc_error(expr->loc,
                              "new for this C++ object requires an unsupported constructor or destructor ABI");
                    return expr->type;
                }
                if (cls && cls->constructors && !expr->call_new_is_array) {
                    constructor = sema_select_cxx_new_constructor(
                        object_type, expr->call_new_args, expr->loc);
                    if (argument_count != 0 || expr->call_new_value_init ||
                        object_type->cxx_nontrivial) {
                        if (!constructor &&
                            (argument_count != 0 || object_type->cxx_nontrivial)) {
                            rcc_error(expr->loc,
                                      "no safely lowerable constructor accepts the new initializer");
                        }
                    }
                }
                if (constructor) {
                    if (!sema_validate_cxx_new_arguments(
                            object_type, expr->call_new_args, constructor)) {
                        rcc_error(expr->loc,
                                  "new constructor arguments require unsupported object storage");
                    } else {
                        expr->call_new_constructor = constructor;
                    }
                } else if (argument_count != 0 &&
                           !sema_validate_cxx_new_arguments(
                               object_type, expr->call_new_args, NULL)) {
                    rcc_error(expr->loc,
                              "new initializer is incompatible with the allocated object");
                }
                return expr->type;
            }
            if (expr->call_func && expr->call_func->kind == EXPR_IDENT &&
                current_cxx_method_owner) {
                TypeMethod* method = sema_find_function_method(
                    current_cxx_method_owner, expr->call_func->ident_name);
                if (method && method->function_decl) {
                    expr->call_func->ident_name = method->function_decl->name;
                    expr->call_func->ident_decl = method->function_decl;
                    expr->call_func->type = method->function_decl->type;
                    if (method->function_decl->func_this_param) {
                        Expr* this_argument = expr_ident(
                            "this", expr->call_func->loc);
                        ExprList* implicit_argument =
                            exprlist_new(this_argument);
                        this_argument->ident_decl = current_cxx_this_param;
                        this_argument->type = current_cxx_this_param->type;
                        implicit_argument->designator_kind =
                            INIT_DESIGNATOR_NONE;
                        implicit_argument->designator_index = 0;
                        implicit_argument->designator_field = NULL;
                        implicit_argument->next = expr->call_args;
                        expr->call_args = implicit_argument;
                        if (method->is_virtual) {
                            if (method->vtable_index < 0 ||
                                !method->vtable_symbol) {
                                rcc_error(expr->loc,
                                          "virtual member '%s' has no vtable entry",
                                          expr->call_func->ident_name);
                            } else {
                                expr->call_is_virtual = true;
                                expr->call_virtual_index =
                                    method->vtable_index;
                                expr->call_virtual_object = this_argument;
                            }
                        }
                    }
                }
            }
            if (expr->call_func && expr->call_func->kind == EXPR_IDENT &&
                strcmp(expr->call_func->ident_name, "rin_free") == 0 &&
                expr->call_args && expr->call_args->expr) {
                Type* freed_type = sema_expr(expr->call_args->expr);
                if (freed_type && freed_type->kind == TYPE_PTR &&
                    freed_type->base && freed_type->base->cxx_nontrivial &&
                    !sema_cxx_trivially_destructible(freed_type->base, 0)) {
                    rcc_error(expr->loc,
                              "delete requires C++ destructor lowering for a non-trivial object");
                }
            }
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
                    if (method->kind == TYPE_METHOD_FIELD_CLOSE) {
                        sema_prepare_cxx_close_call(expr, method,
                                                    member->member_base);
                    }
                    expr->type = method->return_type &&
                        method->return_type->is_reference
                        ? method->return_type->base
                        : method->return_type;
                    break;
                }
                for (argument = expr->call_args; argument;
                     argument = argument->next) {
                    sema_expr(argument->expr);
                }
                method = sema_select_cxx_member_method(
                    expr, owner, member->member_name);
                if (method && method->function_decl) {
                    Expr* function_expression;
                    arguments_analyzed = true;
                    if (method->cxx_access != 0u) {
                        rcc_error(expr->loc, "method '%s' is not accessible",
                                  member->member_name);
                    }
                    function_expression = expr_ident(
                        method->function_decl->name, expr->loc);
                    function_expression->ident_decl =
                        method->function_decl;
                    function_expression->type = method->function_decl->type;
                    expr->call_func = function_expression;
                    if (method->function_decl->func_this_param) {
                        Expr* this_argument = member->kind == EXPR_PTR_MEMBER
                            ? member->member_base
                            : expr_unary(EXPR_ADDR, member->member_base,
                                         member->loc);
                        Type* expected_this =
                            method->function_decl->func_this_param->type;
                        /* Establish the source object's type before deciding
                         * whether an inherited-base conversion is needed. */
                        sema_expr(this_argument);
                        if (method->this_adjustment != 0) {
                            Expr* byte_pointer = expr_cast(
                                type_ptr(type_char), this_argument,
                                member->loc);
                            Expr* byte_offset = expr_binary(
                                EXPR_ADD, byte_pointer,
                                expr_int(method->this_adjustment,
                                         member->loc), member->loc);
                            this_argument = expr_cast(
                                expected_this, byte_offset, member->loc);
                        } else if (expected_this && this_argument->type &&
                                   !type_is_compatible(this_argument->type,
                                                       expected_this)) {
                            this_argument = expr_cast(
                                expected_this, this_argument, member->loc);
                        }
                        ExprList* implicit_argument =
                            exprlist_new(this_argument);
                        implicit_argument->designator_kind =
                            INIT_DESIGNATOR_NONE;
                        implicit_argument->designator_index = 0;
                        implicit_argument->designator_field = NULL;
                        implicit_argument->next = expr->call_args;
                        expr->call_args = implicit_argument;
                        sema_expr(this_argument);
                        if (method->is_virtual) {
                            if (method->vtable_index < 0 ||
                                !method->vtable_symbol) {
                                rcc_error(expr->loc,
                                          "virtual member '%s' has no vtable entry",
                                          member->member_name);
                            } else {
                                expr->call_is_virtual = true;
                                expr->call_virtual_index =
                                    method->vtable_index;
                                expr->call_virtual_object = this_argument;
                            }
                        }
                    }
                }
            }
            if (expr->call_func && expr->call_func->kind == EXPR_IDENT &&
                !sema_cxx_lookup_name(expr->call_func->ident_name)) {
                Symbol* adl_symbol;
                for (argument = expr->call_args; argument;
                     argument = argument->next) {
                    sema_expr(argument->expr);
                }
                adl_symbol = sema_cxx_adl_lookup(
                    expr->call_func->ident_name, expr->call_args);
                if (adl_symbol) {
                    expr->call_func->ident_name = adl_symbol->name;
                    expr->call_func->ident_decl = adl_symbol->decl;
                    expr->call_func->type = adl_symbol->type;
                    arguments_analyzed = true;
                }
            }
            if (sema_atomic_builtin_call(expr)) break;
            if (expr->call_func->kind == EXPR_IDENT) {
                Symbol* overload = sema_cxx_lookup_name(
                    expr->call_func->ident_name);
                if (overload && overload->kind == SYM_FUNC &&
                    overload->decl && overload->decl->func_has_cxx_linkage &&
                    overload->decl->func_overload_next &&
                    (!expr->call_func->ident_decl ||
                     !expr->call_func->ident_decl->func_is_template_instance)) {
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
            call_declaration = selected_overload;
            if (!call_declaration && expr->call_func->kind == EXPR_IDENT &&
                expr->call_func->ident_decl &&
                expr->call_func->ident_decl->kind == DECL_FUNC) {
                call_declaration = expr->call_func->ident_decl;
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
                if (!sema_append_cxx_default_arguments(
                        expr, call_declaration, &parameter,
                        argument_index - 1)) {
                    rcc_error(expr->loc,
                              "too few arguments to function call");
                }
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
                    if ((bt->is_const && !expr->type->is_const) ||
                        (bt->is_volatile && !expr->type->is_volatile)) {
                        Type* qualified = ast_arena_alloc(sizeof(*qualified));
                        *qualified = *expr->type;
                        qualified->is_const = qualified->is_const ||
                                              bt->is_const;
                        qualified->is_volatile = qualified->is_volatile ||
                                                 bt->is_volatile;
                        expr->type = qualified;
                    }
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
            stmt->if_cond = sema_contextual_bool(stmt->if_cond);
            sema_stmt(stmt->if_then);
            if (stmt->if_else) {
                sema_stmt(stmt->if_else);
            }
            break;

        case STMT_WHILE:
            stmt->while_cond = sema_contextual_bool(stmt->while_cond);
            ++loop_depth;
            sema_stmt(stmt->while_body);
            --loop_depth;
            break;

        case STMT_DO:
            ++loop_depth;
            sema_stmt(stmt->while_body);
            --loop_depth;
            stmt->while_cond = sema_contextual_bool(stmt->while_cond);
            break;

        case STMT_FOR:
            symtab_enter_scope(g_symtab);
            if (stmt->for_init) {
                sema_stmt(stmt->for_init);
            }
            if (stmt->for_cond) {
                stmt->for_cond = sema_contextual_bool(stmt->for_cond);
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
            if (!sema_switch_cleanup_scopes_safe(stmt->switch_body, false)) {
                rcc_error(stmt->loc,
                          "case label crosses C++ scope-cleanup object initialization");
            }
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

static bool sema_type_has_vla(Type* type) {
    return type && type->kind == TYPE_ARRAY &&
           (type->array_bound != NULL || type->array_unspecified_bound ||
            sema_type_has_vla(type->base));
}

/* A variably modified type may be hidden behind a pointer (or a typedef),
 * even though only an array object itself needs dynamic storage.  Keep this
 * predicate separate from sema_type_has_vla so pointer variables are not
 * mistaken for VLA objects during stack layout. */
static bool sema_type_is_variably_modified(Type* type) {
    if (!type) return false;
    if (type->kind == TYPE_ARRAY) {
        return type->array_bound != NULL || type->array_unspecified_bound ||
               sema_type_is_variably_modified(type->base);
    }
    if (type->kind == TYPE_PTR) {
        return sema_type_is_variably_modified(type->base);
    }
    return false;
}

static int sema_vla_dimension_count(Type* type) {
    if (!type || type->kind != TYPE_ARRAY) return 0;
    return 1 + sema_vla_dimension_count(type->base);
}

static void sema_vla_bounds(Type* type, SourceLoc loc) {
    Type* bound_type;
    if (!type) return;
    if (type->kind == TYPE_PTR) {
        sema_vla_bounds(type->base, loc);
        return;
    }
    if (type->kind != TYPE_ARRAY) return;
    sema_vla_bounds(type->base, loc);
    if (!type->array_bound) return;
    bound_type = sema_expr(type->array_bound);
    if (!bound_type || !type_is_integer(bound_type)) {
        rcc_error(type->array_bound->loc,
                  "variable-length array bound requires an integer type");
    }
    (void)loc;
}

static void sema_validate_array_parameter_type(Type* type, SourceLoc loc,
                                               bool is_parameter) {
    if (!type) return;
    if (type->kind == TYPE_ARRAY) {
        bool has_spec = type->array_parameter_static ||
            type->array_unspecified_bound ||
            type->array_parameter_const ||
            type->array_parameter_volatile ||
            type->array_parameter_restrict;
        if (has_spec && !is_parameter) {
            rcc_error(loc,
                      "array parameter qualifiers are only valid in function parameter declarations");
        }
        if (is_parameter && type->array_unspecified_bound) {
            rcc_error(loc,
                      "unspecified variable-length array is only valid in a function prototype");
        }
        if (is_parameter && type->array_parameter_static &&
            type->array_len <= 0 && !type->array_bound) {
            rcc_error(loc,
                      "static array parameter requires a bound expression");
        }
        sema_validate_array_parameter_type(type->base, loc, is_parameter);
    } else if (type->kind == TYPE_PTR) {
        sema_validate_array_parameter_type(type->base, loc, is_parameter);
    }
}

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

static int initializer_scalar_capacity(Type* type);
static bool initializer_is_plain_sequence(Expr* initializer);
static bool initializer_is_aggregate_type(Type* type);
static bool initializer_directly_initializes(Type* type, Expr* initializer);
static void consume_brace_elided_subobject(Type* type, ExprList** source);

static void sema_infer_initializer_type(Type* type, Expr* initializer) {
    Expr* string;
    int64_t cursor = 0;
    int64_t maximum = -1;
    if (!type || !initializer) return;
    if (sema_type_has_vla(type)) return;
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
    if (type->array_len < 0 && type->base &&
        initializer_is_plain_sequence(initializer)) {
        ExprList* source = initializer->compound_init;
        while (source) {
            ExprList* item = source;
            if (item->designator_kind == INIT_DESIGNATOR_INDEX) {
                cursor = item->designator_index;
                source = source->next;
            } else if (item->designator_kind == INIT_DESIGNATOR_NONE &&
                       initializer_is_aggregate_type(type->base) &&
                       !initializer_directly_initializes(type->base,
                                                         item->expr)) {
                consume_brace_elided_subobject(type->base, &source);
            } else {
                source = source->next;
            }
            if (item->designator_kind != INIT_DESIGNATOR_FIELD &&
                cursor > maximum) {
                maximum = cursor;
            }
            if (cursor < INT64_MAX) ++cursor;
        }
    } else {
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

/* Return the number of scalar subobjects reached by C's brace-elision walk.
 * This is intentionally bounded by the already-laid-out type graph; a
 * flexible or incomplete aggregate is left to the ordinary diagnostic path. */
static int initializer_scalar_capacity(Type* type) {
    int64_t capacity = 0;
    if (!type) return 0;
    if (type->kind == TYPE_ARRAY) {
        if (type->array_len < 0 || !type->base) return 0;
        capacity = (int64_t)type->array_len *
                   initializer_scalar_capacity(type->base);
    } else if (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) {
        TypeField* field = type->fields;
        if (type->kind == TYPE_UNION && field) {
            capacity = initializer_scalar_capacity(field->type);
        } else {
            for (; field; field = field->next) {
                capacity += initializer_scalar_capacity(field->type);
                if (capacity > INT_MAX) break;
            }
        }
    } else {
        return 1;
    }
    if (capacity <= 0 || capacity > INT_MAX) return 0;
    return (int)capacity;
}

static bool initializer_is_plain_sequence(Expr* initializer) {
    if (!initializer || initializer->kind != EXPR_COMPOUND) return false;
    for (ExprList* item = initializer->compound_init; item;
         item = item->next) {
        if (item->designator_kind != INIT_DESIGNATOR_NONE || !item->expr) {
            return false;
        }
    }
    return true;
}

static bool initializer_is_aggregate_type(Type* type) {
    return type && (type->kind == TYPE_ARRAY ||
                    type->kind == TYPE_STRUCT ||
                    type->kind == TYPE_UNION);
}

static bool initializer_directly_initializes(Type* type, Expr* initializer) {
    if (!initializer_is_aggregate_type(type) || !initializer) return false;
    if (initializer_character_string(type, initializer)) return true;
    if (initializer->kind != EXPR_COMPOUND) {
        /* An aggregate expression (for example a named struct object) is a
         * direct initializer.  Treating it as a brace-elided scalar clause
         * would recursively consume the following clauses and later report
         * a misleading scalar-initializer diagnostic. */
        Type* initializer_type = initializer->type
            ? initializer->type : sema_expr(initializer);
        return initializer_type &&
            type_is_compatible(type, initializer_type);
    }
    if (!initializer->compound_type) return true;
    return type_is_compatible(type, initializer->compound_type);
}

/* Consume one aggregate subobject from a brace-elided initializer sequence.
 * A braced subinitializer or character string initializes the current
 * aggregate as a whole; otherwise scalar clauses continue recursively into
 * its members. */
static void consume_brace_elided_subobject(Type* type, ExprList** source) {
    if (!type || !source || !*source) return;
    if (!initializer_is_aggregate_type(type)) {
        *source = (*source)->next;
        return;
    }
    if (type->kind == TYPE_ARRAY) {
        if (type->array_len < 0 || !type->base) {
            *source = (*source)->next;
            return;
        }
        for (int index = 0; index < type->array_len && *source; ++index) {
            if (initializer_directly_initializes(type->base,
                                                  (*source)->expr)) {
                *source = (*source)->next;
            } else {
                consume_brace_elided_subobject(type->base, source);
            }
        }
        return;
    }
    TypeField* field = type->fields;
    if (type->kind == TYPE_UNION) {
        if (field && *source) {
            if (initializer_directly_initializes(field->type,
                                                  (*source)->expr)) {
                *source = (*source)->next;
            } else {
                consume_brace_elided_subobject(field->type, source);
            }
        }
        return;
    }
    for (; field && *source; field = field->next) {
        if (initializer_directly_initializes(field->type,
                                              (*source)->expr)) {
            *source = (*source)->next;
        } else {
            consume_brace_elided_subobject(field->type, source);
        }
    }
}

static void normalize_brace_elided_initializer(Type* type,
                                                Expr* initializer) {
    ExprList* source;
    ExprList* normalized = NULL;
    bool plain_sequence = true;

    if (!type || !initializer || initializer->kind != EXPR_COMPOUND ||
        (type->kind != TYPE_ARRAY && type->kind != TYPE_STRUCT &&
         type->kind != TYPE_UNION)) {
        return;
    }
    plain_sequence = initializer_is_plain_sequence(initializer);
    if (!plain_sequence) return;

    source = initializer->compound_init;
    if (type->kind == TYPE_ARRAY) {
        for (int index = 0; index < type->array_len && source; ++index) {
            Type* element_type = type->base;
            if (initializer_directly_initializes(element_type,
                                                 source->expr)) {
                exprlist_append_designated(&normalized, source->expr,
                                           INIT_DESIGNATOR_NONE, 0, NULL);
                source = source->next;
            } else if (initializer_is_aggregate_type(element_type)) {
                int capacity = initializer_scalar_capacity(element_type);
                ExprList* nested_items = NULL;
                int consumed = 0;
                if (capacity <= 0) return;
                while (source && consumed < capacity) {
                    exprlist_append_designated(
                        &nested_items, source->expr, INIT_DESIGNATOR_NONE,
                        0, NULL);
                    source = source->next;
                    ++consumed;
                }
                Expr* nested = expr_initializer_list(nested_items,
                                                     initializer->loc);
                nested->compound_type = element_type;
                nested->type = element_type;
                normalize_brace_elided_initializer(element_type, nested);
                exprlist_append_designated(&normalized, nested,
                                           INIT_DESIGNATOR_NONE, 0, NULL);
            } else {
                exprlist_append_designated(&normalized, source->expr,
                                           INIT_DESIGNATOR_NONE, 0, NULL);
                source = source->next;
            }
        }
    } else {
        TypeField* field = type->fields;
        while (field && source) {
            Type* field_type = field->type;
            if (initializer_directly_initializes(field_type,
                                                 source->expr)) {
                exprlist_append_designated(&normalized, source->expr,
                                           INIT_DESIGNATOR_NONE, 0, NULL);
                source = source->next;
            } else if (initializer_is_aggregate_type(field_type)) {
                int capacity = initializer_scalar_capacity(field_type);
                ExprList* nested_items = NULL;
                int consumed = 0;
                if (capacity <= 0) return;
                while (source && consumed < capacity) {
                    exprlist_append_designated(
                        &nested_items, source->expr, INIT_DESIGNATOR_NONE,
                        0, NULL);
                    source = source->next;
                    ++consumed;
                }
                Expr* nested = expr_initializer_list(nested_items,
                                                     initializer->loc);
                nested->compound_type = field_type;
                nested->type = field_type;
                normalize_brace_elided_initializer(field_type, nested);
                exprlist_append_designated(&normalized, nested,
                                           INIT_DESIGNATOR_NONE, 0, NULL);
            } else {
                exprlist_append_designated(&normalized, source->expr,
                                           INIT_DESIGNATOR_NONE, 0, NULL);
                source = source->next;
            }
            field = type->kind == TYPE_UNION ? NULL : field->next;
        }
    }
    /* Preserve excess clauses so sema reports the normal too-many diagnostic. */
    while (source) {
        exprlist_append_designated(&normalized, source->expr,
                                   INIT_DESIGNATOR_NONE, 0, NULL);
        source = source->next;
    }
    initializer->compound_init = normalized;
}

static void sema_initializer(Type* type, Expr* initializer) {
    Expr* string;
    if (!type || !initializer) return;
    normalize_brace_elided_initializer(type, initializer);
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
            if (field->type && field->type->kind == TYPE_ARRAY &&
                field->type->array_len == -1 &&
                !field->type->array_bound &&
                !field->type->array_unspecified_bound) {
                rcc_error(item->expr->loc,
                          "flexible array member cannot be initialized");
                cursor = field->next;
                ++initialized;
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

typedef struct SemaCleanupPath {
    struct SemaCleanupPath* previous;
    struct SemaCleanupPath* allocation_next;
} SemaCleanupPath;

typedef struct SemaCleanupLabel {
    const char* name;
    SemaCleanupPath* path;
    struct SemaCleanupLabel* next;
} SemaCleanupLabel;

typedef struct SemaCleanupGoto {
    Stmt* statement;
    SemaCleanupPath* path;
    struct SemaCleanupGoto* next;
} SemaCleanupGoto;

typedef struct SemaCleanupGotoContext {
    SemaCleanupPath* allocations;
    SemaCleanupLabel* labels;
    SemaCleanupGoto* gotos;
} SemaCleanupGotoContext;

static SemaCleanupLabel* sema_find_cleanup_label(
    SemaCleanupGotoContext* context, const char* name) {
    SemaCleanupLabel* label = context->labels;
    while (label && strcmp(label->name, name) != 0) label = label->next;
    return label;
}

static void sema_record_cleanup_label(SemaCleanupGotoContext* context,
                                      const char* name,
                                      SemaCleanupPath* path) {
    SemaCleanupLabel* label = sema_find_cleanup_label(context, name);
    if (label) return;
    label = rcc_alloc(sizeof(*label));
    label->name = name;
    label->path = path;
    label->next = context->labels;
    context->labels = label;
}

static void sema_collect_cleanup_gotos(Stmt* statement,
                                       SemaCleanupPath** active,
                                       SemaCleanupGotoContext* context) {
    SemaCleanupPath* marker;
    if (!statement) return;
    switch (statement->kind) {
        case STMT_BLOCK:
            marker = *active;
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                sema_collect_cleanup_gotos(item->stmt, active, context);
            }
            *active = marker;
            break;
        case STMT_IF:
            marker = *active;
            sema_collect_cleanup_gotos(statement->if_then, active, context);
            *active = marker;
            sema_collect_cleanup_gotos(statement->if_else, active, context);
            *active = marker;
            break;
        case STMT_WHILE:
        case STMT_DO:
            marker = *active;
            sema_collect_cleanup_gotos(statement->while_body, active,
                                       context);
            *active = marker;
            break;
        case STMT_FOR:
            marker = *active;
            sema_collect_cleanup_gotos(statement->for_init, active, context);
            sema_collect_cleanup_gotos(statement->for_body, active, context);
            *active = marker;
            break;
        case STMT_SWITCH:
            marker = *active;
            sema_collect_cleanup_gotos(statement->switch_body, active,
                                       context);
            *active = marker;
            break;
        case STMT_CASE:
            sema_collect_cleanup_gotos(statement->case_stmt, active, context);
            break;
        case STMT_DEFAULT:
            sema_collect_cleanup_gotos(statement->default_stmt, active,
                                       context);
            break;
        case STMT_LABEL:
            sema_record_cleanup_label(context, statement->label_name,
                                      *active);
            sema_collect_cleanup_gotos(statement->label_stmt, active,
                                       context);
            break;
        case STMT_GOTO: {
            SemaCleanupGoto* item = rcc_alloc(sizeof(*item));
            item->statement = statement;
            item->path = *active;
            item->next = context->gotos;
            context->gotos = item;
            break;
        }
        case STMT_DECL:
            if (statement->decl && statement->decl->var_cleanup) {
                SemaCleanupPath* path = rcc_alloc(sizeof(*path));
                path->previous = *active;
                path->allocation_next = context->allocations;
                context->allocations = path;
                *active = path;
            }
            break;
        default:
            break;
    }
}

static void sema_validate_cleanup_gotos(Stmt* statement) {
    SemaCleanupGotoContext context = {0};
    SemaCleanupPath* active = NULL;
    SemaCleanupGoto* item;
    sema_collect_cleanup_gotos(statement, &active, &context);
    for (item = context.gotos; item; item = item->next) {
        SemaCleanupLabel* label = sema_find_cleanup_label(
            &context, item->statement->goto_label);
        SemaCleanupPath* path = item->path;
        unsigned count = 0;
        if (!label) continue;
        while (path && path != label->path) {
            path = path->previous;
            ++count;
        }
        if (path != label->path) {
            rcc_error(item->statement->loc,
                      "goto enters a C++ scope-cleanup object lifetime");
        } else {
            item->statement->goto_cleanup_count = count;
        }
    }
    while (context.gotos) {
        SemaCleanupGoto* next = context.gotos->next;
        rcc_free(context.gotos);
        context.gotos = next;
    }
    while (context.labels) {
        SemaCleanupLabel* next = context.labels->next;
        rcc_free(context.labels);
        context.labels = next;
    }
    while (context.allocations) {
        SemaCleanupPath* next = context.allocations->allocation_next;
        rcc_free(context.allocations);
        context.allocations = next;
    }
}

typedef struct SemaVlaPath {
    struct SemaVlaPath* previous;
    struct SemaVlaPath* allocation_next;
} SemaVlaPath;

typedef struct SemaVlaLabel {
    const char* name;
    SemaVlaPath* path;
    struct SemaVlaLabel* next;
} SemaVlaLabel;

typedef struct SemaVlaGoto {
    Stmt* statement;
    SemaVlaPath* path;
    struct SemaVlaGoto* next;
} SemaVlaGoto;

typedef struct SemaVlaGotoContext {
    SemaVlaPath* allocations;
    SemaVlaLabel* labels;
    SemaVlaGoto* gotos;
} SemaVlaGotoContext;

static SemaVlaLabel* sema_find_vla_label(SemaVlaGotoContext* context,
                                         const char* name) {
    SemaVlaLabel* label = context->labels;
    while (label && strcmp(label->name, name) != 0) label = label->next;
    return label;
}

static void sema_record_vla_label(SemaVlaGotoContext* context,
                                  const char* name, SemaVlaPath* path) {
    SemaVlaLabel* label = sema_find_vla_label(context, name);
    if (label) return;
    label = rcc_alloc(sizeof(*label));
    label->name = name;
    label->path = path;
    label->next = context->labels;
    context->labels = label;
}

static void sema_collect_vla_gotos(Stmt* statement, SemaVlaPath** active,
                                   SemaVlaGotoContext* context) {
    SemaVlaPath* marker;
    if (!statement) return;
    switch (statement->kind) {
        case STMT_BLOCK:
            marker = *active;
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                sema_collect_vla_gotos(item->stmt, active, context);
            }
            *active = marker;
            break;
        case STMT_IF:
            marker = *active;
            sema_collect_vla_gotos(statement->if_then, active, context);
            *active = marker;
            sema_collect_vla_gotos(statement->if_else, active, context);
            *active = marker;
            break;
        case STMT_WHILE:
        case STMT_DO:
            marker = *active;
            sema_collect_vla_gotos(statement->while_body, active, context);
            *active = marker;
            break;
        case STMT_FOR:
            marker = *active;
            sema_collect_vla_gotos(statement->for_init, active, context);
            sema_collect_vla_gotos(statement->for_body, active, context);
            *active = marker;
            break;
        case STMT_SWITCH:
            marker = *active;
            sema_collect_vla_gotos(statement->switch_body, active, context);
            *active = marker;
            break;
        case STMT_CASE:
            sema_collect_vla_gotos(statement->case_stmt, active, context);
            break;
        case STMT_DEFAULT:
            sema_collect_vla_gotos(statement->default_stmt, active, context);
            break;
        case STMT_LABEL:
            sema_record_vla_label(context, statement->label_name, *active);
            sema_collect_vla_gotos(statement->label_stmt, active, context);
            break;
        case STMT_GOTO: {
            SemaVlaGoto* item = rcc_alloc(sizeof(*item));
            item->statement = statement;
            item->path = *active;
            item->next = context->gotos;
            context->gotos = item;
            break;
        }
        case STMT_DECL:
            if (statement->decl && statement->decl->kind == DECL_VAR &&
                statement->decl->var_is_vla) {
                SemaVlaPath* path = rcc_alloc(sizeof(*path));
                path->previous = *active;
                path->allocation_next = context->allocations;
                context->allocations = path;
                *active = path;
            }
            break;
        default:
            break;
    }
}

static void sema_validate_vla_gotos(Stmt* statement) {
    SemaVlaGotoContext context = {0};
    SemaVlaPath* active = NULL;
    SemaVlaGoto* item;
    sema_collect_vla_gotos(statement, &active, &context);
    for (item = context.gotos; item; item = item->next) {
        SemaVlaLabel* label = sema_find_vla_label(
            &context, item->statement->goto_label);
        SemaVlaPath* path = item->path;
        unsigned count = 0;
        if (!label) continue;
        while (path && path != label->path) {
            path = path->previous;
            ++count;
        }
        if (path != label->path) {
            rcc_error(item->statement->loc,
                      "goto enters a variable-length array scope");
        } else {
            item->statement->goto_vla_count = count;
        }
    }
    while (context.gotos) {
        SemaVlaGoto* next = context.gotos->next;
        rcc_free(context.gotos);
        context.gotos = next;
    }
    while (context.labels) {
        SemaVlaLabel* next = context.labels->next;
        rcc_free(context.labels);
        context.labels = next;
    }
    while (context.allocations) {
        SemaVlaPath* next = context.allocations->allocation_next;
        rcc_free(context.allocations);
        context.allocations = next;
    }
}

static int sema_cxx_field_count(Type* type) {
    int count = 0;
    for (TypeField* field = type ? type->fields : NULL; field;
         field = field->next) {
        ++count;
    }
    return count;
}

static int sema_cxx_field_index(Type* type, const char* name) {
    int index = 0;
    if (!type || !name) return -1;
    for (TypeField* field = type->fields; field; field = field->next, ++index) {
        if (field->name && strcmp(field->name, name) == 0) return index;
    }
    return -1;
}

/* Turn C++ default member initializers into ordinary designated aggregate
 * clauses.  This keeps one well-tested initialization path for local,
 * static, and TLS objects: storage is zeroed first, explicit initializers
 * retain their source order, and omitted members receive their declared
 * defaults.  A non-aggregate initializer is deliberately left alone because
 * it denotes copy/constructor initialization rather than member defaults. */
static Expr* sema_cxx_default_member_initializer(Decl* declaration) {
    Type* type = declaration ? declaration->type : NULL;
    CxxClass* cls = type ? type->cxx_class : NULL;
    Expr* source = declaration ? declaration->var_init : NULL;
    ExprList* items = NULL;
    unsigned char* initialized = NULL;
    int field_count;
    int cursor = 0;
    int index;

    if (!declaration || !type || !cls || !cls->has_field_initializer ||
        !type->fields || (source && source->kind != EXPR_COMPOUND)) {
        return NULL;
    }
    if (cls->has_user_constructor) {
        rcc_error(declaration->loc,
                  "default member initializers with a user constructor require constructor lowering");
        return NULL;
    }

    field_count = sema_cxx_field_count(type);
    if (field_count <= 0) return NULL;
    initialized = rcc_alloc((size_t)field_count);

    /* `{}` is represented as a value-init `{0}` node.  It is not an explicit
     * initializer for the first member in C++, so defaults replace it. */
    if (source && !source->compound_value_init) {
        for (ExprList* item = source->compound_init; item;
             item = item->next) {
            if (item->designator_kind == INIT_DESIGNATOR_FIELD) {
                index = sema_cxx_field_index(type, item->designator_field);
                if (index < 0) {
                    rcc_free(initialized);
                    return NULL;
                }
                cursor = index + 1;
            } else if (item->designator_kind == INIT_DESIGNATOR_NONE) {
                index = cursor++;
                if (index < 0 || index >= field_count) {
                    rcc_free(initialized);
                    return NULL;
                }
            } else {
                /* An array designator is invalid for a class aggregate; let
                 * the normal initializer diagnostic report it unchanged. */
                rcc_free(initialized);
                return NULL;
            }
            if (initialized[index]) {
                rcc_free(initialized);
                return NULL;
            }
            initialized[index] = 1u;
            {
                TypeField* field = type->fields;
                for (int field_index = 0; field && field_index < index;
                     field = field->next, ++field_index) {
                }
                if (!field || !field->name) {
                    rcc_free(initialized);
                    return NULL;
                }
                exprlist_append_designated(&items, item->expr,
                                           INIT_DESIGNATOR_FIELD, 0,
                                           field->name);
            }
        }
    }

    index = 0;
    for (TypeField* field = type->fields; field; field = field->next, ++index) {
        if (!initialized[index] && field->initializer) {
            exprlist_append_designated(&items, field->initializer,
                                       INIT_DESIGNATOR_FIELD, 0,
                                       field->name);
        }
    }
    rcc_free(initialized);
    if (!items) return NULL;

    {
        SourceLoc loc = source ? source->loc : declaration->loc;
        Expr* result = expr_initializer_list(items, loc);
        result->compound_type = type;
        return result;
    }
}

static void sema_decl(Decl* decl) {
    if (!decl) return;

    switch (decl->kind) {
        case DECL_VAR: {
            bool is_global = g_symtab->current == g_symtab->global;
            CxxNamespace* saved_cxx_namespace = current_cxx_namespace;
            if (is_global && rcc_parser_is_cxx_mode()) {
                current_cxx_namespace = sema_decl_namespace(decl);
            }
            if (decl->var_is_auto) {
                decl->type = sema_deduce_auto_type(decl);
            }
            {
                Expr* default_initializer =
                    sema_cxx_default_member_initializer(decl);
                if (default_initializer) decl->var_init = default_initializer;
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
            sema_validate_array_parameter_type(decl->type, decl->loc, false);
            if (sema_type_is_variably_modified(decl->type)) {
                sema_vla_bounds(decl->type, decl->loc);
                if (is_global) {
                    rcc_error(decl->loc,
                              "variable-length array is only valid at block scope");
                }
                if (decl->storage == STORAGE_STATIC ||
                    decl->storage == STORAGE_EXTERN) {
                    rcc_error(decl->loc,
                              "variably modified object cannot have linkage");
                }
                if (sema_type_has_vla(decl->type) && decl->var_init) {
                    rcc_error(decl->loc,
                              "variable-length array cannot have an initializer");
                }
                decl->var_is_vla = !is_global && sema_type_has_vla(decl->type);
            }
            sema_infer_initializer_type(decl->type, decl->var_init);
            if (decl->type && decl->type->kind == TYPE_ARRAY &&
                       decl->type->array_len == -1 &&
                       !decl->type->array_bound &&
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
            if (decl->var_is_vla) {
                int word_size = g_opts.target_arch == ARCH_X64 ? 8 : 4;
                decl->var_vla_size_offset = decl->var_offset + word_size;
                decl->var_vla_extent_offset = decl->var_offset +
                    2 * word_size;
                decl->var_vla_extent_count =
                    sema_vla_dimension_count(decl->type);
            }

            if (decl->var_init) {
                sema_initializer(decl->type, decl->var_init);
            }
            sema_prepare_variable_cleanup(decl, is_global);
            current_cxx_namespace = saved_cxx_namespace;
            break;
        }

        case DECL_FUNC: {
            CxxNamespace* saved_cxx_namespace = current_cxx_namespace;
            if (rcc_parser_is_cxx_mode()) {
                current_cxx_namespace = sema_decl_namespace(decl);
            }
            Symbol* sym = symtab_lookup(g_symtab, decl->name);
            bool cxx_overload_set = false;
            bool cxx_defaults_merged = false;
            Type* previous_method_owner = current_cxx_method_owner;
            Decl* previous_this_param = current_cxx_this_param;
            sema_analyze_cxx_default_arguments(decl);
            if (sym && sym->kind == SYM_FUNC &&
                decl->func_has_cxx_linkage) {
                Decl** slot = &sym->decl;
                while (*slot) {
                    Decl* prior = *slot;
                    bool distinct_template_instances =
                        prior->func_is_template_instance &&
                        decl->func_is_template_instance &&
                        prior->link_name && decl->link_name &&
                        strcmp(prior->link_name, decl->link_name) != 0;
                    if (cxx_same_function_parameters(prior->type,
                                                     decl->type) &&
                        !distinct_template_instances) {
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
                        sema_merge_cxx_default_arguments(prior, decl);
                        sema_validate_cxx_default_suffix(decl);
                        cxx_defaults_merged = true;
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
                /* C language linkage suppresses overloading, but a function
                 * declared from a C++ translation unit still owns and
                 * accumulates default arguments in the surrounding scope. */
                if (sym->decl) {
                    sema_merge_cxx_default_arguments(sym->decl, decl);
                    sema_validate_cxx_default_suffix(decl);
                    cxx_defaults_merged = true;
                }
                /* Check for redefinition */
                if (sym->is_defined && decl->func_body) {
                    rcc_error(decl->loc, "redefinition of function '%s'", decl->name);
                }
            } else {
                sym = symtab_define(g_symtab, decl->name, SYM_FUNC, decl->type, decl->loc);
            }
            if (!cxx_overload_set) sym->decl = decl;
            if (!cxx_defaults_merged) {
                sema_validate_cxx_default_suffix(decl);
            }

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
                if (decl->func_this_param) {
                    Symbol* this_symbol = symtab_define(
                        g_symtab, decl->func_this_param->name, SYM_PARAM,
                        decl->func_this_param->type,
                        decl->func_this_param->loc);
                    this_symbol->decl = decl->func_this_param;
                    this_symbol->offset = param_offset;
                    decl->func_this_param->var_offset = param_offset;
                    param_offset += decl->func_this_param->type &&
                        decl->func_this_param->type->size > 4
                        ? decl->func_this_param->type->size : 4;
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
                for (DeclList* p = decl->func_params; p; p = p->next) {
                    if (p->decl && p->decl->param_array_type) {
                        sema_validate_array_parameter_type(
                            p->decl->param_array_type, p->decl->loc, true);
                        sema_vla_bounds(p->decl->param_array_type,
                                        p->decl->loc);
                    }
                }

                /* Analyze body */
                current_cxx_method_owner = decl->func_method_owner;
                current_cxx_this_param = decl->func_this_param;
                loop_depth = 0;
                current_switch = NULL;
                sema_stmt(decl->func_body);
                sema_validate_cleanup_gotos(decl->func_body);
                sema_validate_vla_gotos(decl->func_body);

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
                current_cxx_method_owner = previous_method_owner;
                current_cxx_this_param = previous_this_param;
            }
            current_cxx_namespace = saved_cxx_namespace;
            break;
        }

        case DECL_PARAM:
            /* Handled in DECL_FUNC */
            break;

        case DECL_TYPEDEF: {
            sema_validate_array_parameter_type(decl->typedef_type,
                                               decl->loc, false);
            if (sema_type_is_variably_modified(decl->typedef_type)) {
                sema_vla_bounds(decl->typedef_type, decl->loc);
                if (g_symtab->current == g_symtab->global) {
                    rcc_error(decl->loc,
                              "variably modified typedef is only valid at block scope");
                }
            }
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
    current_cxx_namespace = NULL;

    /* Process all top-level declarations */
    for (DeclList* d = ast->decls; d; d = d->next) {
        sema_decl(d->decl);
    }

    valid = g_error_count == 0;
    symtab_free(g_symtab);
    g_symtab = NULL;
    current_cxx_namespace = NULL;
    return valid;
}
