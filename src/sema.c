/*
 * RCC - RinOS C Compiler
 * Semantic Analysis
 */

#include "rcc.h"
#include "ast.h"
#include "ast_cxx.h"
#include "cxx_exception_type.h"
#include "symtab.h"
#include <float.h>
#include <limits.h>
#include <math.h>

/* The C compiler intentionally omits the C++ AST object.  Keep the namespace
 * lookup extension optional at this boundary so the C frontend remains a
 * standalone executable while rcc++ supplies the real implementation. */
#if defined(__GNUC__) || defined(__clang__)
extern CxxNamespace* cxx_namespace_global(void) __attribute__((weak));
extern CxxNamespace* cxx_namespace_find(
    CxxNamespace*, const char*) __attribute__((weak));
extern CxxNamespace* cxx_namespace_for_decl_name(
    CxxNamespace*, const char*) __attribute__((weak));
extern const char* cxx_namespace_qualified_name(
    CxxNamespace*) __attribute__((weak));
#endif

static CxxNamespace* sema_cxx_global_namespace(void) {
#if defined(__GNUC__) || defined(__clang__)
    return cxx_namespace_global ? cxx_namespace_global() : NULL;
#else
    return NULL;
#endif
}

/* Current function return type */
static Type* current_func_ret = NULL;
static bool current_func_variadic = false;
static bool current_func_auto_return_pending = false;
static Decl* current_func_last_param = NULL;
static unsigned static_local_counter = 0u;
static unsigned cxx_exception_frame_counter = 0u;
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
static bool sema_exception_body_has_cleanup(const Stmt* stmt);
static bool sema_exception_body_has_unregistered_cleanup(const Stmt* stmt);
static bool sema_exception_body_has_vla(const Stmt* stmt);
static bool sema_exception_body_has_call(const Stmt* stmt);
static Type* sema_expr(Expr* expr);
static void sema_decl(Decl* decl);
static void sema_initializer(Type* type, Expr* initializer);

static void sema_validate_static_integer_expression(Expr* expression);
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
    if (!cls) return false;

    /* A virtual base is owned by the most-derived object.  Its offset cannot
     * be composed from the intermediate base subobjects: a diamond must use
     * the one shared virtual-base entry recorded by the layout pass. */
    for (int virtual_index = 0;
         virtual_index < cls->virtual_base_count; ++virtual_index) {
        CxxVirtualBaseInfo* virtual_base = &cls->virtual_bases[virtual_index];
        Type* virtual_type = virtual_base->base
            ? virtual_base->base->type : NULL;
        int nested_adjustment;
        if (!virtual_base->public_path || virtual_base->offset < 0 ||
            !virtual_type) {
            continue;
        }
        if (sema_cxx_public_base(virtual_type, target,
                                 &nested_adjustment, depth + 1)) {
            if (adjustment) {
                *adjustment = virtual_base->offset + nested_adjustment;
            }
            return true;
        }
    }

    if (!cls->base_offsets) return false;
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

static void sema_cxx_add_exception_tag(CxxCatch* handler, uint64_t tag,
                                       int adjustment) {
    if (!handler || tag == 0u) return;
    for (size_t index = 0u; index < handler->compatible_tag_count; ++index) {
        if (handler->compatible_tags[index] == tag) return;
    }
    handler->compatible_tags = ast_arena_grow(
        handler->compatible_tags,
        sizeof(*handler->compatible_tags) * handler->compatible_tag_count,
            sizeof(*handler->compatible_tags) *
            (handler->compatible_tag_count + 1u));
    handler->compatible_tag_offsets = ast_arena_grow(
        handler->compatible_tag_offsets,
        sizeof(*handler->compatible_tag_offsets) *
            handler->compatible_tag_count,
        sizeof(*handler->compatible_tag_offsets) *
            (handler->compatible_tag_count + 1u));
    handler->compatible_tags[handler->compatible_tag_count++] = tag;
    handler->compatible_tag_offsets[handler->compatible_tag_count - 1u] =
        (int32_t)adjustment;
}

static Type* sema_cxx_exception_match_type(Type* type) {
    return (Type*)rcc_cxx_exception_match_type(type);
}

static bool sema_cxx_exception_reference_type(const Type* type) {
    return type && type->kind == TYPE_PTR && type->is_reference &&
           type->base != NULL;
}

static void sema_cxx_collect_exception_tags(CxxNamespace* ns, Type* target,
                                             CxxCatch* handler, unsigned depth) {
    if (!ns || !target || !handler || depth > 32u) return;
    for (int index = 0; index < ns->class_count; ++index) {
        CxxClass* candidate = ns->classes[index];
        int adjustment;
        if (!candidate || !candidate->type || candidate->type == target ||
            !sema_cxx_public_base(candidate->type, target, &adjustment, 0)) {
            continue;
        }
        sema_cxx_add_exception_tag(
            handler, rcc_cxx_exception_type_tag(candidate->type), adjustment);
    }
    for (CxxNamespace* child = ns->children; child; child = child->next) {
        sema_cxx_collect_exception_tags(child, target, handler, depth + 1u);
    }
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
    type_parameter->is_bitfield = false;
    type_parameter->bit_width = 0u;
    type_parameter->is_static = false;
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
                                    : sema_cxx_global_namespace();
         ns; ns = ns->parent) {
        symbol = sema_cxx_lookup_namespace(ns, name, visited, 0);
        if (symbol) return symbol;
    }
    return NULL;
}

static CxxNamespace* sema_decl_namespace(Decl* decl) {
    CxxNamespace* global_namespace = sema_cxx_global_namespace();
    if (!global_namespace || !decl) return global_namespace;
    if (decl->kind == DECL_FUNC && decl->func_method_owner &&
        decl->func_method_owner->cxx_namespace) {
        CxxNamespace* owner_namespace = cxx_namespace_find(
            global_namespace, decl->func_method_owner->cxx_namespace);
        if (owner_namespace) return owner_namespace;
    }
    return cxx_namespace_for_decl_name(global_namespace, decl->name);
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
    if (e && e->kind == EXPR_CAST && e->type && e->type->is_reference &&
        (e->cxx_cast_kind == CXX_CAST_NONE ||
         e->cxx_cast_kind == CXX_CAST_CONST)) {
        return is_lvalue(e->cast_expr);
    }
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

static bool sema_is_scoped_enum(Type* type) {
    return type && type->kind == TYPE_ENUM && type->enum_is_scoped;
}

static bool sema_is_cxx_nullptr_expr(const Expr* expression) {
    return expression && (expression->is_cxx_nullptr ||
        (expression->type && expression->type->kind == TYPE_NULLPTR));
}

static Type* sema_integer_promotion(Type* type) {
    if (sema_is_scoped_enum(type)) return type;
    if (!type || type->kind == TYPE_ENUM || type->kind < TYPE_INT) {
        return type_int;
    }
    return type;
}

/* Find an ordinary public conversion function whose result is exactly the
 * requested target type.  A user-defined conversion cannot be chained with a
 * second user-defined conversion, so the exact result type is intentional.
 * The caller supplies the ambiguity result because overload ranking and the
 * final cast need to make the same decision without silently picking one. */
static TypeMethod* sema_find_cxx_conversion_method(Type* aggregate,
                                                   Type* target,
                                                   bool* ambiguous) {
    TypeMethod* method;
    TypeMethod* result = NULL;
    if (ambiguous) *ambiguous = false;
    if (!aggregate || !target ||
        (aggregate->kind != TYPE_STRUCT && aggregate->kind != TYPE_UNION)) {
        return NULL;
    }
    for (method = aggregate->methods; method; method = method->next) {
        if (method->kind != TYPE_METHOD_FUNCTION ||
            !method->function_decl || !method->return_type ||
            !method->name || strcmp(method->name, "operator conversion") != 0 ||
            method->cxx_access != ACCESS_PUBLIC || method->is_explicit ||
            method->function_decl->func_params ||
            !type_is_compatible(method->return_type, target)) {
            continue;
        }
        if (result) {
            if (ambiguous) *ambiguous = true;
            return NULL;
        }
        result = method;
    }
    return result;
}

static bool cxx_reference_object_compatible(const Type* source,
                                            const Type* target) {
    Type source_unqualified;
    Type target_unqualified;
    if (!source || !target) return false;
    /* Adding top-level cv is permitted when binding an lvalue reference.  Do
     * not recurse while removing qualifiers: pointee cv is part of the
     * pointed-to object type and must still be checked by the normal type
     * compatibility predicate. */
    source_unqualified = *source;
    target_unqualified = *target;
    source_unqualified.is_const = false;
    source_unqualified.is_volatile = false;
    target_unqualified.is_const = false;
    target_unqualified.is_volatile = false;
    return type_is_compatible(&source_unqualified, &target_unqualified);
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
        Type* source = e->type;
        /* Reference arguments are passed as addresses by the backend, so the
         * supported subset deliberately requires addressable expressions. */
        if (!referred || !is_lvalue(e)) return NULL;
        if (source && source->is_reference) source = source->base;
        if (!source) return NULL;
        if ((source->is_const && !referred->is_const) ||
            (source->is_volatile && !referred->is_volatile)) {
            return NULL;
        }
        return cxx_reference_object_compatible(source, referred)
            ? target : NULL;
    }

    /* Same type */
    if (e->type == target) return target;

    /* A scoped enum is a distinct C++ type.  It deliberately does not
     * participate in the C integer-enum conversions; accepting those here
     * would make `enum class` silently behave like an unscoped C enum. */
    if (sema_is_scoped_enum(e->type) || sema_is_scoped_enum(target)) {
        return type_is_compatible(e->type, target) ? target : NULL;
    }

    if ((target->kind == TYPE_STRUCT || target->kind == TYPE_UNION) &&
        type_is_compatible(e->type, target)) {
        return target;
    }

    /* Lower a public implicit conversion operator as a real member call.  It
     * is important that this goes through sema_expr() rather than assigning a
     * result type: the normal call path supplies the object argument, checks
     * the method ABI, and emits the generated conversion function. */
    if (rcc_parser_is_cxx_mode() &&
        (e->type->kind == TYPE_STRUCT || e->type->kind == TYPE_UNION)) {
        bool ambiguous = false;
        TypeMethod* conversion = sema_find_cxx_conversion_method(
            e->type, target, &ambiguous);
        if (ambiguous) return NULL;
        if (conversion) {
            Expr* source = ast_arena_alloc(sizeof(*source));
            Expr* member;
            *source = *e;
            member = expr_member(source, conversion->name, e->loc);
            Expr* call = expr_call(member, NULL, e->loc);
            *e = *call;
            sema_expr(e);
            return e->type && type_is_compatible(e->type, target)
                ? target : NULL;
        }
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
            if (sema_cxx_pointer_conversion(e->type, target, &adjustment)) {
                e->cxx_pointer_adjustment_valid = adjustment != 0;
                e->cxx_pointer_adjustment = adjustment;
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

/* Static-storage integer initializers must be integer constant expressions.
 * Keep this validation separate from the runtime-initializer extension used
 * for otherwise non-constant globals: an expression such as 1 / 0 is not a
 * runtime initializer and must never be lowered into an executing divide. */
static void sema_validate_static_integer_expression(Expr* expression) {
    int64_t value;
    if (!expression) return;
    switch (expression->kind) {
        case EXPR_COMPOUND:
            for (ExprList* item = expression->compound_init; item;
                 item = item->next) {
                sema_validate_static_integer_expression(item->expr);
            }
            return;
        case EXPR_DIV:
        case EXPR_MOD:
            if (expression->binary_rhs &&
                expr_eval_integer_constant(expression->binary_rhs, &value) &&
                value == 0) {
                rcc_error(expression->binary_rhs->loc,
                          "static integer initializer has a zero divisor");
            }
            sema_validate_static_integer_expression(expression->binary_lhs);
            sema_validate_static_integer_expression(expression->binary_rhs);
            return;
        case EXPR_COND:
            if (expression->cond_test &&
                expr_eval_integer_constant(expression->cond_test, &value)) {
                sema_validate_static_integer_expression(expression->cond_test);
                sema_validate_static_integer_expression(
                    value ? expression->cond_then : expression->cond_else);
            } else {
                sema_validate_static_integer_expression(expression->cond_test);
                sema_validate_static_integer_expression(expression->cond_then);
                sema_validate_static_integer_expression(expression->cond_else);
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
        case EXPR_SIZEOF:
        case EXPR_ALIGNOF:
            sema_validate_static_integer_expression(expression->unary_operand);
            return;
        case EXPR_CAST:
            sema_validate_static_integer_expression(expression->cast_expr);
            return;
        case EXPR_ADD:
        case EXPR_SUB:
        case EXPR_MUL:
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
            sema_validate_static_integer_expression(expression->binary_lhs);
            sema_validate_static_integer_expression(expression->binary_rhs);
            return;
        default:
            return;
    }
}

static bool sema_exception_body_has_cleanup(const Stmt* statement) {
    if (!statement) return false;
    switch (statement->kind) {
        case STMT_BLOCK:
            for (const StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                if (sema_exception_body_has_cleanup(item->stmt)) return true;
            }
            return false;
        case STMT_DECL:
            return statement->decl && statement->decl->kind == DECL_VAR &&
                (statement->decl->var_cleanup ||
                 statement->decl->var_cleanups ||
                 statement->decl->var_is_vla);
        case STMT_IF:
            return sema_exception_body_has_cleanup(statement->if_then) ||
                sema_exception_body_has_cleanup(statement->if_else);
        case STMT_WHILE:
        case STMT_DO:
            return sema_exception_body_has_cleanup(statement->while_body);
        case STMT_FOR:
            return sema_exception_body_has_cleanup(statement->for_init) ||
                sema_exception_body_has_cleanup(statement->for_body);
        case STMT_SWITCH:
            return sema_exception_body_has_cleanup(statement->switch_body);
        case STMT_CASE:
            return sema_exception_body_has_cleanup(statement->case_stmt);
        case STMT_DEFAULT:
            return sema_exception_body_has_cleanup(statement->default_stmt);
        case STMT_LABEL:
            return sema_exception_body_has_cleanup(statement->label_stmt);
        case STMT_TRY:
            if (sema_exception_body_has_cleanup(statement->try_body)) {
                return true;
            }
            for (const CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                if (sema_exception_body_has_cleanup(handler->body)) {
                    return true;
                }
            }
            return false;
        default:
            return false;
    }
}

/* A direct, non-virtual C++ destructor has a stable callback ABI and can be
 * registered in the runtime exception frame.  Scope-cleanup wrappers and
 * other synthesized calls still depend on compiler-side state, so allowing a
 * call to cross such a protected scope would make the cleanup unreachable.
 */
static bool sema_exception_body_has_unregistered_cleanup(
    const Stmt* statement) {
    if (!statement) return false;
    switch (statement->kind) {
        case STMT_BLOCK:
            for (const StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                if (sema_exception_body_has_unregistered_cleanup(item->stmt)) {
                    return true;
                }
            }
            return false;
        case STMT_DECL: {
            Decl* declaration = statement->decl;
            Expr* cleanup = declaration && declaration->kind == DECL_VAR
                ? declaration->var_cleanup : NULL;
            Expr* function = cleanup && cleanup->kind == EXPR_CALL
                ? cleanup->call_func : NULL;
            Decl* destructor = function && function->kind == EXPR_IDENT
                ? function->ident_decl : NULL;
            if (!declaration || declaration->kind != DECL_VAR) return false;
            if (declaration->var_is_vla) return true;
            if (cleanup && (!destructor || !destructor->func_is_cxx_destructor)) {
                return true;
            }
            for (ExprList* item = declaration->var_cleanups; item;
                 item = item->next) {
                Expr* expression = item->expr;
                Expr* function = expression && expression->kind == EXPR_CALL
                    ? expression->call_func : NULL;
                Decl* member_destructor = function &&
                    function->kind == EXPR_IDENT ? function->ident_decl : NULL;
                if (!member_destructor ||
                    !member_destructor->func_is_cxx_destructor) {
                    return true;
                }
            }
            return false;
        }
        case STMT_IF:
            return sema_exception_body_has_unregistered_cleanup(
                       statement->if_then) ||
                sema_exception_body_has_unregistered_cleanup(statement->if_else);
        case STMT_WHILE:
        case STMT_DO:
            return sema_exception_body_has_unregistered_cleanup(
                statement->while_body);
        case STMT_FOR:
            return sema_exception_body_has_unregistered_cleanup(
                       statement->for_init) ||
                sema_exception_body_has_unregistered_cleanup(statement->for_body);
        case STMT_SWITCH:
            return sema_exception_body_has_unregistered_cleanup(
                statement->switch_body);
        case STMT_CASE:
            return sema_exception_body_has_unregistered_cleanup(
                statement->case_stmt);
        case STMT_DEFAULT:
            return sema_exception_body_has_unregistered_cleanup(
                statement->default_stmt);
        case STMT_LABEL:
            return sema_exception_body_has_unregistered_cleanup(
                statement->label_stmt);
        case STMT_TRY:
            if (sema_exception_body_has_unregistered_cleanup(
                    statement->try_body)) {
                return true;
            }
            for (const CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                if (sema_exception_body_has_unregistered_cleanup(handler->body)) {
                    return true;
                }
            }
            return false;
        default:
            return false;
    }
}

static bool sema_exception_body_has_vla(const Stmt* statement) {
    if (!statement) return false;
    switch (statement->kind) {
        case STMT_BLOCK:
            for (const StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                if (sema_exception_body_has_vla(item->stmt)) return true;
            }
            return false;
        case STMT_DECL:
            return statement->decl && statement->decl->kind == DECL_VAR &&
                statement->decl->var_is_vla;
        case STMT_IF:
            return sema_exception_body_has_vla(statement->if_then) ||
                sema_exception_body_has_vla(statement->if_else);
        case STMT_WHILE:
        case STMT_DO:
            return sema_exception_body_has_vla(statement->while_body);
        case STMT_FOR:
            return sema_exception_body_has_vla(statement->for_init) ||
                sema_exception_body_has_vla(statement->for_body);
        case STMT_SWITCH:
            return sema_exception_body_has_vla(statement->switch_body);
        case STMT_CASE:
            return sema_exception_body_has_vla(statement->case_stmt);
        case STMT_DEFAULT:
            return sema_exception_body_has_vla(statement->default_stmt);
        case STMT_LABEL:
            return sema_exception_body_has_vla(statement->label_stmt);
        case STMT_TRY:
            if (sema_exception_body_has_vla(statement->try_body)) return true;
            for (const CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                if (sema_exception_body_has_vla(handler->body)) return true;
            }
            return false;
        default:
            return false;
    }
}

static bool sema_exception_expression_has_call(const Expr* expression) {
    const ExprList* item;
    if (!expression) return false;
    if (expression->kind == EXPR_CALL) return true;
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
        case EXPR_CAST:
            return sema_exception_expression_has_call(
                expression->unary_operand);
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
            return sema_exception_expression_has_call(
                       expression->binary_lhs) ||
                sema_exception_expression_has_call(expression->binary_rhs);
        case EXPR_COND:
            return sema_exception_expression_has_call(expression->cond_test) ||
                sema_exception_expression_has_call(expression->cond_then) ||
                sema_exception_expression_has_call(expression->cond_else);
        case EXPR_INDEX:
            return sema_exception_expression_has_call(expression->index_base) ||
                sema_exception_expression_has_call(expression->index_expr);
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            return sema_exception_expression_has_call(expression->member_base);
        case EXPR_COMPOUND:
            for (item = expression->compound_init; item; item = item->next) {
                if (sema_exception_expression_has_call(item->expr)) return true;
            }
            return false;
        case EXPR_GENERIC:
            if (sema_exception_expression_has_call(
                    expression->generic_control)) {
                return true;
            }
            for (GenericAssociation* association =
                     expression->generic_associations;
                 association; association = association->next) {
                if (sema_exception_expression_has_call(association->expr)) {
                    return true;
                }
            }
            return false;
        case EXPR_VA_START:
        case EXPR_VA_END:
        case EXPR_VA_COPY:
        case EXPR_VA_ARG:
            return sema_exception_expression_has_call(
                       expression->va_list_operand) ||
                sema_exception_expression_has_call(
                    expression->va_second_operand);
        default:
            return false;
    }
}

static bool sema_exception_body_has_call(const Stmt* statement) {
    if (!statement) return false;
    switch (statement->kind) {
        case STMT_EXPR:
            return sema_exception_expression_has_call(statement->expr);
        case STMT_BLOCK:
            for (const StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                if (sema_exception_body_has_call(item->stmt)) return true;
            }
            return false;
        case STMT_IF:
            return sema_exception_expression_has_call(statement->if_cond) ||
                sema_exception_body_has_call(statement->if_then) ||
                sema_exception_body_has_call(statement->if_else);
        case STMT_WHILE:
        case STMT_DO:
            return sema_exception_expression_has_call(statement->while_cond) ||
                sema_exception_body_has_call(statement->while_body);
        case STMT_FOR:
            return sema_exception_body_has_call(statement->for_init) ||
                sema_exception_expression_has_call(statement->for_cond) ||
                sema_exception_expression_has_call(statement->for_inc) ||
                sema_exception_body_has_call(statement->for_body);
        case STMT_SWITCH:
            return sema_exception_expression_has_call(statement->switch_expr) ||
                sema_exception_body_has_call(statement->switch_body);
        case STMT_CASE:
            return sema_exception_expression_has_call(statement->case_val) ||
                sema_exception_body_has_call(statement->case_stmt);
        case STMT_DEFAULT:
            return sema_exception_body_has_call(statement->default_stmt);
        case STMT_LABEL:
            return sema_exception_body_has_call(statement->label_stmt);
        case STMT_RETURN:
            return sema_exception_expression_has_call(statement->return_val);
        case STMT_DECL:
            /* Do not inspect the synthesized destructor expression: it is the
             * cleanup being protected by the exception lowering. */
            return statement->decl &&
                sema_exception_expression_has_call(
                    statement->decl->var_init);
        case STMT_TRY:
            if (sema_exception_body_has_call(statement->try_body)) return true;
            for (const CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                if (sema_exception_body_has_call(handler->body)) return true;
            }
            return false;
        case STMT_THROW:
            return sema_exception_expression_has_call(statement->throw_expr);
        default:
            return false;
    }
}

/* A constexpr binding is deliberately local to one evaluation.  It avoids
 * using the compiler's semantic symbol table as an evaluator environment and
 * therefore keeps constant folding independent from stack offsets and target
 * ABI details. */
typedef struct SemaConstexprBinding {
    Decl* declaration;
    Type* type;
    int64_t value;
    double floating_value;
    bool is_floating;
    bool is_pointer;
    Decl* pointer_declaration;
    int64_t pointer_offset;
    /* Fixed-size aggregate locals are evaluated in an isolated byte buffer.
     * The buffer follows the target layout already computed by the parser, so
     * member/index lvalues can be read and written without inventing a second
     * object-layout model for constant evaluation. */
    unsigned char* object_bytes;
    size_t object_size;
    bool is_object;
} SemaConstexprBinding;

static int constexpr_eval_depth;

static bool sema_constexpr_integer_type(Type* type) {
    return type && (type_is_integer(type) || type->kind == TYPE_ENUM);
}

static bool sema_constexpr_convert(int64_t input, Type* type,
                                   int64_t* output) {
    unsigned bits;
    uint64_t mask;
    uint64_t converted;

    if (!output || !sema_constexpr_integer_type(type) || type->size <= 0) {
        return false;
    }
    if (type->kind == TYPE_BOOL) {
        *output = input != 0;
        return true;
    }
    bits = (unsigned)type->size * 8u;
    if (bits == 0u || bits > 64u) return false;
    converted = (uint64_t)input;
    if (bits < 64u) {
        mask = (UINT64_C(1) << bits) - 1u;
        converted &= mask;
        if (!type->is_unsigned &&
            (converted & (UINT64_C(1) << (bits - 1u)))) {
            converted |= ~mask;
        }
    }
    *output = (int64_t)converted;
    return true;
}

static bool sema_constexpr_add(int64_t left, int64_t right, int64_t* result) {
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_add_overflow(left, right, result);
#else
    if ((right > 0 && left > INT64_MAX - right) ||
        (right < 0 && left < INT64_MIN - right)) return false;
    *result = left + right;
    return true;
#endif
}

static bool sema_constexpr_sub(int64_t left, int64_t right, int64_t* result) {
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_sub_overflow(left, right, result);
#else
    if ((right < 0 && left > INT64_MAX + right) ||
        (right > 0 && left < INT64_MIN + right)) return false;
    *result = left - right;
    return true;
#endif
}

static bool sema_constexpr_mul(int64_t left, int64_t right, int64_t* result) {
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_mul_overflow(left, right, result);
#else
    if (left != 0 && right != 0 &&
        ((left == INT64_MIN && right != 1) ||
         (right == INT64_MIN && left != 1) ||
         left > INT64_MAX / right || left < INT64_MIN / right)) {
        return false;
    }
    *result = left * right;
    return true;
#endif
}

static int sema_constexpr_binding_index(
    Expr* expression, SemaConstexprBinding* bindings, int binding_count) {
    if (!expression || expression->kind != EXPR_IDENT || !bindings) return -1;
    for (int index = 0; index < binding_count; ++index) {
        if ((expression->ident_decl &&
             expression->ident_decl == bindings[index].declaration) ||
            (!expression->ident_decl && expression->ident_name &&
             bindings[index].declaration &&
             bindings[index].declaration->name &&
             strcmp(expression->ident_name,
                    bindings[index].declaration->name) == 0)) {
            return index;
        }
    }
    return -1;
}

static bool sema_eval_constexpr_expr(
    Expr* expression, SemaConstexprBinding* bindings,
    int binding_count, int64_t* value) {
    int64_t left;
    int64_t right;
    int64_t condition;
    Type* measured;

    if (!expression || !value) return false;
    switch (expression->kind) {
        case EXPR_INT_LIT:
            *value = expression->int_val;
            return true;
        case EXPR_CHAR_LIT:
            *value = (unsigned char)expression->char_val;
            return true;
        case EXPR_IDENT:
            for (int index = 0; index < binding_count; ++index) {
                if ((expression->ident_decl &&
                     expression->ident_decl == bindings[index].declaration) ||
                    (!expression->ident_decl &&
                     expression->ident_name &&
                     bindings[index].declaration &&
                     bindings[index].declaration->name &&
                     strcmp(expression->ident_name,
                            bindings[index].declaration->name) == 0)) {
                    *value = bindings[index].value;
                    return true;
                }
            }
            if (expression->ident_decl &&
                expression->ident_decl->kind == DECL_ENUM_CONST) {
                *value = expression->ident_decl->enum_val;
                return true;
            }
            if (expression->ident_decl &&
                expression->ident_decl->kind == DECL_VAR &&
                expression->ident_decl->var_is_constexpr &&
                expression->ident_decl->var_init &&
                constexpr_eval_depth < 64) {
                ++constexpr_eval_depth;
                bool result = sema_eval_constexpr_expr(
                    expression->ident_decl->var_init, bindings,
                    binding_count, value);
                --constexpr_eval_depth;
                return result;
            }
            return false;
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
        case EXPR_RSHIFT_ASSIGN: {
            int binding_index = sema_constexpr_binding_index(
                expression->binary_lhs, bindings, binding_count);
            int64_t assigned;
            if (binding_index < 0 ||
                !sema_eval_constexpr_expr(expression->binary_rhs, bindings,
                                           binding_count, &right)) {
                return false;
            }
            if (expression->kind == EXPR_ASSIGN) {
                assigned = right;
            } else {
                left = bindings[binding_index].value;
                switch (expression->kind) {
                    case EXPR_ADD_ASSIGN:
                        if (!sema_constexpr_add(left, right, &assigned)) {
                            return false;
                        }
                        break;
                    case EXPR_SUB_ASSIGN:
                        if (!sema_constexpr_sub(left, right, &assigned)) {
                            return false;
                        }
                        break;
                    case EXPR_MUL_ASSIGN:
                        if (!sema_constexpr_mul(left, right, &assigned)) {
                            return false;
                        }
                        break;
                    case EXPR_DIV_ASSIGN:
                        if (right == 0 ||
                            (left == INT64_MIN && right == -1)) return false;
                        assigned = left / right;
                        break;
                    case EXPR_MOD_ASSIGN:
                        if (right == 0 ||
                            (left == INT64_MIN && right == -1)) return false;
                        assigned = left % right;
                        break;
                    case EXPR_AND_ASSIGN: assigned = left & right; break;
                    case EXPR_OR_ASSIGN: assigned = left | right; break;
                    case EXPR_XOR_ASSIGN: assigned = left ^ right; break;
                    case EXPR_LSHIFT_ASSIGN:
                        if (right < 0 || right >= 64) return false;
                        assigned = left << right;
                        break;
                    case EXPR_RSHIFT_ASSIGN:
                        if (right < 0 || right >= 64) return false;
                        assigned = left >> right;
                        break;
                    default:
                        return false;
                }
            }
            if (!sema_constexpr_convert(
                    assigned, bindings[binding_index].declaration->type,
                    &assigned)) {
                return false;
            }
            bindings[binding_index].value = assigned;
            *value = assigned;
            return true;
        }
        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC: {
            int binding_index = sema_constexpr_binding_index(
                expression->unary_operand, bindings, binding_count);
            int64_t old_value;
            if (binding_index < 0) return false;
            old_value = bindings[binding_index].value;
            if (expression->kind == EXPR_PREINC ||
                expression->kind == EXPR_POSTINC) {
                if (!sema_constexpr_add(old_value, 1, &right)) return false;
            } else if (!sema_constexpr_sub(old_value, 1, &right)) {
                return false;
            }
            if (!sema_constexpr_convert(
                    right, bindings[binding_index].declaration->type,
                    &right)) {
                return false;
            }
            bindings[binding_index].value = right;
            *value = expression->kind == EXPR_PREINC ||
                     expression->kind == EXPR_PREDEC ? right : old_value;
            return true;
        }
        case EXPR_NEG:
            if (!sema_eval_constexpr_expr(expression->unary_operand,
                                           bindings, binding_count, &left)) {
                return false;
            }
            if (left == INT64_MIN) return false;
            *value = -left;
            return true;
        case EXPR_NOT:
            if (!sema_eval_constexpr_expr(expression->unary_operand,
                                           bindings, binding_count, &left)) {
                return false;
            }
            *value = !left;
            return true;
        case EXPR_BITNOT:
            if (!sema_eval_constexpr_expr(expression->unary_operand,
                                           bindings, binding_count, &left)) {
                return false;
            }
            *value = ~left;
            return true;
        case EXPR_SIZEOF:
        case EXPR_ALIGNOF:
            measured = expression->sizeof_type
                ? expression->sizeof_type
                : (expression->unary_operand
                    ? expression->unary_operand->type : NULL);
            if (!measured || (expression->kind == EXPR_SIZEOF
                                  ? measured->size <= 0 : measured->align <= 0)) {
                return false;
            }
            *value = expression->kind == EXPR_SIZEOF
                ? measured->size : measured->align;
            return true;
        case EXPR_CAST:
            if (!sema_eval_constexpr_expr(expression->cast_expr, bindings,
                                           binding_count, &left)) {
                return false;
            }
            return sema_constexpr_convert(left, expression->cast_type, value);
        case EXPR_COND:
            if (!sema_eval_constexpr_expr(expression->cond_test, bindings,
                                           binding_count, &condition)) {
                return false;
            }
            return sema_eval_constexpr_expr(
                condition ? expression->cond_then : expression->cond_else,
                bindings, binding_count, value);
        case EXPR_COMMA:
            if (!sema_eval_constexpr_expr(expression->binary_lhs, bindings,
                                           binding_count, &left)) {
                return false;
            }
            return sema_eval_constexpr_expr(expression->binary_rhs, bindings,
                                            binding_count, value);
        case EXPR_AND:
            if (!sema_eval_constexpr_expr(expression->binary_lhs, bindings,
                                           binding_count, &left)) {
                return false;
            }
            if (!left) {
                *value = 0;
                return true;
            }
            if (!sema_eval_constexpr_expr(expression->binary_rhs, bindings,
                                           binding_count, &right)) {
                return false;
            }
            *value = right != 0;
            return true;
        case EXPR_OR:
            if (!sema_eval_constexpr_expr(expression->binary_lhs, bindings,
                                           binding_count, &left)) {
                return false;
            }
            if (left) {
                *value = 1;
                return true;
            }
            if (!sema_eval_constexpr_expr(expression->binary_rhs, bindings,
                                           binding_count, &right)) {
                return false;
            }
            *value = right != 0;
            return true;
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
            if (!sema_eval_constexpr_expr(expression->binary_lhs, bindings,
                                           binding_count, &left) ||
                !sema_eval_constexpr_expr(expression->binary_rhs, bindings,
                                           binding_count, &right)) {
                return false;
            }
            switch (expression->kind) {
                case EXPR_ADD:
                    return sema_constexpr_add(left, right, value);
                case EXPR_SUB:
                    return sema_constexpr_sub(left, right, value);
                case EXPR_MUL:
                    return sema_constexpr_mul(left, right, value);
                case EXPR_DIV:
                    if (right == 0 || (left == INT64_MIN && right == -1)) {
                        return false;
                    }
                    *value = left / right;
                    return true;
                case EXPR_MOD:
                    if (right == 0 || (left == INT64_MIN && right == -1)) {
                        return false;
                    }
                    *value = left % right;
                    return true;
                case EXPR_BITAND:
                    *value = left & right;
                    return true;
                case EXPR_BITOR:
                    *value = left | right;
                    return true;
                case EXPR_BITXOR:
                    *value = left ^ right;
                    return true;
                case EXPR_LSHIFT:
                    if (right < 0 || right >= 64 || left < 0 ||
                        (right == 63 && left != 0) ||
                        (right < 63 && left > (INT64_MAX >> right))) {
                        return false;
                    }
                    *value = left << right;
                    return true;
                case EXPR_RSHIFT:
                    if (right < 0 || right >= 64) return false;
                    *value = left >> right;
                    return true;
                case EXPR_EQ:
                    *value = left == right;
                    return true;
                case EXPR_NE:
                    *value = left != right;
                    return true;
                case EXPR_LT:
                    *value = left < right;
                    return true;
                case EXPR_GT:
                    *value = left > right;
                    return true;
                case EXPR_LE:
                    *value = left <= right;
                    return true;
                case EXPR_GE:
                    *value = left >= right;
                    return true;
                default:
                    return false;
            }
        default:
            return false;
    }
}

typedef enum {
    SEMA_CONSTEXPR_STMT_FALLTHROUGH = 0,
    SEMA_CONSTEXPR_STMT_RETURNED = 1,
    SEMA_CONSTEXPR_STMT_BREAK = 2,
    SEMA_CONSTEXPR_STMT_CONTINUE = 3
} SemaConstexprStatementResult;

static bool sema_eval_constexpr_statement(
    Stmt* statement, SemaConstexprBinding* bindings, int* binding_count,
    int64_t* value, SemaConstexprStatementResult* result) {
    int saved_binding_count;

    if (!statement || !bindings || !binding_count || !value || !result) {
        return false;
    }
    *result = SEMA_CONSTEXPR_STMT_FALLTHROUGH;
    switch (statement->kind) {
        case STMT_NULL:
            return true;
        case STMT_EXPR:
            return !statement->expr ||
                sema_eval_constexpr_expr(statement->expr, bindings,
                                         *binding_count, value);
        case STMT_RETURN:
            if (!statement->return_val ||
                !sema_eval_constexpr_expr(statement->return_val, bindings,
                                           *binding_count, value)) {
                return false;
            }
            *result = SEMA_CONSTEXPR_STMT_RETURNED;
            return true;
        case STMT_DECL: {
            int64_t initializer;
            Decl* declaration = statement->decl;
            if (!declaration || declaration->kind != DECL_VAR ||
                !declaration->name || !sema_constexpr_integer_type(
                    declaration->type) || !declaration->var_init ||
                *binding_count >= 64 ||
                !sema_eval_constexpr_expr(declaration->var_init, bindings,
                                           *binding_count, &initializer)) {
                return false;
            }
            bindings[*binding_count].declaration = declaration;
            bindings[*binding_count].value = initializer;
            ++*binding_count;
            return true;
        }
        case STMT_BLOCK:
            saved_binding_count = *binding_count;
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                SemaConstexprStatementResult nested_result;
                if (!sema_eval_constexpr_statement(
                        item->stmt, bindings, binding_count, value,
                        &nested_result)) {
                    *binding_count = saved_binding_count;
                    return false;
                }
                if (nested_result == SEMA_CONSTEXPR_STMT_RETURNED) {
                    *result = nested_result;
                    *binding_count = saved_binding_count;
                    return true;
                }
                if (nested_result == SEMA_CONSTEXPR_STMT_BREAK ||
                    nested_result == SEMA_CONSTEXPR_STMT_CONTINUE) {
                    *result = nested_result;
                    *binding_count = saved_binding_count;
                    return true;
                }
            }
            *binding_count = saved_binding_count;
            return true;
        case STMT_IF: {
            int64_t condition;
            Stmt* selected;
            if (!statement->if_cond ||
                !sema_eval_constexpr_expr(statement->if_cond, bindings,
                                           *binding_count, &condition)) {
                return false;
            }
            selected = condition ? statement->if_then : statement->if_else;
            if (!selected) return true;
            return sema_eval_constexpr_statement(
                selected, bindings, binding_count, value, result);
        }
        case STMT_BREAK:
            *result = SEMA_CONSTEXPR_STMT_BREAK;
            return true;
        case STMT_CONTINUE:
            *result = SEMA_CONSTEXPR_STMT_CONTINUE;
            return true;
        case STMT_FOR: {
            int64_t condition;
            int saved_count = *binding_count;
            unsigned iteration;
            if (statement->for_init) {
                SemaConstexprStatementResult init_result;
                if (!sema_eval_constexpr_statement(
                        statement->for_init, bindings, binding_count, value,
                        &init_result) ||
                    init_result != SEMA_CONSTEXPR_STMT_FALLTHROUGH) {
                    *binding_count = saved_count;
                    return false;
                }
            }
            for (iteration = 0u; iteration < 1000000u; ++iteration) {
                SemaConstexprStatementResult body_result;
                if (statement->for_cond &&
                    !sema_eval_constexpr_expr(
                        statement->for_cond, bindings, *binding_count,
                        &condition)) {
                    *binding_count = saved_count;
                    return false;
                }
                if (statement->for_cond && !condition) break;
                body_result = SEMA_CONSTEXPR_STMT_FALLTHROUGH;
                if (statement->for_body &&
                    !sema_eval_constexpr_statement(
                        statement->for_body, bindings, binding_count, value,
                        &body_result)) {
                    *binding_count = saved_count;
                    return false;
                } else {
                    if (body_result == SEMA_CONSTEXPR_STMT_RETURNED) {
                        *result = body_result;
                        *binding_count = saved_count;
                        return true;
                    }
                    if (body_result == SEMA_CONSTEXPR_STMT_BREAK) break;
                }
                if (statement->for_inc &&
                    !sema_eval_constexpr_expr(
                        statement->for_inc, bindings, *binding_count,
                        value)) {
                    *binding_count = saved_count;
                    return false;
                }
            }
            if (iteration == 1000000u) {
                *binding_count = saved_count;
                return false;
            }
            *binding_count = saved_count;
            return true;
        }
        case STMT_WHILE:
        case STMT_DO: {
            int64_t condition;
            unsigned iteration;
            bool do_body = statement->kind == STMT_DO;
            for (iteration = 0u; iteration < 1000000u; ++iteration) {
                SemaConstexprStatementResult body_result =
                    SEMA_CONSTEXPR_STMT_FALLTHROUGH;
                if (!do_body) {
                    if (!sema_eval_constexpr_expr(
                            statement->while_cond, bindings, *binding_count,
                            &condition)) {
                        return false;
                    }
                    if (!condition) return true;
                }
                if (statement->while_body &&
                    !sema_eval_constexpr_statement(
                        statement->while_body, bindings, binding_count, value,
                        &body_result)) {
                    return false;
                }
                if (body_result == SEMA_CONSTEXPR_STMT_RETURNED) {
                    *result = body_result;
                    return true;
                }
                if (body_result == SEMA_CONSTEXPR_STMT_BREAK) return true;
                if (statement->kind == STMT_WHILE) continue;
                if (!sema_eval_constexpr_expr(
                        statement->while_cond, bindings, *binding_count,
                        &condition)) {
                    return false;
                }
                if (!condition) return true;
                do_body = false;
            }
            return false;
        }
        default:
            return false;
    }
}

static bool sema_eval_constexpr_function(Decl* declaration, ExprList* args,
                                          int64_t* value) {
    SemaConstexprBinding bindings[64];
    DeclList* parameter;
    ExprList* argument;
    Stmt* body;
    int count = 0;
    SemaConstexprStatementResult statement_result;
    int64_t argument_value;
    bool result;

    memset(bindings, 0, sizeof(bindings));

    if (!declaration || !value || !declaration->func_is_constexpr ||
        declaration->func_this_param || !declaration->type ||
        declaration->type->variadic || !declaration->func_body ||
        declaration->func_body->kind != STMT_BLOCK ||
        !declaration->func_body->block_stmts || constexpr_eval_depth >= 64) {
        return false;
    }
    body = declaration->func_body;
    parameter = declaration->func_params;
    argument = args;
    while (parameter && argument) {
        if (count == (int)(sizeof(bindings) / sizeof(bindings[0])) ||
            !parameter->decl || !parameter->decl->name ||
            !sema_constexpr_integer_type(parameter->decl->type) ||
            !sema_eval_constexpr_expr(argument->expr, NULL, 0,
                                      &argument_value)) {
            return false;
        }
        bindings[count].declaration = parameter->decl;
        bindings[count].type = parameter->decl->type;
        bindings[count].value = argument_value;
        bindings[count].floating_value = 0.0;
        bindings[count].is_floating = false;
        ++count;
        parameter = parameter->next;
        argument = argument->next;
    }
    if (parameter || argument) return false;
    ++constexpr_eval_depth;
    result = sema_eval_constexpr_statement(
        body, bindings, &count, value, &statement_result);
    --constexpr_eval_depth;
    if (!result || statement_result != SEMA_CONSTEXPR_STMT_RETURNED) {
        return false;
    }
    return sema_constexpr_convert(*value, declaration->type->ret_type, value);
}

typedef struct SemaConstexprScalar {
    Type* type;
    int64_t integer_value;
    double floating_value;
    bool is_floating;
    bool is_pointer;
    Decl* pointer_declaration;
    int64_t pointer_offset;
} SemaConstexprScalar;

/* Integer constexpr values keep their target-width bit pattern in the
 * int64_t carrier.  Never compare or calculate an unsigned value through
 * the host's signed representation: ULL literals such as UINT64_MAX would
 * otherwise become -1 during static assertion evaluation. */
static uint64_t sema_constexpr_integer_mask(Type* type) {
    unsigned bits = type && type->size > 0
        ? (unsigned)type->size * 8u : 0u;
    if (bits == 0u) return 0u;
    return bits >= 64u ? UINT64_MAX : (UINT64_C(1) << bits) - 1u;
}

static uint64_t sema_constexpr_integer_bits(
    const SemaConstexprScalar* value) {
    if (!value || value->is_floating) return 0u;
    return (uint64_t)value->integer_value &
           sema_constexpr_integer_mask(value->type);
}

static int64_t sema_constexpr_integer_signed(
    const SemaConstexprScalar* value) {
    uint64_t mask;
    uint64_t bits;
    unsigned width;
    if (!value || value->is_floating) return 0;
    mask = sema_constexpr_integer_mask(value->type);
    bits = sema_constexpr_integer_bits(value);
    width = value->type && value->type->size > 0
        ? (unsigned)value->type->size * 8u : 64u;
    if (width < 64u && !value->type->is_unsigned &&
        (bits & (UINT64_C(1) << (width - 1u))) != 0u) {
        bits |= ~mask;
    }
    return (int64_t)bits;
}

static bool sema_constexpr_scalar_type(Type* type) {
    return type && (sema_constexpr_integer_type(type) ||
                    type->kind == TYPE_PTR ||
                    type->kind == TYPE_FLOAT || type->kind == TYPE_DOUBLE);
}

static bool sema_constexpr_scalar_convert(
    const SemaConstexprScalar* input, Type* type,
    SemaConstexprScalar* output) {
    /* The target ABI has no long-double scalar.  Keep evaluator intermediates
     * within the supported language surface so RCC can bootstrap its own
     * semantic analyser with -nostdinc. */
    double numeric;
    unsigned bits;
    uint64_t converted;
    double minimum;
    double maximum;

    if (!input || !output || !sema_constexpr_scalar_type(type) ||
        type->size <= 0) return false;
    if (type->kind == TYPE_PTR) {
        if (input->is_pointer) {
            output->type = type;
            output->integer_value = 0;
            output->floating_value = 0.0;
            output->is_floating = false;
            output->is_pointer = true;
            output->pointer_declaration = input->pointer_declaration;
            output->pointer_offset = input->pointer_offset;
            return true;
        }
        if (!input->is_floating && input->type &&
            sema_constexpr_integer_type(input->type) &&
            sema_constexpr_integer_bits(input) == 0u) {
            output->type = type;
            output->integer_value = 0;
            output->floating_value = 0.0;
            output->is_floating = false;
            output->is_pointer = true;
            output->pointer_declaration = NULL;
            output->pointer_offset = 0;
            return true;
        }
        return false;
    }
    if (input->is_pointer) return false;
    if (type->kind == TYPE_FLOAT || type->kind == TYPE_DOUBLE) {
        numeric = input->is_floating
            ? input->floating_value
            : (input->type && input->type->is_unsigned
                ? (double)sema_constexpr_integer_bits(input)
                : (double)sema_constexpr_integer_signed(input));
        if (!isfinite(numeric)) return false;
        output->type = type;
        output->is_floating = true;
        output->floating_value = type->kind == TYPE_FLOAT
            ? (double)(float)numeric : (double)numeric;
        output->integer_value = 0;
        output->is_pointer = false;
        output->pointer_declaration = NULL;
        output->pointer_offset = 0;
        if (!isfinite(output->floating_value) ||
            (type->kind == TYPE_FLOAT &&
             (output->floating_value > FLT_MAX ||
              output->floating_value < -FLT_MAX))) return false;
        return true;
    }
    if (input->is_floating) {
        numeric = (double)input->floating_value;
        if (!isfinite(numeric)) return false;
        bits = (unsigned)type->size * 8u;
        if (bits == 0u || bits > 64u) return false;
        if (type->is_unsigned) {
            maximum = bits == 64u
                ? (double)UINT64_MAX
                : (double)((UINT64_C(1) << bits) - 1u);
            if (numeric < 0.0 || numeric >= maximum + 1.0) return false;
        } else {
            minimum = bits == 64u
                ? (double)INT64_MIN
                : -(double)(UINT64_C(1) << (bits - 1u));
            maximum = bits == 64u
                ? (double)INT64_MAX
                : (double)((UINT64_C(1) << (bits - 1u)) - 1u);
            if (numeric < minimum || numeric >= maximum + 1.0) return false;
        }
        if (type->is_unsigned && numeric >= 9223372036854775808.0) {
            converted = (uint64_t)(numeric - 9223372036854775808.0) +
                        UINT64_C(0x8000000000000000);
        } else {
            converted = (uint64_t)(int64_t)numeric;
        }
        output->integer_value = (int64_t)converted;
    } else {
        output->integer_value = input->integer_value;
    }
    if (!sema_constexpr_convert(output->integer_value, type,
                                &output->integer_value)) return false;
    output->type = type;
    output->floating_value = 0.0;
    output->is_floating = false;
    output->is_pointer = false;
    output->pointer_declaration = NULL;
    output->pointer_offset = 0;
    return true;
}

static bool sema_eval_constexpr_scalar_expr(
    Expr* expression, SemaConstexprBinding* bindings,
    int binding_count, SemaConstexprScalar* value);

static TypeField* initializer_field(Type* type, const char* name);

static bool sema_eval_constexpr_scalar_object(
    Type* type, Expr* initializer, SemaConstexprBinding* bindings,
    int binding_count, SemaConstexprScalar* value);

static bool sema_constexpr_aggregate_type(Type* type) {
    return type && (type->kind == TYPE_ARRAY ||
                    type->kind == TYPE_STRUCT ||
                    type->kind == TYPE_UNION) &&
           type_is_complete(type) && type->size > 0;
}

static bool sema_constexpr_pointer_offset(int64_t base, int64_t elements,
                                          int element_size, int64_t* result) {
    int64_t bytes;
    if (!result || element_size <= 0) return false;
#if defined(__GNUC__) || defined(__clang__)
    if (__builtin_mul_overflow(elements, (int64_t)element_size, &bytes) ||
        __builtin_add_overflow(base, bytes, result)) return false;
#else
    if (elements != 0 &&
        (elements > INT64_MAX / element_size ||
         elements < INT64_MIN / element_size)) return false;
    bytes = elements * element_size;
    if ((bytes > 0 && base > INT64_MAX - bytes) ||
        (bytes < 0 && base < INT64_MIN - bytes)) return false;
    *result = base + bytes;
#endif
    return true;
}

static bool sema_constexpr_address_target(Expr* expression, Decl** declaration,
                                          int64_t* offset) {
    int64_t index;
    int64_t element_size;
    if (!expression || !declaration || !offset) return false;
    if (expression->kind == EXPR_IDENT && expression->ident_decl) {
        Decl* target = expression->ident_decl;
        if (target->kind == DECL_FUNC ||
            (target->kind == DECL_VAR &&
             (target->var_is_global || target->var_is_static_local))) {
            *declaration = target;
            *offset = 0;
            return true;
        }
        return false;
    }
    if (expression->kind == EXPR_INDEX && expression->index_base &&
        expression->index_expr &&
        sema_constexpr_address_target(expression->index_base, declaration,
                                       offset) &&
        (*declaration)->kind == DECL_VAR && (*declaration)->type &&
        (*declaration)->type->kind == TYPE_ARRAY &&
        (*declaration)->type->base &&
        expr_eval_integer_constant(expression->index_expr, &index)) {
        element_size = (*declaration)->type->base->size;
        if (element_size <= 0 || index < 0 ||
            ((*declaration)->type->array_len >= 0 &&
             index > (*declaration)->type->array_len) ||
            index > INT64_MAX / element_size ||
            index < INT64_MIN / element_size) return false;
        return sema_constexpr_pointer_offset(*offset, index,
                                             (int)element_size, offset);
    }
    if (expression->kind == EXPR_MEMBER && expression->member_base &&
        expression->member_field &&
        sema_constexpr_address_target(expression->member_base, declaration,
                                       offset) &&
        expression->member_field->offset >= 0) {
        return sema_constexpr_pointer_offset(
            *offset, expression->member_field->offset, 1, offset);
    }
    return false;
}

static bool sema_constexpr_zero_initializer(Expr* initializer);

static bool sema_constexpr_store_scalar_bytes(
    unsigned char* storage, size_t storage_size, Type* type,
    const SemaConstexprScalar* value) {
    SemaConstexprScalar converted;
    uint64_t bits;
    size_t index;

    if (!storage || !value || !sema_constexpr_scalar_type(type) ||
        type->size <= 0 || (size_t)type->size > storage_size ||
        (size_t)type->size > sizeof(uint64_t) ||
        !sema_constexpr_scalar_convert(value, type, &converted)) {
        return false;
    }
    memset(storage, 0, (size_t)type->size);
    if (type->kind == TYPE_FLOAT) {
        float floating = (float)converted.floating_value;
        if (sizeof(floating) != (size_t)type->size) return false;
        memcpy(storage, &floating, sizeof(floating));
        return true;
    }
    if (type->kind == TYPE_DOUBLE) {
        if (sizeof(converted.floating_value) != (size_t)type->size) {
            return false;
        }
        memcpy(storage, &converted.floating_value,
               sizeof(converted.floating_value));
        return true;
    }
    bits = sema_constexpr_integer_bits(&converted);
    for (index = 0; index < (size_t)type->size; ++index) {
        storage[index] = (unsigned char)(bits >> (index * 8u));
    }
    return true;
}

static bool sema_constexpr_load_scalar_bytes(
    const unsigned char* storage, size_t storage_size, Type* type,
    SemaConstexprScalar* value) {
    uint64_t bits = 0u;
    size_t index;

    if (!storage || !value || !sema_constexpr_scalar_type(type) ||
        type->size <= 0 || (size_t)type->size > storage_size ||
        (size_t)type->size > sizeof(uint64_t)) return false;
    memset(value, 0, sizeof(*value));
    value->type = type;
    if (type->kind == TYPE_FLOAT) {
        float floating;
        if (sizeof(floating) != (size_t)type->size) return false;
        memcpy(&floating, storage, sizeof(floating));
        value->floating_value = floating;
        value->is_floating = true;
        return isfinite(value->floating_value);
    }
    if (type->kind == TYPE_DOUBLE) {
        if (sizeof(value->floating_value) != (size_t)type->size) {
            return false;
        }
        memcpy(&value->floating_value, storage,
               sizeof(value->floating_value));
        value->is_floating = true;
        return isfinite(value->floating_value);
    }
    for (index = 0; index < (size_t)type->size; ++index) {
        bits |= (uint64_t)storage[index] << (index * 8u);
    }
    value->integer_value = (int64_t)bits;
    value->is_floating = false;
    return true;
}

static bool sema_constexpr_materialize_object(
    Type* type, Expr* initializer, SemaConstexprBinding* bindings,
    int binding_count, unsigned char* storage, size_t storage_size);

static bool sema_eval_constexpr_aggregate_function(
    Decl* declaration, ExprList* args, SemaConstexprBinding* caller_bindings,
    int caller_binding_count, unsigned char* storage, size_t storage_size);

static bool sema_constexpr_scalar_truth(const SemaConstexprScalar* value);

static Expr* sema_constexpr_rebuild_object(
    Type* type, const unsigned char* storage, size_t storage_size,
    SourceLoc loc);

static bool sema_constexpr_binding_lvalue(
    Expr* expression, SemaConstexprBinding* bindings, int binding_count,
    int* binding_index, size_t* offset, Type** type) {
    SemaConstexprScalar index_value;
    Type* base_type;
    size_t base_offset;
    int64_t index;
    size_t element_size;

    if (!expression || !bindings || !binding_index || !offset || !type) {
        return false;
    }
    if (expression->kind == EXPR_IDENT) {
        int index = sema_constexpr_binding_index(
            expression, bindings, binding_count);
        if (index < 0 || !bindings[index].is_object ||
            !bindings[index].object_bytes || !bindings[index].type) {
            return false;
        }
        *binding_index = index;
        *offset = 0u;
        *type = bindings[index].type;
        return true;
    }
    if (expression->kind == EXPR_MEMBER) {
        if (!expression->member_base || !expression->member_field ||
            !sema_constexpr_binding_lvalue(
                expression->member_base, bindings, binding_count,
                binding_index, &base_offset, &base_type) ||
            !base_type ||
            (base_type->kind != TYPE_STRUCT &&
             base_type->kind != TYPE_UNION) ||
            expression->member_field->offset < 0) return false;
        if (base_offset > SIZE_MAX -
                (size_t)expression->member_field->offset) return false;
        *offset = base_offset +
                  (size_t)expression->member_field->offset;
        *type = expression->member_field->type;
        return *type != NULL;
    }
    if (expression->kind != EXPR_INDEX || !expression->index_base) {
        return false;
    }
    if (!sema_constexpr_binding_lvalue(
            expression->index_base, bindings, binding_count,
            binding_index, &base_offset, &base_type) ||
        !base_type || base_type->kind != TYPE_ARRAY || !base_type->base ||
        !sema_eval_constexpr_scalar_expr(
            expression->index_expr, bindings, binding_count, &index_value) ||
        index_value.is_floating || index_value.integer_value < 0 ||
        base_type->array_len < 0 ||
        index_value.integer_value >= base_type->array_len ||
        base_type->base->size <= 0) return false;
    index = index_value.integer_value;
    element_size = (size_t)base_type->base->size;
    if ((uint64_t)index > SIZE_MAX / element_size ||
        base_offset > SIZE_MAX - (size_t)index * element_size) return false;
    *offset = base_offset + (size_t)index * element_size;
    *type = base_type->base;
    return true;
}

static bool sema_constexpr_load_binding_scalar(
    Expr* expression, SemaConstexprBinding* bindings, int binding_count,
    SemaConstexprScalar* value) {
    int binding_index;
    size_t offset;
    Type* type;
    if (!sema_constexpr_binding_lvalue(
            expression, bindings, binding_count, &binding_index, &offset,
            &type) || !sema_constexpr_scalar_type(type) ||
        offset > bindings[binding_index].object_size ||
        (size_t)type->size > bindings[binding_index].object_size - offset) {
        return false;
    }
    return sema_constexpr_load_scalar_bytes(
        bindings[binding_index].object_bytes + offset,
        bindings[binding_index].object_size - offset, type, value);
}

static bool sema_constexpr_store_binding_scalar(
    Expr* expression, SemaConstexprBinding* bindings, int binding_count,
    const SemaConstexprScalar* value, SemaConstexprScalar* stored) {
    int binding_index;
    size_t offset;
    Type* type;
    SemaConstexprScalar converted;
    if (!sema_constexpr_binding_lvalue(
            expression, bindings, binding_count, &binding_index, &offset,
            &type) || !sema_constexpr_scalar_type(type) ||
        offset > bindings[binding_index].object_size ||
        (size_t)type->size > bindings[binding_index].object_size - offset ||
        !sema_constexpr_scalar_convert(value, type, &converted) ||
        !sema_constexpr_store_scalar_bytes(
            bindings[binding_index].object_bytes + offset,
            bindings[binding_index].object_size - offset, type, &converted)) {
        return false;
    }
    if (stored) *stored = converted;
    return true;
}

static bool sema_constexpr_assign_object(
    Expr* expression, SemaConstexprBinding* bindings, int binding_count) {
    int binding_index;
    size_t offset;
    Type* type;
    unsigned char* temporary;

    if (!expression || expression->kind != EXPR_ASSIGN ||
        !expression->binary_lhs || !expression->binary_rhs ||
        !sema_constexpr_binding_lvalue(
            expression->binary_lhs, bindings, binding_count, &binding_index,
            &offset, &type) || !sema_constexpr_aggregate_type(type) ||
        offset > bindings[binding_index].object_size ||
        (size_t)type->size > bindings[binding_index].object_size - offset) {
        return false;
    }
    temporary = ast_arena_alloc((size_t)type->size);
    if (!sema_constexpr_materialize_object(
            type, expression->binary_rhs, bindings, binding_count,
            temporary, (size_t)type->size)) return false;
    memcpy(bindings[binding_index].object_bytes + offset, temporary,
           (size_t)type->size);
    return true;
}

static bool sema_constexpr_materialize_object(
    Type* type, Expr* initializer, SemaConstexprBinding* bindings,
    int binding_count, unsigned char* storage, size_t storage_size) {
    ExprList* item;

    if (!type || !storage || type->size <= 0 ||
        (size_t)type->size > storage_size) return false;
    memset(storage, 0, (size_t)type->size);
    if (!initializer) return true;
    if (sema_constexpr_scalar_type(type)) {
        SemaConstexprScalar value;
        return sema_eval_constexpr_scalar_expr(
                   initializer, bindings, binding_count, &value) &&
               sema_constexpr_store_scalar_bytes(
                   storage, storage_size, type, &value);
    }
    if (!sema_constexpr_aggregate_type(type)) return false;
    if (bindings && (initializer->kind == EXPR_IDENT ||
                     initializer->kind == EXPR_MEMBER ||
                     initializer->kind == EXPR_INDEX)) {
        int binding_index;
        size_t source_offset;
        Type* source_type;
        if (sema_constexpr_binding_lvalue(
                initializer, bindings, binding_count, &binding_index,
                &source_offset, &source_type) &&
            sema_constexpr_aggregate_type(source_type) &&
            type_is_compatible(type, source_type) &&
            source_offset <= bindings[binding_index].object_size &&
            (size_t)type->size <=
                bindings[binding_index].object_size - source_offset) {
            memcpy(storage, bindings[binding_index].object_bytes + source_offset,
                   (size_t)type->size);
            return true;
        }
    }
    if (initializer->kind == EXPR_CALL && initializer->call_func &&
        initializer->call_func->kind == EXPR_IDENT &&
        initializer->call_func->ident_decl &&
        initializer->call_func->ident_decl->kind == DECL_FUNC &&
        initializer->call_func->ident_decl->type &&
        initializer->call_func->ident_decl->type->kind == TYPE_FUNC &&
        sema_constexpr_aggregate_type(
            initializer->call_func->ident_decl->type->ret_type) &&
        type_is_compatible(
            type, initializer->call_func->ident_decl->type->ret_type)) {
        return sema_eval_constexpr_aggregate_function(
            initializer->call_func->ident_decl, initializer->call_args,
            bindings, binding_count, storage, storage_size);
    }
    if (initializer->kind == EXPR_COND || initializer->kind == EXPR_COMMA) {
        SemaConstexprScalar condition;
        if (initializer->kind == EXPR_COND) {
            if (!sema_eval_constexpr_scalar_expr(
                    initializer->cond_test, bindings, binding_count,
                    &condition)) return false;
            return sema_constexpr_materialize_object(
                type, sema_constexpr_scalar_truth(&condition)
                    ? initializer->cond_then : initializer->cond_else,
                bindings, binding_count, storage, storage_size);
        }
        if (!sema_eval_constexpr_scalar_expr(
                initializer->binary_lhs, bindings, binding_count,
                &condition)) return false;
        return sema_constexpr_materialize_object(
            type, initializer->binary_rhs, bindings, binding_count,
            storage, storage_size);
    }
    if (initializer->kind == EXPR_IDENT) {
        int binding_index = sema_constexpr_binding_index(
            initializer, bindings, binding_count);
        if (binding_index >= 0) {
            if (!bindings[binding_index].is_object ||
                !type_is_compatible(type, bindings[binding_index].type) ||
                bindings[binding_index].object_size < (size_t)type->size) {
                return false;
            }
            memcpy(storage, bindings[binding_index].object_bytes,
                   (size_t)type->size);
            return true;
        }
        if (initializer->ident_decl &&
            initializer->ident_decl->kind == DECL_VAR &&
            initializer->ident_decl->var_is_constexpr &&
            initializer->ident_decl->var_init && constexpr_eval_depth < 64) {
            ++constexpr_eval_depth;
            bool result = sema_constexpr_materialize_object(
                type, initializer->ident_decl->var_init, bindings,
                binding_count, storage, storage_size);
            --constexpr_eval_depth;
            return result;
        }
        return false;
    }
    if (initializer->kind != EXPR_COMPOUND ||
        (initializer->compound_type &&
         !type_is_compatible(type, initializer->compound_type))) return false;
    if (sema_constexpr_zero_initializer(initializer)) return true;
    if (type->kind == TYPE_ARRAY) {
        int64_t cursor = 0;
        if (!type->base || type->base->size <= 0) return false;
        for (item = initializer->compound_init; item; item = item->next) {
            if (item->designator_kind == INIT_DESIGNATOR_FIELD ||
                (item->designator_kind == INIT_DESIGNATOR_INDEX &&
                 (cursor = item->designator_index) < 0) ||
                cursor < 0 || cursor >= type->array_len ||
                (size_t)cursor > SIZE_MAX / (size_t)type->base->size ||
                !sema_constexpr_materialize_object(
                    type->base, item->expr, bindings, binding_count,
                    storage + (size_t)cursor * (size_t)type->base->size,
                    storage_size - (size_t)cursor * (size_t)type->base->size)) {
                return false;
            }
            if (cursor == INT64_MAX) return false;
            ++cursor;
        }
        return true;
    }

    if (type->kind == TYPE_STRUCT) {
        for (TypeField* field = type->fields; field; field = field->next) {
            if (field->initializer && field->offset >= 0 &&
                (size_t)field->offset <= storage_size &&
                !sema_constexpr_materialize_object(
                    field->type, field->initializer, bindings, binding_count,
                    storage + (size_t)field->offset,
                    storage_size - (size_t)field->offset)) return false;
        }
    }
    {
        TypeField* cursor = type->fields;
        int initialized = 0;
        for (item = initializer->compound_init; item; item = item->next) {
            TypeField* field = cursor;
            if (item->designator_kind == INIT_DESIGNATOR_INDEX) return false;
            if (item->designator_kind == INIT_DESIGNATOR_FIELD) {
                field = initializer_field(type, item->designator_field);
            }
            if (!field || (type->kind == TYPE_UNION && initialized != 0 &&
                           item->designator_kind == INIT_DESIGNATOR_NONE) ||
                field->offset < 0 || (size_t)field->offset > storage_size ||
                !sema_constexpr_materialize_object(
                    field->type, item->expr, bindings, binding_count,
                    storage + (size_t)field->offset,
                    storage_size - (size_t)field->offset)) return false;
            cursor = field->next;
            ++initialized;
        }
    }
    return true;
}

static bool sema_constexpr_zero_initializer(Expr* initializer) {
    ExprList* item;
    return initializer && initializer->kind == EXPR_COMPOUND &&
           (initializer->compound_value_init ||
            ((item = initializer->compound_init) != NULL &&
             !item->next && item->designator_kind == INIT_DESIGNATOR_NONE &&
             item->expr && item->expr->kind == EXPR_INT_LIT &&
             item->expr->int_val == 0));
}

/* Resolve an aggregate expression to the initializer which supplies its
 * storage.  This intentionally follows only constexpr aggregate variables,
 * compound initializers, and aggregate subobject accesses.  It never reads
 * an address or guesses a runtime value. */
static bool sema_constexpr_resolve_aggregate_expression(
    Expr* expression, Type* expected_type, Expr** initializer, bool* zero);

static bool sema_constexpr_select_aggregate_item(
    Type* type, Expr* initializer, TypeField* wanted_field,
    int64_t wanted_index, Expr** selected, bool* zero) {
    ExprList* item;
    if (!type || !sema_constexpr_aggregate_type(type) || !selected || !zero) {
        return false;
    }
    *selected = NULL;
    *zero = false;
    if (!initializer || sema_constexpr_zero_initializer(initializer)) {
        *zero = true;
        return true;
    }
    if (initializer->kind != EXPR_COMPOUND) {
        if (initializer->type &&
            initializer->type->kind == type->kind &&
            sema_constexpr_resolve_aggregate_expression(
                initializer, type, &initializer, zero)) {
            if (*zero) return true;
            return sema_constexpr_select_aggregate_item(
                type, initializer, wanted_field, wanted_index,
                selected, zero);
        }
        return false;
    }

    if (type->kind == TYPE_ARRAY) {
        int64_t cursor = 0;
        if (!type->base || wanted_index < 0) return false;
        for (item = initializer->compound_init; item; item = item->next) {
            if (item->designator_kind == INIT_DESIGNATOR_FIELD) return false;
            if (item->designator_kind == INIT_DESIGNATOR_INDEX) {
                cursor = item->designator_index;
            }
            if (cursor == wanted_index) {
                *selected = item->expr;
                return true;
            }
            if (cursor == INT64_MAX) return false;
            ++cursor;
        }
        *zero = true;
        return true;
    }

    {
        TypeField* cursor = type->fields;
        int initialized = 0;
        for (item = initializer->compound_init; item; item = item->next) {
            TypeField* field = cursor;
            if (item->designator_kind == INIT_DESIGNATOR_INDEX) return false;
            if (item->designator_kind == INIT_DESIGNATOR_FIELD) {
                field = initializer_field(type, item->designator_field);
            }
            if (!field || (type->kind == TYPE_UNION && initialized != 0 &&
                           item->designator_kind == INIT_DESIGNATOR_NONE)) {
                return false;
            }
            if (field == wanted_field) {
                *selected = item->expr;
                return true;
            }
            cursor = field->next;
            ++initialized;
        }
        if (wanted_field && wanted_field->initializer) {
            *selected = wanted_field->initializer;
            return true;
        }
        *zero = true;
        return true;
    }
}

static bool sema_constexpr_resolve_aggregate_expression(
    Expr* expression, Type* expected_type, Expr** initializer, bool* zero) {
    Decl* declaration;
    Expr* source_initializer;
    Type* base_type;
    Expr* selected;
    bool source_zero;
    if (!expression || !expected_type ||
        !sema_constexpr_aggregate_type(expected_type) || !initializer ||
        !zero) return false;
    *initializer = NULL;
    *zero = false;
    if (expression->kind == EXPR_COMPOUND) {
        if (!expression->compound_type ||
            !type_is_compatible(expression->compound_type, expected_type)) {
            return false;
        }
        *initializer = expression;
        *zero = sema_constexpr_zero_initializer(expression);
        return true;
    }
    if (expression->kind == EXPR_IDENT) {
        declaration = expression->ident_decl;
        if (!declaration || declaration->kind != DECL_VAR ||
            !declaration->var_is_constexpr || !declaration->var_init ||
            !declaration->type ||
            !sema_constexpr_aggregate_type(declaration->type) ||
            !type_is_compatible(declaration->type, expected_type) ||
            constexpr_eval_depth >= 64) return false;
        ++constexpr_eval_depth;
        bool result = sema_constexpr_resolve_aggregate_expression(
            declaration->var_init, declaration->type,
            initializer, zero);
        --constexpr_eval_depth;
        return result;
    }
    if (expression->kind != EXPR_MEMBER && expression->kind != EXPR_INDEX) {
        return false;
    }
    base_type = expression->kind == EXPR_MEMBER
        ? expression->member_base ? expression->member_base->type : NULL
        : expression->index_base ? expression->index_base->type : NULL;
    if (!base_type || base_type->kind == TYPE_PTR ||
        !sema_constexpr_aggregate_type(base_type) ||
        !sema_constexpr_resolve_aggregate_expression(
            expression->kind == EXPR_MEMBER ? expression->member_base
                                            : expression->index_base,
            base_type, &source_initializer, &source_zero)) {
        return false;
    }
    if (source_zero) {
        *zero = true;
        return true;
    }
    if (expression->kind == EXPR_MEMBER) {
        if (!expression->member_field ||
            !sema_constexpr_select_aggregate_item(
                base_type, source_initializer, expression->member_field,
                -1, &selected, zero)) return false;
    } else {
        SemaConstexprScalar index_value;
        int64_t index;
        if (!base_type->base ||
            !sema_eval_constexpr_scalar_expr(
                expression->index_expr, NULL, 0, &index_value) ||
            index_value.is_floating) return false;
        index = index_value.integer_value;
        if (index < 0 || (base_type->array_len >= 0 &&
                          index >= base_type->array_len)) return false;
        if (!sema_constexpr_select_aggregate_item(
                base_type, source_initializer, NULL, index,
                &selected, zero)) return false;
    }
    if (*zero) {
        *initializer = NULL;
    } else {
        *initializer = selected;
    }
    return true;
}

static bool sema_eval_constexpr_scalar_object(
    Type* type, Expr* initializer, SemaConstexprBinding* bindings,
    int binding_count, SemaConstexprScalar* value) {
    ExprList* item;
    if (!type || !sema_constexpr_scalar_type(type) || !value) return false;
    if (!initializer) {
        memset(value, 0, sizeof(*value));
        value->type = type;
        value->is_floating = type->kind == TYPE_FLOAT ||
                             type->kind == TYPE_DOUBLE;
        return true;
    }
    if (initializer->kind == EXPR_COMPOUND) {
        if (sema_constexpr_zero_initializer(initializer)) {
            memset(value, 0, sizeof(*value));
            value->type = type;
            value->is_floating = type->kind == TYPE_FLOAT ||
                                 type->kind == TYPE_DOUBLE;
            return true;
        }
        item = initializer->compound_init;
        if (!item || item->next ||
            item->designator_kind != INIT_DESIGNATOR_NONE || !item->expr) {
            return false;
        }
        return sema_eval_constexpr_scalar_object(
            type, item->expr, bindings, binding_count, value);
    }
    return sema_eval_constexpr_scalar_expr(
        initializer, bindings, binding_count, value) &&
           sema_constexpr_scalar_convert(value, type, value);
}

static bool sema_validate_constexpr_object(
    Type* type, Expr* initializer) {
    ExprList* item;
    if (!type) return false;
    if (sema_constexpr_scalar_type(type)) {
        SemaConstexprScalar value;
        return sema_eval_constexpr_scalar_object(
            type, initializer, NULL, 0, &value) &&
               sema_constexpr_scalar_convert(&value, type, &value);
    }
    if (!sema_constexpr_aggregate_type(type)) return false;
    if (!initializer || sema_constexpr_zero_initializer(initializer)) {
        return initializer != NULL;
    }
    if (initializer->kind == EXPR_IDENT && initializer->ident_decl &&
        initializer->ident_decl->kind == DECL_VAR &&
        initializer->ident_decl->var_is_constexpr &&
        initializer->ident_decl->var_init && constexpr_eval_depth < 64) {
        ++constexpr_eval_depth;
        bool result = sema_validate_constexpr_object(
            type, initializer->ident_decl->var_init);
        --constexpr_eval_depth;
        return result;
    }
    if (initializer->kind == EXPR_CALL && initializer->call_func &&
        initializer->call_func->kind == EXPR_IDENT &&
        initializer->call_func->ident_decl &&
        initializer->call_func->ident_decl->kind == DECL_FUNC &&
        initializer->call_func->ident_decl->type &&
        initializer->call_func->ident_decl->type->kind == TYPE_FUNC &&
        sema_constexpr_aggregate_type(
            initializer->call_func->ident_decl->type->ret_type) &&
        type_is_compatible(
            type, initializer->call_func->ident_decl->type->ret_type)) {
        unsigned char* storage = ast_arena_alloc((size_t)type->size);
        if (!sema_eval_constexpr_aggregate_function(
            initializer->call_func->ident_decl, initializer->call_args,
            NULL, 0, storage, (size_t)type->size)) return false;
        return sema_constexpr_rebuild_object(
            type, storage, (size_t)type->size, initializer->loc) != NULL;
    }
    if (initializer->kind != EXPR_COMPOUND) return false;
    if (type->kind == TYPE_ARRAY) {
        int64_t cursor = 0;
        if (!type->base) return false;
        for (item = initializer->compound_init; item; item = item->next) {
            if (item->designator_kind == INIT_DESIGNATOR_FIELD) return false;
            if (item->designator_kind == INIT_DESIGNATOR_INDEX) {
                cursor = item->designator_index;
            }
            if (cursor < 0 || cursor >= type->array_len ||
                !sema_validate_constexpr_object(type->base, item->expr)) {
                return false;
            }
            if (cursor == INT64_MAX) return false;
            ++cursor;
        }
        return true;
    }
    {
        TypeField* cursor = type->fields;
        int initialized = 0;
        for (item = initializer->compound_init; item; item = item->next) {
            TypeField* field = cursor;
            if (item->designator_kind == INIT_DESIGNATOR_INDEX) return false;
            if (item->designator_kind == INIT_DESIGNATOR_FIELD) {
                field = initializer_field(type, item->designator_field);
            }
            if (!field || (type->kind == TYPE_UNION && initialized != 0 &&
                           item->designator_kind == INIT_DESIGNATOR_NONE) ||
                !sema_validate_constexpr_object(field->type, item->expr)) {
                return false;
            }
            cursor = field->next;
            ++initialized;
        }
        for (TypeField* field = type->fields; field; field = field->next) {
            if (field->initializer &&
                !sema_validate_constexpr_object(field->type,
                                                field->initializer)) {
                return false;
            }
        }
        return true;
    }
}

static bool sema_constexpr_scalar_binary(
    int expression_kind, const SemaConstexprScalar* left,
    const SemaConstexprScalar* right, Type* result_type,
    SemaConstexprScalar* output) {
    SemaConstexprScalar converted_left;
    SemaConstexprScalar converted_right;
    SemaConstexprScalar raw;
    double left_value;
    double right_value;
    Type* operation_type;
    bool comparison;

    if (!left || !right || !output || !result_type) {
        return false;
    }
    comparison = expression_kind == EXPR_EQ || expression_kind == EXPR_NE ||
                expression_kind == EXPR_LT || expression_kind == EXPR_GT ||
                expression_kind == EXPR_LE || expression_kind == EXPR_GE;

    if (left->is_pointer || right->is_pointer) {
        bool left_pointer = left->is_pointer;
        bool right_pointer = right->is_pointer;
        bool left_null = left_pointer && !left->pointer_declaration &&
                         left->pointer_offset == 0;
        bool right_null = right_pointer && !right->pointer_declaration &&
                          right->pointer_offset == 0;
        int64_t element_size;
        int64_t difference;

        if (comparison) {
            if (!left_pointer && left->is_floating) return false;
            if (!right_pointer && right->is_floating) return false;
            if (!left_pointer && left->integer_value == 0) {
                left_pointer = true;
                left_null = true;
            }
            if (!right_pointer && right->integer_value == 0) {
                right_pointer = true;
                right_null = true;
            }
            if (!left_pointer || !right_pointer) return false;
            if (expression_kind == EXPR_EQ || expression_kind == EXPR_NE) {
                bool equal = left_null && right_null;
                if (!left_null && !right_null) {
                    equal = left->pointer_declaration ==
                                right->pointer_declaration &&
                            left->pointer_offset == right->pointer_offset;
                }
                output->type = type_int;
                output->integer_value = expression_kind == EXPR_EQ
                    ? equal : !equal;
                output->floating_value = 0.0;
                output->is_floating = false;
                output->is_pointer = false;
                output->pointer_declaration = NULL;
                output->pointer_offset = 0;
                return true;
            }
            if (left_null || right_null ||
                left->pointer_declaration != right->pointer_declaration) {
                return false;
            }
            output->type = type_int;
            output->is_floating = false;
            output->is_pointer = false;
            output->pointer_declaration = NULL;
            output->pointer_offset = 0;
            switch (expression_kind) {
                case EXPR_LT:
                    output->integer_value = left->pointer_offset <
                                            right->pointer_offset;
                    break;
                case EXPR_GT:
                    output->integer_value = left->pointer_offset >
                                            right->pointer_offset;
                    break;
                case EXPR_LE:
                    output->integer_value = left->pointer_offset <=
                                            right->pointer_offset;
                    break;
                case EXPR_GE:
                    output->integer_value = left->pointer_offset >=
                                            right->pointer_offset;
                    break;
                default:
                    return false;
            }
            return true;
        }

        if (expression_kind == EXPR_SUB && left_pointer && right_pointer) {
            if (left->pointer_declaration != right->pointer_declaration ||
                (!left->pointer_declaration && left->pointer_offset == 0) ||
                (!right->pointer_declaration && right->pointer_offset == 0) ||
                !left->type || left->type->kind != TYPE_PTR ||
                !left->type->base || left->type->base->size <= 0) return false;
#if defined(__GNUC__) || defined(__clang__)
            if (__builtin_sub_overflow(left->pointer_offset,
                                       right->pointer_offset,
                                       &difference)) return false;
#else
            if ((right->pointer_offset < 0 &&
                 left->pointer_offset > INT64_MAX + right->pointer_offset) ||
                (right->pointer_offset > 0 &&
                 left->pointer_offset < INT64_MIN + right->pointer_offset)) {
                return false;
            }
            difference = left->pointer_offset - right->pointer_offset;
#endif
            element_size = left->type->base->size;
            if (difference % element_size != 0) return false;
            output->type = result_type;
            output->integer_value = difference / element_size;
            output->floating_value = 0.0;
            output->is_floating = false;
            output->is_pointer = false;
            output->pointer_declaration = NULL;
            output->pointer_offset = 0;
            return true;
        }
        if (expression_kind == EXPR_ADD || expression_kind == EXPR_SUB) {
            const SemaConstexprScalar* pointer = left_pointer ? left : right;
            const SemaConstexprScalar* integer_value = left_pointer ? right : left;
            int64_t elements;
            if (expression_kind == EXPR_SUB && !left_pointer) return false;
            if (!pointer->type || pointer->type->kind != TYPE_PTR ||
                !pointer->type->base || pointer->type->base->size <= 0 ||
                (!pointer->pointer_declaration && pointer->pointer_offset == 0) ||
                integer_value->is_pointer || integer_value->is_floating) {
                return false;
            }
            elements = integer_value->integer_value;
            if (expression_kind == EXPR_SUB) {
                if (elements == INT64_MIN) return false;
                elements = -elements;
            }
            if (!sema_constexpr_pointer_offset(
                    pointer->pointer_offset, elements,
                    pointer->type->base->size, &difference)) return false;
            output->type = result_type->kind == TYPE_PTR
                ? result_type : pointer->type;
            output->integer_value = 0;
            output->floating_value = 0.0;
            output->is_floating = false;
            output->is_pointer = true;
            output->pointer_declaration = pointer->pointer_declaration;
            output->pointer_offset = difference;
            return true;
        }
        return false;
    }
    operation_type = comparison ? type_common(left->type, right->type)
                                : result_type;
    if (!sema_constexpr_scalar_type(operation_type) ||
        !sema_constexpr_scalar_convert(left, operation_type,
                                       &converted_left) ||
        !sema_constexpr_scalar_convert(right, operation_type,
                                       &converted_right)) {
        return false;
    }
    if (operation_type->kind == TYPE_FLOAT ||
        operation_type->kind == TYPE_DOUBLE) {
        left_value = converted_left.floating_value;
        right_value = converted_right.floating_value;
        if (expression_kind == EXPR_EQ || expression_kind == EXPR_NE ||
            expression_kind == EXPR_LT || expression_kind == EXPR_GT ||
            expression_kind == EXPR_LE || expression_kind == EXPR_GE) {
            output->type = type_int;
            output->is_floating = false;
            output->is_pointer = false;
            output->pointer_declaration = NULL;
            output->pointer_offset = 0;
            switch (expression_kind) {
                case EXPR_EQ: output->integer_value = left_value == right_value; break;
                case EXPR_NE: output->integer_value = left_value != right_value; break;
                case EXPR_LT: output->integer_value = left_value < right_value; break;
                case EXPR_GT: output->integer_value = left_value > right_value; break;
                case EXPR_LE: output->integer_value = left_value <= right_value; break;
                case EXPR_GE: output->integer_value = left_value >= right_value; break;
                default: return false;
            }
            return true;
        }
        if (expression_kind == EXPR_DIV && right_value == 0.0) {
            return false;
        }
        raw.type = operation_type;
        raw.integer_value = 0;
        raw.is_floating = true;
        raw.is_pointer = false;
        raw.pointer_declaration = NULL;
        raw.pointer_offset = 0;
        switch (expression_kind) {
            case EXPR_ADD: raw.floating_value = left_value + right_value; break;
            case EXPR_SUB: raw.floating_value = left_value - right_value; break;
            case EXPR_MUL: raw.floating_value = left_value * right_value; break;
            case EXPR_DIV: raw.floating_value = left_value / right_value; break;
            default: return false;
        }
        return sema_constexpr_scalar_convert(&raw, operation_type, output);
    }
    if (converted_left.is_floating || converted_right.is_floating) {
        return false;
    }
    output->type = comparison ? type_int : result_type;
    output->is_floating = false;
    output->is_pointer = false;
    output->pointer_declaration = NULL;
    output->pointer_offset = 0;
    if (comparison) {
        uint64_t left_bits = sema_constexpr_integer_bits(&converted_left);
        uint64_t right_bits = sema_constexpr_integer_bits(&converted_right);
        int64_t left_signed = sema_constexpr_integer_signed(&converted_left);
        int64_t right_signed = sema_constexpr_integer_signed(&converted_right);
        if (operation_type->is_unsigned) {
            switch (expression_kind) {
                case EXPR_EQ: output->integer_value = left_bits == right_bits; break;
                case EXPR_NE: output->integer_value = left_bits != right_bits; break;
                case EXPR_LT: output->integer_value = left_bits < right_bits; break;
                case EXPR_GT: output->integer_value = left_bits > right_bits; break;
                case EXPR_LE: output->integer_value = left_bits <= right_bits; break;
                case EXPR_GE: output->integer_value = left_bits >= right_bits; break;
                default: return false;
            }
        } else {
            switch (expression_kind) {
                case EXPR_EQ: output->integer_value = left_signed == right_signed; break;
                case EXPR_NE: output->integer_value = left_signed != right_signed; break;
                case EXPR_LT: output->integer_value = left_signed < right_signed; break;
                case EXPR_GT: output->integer_value = left_signed > right_signed; break;
                case EXPR_LE: output->integer_value = left_signed <= right_signed; break;
                case EXPR_GE: output->integer_value = left_signed >= right_signed; break;
                default: return false;
            }
        }
        return true;
    }
    {
        uint64_t left_bits = sema_constexpr_integer_bits(&converted_left);
        uint64_t right_bits = sema_constexpr_integer_bits(&converted_right);
        uint64_t mask = sema_constexpr_integer_mask(result_type);
        if (result_type->is_unsigned) {
            switch (expression_kind) {
                case EXPR_ADD: output->integer_value = (int64_t)((left_bits + right_bits) & mask); return true;
                case EXPR_SUB: output->integer_value = (int64_t)((left_bits - right_bits) & mask); return true;
                case EXPR_MUL: output->integer_value = (int64_t)((left_bits * right_bits) & mask); return true;
                case EXPR_DIV:
                    if (right_bits == 0u) return false;
                    output->integer_value = (int64_t)(left_bits / right_bits);
                    return true;
                case EXPR_MOD:
                    if (right_bits == 0u) return false;
                    output->integer_value = (int64_t)(left_bits % right_bits);
                    return true;
                case EXPR_BITAND: output->integer_value = (int64_t)((left_bits & right_bits) & mask); return true;
                case EXPR_BITOR: output->integer_value = (int64_t)((left_bits | right_bits) & mask); return true;
                case EXPR_BITXOR: output->integer_value = (int64_t)((left_bits ^ right_bits) & mask); return true;
                case EXPR_LSHIFT:
                    if (right_bits >= (uint64_t)result_type->size * 8u) return false;
                    output->integer_value = (int64_t)((left_bits << (unsigned)right_bits) & mask);
                    return true;
                case EXPR_RSHIFT:
                    if (right_bits >= (uint64_t)result_type->size * 8u) return false;
                    output->integer_value = (int64_t)(left_bits >> (unsigned)right_bits);
                    return true;
                default: return false;
            }
        }
    }
    switch (expression_kind) {
        case EXPR_ADD:
            return sema_constexpr_add(converted_left.integer_value,
                                      converted_right.integer_value,
                                      &output->integer_value);
        case EXPR_SUB:
            return sema_constexpr_sub(converted_left.integer_value,
                                      converted_right.integer_value,
                                      &output->integer_value);
        case EXPR_MUL:
            return sema_constexpr_mul(converted_left.integer_value,
                                      converted_right.integer_value,
                                      &output->integer_value);
        case EXPR_DIV:
            if (converted_right.integer_value == 0 ||
                (converted_left.integer_value == INT64_MIN &&
                 converted_right.integer_value == -1)) return false;
            output->integer_value = converted_left.integer_value /
                                    converted_right.integer_value;
            return true;
        case EXPR_MOD:
            if (converted_right.integer_value == 0 ||
                (converted_left.integer_value == INT64_MIN &&
                 converted_right.integer_value == -1)) return false;
            output->integer_value = converted_left.integer_value %
                                    converted_right.integer_value;
            return true;
        case EXPR_BITAND:
            output->integer_value = converted_left.integer_value &
                                    converted_right.integer_value;
            return true;
        case EXPR_BITOR:
            output->integer_value = converted_left.integer_value |
                                    converted_right.integer_value;
            return true;
        case EXPR_BITXOR:
            output->integer_value = converted_left.integer_value ^
                                    converted_right.integer_value;
            return true;
        case EXPR_LSHIFT:
            if (sema_constexpr_integer_signed(&converted_right) < 0 ||
                sema_constexpr_integer_signed(&converted_right) >=
                    (int64_t)result_type->size * 8 ||
                sema_constexpr_integer_signed(&converted_left) < 0 ||
                (sema_constexpr_integer_signed(&converted_right) < 63 &&
                 sema_constexpr_integer_signed(&converted_left) >
                     (INT64_MAX >> sema_constexpr_integer_signed(&converted_right)))) {
                return false;
            }
            output->integer_value = converted_left.integer_value <<
                                    sema_constexpr_integer_signed(&converted_right);
            return true;
        case EXPR_RSHIFT:
            if (sema_constexpr_integer_signed(&converted_right) < 0 ||
                sema_constexpr_integer_signed(&converted_right) >=
                    (int64_t)result_type->size * 8) return false;
            output->integer_value = converted_left.integer_value >>
                                    sema_constexpr_integer_signed(&converted_right);
            return true;
        default:
            return false;
    }
}

static bool sema_constexpr_scalar_assign(
    int expression_kind, const SemaConstexprScalar* current,
    const SemaConstexprScalar* right, Type* variable_type,
    SemaConstexprScalar* assigned) {
    if (!current || !right || !variable_type || !assigned) return false;
    if (expression_kind == EXPR_ASSIGN) {
        return sema_constexpr_scalar_convert(right, variable_type, assigned);
    }
    return sema_constexpr_scalar_binary(
        expression_kind == EXPR_ADD_ASSIGN ? EXPR_ADD :
        expression_kind == EXPR_SUB_ASSIGN ? EXPR_SUB :
        expression_kind == EXPR_MUL_ASSIGN ? EXPR_MUL :
        expression_kind == EXPR_DIV_ASSIGN ? EXPR_DIV :
        expression_kind == EXPR_MOD_ASSIGN ? EXPR_MOD :
        expression_kind == EXPR_AND_ASSIGN ? EXPR_BITAND :
        expression_kind == EXPR_OR_ASSIGN ? EXPR_BITOR :
        expression_kind == EXPR_XOR_ASSIGN ? EXPR_BITXOR :
        expression_kind == EXPR_LSHIFT_ASSIGN ? EXPR_LSHIFT : EXPR_RSHIFT,
        current, right, variable_type, assigned);
}

static bool sema_eval_constexpr_scalar_statement(
    Stmt* statement, SemaConstexprBinding* bindings, int* binding_count,
    SemaConstexprScalar* value, SemaConstexprStatementResult* result);

static bool sema_constexpr_scalar_truth(const SemaConstexprScalar* value) {
    if (!value) return false;
    if (value->is_pointer) {
        return value->pointer_declaration != NULL || value->pointer_offset != 0;
    }
    return value->is_floating ? value->floating_value != 0.0
                              : value->integer_value != 0;
}

/* Aggregate-returning constexpr functions use the same target-layout byte
 * storage as aggregate locals.  Keeping this evaluator separate from the
 * scalar path makes a failed aggregate expression a real semantic failure;
 * it can never turn into a zero-valued recovery result. */
static bool sema_eval_constexpr_aggregate_statement(
    Stmt* statement, SemaConstexprBinding* bindings, int* binding_count,
    SemaConstexprScalar* value, SemaConstexprStatementResult* result,
    Type* return_type, unsigned char* return_storage, size_t return_size) {
    int saved_binding_count;

    if (!statement || !bindings || !binding_count || !value || !result ||
        !return_type || !return_storage) return false;
    *result = SEMA_CONSTEXPR_STMT_FALLTHROUGH;
    switch (statement->kind) {
        case STMT_NULL:
            return true;
        case STMT_EXPR:
            if (!statement->expr) return true;
            if (statement->expr->kind == EXPR_ASSIGN &&
                sema_constexpr_assign_object(
                    statement->expr, bindings, *binding_count)) {
                memset(value, 0, sizeof(*value));
                value->type = statement->expr->type;
                return true;
            }
            return sema_eval_constexpr_scalar_expr(
                statement->expr, bindings, *binding_count, value);
        case STMT_RETURN:
            if (!statement->return_val ||
                !sema_constexpr_materialize_object(
                    return_type, statement->return_val, bindings,
                    *binding_count, return_storage, return_size)) {
                return false;
            }
            *result = SEMA_CONSTEXPR_STMT_RETURNED;
            return true;
        case STMT_DECL: {
            Decl* declaration = statement->decl;
            if (!declaration || declaration->kind != DECL_VAR ||
                !declaration->name || *binding_count >= 64) return false;
            if (sema_constexpr_aggregate_type(declaration->type)) {
                unsigned char* object_bytes;
                if (declaration->type->size <= 0) return false;
                object_bytes = ast_arena_alloc(
                    (size_t)declaration->type->size);
                if (!sema_constexpr_materialize_object(
                        declaration->type, declaration->var_init, bindings,
                        *binding_count, object_bytes,
                        (size_t)declaration->type->size)) return false;
                memset(&bindings[*binding_count], 0,
                       sizeof(bindings[*binding_count]));
                bindings[*binding_count].declaration = declaration;
                bindings[*binding_count].type = declaration->type;
                bindings[*binding_count].object_bytes = object_bytes;
                bindings[*binding_count].object_size =
                    (size_t)declaration->type->size;
                bindings[*binding_count].is_object = true;
                ++*binding_count;
                memset(value, 0, sizeof(*value));
                value->type = declaration->type;
                return true;
            }
            {
                SemaConstexprScalar initializer;
                if (!declaration->var_init ||
                    !sema_constexpr_scalar_type(declaration->type) ||
                    !sema_eval_constexpr_scalar_expr(
                        declaration->var_init, bindings, *binding_count,
                        &initializer) ||
                    !sema_constexpr_scalar_convert(
                        &initializer, declaration->type, &initializer)) {
                    return false;
                }
                memset(&bindings[*binding_count], 0,
                       sizeof(bindings[*binding_count]));
                bindings[*binding_count].declaration = declaration;
                bindings[*binding_count].type = declaration->type;
                bindings[*binding_count].value = initializer.integer_value;
                bindings[*binding_count].floating_value =
                    initializer.floating_value;
                bindings[*binding_count].is_floating = initializer.is_floating;
                bindings[*binding_count].is_pointer = initializer.is_pointer;
                bindings[*binding_count].pointer_declaration =
                    initializer.pointer_declaration;
                bindings[*binding_count].pointer_offset =
                    initializer.pointer_offset;
                ++*binding_count;
                *value = initializer;
                return true;
            }
        }
        case STMT_BLOCK:
            saved_binding_count = *binding_count;
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                SemaConstexprStatementResult nested_result;
                if (!sema_eval_constexpr_aggregate_statement(
                        item->stmt, bindings, binding_count, value,
                        &nested_result, return_type, return_storage,
                        return_size)) {
                    *binding_count = saved_binding_count;
                    return false;
                }
                if (nested_result == SEMA_CONSTEXPR_STMT_RETURNED ||
                    nested_result == SEMA_CONSTEXPR_STMT_BREAK ||
                    nested_result == SEMA_CONSTEXPR_STMT_CONTINUE) {
                    *result = nested_result;
                    *binding_count = saved_binding_count;
                    return true;
                }
            }
            *binding_count = saved_binding_count;
            return true;
        case STMT_IF: {
            SemaConstexprScalar condition;
            Stmt* selected;
            if (!statement->if_cond ||
                !sema_eval_constexpr_scalar_expr(
                    statement->if_cond, bindings, *binding_count,
                    &condition)) return false;
            selected = sema_constexpr_scalar_truth(&condition)
                ? statement->if_then : statement->if_else;
            if (!selected) return true;
            return sema_eval_constexpr_aggregate_statement(
                selected, bindings, binding_count, value, result,
                return_type, return_storage, return_size);
        }
        case STMT_BREAK:
            *result = SEMA_CONSTEXPR_STMT_BREAK;
            return true;
        case STMT_CONTINUE:
            *result = SEMA_CONSTEXPR_STMT_CONTINUE;
            return true;
        case STMT_FOR: {
            SemaConstexprScalar condition;
            int saved_count = *binding_count;
            unsigned iteration;
            if (statement->for_init) {
                SemaConstexprStatementResult init_result;
                if (!sema_eval_constexpr_aggregate_statement(
                        statement->for_init, bindings, binding_count, value,
                        &init_result, return_type, return_storage,
                        return_size) ||
                    init_result != SEMA_CONSTEXPR_STMT_FALLTHROUGH) {
                    *binding_count = saved_count;
                    return false;
                }
            }
            for (iteration = 0u; iteration < 1000000u; ++iteration) {
                SemaConstexprStatementResult body_result =
                    SEMA_CONSTEXPR_STMT_FALLTHROUGH;
                if (statement->for_cond &&
                    (!sema_eval_constexpr_scalar_expr(
                        statement->for_cond, bindings, *binding_count,
                        &condition) ||
                     !sema_constexpr_scalar_truth(&condition))) break;
                if (statement->for_body &&
                    !sema_eval_constexpr_aggregate_statement(
                        statement->for_body, bindings, binding_count, value,
                        &body_result, return_type, return_storage,
                        return_size)) {
                    *binding_count = saved_count;
                    return false;
                }
                if (body_result == SEMA_CONSTEXPR_STMT_RETURNED) {
                    *result = body_result;
                    *binding_count = saved_count;
                    return true;
                }
                if (body_result == SEMA_CONSTEXPR_STMT_BREAK) break;
                if (statement->for_inc &&
                    !sema_eval_constexpr_scalar_expr(
                        statement->for_inc, bindings, *binding_count,
                        &condition)) {
                    *binding_count = saved_count;
                    return false;
                }
            }
            if (iteration == 1000000u) {
                *binding_count = saved_count;
                return false;
            }
            *binding_count = saved_count;
            return true;
        }
        case STMT_WHILE:
        case STMT_DO: {
            SemaConstexprScalar condition;
            unsigned iteration;
            bool do_body = statement->kind == STMT_DO;
            for (iteration = 0u; iteration < 1000000u; ++iteration) {
                SemaConstexprStatementResult body_result =
                    SEMA_CONSTEXPR_STMT_FALLTHROUGH;
                if (!do_body) {
                    if (!sema_eval_constexpr_scalar_expr(
                            statement->while_cond, bindings, *binding_count,
                            &condition)) return false;
                    if (!sema_constexpr_scalar_truth(&condition)) return true;
                }
                if (statement->while_body &&
                    !sema_eval_constexpr_aggregate_statement(
                        statement->while_body, bindings, binding_count, value,
                        &body_result, return_type, return_storage,
                        return_size)) return false;
                if (body_result == SEMA_CONSTEXPR_STMT_RETURNED) {
                    *result = body_result;
                    return true;
                }
                if (body_result == SEMA_CONSTEXPR_STMT_BREAK) return true;
                if (!sema_eval_constexpr_scalar_expr(
                        statement->while_cond, bindings, *binding_count,
                        &condition)) return false;
                if (!sema_constexpr_scalar_truth(&condition)) return true;
                do_body = false;
            }
            return false;
        }
        default:
            return false;
    }
}

static bool sema_eval_constexpr_aggregate_function(
    Decl* declaration, ExprList* args, SemaConstexprBinding* caller_bindings,
    int caller_binding_count, unsigned char* storage, size_t storage_size) {
    SemaConstexprBinding bindings[64];
    DeclList* parameter;
    ExprList* argument;
    SemaConstexprScalar argument_value;
    SemaConstexprScalar result;
    SemaConstexprStatementResult statement_result;
    int count = 0;

    memset(bindings, 0, sizeof(bindings));
    if (!declaration || !declaration->func_is_constexpr ||
        declaration->func_this_param || !declaration->type ||
        declaration->type->kind != TYPE_FUNC || declaration->type->variadic ||
        !sema_constexpr_aggregate_type(declaration->type->ret_type) ||
        !declaration->func_body || declaration->func_body->kind != STMT_BLOCK ||
        !storage || declaration->type->ret_type->size <= 0 ||
        (size_t)declaration->type->ret_type->size > storage_size ||
        constexpr_eval_depth >= 64) return false;

    parameter = declaration->func_params;
    argument = args;
    while (parameter && argument) {
        if (count == (int)(sizeof(bindings) / sizeof(bindings[0])) ||
            !parameter->decl || !parameter->decl->name) return false;
        bindings[count].declaration = parameter->decl;
        bindings[count].type = parameter->decl->type;
        if (sema_constexpr_scalar_type(parameter->decl->type)) {
            if (!sema_eval_constexpr_scalar_expr(
                    argument->expr, caller_bindings, caller_binding_count,
                    &argument_value) ||
                !sema_constexpr_scalar_convert(
                    &argument_value, parameter->decl->type, &argument_value)) {
                return false;
            }
            bindings[count].value = argument_value.integer_value;
            bindings[count].floating_value = argument_value.floating_value;
            bindings[count].is_floating = argument_value.is_floating;
            bindings[count].is_pointer = argument_value.is_pointer;
            bindings[count].pointer_declaration =
                argument_value.pointer_declaration;
            bindings[count].pointer_offset = argument_value.pointer_offset;
        } else if (sema_constexpr_aggregate_type(parameter->decl->type)) {
            if (parameter->decl->type->size <= 0) return false;
            bindings[count].object_bytes = ast_arena_alloc(
                (size_t)parameter->decl->type->size);
            if (!sema_constexpr_materialize_object(
                    parameter->decl->type, argument->expr,
                    caller_bindings, caller_binding_count,
                    bindings[count].object_bytes,
                    (size_t)parameter->decl->type->size)) return false;
            bindings[count].object_size = (size_t)parameter->decl->type->size;
            bindings[count].is_object = true;
        } else {
            return false;
        }
        ++count;
        parameter = parameter->next;
        argument = argument->next;
    }
    if (parameter || argument) return false;
    ++constexpr_eval_depth;
    bool evaluated = sema_eval_constexpr_aggregate_statement(
        declaration->func_body, bindings, &count, &result,
        &statement_result, declaration->type->ret_type, storage,
        storage_size);
    --constexpr_eval_depth;
    return evaluated && statement_result == SEMA_CONSTEXPR_STMT_RETURNED;
}

/* Convert a fully evaluated aggregate back into the normal initializer AST so
 * static storage and ordinary aggregate codegen share one representation.
 * Pointer-bearing results are intentionally left to the existing relocatable
 * initializer path; no host address is ever encoded into the object bytes. */
static Expr* sema_constexpr_rebuild_object(
    Type* type, const unsigned char* storage, size_t storage_size,
    SourceLoc loc) {
    Expr* expression;
    if (!type || !storage || type->size <= 0 ||
        (size_t)type->size > storage_size) return NULL;
    if (sema_constexpr_scalar_type(type)) {
        SemaConstexprScalar value;
        if (!sema_constexpr_load_scalar_bytes(
                storage, storage_size, type, &value) || value.is_pointer) {
            return NULL;
        }
        if (value.is_floating) {
            expression = expr_float(value.floating_value, loc);
            expression->type = type;
            return expression;
        }
        expression = expr_int(value.integer_value, loc);
        expression->type = type;
        return expression;
    }
    if (!sema_constexpr_aggregate_type(type) || type->kind == TYPE_UNION) {
        return NULL;
    }
    expression = expr_initializer_list(NULL, loc);
    expression->compound_type = type;
    expression->type = type;
    if (type->kind == TYPE_ARRAY) {
        if (!type->base || type->base->size <= 0) return NULL;
        for (int64_t index = 0; index < type->array_len; ++index) {
            size_t offset = (size_t)index * (size_t)type->base->size;
            Expr* item;
            if (offset > (size_t)type->size) return NULL;
            item = sema_constexpr_rebuild_object(
                type->base, storage + offset,
                (size_t)type->size - offset, loc);
            exprlist_append(&expression->compound_init, item);
        }
        return expression;
    }
    for (TypeField* field = type->fields; field; field = field->next) {
        size_t offset;
        Expr* item;
        if (field->is_bitfield || field->offset < 0 ||
            field->type->size <= 0) return NULL;
        offset = (size_t)field->offset;
        if (offset > (size_t)type->size ||
            (size_t)field->type->size > (size_t)type->size - offset) {
            return NULL;
        }
        item = sema_constexpr_rebuild_object(
            field->type, storage + offset,
            (size_t)type->size - offset, loc);
        if (!item) return NULL;
        exprlist_append(&expression->compound_init, item);
    }
    return expression;
}

static bool sema_fold_constexpr_aggregate_call(Expr* expression,
                                                Decl* declaration) {
    unsigned char* storage;
    Expr* replacement;
    if (!expression || !declaration || !declaration->type ||
        !sema_constexpr_aggregate_type(declaration->type->ret_type)) {
        return false;
    }
    storage = ast_arena_alloc((size_t)declaration->type->ret_type->size);
    if (!sema_eval_constexpr_aggregate_function(
            declaration, expression->call_args, NULL, 0, storage,
            (size_t)declaration->type->ret_type->size)) return false;
    replacement = sema_constexpr_rebuild_object(
        declaration->type->ret_type, storage,
        (size_t)declaration->type->ret_type->size, expression->loc);
    if (!replacement) return false;
    *expression = *replacement;
    return true;
}

static bool sema_eval_constexpr_scalar_statement(
    Stmt* statement, SemaConstexprBinding* bindings, int* binding_count,
    SemaConstexprScalar* value, SemaConstexprStatementResult* result) {
    int saved_binding_count;

    if (!statement || !bindings || !binding_count || !value || !result) {
        return false;
    }
    *result = SEMA_CONSTEXPR_STMT_FALLTHROUGH;
    switch (statement->kind) {
        case STMT_NULL:
            return true;
        case STMT_EXPR:
            if (!statement->expr) return true;
            if (statement->expr->kind == EXPR_ASSIGN &&
                sema_constexpr_assign_object(
                    statement->expr, bindings, *binding_count)) {
                memset(value, 0, sizeof(*value));
                value->type = statement->expr->type;
                return true;
            }
            return sema_eval_constexpr_scalar_expr(
                statement->expr, bindings, *binding_count, value);
        case STMT_RETURN:
            if (!statement->return_val ||
                !sema_eval_constexpr_scalar_expr(
                    statement->return_val, bindings, *binding_count, value)) {
                return false;
            }
            *result = SEMA_CONSTEXPR_STMT_RETURNED;
            return true;
        case STMT_DECL: {
            SemaConstexprScalar initializer;
            Decl* declaration = statement->decl;
            if (declaration && declaration->kind == DECL_VAR &&
                declaration->name && declaration->var_init &&
                sema_constexpr_aggregate_type(declaration->type)) {
                unsigned char* object_bytes;
                if (*binding_count >= 64 || declaration->type->size <= 0) {
                    return false;
                }
                object_bytes = ast_arena_alloc(
                    (size_t)declaration->type->size);
                if (!sema_constexpr_materialize_object(
                        declaration->type, declaration->var_init, bindings,
                        *binding_count, object_bytes,
                        (size_t)declaration->type->size)) return false;
                bindings[*binding_count].declaration = declaration;
                bindings[*binding_count].type = declaration->type;
                bindings[*binding_count].value = 0;
                bindings[*binding_count].floating_value = 0.0;
                bindings[*binding_count].is_floating = false;
                bindings[*binding_count].is_pointer = false;
                bindings[*binding_count].pointer_declaration = NULL;
                bindings[*binding_count].pointer_offset = 0;
                bindings[*binding_count].object_bytes = object_bytes;
                bindings[*binding_count].object_size =
                    (size_t)declaration->type->size;
                bindings[*binding_count].is_object = true;
                ++*binding_count;
                memset(value, 0, sizeof(*value));
                value->type = declaration->type;
                return true;
            }
            if (!declaration || declaration->kind != DECL_VAR ||
                !declaration->name || !sema_constexpr_scalar_type(
                    declaration->type) || !declaration->var_init ||
                *binding_count >= 64 ||
                !sema_eval_constexpr_scalar_expr(
                    declaration->var_init, bindings, *binding_count,
                    &initializer) ||
                !sema_constexpr_scalar_convert(
                    &initializer, declaration->type, &initializer)) {
                return false;
            }
            bindings[*binding_count].declaration = declaration;
            bindings[*binding_count].type = declaration->type;
            bindings[*binding_count].value = initializer.integer_value;
            bindings[*binding_count].floating_value = initializer.floating_value;
            bindings[*binding_count].is_floating = initializer.is_floating;
            bindings[*binding_count].is_pointer = initializer.is_pointer;
            bindings[*binding_count].pointer_declaration =
                initializer.pointer_declaration;
            bindings[*binding_count].pointer_offset = initializer.pointer_offset;
            bindings[*binding_count].object_bytes = NULL;
            bindings[*binding_count].object_size = 0u;
            bindings[*binding_count].is_object = false;
            ++*binding_count;
            *value = initializer;
            return true;
        }
        case STMT_BLOCK:
            saved_binding_count = *binding_count;
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                SemaConstexprStatementResult nested_result;
                if (!sema_eval_constexpr_scalar_statement(
                        item->stmt, bindings, binding_count, value,
                        &nested_result)) {
                    *binding_count = saved_binding_count;
                    return false;
                }
                if (nested_result == SEMA_CONSTEXPR_STMT_RETURNED ||
                    nested_result == SEMA_CONSTEXPR_STMT_BREAK ||
                    nested_result == SEMA_CONSTEXPR_STMT_CONTINUE) {
                    *result = nested_result;
                    *binding_count = saved_binding_count;
                    return true;
                }
            }
            *binding_count = saved_binding_count;
            return true;
        case STMT_IF: {
            SemaConstexprScalar condition;
            Stmt* selected;
            if (!statement->if_cond ||
                !sema_eval_constexpr_scalar_expr(
                    statement->if_cond, bindings, *binding_count,
                    &condition)) return false;
            selected = sema_constexpr_scalar_truth(&condition)
                ? statement->if_then : statement->if_else;
            if (!selected) return true;
            return sema_eval_constexpr_scalar_statement(
                selected, bindings, binding_count, value, result);
        }
        case STMT_BREAK:
            *result = SEMA_CONSTEXPR_STMT_BREAK;
            return true;
        case STMT_CONTINUE:
            *result = SEMA_CONSTEXPR_STMT_CONTINUE;
            return true;
        case STMT_FOR: {
            SemaConstexprScalar condition;
            int saved_count = *binding_count;
            unsigned iteration;
            if (statement->for_init) {
                SemaConstexprStatementResult init_result;
                if (!sema_eval_constexpr_scalar_statement(
                        statement->for_init, bindings, binding_count, value,
                        &init_result) ||
                    init_result != SEMA_CONSTEXPR_STMT_FALLTHROUGH) {
                    *binding_count = saved_count;
                    return false;
                }
            }
            for (iteration = 0u; iteration < 1000000u; ++iteration) {
                SemaConstexprStatementResult body_result =
                    SEMA_CONSTEXPR_STMT_FALLTHROUGH;
                if (statement->for_cond &&
                    !sema_eval_constexpr_scalar_expr(
                        statement->for_cond, bindings, *binding_count,
                        &condition)) {
                    *binding_count = saved_count;
                    return false;
                }
                if (statement->for_cond &&
                    !sema_constexpr_scalar_truth(&condition)) break;
                if (statement->for_body &&
                    !sema_eval_constexpr_scalar_statement(
                        statement->for_body, bindings, binding_count, value,
                        &body_result)) {
                    *binding_count = saved_count;
                    return false;
                }
                if (body_result == SEMA_CONSTEXPR_STMT_RETURNED) {
                    *result = body_result;
                    *binding_count = saved_count;
                    return true;
                }
                if (body_result == SEMA_CONSTEXPR_STMT_BREAK) break;
                if (statement->for_inc &&
                    !sema_eval_constexpr_scalar_expr(
                        statement->for_inc, bindings, *binding_count,
                        value)) {
                    *binding_count = saved_count;
                    return false;
                }
            }
            if (iteration == 1000000u) {
                *binding_count = saved_count;
                return false;
            }
            *binding_count = saved_count;
            return true;
        }
        case STMT_WHILE:
        case STMT_DO: {
            SemaConstexprScalar condition;
            unsigned iteration;
            bool do_body = statement->kind == STMT_DO;
            for (iteration = 0u; iteration < 1000000u; ++iteration) {
                SemaConstexprStatementResult body_result =
                    SEMA_CONSTEXPR_STMT_FALLTHROUGH;
                if (!do_body) {
                    if (!sema_eval_constexpr_scalar_expr(
                            statement->while_cond, bindings, *binding_count,
                            &condition)) return false;
                    if (!sema_constexpr_scalar_truth(&condition)) return true;
                }
                if (statement->while_body &&
                    !sema_eval_constexpr_scalar_statement(
                        statement->while_body, bindings, binding_count, value,
                        &body_result)) return false;
                if (body_result == SEMA_CONSTEXPR_STMT_RETURNED) {
                    *result = body_result;
                    return true;
                }
                if (body_result == SEMA_CONSTEXPR_STMT_BREAK) return true;
                if (!sema_eval_constexpr_scalar_expr(
                        statement->while_cond, bindings, *binding_count,
                        &condition)) return false;
                if (!sema_constexpr_scalar_truth(&condition)) return true;
                do_body = false;
            }
            return false;
        }
        default:
            return false;
    }
}

static bool sema_eval_constexpr_scalar_function(
    Decl* declaration, ExprList* args, SemaConstexprBinding* caller_bindings,
    int caller_binding_count, SemaConstexprScalar* value) {
    SemaConstexprBinding bindings[64];
    DeclList* parameter;
    ExprList* argument;
    SemaConstexprScalar argument_value;
    SemaConstexprScalar result;
    SemaConstexprStatementResult statement_result;
    int count = 0;

    memset(bindings, 0, sizeof(bindings));

    if (!declaration || !value || !declaration->func_is_constexpr ||
        declaration->func_this_param || !declaration->type ||
        declaration->type->kind != TYPE_FUNC || declaration->type->variadic ||
        !sema_constexpr_scalar_type(declaration->type->ret_type) ||
        !declaration->func_body || declaration->func_body->kind != STMT_BLOCK) {
        return false;
    }
    parameter = declaration->func_params;
    argument = args;
    while (parameter && argument) {
        if (count == (int)(sizeof(bindings) / sizeof(bindings[0])) ||
            !parameter->decl || !parameter->decl->name) {
            return false;
        }
        bindings[count].declaration = parameter->decl;
        bindings[count].type = parameter->decl->type;
        bindings[count].object_bytes = NULL;
        bindings[count].object_size = 0u;
        bindings[count].is_object = false;
        if (sema_constexpr_scalar_type(parameter->decl->type)) {
            if (!sema_eval_constexpr_scalar_expr(
                    argument->expr, caller_bindings, caller_binding_count,
                    &argument_value) ||
                !sema_constexpr_scalar_convert(
                    &argument_value, parameter->decl->type,
                    &argument_value)) return false;
            bindings[count].value = argument_value.integer_value;
            bindings[count].floating_value = argument_value.floating_value;
            bindings[count].is_floating = argument_value.is_floating;
            bindings[count].is_pointer = argument_value.is_pointer;
            bindings[count].pointer_declaration =
                argument_value.pointer_declaration;
            bindings[count].pointer_offset = argument_value.pointer_offset;
        } else if (sema_constexpr_aggregate_type(parameter->decl->type)) {
            if (parameter->decl->type->size <= 0) return false;
            bindings[count].object_bytes = ast_arena_alloc(
                (size_t)parameter->decl->type->size);
            if (!sema_constexpr_materialize_object(
                    parameter->decl->type, argument->expr,
                    caller_bindings, caller_binding_count,
                    bindings[count].object_bytes,
                    (size_t)parameter->decl->type->size)) return false;
            bindings[count].object_size = (size_t)parameter->decl->type->size;
            bindings[count].is_object = true;
        } else {
            return false;
        }
        ++count;
        parameter = parameter->next;
        argument = argument->next;
    }
    if (parameter || argument || constexpr_eval_depth >= 64) return false;
    ++constexpr_eval_depth;
    if (!sema_eval_constexpr_scalar_statement(
            declaration->func_body, bindings, &count, &result,
            &statement_result)) {
        --constexpr_eval_depth;
        return false;
    }
    --constexpr_eval_depth;
    return statement_result == SEMA_CONSTEXPR_STMT_RETURNED &&
           sema_constexpr_scalar_convert(
               &result, declaration->type->ret_type, value);
}

static bool sema_eval_constexpr_scalar_expr(
    Expr* expression, SemaConstexprBinding* bindings,
    int binding_count, SemaConstexprScalar* value) {
    SemaConstexprScalar left;
    SemaConstexprScalar right;
    Type* result_type;
    Type* measured;
    int64_t integer;
    int binding_index;

    if (!expression || !value) return false;
    memset(value, 0, sizeof(*value));
    switch (expression->kind) {
        case EXPR_INT_LIT:
            if (expression->is_cxx_nullptr ||
                (expression->type && expression->type->kind == TYPE_NULLPTR)) {
                value->type = expression->type ? expression->type : type_nullptr;
                value->is_pointer = true;
                return true;
            }
            value->type = expression->type ? expression->type : type_int;
            value->integer_value = expression->int_val;
            value->is_floating = false;
            return true;
        case EXPR_CHAR_LIT:
            value->type = type_int;
            value->integer_value = (unsigned char)expression->char_val;
            value->is_floating = false;
            return true;
        case EXPR_FLOAT_LIT:
            value->type = expression->type &&
                (expression->type->kind == TYPE_FLOAT ||
                 expression->type->kind == TYPE_DOUBLE)
                ? expression->type : type_double;
            value->floating_value = expression->float_val;
            value->is_floating = true;
            return isfinite(value->floating_value);
        case EXPR_IDENT:
            binding_index = sema_constexpr_binding_index(
                expression, bindings, binding_count);
            if (binding_index >= 0) {
                value->type = bindings[binding_index].type;
                value->integer_value = bindings[binding_index].value;
                value->floating_value = bindings[binding_index].floating_value;
                value->is_floating = bindings[binding_index].is_floating;
                value->is_pointer = bindings[binding_index].is_pointer;
                value->pointer_declaration =
                    bindings[binding_index].pointer_declaration;
                value->pointer_offset = bindings[binding_index].pointer_offset;
                return true;
            }
            if (expression->ident_decl &&
                expression->ident_decl->kind == DECL_ENUM_CONST) {
                value->type = type_int;
                value->integer_value = expression->ident_decl->enum_val;
                return true;
            }
            if (expression->ident_decl &&
                expression->ident_decl->kind == DECL_VAR &&
                expression->ident_decl->var_is_constexpr &&
                expression->ident_decl->var_init &&
                sema_constexpr_scalar_type(expression->ident_decl->type) &&
                constexpr_eval_depth < 64) {
                ++constexpr_eval_depth;
                bool result = sema_eval_constexpr_scalar_expr(
                    expression->ident_decl->var_init, bindings,
                    binding_count, value);
                --constexpr_eval_depth;
                return result && sema_constexpr_scalar_convert(
                    value, expression->ident_decl->type, value);
            }
            if (expression->ident_decl &&
                expression->ident_decl->kind == DECL_FUNC) {
                value->type = type_ptr(expression->ident_decl->type);
                value->is_pointer = true;
                value->pointer_declaration = expression->ident_decl;
                value->pointer_offset = 0;
                return true;
            }
            return false;
        case EXPR_COMPOUND:
            return sema_eval_constexpr_scalar_object(
                expression->compound_type ? expression->compound_type
                                           : expression->type,
                expression, bindings, binding_count, value);
        case EXPR_MEMBER: {
            Expr* initializer;
            bool zero;
            Type* base_type = expression->member_base
                ? expression->member_base->type : NULL;
            if (!base_type || !expression->member_field) return false;
            if (bindings && sema_constexpr_load_binding_scalar(
                    expression, bindings, binding_count, value)) {
                return true;
            }
            if (!sema_constexpr_resolve_aggregate_expression(
                    expression->member_base, base_type,
                    &initializer, &zero)) return false;
            if (zero) initializer = NULL;
            else if (!sema_constexpr_select_aggregate_item(
                         base_type, initializer, expression->member_field,
                         -1, &initializer, &zero)) return false;
            if (zero) initializer = NULL;
            return sema_eval_constexpr_scalar_object(
                expression->member_field->type, initializer,
                bindings, binding_count, value);
        }
        case EXPR_INDEX: {
            SemaConstexprScalar index_value;
            Expr* initializer;
            bool zero;
            Type* base_type = expression->index_base
                ? expression->index_base->type : NULL;
            int64_t index;
            if (bindings && sema_constexpr_load_binding_scalar(
                    expression, bindings, binding_count, value)) {
                return true;
            }
            if (!base_type || base_type->kind != TYPE_ARRAY ||
                !base_type->base ||
                !sema_eval_constexpr_scalar_expr(
                    expression->index_expr, bindings, binding_count,
                    &index_value) || index_value.is_floating) return false;
            index = index_value.integer_value;
            if (index < 0 || (base_type->array_len >= 0 &&
                              index >= base_type->array_len) ||
                !sema_constexpr_resolve_aggregate_expression(
                    expression->index_base, base_type,
                    &initializer, &zero)) return false;
            if (zero) initializer = NULL;
            else if (!sema_constexpr_select_aggregate_item(
                         base_type, initializer, NULL, index,
                         &initializer, &zero)) return false;
            if (zero) initializer = NULL;
            return sema_eval_constexpr_scalar_object(
                base_type->base, initializer, bindings,
                binding_count, value);
        }
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
        case EXPR_RSHIFT_ASSIGN: {
            SemaConstexprScalar current;
            SemaConstexprScalar right;
            SemaConstexprScalar assigned;
            int object_binding_index;
            size_t object_offset;
            Type* object_type;
            if (bindings && sema_constexpr_binding_lvalue(
                    expression->binary_lhs, bindings, binding_count,
                    &object_binding_index, &object_offset, &object_type)) {
                if (!sema_constexpr_scalar_type(object_type) ||
                    !sema_constexpr_load_binding_scalar(
                        expression->binary_lhs, bindings, binding_count,
                        &current) ||
                    !sema_eval_constexpr_scalar_expr(
                        expression->binary_rhs, bindings, binding_count,
                        &right) ||
                    !sema_constexpr_scalar_assign(
                        expression->kind, &current, &right, object_type,
                        &assigned) ||
                    !sema_constexpr_store_binding_scalar(
                        expression->binary_lhs, bindings, binding_count,
                        &assigned, &assigned)) return false;
                *value = assigned;
                return true;
            }
            binding_index = sema_constexpr_binding_index(
                expression->binary_lhs, bindings, binding_count);
            if (binding_index < 0 ||
                !sema_eval_constexpr_scalar_expr(
                    expression->binary_rhs, bindings, binding_count,
                    &right)) return false;
            current.type = bindings[binding_index].type;
            current.integer_value = bindings[binding_index].value;
            current.floating_value = bindings[binding_index].floating_value;
            current.is_floating = bindings[binding_index].is_floating;
            current.is_pointer = bindings[binding_index].is_pointer;
            current.pointer_declaration =
                bindings[binding_index].pointer_declaration;
            current.pointer_offset = bindings[binding_index].pointer_offset;
            if (!sema_constexpr_scalar_assign(
                    expression->kind, &current, &right,
                    bindings[binding_index].type, &assigned)) return false;
            bindings[binding_index].value = assigned.integer_value;
            bindings[binding_index].floating_value = assigned.floating_value;
            bindings[binding_index].is_floating = assigned.is_floating;
            bindings[binding_index].type = assigned.type;
            bindings[binding_index].is_pointer = assigned.is_pointer;
            bindings[binding_index].pointer_declaration =
                assigned.pointer_declaration;
            bindings[binding_index].pointer_offset = assigned.pointer_offset;
            *value = assigned;
            return true;
        }
        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC: {
            SemaConstexprScalar current;
            SemaConstexprScalar one;
            SemaConstexprScalar updated;
            int object_binding_index;
            size_t object_offset;
            Type* object_type;
            if (bindings && sema_constexpr_binding_lvalue(
                    expression->unary_operand, bindings, binding_count,
                    &object_binding_index, &object_offset, &object_type)) {
                if (!sema_constexpr_scalar_type(object_type) ||
                    !sema_constexpr_load_binding_scalar(
                        expression->unary_operand, bindings, binding_count,
                        &current)) return false;
                memset(&one, 0, sizeof(one));
                one.type = type_int;
                one.integer_value = 1;
                if (!sema_constexpr_scalar_assign(
                        expression->kind == EXPR_PREINC ||
                        expression->kind == EXPR_POSTINC ? EXPR_ADD_ASSIGN
                                                         : EXPR_SUB_ASSIGN,
                        &current, &one, object_type, &updated) ||
                    !sema_constexpr_store_binding_scalar(
                        expression->unary_operand, bindings, binding_count,
                        &updated, &updated)) return false;
                *value = expression->kind == EXPR_POSTINC ||
                         expression->kind == EXPR_POSTDEC ? current : updated;
                return true;
            }
            binding_index = sema_constexpr_binding_index(
                expression->unary_operand, bindings, binding_count);
            if (binding_index < 0) return false;
            current.type = bindings[binding_index].type;
            current.integer_value = bindings[binding_index].value;
            current.floating_value = bindings[binding_index].floating_value;
            current.is_floating = bindings[binding_index].is_floating;
            current.is_pointer = bindings[binding_index].is_pointer;
            current.pointer_declaration =
                bindings[binding_index].pointer_declaration;
            current.pointer_offset = bindings[binding_index].pointer_offset;
            memset(&one, 0, sizeof(one));
            one.type = type_int;
            one.integer_value = 1;
            if (!sema_constexpr_scalar_assign(
                    expression->kind == EXPR_PREINC ||
                    expression->kind == EXPR_POSTINC ? EXPR_ADD_ASSIGN
                                                     : EXPR_SUB_ASSIGN,
                    &current, &one, bindings[binding_index].type,
                    &updated)) return false;
            bindings[binding_index].value = updated.integer_value;
            bindings[binding_index].floating_value = updated.floating_value;
            bindings[binding_index].is_floating = updated.is_floating;
            bindings[binding_index].type = updated.type;
            bindings[binding_index].is_pointer = updated.is_pointer;
            bindings[binding_index].pointer_declaration =
                updated.pointer_declaration;
            bindings[binding_index].pointer_offset = updated.pointer_offset;
            if (expression->kind == EXPR_POSTINC ||
                expression->kind == EXPR_POSTDEC) {
                *value = current;
            } else {
                *value = updated;
            }
            return true;
        }
        case EXPR_CAST:
            if (!sema_eval_constexpr_scalar_expr(expression->cast_expr,
                                                  bindings, binding_count,
                                                  &left)) return false;
            return sema_constexpr_scalar_convert(&left,
                                                 expression->cast_type, value);
        case EXPR_ADDR: {
            Decl* declaration = NULL;
            int64_t offset = 0;
            if (!sema_constexpr_address_target(
                    expression->unary_operand, &declaration, &offset)) {
                return false;
            }
            value->type = expression->type;
            value->is_pointer = true;
            value->pointer_declaration = declaration;
            value->pointer_offset = offset;
            return true;
        }
        case EXPR_NEG:
            if (!sema_eval_constexpr_scalar_expr(expression->unary_operand,
                                                  bindings, binding_count,
                                                  &left)) return false;
            if (left.is_floating) {
                *value = left;
                value->floating_value = -left.floating_value;
                return true;
            }
            result_type = expression->type ? expression->type : left.type;
            if (!sema_constexpr_scalar_convert(&left, result_type, &left)) {
                return false;
            }
            *value = left;
            if (result_type->is_unsigned) {
                value->integer_value = (int64_t)(
                    (UINT64_C(0) - sema_constexpr_integer_bits(&left)) &
                    sema_constexpr_integer_mask(result_type));
            } else {
                int64_t signed_value = sema_constexpr_integer_signed(&left);
                if (signed_value == INT64_MIN) return false;
                value->integer_value = -signed_value;
            }
            return true;
        case EXPR_NOT:
            if (!sema_eval_constexpr_scalar_expr(expression->unary_operand,
                                                  bindings, binding_count,
                                                  &left)) return false;
            value->type = type_int;
            value->integer_value = !sema_constexpr_scalar_truth(&left);
            return true;
        case EXPR_BITNOT:
            if (!sema_eval_constexpr_scalar_expr(expression->unary_operand,
                                                  bindings, binding_count,
                                                  &left) ||
                left.is_floating) return false;
            result_type = expression->type ? expression->type : left.type;
            if (!sema_constexpr_scalar_convert(&left, result_type, &left)) {
                return false;
            }
            value->type = result_type;
            value->is_floating = false;
            value->integer_value = (int64_t)(
                ~sema_constexpr_integer_bits(&left) &
                sema_constexpr_integer_mask(result_type));
            return true;
        case EXPR_SIZEOF:
        case EXPR_ALIGNOF:
            measured = expression->sizeof_type
                ? expression->sizeof_type
                : (expression->unary_operand
                    ? expression->unary_operand->type : NULL);
            if (measured == NULL ||
                (expression->kind == EXPR_SIZEOF
                    ? measured->size <= 0 : measured->align <= 0)) return false;
            value->type = type_ulong;
            value->integer_value = expression->kind == EXPR_SIZEOF
                ? measured->size : measured->align;
            return true;
        case EXPR_COND:
            if (!sema_eval_constexpr_scalar_expr(expression->cond_test,
                                                  bindings, binding_count,
                                                  &left)) return false;
            if (left.is_floating ? left.floating_value != 0.0
                                 : left.integer_value != 0) {
                return sema_eval_constexpr_scalar_expr(
                    expression->cond_then, bindings, binding_count, value);
            }
            return sema_eval_constexpr_scalar_expr(
                expression->cond_else, bindings, binding_count, value);
        case EXPR_COMMA:
            if (!sema_eval_constexpr_scalar_expr(expression->binary_lhs,
                                                  bindings, binding_count,
                                                  &left)) return false;
            return sema_eval_constexpr_scalar_expr(
                expression->binary_rhs, bindings, binding_count, value);
        case EXPR_AND:
        case EXPR_OR:
            if (!sema_eval_constexpr_scalar_expr(expression->binary_lhs,
                                                  bindings, binding_count,
                                                  &left)) return false;
            integer = left.is_floating ? left.floating_value != 0.0
                                       : left.integer_value != 0;
            if ((expression->kind == EXPR_AND && !integer) ||
                (expression->kind == EXPR_OR && integer)) {
                value->type = type_int;
                value->integer_value = expression->kind == EXPR_OR;
                return true;
            }
            if (!sema_eval_constexpr_scalar_expr(expression->binary_rhs,
                                                  bindings, binding_count,
                                                  &right)) return false;
            value->type = type_int;
            value->integer_value = right.is_floating
                ? right.floating_value != 0.0 : right.integer_value != 0;
            return true;
        case EXPR_CALL:
            if (!expression->call_func ||
                expression->call_func->kind != EXPR_IDENT ||
                !expression->call_func->ident_decl ||
                expression->call_func->ident_decl->kind != DECL_FUNC) {
                return false;
            }
            return sema_eval_constexpr_scalar_function(
                expression->call_func->ident_decl, expression->call_args,
                bindings, binding_count, value);
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
            if (!sema_eval_constexpr_scalar_expr(expression->binary_lhs,
                                                  bindings, binding_count,
                                                  &left) ||
                !sema_eval_constexpr_scalar_expr(expression->binary_rhs,
                                                  bindings, binding_count,
                                                  &right)) return false;
            if (left.is_floating || right.is_floating) {
                double left_value = left.is_floating
                    ? left.floating_value : (double)left.integer_value;
                double right_value = right.is_floating
                    ? right.floating_value : (double)right.integer_value;
                if ((expression->kind == EXPR_DIV) && right_value == 0.0) {
                    return false;
                }
                if (expression->kind == EXPR_EQ || expression->kind == EXPR_NE ||
                    expression->kind == EXPR_LT || expression->kind == EXPR_GT ||
                    expression->kind == EXPR_LE || expression->kind == EXPR_GE) {
                    value->type = type_int;
                    switch (expression->kind) {
                        case EXPR_EQ: value->integer_value = left_value == right_value; break;
                        case EXPR_NE: value->integer_value = left_value != right_value; break;
                        case EXPR_LT: value->integer_value = left_value < right_value; break;
                        case EXPR_GT: value->integer_value = left_value > right_value; break;
                        case EXPR_LE: value->integer_value = left_value <= right_value; break;
                        case EXPR_GE: value->integer_value = left_value >= right_value; break;
                        default: return false;
                    }
                    return true;
                }
                result_type = expression->type &&
                    (expression->type->kind == TYPE_FLOAT ||
                     expression->type->kind == TYPE_DOUBLE)
                    ? expression->type : type_double;
                value->type = result_type;
                value->is_floating = true;
                switch (expression->kind) {
                    case EXPR_ADD: value->floating_value = left_value + right_value; break;
                    case EXPR_SUB: value->floating_value = left_value - right_value; break;
                    case EXPR_MUL: value->floating_value = left_value * right_value; break;
                    case EXPR_DIV: value->floating_value = left_value / right_value; break;
                    default: return false;
                }
                return isfinite(value->floating_value);
            }
            return sema_constexpr_scalar_binary(
                expression->kind, &left, &right,
                expression->type ? expression->type : type_int, value);
        case EXPR_MOD:
        case EXPR_BITAND:
        case EXPR_BITOR:
        case EXPR_BITXOR:
        case EXPR_LSHIFT:
        case EXPR_RSHIFT:
            if (!sema_eval_constexpr_scalar_expr(expression->binary_lhs,
                                                  bindings, binding_count,
                                                  &left) ||
                !sema_eval_constexpr_scalar_expr(expression->binary_rhs,
                                                  bindings, binding_count,
                                                  &right) ||
                left.is_floating || right.is_floating) return false;
            return sema_constexpr_scalar_binary(
                expression->kind, &left, &right,
                expression->type ? expression->type : type_int, value);
        default:
            return false;
    }
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

static Symbol* sema_cxx_operator_function(const char* name,
                                          ExprList* arguments) {
    Symbol* function;
    if (!name) return NULL;
    function = sema_cxx_lookup_name(name);
    if (!function && arguments) function = sema_cxx_adl_lookup(name, arguments);
    return function && function->kind == SYM_FUNC && function->decl
        ? function : NULL;
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
            method->cxx_access == ACCESS_PUBLIC &&
            ((method->field && method->kind != TYPE_METHOD_FUNCTION) ||
             (method->kind == TYPE_METHOD_FUNCTION &&
              method->function_decl && !method->function_decl->func_params))) {
            return method;
        }
    }
    return NULL;
}

/* C++ explicit operator bool participates in contextual conversions without
 * becoming a general implicit conversion.  Both validated field delegates
 * and ordinary conversion functions use the same expression path here. */
static Expr* sema_contextual_bool(Expr* expression) {
    Type* type;
    Type* value_type;
    TypeMethod* method;
    Expr* member;
    Expr* call;
    if (!expression) return expression;
    type = sema_expr(expression);
    value_type = generic_selection_type(type);
    if (sema_is_scoped_enum(value_type)) {
        rcc_error(expression->loc,
                  "scoped enum is not implicitly convertible to bool");
        return expression;
    }
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
    if (method->kind == TYPE_METHOD_FUNCTION) {
        sema_expr(call);
        return call;
    }
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
    if (sema_is_scoped_enum(source) || sema_is_scoped_enum(target)) {
        return type_is_compatible(source, target) ? 0 : -1;
    }
    if (cxx_same_parameter_type(source, target, true)) return 0;

    if (rcc_parser_is_cxx_mode() &&
        (source->kind == TYPE_STRUCT || source->kind == TYPE_UNION)) {
        bool ambiguous = false;
        if (sema_find_cxx_conversion_method(source, target, &ambiguous)) {
            return 3; /* user-defined conversion */
        }
        if (ambiguous) return -1;
    }

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
        if (sema_cxx_pointer_conversion(source, target, NULL)) return 2;
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

/* const_cast changes cv-qualification only; it is not a general pointer or
 * reference conversion.  Keep the structural check independent of the
 * ordinary compatibility predicate so a cast cannot silently change the
 * pointed-to object type or an ABI-relevant integer signedness. */
static bool cxx_const_cast_similar(const Type* source, const Type* target,
                                   unsigned depth) {
    if (!source || !target || depth >= 32u) return false;
    /* The expression type of a reference is its referred-to object type in
     * sema, while a named cast target retains the reference wrapper.  Strip
     * those wrappers before comparing the cv-qualified object shape; the
     * reference category is part of the cast syntax, not the object type
     * being cv-adjusted. */
    if (source->is_reference) source = source->base;
    if (target->is_reference) target = target->base;
    if (!source || !target) return false;
    if (source->kind != target->kind || source->is_unsigned != target->is_unsigned ||
        source->size != target->size) return false;
    if (source->kind == TYPE_PTR || source->kind == TYPE_ARRAY) {
        return cxx_const_cast_similar(source->base, target->base,
                                      depth + 1u);
    }
    if (source->kind == TYPE_STRUCT || source->kind == TYPE_UNION) {
        return type_is_compatible((Type*)source, (Type*)target);
    }
    if (source->kind == TYPE_ENUM) {
        return type_is_compatible((Type*)source, (Type*)target);
    }
    return true;
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

/* Object exceptions use an owned byte copy in the target runtime.  Restrict
 * this ABI extension to complete aggregate types whose fields do not carry
 * user-defined lifetime state; such objects can be copied and destroyed
 * without invoking a constructor, destructor, or hidden ownership hook. */
static bool sema_cxx_trivially_copyable(Type* type, int depth) {
    CxxClass* cls;
    if (!type || depth > 32) return false;
    if (type->kind == TYPE_ARRAY) {
        return type->array_len >= 0 && type->base &&
               sema_cxx_trivially_copyable(type->base, depth + 1);
    }
    if (type->kind != TYPE_STRUCT && type->kind != TYPE_UNION) return true;
    if (!type_is_complete(type) || type->size <= 0) {
        return false;
    }
    cls = type->cxx_class;
    if (cls && (cls->has_user_constructor || cls->has_field_initializer ||
                cls->vtable_size > 0 || cls->destructor_method)) {
        return false;
    }
    if (cls) {
        for (int index = 0; index < cls->base_count; ++index) {
            CxxClass* base = cls->bases[index].base;
            if (!base || !base->type ||
                !sema_cxx_trivially_copyable(base->type, depth + 1)) {
                return false;
            }
        }
    }
    for (TypeField* field = type->fields; field; field = field->next) {
        if (!sema_cxx_trivially_copyable(field->type, depth + 1)) {
            return false;
        }
    }
    return true;
}

static Decl* sema_cxx_destructor_function(Type* object_type);

static bool sema_cxx_validate_default_member_initializers(
    Type* object_type, SourceLoc loc) {
    if (!object_type || !object_type->cxx_class ||
        !object_type->cxx_class->has_field_initializer) {
        return false;
    }
    for (TypeField* field = object_type->fields; field; field = field->next) {
        int64_t value;
        if (!field->initializer) continue;
        if (!field->type || field->type->size <= 0 ||
            !(type_is_integer(field->type) || field->type->kind == TYPE_ENUM ||
              field->type->kind == TYPE_PTR ||
              field->type->kind == TYPE_NULLPTR) ||
            !expr_eval_integer_constant(field->initializer, &value)) {
            rcc_error(loc,
                      "new requires scalar integer constant default member initializers");
            return false;
        }
        if (!sema_expr(field->initializer) ||
            !implicit_cast(field->initializer, field->type)) {
            rcc_error(field->initializer->loc,
                      "default member initializer is incompatible with its field");
            return false;
        }
    }
    return true;
}

/* A non-trivial exception object needs one additional runtime operation: the
 * owned byte copy must be destroyed when the handler releases the payload.
 * Keep this first ABI extension deliberately bounded.  It is valid for a
 * complete class with an available non-throwing-by-construction destructor,
 * no constructor/initializer/base/vtable state, and only trivially-copyable
 * fields.  Such a class has no hidden ownership to duplicate, while its
 * explicit destructor still gives the runtime a complete-object cleanup
 * callback. */
static bool sema_cxx_exception_object_copyable(Type* type, int depth) {
    CxxClass* cls;
    if (!type || depth > 32 || type->kind != TYPE_STRUCT ||
        !type_is_complete(type) || type->size <= 0) {
        return false;
    }
    if (sema_cxx_trivially_copyable(type, depth + 1)) return true;
    cls = type->cxx_class;
    if (!cls || cls->has_user_constructor || cls->has_field_initializer ||
        cls->base_count != 0 || cls->vtable_size != 0 ||
        !sema_cxx_destructor_function(type)) {
        return false;
    }
    for (TypeField* field = type->fields; field; field = field->next) {
        if (!sema_cxx_trivially_copyable(field->type, depth + 1)) {
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

static Decl* sema_cxx_destructor_function(Type* object_type) {
    CxxClass* cls = object_type ? object_type->cxx_class : NULL;
    CxxMethod* method = cls ? cls->destructor_method : NULL;
    if (!method || !method->decl || !method->decl->func_body ||
        !method->decl->link_name || !method->decl->func_this_param) {
        return NULL;
    }
    return method->decl;
}

static TypeField* sema_cxx_object_field(Type* object_type,
                                        const char* name) {
    for (TypeField* field = object_type ? object_type->fields : NULL;
         field; field = field->next) {
        if (field->name && name && strcmp(field->name, name) == 0) {
            return field;
        }
    }
    return NULL;
}

static Expr* sema_cxx_object_member(Expr* object, TypeField* field) {
    Expr* member;
    if (!object || !field || !field->name) return NULL;
    member = expr_member(object, field->name, object->loc);
    member->member_field = field;
    member->type = field->type;
    return member;
}

static Expr* sema_cxx_destructor_call_for_object(Type* object_type,
                                                  Expr* object,
                                                  SourceLoc loc) {
    Decl* destructor;
    Expr* address;
    Expr* function;
    Expr* call;
    if (!object_type || !object) return NULL;
    destructor = sema_cxx_destructor_function(object_type);
    if (!destructor) return NULL;
    address = expr_unary(EXPR_ADDR, object, loc);
    address->type = type_ptr(object_type);
    function = expr_ident(destructor->name, loc);
    function->ident_decl = destructor;
    function->type = destructor->type;
    call = expr_call(function, exprlist_new(address), loc);
    call->type = type_void;
    return call;
}

static Expr* sema_cxx_wrapper_cleanup_for_object(Type* object_type,
                                                 Expr* object,
                                                 SourceLoc loc) {
    Decl* function;
    Expr* field_expression;
    Expr* condition;
    Expr* function_expression;
    Expr* call;
    if (!object_type || !object || !object_type->cleanup_function ||
        !object_type->cleanup_field) {
        return NULL;
    }
    function = sema_cxx_cleanup_function(object_type, loc);
    if (!function) return NULL;
    field_expression = sema_cxx_object_member(
        object, object_type->cleanup_field);
    if (!field_expression) return NULL;
    condition = expr_binary(
        EXPR_NE, field_expression,
        expr_int(object_type->cleanup_invalid, loc), loc);
    condition->type = type_int;
    function_expression = expr_ident(function->name, loc);
    function_expression->ident_decl = function;
    function_expression->type = function->type;
    call = expr_call(function_expression,
                     exprlist_new(field_expression), loc);
    call->type = function->type->ret_type;
    call = expr_cond(condition, call, expr_int(0, loc), loc);
    call->type = call->cond_then->type &&
        call->cond_then->type->kind != TYPE_VOID
        ? call->cond_then->type : type_int;
    return call;
}

static bool sema_cxx_type_has_destructor_cleanup(Type* object_type,
                                                  int depth) {
    CxxClass* cls;
    if (!object_type || depth > 32) return false;
    if (sema_cxx_destructor_function(object_type) ||
        (object_type->cleanup_function && object_type->cleanup_field)) {
        return true;
    }
    cls = object_type->cxx_class;
    if (!cls) return false;
    for (TypeParam* parameter = cls->fields; parameter;
         parameter = parameter->next) {
        Type* field_type;
        if (parameter->is_static) continue;
        field_type = parameter->type;
        if (field_type && field_type->cxx_class &&
            sema_cxx_type_has_destructor_cleanup(field_type, depth + 1)) {
            return true;
        }
    }
    return false;
}

/* Append cleanup calls in the order needed by the cleanup stack.  The stack
 * is executed from its newest entry, so each subobject's children are
 * appended before that subobject and the complete object's own destructor is
 * appended last.  Runtime exception registration can therefore retain one
 * target-width callback per validated destructor call. */
static bool sema_cxx_append_object_cleanups(Decl* declaration,
                                            Type* object_type,
                                            Expr* object,
                                            ExprList** cleanups,
                                            int depth) {
    CxxClass* cls;
    TypeField** fields;
    int field_count = 0;
    int field_index = 0;
    bool valid = true;
    if (!object_type || !object || !cleanups || depth > 32) return false;
    cls = object_type->cxx_class;
    if (!cls) return true;
    for (TypeParam* parameter = cls->fields; parameter;
         parameter = parameter->next) {
        if (!parameter->is_static && parameter->type &&
            parameter->type->cxx_class &&
            sema_cxx_type_has_destructor_cleanup(parameter->type, 0)) {
            ++field_count;
        }
    }
    fields = field_count ? rcc_alloc(sizeof(*fields) * (size_t)field_count)
                         : NULL;
    for (TypeParam* parameter = cls->fields; parameter;
         parameter = parameter->next) {
        TypeField* field;
        if (parameter->is_static || !parameter->type ||
            !parameter->type->cxx_class ||
            !sema_cxx_type_has_destructor_cleanup(parameter->type, 0)) {
            continue;
        }
        field = sema_cxx_object_field(object_type, parameter->name);
        if (!field) {
            valid = false;
            continue;
        }
        fields[field_index++] = field;
    }
    for (int index = 0; index < field_index; ++index) {
        Expr* member = sema_cxx_object_member(object, fields[index]);
        if (!member || !sema_cxx_append_object_cleanups(
                declaration, fields[index]->type, member, cleanups,
                depth + 1)) {
            valid = false;
        }
    }
    if (object_type->cleanup_function && object_type->cleanup_field) {
        Expr* cleanup = sema_cxx_wrapper_cleanup_for_object(
            object_type, object, declaration ? declaration->loc : object->loc);
        if (cleanup) exprlist_append(cleanups, cleanup);
        else valid = false;
    } else if (sema_cxx_destructor_function(object_type)) {
        Expr* destructor = sema_cxx_destructor_call_for_object(
            object_type, object, declaration ? declaration->loc : object->loc);
        if (destructor) exprlist_append(cleanups, destructor);
        else valid = false;
    }
    if (fields) rcc_free(fields);
    return valid;
}

static void sema_prepare_variable_destructor_cleanup(Decl* declaration) {
    Expr* object;
    if (!rcc_parser_is_cxx_mode() || !declaration ||
        !declaration->type || declaration->type->kind != TYPE_STRUCT ||
        declaration->type->cleanup_function || declaration->var_cleanup ||
        declaration->var_cleanups ||
        !declaration->var_init) {
        return;
    }
    object = expr_ident(declaration->name, declaration->loc);
    object->ident_decl = declaration;
    object->type = declaration->type;
    if (sema_cxx_type_has_destructor_cleanup(declaration->type, 0) &&
        !sema_cxx_append_object_cleanups(
            declaration, declaration->type, object,
            &declaration->var_cleanups, 0)) {
        rcc_error(declaration->loc,
                  "C++ object lifetime cleanup metadata is incomplete");
    }
}

static void sema_resolve_cxx_constructor_initializers(
    CxxConstructorInfo* constructor, SourceLoc loc);

static Decl* sema_cxx_constructor_parameter(
    CxxConstructorInfo* constructor, const char* name) {
    if (!constructor || !constructor->method || !name) return NULL;
    for (DeclList* item = constructor->method->decl
             ? constructor->method->decl->func_params : NULL;
         item; item = item->next) {
        if (item->decl && item->decl->name &&
            strcmp(item->decl->name, name) == 0) {
            return item->decl;
        }
    }
    return NULL;
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
            candidate->parameter_count != argument_count ||
            (!candidate->body_is_empty &&
             ((candidate->initializer_count != 0 &&
               !candidate->initializers_are_supported) ||
              !candidate->method->decl ||
              !candidate->method->decl->func_is_cxx_method ||
              !candidate->method->decl->func_body)) ||
            (candidate->body_is_empty &&
             !candidate->initializers_are_supported)) {
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
    sema_resolve_cxx_constructor_initializers(best, loc);
    return best;
}

static void sema_resolve_cxx_constructor_initializers(
    CxxConstructorInfo* constructor, SourceLoc loc) {
    CxxClass* cls;
    if (!constructor || !constructor->method ||
        !constructor->method->owner || !constructor->method->owner->type) {
        return;
    }
    cls = constructor->method->owner;
    for (CxxConstructorInitializer* initializer = constructor->initializers;
         initializer; initializer = initializer->next) {
        TypeField* field = sema_cxx_object_field(
            cls->type, initializer->field);
        if (!field || !field->type) {
            rcc_error(loc,
                      "constructor initializer names an unknown member '%s'",
                      initializer->field ? initializer->field : "");
            continue;
        }
        for (ExprList* argument = initializer->arguments; argument;
             argument = argument->next) {
            Decl* parameter = argument->expr &&
                argument->expr->kind == EXPR_IDENT
                ? sema_cxx_constructor_parameter(
                    constructor, argument->expr->ident_name) : NULL;
            if (parameter) {
                argument->expr->ident_decl = parameter;
                argument->expr->type = parameter->type;
            } else if (argument->expr) {
                sema_expr(argument->expr);
            }
        }
        if (initializer->is_default_member_initializer &&
            initializer->value) {
            Type* value_type = sema_expr(initializer->value);
            if (!value_type || !implicit_cast(initializer->value,
                                               field->type)) {
                rcc_error(initializer->value->loc,
                          "default member initializer is incompatible with its field");
            }
        }
        if (field->type->cxx_class) {
            initializer->constructor = sema_select_cxx_new_constructor(
                field->type, initializer->arguments, initializer->value
                    ? initializer->value->loc : loc);
            if (!initializer->constructor) {
                rcc_error(initializer->value ? initializer->value->loc : loc,
                          "no safely lowerable constructor accepts the member initializer");
            }
        }
    }
}

/* Array new initializers are a sequence of element initializers, rather than
 * one constructor argument list.  The currently lowerable ABI supports a
 * default constructor for every element or a single constructor parameter
 * for each explicitly initialized element.  Select the constructor from the
 * first element and then validate every remaining element against that same
 * signature; mixing constructor arities would otherwise produce a partially
 * initialized allocation. */
static CxxConstructorInfo* sema_select_cxx_array_constructor(
    Type* object_type, ExprList* initializers, SourceLoc loc) {
    CxxConstructorInfo* constructor;
    ExprList one;
    TypeParam* parameter;

    if (!object_type || !object_type->cxx_class) return NULL;
    if (!initializers) {
        return sema_select_cxx_new_constructor(object_type, NULL, loc);
    }
    if (initializers->next) {
        one = *initializers;
        one.next = NULL;
        constructor = sema_select_cxx_new_constructor(
            object_type, &one, loc);
    } else {
        constructor = sema_select_cxx_new_constructor(
            object_type, initializers, loc);
    }
    if (!constructor || constructor->parameter_count != 1 ||
        !constructor->parameters) {
        return NULL;
    }
    parameter = constructor->parameters;
    for (ExprList* item = initializers; item; item = item->next) {
        if (!item->expr || cxx_conversion_rank(item->expr, parameter->type) < 0) {
            rcc_error(item && item->expr ? item->expr->loc : loc,
                      "array new initializer is incompatible with the element constructor");
            return NULL;
        }
    }
    return constructor;
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

/* The cv-qualification of a non-static member function belongs to its
 * implicit object parameter.  It is therefore part of overload viability and
 * ranking even though it is not present in the explicit argument list. */
static int cxx_member_object_conversion_rank(Type* object_type,
                                             TypeMethod* method) {
    Type* this_type;
    Type* this_object;

    if (!method || !method->function_decl ||
        !method->function_decl->func_is_cxx_method ||
        !method->function_decl->func_this_param) {
        return 0;
    }
    this_type = method->function_decl->func_this_param
        ? method->function_decl->func_this_param->type : NULL;
    if (!this_type || this_type->kind != TYPE_PTR || !this_type->base) {
        return -1;
    }
    if (!object_type ||
        (object_type->kind != TYPE_STRUCT &&
         object_type->kind != TYPE_UNION)) {
        return -1;
    }
    this_object = this_type->base;
    if ((object_type->is_const && !this_object->is_const) ||
        (object_type->is_volatile && !this_object->is_volatile)) {
        return -1;
    }
    /* Binding a mutable object to a const member is a qualification
     * conversion.  The mutable overload is the better match when both are
     * viable. */
    if (this_object->is_const && !object_type->is_const) return 1;
    return 0;
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
        int object_rank;

        if (method->kind != TYPE_METHOD_FUNCTION || !method->function_decl ||
            strcmp(method->name, name) != 0) {
            continue;
        }
        function = method->function_decl;
        object_rank = cxx_member_object_conversion_rank(aggregate, method);
        if (object_rank < 0) continue;
        total += object_rank;
        if (object_rank > worst) worst = object_rank;
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

static const char* sema_cxx_binary_operator_name(ExprKind kind) {
    switch (kind) {
        case EXPR_ADD: return "operator+";
        case EXPR_SUB: return "operator-";
        case EXPR_MUL: return "operator*";
        case EXPR_DIV: return "operator/";
        case EXPR_MOD: return "operator%";
        case EXPR_BITAND: return "operator&";
        case EXPR_BITOR: return "operator|";
        case EXPR_BITXOR: return "operator^";
        case EXPR_LSHIFT: return "operator<<";
        case EXPR_RSHIFT: return "operator>>";
        case EXPR_EQ: return "operator==";
        case EXPR_NE: return "operator!=";
        case EXPR_LT: return "operator<";
        case EXPR_GT: return "operator>";
        case EXPR_LE: return "operator<=";
        case EXPR_GE: return "operator>=";
        case EXPR_AND: return "operator&&";
        case EXPR_OR: return "operator||";
        default: return NULL;
    }
}

static const char* sema_cxx_assignment_operator_name(ExprKind kind) {
    switch (kind) {
        case EXPR_ASSIGN: return "operator=";
        case EXPR_ADD_ASSIGN: return "operator+=";
        case EXPR_SUB_ASSIGN: return "operator-=";
        case EXPR_MUL_ASSIGN: return "operator*=";
        case EXPR_DIV_ASSIGN: return "operator/=";
        case EXPR_MOD_ASSIGN: return "operator%=";
        case EXPR_AND_ASSIGN: return "operator&=";
        case EXPR_OR_ASSIGN: return "operator|=";
        case EXPR_XOR_ASSIGN: return "operator^=";
        case EXPR_LSHIFT_ASSIGN: return "operator<<=";
        case EXPR_RSHIFT_ASSIGN: return "operator>>=";
        default: return NULL;
    }
}

/* Rewrite a binary expression to an ordinary member call only after a real
 * operator member exists.  This keeps the built-in arithmetic path intact
 * for scalar operands and ensures an overloaded operation uses the same
 * access, conversion, this-adjustment, and ABI machinery as obj.method(). */
static bool sema_rewrite_cxx_binary_operator(Expr* expression, Type* left_type) {
    const char* name;
    Type* aggregate;
    Type* right_type;
    Type* right_aggregate;
    TypeMethod* method;
    ExprList* arguments = NULL;
    Symbol* function;
    Expr* function_expression;
    Expr* member;
    Expr* call;
    if (!expression || !left_type) return false;
    aggregate = generic_selection_type(left_type);
    name = sema_cxx_binary_operator_name(expression->kind);
    if (!name) return false;
    method = sema_find_function_method(aggregate, name);
    if (aggregate && (aggregate->kind == TYPE_STRUCT ||
                      aggregate->kind == TYPE_UNION) &&
        method && method->function_decl) {
        member = expr_member(expression->binary_lhs, name, expression->loc);
        call = expr_call(member, exprlist_new(expression->binary_rhs),
                         expression->loc);
        *expression = *call;
        return true;
    }
    right_type = sema_expr(expression->binary_rhs);
    right_aggregate = generic_selection_type(right_type);
    if ((!aggregate || (aggregate->kind != TYPE_STRUCT &&
                        aggregate->kind != TYPE_UNION)) &&
        (!right_aggregate || (right_aggregate->kind != TYPE_STRUCT &&
                              right_aggregate->kind != TYPE_UNION))) {
        return false;
    }
    exprlist_append(&arguments, expression->binary_lhs);
    exprlist_append(&arguments, expression->binary_rhs);
    function = sema_cxx_operator_function(name, arguments);
    if (!function) return false;
    function_expression = expr_ident(name, expression->loc);
    call = expr_call(function_expression, arguments, expression->loc);
    *expression = *call;
    return true;
}

static bool sema_rewrite_cxx_assignment_operator(Expr* expression,
                                                 Type* left_type) {
    const char* name;
    Type* aggregate;
    TypeMethod* method;
    Expr* member;
    Expr* call;
    if (!expression || !left_type || !expression->binary_rhs) return false;
    aggregate = generic_selection_type(left_type);
    if (!aggregate || (aggregate->kind != TYPE_STRUCT &&
                       aggregate->kind != TYPE_UNION)) {
        return false;
    }
    name = sema_cxx_assignment_operator_name(expression->kind);
    if (!name) return false;
    if (expression->kind == EXPR_ASSIGN &&
        aggregate->move_assignment_method) {
        /* The validated ownership lowering is attached to the assignment
         * expression itself and must not be replaced by ordinary overload
         * lookup. */
        return false;
    }
    method = sema_find_function_method(aggregate, name);
    if (!method || !method->function_decl) return false;
    member = expr_member(expression->binary_lhs, name, expression->loc);
    call = expr_call(member, exprlist_new(expression->binary_rhs),
                     expression->loc);
    *expression = *call;
    return true;
}

static const char* sema_cxx_unary_operator_name(ExprKind kind) {
    switch (kind) {
        case EXPR_NEG: return "operator-";
        case EXPR_BITNOT: return "operator~";
        case EXPR_NOT: return "operator!";
        case EXPR_PREINC:
        case EXPR_POSTINC: return "operator++";
        case EXPR_PREDEC:
        case EXPR_POSTDEC: return "operator--";
        default: return NULL;
    }
}

/* Lower unary member operators through the same call path as an explicit
 * member invocation.  A postfix increment/decrement receives the required
 * dummy int argument, which lets overload selection distinguish it from the
 * prefix form without inventing a backend-only operation. */
static bool sema_rewrite_cxx_unary_operator(Expr* expression,
                                            Type* operand_type) {
    const char* name;
    Type* aggregate;
    TypeMethod* method;
    ExprList* arguments = NULL;
    Symbol* function;
    Expr* function_expression;
    Expr* member;
    Expr* call;
    if (!expression || !operand_type) return false;
    aggregate = generic_selection_type(operand_type);
    if (!aggregate || (aggregate->kind != TYPE_STRUCT &&
                       aggregate->kind != TYPE_UNION)) {
        return false;
    }
    name = sema_cxx_unary_operator_name(expression->kind);
    if (!name) return false;
    method = sema_find_function_method(aggregate, name);
    if (method && method->function_decl) {
        if (expression->kind == EXPR_POSTINC ||
            expression->kind == EXPR_POSTDEC) {
            arguments = exprlist_new(expr_int(0, expression->loc));
        }
        member = expr_member(expression->unary_operand, name,
                             expression->loc);
        call = expr_call(member, arguments, expression->loc);
        *expression = *call;
        return true;
    }
    exprlist_append(&arguments, expression->unary_operand);
    if (expression->kind == EXPR_POSTINC ||
        expression->kind == EXPR_POSTDEC) {
        exprlist_append(&arguments, expr_int(0, expression->loc));
    }
    function = sema_cxx_operator_function(name, arguments);
    if (!function) return false;
    function_expression = expr_ident(name, expression->loc);
    call = expr_call(function_expression, arguments, expression->loc);
    *expression = *call;
    return true;
}

static bool sema_rewrite_cxx_subscript_operator(Expr* expression,
                                                Type* object_type) {
    Type* aggregate;
    TypeMethod* method;
    ExprList* arguments = NULL;
    Symbol* function;
    Expr* function_expression;
    Expr* member;
    Expr* call;
    if (!expression || !object_type || expression->kind != EXPR_INDEX) {
        return false;
    }
    aggregate = generic_selection_type(object_type);
    if (!aggregate || (aggregate->kind != TYPE_STRUCT &&
                       aggregate->kind != TYPE_UNION)) {
        return false;
    }
    method = sema_find_function_method(aggregate, "operator[]");
    if (method && method->function_decl) {
        member = expr_member(expression->index_base, "operator[]",
                             expression->loc);
        call = expr_call(member, exprlist_new(expression->index_expr),
                         expression->loc);
        *expression = *call;
        return true;
    }
    sema_expr(expression->index_expr);
    exprlist_append(&arguments, expression->index_base);
    exprlist_append(&arguments, expression->index_expr);
    function = sema_cxx_operator_function("operator[]", arguments);
    if (!function) return false;
    function_expression = expr_ident("operator[]", expression->loc);
    call = expr_call(function_expression, arguments, expression->loc);
    *expression = *call;
    return true;
}

static bool sema_rewrite_cxx_call_operator(Expr* expression,
                                           Type* object_type) {
    Type* aggregate;
    TypeMethod* method;
    Expr* member;
    Expr* call;
    if (!expression || !object_type || expression->kind != EXPR_CALL ||
        !expression->call_func || expression->call_is_new ||
        expression->call_is_delete) {
        return false;
    }
    if (expression->call_func->kind == EXPR_MEMBER ||
        expression->call_func->kind == EXPR_PTR_MEMBER) {
        return false;
    }
    aggregate = generic_selection_type(object_type);
    if (!aggregate || (aggregate->kind != TYPE_STRUCT &&
                       aggregate->kind != TYPE_UNION)) {
        return false;
    }
    method = sema_find_function_method(aggregate, "operator()");
    if (!method || !method->function_decl) return false;
    member = expr_member(expression->call_func, "operator()", expression->loc);
    call = expr_call(member, expression->call_args, expression->loc);
    *expression = *call;
    return true;
}

static void sema_expand_cxx_lambda_captures(Expr* call) {
    ExprList* captures;
    ExprList* tail;
    if (!call || !call->call_func || call->call_func->kind != EXPR_IDENT ||
        !call->call_func->cxx_lambda_captures) return;
    captures = call->call_func->cxx_lambda_captures;
    tail = captures;
    while (tail->next) tail = tail->next;
    tail->next = call->call_args;
    call->call_args = captures;
    call->call_func->cxx_lambda_captures = NULL;
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

    if ((expr->kind == EXPR_NEG || expr->kind == EXPR_BITNOT ||
         expr->kind == EXPR_NOT || expr->kind == EXPR_PREINC ||
         expr->kind == EXPR_PREDEC || expr->kind == EXPR_POSTINC ||
         expr->kind == EXPR_POSTDEC) && expr->unary_operand) {
        Type* operand_type = sema_expr(expr->unary_operand);
        if (sema_rewrite_cxx_unary_operator(expr, operand_type)) {
            return sema_expr(expr);
        }
    }

    if (expr->kind == EXPR_INDEX && expr->index_base) {
        Type* object_type = sema_expr(expr->index_base);
        if (sema_rewrite_cxx_subscript_operator(expr, object_type)) {
            return sema_expr(expr);
        }
    }

    if (expr->kind == EXPR_CALL && expr->call_func &&
        !expr->call_is_new && !expr->call_is_delete) {
        Type* object_type;
        sema_expand_cxx_lambda_captures(expr);
        if (expr->call_func->kind == EXPR_MEMBER ||
            expr->call_func->kind == EXPR_PTR_MEMBER) {
            object_type = sema_expr(expr->call_func->member_base);
            if (expr->call_func->kind == EXPR_PTR_MEMBER) {
                object_type = get_pointer_base(object_type);
            }
        } else if (expr->call_func->kind == EXPR_IDENT) {
            Symbol* symbol = expr->call_func->ident_decl
                ? NULL : sema_cxx_lookup_name(expr->call_func->ident_name);
            if (expr->call_func->ident_decl &&
                (expr->call_func->ident_decl->kind == DECL_VAR ||
                 expr->call_func->ident_decl->kind == DECL_PARAM)) {
                object_type = expr->call_func->ident_decl->type;
            } else {
                object_type = symbol ? symbol->type : NULL;
            }
        } else {
            object_type = sema_expr(expr->call_func);
        }
        if (sema_rewrite_cxx_call_operator(expr, object_type)) {
            return sema_expr(expr);
        }
    }

    if (expr->kind >= EXPR_ASSIGN && expr->kind <= EXPR_RSHIFT_ASSIGN &&
        expr->binary_lhs && expr->binary_rhs) {
        Type* left_type = sema_expr(expr->binary_lhs);
        if (sema_rewrite_cxx_assignment_operator(expr, left_type)) {
            return sema_expr(expr);
        }
    }

    if (expr->kind >= EXPR_ADD && expr->kind <= EXPR_OR &&
        expr->binary_lhs && expr->binary_rhs) {
        Type* left_type = sema_expr(expr->binary_lhs);
        if (sema_rewrite_cxx_binary_operator(expr, left_type)) {
            return sema_expr(expr);
        }
    }

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
            if (expr->cxx_lambda_captures) {
                rcc_error(expr->loc,
                          "capturing lambda must be immediately invoked");
            }
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
            if (!sym && current_cxx_method_owner && expr->ident_name) {
                CxxClass* owner = current_cxx_method_owner->cxx_class;
                for (struct CxxMember* member = owner ? owner->members : NULL;
                     member; member = member->next) {
                    Decl* declaration = member->decl;
                    const char* separator;
                    if (!member->is_static || member->method || !declaration ||
                        declaration->kind != DECL_VAR ||
                        !declaration->name) {
                        continue;
                    }
                    separator = strrchr(declaration->name, ':');
                    separator = separator ? separator + 1 : declaration->name;
                    if (strcmp(separator, expr->ident_name) != 0) continue;
                    expr->ident_decl = declaration;
                    expr->ident_name = declaration->name;
                    expr->type = declaration->type;
                    sym = symtab_lookup(g_symtab, declaration->name);
                    break;
                }
            }
            if (!sym && current_cxx_method_owner &&
                current_cxx_this_param && expr->ident_name) {
                TypeField* field;
                const char* field_name = expr->ident_name;
                for (field = current_cxx_method_owner->fields; field;
                     field = field->next) {
                    if (field->name &&
                        strcmp(field->name, field_name) == 0) {
                        Expr* object = expr_ident("this", expr->loc);
                        Type* field_type = field->type;
                        object->ident_decl = current_cxx_this_param;
                        object->type = current_cxx_this_param->type;
                        expr->kind = EXPR_PTR_MEMBER;
                        expr->member_base = object;
                        expr->member_name = field_name;
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
            } else if ((!type_is_arithmetic(t) && t->kind != TYPE_ENUM) ||
                       sema_is_scoped_enum(t)) {
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
            } else if (!sema_is_integer_type(t) || sema_is_scoped_enum(t)) {
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
            if (expr->unary_operand &&
                expr->unary_operand->member_field &&
                expr->unary_operand->member_field->is_bitfield) {
                rcc_error(expr->loc, "cannot take address of a bit-field");
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
            Type* source = sema_expr(expr->cast_expr);
            sema_validate_array_parameter_type(expr->cast_type,
                                               expr->loc, false);
            expr->type = expr->cast_type;
            expr->cxx_pointer_adjustment_valid = false;
            if (expr->cxx_cast_kind == CXX_CAST_DYNAMIC) {
                int adjustment = 0;
                bool supported = false;
                if (source && expr->cast_type &&
                    source->kind == TYPE_PTR &&
                    expr->cast_type->kind == TYPE_PTR &&
                    !source->is_reference &&
                    !expr->cast_type->is_reference && source->base &&
                    expr->cast_type->base) {
                    supported = sema_cxx_public_base(
                        source->base, expr->cast_type->base,
                        &adjustment, 0);
                } else if (source && expr->cast_type &&
                           expr->cast_type->kind == TYPE_PTR &&
                           expr->cast_type->is_reference &&
                           source->kind != TYPE_PTR &&
                           expr->cast_type->base) {
                    supported = sema_cxx_public_base(
                        source, expr->cast_type->base, &adjustment, 0);
                }
                if (!supported) {
                    rcc_error(expr->loc,
                              "dynamic_cast currently supports only a statically known public upcast");
                } else if (adjustment != 0) {
                    expr->cxx_pointer_adjustment_valid = true;
                    expr->cxx_pointer_adjustment = adjustment;
                }
            }
            if (expr->cxx_cast_kind == CXX_CAST_CONST &&
                !cxx_const_cast_similar(source, expr->cast_type, 0u)) {
                rcc_error(expr->loc,
                          "const_cast requires the same object type with only cv qualification changes");
            }
            if (source && expr->cast_type &&
                source->kind == TYPE_PTR &&
                expr->cast_type->kind == TYPE_PTR &&
                expr->cxx_cast_kind != CXX_CAST_CONST &&
                !type_is_compatible(source, expr->cast_type)) {
                int adjustment;
                if (sema_cxx_pointer_conversion(source, expr->cast_type,
                                                 &adjustment)) {
                    expr->cxx_pointer_adjustment_valid = adjustment != 0;
                    expr->cxx_pointer_adjustment = adjustment;
                }
            }
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
                if (rcc_parser_is_cxx_mode() &&
                    expr->compound_type->cxx_class) {
                    expr->compound_constructor = sema_select_cxx_new_constructor(
                        expr->compound_type, expr->compound_init, expr->loc);
                }
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
                       !sema_is_integer_type(rt) ||
                       sema_is_scoped_enum(lt) ||
                       sema_is_scoped_enum(rt)) {
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
                       !sema_is_integer_type(rt) ||
                       sema_is_scoped_enum(lt) ||
                       sema_is_scoped_enum(rt)) {
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
                       !sema_is_integer_type(rt) ||
                       sema_is_scoped_enum(lt) ||
                       sema_is_scoped_enum(rt)) {
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
            bool scoped_enum = sema_is_scoped_enum(left_value) ||
                               sema_is_scoped_enum(right_value);
            bool arithmetic = !left_nullptr && !right_nullptr &&
                              ((scoped_enum &&
                                sema_is_scoped_enum(left_value) &&
                                sema_is_scoped_enum(right_value) &&
                                type_is_compatible(left_value, right_value)) ||
                               (!scoped_enum &&
                                (type_is_arithmetic(left_value) ||
                                 left_value->kind == TYPE_ENUM) &&
                                (type_is_arithmetic(right_value) ||
                                 right_value->kind == TYPE_ENUM)));
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
            Type* rt = sema_expr(expr->binary_rhs);
            if (!is_modifiable_lvalue(expr->binary_lhs)) {
                rcc_error(expr->loc,
                          "assignment requires modifiable lvalue");
            }
            if (sema_is_cxx_nullptr_expr(expr->binary_rhs) &&
                !type_is_pointer(lt) && lt->kind != TYPE_NULLPTR) {
                rcc_error(expr->loc,
                          "nullptr can only be assigned to a pointer");
            }
            if ((sema_is_scoped_enum(lt) || sema_is_scoped_enum(rt)) &&
                !implicit_cast(expr->binary_rhs, lt)) {
                rcc_error(expr->loc,
                          "incompatible scoped enum assignment");
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
            } else if ((sema_is_scoped_enum(tv) || sema_is_scoped_enum(ev)) &&
                       !(sema_is_scoped_enum(tv) &&
                         sema_is_scoped_enum(ev) &&
                         type_is_compatible(tv, ev))) {
                rcc_error(expr->loc,
                          "conditional operands have incompatible scoped enum types");
                expr->type = type_int;
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
                    bool has_member_cleanup =
                        sema_cxx_type_has_destructor_cleanup(object_type, 0);
                    expr->call_delete_object_type = object_type;
                    if (expr->call_delete_is_array) {
                        expr->call_delete_array_destructor =
                            sema_cxx_destructor_function(object_type);
                        if (!expr->call_delete_array_destructor &&
                            object_type->cleanup_function &&
                            object_type->cleanup_field) {
                            cleanup_function = sema_cxx_cleanup_function(
                                object_type, expr->loc);
                            if (cleanup_function) {
                                expr->call_delete_array_cleanup =
                                    cleanup_function;
                                expr->call_delete_array_cleanup_field =
                                    object_type->cleanup_field;
                                expr->call_delete_array_cleanup_invalid =
                                    object_type->cleanup_invalid;
                            }
                        }
                        if (!expr->call_delete_array_destructor &&
                            !expr->call_delete_array_cleanup &&
                            !has_member_cleanup) {
                            rcc_error(expr->loc,
                                      "array delete requires a lowerable element destructor");
                        }
                        return expr->type;
                    }
                    if (!object_type->cleanup_function ||
                        !object_type->cleanup_field) {
                        expr->call_delete_destructor =
                            sema_cxx_destructor_function(object_type);
                        if (!expr->call_delete_destructor &&
                            !has_member_cleanup) {
                            rcc_error(expr->loc,
                                      "delete requires C++ destructor lowering for a non-trivial object");
                            return expr->type;
                        }
                    } else {
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
                if (expr->call_new_is_array) {
                    if (cls && !cls->constructors &&
                        cls->has_field_initializer) {
                        if (expr->call_new_args ||
                            !sema_cxx_validate_default_member_initializers(
                                object_type, expr->loc)) {
                            if (expr->call_new_args) {
                                rcc_error(expr->loc,
                                          "array new with default member initializers requires an empty initializer list");
                            }
                            return expr->type;
                        }
                        expr->call_new_default_member_initializers = true;
                    }
                    int64_t element_count = 0;
                    if (expr->call_new_args && !expr->call_new_brace_init) {
                        rcc_error(expr->loc,
                                  "array new element initializers require braces");
                        return expr->type;
                    }
                    if (!expr->call_new_count) {
                        rcc_error(expr->loc,
                                  "array new requires an element count");
                        return expr->type;
                    }
                    /* A dynamic bound is valid for value-initialized scalar
                     * arrays and for the validated default-constructor path.
                     * Explicit per-element initializers need a constant bound
                     * so the frontend can prove that every initializer fits;
                     * the backend still evaluates a dynamic bound exactly once
                     * for the allocation and initialization loop. */
                    if (expr->call_new_args) {
                        if (!expr_eval_integer_constant(
                                expr->call_new_count, &element_count) ||
                            element_count < 0) {
                            rcc_error(expr->loc,
                                      "array new element initializers require a non-negative constant count");
                            return expr->type;
                        }
                        if (element_count < argument_count) {
                            rcc_error(expr->loc,
                                      "array new has more initializers than elements");
                            return expr->type;
                        }
                    }
                    if (object_type->cxx_nontrivial) {
                        Decl* destructor = sema_cxx_destructor_function(
                            object_type);
                        bool has_cleanup = object_type->cleanup_function &&
                            object_type->cleanup_field;
                        bool has_member_cleanup =
                            sema_cxx_type_has_destructor_cleanup(object_type, 0);
                        bool has_constructor = cls &&
                            rcc_parser_cxx_constructor_arity_mask(object_type);
                        bool has_user_constructor = cls &&
                            cls->constructors != NULL;
                        if (object_type->kind != TYPE_STRUCT || !cls ||
                            (!sema_cxx_trivially_destructible(object_type, 0) &&
                             !destructor && !has_cleanup &&
                             !has_member_cleanup) ||
                            (!has_constructor && has_user_constructor)) {
                            rcc_error(expr->loc,
                                      "array new requires a lowerable element constructor and destructor");
                            return expr->type;
                        }
                        if (has_constructor) {
                            constructor = sema_select_cxx_array_constructor(
                                object_type, expr->call_new_args, expr->loc);
                            if (!constructor) {
                                rcc_error(expr->loc,
                                          "array new has no lowerable constructor for its element initializers");
                                return expr->type;
                            }
                            expr->call_new_constructor = constructor;
                        }
                        if (destructor || has_cleanup || has_member_cleanup) {
                            expr->call_new_array_cookie = true;
                        }
                    } else if (object_type->kind == TYPE_STRUCT ||
                               object_type->kind == TYPE_UNION ||
                               object_type->kind == TYPE_ARRAY) {
                        rcc_error(expr->loc,
                                  "array new currently requires scalar elements");
                        return expr->type;
                    } else {
                        for (argument = expr->call_new_args; argument;
                             argument = argument->next) {
                            if (cxx_conversion_rank(argument->expr,
                                                    object_type) < 0) {
                                rcc_error(argument->expr->loc,
                                          "array new initializer is incompatible with the element type");
                                return expr->type;
                            }
                        }
                    }
                    return expr->type;
                }
                if (cls && !cls->constructors &&
                    cls->has_field_initializer &&
                    !sema_cxx_validate_default_member_initializers(
                        object_type, expr->loc)) {
                    return expr->type;
                }
                if (cls && !cls->constructors &&
                    cls->has_field_initializer) {
                    expr->call_new_default_member_initializers = true;
                }
                if (object_type->cxx_nontrivial &&
                    !expr->call_new_default_member_initializers &&
                    (!cls || !rcc_parser_cxx_constructor_arity_mask(object_type)) &&
                    !(expr->call_new_is_array &&
                      cls && !cls->constructors &&
                      (sema_cxx_destructor_function(object_type) ||
                       (object_type->cleanup_function &&
                        object_type->cleanup_field)))) {
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
                if (owner && owner->cxx_dependent) {
                    /* Dependent member lookup is completed after class
                     * template substitution; never diagnose or lower the
                     * placeholder expression here. */
                    expr->type = owner;
                    break;
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
                if (adl_symbol->decl &&
                    adl_symbol->decl->func_overload_next) {
                    selected_overload = sema_select_cxx_overload(expr);
                    if (!selected_overload) {
                        expr->type = type_int;
                        break;
                    }
                    expr->call_func->ident_decl = selected_overload;
                    expr->call_func->type = selected_overload->type;
                }
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
            if (call_declaration && call_declaration->func_is_constexpr) {
                bool constexpr_folded = false;
                if (sema_constexpr_integer_type(expr->type)) {
                    int64_t constexpr_value;
                    if (sema_eval_constexpr_function(call_declaration,
                                                      expr->call_args,
                                                      &constexpr_value)) {
                        expr->kind = EXPR_INT_LIT;
                        expr->int_val = constexpr_value;
                        expr->is_cxx_nullptr = false;
                        expr->cxx_move_assignment = NULL;
                        expr->cxx_close_call = NULL;
                        constexpr_folded = true;
                    } else {
                        SemaConstexprScalar scalar_value;
                        if (sema_eval_constexpr_scalar_function(
                                call_declaration, expr->call_args,
                                NULL, 0, &scalar_value) &&
                            !scalar_value.is_floating) {
                            expr->kind = EXPR_INT_LIT;
                            expr->int_val = scalar_value.integer_value;
                            expr->is_cxx_nullptr = false;
                            expr->cxx_move_assignment = NULL;
                            expr->cxx_close_call = NULL;
                            constexpr_folded = true;
                        }
                    }
                } else if (expr->type &&
                           (expr->type->kind == TYPE_FLOAT ||
                            expr->type->kind == TYPE_DOUBLE)) {
                    SemaConstexprScalar constexpr_value;
                    if (sema_eval_constexpr_scalar_function(
                            call_declaration, expr->call_args,
                            NULL, 0, &constexpr_value)) {
                        expr->kind = EXPR_FLOAT_LIT;
                        expr->float_val = constexpr_value.floating_value;
                        expr->type = call_declaration->type->ret_type;
                        expr->is_cxx_nullptr = false;
                        expr->cxx_move_assignment = NULL;
                        expr->cxx_close_call = NULL;
                        constexpr_folded = true;
                    }
                } else if (expr->type &&
                           sema_constexpr_aggregate_type(expr->type) &&
                           !expr->type->cxx_nontrivial &&
                           expr->type->cxx_vtable_size == 0) {
                    constexpr_folded = sema_fold_constexpr_aggregate_call(
                        expr, call_declaration);
                }
                if (call_declaration->func_is_consteval && !constexpr_folded) {
                    rcc_error(expr->loc,
                              "consteval call is not a constant expression");
                }
            }
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
                    if (field->cxx_access != 0u &&
                        (!current_cxx_method_owner ||
                         !current_cxx_method_owner->cxx_class ||
                         !bt->cxx_class ||
                         current_cxx_method_owner->cxx_class !=
                             bt->cxx_class)) {
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
            if (stmt->if_is_constexpr) {
                SemaConstexprScalar condition;
                Stmt* selected;
                if (!sema_eval_constexpr_scalar_expr(
                        stmt->if_cond, NULL, 0, &condition)) {
                    rcc_error(stmt->if_cond->loc,
                              "if constexpr condition is not a constant expression");
                    break;
                }
                selected = sema_constexpr_scalar_truth(&condition)
                    ? stmt->if_then : stmt->if_else;
                if (!selected) {
                    stmt->kind = STMT_NULL;
                    break;
                }
                sema_stmt(selected);
                {
                    StmtList* selected_list = rcc_alloc(sizeof(*selected_list));
                    selected_list->stmt = selected;
                    selected_list->next = NULL;
                    stmt->kind = STMT_BLOCK;
                    stmt->block_stmts = selected_list;
                }
                break;
            }
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
                if (!current_func_auto_return_pending &&
                    stmt->return_val->kind == EXPR_COMPOUND &&
                    !stmt->return_val->compound_type && current_func_ret &&
                    current_func_ret != type_void) {
                    stmt->return_val->compound_type = current_func_ret;
                    rcc_parser_validate_cxx_constructor_initializer(
                        current_func_ret, stmt->return_val);
                }
                sema_expr(stmt->return_val);
                if (!current_func_auto_return_pending && current_func_ret &&
                    current_func_ret->cleanup_function) {
                    rcc_error(stmt->loc,
                              "returning a C++ scope-cleanup type is not "
                              "supported yet");
                }
                if (!current_func_auto_return_pending && current_func_ret &&
                    current_func_ret != type_void) {
                    if (!implicit_cast(stmt->return_val, current_func_ret)) {
                        if (sema_is_scoped_enum(stmt->return_val->type) ||
                            sema_is_scoped_enum(current_func_ret)) {
                            rcc_error(stmt->loc,
                                      "cannot implicitly convert scoped enum in return");
                        } else {
                            rcc_warning(stmt->loc, "incompatible return type");
                        }
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

        case STMT_TRY: {
            int frame_size = g_opts.target_arch == ARCH_X64 ? 96 : 40;
            char frame_name[64];
            int written;
            Symbol* frame_symbol;
            bool seen_ellipsis = false;

            if (!stmt->try_body || stmt->try_body->kind != STMT_BLOCK) {
                rcc_error(stmt->loc, "C++ try body must be a compound statement");
                break;
            }
            if (!stmt->try_catches) {
                rcc_error(stmt->loc, "C++ try statement requires a catch handler");
                break;
            }

            ++cxx_exception_frame_counter;
            written = snprintf(frame_name, sizeof(frame_name),
                               "__rcc_exception_frame_%u",
                               cxx_exception_frame_counter);
            if (written < 0 || (size_t)written >= sizeof(frame_name)) {
                rcc_error(stmt->loc, "C++ exception frame name exceeds compiler limits");
                break;
            }
            frame_symbol = symtab_define(
                g_symtab, rcc_intern(frame_name), SYM_VAR,
                type_array(type_uchar, frame_size), stmt->loc);
            stmt->try_frame_offset = frame_symbol->offset;
            stmt->try_frame_size = frame_size;

            sema_stmt(stmt->try_body);
            if (sema_exception_body_has_vla(stmt->try_body)) {
                rcc_error(stmt->loc,
                          "C++ exception unwinding cannot bypass VLA lifetime");
            } else if (sema_exception_body_has_unregistered_cleanup(
                           stmt->try_body)) {
                if (sema_exception_body_has_call(stmt->try_body)) {
                    rcc_error(stmt->loc,
                              "C++ exception cleanup requires a call-free protected body");
                }
            }
            for (CxxCatch* handler = stmt->try_catches; handler;
                 handler = handler->next) {
                Type* match_type = sema_cxx_exception_match_type(handler->type);
                bool reference_type =
                    sema_cxx_exception_reference_type(handler->type);
                bool aggregate_object = handler->type &&
                    match_type && sema_cxx_exception_object_copyable(
                        match_type, 0);
                if (seen_ellipsis) {
                    rcc_error(handler->body ? handler->body->loc : stmt->loc,
                              "C++ catch-all handler must be the last handler");
                }
                if (handler->is_ellipsis) seen_ellipsis = true;
                if (!handler->is_ellipsis &&
                    ((!match_type ||
                      ((!type_is_integer(match_type) &&
                        match_type->kind != TYPE_ENUM &&
                        match_type->kind != TYPE_PTR) &&
                       !aggregate_object)) ||
                     (!aggregate_object && handler->type &&
                      (match_type->size <= 0 ||
                       match_type->size >
                           (g_opts.target_arch == ARCH_X64 ? 8 : 4))))) {
                    rcc_error(stmt->loc,
                              "C++ catch requires a scalar payload no wider than the target word or a supported aggregate exception object");
                }
                if (handler->parameter &&
                    (!match_type || (!reference_type &&
                     ((!type_is_integer(match_type) &&
                       match_type->kind != TYPE_ENUM &&
                       match_type->kind != TYPE_PTR) &&
                      !aggregate_object)))) {
                    rcc_error(handler->parameter->loc,
                              "named C++ catch parameter must have a scalar type or a supported aggregate exception object");
                }
                if (!handler->is_ellipsis && match_type &&
                    match_type->kind == TYPE_STRUCT &&
                    match_type->cxx_class) {
                    sema_cxx_collect_exception_tags(
                        sema_cxx_global_namespace(), match_type, handler, 0u);
                }
                sema_stmt(handler->body);
                if (handler->parameter && !reference_type && match_type &&
                    !sema_cxx_trivially_copyable(match_type, 0) &&
                    sema_cxx_exception_object_copyable(match_type, 0)) {
                    Expr* object = expr_ident(handler->parameter->name,
                                              handler->parameter->loc);
                    object->ident_decl = handler->parameter;
                    object->type = handler->type;
                    if (!sema_cxx_append_object_cleanups(
                            handler->parameter, handler->type, object,
                            &handler->parameter->var_cleanups, 0)) {
                        rcc_error(handler->parameter->loc,
                                  "C++ catch object lifetime cleanup metadata is incomplete");
                    }
                }
                if (sema_exception_body_has_vla(handler->body)) {
                    rcc_error(handler->body ? handler->body->loc : stmt->loc,
                              "C++ exception unwinding cannot bypass VLA lifetime");
                } else if (sema_exception_body_has_cleanup(handler->body)) {
                    if (sema_exception_body_has_call(handler->body)) {
                        rcc_error(handler->body ? handler->body->loc : stmt->loc,
                                  "C++ exception cleanup requires a call-free handler body");
                    }
                }
            }
            break;
        }

        case STMT_THROW: {
            Type* thrown_type;
            bool aggregate_object;
            if (!stmt->throw_expr) {
                /* The runtime validates that a currently handled exception
                 * exists.  Keep `throw;` as a real terminator instead of
                 * dropping it during semantic analysis. */
                break;
            }
            thrown_type = sema_expr(stmt->throw_expr);
            aggregate_object = thrown_type &&
                sema_cxx_exception_object_copyable(thrown_type, 0);
            if (!thrown_type ||
                (((!type_is_integer(thrown_type) &&
                   thrown_type->kind != TYPE_ENUM &&
                   thrown_type->kind != TYPE_PTR) &&
                  !aggregate_object) ||
                 (!aggregate_object &&
                  (thrown_type->size <= 0 ||
                   thrown_type->size >
                       (g_opts.target_arch == ARCH_X64 ? 8 : 4))))) {
                rcc_error(stmt->loc,
                          "C++ throw requires a scalar payload no wider than the target word or a supported aggregate exception object");
            }
            break;
        }
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
    if (!type || type->cxx_dependent || !initializer) return;
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
            if (sema_is_scoped_enum(initializer->type) ||
                sema_is_scoped_enum(type)) {
                rcc_error(initializer->loc,
                          "cannot implicitly convert scoped enum in initialization");
            } else {
                rcc_warning(initializer->loc,
                            "incompatible types in initialization");
            }
        }
        return;
    }
    initializer->type = type;
    if (rcc_parser_is_cxx_mode() && type->cxx_class &&
        type->cxx_class->has_user_constructor &&
        (!initializer->compound_value_init ||
         (rcc_parser_cxx_constructor_arity_mask(type) & 1u) != 0u)) {
        ExprList* constructor_arguments = initializer->compound_value_init
            ? NULL : initializer->compound_init;
        for (ExprList* item = constructor_arguments; item;
             item = item->next) {
            if (item->designator_kind != INIT_DESIGNATOR_NONE) {
                rcc_error(item->expr ? item->expr->loc : initializer->loc,
                          "constructor initializer cannot use an aggregate designator");
            } else if (item->expr) {
                sema_expr(item->expr);
            }
        }
        initializer->compound_constructor = sema_select_cxx_new_constructor(
            type, constructor_arguments, initializer->loc);
        return;
    }
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
        if (rcc_parser_is_cxx_mode() && type->cxx_class) {
            initializer->compound_constructor =
                sema_select_cxx_new_constructor(
                    type, initializer->compound_init, initializer->loc);
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
    if (declaration->var_is_auto_pointer) {
        Type* pointer_base = NULL;
        if (deduced && deduced->kind == TYPE_PTR) {
            pointer_base = deduced->base;
        } else if (deduced && deduced->kind == TYPE_ARRAY) {
            pointer_base = deduced->base;
        }
        if (!pointer_base) {
            rcc_error(declaration->loc,
                      "auto* initializer must be a pointer or array");
            return type_int;
        }
        if (declaration->var_is_auto_const) {
            Type* qualified = ast_arena_alloc(sizeof(*qualified));
            *qualified = *pointer_base;
            qualified->is_const = true;
            pointer_base = qualified;
        }
        return type_ptr(pointer_base);
    }
    if (declaration->var_is_auto_reference) {
        bool binds_lvalue = is_lvalue(declaration->var_init);
        if (!deduced || deduced->kind == TYPE_VOID) {
            rcc_error(declaration->loc,
                      "auto reference initializer has no object type");
            return type_int;
        }
        if (!binds_lvalue && !declaration->var_is_auto_rvalue_reference) {
            rcc_error(declaration->loc,
                      "auto& initializer must be an lvalue");
        }
        if (deduced->kind == TYPE_PTR && deduced->is_reference) {
            deduced = deduced->base;
        }
        if (declaration->var_is_auto_const && deduced) {
            Type* qualified = ast_arena_alloc(sizeof(*qualified));
            *qualified = *deduced;
            qualified->is_const = true;
            deduced = qualified;
        }
        Type* reference = type_ptr(deduced);
        reference->is_reference = true;
        reference->is_rvalue_reference =
            declaration->var_is_auto_rvalue_reference && !binds_lvalue;
        return reference;
    }
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
    if (declaration->var_is_auto_const) {
        Type* qualified = ast_arena_alloc(sizeof(*qualified));
        *qualified = *deduced;
        qualified->is_const = true;
        deduced = qualified;
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
    /* Static-storage cleanup expressions are registered in the module's
     * .fini_array callback after this validation completes.  Keep the same
     * structural restrictions as automatic RAII objects. */
    (void)is_global;
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
    /* User constructors consume the original initializer through the C++
     * constructor-selection path below.  This aggregate-only helper must not
     * rewrite their argument list before that selection occurs. */
    if (cls->has_user_constructor) return NULL;

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

static Type* sema_decltype_auto_return_type(Expr* expression) {
    Type* result;
    if (!expression) return type_void;
    result = sema_expr(expression);
    if (!result) return NULL;

    /* Reference variables are exposed as their referred-to value type by
     * ordinary expression analysis.  decltype(auto) must retain the declared
     * reference type. */
    if (expression->kind == EXPR_IDENT && expression->ident_decl &&
        (expression->ident_decl->kind == DECL_VAR ||
         expression->ident_decl->kind == DECL_PARAM) &&
        expression->ident_decl->type &&
        expression->ident_decl->type->is_reference) {
        return expression->ident_decl->type;
    }

    /* These expression forms are lvalues.  Preserve that category for the
     * lowered reference ABI instead of silently copying the object value. */
    if (expression->kind == EXPR_DEREF || expression->kind == EXPR_INDEX ||
        expression->kind == EXPR_MEMBER ||
        expression->kind == EXPR_PTR_MEMBER) {
        if (result->kind == TYPE_PTR && result->is_reference) return result;
        if (result->kind == TYPE_ARRAY || result->kind == TYPE_FUNC) {
            rcc_error(expression->loc,
                      "decltype(auto) cannot return an array or function lvalue");
            return type_int;
        }
        Type* reference = type_ptr(result);
        reference->is_reference = true;
        return reference;
    }

    /* A reference-returning call already carries the exact reference type. */
    if (expression->kind == EXPR_CALL && result->is_reference) return result;
    return result;
}

static bool sema_validate_auto_return_stmt(Stmt* statement) {
    if (!statement) return true;
    switch (statement->kind) {
        case STMT_RETURN:
            if (statement->return_val && current_func_ret &&
                current_func_ret != type_void &&
                !implicit_cast(statement->return_val, current_func_ret)) {
                if (sema_is_scoped_enum(statement->return_val->type) ||
                    sema_is_scoped_enum(current_func_ret)) {
                    rcc_error(statement->loc,
                              "cannot implicitly convert scoped enum in return");
                } else {
                    rcc_warning(statement->loc, "incompatible return type");
                }
            }
            return true;
        case STMT_BLOCK:
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                if (!sema_validate_auto_return_stmt(item->stmt)) return false;
            }
            return true;
        case STMT_IF:
            return sema_validate_auto_return_stmt(statement->if_then) &&
                   sema_validate_auto_return_stmt(statement->if_else);
        case STMT_WHILE:
        case STMT_DO:
            return sema_validate_auto_return_stmt(statement->while_body);
        case STMT_FOR:
            return sema_validate_auto_return_stmt(statement->for_init) &&
                   sema_validate_auto_return_stmt(statement->for_body);
        case STMT_SWITCH:
            return sema_validate_auto_return_stmt(statement->switch_body);
        case STMT_CASE:
            return sema_validate_auto_return_stmt(statement->case_stmt);
        case STMT_DEFAULT:
            return sema_validate_auto_return_stmt(statement->default_stmt);
        case STMT_LABEL:
            return sema_validate_auto_return_stmt(statement->label_stmt);
        case STMT_TRY:
            if (!sema_validate_auto_return_stmt(statement->try_body)) {
                return false;
            }
            for (CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                if (!sema_validate_auto_return_stmt(handler->body)) {
                    return false;
                }
            }
            return true;
        default:
            return true;
    }
}

static bool sema_deduce_auto_return_stmt(Stmt* statement, Type** deduced,
                                         bool* saw_return,
                                         bool decltype_auto) {
    if (!statement || !deduced || !saw_return) return true;
    switch (statement->kind) {
        case STMT_RETURN: {
            Type* result_type = NULL;
            if (statement->return_val) {
                result_type = decltype_auto
                    ? sema_decltype_auto_return_type(statement->return_val)
                    : generic_selection_type(
                          sema_expr(statement->return_val));
                if (!result_type || result_type->kind == TYPE_VOID) {
                    rcc_error(statement->loc,
                              "auto return expression has no value");
                    return false;
                }
            } else {
                result_type = type_void;
            }
            if (!*saw_return) {
                *deduced = result_type;
                *saw_return = true;
                return true;
            }
            if (!type_is_compatible(*deduced, result_type)) {
                rcc_error(statement->loc,
                          "inconsistent deduction for auto return type");
                return false;
            }
            return true;
        }
        case STMT_BLOCK:
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                if (!sema_deduce_auto_return_stmt(item->stmt, deduced,
                                                  saw_return,
                                                  decltype_auto)) return false;
            }
            return true;
        case STMT_IF:
            return sema_deduce_auto_return_stmt(statement->if_then, deduced,
                                                saw_return, decltype_auto) &&
                   sema_deduce_auto_return_stmt(statement->if_else, deduced,
                                                saw_return, decltype_auto);
        case STMT_WHILE:
        case STMT_DO:
            return sema_deduce_auto_return_stmt(statement->while_body, deduced,
                                                saw_return, decltype_auto);
        case STMT_FOR:
            return sema_deduce_auto_return_stmt(statement->for_init, deduced,
                                                saw_return, decltype_auto) &&
                   sema_deduce_auto_return_stmt(statement->for_body, deduced,
                                                saw_return, decltype_auto);
        case STMT_SWITCH:
            return sema_deduce_auto_return_stmt(statement->switch_body, deduced,
                                                saw_return, decltype_auto);
        case STMT_CASE:
            return sema_deduce_auto_return_stmt(statement->case_stmt, deduced,
                                                saw_return, decltype_auto);
        case STMT_DEFAULT:
            return sema_deduce_auto_return_stmt(statement->default_stmt,
                                                deduced, saw_return,
                                                decltype_auto);
        case STMT_LABEL:
            return sema_deduce_auto_return_stmt(statement->label_stmt, deduced,
                                                saw_return, decltype_auto);
        case STMT_TRY:
            if (!sema_deduce_auto_return_stmt(statement->try_body, deduced,
                                              saw_return, decltype_auto)) return false;
            for (CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                if (!sema_deduce_auto_return_stmt(handler->body, deduced,
                                                  saw_return,
                                                  decltype_auto)) return false;
            }
            return true;
        default:
            return true;
    }
}

static void sema_decl(Decl* decl) {
    if (!decl) return;

    switch (decl->kind) {
        case DECL_STATIC_ASSERT: {
            Type* condition_type = sema_expr(decl->static_assert_expr);
            SemaConstexprScalar condition;
            if (!condition_type ||
                (!sema_constexpr_integer_type(condition_type) &&
                 condition_type->kind != TYPE_ENUM)) {
                rcc_error(decl->loc,
                          "static assertion is not an integer constant expression");
            } else if (!sema_eval_constexpr_scalar_expr(
                           decl->static_assert_expr, NULL, 0, &condition) ||
                       condition.is_floating) {
                rcc_error(decl->loc,
                          "static assertion is not an integer constant expression");
            } else if (condition.integer_value == 0) {
                rcc_error(decl->loc, "static assertion failed%s%s",
                          decl->static_assert_message ? ": " : "",
                          decl->static_assert_message
                              ? decl->static_assert_message : "");
            }
            break;
        }
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
            if (decl->var_is_thread_local && !is_global &&
                decl->storage != STORAGE_STATIC &&
                decl->storage != STORAGE_EXTERN) {
                rcc_error(decl->loc,
                          "block-scope thread-local variable requires static or extern storage");
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
            if (!is_global && decl->storage == STORAGE_STATIC) {
                char name[64];
                int written;
                ++static_local_counter;
                written = snprintf(name, sizeof(name),
                                   "__rcc_static_%u_%s",
                                   static_local_counter, decl->name);
                if (written < 0 || (size_t)written >= sizeof(name)) {
                    rcc_error(decl->loc,
                              "static local symbol name exceeds compiler limits");
                } else {
                    decl->link_name = rcc_intern(name);
                    decl->var_is_static_local = true;
                    /* Static locals use the global address path in both
                     * native backends, while their source scope remains
                     * local in the semantic symbol table. */
                    decl->var_is_global = true;
                }
            }
            if (!is_global && decl->storage == STORAGE_EXTERN) {
                if (decl->var_init) {
                    rcc_error(decl->loc,
                              "block-scope extern declaration cannot have an initializer");
                }
                decl->var_is_block_extern = true;
                /* Keep the source declaration in its block scope, but use
                 * external DATA symbol addressing and avoid a stack slot. */
                decl->var_is_global = true;
            }
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
                if (decl->var_is_constexpr) {
                    bool valid_constexpr = false;
                    if (sema_constexpr_scalar_type(decl->type)) {
                        SemaConstexprScalar constexpr_value;
                        valid_constexpr =
                            sema_eval_constexpr_scalar_expr(
                                decl->var_init, NULL, 0,
                                &constexpr_value) &&
                            sema_constexpr_scalar_convert(
                                &constexpr_value, decl->type,
                                &constexpr_value);
                    } else if (sema_constexpr_aggregate_type(decl->type)) {
                        valid_constexpr = sema_validate_constexpr_object(
                            decl->type, decl->var_init);
                    }
                    if (!valid_constexpr) {
                        rcc_error(decl->loc,
                                  "constexpr variable initializer is not a supported constant expression");
                    } else if (sema_constexpr_scalar_type(decl->type)) {
                        SemaConstexprScalar folded;
                        if (sema_eval_constexpr_scalar_object(
                                decl->type, decl->var_init, NULL, 0,
                                &folded) &&
                            sema_constexpr_scalar_convert(
                                &folded, decl->type, &folded)) {
                            if (folded.is_pointer) {
                                /* Address constants must remain relocatable
                                 * AST expressions; replacing them with an
                                 * integer would lose the target symbol. */
                            } else if (folded.is_floating) {
                                decl->var_init->kind = EXPR_FLOAT_LIT;
                                decl->var_init->float_val =
                                    folded.floating_value;
                            } else {
                                decl->var_init->kind = EXPR_INT_LIT;
                                decl->var_init->int_val =
                                    folded.integer_value;
                            }
                            decl->var_init->type = decl->type;
                        }
                    }
                }
                if ((is_global || decl->storage == STORAGE_STATIC) &&
                    decl->type && (type_is_integer(decl->type) ||
                                   decl->type->kind == TYPE_ENUM)) {
                    sema_validate_static_integer_expression(decl->var_init);
                }
            }
            if (decl->var_is_constexpr && !decl->var_init) {
                rcc_error(decl->loc,
                          "constexpr variable requires an initializer");
            }
            sema_prepare_variable_cleanup(decl, is_global);
            sema_prepare_variable_destructor_cleanup(decl);
            current_cxx_namespace = saved_cxx_namespace;
            break;
        }

        case DECL_FUNC: {
            CxxNamespace* saved_cxx_namespace = current_cxx_namespace;
            if (rcc_parser_is_cxx_mode()) {
                current_cxx_namespace = sema_decl_namespace(decl);
            }
            if (decl->func_is_auto_return && !decl->func_body) {
                rcc_error(decl->loc,
                          "auto return type requires a function definition");
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
                current_func_auto_return_pending = decl->func_is_auto_return;
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
                    Symbol* psym = NULL;
                    if (p->decl->name) {
                        psym = symtab_define(g_symtab, p->decl->name,
                                             SYM_PARAM, p->decl->type,
                                             p->decl->loc);
                        psym->decl = p->decl;
                        psym->offset = param_offset;
                    }
                    p->decl->var_offset = param_offset;
                    {
                        int parameter_size = p->decl->type &&
                            p->decl->type->size > 4
                            ? p->decl->type->size : 4;
                        param_offset += (parameter_size + 3) & ~3;
                    }
                }
                current_cxx_method_owner = decl->func_method_owner;
                current_cxx_this_param = decl->func_this_param;
                for (DeclList* p = decl->func_params; p; p = p->next) {
                    if (p->decl && p->decl->param_array_type) {
                        sema_validate_array_parameter_type(
                            p->decl->param_array_type, p->decl->loc, true);
                        sema_vla_bounds(p->decl->param_array_type,
                                        p->decl->loc);
                    }
                }

                /* Analyze body */
                loop_depth = 0;
                current_switch = NULL;
                sema_stmt(decl->func_body);
                sema_validate_cleanup_gotos(decl->func_body);
                sema_validate_vla_gotos(decl->func_body);

                /* Local declarations are installed by the ordinary body
                 * walk.  Deduce auto and decltype(auto) returns only after
                 * that walk, then validate the already-resolved return
                 * expressions against the exact deduced type. */
                if (decl->func_is_auto_return) {
                    Type* deduced_return = NULL;
                    bool saw_return = false;
                    if (sema_deduce_auto_return_stmt(
                            decl->func_body, &deduced_return, &saw_return,
                            decl->func_is_decltype_auto_return)) {
                        if (!saw_return) deduced_return = type_void;
                        decl->type->ret_type = deduced_return;
                        current_func_ret = deduced_return;
                        sema_validate_auto_return_stmt(decl->func_body);
                    }
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
                current_func_auto_return_pending = false;
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
    static_local_counter = 0u;
    cxx_exception_frame_counter = 0u;

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
