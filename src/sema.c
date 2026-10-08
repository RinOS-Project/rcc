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
extern bool cxx_class_is_abstract(
    const CxxClass*) __attribute__((weak));
extern CxxClass* rcc_parser_cxx_find_class(
    const char*) __attribute__((weak));
extern void* cxx_template_instantiate_with_values(
    CxxTemplate*, Type**, const int64_t*, const bool*, int)
    __attribute__((weak));
#endif

static bool sema_cxx_class_is_abstract(const CxxClass* cls) {
#if defined(__GNUC__) || defined(__clang__)
    return cls && cxx_class_is_abstract && cxx_class_is_abstract(cls);
#else
    (void)cls;
    return false;
#endif
}

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
static bool current_func_template_instance = false;
static Decl* current_func_last_param = NULL;
static unsigned static_local_counter = 0u;
static unsigned cxx_exception_frame_counter = 0u;
static Type* current_cxx_method_owner = NULL;
static Decl* current_cxx_this_param = NULL;
static CxxNamespace* current_cxx_namespace = NULL;
static AST* current_ast = NULL;

static Type* sema_decltype_auto_return_type(Expr* expression);
static bool sema_pointee_qualification_preserved(
    const Type* source, const Type* target);

static bool sema_decltype_auto_expression_is_lvalue(Expr* expression) {
    if (!expression) return false;
    switch (expression->kind) {
        case EXPR_IDENT:
        case EXPR_PTR_MEMBER:
        case EXPR_CXX_MEMBER_PTR_ARROW:
        case EXPR_INDEX:
        case EXPR_DEREF:
        case EXPR_STRING_LIT:
            return true;
        case EXPR_MEMBER:
        case EXPR_CXX_MEMBER_PTR_DOT:
            return !expression->cxx_member_xvalue;
        case EXPR_CALL:
            return expression->call_method &&
                expression->call_method->return_type &&
                expression->call_method->return_type->is_reference &&
                !expression->call_method->return_type->is_rvalue_reference;
        case EXPR_COMMA:
            return sema_decltype_auto_expression_is_lvalue(
                expression->binary_rhs);
        case EXPR_COND:
            return expression->cxx_conditional_lvalue;
        default:
            return false;
    }
}

static bool sema_decltype_auto_expression_is_xvalue(Expr* expression) {
    if (!expression) return false;
    if (expression->kind == EXPR_COMMA) {
        return sema_decltype_auto_expression_is_xvalue(
            expression->binary_rhs);
    }
    return (expression->kind == EXPR_CALL && expression->call_method &&
            expression->call_method->return_type &&
            expression->call_method->return_type->is_reference &&
            expression->call_method->return_type->is_rvalue_reference) ||
           (expression->kind == EXPR_COND &&
            expression->cxx_conditional_xvalue) ||
           (expression->kind == EXPR_MEMBER &&
            expression->cxx_member_xvalue) ||
           (expression->kind == EXPR_CXX_MEMBER_PTR_DOT &&
            expression->cxx_member_xvalue);
}

static bool sema_cxx_class_qualified_name(const CxxClass* cls,
                                          char* buffer, size_t capacity) {
    const CxxNamespace* stack[32];
    int count = 0;
    size_t length = 0u;
    if (!cls || !cls->name || !buffer || capacity == 0u) return false;
    for (const CxxNamespace* ns = cls->ns;
         ns && ns->name;
         ns = ns->parent) {
        if (count >= (int)(sizeof(stack) / sizeof(stack[0]))) return false;
        stack[count++] = ns;
    }
    for (int index = count - 1; index >= 0; --index) {
        size_t part_length = strlen(stack[index]->name);
        if (length != 0u) {
            if (length + 2u >= capacity) return false;
            memcpy(buffer + length, "::", 2u);
            length += 2u;
        }
        if (part_length > capacity - length - 1u) return false;
        memcpy(buffer + length, stack[index]->name, part_length);
        length += part_length;
    }
    if (strlen(cls->name) > capacity - length - 1u) return false;
    memcpy(buffer + length, cls->name, strlen(cls->name));
    length += strlen(cls->name);
    buffer[length] = '\0';
    return true;
}

static bool sema_cxx_class_is_friend(const CxxClass* target,
                                     const CxxClass* candidate) {
    char candidate_name[512];
    if (!target || !candidate || !candidate->name) return false;
    if (!sema_cxx_class_qualified_name(candidate, candidate_name,
                                       sizeof(candidate_name))) {
        return false;
    }
    for (int index = 0; index < target->friend_class_count; ++index) {
        const char* friend_name = target->friend_class_names[index];
        const char* normalized = friend_name;
        if (!friend_name) continue;
        while (normalized[0] == ':' && normalized[1] == ':') {
            normalized += 2;
        }
        if (!strstr(normalized, "::")) {
            if (target->ns == candidate->ns &&
                strcmp(normalized, candidate->name) == 0) {
                return true;
            }
        } else if (strcmp(normalized, candidate_name) == 0) {
            return true;
        }
    }
    return false;
}

static bool sema_cxx_class_derives_from(const CxxClass* derived,
                                        const CxxClass* base,
                                        unsigned depth) {
    if (!derived || !base || depth > 32u) return false;
    for (int index = 0; index < derived->base_count; ++index) {
        CxxClass* candidate = derived->bases[index].base;
        if (!candidate) continue;
        if (candidate == base ||
            (candidate->type && base->type &&
             candidate->type->cxx_class == base->type->cxx_class)) {
            return true;
        }
        if (sema_cxx_class_derives_from(candidate, base, depth + 1u)) {
            return true;
        }
    }
    return false;
}

static bool sema_cxx_member_accessible(CxxClass* target,
                                       unsigned char access) {
    CxxClass* context;
    if (access == ACCESS_PUBLIC) return true;
    context = current_cxx_method_owner
        ? current_cxx_method_owner->cxx_class : NULL;
    if (context == target) return true;
    if (access == ACCESS_PROTECTED &&
        sema_cxx_class_derives_from(context, target, 0u)) {
        return true;
    }
    return target && sema_cxx_class_is_friend(target, context);
}

/* A data-member pointer formation also has a constraint on the class named
 * to the left of `::`.  For protected members, that class must be the access
 * class (or one derived from it); merely being inside a derived method does
 * not make `&Base::protected_member` valid. */
static bool sema_cxx_member_pointer_form_accessible(const Expr* expression) {
    CxxClass* declaring;
    CxxClass* designating;
    CxxClass* context;
    CxxClass* access_class;
    unsigned char access;

    if (!expression || !expression->cxx_member_pointer_form) return true;
    declaring = expression->cxx_member_pointer_form_declaring_class;
    designating = expression->cxx_member_pointer_form_designating_class;
    access = expression->cxx_member_pointer_form_access;
    if (access == ACCESS_PUBLIC) return true;
    if (!declaring || !designating ||
        !sema_cxx_member_accessible(declaring, access)) {
        return false;
    }
    if (access != ACCESS_PROTECTED) return true;

    context = current_cxx_method_owner
        ? current_cxx_method_owner->cxx_class : NULL;
    /* C in the protected-member pointer rule is the class containing the
     * member or friend function. For a friend class this is the friend class,
     * not the class that granted friendship. */
    access_class = context;
    if (!access_class) return false;
    return designating == access_class ||
           sema_cxx_class_derives_from(designating, access_class, 0u);
}

static bool sema_cxx_check_qualified_member_access(const char* name,
                                                   Decl* declaration,
                                                   SourceLoc loc) {
    const char* separator;
    char owner_name[512];
    const char* member_name;
    CxxClass* owner;

#if defined(__GNUC__) || defined(__clang__)
    if (!name || !declaration || !rcc_parser_cxx_find_class) return true;
    separator = strrchr(name, ':');
    if (!separator || separator <= name || separator[-1] != ':') return true;
    if ((size_t)(separator - name - 1u) >= sizeof(owner_name)) {
        rcc_error(loc, "qualified member owner exceeds compiler limit");
        return false;
    }
    memcpy(owner_name, name, (size_t)(separator - name - 1u));
    owner_name[separator - name - 1u] = '\0';
    member_name = separator + 1;
    owner = rcc_parser_cxx_find_class(owner_name);
    if (!owner) return true;
    for (struct CxxMember* member = owner->members; member;
         member = member->next) {
        if (member->decl != declaration || !member->decl->name) continue;
        if (!sema_cxx_member_accessible(owner, member->access)) {
            rcc_error(loc, "member '%s' is not accessible", member_name);
            return false;
        }
        return true;
    }
    return true;
#else
    (void)name;
    (void)declaration;
    (void)loc;
    return true;
#endif
}

static CxxClass* sema_cxx_method_owner(Type* object_type,
                                       TypeMethod* method) {
    Decl* declaration = method
        ? (method->source_decl ? method->source_decl : method->function_decl)
        : NULL;
    if (declaration && declaration->func_method_owner &&
        declaration->func_method_owner->cxx_class) {
        return declaration->func_method_owner->cxx_class;
    }
    return object_type ? object_type->cxx_class : NULL;
}

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
static void sema_resolve_function_noexcept(Decl* declaration);
static bool sema_cxx_select_function_pointer_overload(
    Type* target, Expr* expression);

static bool sema_cxx_is_polymorphic(Type* type) {
    CxxClass* cls = type ? type->cxx_class : NULL;
    return cls && (cls->vtable_size > 0 || cls->secondary_vtable_count > 0);
}

static uint64_t sema_cxx_typeinfo_hash_text(uint64_t hash,
                                            const char* text) {
    if (!text) return hash ^ UINT64_C(0xff);
    while (*text) {
        hash ^= (uint8_t)*text++;
        hash *= UINT64_C(1099511628211);
    }
    return hash ^ UINT64_C(0x9d);
}

static uint64_t sema_cxx_typeinfo_hash(const Type* type, unsigned depth) {
    uint64_t hash;
    if (!type || depth > 32u) return UINT64_C(0x4f1bbcdcaa55ee11);
    hash = UINT64_C(1469598103934665603);
    hash ^= (uint64_t)type->kind;
    hash *= UINT64_C(1099511628211);
    hash ^= (uint64_t)(uint32_t)type->size;
    hash *= UINT64_C(1099511628211);
    hash ^= (uint64_t)(uint32_t)type->align;
    hash *= UINT64_C(1099511628211);
    hash ^= type->is_unsigned ? UINT64_C(0x31) : UINT64_C(0x17);
    hash *= UINT64_C(1099511628211);
    switch (type->kind) {
        case TYPE_PTR:
        case TYPE_ARRAY:
        case TYPE_VECTOR:
            hash ^= sema_cxx_typeinfo_hash(type->base, depth + 1u);
            hash *= UINT64_C(1099511628211);
            if (type->kind == TYPE_ARRAY || type->kind == TYPE_VECTOR) {
                hash ^= (uint64_t)(uint32_t)type->array_len;
                hash *= UINT64_C(1099511628211);
            }
            break;
        case TYPE_STRUCT:
        case TYPE_UNION:
            hash = sema_cxx_typeinfo_hash_text(hash, type->tag);
            if (type->cxx_typeinfo_symbol) {
                hash = sema_cxx_typeinfo_hash_text(
                    hash, type->cxx_typeinfo_symbol);
            }
            break;
        case TYPE_ENUM:
            hash = sema_cxx_typeinfo_hash_text(hash, type->enum_tag);
            break;
        case TYPE_FUNC:
            hash ^= sema_cxx_typeinfo_hash(type->ret_type, depth + 1u);
            hash *= UINT64_C(1099511628211);
            break;
        default:
            break;
    }
    return hash;
}

static const char* sema_cxx_typeinfo_symbol(Type* type, SourceLoc loc) {
    char* symbol;
    uint64_t hash;
    if (!type) return NULL;
    if (type->cxx_typeinfo_symbol) return type->cxx_typeinfo_symbol;
    if (type->kind == TYPE_VOID || type->kind == TYPE_FUNC ||
        type->size <= 0) {
        rcc_error(loc,
                  "typeid requires a complete object type operand");
        return NULL;
    }
    hash = sema_cxx_typeinfo_hash(type, 0u);
    symbol = ast_arena_alloc(32u);
    if (snprintf(symbol, 32u, "__rcc_typeinfo_%016llx",
                 (unsigned long long)hash) < 0) {
        rcc_fatal("C++ typeinfo symbol formatting failed");
    }
    type->cxx_typeinfo_symbol = symbol;
    return symbol;
}

static void sema_validate_static_integer_expression(Expr* expression);
static bool sema_compiler_builtin_call(Expr* expr);
static bool sema_atomic_builtin_call(Expr* expr);
static void sema_vla_bounds(Type* type, SourceLoc loc);
static void sema_validate_array_parameter_type(Type* type, SourceLoc loc,
                                               bool is_parameter);
static void sema_validate_restrict_type(Type* type, SourceLoc loc);

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

/* Count fixed non-virtual routes.  Callers that need an accessible route
 * request public edges; ambiguity checks also count private/protected routes
 * because an inaccessible duplicate is still a distinct base subobject. */
static int sema_cxx_nonvirtual_base_paths_impl(
    Type* derived, Type* target, int* adjustment, unsigned depth,
    bool require_public) {
    CxxClass* cls;
    int count = 0;
    if (!derived || !target || depth > 32u) return 0;
    if (derived == target ||
        (derived->cxx_class && target->cxx_class &&
         derived->cxx_class == target->cxx_class)) {
        if (adjustment) *adjustment = 0;
        return 1;
    }
    cls = derived->cxx_class;
    if (!cls || !cls->base_offsets) return 0;
    /* A class can own unrelated virtual bases while this selected route uses
     * only fixed non-virtual edges.  Their virtual layout does not change the
     * byte offset of a non-virtual base subobject. */
    for (int index = 0; index < cls->base_count; ++index) {
        Type* base_type = cls->bases[index].base
            ? cls->bases[index].base->type : NULL;
        int nested_adjustment = 0;
        int nested_count;
        if (!base_type || cls->bases[index].is_virtual ||
            (require_public &&
             cls->bases[index].access != ACCESS_PUBLIC) ||
            cls->base_offsets[index] < 0) {
            continue;
        }
        nested_count = sema_cxx_nonvirtual_base_paths_impl(
            base_type, target, &nested_adjustment, depth + 1u,
            require_public);
        if (nested_count <= 0) continue;
        if (count == 0 && adjustment) {
            *adjustment = cls->base_offsets[index] + nested_adjustment;
        }
        count += nested_count;
        if (count > 1) return 2;
    }
    return count;
}

static int sema_cxx_nonvirtual_public_base_paths(Type* derived, Type* target,
                                                 int* adjustment,
                                                 unsigned depth) {
    return sema_cxx_nonvirtual_base_paths_impl(
        derived, target, adjustment, depth, true);
}

static int sema_cxx_nonvirtual_all_base_paths(Type* derived, Type* target,
                                               unsigned depth) {
    return sema_cxx_nonvirtual_base_paths_impl(
        derived, target, NULL, depth, false);
}

/* Count public routes from a most-derived object through one shared virtual
 * base and then only fixed non-virtual edges to target.  The returned virtual
 * index identifies the runtime vbtable entry; nested_adjustment identifies a
 * target that is itself a non-virtual base of that virtual base. */
static int sema_cxx_virtual_member_base_paths_impl(
    Type* derived, Type* target, int* virtual_index,
    int* nested_adjustment, bool require_public) {
    CxxClass* cls = derived ? derived->cxx_class : NULL;
    int count = 0;
    if (virtual_index) *virtual_index = -1;
    if (nested_adjustment) *nested_adjustment = 0;
    if (!cls || !target || !cls->virtual_bases) return 0;
    for (int index = 0; index < cls->virtual_base_count; ++index) {
        CxxVirtualBaseInfo* base = &cls->virtual_bases[index];
        int nested = 0;
        int paths;
        if (!base->base || !base->base->type ||
            (require_public && !base->public_path)) {
            continue;
        }
        paths = sema_cxx_nonvirtual_base_paths_impl(
            base->base->type, target, &nested, 0u, require_public);
        if (paths <= 0) continue;
        if (count == 0 && paths == 1) {
            if (virtual_index) *virtual_index = index;
            if (nested_adjustment) *nested_adjustment = nested;
        }
        count += paths;
        if (count > 1) return 2;
    }
    return count;
}

static int sema_cxx_public_virtual_member_base_paths(
    Type* derived, Type* target, int* virtual_index,
    int* nested_adjustment) {
    return sema_cxx_virtual_member_base_paths_impl(
        derived, target, virtual_index, nested_adjustment, true);
}

static int sema_cxx_all_virtual_member_base_paths(Type* derived,
                                                  Type* target) {
    return sema_cxx_virtual_member_base_paths_impl(
        derived, target, NULL, NULL, false);
}

/* Pointer-to-member owner conversion requires one accessible non-virtual
 * base subobject.  Count every route first so a private duplicate or a
 * virtual duplicate cannot be hidden by selecting the sole public fixed path. */
static bool sema_cxx_unique_public_nonvirtual_member_owner_path(
    Type* derived, Type* target, int* adjustment) {
    int public_paths = sema_cxx_nonvirtual_public_base_paths(
        derived, target, adjustment, 0u);
    int all_paths = sema_cxx_nonvirtual_all_base_paths(
        derived, target, 0u) +
        sema_cxx_all_virtual_member_base_paths(derived, target);
    return public_paths == 1 && all_paths == 1;
}

static bool sema_cxx_unique_public_base(Type* derived, Type* target,
                                         int* adjustment) {
    CxxClass* cls;
    if (!derived || !target) return false;
    cls = derived->cxx_class;
    if (cls && cls->virtual_base_count == 0) {
        int paths = sema_cxx_nonvirtual_public_base_paths(
            derived, target, adjustment, 0u);
        return paths == 1;
    }
    return sema_cxx_public_base(derived, target, adjustment, 0);
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
    return sema_cxx_unique_public_base(source->base, target->base,
                                       adjustment);
}

/* Return the first virtual-base edge in a public pointer conversion.  The
 * hidden table belongs to the source subobject, so the remaining conversion
 * inside the virtual base can still use its ordinary fixed layout offset. */
static bool sema_cxx_virtual_base_conversion(Type* source, Type* target,
                                             int* virtual_index,
                                             int* nested_adjustment) {
    CxxClass* cls;
    if (!source || !target || source->kind != TYPE_PTR ||
        target->kind != TYPE_PTR || source->is_reference ||
        target->is_reference || !source->base || !target->base) {
        return false;
    }
    cls = source->base->cxx_class;
    if (!cls) return false;
    for (int index = 0; index < cls->virtual_base_count; ++index) {
        CxxVirtualBaseInfo* item = &cls->virtual_bases[index];
        int nested = 0;
        if (!item->base || !item->public_path ||
            !item->base->type ||
            !sema_cxx_public_base(item->base->type, target->base,
                                  &nested, 0)) {
            continue;
        }
        if (virtual_index) *virtual_index = index;
        if (nested_adjustment) *nested_adjustment = nested;
        return true;
    }
    return false;
}

/* The reference form of a derived-to-virtual-base conversion starts with an
 * object lvalue rather than a pointer expression.  Keep its metadata on the
 * same expression so call lowering can read the source object's vbptr just as
 * it does for an explicit pointer conversion. */
static bool sema_cxx_virtual_object_conversion(Type* source, Type* target,
                                               int* virtual_index,
                                               int* nested_adjustment) {
    CxxClass* cls;
    if (!source || !target ||
        (source->kind != TYPE_STRUCT && source->kind != TYPE_UNION) ||
        (target->kind != TYPE_STRUCT && target->kind != TYPE_UNION)) {
        return false;
    }
    cls = source->cxx_class;
    if (!cls) return false;
    for (int index = 0; index < cls->virtual_base_count; ++index) {
        CxxVirtualBaseInfo* item = &cls->virtual_bases[index];
        int nested = 0;
        if (!item->base || !item->public_path || !item->base->type ||
            !sema_cxx_public_base(item->base->type, target,
                                  &nested, 0)) {
            continue;
        }
        if (virtual_index) *virtual_index = index;
        if (nested_adjustment) *nested_adjustment = nested;
        return true;
    }
    return false;
}

static bool sema_cxx_set_pointer_conversion(Expr* expression, Type* source,
                                            Type* target, int* adjustment) {
    int virtual_index;
    int nested_adjustment;
    if (!expression || !source || !target) return false;
    expression->cxx_virtual_base_adjustment = false;
    expression->cxx_virtual_base_source_class = NULL;
    if (sema_cxx_virtual_base_conversion(source, target, &virtual_index,
                                         &nested_adjustment)) {
        expression->cxx_virtual_base_adjustment = true;
        expression->cxx_virtual_base_index = virtual_index;
        expression->cxx_virtual_base_nested_adjustment = nested_adjustment;
        expression->cxx_virtual_base_source_class = source->base->cxx_class;
        expression->cxx_virtual_base_pointer_offset =
            source->base->cxx_class->virtual_base_pointer_offset;
        return true;
    }
    return sema_cxx_pointer_conversion(source, target, adjustment);
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

typedef struct SemaCxxAdlCandidates {
    Decl* declarations[128];
    int count;
    bool overflow;
} SemaCxxAdlCandidates;

static bool sema_cxx_adl_contains(const SemaCxxAdlCandidates* candidates,
                                  Decl* declaration) {
    if (!candidates || !declaration) return false;
    for (int index = 0; index < candidates->count; ++index) {
        if (candidates->declarations[index] == declaration) return true;
    }
    return false;
}

static void sema_cxx_adl_collect_symbol(
    Symbol* symbol, SemaCxxAdlCandidates* candidates) {
    if (!symbol || symbol->kind != SYM_FUNC || !symbol->decl || !candidates) {
        return;
    }
    for (Decl* declaration = symbol->decl; declaration;
         declaration = declaration->func_overload_next) {
        if (declaration->kind != DECL_FUNC ||
            sema_cxx_adl_contains(candidates, declaration)) {
            continue;
        }
        if (candidates->count == (int)(sizeof(candidates->declarations) /
                                       sizeof(candidates->declarations[0]))) {
            candidates->overflow = true;
            return;
        }
        candidates->declarations[candidates->count++] = declaration;
    }
}

static void sema_cxx_adl_collect_namespace(
    const char* namespace_name, const char* name,
    SemaCxxAdlCandidates* candidates) {
    char qualified[512];
    size_t length;

    if (!namespace_name || !*namespace_name || !name || !candidates) return;
    length = strlen(namespace_name);
    if (length + strlen(name) + 3u >= sizeof(qualified)) {
        rcc_error((SourceLoc){"<sema>", 0, 0},
                  "ADL namespace qualification exceeds compiler limits");
        return;
    }
    strcpy(qualified, namespace_name);
    strcpy(qualified + length, "::");
    strcpy(qualified + length + 2u, name);
    sema_cxx_adl_collect_symbol(symtab_lookup(g_symtab, qualified), candidates);
}

static Symbol* sema_cxx_make_function_symbol(
    const char* name, const SemaCxxAdlCandidates* candidates) {
    Symbol* result;
    Decl* head = NULL;
    Decl* tail = NULL;

    if (!name || !candidates || candidates->count == 0) return NULL;
    /* Keep the source declarations owned by the AST.  The linked copies are
     * an analysis-only overload view; each copy retains the original ABI
     * spelling, body, and lifetime metadata for the selected call. */
    for (int index = 0; index < candidates->count; ++index) {
        Decl* copy = ast_arena_alloc(sizeof(*copy));
        *copy = *candidates->declarations[index];
        copy->func_overload_next = NULL;
        if (tail) tail->func_overload_next = copy;
        else head = copy;
        tail = copy;
    }
    result = ast_arena_alloc(sizeof(*result));
    memset(result, 0, sizeof(*result));
    result->name = name;
    result->kind = SYM_FUNC;
    result->type = head ? head->type : NULL;
    result->decl = head;
    return result;
}

static Symbol* sema_cxx_adl_lookup(const char* name, ExprList* arguments) {
    SemaCxxAdlCandidates candidates = {0};

    if (!rcc_parser_is_cxx_mode() || !name) return NULL;
    for (ExprList* item = arguments; item; item = item->next) {
        Type* type = item->expr ? item->expr->type : NULL;
        while (type && type->kind == TYPE_PTR && !type->is_reference) {
            type = type->base;
        }
        if (!type || (type->kind != TYPE_STRUCT &&
                      type->kind != TYPE_UNION) ||
            !type->cxx_namespace) {
            continue;
        }
        sema_cxx_adl_collect_namespace(type->cxx_namespace, name,
                                       &candidates);
    }
    if (candidates.overflow) {
        rcc_error((SourceLoc){"<sema>", 0, 0},
                  "associated ADL overload set exceeds compiler limits");
        return NULL;
    }
    return sema_cxx_make_function_symbol(name, &candidates);
}

static bool sema_cxx_declaration_visible_at(Decl* declaration,
                                             SourceLoc use_loc) {
    if (!current_func_template_instance || !declaration ||
        use_loc.line <= 0 || declaration->loc.line <= 0) {
        return true;
    }
    /* A preprocessed include can have a different filename, so source order
     * is only meaningful within one source file.  The parser already
     * preserves that distinction in SourceLoc. */
    if (declaration->loc.filename && use_loc.filename &&
        strcmp(declaration->loc.filename, use_loc.filename) != 0) {
        return true;
    }
    if (declaration->loc.line != use_loc.line) {
        return declaration->loc.line < use_loc.line;
    }
    return declaration->loc.column <= use_loc.column;
}

static Symbol* sema_cxx_visible_symbol(Symbol* symbol, SourceLoc use_loc) {
    SemaCxxAdlCandidates candidates = {0};
    if (!symbol || !current_func_template_instance) return symbol;
    if (symbol->kind != SYM_FUNC) {
        return sema_cxx_declaration_visible_at(symbol->decl, use_loc)
            ? symbol : NULL;
    }
    for (Decl* declaration = symbol->decl; declaration;
         declaration = declaration->func_overload_next) {
        if (!sema_cxx_declaration_visible_at(declaration, use_loc)) continue;
        if (candidates.count == (int)(sizeof(candidates.declarations) /
                                       sizeof(candidates.declarations[0]))) {
            candidates.overflow = true;
            break;
        }
        candidates.declarations[candidates.count++] = declaration;
    }
    if (candidates.overflow) {
        rcc_error(use_loc,
                  "ordinary lookup overload set exceeds compiler limits");
        return NULL;
    }
    if (candidates.count == 0) return NULL;
    return sema_cxx_make_function_symbol(symbol->name, &candidates);
}

static bool sema_cxx_namespace_has_inline_child(const CxxNamespace* ns) {
    for (const CxxNamespace* child = ns ? ns->children : NULL;
         child; child = child->next) {
        if (child->is_inline_namespace) return true;
    }
    return false;
}

static Symbol* sema_cxx_lookup_namespace(CxxNamespace* ns,
                                          const char* name,
                                          CxxNamespace** visited,
                                          int visited_count,
                                          SourceLoc use_loc) {
    char qualified[512];
    Symbol* result = NULL;
    Symbol* non_function = NULL;
    SemaCxxAdlCandidates function_candidates = {0};
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
            result = sema_cxx_visible_symbol(
                symtab_lookup(g_symtab, qualified), use_loc);
        }
    } else {
        result = symtab_lookup(g_symtab, name);
    }
    if (result) {
        /* Preserve the established direct/using lookup precedence for
         * ordinary namespaces.  Only an enclosing namespace with an inline
         * child needs the merged overload set below. */
        if (result->kind != SYM_FUNC ||
            !sema_cxx_namespace_has_inline_child(ns)) {
            return result;
        }
        sema_cxx_adl_collect_symbol(result, &function_candidates);
    }

    for (int index = 0; index < ns->using_declaration_count; ++index) {
        const char* target = ns->using_declarations[index];
        const char* final_component = strrchr(target, ':');
        final_component = final_component ? final_component + 1 : target;
        if (strcmp(final_component, name) != 0) continue;
        result = sema_cxx_visible_symbol(symtab_lookup(g_symtab, target),
                                         use_loc);
        if (!result) continue;
        if (result->kind == SYM_FUNC) {
            sema_cxx_adl_collect_symbol(result, &function_candidates);
        } else if (!non_function) {
            non_function = result;
        }
    }
    for (int index = 0; index < ns->using_namespace_count; ++index) {
        result = sema_cxx_lookup_namespace(ns->using_namespaces[index], name,
                                           visited, visited_count, use_loc);
        if (!result) continue;
        if (result->kind == SYM_FUNC) {
            sema_cxx_adl_collect_symbol(result, &function_candidates);
        } else if (!non_function) {
            non_function = result;
        }
    }
    for (CxxNamespace* child = ns->children; child; child = child->next) {
        if (!child->is_inline_namespace) continue;
        result = sema_cxx_lookup_namespace(child, name, visited,
                                           visited_count, use_loc);
        if (!result) continue;
        if (result->kind == SYM_FUNC) {
            sema_cxx_adl_collect_symbol(result, &function_candidates);
        } else if (!non_function) {
            non_function = result;
        }
    }
    if (function_candidates.overflow) {
        rcc_error((SourceLoc){"<sema>", 0, 0},
                  "using-namespace overload set exceeds compiler limits");
        return NULL;
    }
    if (function_candidates.count > 0) {
        return sema_cxx_make_function_symbol(name, &function_candidates);
    }
    return non_function;
}

/* Resolve a namespace-qualified expression such as `api::make_value` before
 * falling back to ordinary identifier lookup.  The existing direct symbol
 * lookup is sufficient for declarations emitted under their complete link
 * spelling, but it cannot see a name introduced through an inline namespace
 * below `api`; route the final component through the namespace lookup logic. */
static Symbol* sema_cxx_lookup_qualified_name(const char* name,
                                               SourceLoc use_loc) {
    char buffer[512];
    char* separator;
    char* cursor;
    const char* final_name;
    CxxNamespace* global_namespace = sema_cxx_global_namespace();
    CxxNamespace* namespace;
    CxxNamespace* visited[32] = { 0 };

    if (!global_namespace || !name || strlen(name) >= sizeof(buffer)) {
        return NULL;
    }
    strcpy(buffer, name);
    while (buffer[0] == ':' && buffer[1] == ':') {
        memmove(buffer, buffer + 2, strlen(buffer + 2) + 1u);
    }
    separator = NULL;
    for (cursor = buffer; (cursor = strstr(cursor, "::")) != NULL;
         cursor += 2) {
        separator = cursor;
    }
    if (!separator || separator == buffer) {
        return NULL;
    }
    *separator = '\0';
    final_name = separator + 2;
    if (!*final_name) return NULL;
    namespace = cxx_namespace_find(global_namespace, buffer);
    if (!namespace) return NULL;
    return sema_cxx_lookup_namespace(namespace, final_name, visited, 0,
                                     use_loc);
}

static Symbol* sema_cxx_lookup_name(const char* name, SourceLoc use_loc) {
    Symbol* symbol;
    CxxNamespace* visited[32] = { 0 };
    CxxNamespace* ns;

    if (!name) return NULL;
    symbol = sema_cxx_visible_symbol(symtab_lookup(g_symtab, name), use_loc);
    if (symbol || !rcc_parser_is_cxx_mode()) {
        return symbol;
    }
    if (strchr(name, ':')) {
        return sema_cxx_lookup_qualified_name(name, use_loc);
    }
    for (ns = current_cxx_namespace ? current_cxx_namespace
                                    : sema_cxx_global_namespace();
         ns; ns = ns->parent) {
        symbol = sema_cxx_lookup_namespace(ns, name, visited, 0, use_loc);
        if (symbol) return symbol;
    }
    return NULL;
}

static CxxNamespace* sema_decl_namespace(Decl* decl) {
    CxxNamespace* global_namespace = sema_cxx_global_namespace();
    if (!global_namespace || !decl) return global_namespace;
    if (decl->kind == DECL_FUNC && decl->func_cxx_namespace) {
        CxxNamespace* defining_namespace = cxx_namespace_find(
            global_namespace, decl->func_cxx_namespace);
        if (defining_namespace) return defining_namespace;
    }
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

static bool sema_decl_has_scope_cleanup(const Decl* declaration) {
    if (!declaration || declaration->kind != DECL_VAR) return false;
    return declaration->var_cleanup || declaration->var_cleanups;
}

static bool sema_cleanup_plan_has_unregistered_call(
    const CxxCleanupPlan* plan) {
    for (; plan; plan = plan->next) {
        if (plan->kind == CXX_CLEANUP_ARRAY_LOOP) {
            if (sema_cleanup_plan_has_unregistered_call(plan->body)) {
                return true;
            }
        } else {
            Expr* function = plan->expression &&
                    plan->expression->kind == EXPR_CALL
                ? plan->expression->call_func : NULL;
            Decl* destructor = function && function->kind == EXPR_IDENT
                ? function->ident_decl : NULL;
            if (!destructor || !destructor->func_is_cxx_destructor) {
                return true;
            }
        }
    }
    return false;
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
            return !label_scope || !sema_decl_has_scope_cleanup(statement->decl);
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
           strcmp(name, "__atomic_compare_exchange_n") == 0 ||
           strcmp(name, "__atomic_load") == 0 ||
           strcmp(name, "__atomic_store") == 0 ||
           strcmp(name, "__atomic_exchange") == 0 ||
           strcmp(name, "__atomic_compare_exchange") == 0;
}

/* ═══════════════════════════════════════
 * Type Checking Helpers
 * ═══════════════════════════════════════ */

static bool is_lvalue(Expr* e) {
    if (!e) return false;
    if (e && e->kind == EXPR_CAST && e->type && e->type->is_reference &&
        !e->type->is_rvalue_reference &&
        (e->cxx_cast_kind == CXX_CAST_NONE ||
         e->cxx_cast_kind == CXX_CAST_STATIC ||
         e->cxx_cast_kind == CXX_CAST_CONST ||
         e->cxx_cast_kind == CXX_CAST_DYNAMIC)) {
        return is_lvalue(e->cast_expr);
    }
    switch (e->kind) {
        case EXPR_IDENT:
        case EXPR_DEREF:
        case EXPR_INDEX:
        case EXPR_PTR_MEMBER:
        case EXPR_CXX_TYPEID:
            return true;
        case EXPR_COMPOUND:
            return !rcc_parser_is_cxx_mode();
        case EXPR_STRING_LIT:
            return rcc_parser_is_cxx_mode();
        case EXPR_MEMBER:
            return !e->cxx_member_xvalue;
        case EXPR_CXX_MEMBER_PTR_DOT:
            return !e->cxx_member_xvalue;
        case EXPR_CXX_MEMBER_PTR_ARROW:
            return true;
        case EXPR_COND:
            return e->cxx_conditional_lvalue;
        case EXPR_COMMA:
            return rcc_parser_is_cxx_mode() && is_lvalue(e->binary_rhs);
        case EXPR_CALL:
            if (e->call_method && e->call_method->return_type &&
                e->call_method->return_type->is_reference) {
                return !e->call_method->return_type->is_rvalue_reference;
            }
            return e->type && e->type->is_reference &&
                   !e->type->is_rvalue_reference;
        default:
            return false;
    }
}

static bool is_xvalue(Expr* expression) {
    if (!expression) return false;
    if (expression->kind == EXPR_COMMA) {
        return rcc_parser_is_cxx_mode() &&
               is_xvalue(expression->binary_rhs);
    }
    if (expression->kind == EXPR_COND) {
        return expression->cxx_conditional_xvalue;
    }
    if (expression->kind == EXPR_MEMBER) {
        return expression->cxx_member_xvalue;
    }
    if (expression->kind == EXPR_CXX_MEMBER_PTR_DOT) {
        return expression->cxx_member_xvalue;
    }
    if (expression->kind == EXPR_CALL && expression->call_method &&
        expression->call_method->return_type &&
        expression->call_method->return_type->is_reference) {
        return expression->call_method->return_type->is_rvalue_reference;
    }
    return expression->type && expression->type->is_reference &&
           expression->type->is_rvalue_reference &&
           (expression->kind == EXPR_CAST ||
            expression->kind == EXPR_CALL);
}

/* Find a prvalue object materialized by a direct rvalue-reference cast.
 * Named xvalues and calls returning references do not own a temporary here. */
static Expr* sema_cxx_reference_temporary_source(Expr* expression) {
    Type* cast_type;
    Expr* source;
    if (!expression || expression->kind != EXPR_CAST) return NULL;
    cast_type = expression->cast_type;
    source = expression->cast_expr;
    if (!cast_type || !cast_type->is_reference ||
        !cast_type->is_rvalue_reference || !source) {
        return NULL;
    }
    if (!is_lvalue(source) && !is_xvalue(source)) return source;
    return sema_cxx_reference_temporary_source(source);
}

/* Find the complete class prvalue whose lifetime is extended through a
 * static reference binding.  A reference to an xvalue member keeps the
 * complete temporary alive, and the owner may be wrapped in a direct
 * rvalue-reference cast, comma expression, or same-type conditional. */
static Expr* sema_cxx_static_reference_temporary_source(Expr* expression) {
    Type* type;
    if (!expression) return NULL;
    type = expression->type;
    if (type && type->kind == TYPE_PTR && type->is_reference) {
        type = type->base;
    }
    if (type && (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) &&
        !is_lvalue(expression) && !is_xvalue(expression)) {
        return expression;
    }
    if (expression->kind == EXPR_CAST && expression->cast_type &&
        expression->cast_type->is_reference &&
        expression->cast_type->is_rvalue_reference) {
        return sema_cxx_static_reference_temporary_source(
            expression->cast_expr);
    }
    if (expression->kind == EXPR_MEMBER &&
        expression->cxx_member_xvalue) {
        return sema_cxx_static_reference_temporary_source(
            expression->member_base);
    }
    if (expression->kind == EXPR_CXX_MEMBER_PTR_DOT &&
        expression->cxx_member_xvalue) {
        return sema_cxx_static_reference_temporary_source(
            expression->binary_lhs);
    }
    if (expression->kind == EXPR_COMMA && !is_lvalue(expression)) {
        Expr* right = sema_cxx_static_reference_temporary_source(
            expression->binary_rhs);
        return right ? expression : NULL;
    }
    if (expression->kind == EXPR_COND &&
        expression->cxx_conditional_xvalue && type &&
        (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION)) {
        Expr* then_source = sema_cxx_static_reference_temporary_source(
            expression->cond_then);
        Expr* else_source = sema_cxx_static_reference_temporary_source(
            expression->cond_else);
        Type* then_type = then_source ? then_source->type : NULL;
        Type* else_type = else_source ? else_source->type : NULL;
        if (then_type && then_type->kind == TYPE_PTR &&
            then_type->is_reference) then_type = then_type->base;
        if (else_type && else_type->kind == TYPE_PTR &&
            else_type->is_reference) else_type = else_type->base;
        if (then_source && else_source && then_type && else_type &&
            type_is_compatible(then_type, else_type) &&
            then_type->is_const == else_type->is_const &&
            then_type->is_volatile == else_type->is_volatile) {
            return expression;
        }
    }
    return NULL;
}

static bool sema_cxx_static_reference_has_temporary_source(
    Expr* expression) {
    Type* type;
    if (!expression) return false;
    type = expression->type;
    if (type && type->kind == TYPE_PTR && type->is_reference) {
        type = type->base;
    }
    if (type && (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) &&
        !is_lvalue(expression) && !is_xvalue(expression)) {
        return true;
    }
    switch (expression->kind) {
        case EXPR_CAST:
            return expression->cast_type &&
                   expression->cast_type->is_reference &&
                   expression->cast_type->is_rvalue_reference &&
                   sema_cxx_static_reference_has_temporary_source(
                       expression->cast_expr);
        case EXPR_MEMBER:
            return expression->cxx_member_xvalue &&
                   sema_cxx_static_reference_has_temporary_source(
                       expression->member_base);
        case EXPR_CXX_MEMBER_PTR_DOT:
            return expression->cxx_member_xvalue &&
                   sema_cxx_static_reference_has_temporary_source(
                       expression->binary_lhs);
        case EXPR_COMMA:
            return is_xvalue(expression) &&
                   sema_cxx_static_reference_has_temporary_source(
                       expression->binary_rhs);
        case EXPR_COND:
            return expression->cxx_conditional_xvalue &&
                   (sema_cxx_static_reference_has_temporary_source(
                        expression->cond_then) ||
                    sema_cxx_static_reference_has_temporary_source(
                        expression->cond_else));
        default:
            return false;
    }
}

static bool sema_cxx_reference_subobject_path(Expr* expression,
                                               Expr* complete_object) {
    if (!expression || !complete_object || expression == complete_object) {
        return false;
    }
    switch (expression->kind) {
        case EXPR_MEMBER:
            if (expression->member_base == complete_object) return true;
            return sema_cxx_reference_subobject_path(
                expression->member_base, complete_object);
        case EXPR_CXX_MEMBER_PTR_DOT:
            return sema_cxx_reference_subobject_path(
                expression->binary_lhs, complete_object);
        case EXPR_CAST:
            return sema_cxx_reference_subobject_path(
                expression->cast_expr, complete_object);
        case EXPR_COMMA:
            return sema_cxx_reference_subobject_path(
                       expression->binary_lhs, complete_object) ||
                   sema_cxx_reference_subobject_path(
                       expression->binary_rhs, complete_object);
        case EXPR_COND:
            return sema_cxx_reference_subobject_path(
                       expression->cond_then, complete_object) ||
                   sema_cxx_reference_subobject_path(
                       expression->cond_else, complete_object);
        default:
            return false;
    }
}

static bool sema_cxx_same_glvalue_type(Type* left, Type* right,
                                       unsigned depth) {
    if (!left || !right || depth > 64u ||
        !type_is_compatible(left, right) ||
        left->is_const != right->is_const ||
        left->is_volatile != right->is_volatile ||
        left->is_atomic != right->is_atomic ||
        left->is_restrict != right->is_restrict) {
        return false;
    }
    if (left->kind == TYPE_ARRAY && left->array_len != right->array_len) {
        return false;
    }
    if (left->kind == TYPE_PTR || left->kind == TYPE_ARRAY ||
        left->kind == TYPE_VECTOR) {
        return sema_cxx_same_glvalue_type(left->base, right->base,
                                           depth + 1u);
    }
    return true;
}

static bool is_modifiable_lvalue(Expr* expression) {
    Type* type;
    if (!expression || (!is_lvalue(expression) &&
        !(rcc_parser_is_cxx_mode() && is_xvalue(expression)))) return false;
    type = expression->type;
    return type && !type->is_const && type->kind != TYPE_ARRAY &&
           type->kind != TYPE_FUNC;
}

/* A class xvalue can still be the receiver of an implicit aggregate copy
 * assignment in the bounded frontend. Built-in scalar assignment requires an
 * actual lvalue. */
static bool is_modifiable_class_xvalue(Expr* expression) {
    Type* type;
    if (!rcc_parser_is_cxx_mode() || !is_xvalue(expression)) return false;
    type = expression->type;
    if (type && type->kind == TYPE_PTR && type->is_reference) {
        type = type->base;
    }
    return type && !type->is_const &&
           (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION);
}

static bool is_modifiable_builtin_assignment_target(Expr* expression) {
    if (!is_modifiable_lvalue(expression)) return false;
    return !rcc_parser_is_cxx_mode() || !is_xvalue(expression) ||
           is_modifiable_class_xvalue(expression);
}

static Type* get_pointer_base(Type* t) {
    if (t->kind == TYPE_PTR) return t->base;
    if (t->kind == TYPE_ARRAY) return t->base;
    if (t->kind == TYPE_VECTOR) return t->base;
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

static Type* sema_cxx_object_type(Type* type) {
    if (type && type->kind == TYPE_PTR && type->is_reference) {
        return type->base;
    }
    return type;
}

static bool sema_is_integer_type(Type* type) {
    return type && (type_is_integer(type) || type->kind == TYPE_ENUM);
}

/* The inline-assembly backend deliberately implements a small, explicit
 * fixed-register ABI.  Do not let GCC-style constraints which the backend
 * cannot materialize fall through as an ignored operand. */
static bool sema_asm_register_name_supported(const char* name,
                                              bool output,
                                              SourceLoc loc) {
    if (!name || !name[0]) {
        rcc_error(loc, "inline asm constraint has no register class");
        return false;
    }
    if (g_opts.target_arch == ARCH_X86) {
        if (strlen(name) == 1 && strchr("abcdSD", name[0])) return true;
        if (strcmp(name, "r") == 0 || strcmp(name, "X") == 0) return true;
        rcc_error(loc, "unsupported i686 inline asm %s register constraint '%s'",
                  output ? "output" : "input", name);
        return false;
    }
    if (strlen(name) == 1 && strchr("abcdSD", name[0])) return true;
    if (strcmp(name, "r") == 0 || strcmp(name, "X") == 0) return true;
    if (!output && (strcmp(name, "{eax}") == 0 ||
                    strcmp(name, "{rax}") == 0 ||
                    strcmp(name, "{ebx}") == 0 ||
                    strcmp(name, "{rbx}") == 0 ||
                    strcmp(name, "{ecx}") == 0 ||
                    strcmp(name, "{rcx}") == 0 ||
                    strcmp(name, "{edx}") == 0 ||
                    strcmp(name, "{rdx}") == 0 ||
                    strcmp(name, "{esi}") == 0 ||
                    strcmp(name, "{rsi}") == 0 ||
                    strcmp(name, "{edi}") == 0 ||
                    strcmp(name, "{rdi}") == 0)) {
        return true;
    }
    rcc_error(loc, "unsupported AMD64 inline asm %s register constraint '%s'",
              output ? "output" : "input", name);
    return false;
}

static bool sema_asm_immediate_name(const char* name) {
    return name && (strcmp(name, "i") == 0 || strcmp(name, "n") == 0);
}

static bool sema_asm_port_name(const char* name) {
    return name && strcmp(name, "Nd") == 0;
}

static bool sema_asm_constraint_supported(const char* constraint,
                                          bool output, SourceLoc loc) {
    const char* name;
    char mode;
    bool early_clobber = false;

    if (!constraint || !constraint[0]) {
        rcc_error(loc, "inline asm %s constraint is empty",
                  output ? "output" : "input");
        return false;
    }
    mode = constraint[0];
    if (output) {
        if (mode != '=' && mode != '+') {
            rcc_error(loc,
                      "inline asm output constraint '%s' must start with '=' or '+'",
                      constraint);
            return false;
        }
    } else if (mode == '=' || mode == '+' || mode == '&') {
        rcc_error(loc,
                  "inline asm input constraint '%s' has an output modifier",
                  constraint);
        return false;
    }

    name = output ? constraint + 1 : constraint;
    if (output && *name == '&') {
        early_clobber = true;
        ++name;
    }
    if (early_clobber && mode != '=' && mode != '+') {
        rcc_error(loc, "malformed inline asm early-clobber constraint '%s'",
                  constraint);
        return false;
    }
    if (strchr(name, '=') || strchr(name, '+') || strchr(name, '&')) {
        rcc_error(loc, "malformed inline asm constraint '%s'", constraint);
        return false;
    }
    if (!output && (sema_asm_immediate_name(name) ||
                    sema_asm_port_name(name))) return true;
    if (output && sema_asm_port_name(name)) {
        rcc_error(loc, "inline asm port constraint 'Nd' is input-only");
        return false;
    }
    return sema_asm_register_name_supported(name, output, loc);
}

static bool sema_asm_scalar_operand(Type* type) {
    return type && (sema_is_integer_type(type) || type_is_pointer(type) ||
                    type->kind == TYPE_NULLPTR);
}

static bool sema_asm_clobber_supported(const char* name, SourceLoc loc) {
    static const char* const x86_names[] = {
        "eax", "ax", "al", "ah", "ebx", "bx", "bl", "bh",
        "ecx", "cx", "cl", "ch", "edx", "dx", "dl", "dh",
        "esi", "si", "edi", "di", "cc", "memory"
    };
    static const char* const x64_names[] = {
        "rax", "eax", "ax", "al", "ah", "rbx", "ebx", "bx", "bl",
        "bh", "rcx", "ecx", "cx", "cl", "ch", "rdx", "edx", "dx",
        "dl", "dh", "rsi", "esi", "si", "rdi", "edi", "di", "r8",
        "r9", "r10", "r11", "cc", "memory"
    };
    const char* const* names = g_opts.target_arch == ARCH_X86
        ? x86_names : x64_names;
    size_t count = g_opts.target_arch == ARCH_X86
        ? sizeof(x86_names) / sizeof(x86_names[0])
        : sizeof(x64_names) / sizeof(x64_names[0]);
    for (size_t index = 0; index < count; ++index) {
        if (strcmp(name, names[index]) == 0) return true;
    }
    rcc_error(loc, "unsupported %s inline asm clobber '%s'",
              g_opts.target_arch == ARCH_X86 ? "i686" : "AMD64", name);
    return false;
}

/* The bounded backend assigns every fixed-register operand directly.  Keep
 * the conflict check here, before code generation can silently let one pop
 * overwrite another operand.  Each target materializes a generic input class
 * in one dedicated scratch register, so duplicate generic inputs are conflicts
 * as well. */
static int sema_asm_fixed_register_id(const char* name) {
    if (!name || !name[0]) return -1;
    if (strcmp(name, "r") == 0 || strcmp(name, "X") == 0) {
        return g_opts.target_arch == ARCH_X64 ? 6 : 2;
    }
    if (strcmp(name, "a") == 0 || strcmp(name, "eax") == 0 ||
        strcmp(name, "rax") == 0 || strcmp(name, "ax") == 0 ||
        strcmp(name, "al") == 0 || strcmp(name, "ah") == 0 ||
        strcmp(name, "{eax}") == 0 || strcmp(name, "{rax}") == 0) return 0;
    if (strcmp(name, "b") == 0 || strcmp(name, "ebx") == 0 ||
        strcmp(name, "rbx") == 0 || strcmp(name, "bx") == 0 ||
        strcmp(name, "bl") == 0 || strcmp(name, "bh") == 0 ||
        strcmp(name, "{ebx}") == 0 || strcmp(name, "{rbx}") == 0) return 1;
    if (strcmp(name, "c") == 0 || strcmp(name, "ecx") == 0 ||
        strcmp(name, "rcx") == 0 || strcmp(name, "cx") == 0 ||
        strcmp(name, "cl") == 0 || strcmp(name, "ch") == 0 ||
        strcmp(name, "{ecx}") == 0 || strcmp(name, "{rcx}") == 0) return 2;
    if (strcmp(name, "d") == 0 || strcmp(name, "edx") == 0 ||
        strcmp(name, "rdx") == 0 || strcmp(name, "dx") == 0 ||
        strcmp(name, "dl") == 0 || strcmp(name, "dh") == 0 ||
        strcmp(name, "{edx}") == 0 || strcmp(name, "{rdx}") == 0) return 3;
    if (strcmp(name, "S") == 0 || strcmp(name, "esi") == 0 ||
        strcmp(name, "rsi") == 0 || strcmp(name, "si") == 0 ||
        strcmp(name, "{esi}") == 0 || strcmp(name, "{rsi}") == 0) return 4;
    if (strcmp(name, "D") == 0 || strcmp(name, "edi") == 0 ||
        strcmp(name, "rdi") == 0 || strcmp(name, "di") == 0 ||
        strcmp(name, "{edi}") == 0 || strcmp(name, "{rdi}") == 0) return 5;
    if (strcmp(name, "r10") == 0 || strcmp(name, "{r10}") == 0) return 6;
    return -1;
}

static const char* sema_asm_constraint_name(const char* constraint) {
    const char* name = constraint;
    if (!name) return NULL;
    while (*name == '=' || *name == '+' || *name == '&') ++name;
    return name;
}

static bool sema_asm_nd_uses_immediate(const AsmOperand* operand) {
    int64_t value;
    const char* name = operand
        ? sema_asm_constraint_name(operand->constraint) : NULL;
    return sema_asm_port_name(name) && operand->expr &&
           expr_eval_integer_constant(operand->expr, &value) &&
           value >= 0 && value <= 255;
}

static int sema_asm_operand_fixed_register_id(const AsmOperand* operand) {
    const char* name = operand
        ? sema_asm_constraint_name(operand->constraint) : NULL;
    if (sema_asm_port_name(name)) {
        return sema_asm_nd_uses_immediate(operand) ? -1 : 3;
    }
    return sema_asm_fixed_register_id(name);
}

static void sema_asm_validate_conflicts(Stmt* stmt) {
    AsmOperand* output;
    AsmOperand* input;
    if (!stmt) return;
    for (output = stmt->asm_outputs; output; output = output->next) {
        int output_id = sema_asm_operand_fixed_register_id(output);
        for (AsmOperand* later = output->next; later; later = later->next) {
            if (output_id >= 0 && output_id ==
                sema_asm_operand_fixed_register_id(later)) {
                rcc_error(stmt->loc,
                          "inline asm outputs use the same fixed register");
            }
        }
        for (input = stmt->asm_inputs; input; input = input->next) {
            int input_id = sema_asm_operand_fixed_register_id(input);
            if (output_id < 0 || output_id != input_id) continue;
            if (output->constraint[0] != '=') {
                rcc_error(stmt->loc,
                          "inline asm read-write output conflicts with a duplicate fixed-register input");
            }
        }
    }
    for (input = stmt->asm_inputs; input; input = input->next) {
        int input_id = sema_asm_operand_fixed_register_id(input);
        for (AsmOperand* later = input->next; later; later = later->next) {
            if (input_id >= 0 && input_id ==
                sema_asm_operand_fixed_register_id(later)) {
                rcc_error(stmt->loc,
                          "inline asm inputs use the same fixed register");
            }
        }
    }
    for (AsmClobber* clobber = stmt->asm_clobbers; clobber;
         clobber = clobber->next) {
        int clobber_id = sema_asm_fixed_register_id(clobber->reg);
        for (AsmClobber* earlier = stmt->asm_clobbers; earlier != clobber;
             earlier = earlier->next) {
            int earlier_id = sema_asm_fixed_register_id(earlier->reg);
            if ((clobber_id >= 0 && clobber_id == earlier_id) ||
                (clobber_id < 0 && earlier_id < 0 &&
                 strcmp(clobber->reg, earlier->reg) == 0)) {
                rcc_error(stmt->loc,
                          "inline asm clobbers list the same register twice");
                break;
            }
        }
        for (output = stmt->asm_outputs; output; output = output->next) {
            if (clobber_id >= 0 && clobber_id ==
                sema_asm_operand_fixed_register_id(output)) {
                rcc_error(stmt->loc,
                          "inline asm clobber conflicts with an operand fixed register");
            }
        }
        for (input = stmt->asm_inputs; input; input = input->next) {
            if (clobber_id >= 0 && clobber_id ==
                sema_asm_operand_fixed_register_id(input)) {
                rcc_error(stmt->loc,
                          "inline asm clobber conflicts with an operand fixed register");
            }
        }
    }
}

static void sema_asm_stmt(Stmt* stmt) {
    const char* cursor;
    int output_count = 0;
    int input_count = 0;
    int total_count;

    if (!stmt) return;
    for (AsmOperand* op = stmt->asm_outputs; op; op = op->next) {
        ++output_count;
    }
    for (AsmOperand* op = stmt->asm_inputs; op; op = op->next) {
        ++input_count;
    }
    total_count = output_count + input_count;
    cursor = stmt->asm_template ? stmt->asm_template : "";
    while (*cursor) {
        if (*cursor != '%') {
            ++cursor;
            continue;
        }
        if (cursor[1] == '%') {
            cursor += 2;
            continue;
        }
        if (cursor[1] != 'b' && cursor[1] != 'w' && cursor[1] != 'k' &&
            (cursor[1] < '0' || cursor[1] > '9')) {
            rcc_error(stmt->loc,
                      "inline asm placeholder must be %%, %%N, %%bN, %%wN, or %%kN");
            ++cursor;
            continue;
        }
        {
            uint64_t index = 0u;
            const char* digit = cursor +
                ((cursor[1] == 'b' || cursor[1] == 'w' || cursor[1] == 'k')
                     ? 2 : 1);
            if (*digit < '0' || *digit > '9') {
                rcc_error(stmt->loc,
                          "inline asm width modifier must be followed by an operand index");
                ++cursor;
                continue;
            }
            while (*digit >= '0' && *digit <= '9') {
                if (index > (UINT64_MAX - 9u) / 10u) {
                    index = UINT64_MAX;
                    break;
                }
                index = index * 10u + (uint64_t)(*digit - '0');
                ++digit;
            }
            if (index >= (uint64_t)total_count) {
                rcc_error(stmt->loc,
                          "inline asm operand placeholder index is out of range");
            }
            cursor = digit;
        }
    }
    for (AsmOperand* op = stmt->asm_outputs; op; op = op->next) {
        (void)sema_asm_constraint_supported(op->constraint, true, stmt->loc);
        if (!op->expr) {
            rcc_error(stmt->loc, "inline asm output has no expression");
        } else if (!is_modifiable_lvalue(op->expr)) {
            rcc_error(op->expr->loc,
                      "inline asm output expression must be a modifiable lvalue");
        } else if (!sema_asm_scalar_operand(op->expr->type)) {
            rcc_error(op->expr->loc,
                      "inline asm output expression must have scalar integer or pointer type");
        }
    }
    for (AsmOperand* op = stmt->asm_inputs; op; op = op->next) {
        (void)sema_asm_constraint_supported(op->constraint, false, stmt->loc);
        if (!op->expr) {
            rcc_error(stmt->loc, "inline asm input has no expression");
        } else if (!sema_asm_scalar_operand(op->expr->type)) {
            rcc_error(op->expr->loc,
                      "inline asm input expression must have scalar integer or pointer type");
        } else if (sema_asm_immediate_name(op->constraint)) {
            int64_t immediate_value;
            if (!sema_is_integer_type(op->expr->type) ||
                !expr_eval_integer_constant(op->expr, &immediate_value)) {
                rcc_error(op->expr->loc,
                          "inline asm immediate input must be an integer constant expression");
            }
        } else if (sema_asm_port_name(op->constraint) &&
                   !sema_is_integer_type(op->expr->type)) {
            rcc_error(op->expr->loc,
                      "inline asm port input must have integer type");
        }
    }
    for (AsmClobber* clobber = stmt->asm_clobbers; clobber;
         clobber = clobber->next) {
        (void)sema_asm_clobber_supported(clobber->reg, stmt->loc);
    }
    sema_asm_validate_conflicts(stmt);
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
    if (type && type->kind == TYPE_ENUM) {
        return type->enum_underlying_type
            ? type->enum_underlying_type : type_int;
    }
    if (!type || type->kind < TYPE_INT) {
        return type_int;
    }
    return type;
}

static bool sema_is_arithmetic_type(Type* type) {
    return type_is_arithmetic(type) ||
           (type && type->kind == TYPE_ENUM &&
            !sema_is_scoped_enum(type));
}

static Type* sema_common_arithmetic_type(Type* left, Type* right) {
    if (left && left->kind == TYPE_ENUM) left = sema_integer_promotion(left);
    if (right && right->kind == TYPE_ENUM) right = sema_integer_promotion(right);
    return type_common(left, right);
}

/* Rank only the standard conversion that follows a user-defined conversion
 * operator.  A second user-defined conversion is never considered here.
 * Keeping this helper separate from cxx_conversion_rank() prevents recursive
 * operator lookup while still allowing `operator int()` to initialize a
 * `long` parameter and allowing an exact result to win overload resolution. */
static int sema_cxx_conversion_result_rank(Type* source, Type* target) {
    Type* source_base;
    Type* target_base;
    bool reference_target;
    if (!source || !target) return -1;
    reference_target = target->is_reference;
    if (source->is_reference) source = source->base;
    if (reference_target) target = target->base;
    if (!source || !target) return -1;
    if (reference_target &&
        ((source->is_const && !target->is_const) ||
         (source->is_volatile && !target->is_volatile))) {
        return -1;
    }
    if (type_is_compatible(source, target)) return 0;
    if (reference_target &&
        (source->kind == TYPE_STRUCT || source->kind == TYPE_UNION) &&
        (target->kind == TYPE_STRUCT || target->kind == TYPE_UNION) &&
        sema_cxx_unique_public_base(source, target, NULL)) {
        return 2;
    }
    if (sema_is_scoped_enum(source) || sema_is_scoped_enum(target)) {
        return -1;
    }
    if (type_is_arithmetic(source) && type_is_arithmetic(target)) return 2;
    if ((source->kind == TYPE_PTR && source->cxx_is_member_pointer) ||
        (target->kind == TYPE_PTR && target->cxx_is_member_pointer)) {
        if (source->kind == TYPE_NULLPTR && target->kind == TYPE_PTR &&
            target->cxx_is_member_pointer) {
            return 2;
        }
        if (source->kind != TYPE_PTR || target->kind != TYPE_PTR ||
            !source->cxx_is_member_pointer ||
            !target->cxx_is_member_pointer || !source->base ||
            !target->base || source->base->kind == TYPE_FUNC ||
            target->base->kind == TYPE_FUNC ||
            !type_is_compatible(source->base, target->base) ||
            !sema_pointee_qualification_preserved(source->base,
                                                  target->base)) {
            return -1;
        }
        if (type_is_compatible(source->cxx_member_pointer_owner,
                               target->cxx_member_pointer_owner)) {
            return 1;
        }
        return source->cxx_member_pointer_owner &&
                       target->cxx_member_pointer_owner &&
                       sema_cxx_unique_public_nonvirtual_member_owner_path(
                           target->cxx_member_pointer_owner,
                           source->cxx_member_pointer_owner, NULL)
                   ? 2 : -1;
    }
    if (source->kind == TYPE_PTR && target->kind == TYPE_PTR) {
        source_base = source->base;
        target_base = target->base;
        if (!source_base || !target_base) return -1;
        if (!sema_pointee_qualification_preserved(
                source_base, target_base)) {
            return -1;
        }
        if (type_is_compatible(source_base, target_base)) return 1;
        if (source_base->kind == TYPE_VOID || target_base->kind == TYPE_VOID) {
            return 2;
        }
        return sema_cxx_pointer_conversion(source, target, NULL) ? 2 : -1;
    }
    return -1;
}

/* Find an ordinary public conversion function whose result can reach the
 * requested target through one standard conversion.  A user-defined
 * conversion cannot be chained with a second user-defined conversion.  The
 * caller supplies the ambiguity result because overload ranking and the final
 * cast need to make the same decision without silently picking one. */
static TypeMethod* sema_find_cxx_conversion_method(Type* aggregate,
                                                   Type* target,
                                                   bool* ambiguous) {
    TypeMethod* method;
    TypeMethod* result = NULL;
    int result_rank = INT_MAX;
    if (ambiguous) *ambiguous = false;
    if (!aggregate || !target ||
        (aggregate->kind != TYPE_STRUCT && aggregate->kind != TYPE_UNION)) {
        return NULL;
    }
    for (method = aggregate->methods; method; method = method->next) {
        Type* result_type;
        int rank;
        if (method->kind != TYPE_METHOD_FUNCTION ||
            !method->function_decl || !method->return_type ||
            !method->name || strcmp(method->name, "operator conversion") != 0 ||
            method->cxx_access != ACCESS_PUBLIC || method->is_explicit ||
            method->function_decl->func_params) {
            continue;
        }
        result_type = method->return_type;
        if (target->is_reference) {
            if (result_type->is_reference) {
                if (target->is_rvalue_reference &&
                    !result_type->is_rvalue_reference) {
                    continue;
                }
                if (!target->is_rvalue_reference &&
                    result_type->is_rvalue_reference &&
                    !target->base->is_const) {
                    continue;
                }
            } else if (!target->is_rvalue_reference &&
                       !target->base->is_const) {
                /* A non-const lvalue reference cannot bind a conversion
                 * function's prvalue result. */
                continue;
            }
        }
        rank = sema_cxx_conversion_result_rank(result_type, target);
        if (rank < 0) continue;
        if (!result || rank < result_rank) {
            result = method;
            result_rank = rank;
            if (ambiguous) *ambiguous = false;
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

/* C qualification conversion may add cv at the directly pointed-to object,
 * but adding cv below an unqualified pointer-to-pointer would allow a write
 * through the outer pointer to change the type of the object seen by the
 * inner pointer.  A const-qualified intermediate pointer protects that
 * deeper conversion.  Keep this structural check separate from
 * type_is_compatible(), which intentionally ignores top-level cv. */
static bool sema_pointee_qualification_preserved_internal(
    const Type* source, const Type* target, bool protected_level,
    bool nested_level) {
    bool source_const;
    bool target_const;
    bool source_volatile;
    bool target_volatile;
    if (!source || !target) return false;
    source_const = source->is_const;
    target_const = target->is_const;
    source_volatile = source->is_volatile;
    target_volatile = target->is_volatile;
    if ((source_const && !target_const) ||
        (source_volatile && !target_volatile) ||
        (nested_level && (target_const && !source_const) &&
         !protected_level) ||
        (nested_level && (target_volatile && !source_volatile) &&
         !protected_level)) {
        return false;
    }
    if (source->kind == TYPE_PTR || target->kind == TYPE_PTR) {
        if (source->kind != TYPE_PTR || target->kind != TYPE_PTR) {
            return true;
        }
        return sema_pointee_qualification_preserved_internal(
            source->base, target->base,
            target->is_const || target->is_volatile, true);
    }
    return true;
}

static bool sema_pointee_qualification_preserved(const Type* source,
                                                 const Type* target) {
    return sema_pointee_qualification_preserved_internal(
        source, target, false, false);
}

static bool sema_is_null_pointer_constant(const Expr* expression) {
    int64_t value;
    if (!expression || !expression->type ||
        !type_is_integer(expression->type)) {
        return false;
    }
    if (rcc_parser_is_cxx_mode()) {
        return expression->kind == EXPR_INT_LIT && expression->int_val == 0;
    }
    return expr_eval_integer_constant((Expr*)expression, &value) && value == 0;
}

static Type* implicit_cast(Expr* e, Type* target) {
    if (!e->type || !target) return NULL;

    /* nullptr has a zero machine representation, but it is not an integer.
     * Keep its standard null-pointer conversion separate from the legacy C
     * integer/pointer conversion paths below. */
    if (e->type->kind == TYPE_NULLPTR) {
        if (target->kind == TYPE_NULLPTR) return target;
        if (!target->is_reference && target->kind == TYPE_BOOL) return target;
        if (!target->is_reference && target->kind == TYPE_PTR &&
            target->cxx_is_member_pointer) {
            e->type = target;
            e->is_cxx_nullptr = true;
            return target;
        }
        return !target->is_reference && type_is_pointer(target)
            ? target : NULL;
    }

    if (target->is_reference) {
        Type* referred = target->base;
        Type* source;
        Type* source_object;
        bool conversion_attempted = false;
        /* Reference arguments are passed as addresses by the backend.  A
         * const lvalue reference may also bind a scalar rvalue; the backend
         * materializes that value for the duration of the call. */
        if (!referred) return NULL;
reference_binding_source:
        source = e->type;
        source_object = source && source->is_reference
            ? source->base : source;
        if (!source_object) return NULL;
        if (rcc_parser_is_cxx_mode() &&
            (source_object->kind == TYPE_STRUCT ||
             source_object->kind == TYPE_UNION) &&
            (referred->kind == TYPE_STRUCT || referred->kind == TYPE_UNION) &&
            (!source_object->is_const || referred->is_const) &&
            (!source_object->is_volatile || referred->is_volatile)) {
            int adjustment = 0;
            int virtual_index;
            int nested_adjustment;
            if (sema_cxx_virtual_object_conversion(
                    source_object, referred,
                    &virtual_index, &nested_adjustment)) {
                e->cxx_virtual_base_adjustment = true;
                e->cxx_virtual_base_index = virtual_index;
                e->cxx_virtual_base_nested_adjustment = nested_adjustment;
                e->cxx_virtual_base_source_class = source_object->cxx_class;
                e->cxx_virtual_base_pointer_offset = source_object->cxx_class
                    ? source_object->cxx_class->virtual_base_pointer_offset
                    : -1;
                goto reference_binding_validated;
            }
            if (sema_cxx_unique_public_base(source_object, referred,
                                            &adjustment)) {
                /* Reference binding keeps the source lvalue address but uses
                 * the same fixed public-base displacement as a pointer
                 * conversion.  Record it on the initializer expression so
                 * local/global reference storage receives the subobject
                 * address rather than the complete-object address. */
                e->cxx_pointer_adjustment_valid = adjustment != 0;
                e->cxx_pointer_adjustment = adjustment;
                goto reference_binding_validated;
            }
        }
        if (cxx_reference_object_compatible(source_object, referred)) {
            if ((source_object->is_const && !referred->is_const) ||
                (source_object->is_volatile && !referred->is_volatile)) {
                return NULL;
            }
            goto reference_binding_validated;
        }
        if (!conversion_attempted && rcc_parser_is_cxx_mode() &&
            (referred->kind == TYPE_STRUCT ||
             referred->kind == TYPE_UNION) &&
            (source_object->kind == TYPE_STRUCT ||
             source_object->kind == TYPE_UNION)) {
            bool ambiguous = false;
            TypeMethod* conversion = sema_find_cxx_conversion_method(
                source_object, target, &ambiguous);
            if (!ambiguous && conversion) {
                Expr* source_expression =
                    ast_arena_alloc(sizeof(*source_expression));
                Expr* member;
                Expr* call;
                *source_expression = *e;
                member = expr_member(source_expression, conversion->name,
                                     e->loc);
                call = expr_call(member, NULL, e->loc);
                *e = *call;
                sema_expr(e);
                conversion_attempted = true;
                goto reference_binding_source;
            }
        }
        return NULL;
reference_binding_validated:
        if ((!target->is_rvalue_reference && !is_lvalue(e) &&
             !referred->is_const) ||
            (target->is_rvalue_reference && is_lvalue(e))) {
            return NULL;
        }
        return target;
    }

    /* Same type */
    if (e->type == target) return target;

    /* Data-member pointers are not ordinary object pointers: keep them out
     * of void-pointer and integer conversions, but allow the standard
     * same-owner qualification conversion for their member type. */
    if ((e->type->kind == TYPE_PTR &&
         e->type->cxx_is_member_pointer) ||
        (target->kind == TYPE_PTR && target->cxx_is_member_pointer)) {
        if (target->kind == TYPE_PTR && target->cxx_is_member_pointer &&
            (e->type->kind == TYPE_NULLPTR ||
             sema_is_null_pointer_constant(e))) {
            e->type = target;
            e->is_cxx_nullptr = true;
            return target;
        }
        if (e->type->kind == TYPE_PTR && e->type->cxx_is_member_pointer &&
            target->kind == TYPE_PTR && target->cxx_is_member_pointer) {
            Type* source_owner = e->type->cxx_member_pointer_owner;
            Type* target_owner = target->cxx_member_pointer_owner;
            int owner_adjustment = 0;
            int paths = 0;
            if (e->is_cxx_nullptr) {
                e->type = target;
                return target;
            }
            if (e->type->base && target->base &&
                e->type->base->kind != TYPE_FUNC &&
                target->base->kind != TYPE_FUNC &&
                type_is_compatible(e->type->base, target->base) &&
                sema_pointee_qualification_preserved(
                    e->type->base, target->base) &&
                source_owner && target_owner &&
                !type_is_compatible(source_owner, target_owner)) {
                paths = sema_cxx_unique_public_nonvirtual_member_owner_path(
                    target_owner, source_owner, &owner_adjustment) ? 1 : 0;
            }
            if (paths == 1) {
                int64_t total_adjustment = owner_adjustment;
                if (e->cxx_member_pointer_adjustment_valid) {
                    total_adjustment +=
                        e->cxx_member_pointer_adjustment;
                }
                if (total_adjustment < INT32_MIN ||
                    total_adjustment > INT32_MAX) {
                    return NULL;
                }
                if (e->kind == EXPR_INT_LIT) {
                    int64_t converted;
                    if ((total_adjustment > 0 &&
                         e->int_val > INT64_MAX - total_adjustment) ||
                        (total_adjustment < 0 &&
                         e->int_val < INT64_MIN - total_adjustment)) {
                        return NULL;
                    }
                    converted = e->int_val + total_adjustment;
                    e->type = target;
                    e->int_val = converted;
                    e->cxx_member_pointer_adjustment_valid = false;
                    e->cxx_member_pointer_adjustment = 0;
                } else {
                    e->type = target;
                    e->cxx_member_pointer_adjustment_valid = true;
                    e->cxx_member_pointer_adjustment =
                        (int32_t)total_adjustment;
                }
                return target;
            }
        }
        if (e->type->kind == TYPE_PTR && target->kind == TYPE_PTR &&
            e->type->cxx_is_member_pointer &&
            target->cxx_is_member_pointer &&
            type_is_compatible(e->type, target) &&
            sema_pointee_qualification_preserved(e->type->base,
                                                 target->base)) {
            return target;
        }
        return NULL;
    }

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
    if (target->kind == TYPE_VECTOR &&
        e->type->kind == TYPE_VECTOR &&
        e->type->size == target->size) {
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
            return e->type && sema_cxx_conversion_result_rank(e->type, target) >= 0
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

    /* A pointer converts implicitly to C++ bool (and C _Bool), but converting
     * between pointers and other integer types requires an explicit cast.
     * The only implicit integer-to-pointer conversion is a null pointer
     * constant: an integer constant expression equal to zero in C, and an
     * integer literal equal to zero in C++. */
    if (type_is_pointer(e->type) && type_is_integer(target)) {
        return target->kind == TYPE_BOOL ? target : NULL;
    }
    if (type_is_integer(e->type) && type_is_pointer(target)) {
        return sema_is_null_pointer_constant(e) ? target : NULL;
    }

    /* Array to pointer decay */
    if (type_is_array(e->type) && type_is_pointer(target)) {
        if (!sema_pointee_qualification_preserved(
                e->type->base, target->base)) {
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
        bool source_points_to_function = type_is_function(e->type->base);
        bool target_points_to_function = type_is_function(target->base);
        if (source_points_to_function != target_points_to_function) {
            return NULL;
        }
        if (rcc_parser_is_cxx_mode() && e->type->base && target->base &&
            e->type->base->kind == TYPE_VOID &&
            target->base->kind != TYPE_VOID) {
            return NULL;
        }
        if (!sema_pointee_qualification_preserved(
                e->type->base, target->base)) {
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
            if (sema_cxx_set_pointer_conversion(e, e->type, target,
                                                &adjustment)) {
                if (!e->cxx_virtual_base_adjustment) {
                    e->cxx_pointer_adjustment_valid = adjustment != 0;
                    e->cxx_pointer_adjustment = adjustment;
                }
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
            if (sema_cleanup_plan_has_unregistered_call(
                    declaration->var_cleanups)) {
                return true;
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
        case EXPR_CXX_MEMBER_PTR_DOT:
        case EXPR_CXX_MEMBER_PTR_ARROW:
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
    bool is_constexpr_dynamic;
    bool is_constexpr_dynamic_array;
    bool is_constexpr_initialized;
} SemaConstexprBinding;

static int constexpr_eval_depth;
/* C++20 constant evaluation may use transient allocation, but the storage
 * must be reclaimed before the evaluation completes.  Keep only the live
 * count here; object bytes and provenance remain attached to the binding that
 * owns the allocation.  The count is saved/restored around nested constexpr
 * calls so a failed inner evaluation cannot leak state into its caller. */
static int constexpr_dynamic_live_count;

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
        case STMT_SWITCH: {
            StmtList* item;
            StmtList* selected = NULL;
            StmtList* fallback = NULL;
            int64_t selector;
            int saved_count = *binding_count;
            if (!statement->switch_expr ||
                !sema_eval_constexpr_expr(statement->switch_expr, bindings,
                                           *binding_count, &selector) ||
                !statement->switch_body ||
                statement->switch_body->kind != STMT_BLOCK) {
                return false;
            }
            for (item = statement->switch_body->block_stmts; item;
                 item = item->next) {
                Stmt* label = item->stmt;
                int64_t case_value;
                if (!label) return false;
                if (label->kind == STMT_DEFAULT) {
                    if (!fallback) fallback = item;
                    continue;
                }
                if (label->kind != STMT_CASE || !label->case_val ||
                    !sema_eval_constexpr_expr(label->case_val, bindings,
                                               *binding_count, &case_value)) {
                    continue;
                }
                if (!selected && case_value == selector) selected = item;
            }
            if (!selected) selected = fallback;
            if (!selected) {
                *binding_count = saved_count;
                return true;
            }
            for (item = selected; item; item = item->next) {
                Stmt* current = item->stmt;
                Stmt* body = current &&
                    (current->kind == STMT_CASE ||
                     current->kind == STMT_DEFAULT)
                    ? (current->kind == STMT_CASE
                        ? current->case_stmt : current->default_stmt)
                    : current;
                SemaConstexprStatementResult nested_result;
                if (!body) {
                    *binding_count = saved_count;
                    return false;
                }
                if (!sema_eval_constexpr_statement(
                        body, bindings, binding_count, value,
                        &nested_result)) {
                    *binding_count = saved_count;
                    return false;
                }
                if (nested_result == SEMA_CONSTEXPR_STMT_RETURNED) {
                    *result = nested_result;
                    *binding_count = saved_count;
                    return true;
                }
                if (nested_result == SEMA_CONSTEXPR_STMT_BREAK) {
                    *binding_count = saved_count;
                    return true;
                }
                if (nested_result == SEMA_CONSTEXPR_STMT_CONTINUE) {
                    *result = nested_result;
                    *binding_count = saved_count;
                    return true;
                }
            }
            *binding_count = saved_count;
            return true;
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

/* Aggregate constant evaluation uses target-layout bytes so the evaluator
 * never has to manufacture a host pointer.  Keep the corresponding pointer
 * provenance beside those bytes rather than encoding a process-local address
 * into the buffer.  The side table is compiler-lifetime storage, which is
 * appropriate because every evaluator buffer comes from the AST arena. */
typedef struct SemaConstexprPointerSlot {
    unsigned char* storage;
    Type* type;
    Decl* declaration;
    int64_t offset;
    struct SemaConstexprPointerSlot* next;
} SemaConstexprPointerSlot;

static SemaConstexprPointerSlot* constexpr_pointer_slots;

static bool sema_constexpr_address_in_range(
    const unsigned char* base, size_t size,
    const unsigned char* address, size_t address_size) {
    uintptr_t base_value;
    uintptr_t address_value;
    if (!base || !address || address_size > size) return false;
    base_value = (uintptr_t)base;
    address_value = (uintptr_t)address;
    return address_value >= base_value &&
           address_value - base_value <= size - address_size;
}

static SemaConstexprPointerSlot* sema_constexpr_pointer_slot_find(
    const unsigned char* storage) {
    for (SemaConstexprPointerSlot* slot = constexpr_pointer_slots;
         slot; slot = slot->next) {
        if (slot->storage == storage) return slot;
    }
    return NULL;
}

static void sema_constexpr_pointer_slots_clear(
    unsigned char* storage, size_t size) {
    SemaConstexprPointerSlot** link = &constexpr_pointer_slots;
    while (*link) {
        SemaConstexprPointerSlot* slot = *link;
        if (sema_constexpr_address_in_range(
                storage, size, slot->storage,
                slot->type && slot->type->size > 0
                    ? (size_t)slot->type->size : 0u)) {
            *link = slot->next;
        } else {
            link = &slot->next;
        }
    }
}

static void sema_constexpr_pointer_slot_record(
    unsigned char* storage, Type* type, const SemaConstexprScalar* value) {
    SemaConstexprPointerSlot* slot;
    if (!storage || !type || type->kind != TYPE_PTR || !value) return;
    sema_constexpr_pointer_slots_clear(storage, (size_t)type->size);
    slot = ast_arena_alloc(sizeof(*slot));
    slot->storage = storage;
    slot->type = type;
    slot->declaration = value->pointer_declaration;
    slot->offset = value->pointer_offset;
    slot->next = constexpr_pointer_slots;
    constexpr_pointer_slots = slot;
}

static void sema_constexpr_copy_object_bytes(
    unsigned char* destination, const unsigned char* source, size_t size) {
    SemaConstexprPointerSlot* snapshot = NULL;
    SemaConstexprPointerSlot* tail = NULL;
    uintptr_t source_value = (uintptr_t)source;

    if (!destination || !source || size == 0u) return;
    for (SemaConstexprPointerSlot* item = constexpr_pointer_slots;
         item; item = item->next) {
        if (sema_constexpr_address_in_range(
                source, size, item->storage,
                item->type && item->type->size > 0
                    ? (size_t)item->type->size : 0u)) {
            SemaConstexprPointerSlot* copy = ast_arena_alloc(sizeof(*copy));
            *copy = *item;
            copy->next = NULL;
            if (tail) tail->next = copy;
            else snapshot = copy;
            tail = copy;
        }
    }
    sema_constexpr_pointer_slots_clear(destination, size);
    memcpy(destination, source, size);
    for (SemaConstexprPointerSlot* item = snapshot;
         item; item = item->next) {
        SemaConstexprPointerSlot* copy = ast_arena_alloc(sizeof(*copy));
        uintptr_t item_value = (uintptr_t)item->storage;
        *copy = *item;
        copy->storage = destination + (item_value - source_value);
        copy->next = constexpr_pointer_slots;
        constexpr_pointer_slots = copy;
    }
}

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
    if (type->kind == TYPE_PTR) {
        /* Pointer bytes are intentionally kept zero.  The side table above
         * carries the only meaningful value and is copied with aggregates. */
        sema_constexpr_pointer_slot_record(storage, type, &converted);
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
    if (type->kind == TYPE_PTR) {
        SemaConstexprPointerSlot* slot =
            sema_constexpr_pointer_slot_find(storage);
        if (slot) {
            value->is_pointer = true;
            value->pointer_declaration = slot->declaration;
            value->pointer_offset = slot->offset;
            return true;
        }
        for (index = 0; index < (size_t)type->size; ++index) {
            if (storage[index] != 0u) return false;
        }
        value->is_pointer = true;
        value->pointer_declaration = NULL;
        value->pointer_offset = 0;
        return true;
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
    SemaConstexprScalar pointer_value;
    Type* base_type;
    size_t base_offset;
    int64_t index;
    int64_t pointer_offset;
    size_t element_size = 0;

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
    if (expression->kind == EXPR_PTR_MEMBER) {
        int pointer_index;
        if (!expression->member_base || !expression->member_field ||
            !sema_eval_constexpr_scalar_expr(
                expression->member_base, bindings, binding_count,
                &pointer_value) || !pointer_value.is_pointer ||
            !pointer_value.pointer_declaration ||
            pointer_value.pointer_offset < 0 ||
            expression->member_field->offset < 0) {
            return false;
        }
        pointer_index = -1;
        for (int candidate = 0; candidate < 64; ++candidate) {
            if (bindings[candidate].declaration ==
                    pointer_value.pointer_declaration &&
                bindings[candidate].is_object &&
                bindings[candidate].object_bytes &&
                bindings[candidate].type) {
                pointer_index = candidate;
                break;
            }
        }
        if (pointer_index < 0 ||
            (uint64_t)pointer_value.pointer_offset > SIZE_MAX ||
            (size_t)pointer_value.pointer_offset >
                bindings[pointer_index].object_size ||
            (size_t)expression->member_field->offset >
                bindings[pointer_index].object_size -
                (size_t)pointer_value.pointer_offset) {
            return false;
        }
        *binding_index = pointer_index;
        *offset = (size_t)pointer_value.pointer_offset +
                  (size_t)expression->member_field->offset;
        *type = expression->member_field->type;
        return *type != NULL &&
               (size_t)(*type)->size <=
                   bindings[pointer_index].object_size - *offset;
    }
    if (expression->kind == EXPR_DEREF) {
        int pointer_index;
        Type* target_type = expression->type;
        if (!expression->unary_operand ||
            !sema_eval_constexpr_scalar_expr(
                expression->unary_operand, bindings, binding_count,
                &pointer_value) || !pointer_value.is_pointer ||
            !pointer_value.pointer_declaration ||
            pointer_value.pointer_offset < 0 || !target_type ||
            target_type->size <= 0) {
            return false;
        }
        pointer_index = -1;
        for (int candidate = 0; candidate < 64; ++candidate) {
            if (bindings[candidate].declaration ==
                    pointer_value.pointer_declaration &&
                bindings[candidate].is_object &&
                bindings[candidate].object_bytes &&
                bindings[candidate].type) {
                pointer_index = candidate;
                break;
            }
        }
        if (pointer_index < 0 ||
            (uint64_t)pointer_value.pointer_offset > SIZE_MAX ||
            (size_t)pointer_value.pointer_offset >
                bindings[pointer_index].object_size ||
            (size_t)target_type->size >
                bindings[pointer_index].object_size -
                (size_t)pointer_value.pointer_offset) {
            return false;
        }
        *binding_index = pointer_index;
        *offset = (size_t)pointer_value.pointer_offset;
        *type = target_type;
        return true;
    }
    if (expression->kind != EXPR_INDEX || !expression->index_base) {
        return false;
    }
    if (expression->index_base->type &&
        expression->index_base->type->kind == TYPE_PTR) {
        int pointer_index;
        Type* element_type = expression->index_base->type->base;
        if (!element_type || element_type->size <= 0 ||
            !sema_eval_constexpr_scalar_expr(
                expression->index_base, bindings, binding_count,
                &pointer_value) || !pointer_value.is_pointer ||
            !pointer_value.pointer_declaration ||
            pointer_value.pointer_offset < 0 ||
            !sema_eval_constexpr_scalar_expr(
                expression->index_expr, bindings, binding_count,
                &index_value) || index_value.is_floating) {
            return false;
        }
        index = index_value.integer_value;
        if (index < 0 || !sema_constexpr_pointer_offset(
                pointer_value.pointer_offset, index,
                element_type->size, &pointer_offset) ||
            pointer_offset < 0 || (uint64_t)pointer_offset > SIZE_MAX) {
            return false;
        }
        base_offset = (size_t)pointer_offset;
        pointer_index = -1;
        for (int candidate = 0; candidate < 64; ++candidate) {
            if (bindings[candidate].declaration ==
                    pointer_value.pointer_declaration &&
                bindings[candidate].is_object &&
                bindings[candidate].object_bytes &&
                bindings[candidate].type) {
                pointer_index = candidate;
                break;
            }
        }
        if (pointer_index < 0 || base_offset >
                bindings[pointer_index].object_size ||
            (size_t)element_type->size >
                bindings[pointer_index].object_size - base_offset) {
            return false;
        }
        *binding_index = pointer_index;
        *offset = base_offset;
        *type = element_type;
        return true;
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
        (bindings[binding_index].is_constexpr_dynamic &&
         !bindings[binding_index].is_constexpr_initialized) ||
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
    if (bindings[binding_index].is_constexpr_dynamic) {
        bindings[binding_index].is_constexpr_initialized = true;
    }
    if (stored) *stored = converted;
    return true;
}

static int sema_constexpr_dynamic_binding_index(
    Decl* declaration, SemaConstexprBinding* bindings, int binding_count) {
    (void)binding_count;
    if (!declaration || !bindings) return -1;
    for (int index = 0; index < 64; ++index) {
        if (bindings[index].is_constexpr_dynamic &&
            bindings[index].declaration == declaration) {
            return index;
        }
    }
    return -1;
}

static bool sema_eval_constexpr_dynamic_call(
    Expr* expression, SemaConstexprBinding* bindings, int* binding_count,
    SemaConstexprScalar* value) {
    Type* object_type;
    Type* storage_type;
    ExprList* initializer;
    SemaConstexprScalar initializer_value;
    Decl* allocation;
    unsigned char* object_bytes;
    int dynamic_index;
    int binding_index;
    int64_t element_count = 1;
    size_t element_size = 0;
    int64_t initializer_index;
    bool aggregate_object;

    if (!expression || !bindings || !binding_count || !value ||
        constexpr_eval_depth <= 0) return false;
    if (expression->call_is_new) {
        object_type = expression->call_new_type;
        initializer = expression->call_new_args;
        aggregate_object = object_type &&
            (object_type->kind == TYPE_STRUCT ||
             object_type->kind == TYPE_UNION) &&
            !object_type->cxx_nontrivial;
        /* This evaluator owns no target heap.  It supports scalar objects,
         * fixed-size scalar arrays, and complete trivial aggregates whose
         * bytes can be materialized by the existing constexpr object model.
         * Non-trivial class lifetime remains a separate semantic path. */
        if (!object_type ||
            (!sema_constexpr_scalar_type(object_type) && !aggregate_object) ||
            object_type->kind == TYPE_PTR || object_type->size <= 0 ||
            (!expression->call_new_is_array && !aggregate_object && initializer &&
             initializer->next)) {
            return false;
        }
        storage_type = object_type;
        if (expression->call_new_is_array) {
            if (!expression->call_new_count ||
                !sema_eval_constexpr_scalar_expr(
                    expression->call_new_count, bindings, *binding_count,
                    &initializer_value) || initializer_value.is_floating ||
                initializer_value.integer_value <= 0 ||
                initializer_value.integer_value > INT_MAX ||
                (size_t)object_type->size > SIZE_MAX /
                    (size_t)initializer_value.integer_value) {
                return false;
            }
            element_count = initializer_value.integer_value;
            element_size = (size_t)object_type->size;
            storage_type = type_array(object_type, (int)element_count);
            if (!storage_type || storage_type->size <= 0) return false;
            if (!initializer && !expression->call_new_value_init &&
                !expression->call_new_brace_init) return false;
        }
        binding_index = -1;
        /* Keep transient allocations above the ordinary lexical binding
         * range.  A declaration such as `int* p = new int(1)` is evaluated
         * before the binding for `p` is installed, so using the next lexical
         * slot here would overwrite the allocation record immediately. */
        for (int index = 63; index >= *binding_count; --index) {
            if (!bindings[index].declaration ||
                (bindings[index].is_constexpr_dynamic &&
                 !bindings[index].is_object)) {
                binding_index = index;
                break;
            }
        }
        if (binding_index < 0) return false;
        object_bytes = ast_arena_alloc((size_t)storage_type->size);
        memset(object_bytes, 0, (size_t)storage_type->size);
        if (expression->call_new_is_array) {
            initializer_index = 0;
            for (ExprList* item = initializer; item; item = item->next) {
                if (initializer_index >= element_count ||
                    (aggregate_object
                        ? !sema_constexpr_materialize_object(
                              object_type, item->expr, bindings,
                              *binding_count,
                              object_bytes + (size_t)initializer_index *
                                  element_size,
                              element_size)
                        : (!sema_eval_constexpr_scalar_expr(
                               item->expr, bindings, *binding_count,
                               &initializer_value) ||
                           !sema_constexpr_store_scalar_bytes(
                               object_bytes + (size_t)initializer_index *
                                   element_size,
                               element_size, object_type,
                               &initializer_value)))) {
                    return false;
                }
                ++initializer_index;
            }
        } else if (aggregate_object) {
            if (initializer) {
                Expr* aggregate_initializer = expr_initializer_list(
                    initializer, expression->loc);
                aggregate_initializer->compound_type = object_type;
                aggregate_initializer->type = object_type;
                if (!sema_constexpr_materialize_object(
                        object_type, aggregate_initializer, bindings,
                        *binding_count, object_bytes,
                        (size_t)storage_type->size)) {
                    return false;
                }
            } else if (!expression->call_new_value_init &&
                       !expression->call_new_brace_init) {
                /* Default-initialized aggregate storage is not readable until
                 * each read is preceded by a constexpr assignment. */
                memset(object_bytes, 0, (size_t)storage_type->size);
            }
        } else if (initializer) {
            if (!sema_eval_constexpr_scalar_expr(
                    initializer->expr, bindings, *binding_count,
                    &initializer_value) ||
                !sema_constexpr_store_scalar_bytes(
                    object_bytes, (size_t)storage_type->size, object_type,
                    &initializer_value)) {
                return false;
            }
        } else if (!expression->call_new_value_init &&
                   !expression->call_new_brace_init) {
            /* Default-initialized scalar storage is not readable until an
             * evaluated assignment has written it. */
            memset(&initializer_value, 0, sizeof(initializer_value));
            initializer_value.type = object_type;
        }
        allocation = ast_arena_alloc(sizeof(*allocation));
        memset(allocation, 0, sizeof(*allocation));
        allocation->kind = DECL_VAR;
        allocation->name = rcc_intern("__rcc_constexpr_heap");
        allocation->type = storage_type;
        memset(&bindings[binding_index], 0,
               sizeof(bindings[binding_index]));
        bindings[binding_index].declaration = allocation;
        bindings[binding_index].type = storage_type;
        bindings[binding_index].object_bytes = object_bytes;
        bindings[binding_index].object_size = (size_t)storage_type->size;
        bindings[binding_index].is_object = true;
        bindings[binding_index].is_constexpr_dynamic = true;
        bindings[binding_index].is_constexpr_dynamic_array =
            expression->call_new_is_array;
        bindings[binding_index].is_constexpr_initialized =
            initializer != NULL || expression->call_new_value_init ||
            expression->call_new_brace_init;
        ++constexpr_dynamic_live_count;
        memset(value, 0, sizeof(*value));
        value->type = expression->type ? expression->type : type_ptr(object_type);
        value->is_pointer = true;
        value->pointer_declaration = allocation;
        value->pointer_offset = 0;
        return true;
    }

    if (!expression->call_is_delete || !expression->call_args ||
        expression->call_args->next || !expression->call_args->expr ||
        !sema_eval_constexpr_scalar_expr(
            expression->call_args->expr, bindings, *binding_count,
            &initializer_value) || !initializer_value.is_pointer) {
        return false;
    }
    if (!initializer_value.pointer_declaration &&
        initializer_value.pointer_offset == 0) {
        memset(value, 0, sizeof(*value));
        value->type = type_void;
        return true;
    }
    if (initializer_value.pointer_offset != 0 ||
        (dynamic_index = sema_constexpr_dynamic_binding_index(
            initializer_value.pointer_declaration, bindings, *binding_count)) < 0 ||
        !bindings[dynamic_index].is_object ||
        bindings[dynamic_index].is_constexpr_dynamic_array !=
            expression->call_delete_is_array) {
        return false;
    }
    bindings[dynamic_index].is_object = false;
    bindings[dynamic_index].is_constexpr_initialized = false;
    if (constexpr_dynamic_live_count <= 0) return false;
    --constexpr_dynamic_live_count;
    memset(value, 0, sizeof(*value));
    value->type = type_void;
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
    sema_constexpr_copy_object_bytes(
        bindings[binding_index].object_bytes + offset, temporary,
        (size_t)type->size);
    return true;
}

static bool sema_constexpr_materialize_object(
    Type* type, Expr* initializer, SemaConstexprBinding* bindings,
    int binding_count, unsigned char* storage, size_t storage_size) {
    ExprList* item;

    if (!type || !storage || type->size <= 0 ||
        (size_t)type->size > storage_size) return false;
    sema_constexpr_pointer_slots_clear(storage, (size_t)type->size);
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
            sema_constexpr_copy_object_bytes(
                storage, bindings[binding_index].object_bytes + source_offset,
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
            sema_constexpr_copy_object_bytes(
                storage, bindings[binding_index].object_bytes,
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

static Expr* sema_constexpr_rebuild_pointer_lvalue(
    Type* type, Expr* object, Type* target_type, int64_t offset,
    SourceLoc loc) {
    if (!type || !object || !target_type || offset < 0 || type->size <= 0) {
        return NULL;
    }
    if (offset == 0 && type_is_compatible(type, target_type)) {
        object->type = type;
        return object;
    }
    if (type->kind == TYPE_ARRAY && type->base && type->base->size > 0) {
        int64_t element_size = type->base->size;
        int64_t index;
        Expr* index_expression;
        Expr* element;
        if (offset > (int64_t)type->size ||
            offset % element_size != 0) return NULL;
        index = offset / element_size;
        if (index < 0 || index > type->array_len) return NULL;
        index_expression = expr_int(index, loc);
        index_expression->type = type_int;
        element = expr_index(object, index_expression, loc);
        element->type = type->base;
        return sema_constexpr_rebuild_pointer_lvalue(
            type->base, element, target_type, 0, loc);
    }
    if (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) {
        for (TypeField* field = type->fields; field; field = field->next) {
            Expr* member;
            int64_t field_end;
            if (!field->name || field->offset < 0 || !field->type ||
                field->type->size <= 0 ||
                offset < (int64_t)field->offset) continue;
            field_end = (int64_t)field->offset + field->type->size;
            if (offset >= field_end) continue;
            member = expr_member(object, field->name, loc);
            member->member_field = field;
            member->type = field->type;
            return sema_constexpr_rebuild_pointer_lvalue(
                field->type, member, target_type,
                offset - (int64_t)field->offset, loc);
        }
    }
    return NULL;
}

static Expr* sema_constexpr_rebuild_pointer(
    Type* type, const SemaConstexprScalar* value, SourceLoc loc) {
    Expr* object;
    Expr* lvalue;
    Expr* address;
    Type* target_type;
    if (!type || type->kind != TYPE_PTR || !value) return NULL;
    target_type = type->base;
    if (!value->pointer_declaration) {
        object = expr_int(0, loc);
        object->type = type;
        return object;
    }
    if (!target_type || !value->pointer_declaration->name ||
        (value->pointer_declaration->kind != DECL_FUNC &&
         value->pointer_declaration->kind != DECL_VAR) ||
        (value->pointer_declaration->kind == DECL_VAR &&
         !value->pointer_declaration->var_is_global &&
         !value->pointer_declaration->var_is_static_local) ||
        value->pointer_offset < 0) return NULL;
    object = expr_ident(value->pointer_declaration->name, loc);
    object->ident_decl = value->pointer_declaration;
    object->type = value->pointer_declaration->type;
    if (value->pointer_declaration->kind == DECL_FUNC) {
        if (value->pointer_offset != 0) return NULL;
        lvalue = object;
    } else {
        lvalue = sema_constexpr_rebuild_pointer_lvalue(
            value->pointer_declaration->type, object, target_type,
            value->pointer_offset, loc);
        if (!lvalue) return NULL;
    }
    address = expr_unary(EXPR_ADDR, lvalue, loc);
    address->type = type;
    return address;
}

/* Convert a fully evaluated aggregate back into the normal initializer AST so
 * static storage and ordinary aggregate codegen share one representation.
 * Pointer-bearing results retain symbolic provenance and are rebuilt as
 * relocatable address expressions; no host address is ever encoded. */
static Expr* sema_constexpr_rebuild_object(
    Type* type, const unsigned char* storage, size_t storage_size,
    SourceLoc loc) {
    Expr* expression;
    if (!type || !storage || type->size <= 0 ||
        (size_t)type->size > storage_size) return NULL;
    if (sema_constexpr_scalar_type(type)) {
        SemaConstexprScalar value;
        if (!sema_constexpr_load_scalar_bytes(
                storage, storage_size, type, &value)) {
            return NULL;
        }
        if (value.is_pointer) return sema_constexpr_rebuild_pointer(
            type, &value, loc);
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
            memset(&bindings[*binding_count], 0,
                   sizeof(bindings[*binding_count]));
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
    int saved_dynamic_live_count;
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
    saved_dynamic_live_count = constexpr_dynamic_live_count;
    ++constexpr_eval_depth;
    if (!sema_eval_constexpr_scalar_statement(
            declaration->func_body, bindings, &count, &result,
            &statement_result)) {
        --constexpr_eval_depth;
        constexpr_dynamic_live_count = saved_dynamic_live_count;
        return false;
    }
    --constexpr_eval_depth;
    if (statement_result != SEMA_CONSTEXPR_STMT_RETURNED ||
        constexpr_dynamic_live_count != saved_dynamic_live_count ||
        (result.is_pointer && result.pointer_declaration &&
         result.pointer_declaration->name &&
         strcmp(result.pointer_declaration->name,
                "__rcc_constexpr_heap") == 0)) {
        constexpr_dynamic_live_count = saved_dynamic_live_count;
        return false;
    }
    return sema_constexpr_scalar_convert(
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
        case EXPR_PTR_MEMBER: {
            Decl* declaration;
            SemaConstexprScalar pointer_value;
            unsigned char* storage;
            size_t offset;
            Type* target_type = expression->type;
            if (bindings && sema_constexpr_load_binding_scalar(
                    expression, bindings, binding_count, value)) {
                return true;
            }
            if (!expression->member_base || !expression->member_field ||
                !sema_eval_constexpr_scalar_expr(
                    expression->member_base, bindings, binding_count,
                    &pointer_value) ||
                !pointer_value.is_pointer ||
                !pointer_value.pointer_declaration ||
                pointer_value.pointer_declaration->kind != DECL_VAR ||
                !pointer_value.pointer_declaration->var_init ||
                (!pointer_value.pointer_declaration->var_is_global &&
                 !pointer_value.pointer_declaration->var_is_static_local) ||
                pointer_value.pointer_offset < 0 ||
                expression->member_field->offset < 0 ||
                !target_type || !sema_constexpr_scalar_type(target_type)) {
                return false;
            }
            declaration = pointer_value.pointer_declaration;
            if (!declaration->type || declaration->type->size <= 0 ||
                (uint64_t)pointer_value.pointer_offset > SIZE_MAX ||
                (size_t)pointer_value.pointer_offset >
                    (size_t)declaration->type->size ||
                (size_t)expression->member_field->offset >
                    (size_t)declaration->type->size -
                    (size_t)pointer_value.pointer_offset ||
                (size_t)target_type->size >
                    (size_t)declaration->type->size -
                    (size_t)pointer_value.pointer_offset -
                    (size_t)expression->member_field->offset) {
                return false;
            }
            offset = (size_t)pointer_value.pointer_offset +
                     (size_t)expression->member_field->offset;
            storage = ast_arena_alloc((size_t)declaration->type->size);
            if (!sema_constexpr_materialize_object(
                    declaration->type, declaration->var_init, NULL, 0,
                    storage, (size_t)declaration->type->size)) {
                return false;
            }
            return sema_constexpr_load_scalar_bytes(
                storage + offset,
                (size_t)declaration->type->size - offset,
                target_type, value);
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
            if (base_type && base_type->kind == TYPE_PTR) {
                Decl* declaration;
                SemaConstexprScalar pointer_value;
                unsigned char* storage;
                size_t offset;
                int64_t pointer_offset;
                Type* element_type = base_type->base;
                if (!element_type || element_type->size <= 0 ||
                    !sema_eval_constexpr_scalar_expr(
                        expression->index_base, bindings, binding_count,
                        &pointer_value) ||
                    !pointer_value.is_pointer ||
                    !pointer_value.pointer_declaration ||
                    pointer_value.pointer_declaration->kind != DECL_VAR ||
                    !pointer_value.pointer_declaration->var_init ||
                    (!pointer_value.pointer_declaration->var_is_global &&
                     !pointer_value.pointer_declaration->var_is_static_local) ||
                    pointer_value.pointer_offset < 0 ||
                    !sema_eval_constexpr_scalar_expr(
                        expression->index_expr, bindings, binding_count,
                        &index_value) ||
                    index_value.is_floating ||
                    !sema_constexpr_pointer_offset(
                        pointer_value.pointer_offset, index_value.integer_value,
                        element_type->size, &pointer_offset) ||
                    pointer_offset < 0) {
                    return false;
                }
                declaration = pointer_value.pointer_declaration;
                offset = (size_t)pointer_offset;
                if (!declaration->type || declaration->type->size <= 0 ||
                    (uint64_t)offset > SIZE_MAX ||
                    offset > (size_t)declaration->type->size ||
                    (size_t)element_type->size >
                        (size_t)declaration->type->size - offset) {
                    return false;
                }
                storage = ast_arena_alloc((size_t)declaration->type->size);
                if (!sema_constexpr_materialize_object(
                        declaration->type, declaration->var_init, NULL, 0,
                        storage, (size_t)declaration->type->size)) {
                    return false;
                }
                return sema_constexpr_load_scalar_bytes(
                    storage + offset, (size_t)declaration->type->size - offset,
                    element_type, value);
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
                    !sema_eval_constexpr_scalar_expr(
                        expression->binary_rhs, bindings, binding_count,
                        &right)) return false;
                if (expression->kind == EXPR_ASSIGN) {
                    if (!sema_constexpr_store_binding_scalar(
                            expression->binary_lhs, bindings, binding_count,
                            &right, &assigned)) return false;
                } else if (!sema_constexpr_load_binding_scalar(
                               expression->binary_lhs, bindings, binding_count,
                               &current) ||
                           !sema_constexpr_scalar_assign(
                               expression->kind, &current, &right, object_type,
                               &assigned) ||
                           !sema_constexpr_store_binding_scalar(
                               expression->binary_lhs, bindings, binding_count,
                               &assigned, &assigned)) {
                    return false;
                }
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
            int object_binding_index;
            size_t object_offset;
            Type* object_type;
            if (bindings && sema_constexpr_binding_lvalue(
                    expression->unary_operand, bindings, binding_count,
                    &object_binding_index, &object_offset, &object_type)) {
                if (!object_type || (uint64_t)object_offset > INT64_MAX) {
                    return false;
                }
                value->type = expression->type;
                value->is_pointer = true;
                value->pointer_declaration =
                    bindings[object_binding_index].declaration;
                value->pointer_offset = (int64_t)object_offset;
                return true;
            }
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
        case EXPR_DEREF: {
            Decl* declaration;
            unsigned char* storage;
            size_t offset;
            Type* target_type = expression->type;
            if (bindings && sema_constexpr_load_binding_scalar(
                    expression, bindings, binding_count, value)) {
                return true;
            }
            if (!sema_eval_constexpr_scalar_expr(
                    expression->unary_operand, bindings, binding_count,
                    &left) || !left.is_pointer ||
                !left.pointer_declaration ||
                left.pointer_declaration->kind != DECL_VAR ||
                !left.pointer_declaration->type || !target_type ||
                !sema_constexpr_scalar_type(target_type) ||
                left.pointer_offset < 0) {
                return false;
            }
            declaration = left.pointer_declaration;
            if (declaration->type->size <= 0 || !declaration->var_init) {
                return false;
            }
            offset = (size_t)left.pointer_offset;
            if (offset > (size_t)declaration->type->size ||
                (size_t)target_type->size >
                    (size_t)declaration->type->size - offset) {
                return false;
            }
            storage = ast_arena_alloc((size_t)declaration->type->size);
            if (!sema_constexpr_materialize_object(
                    declaration->type, declaration->var_init, NULL, 0,
                    storage, (size_t)declaration->type->size)) {
                return false;
            }
            return sema_constexpr_load_scalar_bytes(
                storage + offset, (size_t)target_type->size,
                target_type, value);
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
        case EXPR_NOEXCEPT:
            if (!expression->cxx_noexcept_value_valid) return false;
            value->type = type_bool;
            value->integer_value = expression->cxx_noexcept_value ? 1 : 0;
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
            if (expression->call_is_new || expression->call_is_delete) {
                return sema_eval_constexpr_dynamic_call(
                    expression, bindings, &binding_count, value);
            }
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
    function = sema_cxx_lookup_name(name, (SourceLoc){"<sema>", 0, 0});
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
        if (source->cxx_is_member_pointer !=
            target->cxx_is_member_pointer) return false;
        if (source->cxx_is_member_pointer &&
            !type_is_compatible(source->cxx_member_pointer_owner,
                                target->cxx_member_pointer_owner)) {
            return false;
        }
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
        if (!target->is_reference && target->kind == TYPE_BOOL) return 1;
        if (!target->is_reference && target->kind == TYPE_PTR &&
            target->cxx_is_member_pointer) return 1;
        return !target->is_reference && type_is_pointer(target) ? 1 : -1;
    }
    if (!target->is_reference && target->kind == TYPE_PTR &&
        target->cxx_is_member_pointer &&
        sema_is_null_pointer_constant(argument)) {
        return 2;
    }
    if (target->is_reference) {
        target_base = target->base;
        if (source && source->kind == TYPE_PTR && source->is_reference) {
            source = source->base;
        }
        if (!target_base ||
            !source ||
            (!target->is_rvalue_reference && !is_lvalue(argument) &&
             !target_base->is_const) ||
            (target->is_rvalue_reference && is_lvalue(argument))) {
            return -1;
        }
        if ((source->is_const && !target_base->is_const) ||
            (source->is_volatile && !target_base->is_volatile)) {
            return -1;
        }
        if (cxx_same_parameter_type(source, target_base, false)) return 0;
        if (rcc_parser_is_cxx_mode() &&
            (source->kind == TYPE_STRUCT || source->kind == TYPE_UNION) &&
            (target_base->kind == TYPE_STRUCT ||
             target_base->kind == TYPE_UNION) &&
            sema_cxx_unique_public_base(source, target_base, NULL)) {
            /* Binding a reference to a public base subobject is a standard
             * conversion.  implicit_cast() records the actual subobject
             * displacement; keep the conversion rank here so overload
             * resolution can distinguish Derived& from Base&. */
            return 2;
        }
        return type_is_compatible(source, target_base) ? 1 : -1;
    }
    if (sema_is_scoped_enum(source) || sema_is_scoped_enum(target)) {
        return type_is_compatible(source, target) ? 0 : -1;
    }
    if (cxx_same_parameter_type(source, target, true)) return 0;

    if (rcc_parser_is_cxx_mode() &&
        (source->kind == TYPE_STRUCT || source->kind == TYPE_UNION)) {
        bool ambiguous = false;
        TypeMethod* conversion = sema_find_cxx_conversion_method(
            source, target, &ambiguous);
        if (conversion) {
            return 3 + sema_cxx_conversion_result_rank(
                conversion->return_type, target);
        }
        if (ambiguous) return -1;
    }

    if (source->kind == TYPE_ARRAY && target->kind == TYPE_PTR) {
        source_base = source->base;
        target_base = target->base;
        if (source_base && target_base &&
            !sema_pointee_qualification_preserved(
                source_base, target_base)) {
            return -1;
        }
        if ((target_base && target_base->kind == TYPE_VOID) ||
            type_is_compatible(source_base, target_base)) {
            return cxx_same_parameter_type(source_base, target_base, false)
                ? 1 : 2;
        }
        return -1;
    }

    if ((source->kind == TYPE_PTR && source->cxx_is_member_pointer) ||
        (target->kind == TYPE_PTR && target->cxx_is_member_pointer)) {
        if (source->kind != TYPE_PTR || target->kind != TYPE_PTR ||
            !source->cxx_is_member_pointer ||
            !target->cxx_is_member_pointer || !source->base ||
            !target->base || source->base->kind == TYPE_FUNC ||
            target->base->kind == TYPE_FUNC ||
            !type_is_compatible(source->base, target->base) ||
            !sema_pointee_qualification_preserved(source->base,
                                                  target->base)) {
            return -1;
        }
        if (type_is_compatible(source->cxx_member_pointer_owner,
                               target->cxx_member_pointer_owner)) {
            return 1;
        }
        if (source->cxx_member_pointer_owner &&
            target->cxx_member_pointer_owner &&
            sema_cxx_unique_public_nonvirtual_member_owner_path(
                target->cxx_member_pointer_owner,
                source->cxx_member_pointer_owner, NULL)) {
            return 2;
        }
        return -1;
    }
    if (source->kind == TYPE_PTR && target->kind == TYPE_PTR) {
        source_base = source->base;
        target_base = target->base;
        if (!source_base || !target_base) return -1;
        /* Standard qualification conversion may add, but never remove,
         * pointee cv-qualification. */
        if (!sema_pointee_qualification_preserved(
                source_base, target_base)) {
            return -1;
        }
        if (type_is_compatible(source_base, target_base)) return 1;
        if (source_base->kind == TYPE_VOID || target_base->kind == TYPE_VOID) {
            return 2;
        }
        if (sema_cxx_pointer_conversion(source, target, NULL)) return 2;
        return -1;
    }
    if (source->kind == TYPE_PTR && target->kind == TYPE_BOOL) return 2;

    if (source->kind == TYPE_FLOAT && target->kind == TYPE_DOUBLE) {
        return 1;
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

/* An overloaded function used as a value has no call arguments from which to
 * select a candidate.  Its target function-pointer type is the contextual
 * information required by C++ overload resolution.  Resolve only exact
 * function-type matches here; function-pointer conversions that need a real
 * expression conversion remain the responsibility of implicit_cast(). */
static bool sema_cxx_select_function_pointer_overload(
    Type* target, Expr* expression) {
    Symbol* symbol;
    Decl* candidate;
    Decl* selected = NULL;
    Type* function_type;
    if (!rcc_parser_is_cxx_mode() || !target || !expression ||
        expression->kind != EXPR_IDENT || target->kind != TYPE_PTR ||
        !target->base || target->base->kind != TYPE_FUNC) {
        return false;
    }
    symbol = sema_cxx_lookup_name(expression->ident_name, expression->loc);
    if (!symbol || symbol->kind != SYM_FUNC || !symbol->decl ||
        !symbol->decl->func_overload_next) {
        return false;
    }
    function_type = target->base;
    for (candidate = symbol->decl; candidate;
         candidate = candidate->func_overload_next) {
        if (candidate->kind != DECL_FUNC || !candidate->type ||
            candidate->type->kind != TYPE_FUNC ||
            candidate->func_this_param ||
            !type_is_compatible(candidate->type, function_type)) {
            continue;
        }
        if (selected) {
            rcc_error(expression->loc,
                      "ambiguous overload '%s' for function-pointer target",
                      expression->ident_name);
            expression->type = type_int;
            return true;
        }
        selected = candidate;
    }
    if (!selected) {
        rcc_error(expression->loc,
                  "no matching overload '%s' for function-pointer target",
                  expression->ident_name);
        expression->type = type_int;
        return true;
    }
    expression->ident_decl = selected;
    expression->type = selected->type;
    return true;
}

/* Overload conversion sequences are ordered per argument.  A scalar sum or
 * worst-rank tie breaker is not sufficient: candidates with ranks [0, 2] and
 * [1, 1], for example, are incomparable and must remain ambiguous. */
static int cxx_conversion_vector_relation(const int* left, const int* right,
                                          int count) {
    bool left_better = false;
    bool right_better = false;
    if (!left || !right || count < 0) return 0;
    for (int index = 0; index < count; ++index) {
        if (left[index] < right[index]) left_better = true;
        if (left[index] > right[index]) right_better = true;
    }
    if (left_better && !right_better) return 1;
    if (right_better && !left_better) return -1;
    return 0;
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

/* A bytewise copy is valid for a trivially copyable aggregate.  An ordinary
 * user constructor does not make the implicit copy constructor non-trivial;
 * only an explicitly declared copy/move constructor, a virtual layout, or a
 * user-defined destructor changes the copy semantics represented here. */
static bool sema_cxx_has_user_copy_constructor(Type* type) {
    CxxClass* cls = type ? type->cxx_class : NULL;
    if (!cls) return false;
    for (CxxConstructorInfo* constructor = cls->constructors;
         constructor; constructor = constructor->next) {
        TypeParam* parameter = constructor->parameters;
        Type* parameter_type = parameter ? parameter->type : NULL;
        if (constructor->parameter_count == 1 && parameter &&
            !parameter->next && parameter_type &&
            parameter_type->kind == TYPE_PTR && parameter_type->is_reference &&
            parameter_type->base && type_is_compatible(parameter_type->base,
                                                        type) &&
            !constructor->is_defaulted) {
            return true;
        }
    }
    return false;
}

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
    if (cls && (sema_cxx_has_user_copy_constructor(type) ||
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

static bool sema_cxx_default_member_constant(Type* field_type,
                                             Expr* initializer) {
    SemaConstexprScalar value;
    unsigned char* object_bytes;
    if (field_type && initializer &&
        (field_type->kind == TYPE_STRUCT ||
         field_type->kind == TYPE_UNION ||
         field_type->kind == TYPE_ARRAY) &&
        !field_type->cxx_nontrivial && type_is_complete(field_type) &&
        field_type->size > 0) {
        if (!sema_expr(initializer) ||
            !initializer->type ||
            !type_is_compatible(field_type, initializer->type)) {
            return false;
        }
        object_bytes = ast_arena_alloc((size_t)field_type->size);
        return sema_constexpr_materialize_object(
            field_type, initializer, NULL, 0, object_bytes,
            (size_t)field_type->size);
    }
    if (!field_type || !initializer ||
        !(type_is_arithmetic(field_type) ||
          field_type->kind == TYPE_ENUM || field_type->kind == TYPE_PTR ||
          field_type->kind == TYPE_NULLPTR) ||
        field_type->size <= 0 ||
        (g_opts.target_arch == ARCH_X86 && field_type->size > 4) ||
        (g_opts.target_arch == ARCH_X64 && field_type->size > 8)) {
        return false;
    }
    if (!sema_expr(initializer) ||
        cxx_conversion_rank(initializer, field_type) < 0 ||
        !sema_eval_constexpr_scalar_expr(initializer, NULL, 0, &value) ||
        !sema_constexpr_scalar_convert(&value, field_type, &value)) {
        return false;
    }
    return true;
}

static bool sema_cxx_validate_default_member_initializers(
    Type* object_type, SourceLoc loc) {
    if (!object_type || !object_type->cxx_class ||
        !object_type->cxx_class->has_field_initializer) {
        return false;
    }
    for (TypeField* field = object_type->fields; field; field = field->next) {
        if (!field->initializer) continue;
        if (!sema_cxx_default_member_constant(
                field->type, field->initializer)) {
            rcc_error(loc,
                      "new requires scalar constant default member initializers");
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

static const char* sema_cxx_unqualified_name(const char* name) {
    const char* separator;
    if (!name) return NULL;
    separator = strrchr(name, ':');
    return separator && separator > name && separator[-1] == ':'
        ? separator + 1 : name;
}

static int sema_cxx_constructor_base_index(CxxClass* cls,
                                            const char* name) {
    const char* name_tail = sema_cxx_unqualified_name(name);
    if (!cls || !name || !name_tail) return -1;
    for (int index = 0; index < cls->base_count; ++index) {
        CxxClass* base = cls->bases[index].base;
        if ((cls->bases[index].base_name &&
             strcmp(cls->bases[index].base_name, name) == 0) ||
            (base && base->name && strcmp(base->name, name) == 0) ||
            (base && base->name &&
             strcmp(sema_cxx_unqualified_name(base->name), name_tail) == 0)) {
            return index;
        }
    }
    return -1;
}

static int sema_cxx_constructor_virtual_base_index(CxxClass* cls,
                                                   const char* name) {
    const char* name_tail = sema_cxx_unqualified_name(name);
    if (!cls || !name || !name_tail) return -1;
    for (int index = 0; index < cls->virtual_base_count; ++index) {
        CxxClass* base = cls->virtual_bases[index].base;
        if ((base && base->name && strcmp(base->name, name) == 0) ||
            (base && base->name &&
             strcmp(sema_cxx_unqualified_name(base->name), name_tail) == 0)) {
            return index;
        }
    }
    return -1;
}

static Expr* sema_cxx_object_member(Expr* object, TypeField* field) {
    Expr* member;
    if (!object || !field || !field->name) return NULL;
    member = expr_member(object, field->name, object->loc);
    member->member_field = field;
    member->type = field->type;
    return member;
}

/* Build an lvalue for a base subobject from its byte offset in the complete
 * object. Layout has already validated the fixed offset for both target
 * ABIs, and ordinary pointer arithmetic keeps the adjustment visible to
 * code generation. */
static Expr* sema_cxx_object_base(Expr* object, Type* object_type,
                                  Type* base_type, int offset) {
    SourceLoc loc;
    Type* byte_pointer_type;
    Type* base_pointer_type;
    Expr* address;
    Expr* byte_pointer;
    Expr* adjusted_bytes;
    Expr* base_pointer;
    Expr* base_object;
    if (!object || !object_type || !base_type || offset < 0) return NULL;
    loc = object->loc;
    byte_pointer_type = type_ptr(type_char);
    base_pointer_type = type_ptr(base_type);
    address = expr_unary(EXPR_ADDR, object, loc);
    address->type = type_ptr(object_type);
    byte_pointer = expr_cast(byte_pointer_type, address, loc);
    byte_pointer->type = byte_pointer_type;
    adjusted_bytes = offset == 0
        ? byte_pointer
        : expr_binary(EXPR_ADD, byte_pointer, expr_int(offset, loc), loc);
    adjusted_bytes->type = byte_pointer_type;
    base_pointer = expr_cast(base_pointer_type, adjusted_bytes, loc);
    base_pointer->type = base_pointer_type;
    base_object = expr_unary(EXPR_DEREF, base_pointer, loc);
    base_object->type = base_type;
    return base_object;
}

static void sema_cxx_cleanup_append(CxxCleanupPlan** cleanups,
                                    CxxCleanupPlan* item) {
    CxxCleanupPlan** tail = cleanups;
    while (*tail) tail = &(*tail)->next;
    *tail = item;
}

static void sema_cxx_cleanup_append_expression(CxxCleanupPlan** cleanups,
                                              Expr* expression) {
    CxxCleanupPlan* item = rcc_alloc(sizeof(*item));
    memset(item, 0, sizeof(*item));
    item->kind = CXX_CLEANUP_EXPRESSION;
    item->expression = expression;
    sema_cxx_cleanup_append(cleanups, item);
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
    if (object_type->kind == TYPE_ARRAY) {
        return object_type->base &&
            sema_cxx_type_has_destructor_cleanup(object_type->base,
                                                  depth + 1);
    }
    if (sema_cxx_destructor_function(object_type) ||
        (object_type->cleanup_function && object_type->cleanup_field)) {
        return true;
    }
    cls = object_type->cxx_class;
    if (!cls) return false;
    for (int index = 0; index < cls->base_count; ++index) {
        CxxClass* base = cls->bases[index].base;
        if (base && base->type &&
            sema_cxx_type_has_destructor_cleanup(base->type, depth + 1)) {
            return true;
        }
    }
    for (int index = 0; index < cls->virtual_base_count; ++index) {
        CxxClass* base = cls->virtual_bases[index].base;
        if (base && base->type &&
            sema_cxx_type_has_destructor_cleanup(base->type, depth + 1)) {
            return true;
        }
    }
    for (TypeParam* parameter = cls->fields; parameter;
         parameter = parameter->next) {
        Type* field_type;
        if (parameter->is_static) continue;
        field_type = parameter->type;
        if (field_type &&
            sema_cxx_type_has_destructor_cleanup(field_type, depth + 1)) {
            return true;
        }
    }
    return false;
}

/* Automatic objects without an initializer are safe to leave untouched only
 * when their default-initialization needs no constructor/member/base work.
 * This conservative predicate keeps array declarations from silently
 * skipping required C++ initialization while still allowing cleanup for
 * implicitly trivial default construction. */
static bool sema_cxx_default_initialization_needs_lowering(
    Type* object_type, int depth) {
    CxxClass* cls;
    if (!object_type || depth > 32) return true;
    if (object_type->kind == TYPE_ARRAY) {
        if (object_type->array_len < 0 || !object_type->base) return true;
        if (object_type->array_len == 0) return false;
        return sema_cxx_default_initialization_needs_lowering(
            object_type->base, depth + 1);
    }
    if (object_type->kind != TYPE_STRUCT &&
        object_type->kind != TYPE_UNION) {
        return object_type->is_reference;
    }
    cls = object_type->cxx_class;
    if (!cls) return false;
    /* Vtable/vbtable pointer installation is handled by the local-object
     * backend path; it does not itself require a C++ constructor call. */
    if (cls->has_user_constructor || cls->has_field_initializer) {
        return true;
    }
    for (int index = 0; index < cls->base_count; ++index) {
        CxxClass* base = cls->bases[index].base;
        if (!base || !base->type ||
            sema_cxx_default_initialization_needs_lowering(
                base->type, depth + 1)) {
            return true;
        }
    }
    for (int index = 0; index < cls->virtual_base_count; ++index) {
        CxxClass* base = cls->virtual_bases[index].base;
        if (!base || !base->type ||
            sema_cxx_default_initialization_needs_lowering(
                base->type, depth + 1)) {
            return true;
        }
    }
    for (TypeParam* parameter = cls->fields; parameter;
         parameter = parameter->next) {
        if (!parameter->is_static && parameter->type &&
            sema_cxx_default_initialization_needs_lowering(
                parameter->type, depth + 1)) {
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
                                            CxxCleanupPlan** cleanups,
                                            int depth,
                                            int* cleanup_budget,
                                            bool include_virtual_bases,
                                            bool allow_runtime_array_loops) {
    CxxClass* cls;
    TypeField** fields;
    int field_count = 0;
    int field_index = 0;
    bool valid = true;
    bool is_union;
    if (!object_type || !object || !cleanups || !cleanup_budget ||
        depth > 32) return false;
    if (object_type->kind == TYPE_ARRAY) {
        if (!object_type->base || object_type->array_len < 0) {
            return false;
        }
        if (object_type->array_len == 0 ||
            !sema_cxx_type_has_destructor_cleanup(object_type->base, 0)) {
            return true;
        }
        if (allow_runtime_array_loops && object_type->array_len > 1) {
            Decl* index_decl = decl_var("__rcc_cleanup_array_index",
                                        type_int, NULL, object->loc);
            Expr* index = expr_ident(index_decl->name, object->loc);
            Expr* element;
            CxxCleanupPlan* body = NULL;
            CxxCleanupPlan* loop;
            index->ident_decl = index_decl;
            index->type = type_int;
            element = expr_index(object, index, object->loc);
            element->type = object_type->base;
            if (!sema_cxx_append_object_cleanups(
                    declaration, object_type->base, element, &body,
                    depth + 1, cleanup_budget, include_virtual_bases,
                    allow_runtime_array_loops)) {
                return false;
            }
            if (!body) return true;
            loop = rcc_alloc(sizeof(*loop));
            memset(loop, 0, sizeof(*loop));
            loop->kind = CXX_CLEANUP_ARRAY_LOOP;
            loop->index_decl = index_decl;
            loop->element_count = object_type->array_len;
            loop->body = body;
            sema_cxx_cleanup_append(cleanups, loop);
            return true;
        }
        if (object_type->array_len > 4096) return false;
        for (int index = 0; index < object_type->array_len; ++index) {
            Expr* element = expr_index(
                object, expr_int(index, object->loc), object->loc);
            element->type = object_type->base;
            if (!sema_cxx_append_object_cleanups(
                    declaration, object_type->base, element, cleanups,
                    depth + 1, cleanup_budget, include_virtual_bases,
                    allow_runtime_array_loops)) {
                valid = false;
            }
        }
        return valid;
    }
    cls = object_type->cxx_class;
    if (!cls) return true;
    is_union = object_type->kind == TYPE_UNION;
    if (is_union &&
        sema_cxx_type_has_destructor_cleanup(object_type, depth) &&
        !sema_cxx_destructor_function(object_type) &&
        !(object_type->cleanup_function && object_type->cleanup_field)) {
        return false;
    }
    if (!is_union && include_virtual_bases) {
        for (int index = 0; index < cls->virtual_base_count; ++index) {
            CxxClass* base = cls->virtual_bases[index].base;
            Expr* base_object;
            if (!base || !base->type) {
                valid = false;
                continue;
            }
            base_object = sema_cxx_object_base(
                object, object_type, base->type,
                cls->virtual_bases[index].offset);
            if (!base_object || !sema_cxx_append_object_cleanups(
                    declaration, base->type, base_object, cleanups,
                    depth + 1, cleanup_budget, false,
                    allow_runtime_array_loops)) {
                valid = false;
            }
        }
    }
    if (!is_union) {
        for (int index = 0; index < cls->base_count; ++index) {
            CxxClass* base = cls->bases[index].base;
            Expr* base_object;
            if (cls->bases[index].is_virtual) continue;
            if (!base || !base->type || !cls->base_offsets ||
                cls->base_offsets[index] < 0) {
                valid = false;
                continue;
            }
            base_object = sema_cxx_object_base(
                object, object_type, base->type,
                cls->base_offsets[index]);
            if (!base_object || !sema_cxx_append_object_cleanups(
                    declaration, base->type, base_object, cleanups,
                    depth + 1, cleanup_budget, false,
                    allow_runtime_array_loops)) {
                valid = false;
            }
        }
    }
    if (!is_union) {
        for (TypeParam* parameter = cls->fields; parameter;
             parameter = parameter->next) {
            if (!parameter->is_static && parameter->type &&
                sema_cxx_type_has_destructor_cleanup(parameter->type, 0)) {
                ++field_count;
            }
        }
    }
    fields = field_count ? rcc_alloc(sizeof(*fields) * (size_t)field_count)
                         : NULL;
    if (!is_union) {
        for (TypeParam* parameter = cls->fields; parameter;
             parameter = parameter->next) {
            TypeField* field;
            if (parameter->is_static || !parameter->type ||
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
    }
    for (int index = 0; index < field_index; ++index) {
        Expr* member = sema_cxx_object_member(object, fields[index]);
        if (!member || !sema_cxx_append_object_cleanups(
                declaration, fields[index]->type, member, cleanups,
                depth + 1, cleanup_budget, true,
                allow_runtime_array_loops)) {
            valid = false;
        }
    }
    if (sema_cxx_destructor_function(object_type)) {
        Expr* destructor = sema_cxx_destructor_call_for_object(
            object_type, object, declaration ? declaration->loc : object->loc);
        if (destructor && *cleanup_budget > 0) {
            --*cleanup_budget;
            sema_cxx_cleanup_append_expression(cleanups, destructor);
        }
        else valid = false;
    } else if (object_type->cleanup_function && object_type->cleanup_field) {
        Expr* cleanup = sema_cxx_wrapper_cleanup_for_object(
            object_type, object, declaration ? declaration->loc : object->loc);
        if (cleanup && *cleanup_budget > 0) {
            --*cleanup_budget;
            sema_cxx_cleanup_append_expression(cleanups, cleanup);
        }
        else valid = false;
    }
    if (fields) rcc_free(fields);
    return valid;
}

/* A class prvalue used as a reference argument or member-call receiver lives
 * through the complete containing full-expression.  Keep a synthetic local
 * owner so codegen binds cleanup to its caller-frame slot instead of
 * reevaluating the producing expression. */
static bool sema_prepare_class_prvalue_cleanup(
    Expr* source, Decl** owner_out, CxxCleanupPlan** cleanups_out,
    bool owner_stores_address, const char* unsupported_message) {
    Type* object_type;
    Expr* object;
    CxxCleanupPlan* cleanups = NULL;
    int cleanup_budget = 4096;
    if (owner_out) *owner_out = NULL;
    if (cleanups_out) *cleanups_out = NULL;
    if (!rcc_parser_is_cxx_mode() || !source || !source->type ||
        is_lvalue(source) || is_xvalue(source)) {
        return false;
    }
    object_type = source->type;
    if ((object_type->kind != TYPE_STRUCT &&
         object_type->kind != TYPE_UNION) ||
        !sema_cxx_type_has_destructor_cleanup(object_type, 0)) {
        return false;
    }
    if (!owner_stores_address && source->kind != EXPR_CALL &&
        source->kind != EXPR_COMPOUND) {
        rcc_error(source->loc, "%s", unsupported_message);
        return true;
    }
    if (!owner_out || !cleanups_out) return true;
    *owner_out = decl_var(
        "__rcc_full_expression_temporary",
        owner_stores_address ? type_reference(object_type, false)
                             : object_type,
        NULL, source->loc);
    object = expr_ident((*owner_out)->name, source->loc);
    object->ident_decl = *owner_out;
    object->type = object_type;
    if (!sema_cxx_append_object_cleanups(
            NULL, object_type, object, &cleanups, 0, &cleanup_budget,
            true, true) || !cleanups) {
        *owner_out = NULL;
        rcc_error(source->loc, "%s", unsupported_message);
        return true;
    }
    *cleanups_out = cleanups;
    return true;
}

static void sema_prepare_reference_argument_cleanup(ExprList* argument,
                                                    Type* parameter_type) {
    if (!argument || !argument->expr || !parameter_type ||
        !parameter_type->is_reference) {
        return;
    }
    sema_prepare_class_prvalue_cleanup(
        argument->expr, &argument->cxx_temporary_owner,
        &argument->cxx_temporary_cleanups,
        true,
        "class-prvalue reference argument cleanup is unsupported");
}

static const char* sema_reference_temporary_symbol(
    const Decl* declaration, const char* suffix) {
    const char* base = decl_link_name(declaration);
    size_t base_length = base ? strlen(base) : 0u;
    size_t suffix_length = suffix ? strlen(suffix) : 0u;
    char* symbol;
    if (base_length > SIZE_MAX - suffix_length - 1u) return NULL;
    symbol = rcc_alloc(base_length + suffix_length + 1u);
    if (base_length) memcpy(symbol, base, base_length);
    if (suffix_length) memcpy(symbol + base_length, suffix, suffix_length);
    symbol[base_length + suffix_length] = '\0';
    return symbol;
}

static bool sema_attach_static_initializer_guard(
    Decl* declaration) {
    const char* guard_name;
    if (!declaration ||
        (!declaration->var_is_thread_local &&
         !declaration->var_is_static_local) ||
        declaration->var_reference_temporary_guard) {
        return declaration != NULL;
    }
    guard_name = sema_reference_temporary_symbol(
        declaration, "$rcc_reference_guard");
    if (!guard_name) {
        rcc_error(declaration->loc,
                  "static initialization guard symbol is too large");
        return false;
    }
    declaration->var_reference_temporary_guard = decl_var(
        "__rcc_static_guard", type_llong, NULL, declaration->loc);
    declaration->var_reference_temporary_guard->name = guard_name;
    declaration->var_reference_temporary_guard->link_name = guard_name;
    declaration->var_reference_temporary_guard->storage = STORAGE_STATIC;
    declaration->var_reference_temporary_guard->var_is_global = true;
    declaration->var_reference_temporary_guard->var_is_thread_local =
        declaration->var_is_thread_local;
    return true;
}

static void sema_prepare_variable_destructor_cleanup(Decl* declaration) {
    Expr* object;
    Expr* materialized_xvalue_source = NULL;
    Expr* static_xvalue_source = NULL;
    Expr* static_subobject_source = NULL;
    Type* object_type;
    bool reference_temporary = false;
    bool static_reference_temporary = false;
    bool static_xvalue_has_temporary = false;
    int cleanup_budget = 4096;
    if (!rcc_parser_is_cxx_mode() || !declaration ||
        !declaration->type || declaration->var_cleanup ||
        declaration->var_cleanups) {
        return;
    }
    object_type = declaration->type;
    if (object_type->kind == TYPE_PTR && object_type->is_reference) {
        bool prvalue_initializer;
        if (!declaration->var_init) return;
        materialized_xvalue_source =
            sema_cxx_reference_temporary_source(declaration->var_init);
        if ((declaration->var_is_global ||
             declaration->var_is_static_local) &&
            is_xvalue(declaration->var_init)) {
            static_xvalue_has_temporary =
                sema_cxx_static_reference_has_temporary_source(
                    declaration->var_init);
            static_xvalue_source =
                sema_cxx_static_reference_temporary_source(
                    declaration->var_init);
            if (static_xvalue_has_temporary && !static_xvalue_source) {
                rcc_error(declaration->loc,
                          "static reference lifetime extension for this class xvalue path is unsupported");
                return;
            }
            if (static_xvalue_source &&
                sema_cxx_reference_subobject_path(
                    declaration->var_init, static_xvalue_source)) {
                static_subobject_source = static_xvalue_source;
            }
        }
        prvalue_initializer = !is_lvalue(declaration->var_init) &&
                              !is_xvalue(declaration->var_init);
        object_type = static_xvalue_source
            ? static_xvalue_source->type
            : materialized_xvalue_source
                ? (declaration->var_init->type
                       ? declaration->var_init->type->base : NULL)
                : declaration->var_init->type;
        if (materialized_xvalue_source && object_type &&
            materialized_xvalue_source->type &&
            (materialized_xvalue_source->type->kind == TYPE_STRUCT ||
             materialized_xvalue_source->type->kind == TYPE_UNION) &&
            (object_type->kind == TYPE_STRUCT ||
             object_type->kind == TYPE_UNION) &&
            !type_is_compatible(materialized_xvalue_source->type,
                                object_type)) {
            int virtual_index;
            int nested_adjustment;
            if (sema_cxx_unique_public_base(
                    materialized_xvalue_source->type, object_type, NULL) ||
                sema_cxx_virtual_object_conversion(
                    materialized_xvalue_source->type, object_type,
                    &virtual_index, &nested_adjustment)) {
                /* A reference cast to a base subobject does not change the
                 * complete temporary's type or destructor sequence. */
                object_type = materialized_xvalue_source->type;
            }
        }
        static_reference_temporary = object_type &&
            (declaration->var_is_global || declaration->var_is_static_local) &&
            !declaration->var_is_block_extern &&
            (prvalue_initializer || materialized_xvalue_source ||
             static_xvalue_source);
        if (declaration->var_is_thread_local && object_type &&
            !declaration->var_is_block_extern &&
            (prvalue_initializer || materialized_xvalue_source ||
             static_xvalue_source)) {
            static_reference_temporary = true;
        }
        reference_temporary = object_type &&
            (static_reference_temporary ||
             (!declaration->var_is_global &&
              !declaration->var_is_static_local &&
              !declaration->var_is_block_extern &&
              (prvalue_initializer || materialized_xvalue_source)));
        if (!static_xvalue_source && materialized_xvalue_source && object_type &&
            (object_type->kind == TYPE_STRUCT ||
             object_type->kind == TYPE_UNION) &&
            materialized_xvalue_source->type &&
            (materialized_xvalue_source->type->kind == TYPE_STRUCT ||
             materialized_xvalue_source->type->kind == TYPE_UNION) &&
            !type_is_compatible(materialized_xvalue_source->type,
                                object_type)) {
            rcc_error(declaration->loc,
                      "lifetime extension for a converted class xvalue is unsupported");
            return;
        }
        if (!reference_temporary) return;
    }
    if (!object_type) return;
    if (!declaration->var_init &&
        (declaration->var_is_block_extern ||
         (declaration->var_is_global &&
          declaration->storage == STORAGE_EXTERN))) {
        return;
    }
    if (!declaration->var_init &&
        sema_cxx_default_initialization_needs_lowering(object_type, 0)) {
        rcc_error(declaration->loc,
                  "default initialization of this C++ object requires unsupported constructor or member initialization");
        return;
    }
    if (!static_reference_temporary &&
        object_type->kind != TYPE_STRUCT &&
        object_type->kind != TYPE_UNION &&
        object_type->kind != TYPE_ARRAY) return;
    if (object_type->kind == TYPE_UNION &&
        sema_cxx_type_has_destructor_cleanup(object_type, 0) &&
        !sema_cxx_destructor_function(object_type) &&
        !(object_type->cleanup_function && object_type->cleanup_field)) {
        rcc_error(declaration->loc,
                  "union with a nontrivial member requires an explicit cleanup");
        return;
    }
    if (static_reference_temporary ||
        (sema_cxx_type_has_destructor_cleanup(object_type, 0) &&
         reference_temporary)) {
        Decl* owner = decl_var("__rcc_reference_temporary", object_type,
                               NULL, declaration->loc);
        if (static_reference_temporary) {
            const char* owner_name = sema_reference_temporary_symbol(
                declaration, "$rcc_reference_temporary");
            if (!owner_name) {
                rcc_error(declaration->loc,
                          "static reference temporary symbol is too large");
                return;
            }
            owner->name = owner_name;
            owner->link_name = owner_name;
            owner->storage = STORAGE_STATIC;
            owner->var_is_global = true;
            owner->var_is_thread_local = declaration->var_is_thread_local;
            if (declaration->var_is_static_local ||
                declaration->var_is_thread_local) {
                const char* guard_name = sema_reference_temporary_symbol(
                    declaration, "$rcc_reference_guard");
                if (!guard_name) {
                    rcc_error(declaration->loc,
                              "static reference guard symbol is too large");
                    return;
                }
                declaration->var_reference_temporary_guard = decl_var(
                    "__rcc_reference_guard", type_llong, NULL,
                    declaration->loc);
                declaration->var_reference_temporary_guard->name = guard_name;
                declaration->var_reference_temporary_guard->link_name =
                    guard_name;
                declaration->var_reference_temporary_guard->storage =
                    STORAGE_STATIC;
                declaration->var_reference_temporary_guard->var_is_global =
                    true;
                declaration->var_reference_temporary_guard->var_is_thread_local =
                    declaration->var_is_thread_local;
            }
        }
        declaration->var_reference_temporary_owner = owner;
        declaration->var_reference_temporary_source =
            static_subobject_source;
        object = expr_ident(owner->name, declaration->loc);
        object->ident_decl = owner;
        object->type = object_type;
    } else {
        object = expr_ident(declaration->name, declaration->loc);
        object->ident_decl = declaration;
        object->type = object_type;
    }
    if (sema_cxx_type_has_destructor_cleanup(object_type, 0) &&
        !sema_cxx_append_object_cleanups(declaration, object_type, object,
                                         &declaration->var_cleanups, 0,
                                         &cleanup_budget, true,
                                         !declaration->var_is_thread_local)) {
        rcc_error(declaration->loc,
                  "C++ object lifetime cleanup metadata is incomplete");
    }
    if ((declaration->var_is_thread_local ||
         declaration->var_is_static_local) && declaration->var_cleanups) {
        (void)sema_attach_static_initializer_guard(declaration);
    }
}

static void sema_resolve_cxx_constructor_initializers(
    CxxConstructorInfo* constructor, SourceLoc loc);

static CxxConstructorInfo* sema_constructor_resolution_stack[64];
static unsigned sema_constructor_resolution_depth;

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

static void sema_bind_cxx_constructor_expression(
    CxxConstructorInfo* constructor, Expr* expression) {
    if (!expression) return;
    if (expression->kind == EXPR_IDENT) {
        Decl* parameter = sema_cxx_constructor_parameter(
            constructor, expression->ident_name);
        if (parameter) {
            expression->ident_decl = parameter;
            expression->type = parameter->type;
        }
        return;
    }
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
            sema_bind_cxx_constructor_expression(
                constructor, expression->unary_operand);
            return;
        case EXPR_CAST:
            sema_bind_cxx_constructor_expression(
                constructor, expression->cast_expr);
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
        case EXPR_CXX_MEMBER_PTR_DOT:
        case EXPR_CXX_MEMBER_PTR_ARROW:
            sema_bind_cxx_constructor_expression(
                constructor, expression->binary_lhs);
            sema_bind_cxx_constructor_expression(
                constructor, expression->binary_rhs);
            return;
        case EXPR_COND:
            sema_bind_cxx_constructor_expression(
                constructor, expression->cond_test);
            sema_bind_cxx_constructor_expression(
                constructor, expression->cond_then);
            sema_bind_cxx_constructor_expression(
                constructor, expression->cond_else);
            return;
        case EXPR_INDEX:
            sema_bind_cxx_constructor_expression(
                constructor, expression->index_base);
            sema_bind_cxx_constructor_expression(
                constructor, expression->index_expr);
            return;
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            sema_bind_cxx_constructor_expression(
                constructor, expression->member_base);
            return;
        case EXPR_CALL:
            sema_bind_cxx_constructor_expression(
                constructor, expression->call_func);
            for (ExprList* argument = expression->call_args; argument;
                 argument = argument->next) {
                sema_bind_cxx_constructor_expression(
                    constructor, argument->expr);
            }
            return;
        default:
            return;
    }
}

/* Constructor calls use the same trailing-default rule as ordinary C++
 * functions, but their arguments are consumed by the constructor lowering
 * helpers rather than by the ordinary call expression.  Materialize the
 * omitted values before code generation so every constructor ABI call has
 * its complete parameter list. */
static bool sema_cxx_constructor_arity_has_defaults(
    CxxConstructorInfo* constructor, int supplied_count) {
    TypeParam* parameter;
    DeclList* declaration;
    int index;
    if (!constructor || supplied_count < 0 ||
        supplied_count > constructor->parameter_count) return false;
    parameter = constructor->parameters;
    declaration = constructor->method && constructor->method->decl
        ? constructor->method->decl->func_params : NULL;
    for (index = 0; index < supplied_count; ++index) {
        if (!parameter || !declaration) return false;
        parameter = parameter->next;
        declaration = declaration->next;
    }
    while (parameter && declaration) {
        if (!declaration->decl || !declaration->decl->param_default) {
            return false;
        }
        parameter = parameter->next;
        declaration = declaration->next;
    }
    return !parameter && !declaration;
}

static bool sema_append_cxx_constructor_default_arguments(
    ExprList** arguments, CxxConstructorInfo* constructor,
    int supplied_count) {
    TypeParam* parameter;
    DeclList* declaration;
    int index;
    if (!arguments || !constructor ||
        !sema_cxx_constructor_arity_has_defaults(constructor, supplied_count)) {
        return false;
    }
    parameter = constructor->parameters;
    declaration = constructor->method && constructor->method->decl
        ? constructor->method->decl->func_params : NULL;
    for (index = 0; index < supplied_count; ++index) {
        parameter = parameter->next;
        declaration = declaration->next;
    }
    while (parameter && declaration) {
        exprlist_append(arguments, declaration->decl->param_default);
        parameter = parameter->next;
        declaration = declaration->next;
    }
    return true;
}

static CxxConstructorInfo* sema_select_cxx_delegating_constructor(
    CxxClass* cls, CxxConstructorInfo* current, ExprList** arguments,
    SourceLoc loc) {
    CxxConstructorInfo* candidate;
    CxxConstructorInfo* best = NULL;
    ExprList* supplied_arguments = arguments ? *arguments : NULL;
    int argument_count = sema_cxx_argument_count(supplied_arguments);
    int best_ranks[32] = { 0 };
    bool ambiguous = false;

    if (!cls || !cls->type || argument_count < 0 || argument_count >= 32) {
        return NULL;
    }
    for (candidate = cls->constructors; candidate;
         candidate = candidate->next) {
        TypeParam* parameter;
        ExprList* argument;
        int candidate_ranks[32] = { 0 };
        int rank_count = 0;
        bool viable = true;
        if (candidate == current || !candidate->method ||
            candidate->method->owner != cls ||
            candidate->access != ACCESS_PUBLIC || candidate->is_deleted ||
            candidate->is_defaulted || candidate->parameter_count < argument_count ||
            (candidate->parameter_count != argument_count &&
             !sema_cxx_constructor_arity_has_defaults(
                 candidate, argument_count)) ||
            !candidate->initializers_are_supported ||
            (!candidate->body_is_empty &&
             (!candidate->method->decl ||
              !candidate->method->decl->func_is_cxx_method ||
              !candidate->method->decl->func_body))) {
            continue;
        }
        parameter = candidate->parameters;
        argument = supplied_arguments;
        while (parameter && argument) {
            int rank = cxx_conversion_rank(argument->expr, parameter->type);
            if (rank < 0) {
                viable = false;
                break;
            }
            candidate_ranks[rank_count++] = rank;
            parameter = parameter->next;
            argument = argument->next;
        }
        if (!viable || argument ||
            (parameter && !sema_cxx_constructor_arity_has_defaults(
                candidate, argument_count))) {
            continue;
        }
        {
            int relation = best
                ? cxx_conversion_vector_relation(
                    candidate_ranks, best_ranks, rank_count)
                : 1;
            if (!best || (relation > 0 && !ambiguous)) {
                best = candidate;
                memcpy(best_ranks, candidate_ranks,
                       sizeof(best_ranks));
                ambiguous = false;
            } else if (relation == 0) {
                ambiguous = true;
            }
        }
    }
    if (ambiguous) {
        rcc_error(loc, "ambiguous C++ delegating constructor");
        return NULL;
    }
    if (best && arguments && argument_count < best->parameter_count &&
        !sema_append_cxx_constructor_default_arguments(
            arguments, best, argument_count)) {
        rcc_error(loc,
                  "delegating constructor defaults are not safely lowerable");
        return NULL;
    }
    return best;
}

static CxxConstructorInfo* sema_select_cxx_new_constructor_ex(
    Type* object_type, ExprList** arguments, SourceLoc loc,
    bool allow_explicit) {
    CxxClass* cls = object_type ? object_type->cxx_class : NULL;
    CxxConstructorInfo* candidate;
    CxxConstructorInfo* best = NULL;
    ExprList* supplied_arguments = arguments ? *arguments : NULL;
    int argument_count = sema_cxx_argument_count(supplied_arguments);
    int best_ranks[32] = { 0 };
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
        int candidate_ranks[32] = { 0 };
        int rank_count = 0;
        bool viable = true;
        if (!candidate->method || candidate->access != ACCESS_PUBLIC ||
            (!allow_explicit && candidate->method->is_explicit) ||
            candidate->is_deleted || candidate->is_defaulted ||
            candidate->parameter_count < argument_count ||
            (candidate->parameter_count != argument_count &&
             !sema_cxx_constructor_arity_has_defaults(
                 candidate, argument_count)) ||
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
        argument = supplied_arguments;
        while (parameter && argument) {
            int rank = cxx_conversion_rank(argument->expr, parameter->type);
            if (rank < 0) {
                viable = false;
                break;
            }
            candidate_ranks[rank_count++] = rank;
            parameter = parameter->next;
            argument = argument->next;
        }
        if (!viable || argument ||
            (parameter && !sema_cxx_constructor_arity_has_defaults(
                candidate, argument_count))) continue;
        {
            int relation = best
                ? cxx_conversion_vector_relation(
                    candidate_ranks, best_ranks, rank_count)
                : 1;
            if (!best || (relation > 0 && !ambiguous)) {
                best = candidate;
                memcpy(best_ranks, candidate_ranks,
                       sizeof(best_ranks));
                ambiguous = false;
            } else if (relation == 0) {
                ambiguous = true;
            }
        }
    }
    if (ambiguous) {
        rcc_error(loc, "ambiguous constructor for C++ new expression");
        return NULL;
    }
    sema_resolve_cxx_constructor_initializers(best, loc);
    if (best && arguments && argument_count < best->parameter_count &&
        !sema_append_cxx_constructor_default_arguments(
            arguments, best, argument_count)) {
        rcc_error(loc,
                  "constructor default arguments are not safely lowerable");
        return NULL;
    }
    return best;
}

static CxxConstructorInfo* sema_select_cxx_new_constructor(
    Type* object_type, ExprList** arguments, SourceLoc loc) {
    return sema_select_cxx_new_constructor_ex(
        object_type, arguments, loc, true);
}

static void sema_resolve_cxx_constructor_initializers(
    CxxConstructorInfo* constructor, SourceLoc loc) {
    CxxClass* cls;
    unsigned index;
    if (!constructor || !constructor->method ||
        !constructor->method->owner || !constructor->method->owner->type) {
        return;
    }
    for (index = 0; index < sema_constructor_resolution_depth; ++index) {
        if (sema_constructor_resolution_stack[index] == constructor) {
            rcc_error(loc, "cyclic C++ delegating constructor");
            return;
        }
    }
    if (sema_constructor_resolution_depth >=
        sizeof(sema_constructor_resolution_stack) /
            sizeof(sema_constructor_resolution_stack[0])) {
        rcc_error(loc, "C++ constructor delegation depth is unsupported");
        return;
    }
    sema_constructor_resolution_stack[
        sema_constructor_resolution_depth++] = constructor;
    cls = constructor->method->owner;
    for (CxxConstructorInitializer* initializer = constructor->initializers;
         initializer; initializer = initializer->next) {
        if (initializer->is_delegating_constructor) {
            CxxConstructorInfo* target;
            ExprList* argument = initializer->arguments;
            int argument_count = sema_cxx_argument_count(argument);
            for (; argument; argument = argument->next) {
                sema_bind_cxx_constructor_expression(
                    constructor, argument->expr);
                sema_expr(argument->expr);
            }
            target = sema_select_cxx_delegating_constructor(
                cls, constructor, &initializer->arguments, loc);
            if (!target) {
                initializer->constructor = NULL;
                rcc_error(loc,
                          "delegating constructor target is not safely lowerable");
                continue;
            }
            initializer->constructor = target;
            if (argument_count < target->parameter_count) {
                for (argument = initializer->arguments; argument;
                     argument = argument->next) {
                    sema_bind_cxx_constructor_expression(
                        constructor, argument->expr);
                    sema_expr(argument->expr);
                }
            }
            sema_resolve_cxx_constructor_initializers(target, loc);
            continue;
        }
        int base_index = sema_cxx_constructor_base_index(
            cls, initializer->field);
        if (base_index >= 0) {
            CxxClass* base = cls->bases[base_index].base;
            Type* base_type = base ? base->type : NULL;
            if (!base_type || !type_is_complete(base_type) ||
                cls->bases[base_index].access != ACCESS_PUBLIC) {
                rcc_error(loc,
                          "base constructor initializer is not safely lowerable");
                continue;
            }
            for (ExprList* argument = initializer->arguments; argument;
                 argument = argument->next) {
                sema_bind_cxx_constructor_expression(
                    constructor, argument->expr);
                sema_expr(argument->expr);
            }
            if (base->constructors || initializer->arguments) {
                initializer->constructor = sema_select_cxx_new_constructor(
                    base_type, &initializer->arguments, loc);
                if (!initializer->constructor) {
                    rcc_error(loc,
                              "no safely lowerable constructor accepts the base initializer");
                }
            }
            initializer->is_virtual_base_initializer =
                cls->bases[base_index].is_virtual;
            continue;
        }
        int virtual_base_index = sema_cxx_constructor_virtual_base_index(
            cls, initializer->field);
        if (virtual_base_index >= 0) {
            CxxClass* base = cls->virtual_bases[virtual_base_index].base;
            Type* base_type = base ? base->type : NULL;
            if (!base_type || !type_is_complete(base_type) ||
                !cls->virtual_bases[virtual_base_index].public_path) {
                rcc_error(loc,
                          "virtual base constructor initializer is not safely lowerable");
                continue;
            }
            initializer->is_base_initializer = true;
            initializer->is_virtual_base_initializer = true;
            for (ExprList* argument = initializer->arguments; argument;
                 argument = argument->next) {
                sema_bind_cxx_constructor_expression(
                    constructor, argument->expr);
                sema_expr(argument->expr);
            }
            if (base->constructors || initializer->arguments) {
                initializer->constructor = sema_select_cxx_new_constructor(
                    base_type, &initializer->arguments, loc);
                if (!initializer->constructor) {
                    rcc_error(loc,
                              "no safely lowerable constructor accepts the virtual base initializer");
                }
            }
            continue;
        }
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
            sema_bind_cxx_constructor_expression(
                constructor, argument->expr);
            sema_expr(argument->expr);
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
                field->type, &initializer->arguments, initializer->value
                    ? initializer->value->loc : loc);
            if (!initializer->constructor) {
                rcc_error(initializer->value ? initializer->value->loc : loc,
                          "no safely lowerable constructor accepts the member initializer");
            }
        }
    }
    --sema_constructor_resolution_depth;
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
    ExprList* selected_arguments = NULL;
    ExprList* one_arguments = NULL;
    TypeParam* parameter;

    if (!object_type || !object_type->cxx_class) return NULL;
    if (!initializers) {
        constructor = sema_select_cxx_new_constructor(
            object_type, &selected_arguments, loc);
        /* The array backend has one flat initializer list and no per-element
         * argument list for an omitted constructor argument.  Reject that
         * shape explicitly instead of emitting a short ABI call in the
         * default-constructor loop. */
        return constructor && constructor->parameter_count == 0
            ? constructor : NULL;
    }
    if (initializers->next) {
        one = *initializers;
        one.next = NULL;
        one_arguments = &one;
        constructor = sema_select_cxx_new_constructor(
            object_type, &one_arguments, loc);
    } else {
        selected_arguments = initializers;
        constructor = sema_select_cxx_new_constructor(
            object_type, &selected_arguments, loc);
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
    int argument_count;
    int* best_ranks;
    bool ambiguous = false;

    if (!call || !call->call_func ||
        call->call_func->kind != EXPR_IDENT ||
        !call->call_func->ident_decl) {
        return NULL;
    }
    argument_count = sema_cxx_argument_count(call->call_args);
    if (argument_count < 0 || argument_count == INT_MAX) {
        rcc_error(call->loc, "too many arguments for overload '%s'",
                  call->call_func->ident_name);
        return NULL;
    }
    best_ranks = argument_count > 0
        ? ast_arena_alloc(sizeof(*best_ranks) * (size_t)argument_count)
        : NULL;
    candidate = call->call_func->ident_decl;
    for (; candidate; candidate = candidate->func_overload_next) {
        TypeParam* parameter;
        DeclList* declared_parameter;
        ExprList* argument;
        int* candidate_ranks = argument_count > 0
            ? ast_arena_alloc(sizeof(*candidate_ranks) *
                              (size_t)argument_count)
            : NULL;
        int rank_count = 0;
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
            if (rank_count >= argument_count) {
                viable = false;
                break;
            }
            candidate_ranks[rank_count++] = rank;
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
                if (rank_count >= argument_count) {
                    viable = false;
                    break;
                }
                candidate_ranks[rank_count++] = 8;
                argument = argument->next;
            }
        }
        if (!viable || rank_count != argument_count) continue;
        {
            int relation = best
                ? cxx_conversion_vector_relation(
                    candidate_ranks, best_ranks, argument_count)
                : 1;
            if (!best || (relation > 0 && !ambiguous)) {
                best = candidate;
                if (argument_count > 0) {
                    memcpy(best_ranks, candidate_ranks,
                           sizeof(*best_ranks) * (size_t)argument_count);
                }
                ambiguous = false;
            } else if (relation == 0) {
                ambiguous = true;
            }
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
    int argument_count;
    int* best_ranks;
    bool ambiguous = false;

    if (!call || !aggregate || !name) return NULL;
    argument_count = sema_cxx_argument_count(call->call_args);
    if (argument_count < 0 || argument_count == INT_MAX) {
        rcc_error(call->loc, "too many arguments for member overload '%s'",
                  name);
        return NULL;
    }
    best_ranks = ast_arena_alloc(sizeof(*best_ranks) *
                                 (size_t)(argument_count + 1));
    for (method = aggregate->methods; method; method = method->next) {
        Decl* function;
        TypeParam* parameter;
        DeclList* declared_parameter;
        ExprList* argument;
        int* candidate_ranks = ast_arena_alloc(
            sizeof(*candidate_ranks) * (size_t)(argument_count + 1));
        int rank_count = 0;
        bool viable = true;
        int object_rank;

        if (method->kind != TYPE_METHOD_FUNCTION || !method->function_decl ||
            strcmp(method->name, name) != 0) {
            continue;
        }
        function = method->function_decl;
        object_rank = cxx_member_object_conversion_rank(aggregate, method);
        if (object_rank < 0) continue;
        candidate_ranks[rank_count++] = object_rank;
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
            if (rank_count > argument_count) {
                viable = false;
                break;
            }
            candidate_ranks[rank_count++] = rank;
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
                if (rank_count > argument_count) {
                    viable = false;
                    break;
                }
                candidate_ranks[rank_count++] = 8;
                argument = argument->next;
            }
        }
        if (!viable || rank_count != argument_count + 1) continue;
        {
            int relation = best
                ? cxx_conversion_vector_relation(
                    candidate_ranks, best_ranks, argument_count + 1)
                : 1;
            if (!best || (relation > 0 && !ambiguous)) {
                best = method;
                memcpy(best_ranks, candidate_ranks,
                       sizeof(*best_ranks) * (size_t)(argument_count + 1));
                ambiguous = false;
            } else if (relation == 0) {
                ambiguous = true;
            }
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
        case EXPR_SPACESHIP: return "operator<=>";
        case EXPR_AND: return "operator&&";
        case EXPR_OR: return "operator||";
        default: return NULL;
    }
}

static bool sema_cxx_spaceship_comparison_kind(ExprKind kind,
                                               ExprKind* comparison_kind) {
    if (!comparison_kind) return false;
    switch (kind) {
        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_GT:
        case EXPR_LE:
        case EXPR_GE:
            *comparison_kind = kind;
            return true;
        default:
            return false;
    }
}

/* C++20 rewrites relational/equality expressions through operator<=> when a
 * direct operator for the requested spelling is absent.  The RinOS bounded
 * comparison ABI represents the comparison category by an integer sign, so
 * only integer-returning spaceship functions are eligible here; category
 * class objects require an explicit diagnostic instead of guessed lowering. */
static bool sema_rewrite_cxx_spaceship_comparison(
    Expr* expression, Type* left_type) {
    ExprKind comparison_kind;
    Type* aggregate;
    Type* right_type;
    Type* right_aggregate;
    TypeMethod* method = NULL;
    Symbol* function = NULL;
    ExprList* arguments = NULL;
    Expr* spaceship_call;
    Expr* rewritten;

    if (!expression || !left_type ||
        !sema_cxx_spaceship_comparison_kind(expression->kind,
                                            &comparison_kind)) {
        return false;
    }
    aggregate = generic_selection_type(left_type);
    if (aggregate && (aggregate->kind == TYPE_STRUCT ||
                      aggregate->kind == TYPE_UNION)) {
        method = sema_find_function_method(aggregate, "operator<=>");
        if (method && method->return_type &&
            !sema_is_integer_type(method->return_type)) {
            rcc_error(expression->loc,
                      "C++20 comparison rewriting requires an integer-returning "
                      "operator<=> in the bounded RCC++ profile");
            return false;
        }
    }
    right_type = sema_expr(expression->binary_rhs);
    right_aggregate = generic_selection_type(right_type);
    if (!method &&
        (!aggregate || (aggregate->kind != TYPE_STRUCT &&
                        aggregate->kind != TYPE_UNION)) &&
        (!right_aggregate || (right_aggregate->kind != TYPE_STRUCT &&
                              right_aggregate->kind != TYPE_UNION))) {
        return false;
    }
    exprlist_append(&arguments, expression->binary_lhs);
    exprlist_append(&arguments, expression->binary_rhs);
    if (!method) {
        function = sema_cxx_operator_function("operator<=>", arguments);
        if (!function || !function->decl || !function->decl->type ||
            function->decl->type->kind != TYPE_FUNC ||
            !function->decl->type->ret_type ||
            !sema_is_integer_type(function->decl->type->ret_type)) {
            if (function && function->decl) {
                rcc_error(expression->loc,
                          "C++20 comparison rewriting requires an integer-returning "
                          "operator<=> in the bounded RCC++ profile");
            }
            return false;
        }
    }
    if (method) {
        Expr* member = expr_member(expression->binary_lhs,
                                   "operator<=>", expression->loc);
        spaceship_call = expr_call(member,
                                   exprlist_new(expression->binary_rhs),
                                   expression->loc);
    } else {
        Expr* function_expression = expr_ident("operator<=>",
                                                expression->loc);
        spaceship_call = expr_call(function_expression, arguments,
                                   expression->loc);
    }
    rewritten = expr_binary(comparison_kind, spaceship_call,
                            expr_int(0, expression->loc), expression->loc);
    *expression = *rewritten;
    return true;
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
        case EXPR_DEREF: return "operator*";
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

/* Lower the bounded RCC++ `operator->` form through the ordinary member-call
 * ABI.  Pointer returns are ABI-equivalent to a built-in pointer selection;
 * class/proxy returns remain an explicit diagnostic until class-return ABI
 * materialization is available, rather than being treated as a fake pointer. */
static bool sema_rewrite_cxx_arrow_operator(Expr* expression,
                                            Type* object_type) {
    Type* aggregate;
    TypeMethod* method;
    Expr* member;
    Expr* call;
    Expr* rewritten;

    if (!expression || expression->kind != EXPR_PTR_MEMBER ||
        !expression->member_base || !object_type) return false;
    aggregate = object_type;
    if (aggregate->kind == TYPE_PTR && aggregate->is_reference) {
        aggregate = aggregate->base;
    }
    aggregate = generic_selection_type(aggregate);
    if (!aggregate || (aggregate->kind != TYPE_STRUCT &&
                       aggregate->kind != TYPE_UNION)) return false;
    method = sema_find_function_method(aggregate, "operator->");
    if (!method || !method->function_decl) return false;
    member = expr_member(expression->member_base, "operator->",
                         expression->loc);
    call = expr_call(member, NULL, expression->loc);
    sema_expr(call);
    if (!call->type || call->type->kind != TYPE_PTR) {
        rcc_error(expression->loc,
                  "operator-> must return a pointer in the bounded RCC++ profile");
        expression->type = type_int;
        return false;
    }
    rewritten = expr_member(call, expression->member_name, expression->loc);
    rewritten->kind = EXPR_PTR_MEMBER;
    *expression = *rewritten;
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

static Expr* sema_cxx_lambda_argument(ExprList* arguments, int index) {
    while (arguments && index > 0) {
        arguments = arguments->next;
        --index;
    }
    return arguments ? arguments->expr : NULL;
}

static bool sema_cxx_lambda_type_uses_parameter(
    Type* type, const char* parameter_name) {
    if (!type || !parameter_name) return false;
    if (type->kind == TYPE_STRUCT && type->tag &&
        strcmp(type->tag, parameter_name) == 0) {
        return true;
    }
    if (type->kind == TYPE_PTR) {
        return sema_cxx_lambda_type_uses_parameter(type->base,
                                                   parameter_name);
    }
    if (type->kind == TYPE_ARRAY) {
        if (sema_cxx_lambda_type_uses_parameter(type->base,
                                                parameter_name)) {
            return true;
        }
        return type->array_bound &&
            type->array_bound->kind == EXPR_IDENT &&
            type->array_bound->ident_name &&
            strcmp(type->array_bound->ident_name, parameter_name) == 0;
    }
    return false;
}

static Type* sema_cxx_lambda_deduction_type(Expr* argument,
                                             Type* parameter_pattern) {
    Type* type = argument ? argument->type : NULL;
    if (!type) return NULL;
    /* For a pointer pattern such as `auto*`, template deduction binds the
     * placeholder to the pointee rather than to the complete argument
     * pointer.  Reference patterns retain the referred object type. */
    if (parameter_pattern && parameter_pattern->kind == TYPE_PTR &&
        !parameter_pattern->is_reference) {
        while (parameter_pattern->kind == TYPE_PTR &&
               !parameter_pattern->is_reference &&
               type->kind == TYPE_PTR) {
            parameter_pattern = parameter_pattern->base;
            type = type->base;
        }
    }
    /* A generic `auto&&` parameter is a forwarding reference.  For an
     * lvalue argument the deduced T is itself an lvalue reference; the
     * substitution layer collapses it with the outer `&&`.  Keep this as a
     * real type rather than allowing the call checker to reject the lvalue
     * as if the lambda had a fixed rvalue-reference parameter. */
    if (parameter_pattern && parameter_pattern->kind == TYPE_PTR &&
        parameter_pattern->is_reference &&
        parameter_pattern->is_rvalue_reference && argument &&
        is_lvalue(argument)) {
        return type_reference(type, false);
    }
    /* Function parameters declared by value apply the standard array/function
     * decay before deduction.  References retain the expression's exact
     * referred type because sema_expr has already removed the ABI carrier. */
    if (type->kind == TYPE_ARRAY) return type_ptr(type->base);
    if (type->kind == TYPE_FUNC) return type_ptr(type);
    return type;
}

static bool sema_cxx_lambda_deduction_value(
    Expr* argument, Type* parameter_pattern, const char* parameter_name,
    int64_t* value) {
    Type* actual;
    if (!argument || !parameter_pattern || !parameter_name || !value ||
        !argument->type) {
        return false;
    }
    actual = argument->type;
    if (parameter_pattern->kind == TYPE_PTR &&
        parameter_pattern->is_reference) {
        return sema_cxx_lambda_deduction_value(
            argument, parameter_pattern->base, parameter_name, value);
    }
    if (parameter_pattern->kind == TYPE_ARRAY) {
        if (parameter_pattern->array_bound &&
            parameter_pattern->array_bound->kind == EXPR_IDENT &&
            parameter_pattern->array_bound->ident_name &&
            strcmp(parameter_pattern->array_bound->ident_name,
                   parameter_name) == 0 &&
            actual->kind == TYPE_ARRAY && actual->array_len > 0) {
            *value = actual->array_len;
            return true;
        }
        return parameter_pattern->base && actual->kind == TYPE_ARRAY &&
            sema_cxx_lambda_deduction_value(
                argument, parameter_pattern->base, parameter_name, value);
    }
    if (parameter_pattern->kind == TYPE_PTR &&
        actual->kind == TYPE_PTR) {
        return sema_cxx_lambda_deduction_value(
            argument, parameter_pattern->base, parameter_name, value);
    }
    return false;
}

/* Instantiate a generic lambda at the call site.  The parser intentionally
 * retains only the dependent call operator; this routine supplies each
 * `auto` parameter's real argument type, then hands the body to the normal
 * template clone/sema/codegen pipeline. */
static bool sema_instantiate_cxx_lambda(Expr* call) {
    Expr* function_expression;
    CxxTemplate* tmpl;
    Decl* lambda_variable = NULL;
    Type* arguments[32] = { NULL };
    Type* pack_arguments[32] = { NULL };
    int64_t pack_values[32] = { 0 };
    bool pack_value_present[32] = { false };
    int64_t values[32] = { 0 };
    bool value_present[32] = { false };
    int capture_count = 0;
    int function_parameter_index;
    int pack_count = 0;
    bool has_pack = false;
    bool has_value_pack = false;

    if (!call || !call->call_func ||
        call->call_func->kind != EXPR_IDENT) return true;
    function_expression = call->call_func;
    if (!function_expression->ident_decl && function_expression->ident_name) {
        Symbol* symbol = sema_cxx_lookup_name(function_expression->ident_name,
                                              function_expression->loc);
        if (symbol && symbol->decl && symbol->decl->kind == DECL_VAR) {
            function_expression->ident_decl = symbol->decl;
            function_expression->cxx_lambda_template =
                symbol->decl->var_cxx_lambda_template;
        }
    }
    tmpl = function_expression->cxx_lambda_template;
    if (!tmpl) return true;
    if (function_expression->ident_decl &&
        function_expression->ident_decl->kind == DECL_VAR &&
        function_expression->ident_decl->var_cxx_lambda_template == tmpl) {
        lambda_variable = function_expression->ident_decl;
    }
    if (!tmpl->func_def || tmpl->param_count <= 0 ||
        tmpl->param_count > (int)(sizeof(arguments) / sizeof(arguments[0]))) {
        rcc_error(call->loc, "generic lambda has an invalid template shape");
        return false;
    }
    for (ExprList* capture = function_expression->cxx_lambda_captures;
         capture; capture = capture->next) {
        ++capture_count;
    }
    {
        bool seen_parameter_pack = false;
        for (DeclList* item = tmpl->func_def->func_params; item;
             item = item->next) {
            if (seen_parameter_pack) {
                rcc_error(call->loc,
                          "generic lambda parameter pack must be last");
                return false;
            }
            if (item->decl && item->decl->param_is_pack) {
                seen_parameter_pack = true;
            }
        }
    }
    for (int template_index = 0; template_index < tmpl->param_count;
         ++template_index) {
        TemplateParam* parameter = &tmpl->params[template_index];
        Expr* argument = NULL;
        Type* parameter_pattern = NULL;
        bool parameter_is_pack = false;
        int pack_first_user_index = -1;
        function_parameter_index = 0;
        for (DeclList* item = tmpl->func_def->func_params; item;
             item = item->next, ++function_parameter_index) {
            if (item->decl && sema_cxx_lambda_type_uses_parameter(
                    item->decl->type, parameter->name)) {
                parameter_pattern = item->decl->type;
                parameter_is_pack = item->decl->param_is_pack;
                if (parameter_is_pack) {
                    pack_first_user_index = function_parameter_index -
                        capture_count;
                } else {
                    argument = sema_cxx_lambda_argument(
                        call->call_args,
                        function_parameter_index - capture_count);
                }
                break;
            }
        }
        if (parameter->kind == TPARAM_NONTYPE && parameter_is_pack) {
            ExprList* pack_argument;
            if (has_pack) {
                rcc_error(call->loc,
                          "generic lambda supports only one parameter pack");
                return false;
            }
            if (pack_first_user_index < 0 || !parameter_pattern) {
                rcc_error(call->loc,
                          "generic lambda non-type parameter pack does not "
                          "match a function parameter pack");
                return false;
            }
            pack_argument = call->call_args;
            while (pack_argument && pack_first_user_index > 0) {
                pack_argument = pack_argument->next;
                --pack_first_user_index;
            }
            while (pack_argument) {
                int64_t deduced_value = 0;
                if (pack_count >= (int)(sizeof(pack_values) /
                                        sizeof(pack_values[0]))) {
                    rcc_error(pack_argument->expr->loc,
                              "generic lambda non-type parameter pack exceeds compiler limits");
                    return false;
                }
                sema_expr(pack_argument->expr);
                if (!sema_cxx_lambda_deduction_value(
                        pack_argument->expr, parameter_pattern,
                        parameter->name, &deduced_value)) {
                    rcc_error(pack_argument->expr->loc,
                              "cannot deduce generic lambda non-type parameter pack value");
                    return false;
                }
                pack_values[pack_count] = deduced_value;
                pack_value_present[pack_count] = true;
                ++pack_count;
                pack_argument = pack_argument->next;
            }
            arguments[template_index] = parameter->type;
            has_pack = true;
            has_value_pack = true;
            continue;
        }
        if (parameter->kind == TPARAM_NONTYPE) {
            int64_t deduced_value = 0;
            if (!argument || !parameter_pattern) {
                rcc_error(call->loc,
                          "generic lambda non-type argument does not match a parameter");
                return false;
            }
            sema_expr(argument);
            if (!sema_cxx_lambda_deduction_value(
                    argument, parameter_pattern, parameter->name,
                    &deduced_value)) {
                rcc_error(argument->loc,
                          "cannot deduce generic lambda non-type parameter");
                return false;
            }
            values[template_index] = deduced_value;
            value_present[template_index] = true;
            continue;
        }
        if (parameter->kind != TPARAM_TYPE ||
            (parameter_is_pack && has_pack)) {
            rcc_error(call->loc,
                      parameter_is_pack
                          ? "generic lambda supports only one parameter pack"
                          : "generic lambda argument does not match an auto parameter");
            return false;
        }
        if (parameter_is_pack) {
            ExprList* pack_argument = call->call_args;
            if (pack_first_user_index < 0) {
                rcc_error(call->loc,
                          "generic lambda parameter pack cannot be a capture");
                return false;
            }
            while (pack_argument && pack_first_user_index > 0) {
                pack_argument = pack_argument->next;
                --pack_first_user_index;
            }
            while (pack_argument) {
                Type* deduced;
                if (pack_count >= (int)(sizeof(pack_arguments) /
                                        sizeof(pack_arguments[0]))) {
                    rcc_error(pack_argument->expr->loc,
                              "generic lambda parameter pack exceeds compiler limits");
                    return false;
                }
                sema_expr(pack_argument->expr);
                deduced = sema_cxx_lambda_deduction_type(
                    pack_argument->expr, parameter_pattern);
                if (!deduced) {
                    rcc_error(pack_argument->expr->loc,
                              "cannot deduce generic lambda parameter pack type");
                    return false;
                }
                pack_arguments[pack_count++] = deduced;
                pack_argument = pack_argument->next;
            }
            arguments[template_index] = pack_count > 0
                ? pack_arguments[0] : type_void;
            has_pack = true;
            continue;
        }
        if (!argument) {
            rcc_error(call->loc,
                      "generic lambda argument does not match an auto parameter");
            return false;
        }
        sema_expr(argument);
        arguments[template_index] = sema_cxx_lambda_deduction_type(
            argument, parameter_pattern);
        if (!arguments[template_index]) {
            rcc_error(argument->loc,
                      "cannot deduce generic lambda parameter type");
            return false;
        }
    }
    {
#if defined(__GNUC__) || defined(__clang__)
        if (!cxx_template_instantiate_with_values) {
            rcc_error(call->loc,
                      "generic lambda template instantiation is unavailable");
            return false;
        }
#else
        rcc_error(call->loc,
                  "generic lambda template instantiation is unavailable");
        return false;
#endif
        if (has_pack) {
            tmpl->pending_pack_args = has_value_pack ? NULL : pack_arguments;
            tmpl->pending_pack_values = has_value_pack ? pack_values : NULL;
            tmpl->pending_pack_value_present = has_value_pack
                ? pack_value_present : NULL;
            tmpl->pending_pack_count = pack_count;
        }
        Decl* instance = (Decl*)cxx_template_instantiate_with_values(
            tmpl, arguments, values, value_present, tmpl->param_count);
        if (has_pack) {
            tmpl->pending_pack_args = NULL;
            tmpl->pending_pack_values = NULL;
            tmpl->pending_pack_value_present = NULL;
            tmpl->pending_pack_count = -1;
        }
        if (!instance || instance->kind != DECL_FUNC) {
            rcc_error(call->loc, "could not instantiate generic lambda");
            return false;
        }
        if (current_ast) {
            bool present = false;
            for (DeclList* item = current_ast->decls; item; item = item->next) {
                if (item->decl == instance) {
                    present = true;
                    break;
                }
            }
            if (!present) ast_add_decl(current_ast, instance);
        }
        if (lambda_variable) {
            Expr* initializer = lambda_variable->var_init;
            lambda_variable->var_cxx_lambda_specialized = true;
            /* The generic closure has no runtime state in this bounded
             * captureless profile.  Point its stored function value at the
             * first concrete specialization so the ordinary function-pointer
             * initializer remains a real symbol instead of an unresolved
             * dependent template name. */
            if (initializer && initializer->kind == EXPR_IDENT &&
                initializer->cxx_lambda_template == tmpl) {
                initializer->ident_decl = instance;
                initializer->type = instance->type;
                initializer->cxx_lambda_template = NULL;
            }
        }
        function_expression->ident_decl = instance;
        function_expression->type = instance->type;
        function_expression->cxx_lambda_template = NULL;
    }
    return true;
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

static bool sema_noexcept_expr(Expr* expression);

static bool sema_noexcept_expr_list(ExprList* list) {
    for (; list; list = list->next) {
        if (!sema_noexcept_expr(list->expr)) return false;
    }
    return true;
}

/* Determine whether an already semantically analyzed expression is
 * potentially-throwing.  This is deliberately conservative: only direct
 * calls carrying a real `noexcept` declaration are proven non-throwing;
 * function pointers, unknown calls, allocation, and unsupported extension
 * nodes remain potentially-throwing instead of receiving a guessed value. */
static bool sema_noexcept_expr(Expr* expression) {
    if (!expression) return false;
    switch (expression->kind) {
        case EXPR_INT_LIT:
        case EXPR_FLOAT_LIT:
        case EXPR_CHAR_LIT:
        case EXPR_STRING_LIT:
        case EXPR_IDENT:
        case EXPR_CXX_THIS:
            return true;
        case EXPR_NOEXCEPT:
            return expression->cxx_noexcept_value_valid;
        case EXPR_CXX_TYPEID:
            return true;
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
            return sema_noexcept_expr(expression->unary_operand);
        case EXPR_CAST:
            if (expression->cxx_cast_kind == CXX_CAST_DYNAMIC &&
                expression->cast_type && expression->cast_type->is_reference) {
                return false;
            }
            if (expression->cast_type &&
                (expression->cast_type->kind == TYPE_STRUCT ||
                 expression->cast_type->kind == TYPE_UNION) &&
                expression->cast_type->cxx_nontrivial) {
                return false;
            }
            return sema_noexcept_expr(expression->cast_expr);
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
        case EXPR_CXX_MEMBER_PTR_DOT:
        case EXPR_CXX_MEMBER_PTR_ARROW:
            return sema_noexcept_expr(expression->binary_lhs) &&
                   sema_noexcept_expr(expression->binary_rhs);
        case EXPR_COND:
            return sema_noexcept_expr(expression->cond_test) &&
                   sema_noexcept_expr(expression->cond_then) &&
                   sema_noexcept_expr(expression->cond_else);
        case EXPR_CALL:
            return expression->cxx_call_is_noexcept &&
                   sema_noexcept_expr(expression->call_func) &&
                   sema_noexcept_expr_list(expression->call_args);
        case EXPR_INDEX:
            return sema_noexcept_expr(expression->index_base) &&
                   sema_noexcept_expr(expression->index_expr);
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            return sema_noexcept_expr(expression->member_base);
        case EXPR_COMPOUND:
            if (expression->compound_type &&
                expression->compound_type->cxx_nontrivial) return false;
            return sema_noexcept_expr_list(expression->compound_init);
        default:
            return false;
    }
}

static Type* sema_expr(Expr* expr) {
    if (!expr) return NULL;

    if (rcc_parser_is_cxx_mode() &&
        expr->kind == EXPR_PTR_MEMBER && expr->member_base) {
        Type* object_type = sema_expr(expr->member_base);
        if (sema_rewrite_cxx_arrow_operator(expr, object_type)) {
            return sema_expr(expr);
        }
    }

    if (rcc_parser_is_cxx_mode() &&
        (expr->kind == EXPR_NEG || expr->kind == EXPR_BITNOT ||
         expr->kind == EXPR_NOT || expr->kind == EXPR_PREINC ||
         expr->kind == EXPR_PREDEC || expr->kind == EXPR_POSTINC ||
         expr->kind == EXPR_POSTDEC || expr->kind == EXPR_DEREF) &&
        expr->unary_operand) {
        Type* operand_type = sema_expr(expr->unary_operand);
        if (sema_rewrite_cxx_unary_operator(expr, operand_type)) {
            return sema_expr(expr);
        }
    }

    if (rcc_parser_is_cxx_mode() &&
        expr->kind == EXPR_INDEX && expr->index_base) {
        Type* object_type = sema_expr(expr->index_base);
        if (sema_rewrite_cxx_subscript_operator(expr, object_type)) {
            return sema_expr(expr);
        }
    }

    if (rcc_parser_is_cxx_mode() &&
        expr->kind == EXPR_CALL && expr->call_func &&
        !expr->call_is_new && !expr->call_is_delete) {
        Type* object_type;
        if (!sema_instantiate_cxx_lambda(expr)) {
            expr->type = type_int;
            return expr->type;
        }
        sema_expand_cxx_lambda_captures(expr);
        if (expr->call_func->kind == EXPR_MEMBER ||
            expr->call_func->kind == EXPR_PTR_MEMBER) {
            object_type = sema_expr(expr->call_func->member_base);
            if (expr->call_func->kind == EXPR_PTR_MEMBER) {
                object_type = get_pointer_base(object_type);
            }
        } else if (expr->call_func->kind == EXPR_IDENT) {
            Symbol* symbol = expr->call_func->ident_decl
                ? NULL : sema_cxx_lookup_name(expr->call_func->ident_name,
                                               expr->call_func->loc);
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
        if ((expr->call_func->kind == EXPR_MEMBER ||
             expr->call_func->kind == EXPR_PTR_MEMBER) &&
            expr->call_func->member_base &&
            expr->call_func->member_base->kind == EXPR_CXX_TYPEID &&
            object_type == rcc_cxx_type_info_type() &&
            (strcmp(expr->call_func->member_name, "hash_code") == 0 ||
             strcmp(expr->call_func->member_name, "name") == 0 ||
             strcmp(expr->call_func->member_name, "before") == 0)) {
            if (strcmp(expr->call_func->member_name, "before") == 0) {
                Type* argument_type = NULL;
                if (expr->call_args && !expr->call_args->next &&
                    expr->call_args->expr) {
                    argument_type = sema_expr(expr->call_args->expr);
                }
                if (!argument_type || argument_type != rcc_cxx_type_info_type()) {
                    rcc_error(expr->loc,
                              "type_info::before() requires one type_info argument");
                } else {
                    expr->cxx_typeinfo_before = true;
                }
                expr->type = type_bool;
                return expr->type;
            }
            if (expr->call_args) {
                rcc_error(expr->loc,
                          strcmp(expr->call_func->member_name, "hash_code") == 0
                              ? "type_info::hash_code() takes no arguments"
                              : strcmp(expr->call_func->member_name, "name") == 0
                                  ? "type_info::name() takes no arguments"
                                  : "type_info::before() requires one type_info argument");
            } else {
                if (strcmp(expr->call_func->member_name, "hash_code") == 0) {
                    expr->cxx_typeinfo_hash_code = true;
                    expr->type = g_opts.target_arch == ARCH_X64
                        ? type_ulong : type_uint;
                } else if (strcmp(expr->call_func->member_name, "name") == 0) {
                    expr->cxx_typeinfo_name = true;
                    expr->type = type_ptr(type_char);
                } else {
                    rcc_error(expr->loc,
                              "type_info::before() requires one type_info argument");
                    expr->type = type_bool;
                }
                return expr->type;
            }
        }
        if (sema_rewrite_cxx_call_operator(expr, object_type)) {
            return sema_expr(expr);
        }
    }

    if (rcc_parser_is_cxx_mode() &&
        expr->kind >= EXPR_ASSIGN && expr->kind <= EXPR_RSHIFT_ASSIGN &&
        expr->binary_lhs && expr->binary_rhs) {
        Type* left_type = sema_expr(expr->binary_lhs);
        left_type = sema_cxx_object_type(left_type);
        if (sema_rewrite_cxx_assignment_operator(expr, left_type)) {
            return sema_expr(expr);
        }
    }

    if (rcc_parser_is_cxx_mode() &&
        expr->kind >= EXPR_ADD && expr->kind <= EXPR_OR &&
        expr->binary_lhs && expr->binary_rhs) {
        Type* left_type = sema_expr(expr->binary_lhs);
        if (sema_rewrite_cxx_binary_operator(expr, left_type)) {
            return sema_expr(expr);
        }
        if (sema_rewrite_cxx_spaceship_comparison(expr, left_type)) {
            return sema_expr(expr);
        }
    }

    switch (expr->kind) {
        case EXPR_INT_LIT:
            if (!expr->type) expr->type = type_int;
            if (rcc_parser_is_cxx_mode() &&
                expr->cxx_member_pointer_form &&
                !sema_cxx_member_pointer_form_accessible(expr)) {
                rcc_error(expr->loc,
                          "data-member pointer formation is not accessible in this context");
                expr->cxx_member_pointer_form = false;
            }
            break;

        case EXPR_FLOAT_LIT:
            if (!expr->type || !type_is_floating(expr->type)) {
                expr->type = type_double;
            }
            break;

        case EXPR_CHAR_LIT:
            expr->type = expr->is_cxx_utf8_literal ? type_uchar : type_int;
            break;

        case EXPR_STRING_LIT: {
            Type* element_type = expr->is_cxx_utf8_literal
                ? type_uchar : type_char;
            if (rcc_parser_is_cxx_mode()) {
                Type* qualified = ast_arena_alloc(sizeof(*qualified));
                *qualified = *element_type;
                qualified->is_const = true;
                element_type = qualified;
            }
            expr->type = type_array(element_type,
                                    (int)expr->str_length + 1);
            break;
        }

        case EXPR_IDENT: {
            if (expr->cxx_pack_expansion) {
                rcc_error(expr->loc,
                          "C++ pack expansion was not expanded in a call argument list");
            }
            if (expr->cxx_lambda_captures) {
                rcc_error(expr->loc,
                          "capturing lambda must be immediately invoked");
            }
            /* A dependent member base such as the `T` in `T::value` is
             * deliberately pre-typed by the C++ parser.  It is not an
             * object identifier and must wait for function-template
             * substitution before ordinary lookup is attempted. */
            if (expr->type && expr->type->cxx_dependent) {
                break;
            }
            if (expr->ident_decl && expr->ident_decl->kind == DECL_FUNC) {
                sema_cxx_check_qualified_member_access(
                    expr->ident_name, expr->ident_decl, expr->loc);
                expr->type = expr->ident_decl->type;
                break;
            }
            if (expr->ident_decl &&
                (expr->ident_decl->kind == DECL_VAR ||
                 expr->ident_decl->kind == DECL_PARAM)) {
                sema_cxx_check_qualified_member_access(
                    expr->ident_name, expr->ident_decl, expr->loc);
                if (expr->ident_decl->kind == DECL_VAR &&
                    expr->ident_decl->var_is_deprecated) {
                    if (expr->ident_decl->var_deprecated_message &&
                        *expr->ident_decl->var_deprecated_message) {
                        rcc_warning(expr->loc,
                                    "use of deprecated variable '%s': %s",
                                    expr->ident_decl->name
                                        ? expr->ident_decl->name : "<variable>",
                                    expr->ident_decl->var_deprecated_message);
                    } else {
                        rcc_warning(expr->loc,
                                    "use of deprecated variable '%s'",
                                    expr->ident_decl->name
                                        ? expr->ident_decl->name : "<variable>");
                    }
                }
                expr->type = expr->ident_decl->type &&
                    expr->ident_decl->type->is_reference
                    ? expr->ident_decl->type->base
                    : expr->ident_decl->type;
                if (expr->ident_decl->kind == DECL_VAR) {
                    expr->cxx_lambda_template =
                        expr->ident_decl->var_cxx_lambda_template;
                }
                break;
            }
            Symbol* sym = sema_cxx_lookup_name(expr->ident_name, expr->loc);
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
                sema_cxx_check_qualified_member_access(
                    expr->ident_name, sym->decl, expr->loc);
                expr->ident_decl = sym->decl;
                expr->type = sym->type && sym->type->is_reference
                    ? sym->type->base : sym->type;
                if (sym->decl && sym->decl->kind == DECL_VAR &&
                    sym->decl->var_is_deprecated) {
                    if (sym->decl->var_deprecated_message &&
                        *sym->decl->var_deprecated_message) {
                        rcc_warning(expr->loc,
                                    "use of deprecated variable '%s': %s",
                                    sym->decl->name ? sym->decl->name
                                                    : "<variable>",
                                    sym->decl->var_deprecated_message);
                    } else {
                        rcc_warning(expr->loc,
                                    "use of deprecated variable '%s'",
                                    sym->decl->name ? sym->decl->name
                                                    : "<variable>");
                    }
                }
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
            Type* addressed_type = t;
            bool materialized_implicit_object =
                expr->cxx_implicit_object_address && t &&
                (t->kind == TYPE_STRUCT || t->kind == TYPE_UNION);
            if (!is_lvalue(expr->unary_operand) &&
                !materialized_implicit_object) {
                rcc_error(expr->loc, "cannot take address of rvalue");
            }
            if (expr->unary_operand &&
                (expr->unary_operand->kind == EXPR_MEMBER ||
                 expr->unary_operand->kind == EXPR_PTR_MEMBER) &&
                expr->unary_operand->member_field &&
                expr->unary_operand->member_field->is_bitfield) {
                rcc_error(expr->loc, "cannot take address of a bit-field");
            }
            /* A reference is an expression alias, not an addressable object
             * layer.  In particular, &function_returning_reference() has
             * pointer-to-referred-type, never pointer-to-reference type. */
            if (rcc_parser_is_cxx_mode() && addressed_type &&
                addressed_type->kind == TYPE_PTR &&
                addressed_type->is_reference) {
                addressed_type = addressed_type->base;
            }
            expr->type = type_ptr(addressed_type);
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
            if (!is_modifiable_builtin_assignment_target(
                    expr->unary_operand)) {
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
            if (expr->sizeof_pack_name) {
                rcc_error(expr->loc,
                          "sizeof... pack was not substituted during template instantiation");
                expr->type = type_uint;
            } else if (expr->sizeof_type) {
                sema_validate_array_parameter_type(expr->sizeof_type,
                                                   expr->loc, false);
                sema_validate_restrict_type(expr->sizeof_type, expr->loc);
                sema_vla_bounds(expr->sizeof_type, expr->loc);
                expr->type = type_uint;
            } else {
                sema_expr(expr->unary_operand);
                expr->type = type_uint;
            }
            break;
        }

        case EXPR_CXX_FOLD:
            rcc_error(expr->loc,
                      "C++ fold expression was not expanded during template instantiation");
            expr->type = type_int;
            break;

        case EXPR_CXX_REQUIRES: {
            bool valid = true;
            symtab_enter_scope(g_symtab);
            for (DeclList* parameter = expr->cxx_requires_params;
                 parameter; parameter = parameter->next) {
                if (!parameter->decl || !parameter->decl->name ||
                    !parameter->decl->type) {
                    rcc_error(expr->loc,
                              "requires-expression parameter is incomplete");
                    valid = false;
                    continue;
                }
                (void)symtab_define(g_symtab, parameter->decl->name,
                                    SYM_PARAM, parameter->decl->type,
                                    parameter->decl->loc);
            }
            for (TypeList* requirement = expr->cxx_requires_types;
                 requirement; requirement = requirement->next) {
                int suppressed_before = g_suppressed_error_count;
                bool suppress_before = g_suppress_errors;
                CxxTypeAlias* alias = NULL;
                g_suppress_errors = true;
                if (!requirement->type) {
                    rcc_error(requirement->loc,
                              "requires-expression type requirement is unresolved");
                } else if (requirement->type->cxx_dependent_member_name) {
                    if (requirement->type->cxx_class) {
                        for (CxxTypeAlias* candidate =
                                 requirement->type->cxx_class->type_aliases;
                             candidate; candidate = candidate->next) {
                            if (strcmp(candidate->name,
                                       requirement->type
                                           ->cxx_dependent_member_name) == 0) {
                                alias = candidate;
                                break;
                            }
                        }
                    }
                    if (!alias || alias->access != ACCESS_PUBLIC) {
                        rcc_error(requirement->loc,
                                  "requires-expression type requirement names an unresolved or non-public type");
                    }
                } else if (requirement->type->cxx_dependent) {
                    rcc_error(requirement->loc,
                              "requires-expression type requirement remains dependent");
                }
                g_suppress_errors = suppress_before;
                if (g_suppressed_error_count != suppressed_before) {
                    valid = false;
                }
            }
            for (ExprList* requirement = expr->cxx_requires_items;
                 requirement; requirement = requirement->next) {
                int suppressed_before = g_suppressed_error_count;
                bool suppress_before = g_suppress_errors;
                g_suppress_errors = true;
                sema_expr(requirement->expr);
                g_suppress_errors = suppress_before;
                if (g_suppressed_error_count != suppressed_before) {
                    valid = false;
                }
            }
            for (ExprList* requirement = expr->cxx_requires_nested;
                 requirement; requirement = requirement->next) {
                int suppressed_before = g_suppressed_error_count;
                bool suppress_before = g_suppress_errors;
                int64_t value = 0;
                g_suppress_errors = true;
                sema_expr(requirement->expr);
                g_suppress_errors = suppress_before;
                if (g_suppressed_error_count != suppressed_before ||
                    !expr_eval_integer_constant(requirement->expr, &value) ||
                    value == 0) {
                    valid = false;
                }
            }
            for (CxxCompoundRequirement* requirement =
                     expr->cxx_requires_compound;
                 requirement; requirement = requirement->next) {
                int suppressed_before = g_suppressed_error_count;
                bool suppress_before = g_suppress_errors;
                Type* actual_type;
                g_suppress_errors = true;
                actual_type = sema_expr(requirement->expr);
                if (requirement->is_noexcept &&
                    !sema_noexcept_expr(requirement->expr)) {
                    rcc_error(requirement->loc,
                              "requires-expression compound requirement is not noexcept");
                }
                if (requirement->return_type &&
                    ((!requirement->return_type_convertible &&
                      !type_is_compatible(actual_type,
                                          requirement->return_type)) ||
                     (requirement->return_type_convertible &&
                      cxx_conversion_rank(requirement->expr,
                                          requirement->return_type) < 0))) {
                    valid = false;
                }
                g_suppress_errors = suppress_before;
                if (g_suppressed_error_count != suppressed_before) {
                    valid = false;
                }
            }
            symtab_leave_scope(g_symtab);
            expr->kind = EXPR_INT_LIT;
            expr->int_val = valid ? 1 : 0;
            expr->type = type_bool;
            break;
        }

        case EXPR_CAST: {
            Type* source = sema_expr(expr->cast_expr);
            sema_validate_array_parameter_type(expr->cast_type,
                                               expr->loc, false);
            sema_validate_restrict_type(expr->cast_type, expr->loc);
            expr->type = expr->cast_type;
            expr->cxx_pointer_adjustment_valid = false;
            expr->cxx_member_pointer_adjustment_valid = false;
            if (source && expr->cast_type &&
                ((source->kind == TYPE_PTR &&
                  source->cxx_is_member_pointer) ||
                 (expr->cast_type->kind == TYPE_PTR &&
                  expr->cast_type->cxx_is_member_pointer))) {
                Type* target = expr->cast_type;
                bool source_member = source->kind == TYPE_PTR &&
                                     source->cxx_is_member_pointer &&
                                     !source->is_reference;
                bool target_member = target->kind == TYPE_PTR &&
                                     target->cxx_is_member_pointer &&
                                     !target->is_reference;
                if (expr->cxx_cast_kind != CXX_CAST_STATIC ||
                    !target_member) {
                    rcc_error(expr->loc,
                              "this explicit pointer-to-member conversion is unsupported");
                    break;
                }
                if (!source_member) {
                    Type* converted = implicit_cast(expr->cast_expr, target);
                    if (!converted) {
                        rcc_error(expr->loc,
                                  "static_cast requires a null pointer-to-member constant");
                    } else {
                        expr->is_cxx_nullptr =
                            expr->cast_expr->is_cxx_nullptr;
                    }
                    break;
                }
                if (!source->base || !target->base ||
                    source->base->kind == TYPE_FUNC ||
                    target->base->kind == TYPE_FUNC ||
                    !type_is_compatible(source->base, target->base) ||
                    !sema_pointee_qualification_preserved(
                        source->base, target->base)) {
                    rcc_error(expr->loc,
                              "static_cast requires compatible data-member pointer types");
                    break;
                }
                if (expr->cast_expr->is_cxx_nullptr) {
                    expr->is_cxx_nullptr = true;
                    break;
                }
                if (type_is_compatible(source->cxx_member_pointer_owner,
                                       target->cxx_member_pointer_owner)) {
                    break;
                }
                {
                    int base_adjustment = 0;
                    int paths;
                    if (!source->cxx_member_pointer_owner ||
                        !target->cxx_member_pointer_owner) {
                        paths = 0;
                    } else {
                        paths = sema_cxx_unique_public_nonvirtual_member_owner_path(
                            target->cxx_member_pointer_owner,
                            source->cxx_member_pointer_owner,
                            &base_adjustment) ? 1 : 0;
                        if (paths == 1) {
                            expr->cxx_member_pointer_adjustment =
                                base_adjustment;
                        } else {
                            paths = sema_cxx_unique_public_nonvirtual_member_owner_path(
                                source->cxx_member_pointer_owner,
                                target->cxx_member_pointer_owner,
                                &base_adjustment) ? 1 : 0;
                            if (paths == 1) {
                                expr->cxx_member_pointer_adjustment =
                                    -base_adjustment;
                            }
                        }
                    }
                    if (paths != 1) {
                        rcc_error(expr->loc,
                                  "static_cast requires one public non-virtual owner base path");
                    } else {
                        expr->cxx_member_pointer_adjustment_valid = true;
                    }
                }
                break;
            }
            if (rcc_parser_is_cxx_mode() && source && expr->cast_type &&
                expr->cxx_cast_kind == CXX_CAST_STATIC &&
                expr->cast_type->kind == TYPE_PTR &&
                expr->cast_type->is_reference &&
                expr->cast_type->is_rvalue_reference &&
                expr->cast_type->base) {
                Type* source_object = source->is_reference
                    ? source->base : source;
                Type* target_object = expr->cast_type->base;
                if (source_object &&
                    (source_object->kind == TYPE_STRUCT ||
                     source_object->kind == TYPE_UNION) &&
                    (target_object->kind == TYPE_STRUCT ||
                     target_object->kind == TYPE_UNION) &&
                    !type_is_compatible(source_object, target_object) &&
                    !sema_cxx_unique_public_base(
                        source_object, target_object, NULL) &&
                    !sema_cxx_unique_public_base(
                        target_object, source_object, NULL)) {
                    int virtual_index;
                    int nested_adjustment;
                    if (!sema_cxx_virtual_object_conversion(
                            source_object, target_object, &virtual_index,
                            &nested_adjustment)) {
                        Type* converted = implicit_cast(
                            expr->cast_expr, expr->cast_type);
                        if (converted) source = expr->cast_expr->type;
                    }
                }
            }
            if (expr->cxx_cast_kind == CXX_CAST_DYNAMIC) {
                int adjustment = 0;
                bool supported = false;
                bool source_polymorphic = false;
                if (source && expr->cast_type &&
                    source->kind == TYPE_PTR &&
                    expr->cast_type->kind == TYPE_PTR &&
                    !source->is_reference &&
                    !expr->cast_type->is_reference && source->base &&
                    expr->cast_type->base) {
                    supported = sema_cxx_public_base(
                        source->base, expr->cast_type->base,
                        &adjustment, 0);
                    source_polymorphic = sema_cxx_is_polymorphic(source->base);
                    if (sema_cxx_virtual_base_conversion(
                            source, expr->cast_type, NULL, NULL) &&
                        source_polymorphic &&
                        expr->cast_type->base->cxx_class &&
                        expr->cast_type->base->cxx_class->type &&
                        expr->cast_type->base->cxx_class->type
                            ->cxx_typeinfo_symbol) {
                        /* A virtual-base adjustment depends on the
                         * complete object.  Use the same RTTI search as a
                         * downcast/cross-cast instead of baking in the
                         * layout of the static source type. */
                        supported = true;
                        expr->cxx_dynamic_cast_runtime = true;
                        expr->cxx_dynamic_cast_typeinfo_symbol =
                            expr->cast_type->base->cxx_class->type
                                ->cxx_typeinfo_symbol;
                    }
                    if (!supported && source_polymorphic &&
                        expr->cast_type->base->cxx_class &&
                        expr->cast_type->base->cxx_class->type &&
                        expr->cast_type->base->cxx_class->type->is_complete &&
                        expr->cast_type->base->cxx_class->type->cxx_typeinfo_symbol) {
                        /* The source is polymorphic and the target is a
                         * complete class.  Defer the relationship search to
                         * the complete-object RTTI table emitted with each
                         * vtable; this covers public downcasts, cross-casts,
                         * virtual bases, and further-derived objects. */
                        supported = true;
                        expr->cxx_dynamic_cast_runtime = true;
                        expr->cxx_dynamic_cast_typeinfo_symbol =
                            expr->cast_type->base->cxx_class->type->cxx_typeinfo_symbol;
                    }
                } else if (source && expr->cast_type &&
                           expr->cast_type->kind == TYPE_PTR &&
                           expr->cast_type->is_reference &&
                           source->kind != TYPE_PTR &&
                           expr->cast_type->base) {
                    supported = sema_cxx_public_base(
                        source, expr->cast_type->base, &adjustment, 0);
                    source_polymorphic = sema_cxx_is_polymorphic(source);
                    if (!supported && source_polymorphic &&
                        expr->cast_type->base->kind == TYPE_STRUCT &&
                        expr->cast_type->base->cxx_class &&
                        expr->cast_type->base->cxx_class->type &&
                        expr->cast_type->base->cxx_class->type->is_complete &&
                        expr->cast_type->base->cxx_class->type
                            ->cxx_typeinfo_symbol) {
                        /* Reference dynamic_cast has the same complete-object
                         * relationship search as the pointer form.  A
                         * failed search is not a null reference: codegen
                         * transfers through the RinOS exception ABI. */
                        supported = true;
                        expr->cxx_dynamic_cast_runtime = true;
                        expr->cxx_dynamic_cast_typeinfo_symbol =
                            expr->cast_type->base->cxx_class->type
                                ->cxx_typeinfo_symbol;
                    }
                }
                if (supported && !source_polymorphic) supported = false;
                if (!supported) {
                    rcc_error(expr->loc,
                              "dynamic_cast requires a polymorphic source and a supported public base conversion");
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
                if (sema_cxx_set_pointer_conversion(
                        expr, source, expr->cast_type, &adjustment)) {
                    if (!expr->cxx_virtual_base_adjustment) {
                        expr->cxx_pointer_adjustment_valid = adjustment != 0;
                        expr->cxx_pointer_adjustment = adjustment;
                    }
                }
            }
            if (rcc_parser_is_cxx_mode() &&
                expr->cxx_cast_kind == CXX_CAST_STATIC && source &&
                expr->cast_type && expr->cast_type->kind == TYPE_PTR &&
                expr->cast_type->is_reference &&
                expr->cast_type->base) {
                Type* source_object = source->is_reference
                    ? source->base : source;
                Type* target_object = expr->cast_type->base;
                if (source_object &&
                    (source_object->kind == TYPE_STRUCT ||
                     source_object->kind == TYPE_UNION) &&
                    (target_object->kind == TYPE_STRUCT ||
                     target_object->kind == TYPE_UNION)) {
                    int reference_adjustment = 0;
                    int virtual_index;
                    int nested_adjustment;
                    expr->cxx_virtual_base_adjustment = false;
                    expr->cxx_pointer_adjustment_valid = false;
                    if (sema_cxx_virtual_object_conversion(
                            source_object, target_object, &virtual_index,
                            &nested_adjustment)) {
                        expr->cxx_virtual_base_adjustment = true;
                        expr->cxx_virtual_base_index = virtual_index;
                        expr->cxx_virtual_base_nested_adjustment =
                            nested_adjustment;
                        expr->cxx_virtual_base_source_class =
                            source_object->cxx_class;
                        expr->cxx_virtual_base_pointer_offset =
                            source_object->cxx_class
                                ? source_object->cxx_class
                                      ->virtual_base_pointer_offset
                                : -1;
                    } else if (sema_cxx_unique_public_base(
                                   source_object, target_object,
                                   &reference_adjustment)) {
                        expr->cxx_pointer_adjustment_valid =
                            reference_adjustment != 0;
                        expr->cxx_pointer_adjustment =
                            reference_adjustment;
                    }
                }
            }
            break;
        }

        case EXPR_ALIGNOF:
            expr->type = type_uint;
            break;

        case EXPR_CXX_TYPEID: {
            Type* operand_type = expr->cxx_typeid_is_type
                ? expr->cxx_typeid_operand_type
                : sema_expr(expr->cxx_typeid_operand);
            expr->type = rcc_cxx_type_info_type();
            if (!operand_type) {
                rcc_error(expr->loc, "typeid operand has no type");
                break;
            }
            if (!expr->cxx_typeid_is_type &&
                sema_cxx_is_polymorphic(operand_type)) {
                if (!is_lvalue(expr->cxx_typeid_operand)) {
                    rcc_error(expr->loc,
                              "typeid of a polymorphic expression requires a glvalue");
                    break;
                }
                expr->cxx_typeid_dynamic = true;
                break;
            }
            expr->cxx_typeid_symbol = sema_cxx_typeinfo_symbol(
                operand_type, expr->loc);
            break;
        }

        case EXPR_NOEXCEPT:
            sema_expr(expr->unary_operand);
            expr->cxx_noexcept_value =
                sema_noexcept_expr(expr->unary_operand);
            expr->cxx_noexcept_value_valid = true;
            expr->type = type_bool;
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
            } else if (sema_is_arithmetic_type(lt) &&
                       sema_is_arithmetic_type(rt)) {
                expr->type = sema_common_arithmetic_type(lt, rt);
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
            } else if (!sema_is_arithmetic_type(lt) ||
                       !sema_is_arithmetic_type(rt)) {
                rcc_error(expr->loc, "invalid operands to binary operator");
            }
            expr->type = sema_common_arithmetic_type(lt, rt);
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
                    ExprList* constructor_arguments =
                        expr->compound_value_init ? NULL : expr->compound_init;
                    expr->compound_constructor =
                        sema_select_cxx_new_constructor(
                            expr->compound_type, &constructor_arguments,
                            expr->loc);
                    if (constructor_arguments != expr->compound_init) {
                        expr->compound_init = constructor_arguments;
                        expr->compound_value_init = constructor_arguments == NULL;
                    }
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
            bool typeinfo_equality = equality &&
                left_value == rcc_cxx_type_info_type() &&
                right_value == rcc_cxx_type_info_type();
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
            bool left_member_pointer = left_value &&
                left_value->kind == TYPE_PTR &&
                left_value->cxx_is_member_pointer;
            bool right_member_pointer = right_value &&
                right_value->kind == TYPE_PTR &&
                right_value->cxx_is_member_pointer;
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
            bool member_pointer_comparison = equality &&
                left_member_pointer && right_member_pointer &&
                type_is_compatible(left_value, right_value);
            bool member_pointer_null = equality &&
                ((left_member_pointer && (right_nullptr || right_zero)) ||
                 (right_member_pointer && (left_nullptr || left_zero)));
            if (member_pointer_null) {
                if (left_member_pointer) {
                    (void)implicit_cast(expr->binary_rhs, left_value);
                } else {
                    (void)implicit_cast(expr->binary_lhs, right_value);
                }
            }
            if (!arithmetic && !pointers && !pointer_null &&
                !member_pointer_comparison && !member_pointer_null &&
                !typeinfo_equality &&
                !nullptr_equality) {
                rcc_error(expr->loc,
                          "comparison requires arithmetic or pointer operands");
            }
            expr->type = type_int;
            break;
        }

        case EXPR_SPACESHIP: {
            Type* left = generic_selection_type(sema_expr(expr->binary_lhs));
            Type* right = generic_selection_type(sema_expr(expr->binary_rhs));
            bool integral = left && right &&
                (type_is_integer(left) || left->kind == TYPE_ENUM) &&
                (type_is_integer(right) || right->kind == TYPE_ENUM);
            bool pointers = left && right && type_is_pointer(left) &&
                type_is_pointer(right) && type_is_compatible(left, right);
            if (!integral && !pointers) {
                rcc_error(expr->loc,
                          "RinOS C++20 built-in <=> requires integral, enum, "
                          "or compatible pointer operands");
            }
            /* RinOS has no standard comparison-category object ABI.  The
             * bounded scalar profile exposes the category's sign as int:
             * -1, 0, or 1.  Relational use remains directly representable
             * and unsupported category members cannot be mistaken for a
             * silently generated object. */
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
            if (!is_modifiable_builtin_assignment_target(
                    expr->binary_lhs)) {
                rcc_error(expr->loc,
                          "assignment requires modifiable lvalue");
            }
            if (sema_is_cxx_nullptr_expr(expr->binary_rhs)) {
                rcc_error(expr->loc,
                          "nullptr does not support arithmetic operators");
            } else if (!((type_is_pointer(lt) &&
                          is_pointer_arithmetic_type(lt) &&
                          type_is_integer(rt)) ||
                  (sema_is_arithmetic_type(lt) &&
                   sema_is_arithmetic_type(rt)))) {
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
            if (!is_modifiable_builtin_assignment_target(
                    expr->binary_lhs)) {
                rcc_error(expr->loc,
                          "assignment requires modifiable lvalue");
            }
            if (sema_is_cxx_nullptr_expr(expr->binary_rhs)) {
                rcc_error(expr->loc,
                          "nullptr does not support arithmetic operators");
            } else if (!sema_is_arithmetic_type(lt) ||
                       !sema_is_arithmetic_type(rt)) {
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
            if (!is_modifiable_builtin_assignment_target(
                    expr->binary_lhs)) {
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
            Type* rt;
            if (rcc_parser_is_cxx_mode()) {
                lt = sema_cxx_object_type(lt);
            }
            Expr* contextual = expr->binary_rhs &&
                    expr->binary_rhs->kind == EXPR_ADDR
                ? expr->binary_rhs->unary_operand : expr->binary_rhs;
            if (sema_cxx_select_function_pointer_overload(lt, contextual)) {
                if (contextual->type == type_int) {
                    expr->binary_rhs->type = type_int;
                } else {
                    (void)sema_expr(expr->binary_rhs);
                }
                rt = expr->binary_rhs->type;
            } else {
                rt = sema_expr(expr->binary_rhs);
            }
            if (!is_modifiable_builtin_assignment_target(
                    expr->binary_lhs) &&
                !is_modifiable_class_xvalue(expr->binary_lhs)) {
                rcc_error(expr->loc,
                          "assignment requires modifiable lvalue");
            } else {
                bool move_assignment =
                    sema_prepare_cxx_move_assignment(expr, lt);
                if (!move_assignment &&
                    !implicit_cast(expr->binary_rhs, lt)) {
                    if (sema_is_cxx_nullptr_expr(expr->binary_rhs) &&
                        !type_is_pointer(lt) && lt->kind != TYPE_NULLPTR) {
                        rcc_error(expr->loc,
                                  "nullptr can only be assigned to a pointer");
                    } else if (sema_is_scoped_enum(lt) ||
                               sema_is_scoped_enum(rt)) {
                        rcc_error(expr->loc,
                                  "incompatible scoped enum assignment");
                    } else {
                        rcc_error(expr->loc,
                                  "incompatible assignment");
                    }
                }
            }
            expr->type = lt;
            break;
        }

        case EXPR_COND: {
            bool then_lvalue;
            bool else_lvalue;
            bool then_xvalue;
            bool else_xvalue;
            expr->cxx_conditional_lvalue = false;
            expr->cxx_conditional_xvalue = false;
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
            then_lvalue = is_lvalue(expr->cond_then);
            else_lvalue = is_lvalue(expr->cond_else);
            then_xvalue = is_xvalue(expr->cond_then);
            else_xvalue = is_xvalue(expr->cond_else);
            if (rcc_parser_is_cxx_mode() &&
                sema_cxx_same_glvalue_type(tt, et, 0u)) {
                expr->cxx_conditional_lvalue =
                    then_lvalue && else_lvalue;
                expr->cxx_conditional_xvalue =
                    then_xvalue && else_xvalue;
                if (expr->cxx_conditional_lvalue ||
                    expr->cxx_conditional_xvalue) {
                    /* The conditional expression has the operands' exact
                     * type; arithmetic usual conversions do not apply to
                     * the C++ glvalue result. */
                    expr->type = tt;
                }
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
                    expr->call_delete_destructor =
                        sema_cxx_destructor_function(object_type);
                    if (!expr->call_delete_destructor &&
                        object_type->cleanup_function &&
                        object_type->cleanup_field) {
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
                    if (!expr->call_delete_destructor &&
                        !expr->call_delete_cleanup && !has_member_cleanup) {
                        rcc_error(expr->loc,
                                  "delete requires C++ destructor lowering for a non-trivial object");
                        return expr->type;
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
                if (sema_cxx_class_is_abstract(cls)) {
                    rcc_error(expr->loc,
                              "cannot allocate abstract class '%s'",
                              cls->name ? cls->name : "<anonymous>");
                }
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
                               object_type->kind == TYPE_UNION) {
                        for (argument = expr->call_new_args; argument;
                             argument = argument->next) {
                            if (cxx_conversion_rank(argument->expr,
                                                     object_type) < 0) {
                                rcc_error(argument->expr->loc,
                                          "array new initializer is incompatible with the aggregate element type");
                                return expr->type;
                            }
                        }
                    } else if (object_type->kind == TYPE_ARRAY) {
                        rcc_error(expr->loc,
                                  "array new currently requires scalar or class elements");
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
                if (object_type->cxx_class && argument_count == 1 &&
                    expr->call_new_args && expr->call_new_args->expr &&
                    expr->call_new_args->expr->type &&
                    type_is_compatible(object_type,
                                       expr->call_new_args->expr->type) &&
                    sema_cxx_trivially_copyable(object_type, 0)) {
                    /* A same-type argument selects the implicitly declared
                     * copy constructor even when the class also has ordinary
                     * converting constructors. */
                    expr->call_new_copy_init = true;
                }
                if (object_type->cxx_nontrivial &&
                    !expr->call_new_default_member_initializers &&
                    !expr->call_new_copy_init &&
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
                if (cls && cls->constructors && !expr->call_new_is_array &&
                    !expr->call_new_copy_init) {
                    constructor = sema_select_cxx_new_constructor(
                        object_type, &expr->call_new_args, expr->loc);
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
                           !expr->call_new_copy_init &&
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
                    if (!sema_cxx_member_accessible(
                            sema_cxx_method_owner(owner, method),
                            method->cxx_access)) {
                        rcc_error(expr->loc, "method '%s' is not accessible",
                                  member->member_name);
                    }
                    expr->call_method = method;
                    if (method->source_decl) {
                        /* Inline lowerings do not enter the ordinary
                         * function-declaration symbol path.  Resolve their
                         * stored conditional specification lazily, once
                         * semantic types and template substitutions exist. */
                        sema_resolve_function_noexcept(method->source_decl);
                        method->is_noexcept =
                            method->source_decl->func_is_noexcept;
                    }
                    expr->cxx_call_is_noexcept = method->is_noexcept;
                    if (method->source_decl &&
                        method->source_decl->func_is_deprecated) {
                        if (method->source_decl->func_deprecated_message &&
                            *method->source_decl->func_deprecated_message) {
                            rcc_warning(expr->loc,
                                        "use of deprecated function '%s': %s",
                                        method->source_decl->name
                                            ? method->source_decl->name
                                            : "<function>",
                                        method->source_decl
                                            ->func_deprecated_message);
                        } else {
                            rcc_warning(expr->loc,
                                        "use of deprecated function '%s'",
                                        method->source_decl->name
                                            ? method->source_decl->name
                                            : "<function>");
                        }
                    }
                    if (method->kind == TYPE_METHOD_FIELD_CLOSE) {
                        sema_prepare_cxx_close_call(expr, method,
                                                    member->member_base);
                    }
                    /* Reference results have the referred-to object type in
                     * value contexts.  The accessor metadata retains the
                     * glvalue category for address-of and decltype(auto). */
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
                    if (!sema_cxx_member_accessible(
                            sema_cxx_method_owner(owner, method),
                            method->cxx_access)) {
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
                        Expr* this_argument;
                        if (member->kind == EXPR_PTR_MEMBER) {
                            this_argument = member->member_base;
                        } else {
                            this_argument = expr_unary(
                                EXPR_ADDR, member->member_base, member->loc);
                            this_argument->cxx_implicit_object_address = true;
                        }
                        Type* expected_this =
                            method->function_decl->func_this_param->type;
                        /* Establish the source object's type before deciding
                         * whether an inherited-base conversion is needed. */
                        sema_expr(this_argument);
                        if (member->kind != EXPR_PTR_MEMBER) {
                            sema_prepare_class_prvalue_cleanup(
                                member->member_base,
                                &expr->cxx_temporary_owner,
                                &expr->cxx_temporary_cleanups,
                                false,
                                "class-prvalue member receiver cleanup is unsupported");
                            if (expr->cxx_temporary_owner) {
                                expr->cxx_temporary_source =
                                    member->member_base;
                            }
                        }
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
                !sema_cxx_lookup_name(expr->call_func->ident_name,
                                      expr->call_func->loc)) {
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
            if (sema_compiler_builtin_call(expr)) break;
            if (sema_atomic_builtin_call(expr)) break;
            if (expr->call_func->kind == EXPR_IDENT) {
                Symbol* overload = sema_cxx_lookup_name(
                    expr->call_func->ident_name, expr->call_func->loc);
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
            if (call_declaration && call_declaration->func_is_deprecated) {
                if (call_declaration->func_deprecated_message &&
                    *call_declaration->func_deprecated_message) {
                    rcc_warning(expr->loc,
                                "use of deprecated function '%s': %s",
                                call_declaration->name
                                    ? call_declaration->name : "<function>",
                                call_declaration->func_deprecated_message);
                } else {
                    rcc_warning(expr->loc,
                                "use of deprecated function '%s'",
                                call_declaration->name
                                    ? call_declaration->name : "<function>");
                }
            }
            expr->cxx_call_is_noexcept = call_declaration &&
                                         call_declaration->func_is_noexcept;

            parameter = ft->params;
            argument = expr->call_args;
            while (argument) {
                if (!arguments_analyzed) {
                    Expr* contextual = argument->expr &&
                            argument->expr->kind == EXPR_ADDR
                        ? argument->expr->unary_operand : argument->expr;
                    bool contextual_function_pointer = parameter &&
                        sema_cxx_select_function_pointer_overload(
                            parameter->type, contextual);
                    if (!contextual_function_pointer ||
                        contextual->type != type_int) {
                        sema_expr(argument->expr);
                    } else {
                        argument->expr->type = type_int;
                    }
                }
                if (parameter) {
                    if (!implicit_cast(argument->expr, parameter->type)) {
                        const char* function_name =
                            expr->call_func->kind == EXPR_IDENT
                                ? expr->call_func->ident_name : "<function>";
                        rcc_error(argument->expr->loc,
                                  "incompatible type for argument %d to '%s'",
                                  argument_index, function_name);
                    } else {
                        sema_prepare_reference_argument_cleanup(
                            argument, parameter->type);
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

        case EXPR_CXX_MEMBER_PTR_DOT:
        case EXPR_CXX_MEMBER_PTR_ARROW: {
            bool arrow = expr->kind == EXPR_CXX_MEMBER_PTR_ARROW;
            Type* object_type = sema_expr(expr->binary_lhs);
            Type* member_pointer_type = sema_expr(expr->binary_rhs);
            Type* member_type;
            expr->cxx_pointer_adjustment_valid = false;
            expr->cxx_virtual_base_adjustment = false;
            expr->cxx_virtual_base_member_access = false;
            expr->cxx_virtual_base_source_class = NULL;
            expr->cxx_virtual_base_index = -1;
            expr->cxx_virtual_base_pointer_offset = -1;
            expr->cxx_virtual_base_nested_adjustment = 0;
            if (object_type && object_type->kind == TYPE_PTR &&
                object_type->is_reference) {
                object_type = object_type->base;
            }
            if (arrow) {
                if (!object_type || object_type->kind != TYPE_PTR ||
                    object_type->cxx_is_member_pointer) {
                    rcc_error(expr->loc,
                              "pointer-to-member arrow requires an object pointer");
                    expr->type = type_int;
                    break;
                }
                object_type = object_type->base;
            }
            if (!member_pointer_type ||
                member_pointer_type->kind != TYPE_PTR ||
                !member_pointer_type->cxx_is_member_pointer) {
                rcc_error(expr->loc,
                          "pointer-to-member operator requires a data member pointer");
                expr->type = type_int;
                break;
            }
            if (!object_type ||
                (object_type->kind != TYPE_STRUCT &&
                 object_type->kind != TYPE_UNION)) {
                rcc_error(expr->loc,
                          "pointer-to-member operator requires a class object");
                expr->type = type_int;
                break;
            }
            if (!type_is_compatible(object_type,
                                    member_pointer_type
                                        ->cxx_member_pointer_owner)) {
                CxxClass* object_class = object_type->cxx_class;
                int virtual_index = -1;
                int virtual_nested_adjustment = 0;
                int object_adjustment = 0;
                int nonvirtual_paths =
                    sema_cxx_nonvirtual_public_base_paths(
                        object_type,
                        member_pointer_type->cxx_member_pointer_owner,
                        &object_adjustment, 0u);
                int virtual_paths = sema_cxx_public_virtual_member_base_paths(
                    object_type,
                    member_pointer_type->cxx_member_pointer_owner,
                    &virtual_index, &virtual_nested_adjustment);
                int all_paths = sema_cxx_nonvirtual_all_base_paths(
                    object_type,
                    member_pointer_type->cxx_member_pointer_owner, 0u) +
                    sema_cxx_all_virtual_member_base_paths(
                        object_type,
                        member_pointer_type->cxx_member_pointer_owner);
                if (all_paths != 1 ||
                    nonvirtual_paths + virtual_paths != 1) {
                    rcc_error(expr->loc,
                              "member-pointer application requires one public base subobject path");
                    expr->type = type_int;
                    break;
                }
                if (nonvirtual_paths == 1) {
                    expr->cxx_pointer_adjustment_valid = true;
                    expr->cxx_pointer_adjustment = object_adjustment;
                } else if (virtual_paths == 1 && object_class &&
                           virtual_index >= 0 &&
                           object_class->virtual_base_pointer_offset >= 0) {
                    expr->cxx_virtual_base_adjustment = true;
                    expr->cxx_virtual_base_member_access = true;
                    expr->cxx_virtual_base_source_class = object_class;
                    expr->cxx_virtual_base_index = virtual_index;
                    expr->cxx_virtual_base_pointer_offset =
                        object_class->virtual_base_pointer_offset;
                    expr->cxx_virtual_base_nested_adjustment =
                        virtual_nested_adjustment;
                } else {
                    rcc_error(expr->loc,
                              "member-pointer virtual-base path has incomplete vbtable metadata");
                    expr->type = type_int;
                    break;
                }
            }
            member_type = member_pointer_type->base;
            if (!member_type || member_type->kind == TYPE_FUNC) {
                rcc_error(expr->loc,
                          "pointer-to-member function application is unsupported");
                expr->type = type_int;
                break;
            }
            expr->type = member_type;
            if (object_type->is_const || object_type->is_volatile) {
                Type* qualified = ast_arena_alloc(sizeof(*qualified));
                *qualified = *member_type;
                qualified->is_const = qualified->is_const ||
                                      object_type->is_const;
                qualified->is_volatile = qualified->is_volatile ||
                                         object_type->is_volatile;
                expr->type = qualified;
            }
            expr->cxx_member_xvalue = !arrow &&
                !is_lvalue(expr->binary_lhs) &&
                !member_type->is_reference;
            break;
        }

        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER: {
            Type* bt;
            expr->cxx_member_xvalue = false;
            bool pretyped_class_base = expr->member_base &&
                expr->member_base->kind == EXPR_IDENT &&
                expr->member_base->type &&
                (expr->member_base->type->cxx_dependent ||
                 (expr->member_base->type->cxx_class &&
                  !expr->member_base->ident_decl));

            /* Function-template cloning substitutes the type carried by a
             * dependent base identifier, while its source spelling remains
             * `T`.  Use that substituted type directly; looking up `T` as a
             * value would reject a valid static member expression before the
             * member can be resolved. */
            bt = pretyped_class_base
                ? expr->member_base->type
                : sema_expr(expr->member_base);

            /* References are represented as pointer-shaped ABI carriers,
             * but member lookup operates on the referred object. */
            if (bt && bt->kind == TYPE_PTR && bt->is_reference) {
                bt = bt->base;
            }

            /* A cv-qualified class type may have been copied while its
             * inline class body was still being parsed.  Refresh its
             * structural metadata from the completed canonical class type,
             * while retaining the qualifiers carried by the expression. */
            if (bt && (bt->kind == TYPE_STRUCT || bt->kind == TYPE_UNION) &&
                !bt->fields && bt->cxx_class && bt->cxx_class->type &&
                bt->cxx_class->type->fields) {
                Type* canonical = bt->cxx_class->type;
                Type* qualified = ast_arena_alloc(sizeof(*qualified));
                *qualified = *canonical;
                qualified->is_const = canonical->is_const || bt->is_const;
                qualified->is_volatile = canonical->is_volatile ||
                                         bt->is_volatile;
                bt = qualified;
            }

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

            if (bt->cxx_dependent) {
                /* The member name is retained on the expression and will be
                 * resolved after the enclosing function template is cloned.
                 * The scalar type is sufficient for contextual-bool parsing,
                 * but no value or declaration is fabricated here. */
                expr->type = type_int;
                break;
            }

            /* Static C++ data members are represented in the class field
             * metadata and published as declarations, not as object-layout
             * TypeFields.  Resolve the declaration on the substituted class
             * and rewrite the expression to the same identifier form used by
             * direct static-member lookup, which also lets constexpr folding
             * consume its initializer. */
            if (expr->kind == EXPR_MEMBER && bt->cxx_class) {
                TypeParam* static_field;
                for (static_field = bt->cxx_class->fields;
                     static_field; static_field = static_field->next) {
                    if (!static_field->is_static || !static_field->name ||
                        strcmp(static_field->name, expr->member_name) != 0) {
                        continue;
                    }
                    for (struct CxxMember* member = bt->cxx_class->members;
                         member; member = member->next) {
                        Decl* declaration = member->decl;
                        const char* final_name;
                        if (!member->is_static || member->method ||
                            !declaration || declaration->kind != DECL_VAR ||
                            !declaration->name) {
                            continue;
                        }
                        final_name = strrchr(declaration->name, ':');
                        final_name = final_name ? final_name + 1
                                                : declaration->name;
                        if (strcmp(final_name, expr->member_name) != 0) {
                            continue;
                        }
                        if (!sema_cxx_member_accessible(
                                bt->cxx_class, static_field->cxx_access)) {
                            rcc_error(expr->loc,
                                      "member '%s' is not accessible",
                                      expr->member_name);
                        }
                        expr->member_base = NULL;
                        expr->member_name = NULL;
                        expr->member_field = NULL;
                        expr->kind = EXPR_IDENT;
                        expr->ident_name = declaration->name;
                        expr->ident_decl = declaration;
                        expr->type = declaration->type;
                        return expr->type;
                    }
                    break;
                }
            }

            /* Find member */
            TypeField* field = bt->fields;
            while (field) {
                if (strcmp(field->name, expr->member_name) == 0) {
                    expr->member_field = field;
                    expr->type = field->type;
                    if (rcc_parser_is_cxx_mode() &&
                        field->from_virtual_base) {
                        CxxClass* source_class = bt->cxx_class;
                        int virtual_index = -1;
                        if (source_class && field->virtual_base_owner &&
                            field->virtual_base_member_offset >= 0) {
                            for (int index = 0;
                                 index < source_class->virtual_base_count;
                                 ++index) {
                                if (source_class->virtual_bases[index].base ==
                                    field->virtual_base_owner) {
                                    virtual_index = index;
                                    break;
                                }
                            }
                        }
                        if (!source_class || virtual_index < 0 ||
                            source_class->virtual_base_pointer_offset < 0) {
                            rcc_error(expr->loc,
                                      "virtual-base member has no vbtable layout");
                        } else {
                            expr->cxx_virtual_base_adjustment = true;
                            expr->cxx_virtual_base_member_access = true;
                            expr->cxx_virtual_base_index = virtual_index;
                            expr->cxx_virtual_base_nested_adjustment =
                                field->virtual_base_member_offset;
                            expr->cxx_virtual_base_pointer_offset =
                                source_class->virtual_base_pointer_offset;
                            expr->cxx_virtual_base_source_class = source_class;
                        }
                    }
                    if (rcc_parser_is_cxx_mode() &&
                        expr->kind == EXPR_MEMBER && field->type &&
                        !field->type->is_reference &&
                        !is_lvalue(expr->member_base)) {
                        expr->cxx_member_xvalue = true;
                    }
                    if (field->is_deprecated) {
                        if (field->deprecated_message &&
                            *field->deprecated_message) {
                            rcc_warning(expr->loc,
                                        "use of deprecated member '%s': %s",
                                        field->name,
                                        field->deprecated_message);
                        } else {
                            rcc_warning(expr->loc,
                                        "use of deprecated member '%s'",
                                        field->name);
                        }
                    }
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
                    if (!sema_cxx_member_accessible(
                            bt->cxx_class, field->cxx_access)) {
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

static Decl* sema_nodiscard_call_decl(const Expr* expression) {
    if (!expression || expression->kind != EXPR_CALL) return NULL;
    if (expression->call_method) {
        if (expression->call_method->source_decl &&
            expression->call_method->source_decl->kind == DECL_FUNC) {
            return expression->call_method->source_decl;
        }
        if (expression->call_method->function_decl &&
            expression->call_method->function_decl->kind == DECL_FUNC) {
            return expression->call_method->function_decl;
        }
    }
    if (expression->call_func && expression->call_func->ident_decl &&
        expression->call_func->ident_decl->kind == DECL_FUNC) {
        return expression->call_func->ident_decl;
    }
    return NULL;
}

static void sema_warn_discarded_nodiscard(const Expr* expression) {
    Decl* declaration = sema_nodiscard_call_decl(expression);
    if (!declaration || !declaration->func_is_nodiscard ||
        !declaration->type || declaration->type->kind != TYPE_FUNC ||
        declaration->type->ret_type == type_void) {
        return;
    }
    rcc_warning(expression->loc,
                "ignoring return value of nodiscard function '%s'",
                declaration->name ? declaration->name : "<function>");
}

bool rcc_sema_cxx_requires_satisfied(Expr* expression) {
    int suppressed_before;
    bool suppress_before;
    bool valid;
    SymTab* temporary_symtab = NULL;
    if (!expression || expression->kind != EXPR_CXX_REQUIRES || !g_symtab) {
        if (!expression || expression->kind != EXPR_CXX_REQUIRES) return false;
        temporary_symtab = symtab_new();
        if (!temporary_symtab) return false;
        g_symtab = temporary_symtab;
    }
    suppressed_before = g_suppressed_error_count;
    suppress_before = g_suppress_errors;
    g_suppress_errors = true;
    (void)sema_expr(expression);
    g_suppress_errors = suppress_before;
    valid = expression->kind == EXPR_INT_LIT && expression->int_val != 0;
    if (g_suppressed_error_count != suppressed_before) valid = false;
    if (temporary_symtab) {
        symtab_free(temporary_symtab);
        g_symtab = NULL;
    }
    return valid;
}

static void sema_stmt(Stmt* stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case STMT_EXPR:
            if (stmt->expr) {
                sema_expr(stmt->expr);
                if (stmt->expr->kind == EXPR_CALL) {
                    sema_warn_discarded_nodiscard(stmt->expr);
                }
            }
            break;

        case STMT_BLOCK:
            if (!stmt->block_no_scope) symtab_enter_scope(g_symtab);
            for (StmtList* s = stmt->block_stmts; s; s = s->next) {
                sema_stmt(s->stmt);
            }
            if (!stmt->block_no_scope) symtab_leave_scope(g_symtab);
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
            stmt->return_reference_result =
                !current_func_auto_return_pending && stmt->return_val &&
                current_func_ret && current_func_ret->is_reference;
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
            sema_asm_stmt(stmt);
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
                /* The catch parameter is represented as the first declaration
                 * in handler->body and sema_stmt() has already attached its
                 * automatic object cleanups.  Only synthesize catch-object
                 * cleanup metadata when that declaration did not produce a
                 * plan; appending unconditionally registers each destructor
                 * twice (once through the declaration and once here). */
                if (handler->parameter &&
                    !handler->parameter->var_cleanups &&
                    !reference_type && match_type &&
                    !sema_cxx_trivially_copyable(match_type, 0) &&
                    sema_cxx_exception_object_copyable(match_type, 0)) {
                    int cleanup_budget = 4096;
                    Expr* object = expr_ident(handler->parameter->name,
                                              handler->parameter->loc);
                    object->ident_decl = handler->parameter;
                    object->type = handler->type;
                    if (!sema_cxx_append_object_cleanups(
                            handler->parameter, handler->type, object,
                            &handler->parameter->var_cleanups, 0,
                            &cleanup_budget, true, false)) {
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

/* C17 restrict qualifies a pointer itself, and the pointed-to type must be
 * an object or incomplete type.  The bounded frontend does not use restrict
 * as an optimizer promise, but it must still enforce the declaration
 * constraints instead of accepting a function pointer or a scalar-qualified
 * spelling and silently dropping it. */
static void sema_validate_restrict_type(Type* type, SourceLoc loc) {
    if (!type) return;
    if (type->is_restrict &&
        (type->kind != TYPE_PTR || type->is_reference)) {
        rcc_error(loc, "restrict qualifier is only valid on pointer types");
    }
    if (type->kind == TYPE_PTR) {
        if (type->is_restrict &&
            (!type->base || type->base->kind == TYPE_FUNC)) {
            rcc_error(loc,
                      "restrict-qualified pointer must point to an object or incomplete type");
        }
        sema_validate_restrict_type(type->base, loc);
    } else if (type->kind == TYPE_ARRAY) {
        sema_validate_restrict_type(type->base, loc);
    } else if (type->kind == TYPE_FUNC) {
        sema_validate_restrict_type(type->ret_type, loc);
        for (TypeParam* parameter = type->params; parameter;
             parameter = parameter->next) {
            sema_validate_restrict_type(parameter->type, loc);
        }
    }
}

static Expr* initializer_character_string(Type* type, Expr* initializer) {
    Expr* string = NULL;
    bool expects_utf8;
    if (!type || type->kind != TYPE_ARRAY || !type->base || !initializer ||
        type->base->kind != TYPE_CHAR) {
        return NULL;
    }
    if (initializer->kind == EXPR_STRING_LIT) {
        string = initializer;
    }
    if (initializer->kind == EXPR_COMPOUND && initializer->compound_init &&
        !initializer->compound_init->next &&
        initializer->compound_init->designator_kind == INIT_DESIGNATOR_NONE &&
        initializer->compound_init->expr &&
        initializer->compound_init->expr->kind == EXPR_STRING_LIT) {
        string = initializer->compound_init->expr;
    }
    if (!string) return NULL;
    expects_utf8 = rcc_parser_is_cxx_mode() && type->base->is_unsigned;
    if (expects_utf8 != string->is_cxx_utf8_literal) return NULL;
    return string;
}

static Expr* initializer_string_literal(Expr* initializer) {
    if (!initializer) return NULL;
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

typedef enum {
    RCC_MMX_BUILTIN_NONE,
    RCC_MMX_BUILTIN_BINARY,
    RCC_MMX_BUILTIN_VARIABLE_SHIFT,
    RCC_MMX_BUILTIN_IMMEDIATE_SHIFT,
    RCC_MMX_BUILTIN_STORE,
    RCC_MMX_BUILTIN_INIT_V2SI,
    RCC_MMX_BUILTIN_INIT_V4HI,
    RCC_MMX_BUILTIN_INIT_V8QI
} RccMmxBuiltinKind;

static RccMmxBuiltinKind sema_mmx_builtin_kind(const char* name) {
    static const char* const binary_names[] = {
        "__builtin_ia32_packsswb", "__builtin_ia32_packssdw",
        "__builtin_ia32_packuswb", "__builtin_ia32_punpckhbw",
        "__builtin_ia32_punpckhwd", "__builtin_ia32_punpckhdq",
        "__builtin_ia32_punpcklbw", "__builtin_ia32_punpcklwd",
        "__builtin_ia32_punpckldq", "__builtin_ia32_paddb",
        "__builtin_ia32_paddw", "__builtin_ia32_paddd",
        "__builtin_ia32_paddsb", "__builtin_ia32_paddsw",
        "__builtin_ia32_paddusb", "__builtin_ia32_paddusw",
        "__builtin_ia32_psubb", "__builtin_ia32_psubw",
        "__builtin_ia32_psubd", "__builtin_ia32_psubsb",
        "__builtin_ia32_psubsw", "__builtin_ia32_psubusb",
        "__builtin_ia32_psubusw", "__builtin_ia32_pmaddwd",
        "__builtin_ia32_pmulhw", "__builtin_ia32_pmullw",
        "__builtin_ia32_pand", "__builtin_ia32_pandn",
        "__builtin_ia32_por", "__builtin_ia32_pxor",
        "__builtin_ia32_pcmpeqb", "__builtin_ia32_pcmpeqw",
        "__builtin_ia32_pcmpeqd", "__builtin_ia32_pcmpgtb",
        "__builtin_ia32_pcmpgtw", "__builtin_ia32_pcmpgtd"
    };
    static const char* const variable_shift_names[] = {
        "__builtin_ia32_psllw", "__builtin_ia32_pslld",
        "__builtin_ia32_psllq", "__builtin_ia32_psraw",
        "__builtin_ia32_psrad", "__builtin_ia32_psrlw",
        "__builtin_ia32_psrld", "__builtin_ia32_psrlq"
    };
    static const char* const immediate_shift_names[] = {
        "__builtin_ia32_psllwi", "__builtin_ia32_pslldi",
        "__builtin_ia32_psllqi", "__builtin_ia32_psrawi",
        "__builtin_ia32_psradi", "__builtin_ia32_psrlwi",
        "__builtin_ia32_psrldi", "__builtin_ia32_psrlqi"
    };
    size_t index;
    if (!name) return RCC_MMX_BUILTIN_NONE;
    if (strcmp(name, "__builtin_ia32_movntq") == 0) {
        return RCC_MMX_BUILTIN_STORE;
    }
    for (index = 0; index < sizeof(binary_names) / sizeof(binary_names[0]);
         ++index) {
        if (strcmp(name, binary_names[index]) == 0) {
            return RCC_MMX_BUILTIN_BINARY;
        }
    }
    for (index = 0;
         index < sizeof(variable_shift_names) / sizeof(variable_shift_names[0]);
         ++index) {
        if (strcmp(name, variable_shift_names[index]) == 0) {
            return RCC_MMX_BUILTIN_VARIABLE_SHIFT;
        }
    }
    for (index = 0;
         index < sizeof(immediate_shift_names) /
                    sizeof(immediate_shift_names[0]); ++index) {
        if (strcmp(name, immediate_shift_names[index]) == 0) {
            return RCC_MMX_BUILTIN_IMMEDIATE_SHIFT;
        }
    }
    if (strcmp(name, "__builtin_ia32_vec_init_v2si") == 0) {
        return RCC_MMX_BUILTIN_INIT_V2SI;
    }
    if (strcmp(name, "__builtin_ia32_vec_init_v4hi") == 0) {
        return RCC_MMX_BUILTIN_INIT_V4HI;
    }
    if (strcmp(name, "__builtin_ia32_vec_init_v8qi") == 0) {
        return RCC_MMX_BUILTIN_INIT_V8QI;
    }
    return RCC_MMX_BUILTIN_NONE;
}

static bool sema_sse_builtin_name(const char* name) {
    return name && strncmp(name, "__builtin_ia32_", 15) == 0;
}

static bool sema_sse2_builtin_name(const char* name) {
    const char* suffix;
    if (!sema_sse_builtin_name(name)) return false;
    suffix = name + 15;
    if (strcmp(suffix, "pause") == 0 || strcmp(suffix, "clflush") == 0 ||
        strcmp(suffix, "lfence") == 0 || strcmp(suffix, "mfence") == 0 ||
        strcmp(suffix, "movnti") == 0 || strcmp(suffix, "loaddqu") == 0 ||
        strcmp(suffix, "storedqu") == 0 ||
        strcmp(suffix, "vec_ext_v8hi") == 0 ||
        strcmp(suffix, "vec_set_v8hi") == 0) return true;
    if (strstr(suffix, "pd") || strstr(suffix, "sd") ||
        strstr(suffix, "comi") || strstr(suffix, "ucomi") ||
        strncmp(suffix, "cvt", 3) == 0 ||
        strncmp(suffix, "p", 1) == 0) return true;
    return false;
}

static int sema_sse_builtin_arity(const char* name) {
    const char* suffix;
    if (!sema_sse_builtin_name(name)) return -1;
    suffix = name + 15;
    if (strcmp(suffix, "pause") == 0 || strcmp(suffix, "sfence") == 0 ||
        strcmp(suffix, "lfence") == 0 || strcmp(suffix, "mfence") == 0) {
        return 0;
    }
    if (strcmp(suffix, "vec_set_v8hi") == 0) return 9;
    if (strcmp(suffix, "cvtsi2ss") == 0 ||
        strcmp(suffix, "cvtsi642ss") == 0) return 2;
    if (strcmp(suffix, "movmskps") == 0 ||
        strcmp(suffix, "movmskpd") == 0 ||
        strcmp(suffix, "pmovmskb128") == 0 ||
        strcmp(suffix, "loadss") == 0 || strcmp(suffix, "loadups") == 0 ||
        strcmp(suffix, "loadsd") == 0 || strcmp(suffix, "loadupd") == 0 ||
        strcmp(suffix, "loaddqu") == 0 || strcmp(suffix, "ldmxcsr") == 0 ||
        strcmp(suffix, "clflush") == 0 ||
        strncmp(suffix, "cvt", 3) == 0 ||
        strncmp(suffix, "sqrt", 4) == 0 || strncmp(suffix, "rcp", 3) == 0 ||
        strncmp(suffix, "rsqrt", 5) == 0) return 1;
    if (strcmp(suffix, "shufps") == 0 || strcmp(suffix, "shufpd") == 0) {
        return 3;
    }
    if (strcmp(suffix, "vec_ext_v8hi") == 0) return 2;
    if (strcmp(suffix, "stmxcsr") == 0 ||
        strcmp(suffix, "pause") == 0) return 0;
    if (strncmp(suffix, "pshuf", 5) == 0 ||
        strncmp(suffix, "psll", 4) == 0 || strncmp(suffix, "psrl", 4) == 0 ||
        strncmp(suffix, "psra", 4) == 0 ||
        strncmp(suffix, "p", 1) == 0 ||
        strncmp(suffix, "comi", 4) == 0 ||
        strncmp(suffix, "ucomi", 5) == 0 ||
        strncmp(suffix, "cmp", 3) == 0 ||
        strncmp(suffix, "mov", 3) == 0 ||
        strncmp(suffix, "unpck", 5) == 0 ||
        strncmp(suffix, "unpack", 6) == 0 ||
        strncmp(suffix, "add", 3) == 0 || strncmp(suffix, "sub", 3) == 0 ||
        strncmp(suffix, "mul", 3) == 0 || strncmp(suffix, "div", 3) == 0 ||
        strncmp(suffix, "min", 3) == 0 || strncmp(suffix, "max", 3) == 0 ||
        strncmp(suffix, "and", 3) == 0 || strncmp(suffix, "or", 2) == 0 ||
        strncmp(suffix, "xor", 3) == 0) return 2;
    if (strncmp(suffix, "store", 5) == 0 ||
        strncmp(suffix, "movnt", 5) == 0) return 2;
    return -1;
}

static Expr* sema_sse_argument(Expr* call, int index) {
    ExprList* argument = call ? call->call_args : NULL;
    while (argument && index-- > 0) argument = argument->next;
    return argument ? argument->expr : NULL;
}

static void sema_sse_require_vector(Expr* call, const char* name, int index) {
    Expr* argument = sema_sse_argument(call, index);
    if (!argument || !argument->type || !type_is_vector(argument->type)) {
        rcc_error(call->loc, "%s argument %d must have vector type", name,
                  index + 1);
    }
}

static void sema_sse_require_pointer(Expr* call, const char* name, int index) {
    Expr* argument = sema_sse_argument(call, index);
    if (!argument || !argument->type || argument->type->kind != TYPE_PTR) {
        rcc_error(call->loc, "%s argument %d must have pointer type", name,
                  index + 1);
    }
}

static void sema_sse_require_integer(Expr* call, const char* name, int index) {
    Expr* argument = sema_sse_argument(call, index);
    if (!argument || !argument->type || !type_is_integer(argument->type)) {
        rcc_error(call->loc, "%s argument %d must have integer type", name,
                  index + 1);
    }
}

static void sema_validate_sse_operands(Expr* call, const char* name,
                                        int expected_count) {
    const char* suffix;
    int64_t immediate;
    if (!call || !name || expected_count < 0) return;
    if (0 != strncmp(name, "__builtin_ia32_", 15)) return;
    if (sema_sse_builtin_arity(name) != expected_count) return;
    suffix = name + 15;
    if (strcmp(suffix, "ldmxcsr") == 0) {
        sema_sse_require_integer(call, name, 0);
    } else if (strcmp(suffix, "clflush") == 0 ||
               strncmp(suffix, "load", 4) == 0) {
        sema_sse_require_pointer(call, name, 0);
    } else if (strncmp(suffix, "store", 5) == 0 ||
               strncmp(suffix, "movnt", 5) == 0) {
        sema_sse_require_pointer(call, name, 0);
        if (strcmp(suffix, "movnti") == 0 ||
            strcmp(suffix, "movntq") == 0) {
            sema_sse_require_integer(call, name, 1);
        } else {
            sema_sse_require_vector(call, name, 1);
        }
    } else if (strcmp(suffix, "movmskps") == 0 ||
               strcmp(suffix, "movmskpd") == 0 ||
               strcmp(suffix, "pmovmskb128") == 0 ||
               strcmp(suffix, "vec_ext_v8hi") == 0) {
        sema_sse_require_vector(call, name, 0);
        if (strcmp(suffix, "vec_ext_v8hi") == 0) {
            sema_sse_require_integer(call, name, 1);
        }
    } else if (strcmp(suffix, "vec_set_v8hi") == 0) {
        for (int index = 0; index < expected_count; ++index) {
            sema_sse_require_integer(call, name, index);
        }
    } else if (strcmp(suffix, "cvtsi2ss") == 0 ||
               strcmp(suffix, "cvtsi642ss") == 0) {
        sema_sse_require_vector(call, name, 0);
        sema_sse_require_integer(call, name, 1);
    } else if (strncmp(suffix, "cvt", 3) == 0 ||
               strncmp(suffix, "sqrt", 4) == 0 ||
               strncmp(suffix, "rcp", 3) == 0 ||
               strncmp(suffix, "rsqrt", 5) == 0) {
        sema_sse_require_vector(call, name, 0);
    } else if (strcmp(suffix, "shufps") == 0 ||
               strcmp(suffix, "shufpd") == 0) {
        sema_sse_require_vector(call, name, 0);
        sema_sse_require_vector(call, name, 1);
        sema_sse_require_integer(call, name, 2);
        if (!expr_eval_integer_constant(sema_sse_argument(call, 2),
                                        &immediate)) {
            rcc_error(call->loc, "%s requires an integer constant immediate",
                      name);
        } else if (immediate < 0 || immediate > 255) {
            rcc_error(call->loc, "%s immediate must be between 0 and 255",
                      name);
        }
    } else if (strncmp(suffix, "pshuf", 5) == 0) {
        sema_sse_require_vector(call, name, 0);
        sema_sse_require_integer(call, name, 1);
        if (!expr_eval_integer_constant(sema_sse_argument(call, 1),
                                        &immediate)) {
            rcc_error(call->loc, "%s requires an integer constant immediate",
                      name);
        } else if (immediate < 0 || immediate > 255) {
            rcc_error(call->loc, "%s immediate must be between 0 and 255",
                      name);
        }
    } else if (strncmp(suffix, "psll", 4) == 0 ||
               strncmp(suffix, "psrl", 4) == 0 ||
               strncmp(suffix, "psra", 4) == 0) {
        sema_sse_require_vector(call, name, 0);
        sema_sse_require_integer(call, name, 1);
    } else if (expected_count == 2) {
        sema_sse_require_vector(call, name, 0);
        sema_sse_require_vector(call, name, 1);
    }
}

static Type* sema_sse_builtin_vector(const char* name) {
    if (!name) return NULL;
    if (strstr(name, "pd") || strstr(name, "sd") ||
        strstr(name, "cvtps2pd") || strstr(name, "cvtdq2pd")) {
        return rcc_parser_lookup_type("__v2df");
    }
    if (strstr(name, "128") || strstr(name, "loaddqu") ||
        strstr(name, "storedqu")) {
        return rcc_parser_lookup_type("__v2di");
    }
    if (strstr(name, "cvtps2dq") || strstr(name, "cvttpd2dq") ||
        strstr(name, "cvttps2dq")) {
        return rcc_parser_lookup_type("__v4si");
    }
    if (strstr(name, "cvtpd2ps")) {
        return rcc_parser_lookup_type("__v4sf");
    }
    if (strstr(name, "vec_set_v8hi")) {
        return rcc_parser_lookup_type("__v8hi");
    }
    return rcc_parser_lookup_type("__v4sf");
}

static Type* sema_sse_builtin_return_type(const char* name) {
    if (!name) return type_int;
    if (strstr(name, "comi") || strstr(name, "ucomi") ||
        strstr(name, "movmsk") || strstr(name, "cvtss2si") ||
        strstr(name, "cvttss2si") ||
        strstr(name, "vec_ext_v8hi")) {
        return (strstr(name, "64") || strstr(name, "si64"))
            ? type_llong : type_int;
    }
    if (strstr(name, "lfence") || strstr(name, "mfence") ||
        strstr(name, "sfence") || strstr(name, "pause") ||
        strstr(name, "clflush") || strstr(name, "ldmxcsr") ||
        strstr(name, "movnt") ||
        strstr(name, "store")) {
        return type_void;
    }
    if (strstr(name, "stmxcsr")) return type_uint;
    return sema_sse_builtin_vector(name);
}

static bool sema_compiler_builtin_call(Expr* expr) {
    Expr* function;
    Expr* first;
    Expr* second;
    Expr* third;
    ExprList* argument;
    const char* name;
    int argument_count = 0;
    int bswap_width = 0;
    bool is_bit_count = false;
    bool is_bit_count_wide = false;
    RccMmxBuiltinKind mmx_kind;

    if (!expr || expr->kind != EXPR_CALL ||
        !expr->call_func || expr->call_func->kind != EXPR_IDENT) {
        return false;
    }
    function = expr->call_func;
    name = function->ident_name;
    mmx_kind = sema_mmx_builtin_kind(name);
    if (mmx_kind != RCC_MMX_BUILTIN_NONE) {
        int expected_count = mmx_kind == RCC_MMX_BUILTIN_INIT_V2SI ? 2 :
            mmx_kind == RCC_MMX_BUILTIN_INIT_V4HI ? 4 :
            mmx_kind == RCC_MMX_BUILTIN_INIT_V8QI ? 8 : 2;
        int64_t immediate = 0;
        for (argument = expr->call_args; argument;
             argument = argument->next) {
            sema_expr(argument->expr);
            ++argument_count;
        }
        if (argument_count != expected_count) {
            rcc_error(expr->loc, "%s expects %d arguments, got %d", name,
                      expected_count, argument_count);
        }
        if (mmx_kind == RCC_MMX_BUILTIN_STORE) {
            first = expr->call_args ? expr->call_args->expr : NULL;
            second = expr->call_args && expr->call_args->next
                ? expr->call_args->next->expr : NULL;
            if (!first || !first->type || first->type->kind != TYPE_PTR) {
                rcc_error(expr->loc,
                          "%s first argument must have pointer type", name);
            }
            if (!second || !second->type || !type_is_integer(second->type) ||
                second->type->size > 8) {
                rcc_error(expr->loc,
                          "%s second argument must be an integer no wider than 8 bytes",
                          name);
            }
        } else {
            for (argument = expr->call_args; argument;
                 argument = argument->next) {
                if (!argument->expr->type ||
                    !type_is_integer(argument->expr->type) ||
                    argument->expr->type->size >
                        (mmx_kind == RCC_MMX_BUILTIN_INIT_V2SI ||
                         mmx_kind == RCC_MMX_BUILTIN_INIT_V4HI ||
                         mmx_kind == RCC_MMX_BUILTIN_INIT_V8QI ? 4 : 8)) {
                    rcc_error(expr->loc,
                              "%s expects integer arguments within the MMX value width",
                              name);
                }
            }
        }
        if (mmx_kind == RCC_MMX_BUILTIN_IMMEDIATE_SHIFT) {
            second = expr->call_args && expr->call_args->next
                ? expr->call_args->next->expr : NULL;
            if (!second || !second->type || !type_is_integer(second->type)) {
                rcc_error(expr->loc,
                          "%s shift count must have integer type",
                          name);
            } else if (expr_eval_integer_constant(second, &immediate) &&
                       (immediate < 0 || immediate > 63)) {
                rcc_error(expr->loc,
                          "%s constant shift count must be between 0 and 63",
                          name);
            }
        }
        function->type = type_ptr(mmx_kind == RCC_MMX_BUILTIN_STORE
                                      ? type_void : type_llong);
        expr->type = mmx_kind == RCC_MMX_BUILTIN_STORE ? type_void : type_llong;
        return true;
    }
    if (sema_sse_builtin_name(name)) {
        Type* return_type = sema_sse_builtin_return_type(name);
        int expected_count = sema_sse_builtin_arity(name);
        bool feature_error = false;
        for (argument = expr->call_args; argument;
             argument = argument->next) {
            sema_expr(argument->expr);
            ++argument_count;
        }
        if (!g_opts.sse_enabled) {
            rcc_error(expr->loc,
                      "%s requires SSE; enable it with -msse", name);
            feature_error = true;
        } else if (sema_sse2_builtin_name(name) && !g_opts.sse2_enabled) {
            rcc_error(expr->loc,
                      "%s requires SSE2; enable it with -msse2", name);
            feature_error = true;
        }
        if (expected_count < 0) {
            rcc_error(expr->loc, "unsupported SSE/SSE2 compiler builtin '%s'",
                      name);
        } else if (argument_count != expected_count) {
            rcc_error(expr->loc, "%s expects %d arguments, got %d", name,
                      expected_count, argument_count);
        } else if (!feature_error) {
            sema_validate_sse_operands(expr, name, expected_count);
        }
        function->type = type_ptr(return_type);
        expr->type = return_type;
        return true;
    }
    if (strcmp(name, "__builtin_choose_expr") == 0) {
        int64_t condition = 0;
        bool condition_constant = false;
        Expr* selected;
        for (argument = expr->call_args; argument;
             argument = argument->next) {
            sema_expr(argument->expr);
            ++argument_count;
        }
        if (argument_count != 3) {
            rcc_error(expr->loc,
                      "__builtin_choose_expr expects 3 arguments, got %d",
                      argument_count);
        }
        first = expr->call_args ? expr->call_args->expr : NULL;
        second = expr->call_args && expr->call_args->next
            ? expr->call_args->next->expr : NULL;
        third = expr->call_args && expr->call_args->next &&
            expr->call_args->next->next
            ? expr->call_args->next->next->expr : NULL;
        if (!first || !first->type || !type_is_integer(first->type)) {
            rcc_error(expr->loc,
                      "__builtin_choose_expr condition must have integer type");
        } else {
            condition_constant = expr_eval_integer_constant(first, &condition);
            if (!condition_constant) {
                rcc_error(first->loc,
                          "__builtin_choose_expr condition must be an integer constant expression");
            }
        }
        selected = condition_constant && condition != 0 ? second : third;
        function->type = type_ptr(selected && selected->type
                                       ? selected->type : type_int);
        expr->type = selected && selected->type ? selected->type : type_int;
        return true;
    }
    if (strcmp(name, "__builtin_bswap16") == 0) {
        bswap_width = 2;
    } else if (strcmp(name, "__builtin_bswap32") == 0) {
        bswap_width = 4;
    } else if (strcmp(name, "__builtin_bswap64") == 0) {
        bswap_width = 8;
    }
    if (bswap_width != 0) {
        for (argument = expr->call_args; argument;
             argument = argument->next) {
            sema_expr(argument->expr);
            ++argument_count;
        }
        if (argument_count != 1) {
            rcc_error(expr->loc, "%s expects 1 argument, got %d", name,
                      argument_count);
        }
        first = expr->call_args ? expr->call_args->expr : NULL;
        if (!first || !first->type || !type_is_integer(first->type) ||
            first->type->size > bswap_width) {
            rcc_error(expr->loc,
                      "%s expects an integer argument no wider than %d bytes",
                      name, bswap_width);
        }
        function->type = type_ptr(bswap_width == 2 ? type_ushort :
                                  bswap_width == 4 ? type_uint : type_ullong);
        expr->type = bswap_width == 2 ? type_ushort :
                     bswap_width == 4 ? type_uint : type_ullong;
        return true;
    }
    if (strcmp(name, "__builtin_clz") == 0 ||
        strcmp(name, "__builtin_ctz") == 0 ||
        strcmp(name, "__builtin_popcount") == 0 ||
        strcmp(name, "__builtin_parity") == 0 ||
        strcmp(name, "__builtin_ffs") == 0) {
        is_bit_count = true;
    } else if (strcmp(name, "__builtin_clzl") == 0 ||
               strcmp(name, "__builtin_ctzl") == 0 ||
               strcmp(name, "__builtin_popcountl") == 0 ||
               strcmp(name, "__builtin_parityl") == 0 ||
               strcmp(name, "__builtin_ffsl") == 0) {
        is_bit_count = true;
        is_bit_count_wide = g_opts.target_arch == ARCH_X64;
    } else if (strcmp(name, "__builtin_clzll") == 0 ||
               strcmp(name, "__builtin_ctzll") == 0 ||
               strcmp(name, "__builtin_popcountll") == 0 ||
               strcmp(name, "__builtin_parityll") == 0 ||
               strcmp(name, "__builtin_ffsll") == 0) {
        is_bit_count = true;
        is_bit_count_wide = true;
    } else if (strcmp(name, "__builtin_clrsb") == 0) {
        is_bit_count = true;
    } else if (strcmp(name, "__builtin_clrsbl") == 0) {
        is_bit_count = true;
        is_bit_count_wide = g_opts.target_arch == ARCH_X64;
    } else if (strcmp(name, "__builtin_clrsbll") == 0) {
        is_bit_count = true;
        is_bit_count_wide = true;
    }
    if (is_bit_count) {
        for (argument = expr->call_args; argument;
             argument = argument->next) {
            sema_expr(argument->expr);
            ++argument_count;
        }
        if (argument_count != 1) {
            rcc_error(expr->loc, "%s expects 1 argument, got %d", name,
                      argument_count);
        }
        first = expr->call_args ? expr->call_args->expr : NULL;
        if (!first || !first->type || !type_is_integer(first->type) ||
            first->type->size > (is_bit_count_wide ? 8 : 4)) {
            rcc_error(expr->loc,
                      "%s expects an integer argument no wider than %d bytes",
                      name, is_bit_count_wide ? 8 : 4);
        }
        function->type = type_ptr(type_int);
        expr->type = type_int;
        return true;
    }
    if (strcmp(name, "__builtin_prefetch") == 0) {
        int64_t value;
        for (argument = expr->call_args; argument;
             argument = argument->next) {
            sema_expr(argument->expr);
            ++argument_count;
        }
        if (argument_count < 1 || argument_count > 3) {
            rcc_error(expr->loc,
                      "__builtin_prefetch expects 1 to 3 arguments, got %d",
                      argument_count);
        }
        first = expr->call_args ? expr->call_args->expr : NULL;
        if (!first || !first->type || !type_is_pointer(first->type)) {
            rcc_error(expr->loc,
                      "__builtin_prefetch address must have pointer type");
        }
        second = expr->call_args && expr->call_args->next
            ? expr->call_args->next->expr : NULL;
        if (second) {
            if (!second->type || !type_is_integer(second->type)) {
                rcc_error(expr->loc,
                          "__builtin_prefetch rw argument must have integer type");
            } else if (expr_eval_integer_constant(second, &value) &&
                       value != 0 && value != 1) {
                rcc_error(expr->loc,
                          "__builtin_prefetch rw argument must be 0 or 1");
            }
        }
        second = expr->call_args && expr->call_args->next
            ? expr->call_args->next->next
                ? expr->call_args->next->next->expr : NULL : NULL;
        if (second) {
            if (!second->type || !type_is_integer(second->type)) {
                rcc_error(expr->loc,
                          "__builtin_prefetch locality argument must have integer type");
            } else if (expr_eval_integer_constant(second, &value) &&
                       (value < 0 || value > 3)) {
                rcc_error(expr->loc,
                          "__builtin_prefetch locality argument must be between 0 and 3");
            }
        }
        function->type = type_ptr(type_void);
        expr->type = type_void;
        return true;
    }
    if (strcmp(name, "__builtin_constant_p") == 0) {
        for (argument = expr->call_args; argument;
             argument = argument->next) {
            sema_expr(argument->expr);
            ++argument_count;
        }
        if (argument_count != 1) {
            rcc_error(expr->loc,
                      "__builtin_constant_p expects 1 argument, got %d",
                      argument_count);
        }
        /* GCC's constant_p query is intentionally not a runtime evaluation:
         * the operand is type-checked above, but code generation only needs
         * the front-end's integer-constant-expression answer. */
        function->type = type_ptr(type_int);
        expr->type = type_int;
        return true;
    }
    if (strcmp(name, "__builtin_object_size") == 0) {
        int64_t object_size_mode = 0;
        bool mode_is_constant = false;
        for (argument = expr->call_args; argument;
             argument = argument->next) {
            sema_expr(argument->expr);
            ++argument_count;
        }
        if (argument_count != 2) {
            rcc_error(expr->loc,
                      "__builtin_object_size expects 2 arguments, got %d",
                      argument_count);
        }
        first = expr->call_args ? expr->call_args->expr : NULL;
        if (!first || !first->type ||
            (!type_is_pointer(first->type) &&
             first->type->kind != TYPE_ARRAY)) {
            rcc_error(expr->loc,
                      "__builtin_object_size first argument must have pointer type");
        }
        second = expr->call_args && expr->call_args->next
            ? expr->call_args->next->expr : NULL;
        if (!second || !second->type || !type_is_integer(second->type)) {
            rcc_error(expr->loc,
                      "__builtin_object_size type argument must have integer type");
        } else {
            mode_is_constant = expr_eval_integer_constant(
                second, &object_size_mode);
            if (!mode_is_constant || object_size_mode < 0 ||
                object_size_mode > 3) {
                rcc_error(expr->loc,
                          "__builtin_object_size type argument must be an integer constant between 0 and 3");
            }
        }
        (void)mode_is_constant;
        function->type = type_ptr(g_opts.target_arch == ARCH_X64
                                      ? type_ullong : type_uint);
        expr->type = g_opts.target_arch == ARCH_X64 ? type_ullong : type_uint;
        return true;
    }
    if (strcmp(name, "__builtin_strlen") == 0) {
        Expr* string = NULL;
        Expr* literal = NULL;
        Type* string_type = NULL;
        bool character_data = false;
        for (argument = expr->call_args; argument;
             argument = argument->next) {
            sema_expr(argument->expr);
            ++argument_count;
        }
        if (argument_count != 1) {
            rcc_error(expr->loc,
                      "__builtin_strlen expects 1 argument, got %d",
                      argument_count);
        }
        string = expr->call_args ? expr->call_args->expr : NULL;
        literal = string;
        while (literal && literal->kind == EXPR_CAST) {
            literal = literal->cast_expr;
        }
        if (literal && literal->kind == EXPR_STRING_LIT) {
            character_data = true;
        } else if (string && string->type) {
            string_type = string->type;
            if ((string_type->kind == TYPE_PTR ||
                 string_type->kind == TYPE_ARRAY) && string_type->base &&
                type_is_integer(string_type->base) &&
                string_type->base->size == 1) {
                character_data = true;
            }
        }
        if (!character_data) {
            rcc_error(expr->loc,
                      "__builtin_strlen expects a pointer to character data or a string literal");
        }
        function->type = type_ptr(g_opts.target_arch == ARCH_X64
                                      ? type_ullong : type_uint);
        expr->type = g_opts.target_arch == ARCH_X64 ? type_ullong : type_uint;
        return true;
    }
    if (strcmp(name, "__builtin_add_overflow") == 0 ||
        strcmp(name, "__builtin_sub_overflow") == 0 ||
        strcmp(name, "__builtin_mul_overflow") == 0) {
        Type* result_type = NULL;
        for (argument = expr->call_args; argument;
             argument = argument->next) {
            sema_expr(argument->expr);
            ++argument_count;
        }
        if (argument_count != 3) {
            rcc_error(expr->loc, "%s expects 3 arguments, got %d", name,
                      argument_count);
        }
        first = expr->call_args ? expr->call_args->expr : NULL;
        second = expr->call_args && expr->call_args->next
            ? expr->call_args->next->expr : NULL;
        third = expr->call_args && expr->call_args->next &&
            expr->call_args->next->next
            ? expr->call_args->next->next->expr : NULL;
        if (!first || !first->type || !type_is_integer(first->type)) {
            rcc_error(expr->loc,
                      "%s first argument must have integer type", name);
        }
        if (!second || !second->type || !type_is_integer(second->type)) {
            rcc_error(expr->loc,
                      "%s second argument must have integer type", name);
        }
        if (!third || !third->type || third->type->kind != TYPE_PTR ||
            !third->type->base || !type_is_integer(third->type->base)) {
            rcc_error(expr->loc,
                      "%s result argument must point to an integer type", name);
        } else {
            result_type = third->type->base;
        }
        if (result_type &&
            (result_type->size != 1 && result_type->size != 2 &&
             result_type->size != 4 && result_type->size != 8)) {
            rcc_error(expr->loc,
                      "%s result type has an unsupported integer width", name);
        }
        if (first && first->type && result_type &&
            (first->type->size != result_type->size ||
             first->type->is_unsigned != result_type->is_unsigned)) {
            rcc_error(expr->loc,
                      "%s operands and result must have the same integer width and signedness",
                      name);
        }
        if (second && second->type && result_type &&
            (second->type->size != result_type->size ||
             second->type->is_unsigned != result_type->is_unsigned)) {
            rcc_error(expr->loc,
                      "%s operands and result must have the same integer width and signedness",
                      name);
        }
        function->type = type_ptr(type_int);
        expr->type = type_int;
        return true;
    }
    if (strcmp(name, "__builtin_assume_aligned") == 0) {
        int64_t alignment = 0;
        int64_t offset = 0;
        bool alignment_constant = false;
        bool offset_constant = false;
        for (argument = expr->call_args; argument;
             argument = argument->next) {
            sema_expr(argument->expr);
            ++argument_count;
        }
        if (argument_count < 2 || argument_count > 3) {
            rcc_error(expr->loc,
                      "__builtin_assume_aligned expects 2 or 3 arguments, got %d",
                      argument_count);
        }
        first = expr->call_args ? expr->call_args->expr : NULL;
        if (!first || !first->type || !type_is_pointer(first->type)) {
            rcc_error(expr->loc,
                      "__builtin_assume_aligned first argument must have pointer type");
        }
        second = expr->call_args && expr->call_args->next
            ? expr->call_args->next->expr : NULL;
        if (!second || !second->type || !type_is_integer(second->type)) {
            rcc_error(expr->loc,
                      "__builtin_assume_aligned alignment must have integer type");
        } else {
            alignment_constant = expr_eval_integer_constant(second, &alignment);
            if (!alignment_constant || alignment <= 0 ||
                (alignment & (alignment - 1)) != 0) {
                rcc_error(expr->loc,
                          "__builtin_assume_aligned alignment must be a positive power of two constant");
            }
        }
        third = expr->call_args && expr->call_args->next
            ? expr->call_args->next->next
                ? expr->call_args->next->next->expr : NULL : NULL;
        if (third) {
            if (!third->type || !type_is_integer(third->type)) {
                rcc_error(expr->loc,
                          "__builtin_assume_aligned offset must have integer type");
            } else {
                offset_constant = expr_eval_integer_constant(third, &offset);
                if (!offset_constant) {
                    rcc_error(expr->loc,
                              "__builtin_assume_aligned offset must be an integer constant");
                }
            }
        }
        (void)alignment_constant;
        (void)offset_constant;
        function->type = type_ptr(type_void);
        expr->type = first && first->type ? first->type : type_ptr(type_void);
        return true;
    }
    if (strcmp(name, "__builtin_unreachable") == 0 ||
        strcmp(name, "__builtin_trap") == 0) {
        if (expr->call_args) {
            for (argument = expr->call_args; argument;
                 argument = argument->next) {
                sema_expr(argument->expr);
                ++argument_count;
            }
        }
        if (argument_count != 0) {
            rcc_error(expr->loc,
                      "%s expects no arguments, got %d", name,
                      argument_count);
        }
        function->type = type_ptr(type_void);
        expr->type = type_void;
        return true;
    }
    if (strcmp(name, "__builtin_expect") != 0 &&
        strcmp(name, "__builtin_expect_with_probability") != 0) {
        return false;
    }

    for (argument = expr->call_args; argument; argument = argument->next) {
        sema_expr(argument->expr);
        ++argument_count;
    }
    if (strcmp(name, "__builtin_expect_with_probability") == 0) {
        if (argument_count != 3) {
            rcc_error(expr->loc,
                      "__builtin_expect_with_probability expects 3 arguments, got %d",
                      argument_count);
        }
    } else if (argument_count != 2) {
        rcc_error(expr->loc, "__builtin_expect expects 2 arguments, got %d",
                  argument_count);
    }
    first = expr->call_args ? expr->call_args->expr : NULL;
    second = expr->call_args && expr->call_args->next
        ? expr->call_args->next->expr : NULL;
    if (!first || !first->type || !type_is_integer(first->type)) {
        rcc_error(expr->loc,
                  "__builtin_expect value must have integer type");
    }
    if (!second || !second->type || !type_is_integer(second->type)) {
        rcc_error(expr->loc,
                  "__builtin_expect expected value must have integer type");
    }
    if (strcmp(name, "__builtin_expect_with_probability") == 0) {
        third = expr->call_args && expr->call_args->next
            ? expr->call_args->next->next
                ? expr->call_args->next->next->expr : NULL : NULL;
        if (!third || !third->type || !type_is_floating(third->type) ||
            third->kind != EXPR_FLOAT_LIT || third->float_val < 0.0 ||
            third->float_val > 1.0) {
            rcc_error(expr->loc,
                      "__builtin_expect_with_probability probability must be a floating constant between 0 and 1");
        }
    }
    function->type = type_ptr(type_void);
    expr->type = first && first->type ? first->type : type_int;
    return true;
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
    if (strcmp(name, "__atomic_load") == 0) {
        expected_count = 3;
        returns_void = true;
    } else if (strcmp(name, "__atomic_store") == 0) {
        expected_count = 3;
        returns_void = true;
    } else if (strcmp(name, "__atomic_exchange") == 0) {
        expected_count = 4;
        returns_void = true;
    } else if (strcmp(name, "__atomic_compare_exchange") == 0) {
        expected_count = 6;
        requires_expected_pointer = true;
        returns_bool = true;
    } else if (strcmp(name, "__atomic_load_n") == 0) {
        expected_count = 2;
    } else if (strcmp(name, "__atomic_store_n") == 0) {
        expected_count = 3;
        returns_void = true;
    } else if (strcmp(name, "__atomic_always_lock_free") == 0 ||
               strcmp(name, "__atomic_is_lock_free") == 0) {
        expected_count = 2;
        requires_pointer = false;
        returns_bool = true;
    } else if (strcmp(name, "__atomic_test_and_set") == 0) {
        expected_count = 2;
        returns_bool = true;
    } else if (strcmp(name, "__atomic_clear") == 0) {
        expected_count = 2;
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
    if (strcmp(name, "__atomic_always_lock_free") == 0 ||
        strcmp(name, "__atomic_is_lock_free") == 0) {
        Expr* size_expression = sema_call_argument(expr, 0);
        Expr* pointer_expression = sema_call_argument(expr, 1);
        int64_t size_value = 0;
        int64_t pointer_value = 0;
        bool size_constant = size_expression &&
            expr_eval_integer_constant(size_expression, &size_value);
        bool null_pointer = pointer_expression &&
            ((pointer_expression->type &&
              pointer_expression->type->kind == TYPE_NULLPTR) ||
             (pointer_expression->type &&
              type_is_integer(pointer_expression->type) &&
              expr_eval_integer_constant(pointer_expression, &pointer_value) &&
              pointer_value == 0));
        if (!size_expression || !size_expression->type ||
            (!type_is_integer(size_expression->type) &&
             size_expression->type->kind != TYPE_ENUM)) {
            rcc_error(expr->loc,
                      "%s size argument must have integer type", name);
        } else if (strcmp(name, "__atomic_always_lock_free") == 0 &&
                   !size_constant) {
            rcc_error(size_expression->loc,
                      "%s size argument must be an integer constant", name);
        } else if (size_expression->type->size >
                   (g_opts.target_arch == ARCH_X64 ? 8 : 4)) {
            rcc_error(size_expression->loc,
                      "%s size argument is wider than the target word", name);
        }
        if (!pointer_expression || !pointer_expression->type ||
            (!type_is_pointer(pointer_expression->type) && !null_pointer)) {
            rcc_error(expr->loc,
                      "%s second argument must have pointer or null-pointer type",
                      name);
        }
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
        } else if ((strcmp(name, "__atomic_load") == 0 ||
                    strcmp(name, "__atomic_store") == 0 ||
                    strcmp(name, "__atomic_exchange") == 0 ||
                    strcmp(name, "__atomic_compare_exchange") == 0) &&
                   (!pointer_type->base ||
                    (!type_is_integer(pointer_type->base) &&
                     pointer_type->base->kind != TYPE_PTR))) {
            rcc_error(expr->loc,
                      "%s requires a lock-free integer or pointer object type",
                      name);
        } else if ((strcmp(name, "__atomic_test_and_set") == 0 ||
                    strcmp(name, "__atomic_clear") == 0) &&
                   (!pointer_type->base || pointer_type->base->size != 1u)) {
            rcc_error(expr->loc,
                      "%s requires a byte-sized object pointer", name);
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
    if (strcmp(name, "__atomic_load") == 0 ||
        strcmp(name, "__atomic_store") == 0 ||
        strcmp(name, "__atomic_exchange") == 0 ||
        strcmp(name, "__atomic_compare_exchange") == 0) {
        int first_value_index = 1;
        int second_value_index = strcmp(name, "__atomic_exchange") == 0 ? 2 :
            strcmp(name, "__atomic_compare_exchange") == 0 ? 2 : -1;
        Expr* value_argument = sema_call_argument(expr, first_value_index);
        Expr* second_value_argument = second_value_index >= 0
            ? sema_call_argument(expr, second_value_index) : NULL;
        Type* value_pointer_type = value_argument ? value_argument->type : NULL;
        Type* second_pointer_type = second_value_argument
            ? second_value_argument->type : NULL;
        if (!value_pointer_type || value_pointer_type->kind != TYPE_PTR ||
            !value_pointer_type->base || !pointer_type ||
            !type_is_compatible(value_pointer_type->base, pointer_type->base)) {
            rcc_error(expr->loc,
                      "%s value pointer must match the object type", name);
        }
        if (second_value_argument &&
            (!second_pointer_type || second_pointer_type->kind != TYPE_PTR ||
             !second_pointer_type->base || !pointer_type ||
             !type_is_compatible(second_pointer_type->base,
                                 pointer_type->base))) {
            rcc_error(expr->loc,
                      "%s value pointers must match the object type", name);
        }
    }
    if (strcmp(name, "__atomic_compare_exchange") == 0) {
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
    } else if (strcmp(name, "__atomic_load") == 0 ||
               strcmp(name, "__atomic_load_n") == 0) {
        int order_index = strcmp(name, "__atomic_load") == 0 ? 2 : 1;
        if (sema_atomic_order(expr, name, order_index, &success_order,
                              &success_constant) && success_constant &&
            (success_order == 3 || success_order == 4)) {
            rcc_error(sema_call_argument(expr, order_index)->loc,
                      "%s does not accept release or acq_rel order", name);
        }
    } else if (strcmp(name, "__atomic_store") == 0 ||
               strcmp(name, "__atomic_store_n") == 0) {
        int order_index = 2;
        if (sema_atomic_order(expr, name, order_index, &success_order,
                              &success_constant) && success_constant &&
            (success_order == 1 || success_order == 2 ||
             success_order == 4)) {
            rcc_error(sema_call_argument(expr, order_index)->loc,
                      "%s accepts only relaxed, release, or seq_cst order",
                      name);
        }
    } else if (strcmp(name, "__atomic_exchange") == 0) {
        sema_atomic_order(expr, name, 3, &success_order,
                          &success_constant);
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
            ? 0
            : ((strcmp(name, "__atomic_test_and_set") == 0 ||
                strcmp(name, "__atomic_clear") == 0) ? 1 : 2);
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
        size_t characters = string->str_length;
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
        if (field->name && strcmp(field->name, name) == 0) return field;
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
    if (type->kind == TYPE_VECTOR) {
        if (type->array_len <= 0 || !type->base) return 0;
        capacity = (int64_t)type->array_len *
                   initializer_scalar_capacity(type->base);
    } else if (type->kind == TYPE_ARRAY) {
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
    return type && (type->kind == TYPE_ARRAY || type->kind == TYPE_VECTOR ||
                    type->kind == TYPE_STRUCT ||
                    type->kind == TYPE_UNION);
}

typedef struct InitializerDesignatorCursor {
    Type* aggregate;
    TypeField* field;
    int64_t array_index;
    ExprList* tail;
} InitializerDesignatorCursor;

static Type* initializer_designator_target(Type* aggregate,
                                           const ExprList* item,
                                           TypeField** field_out) {
    if (field_out) *field_out = NULL;
    if (!aggregate || !item) return NULL;
    if (item->designator_kind == INIT_DESIGNATOR_INDEX) {
        if (aggregate->kind != TYPE_ARRAY || !aggregate->base ||
            item->designator_index < 0 ||
            item->designator_index >= aggregate->array_len) {
            return NULL;
        }
        return aggregate->base;
    }
    if (item->designator_kind == INIT_DESIGNATOR_FIELD &&
        (aggregate->kind == TYPE_STRUCT || aggregate->kind == TYPE_UNION)) {
        TypeField* field = initializer_field(
            aggregate, item->designator_field);
        if (!field) return NULL;
        if (field_out) *field_out = field;
        return field->type;
    }
    return NULL;
}

static Type* initializer_designator_cursor_next(
    InitializerDesignatorCursor* cursor) {
    if (!cursor || !cursor->aggregate) return NULL;
    if (cursor->aggregate->kind == TYPE_ARRAY) {
        if (!cursor->aggregate->base || cursor->array_index < 0 ||
            cursor->array_index == INT64_MAX ||
            cursor->array_index + 1 >= cursor->aggregate->array_len) {
            return NULL;
        }
        return cursor->aggregate->base;
    }
    if (cursor->aggregate->kind == TYPE_STRUCT) {
        return cursor->field && cursor->field->next
            ? cursor->field->next->type : NULL;
    }
    return NULL;
}

static void initializer_designator_cursor_advance(
    InitializerDesignatorCursor* cursor) {
    if (!cursor || !cursor->aggregate) return;
    if (cursor->aggregate->kind == TYPE_ARRAY) {
        if (cursor->array_index < INT64_MAX) ++cursor->array_index;
    } else if (cursor->aggregate->kind == TYPE_STRUCT && cursor->field) {
        cursor->field = cursor->field->next;
    }
}

static void normalize_brace_elided_initializer(Type* type,
                                                Expr* initializer);

/* A nested C designator creates initializer lists for the remaining levels
 * of the path. Following positional scalar clauses continue in depth-first
 * subobject order: first within the innermost aggregate, then in the next
 * enclosing designated aggregate once that child is exhausted. */
static void initializer_absorb_designator_followups(
    Type* selected_type, ExprList* selected_item) {
    Expr* wrapper;
    Type* aggregate = selected_type;
    ExprList* source;
    InitializerDesignatorCursor* path;
    size_t path_length = 0;
    size_t path_index;

    if (rcc_parser_is_cxx_mode() || !selected_item ||
        !selected_item->next || !selected_item->expr) {
        return;
    }
    wrapper = selected_item->expr;
    while (wrapper && wrapper->kind == EXPR_COMPOUND &&
           wrapper->compound_designator_wrapper) {
        ExprList* designated = wrapper->compound_init;
        Type* target = designated && !designated->next
            ? initializer_designator_target(aggregate, designated, NULL)
            : NULL;
        if (!target) return;
        ++path_length;
        if (designated->expr && designated->expr->kind == EXPR_COMPOUND &&
            designated->expr->compound_designator_wrapper) {
            aggregate = target;
            wrapper = designated->expr;
            continue;
        }
        break;
    }
    if (path_length == 0 ||
        path_length > SIZE_MAX / sizeof(*path)) return;
    path = rcc_alloc(path_length * sizeof(*path));

    aggregate = selected_type;
    wrapper = selected_item->expr;
    for (path_index = 0; path_index < path_length; ++path_index) {
        ExprList* designated = wrapper->compound_init;
        Type* target = initializer_designator_target(
            aggregate, designated, &path[path_index].field);
        path[path_index].aggregate = aggregate;
        path[path_index].array_index = designated->designator_index;
        path[path_index].tail = designated;
        if (path_index + 1 < path_length) {
            aggregate = target;
            wrapper = designated->expr;
        }
    }

    source = selected_item->next;
    path_index = path_length;
    while (source && source->designator_kind == INIT_DESIGNATOR_NONE &&
           source->expr) {
        InitializerDesignatorCursor* cursor = &path[path_index - 1];
        Type* next_type = initializer_designator_cursor_next(cursor);
        if (!next_type) {
            if (path_index == 1) break;
            --path_index;
            continue;
        }
        if (initializer_is_aggregate_type(next_type)) {
            ExprList* next_source;
            if (initializer_directly_initializes(next_type, source->expr)) {
                next_source = source->next;
                source->next = NULL;
                cursor->tail->next = source;
                cursor->tail = source;
                source = next_source;
                initializer_designator_cursor_advance(cursor);
                continue;
            }
            {
                ExprList* segment_last = source;
                ExprList* after_segment;
                ExprList* probe;
                ExprList* stop;
                ExprList* nested_items = NULL;
                Expr* nested;
                ExprList* appended;

                while (segment_last->next &&
                       segment_last->next->designator_kind ==
                           INIT_DESIGNATOR_NONE) {
                    segment_last = segment_last->next;
                }
                after_segment = segment_last->next;
                segment_last->next = NULL;
                probe = source;
                consume_brace_elided_subobject(next_type, &probe);
                segment_last->next = after_segment;
                if (probe == source) break;
                stop = probe ? probe : after_segment;
                for (ExprList* item = source; item != stop;
                     item = item->next) {
                    exprlist_append_designated(&nested_items, item->expr,
                                               INIT_DESIGNATOR_NONE, 0, NULL);
                }
                nested = expr_initializer_list(
                    nested_items, source->expr ? source->expr->loc
                                               : selected_item->expr->loc);
                nested->compound_type = next_type;
                nested->type = next_type;
                normalize_brace_elided_initializer(next_type, nested);
                appended = exprlist_new(nested);
                appended->designator_kind = INIT_DESIGNATOR_NONE;
                appended->designator_index = 0;
                appended->designator_field = NULL;
                cursor->tail->next = appended;
                cursor->tail = appended;
                source = stop;
                initializer_designator_cursor_advance(cursor);
                continue;
            }
        }
        {
            ExprList* next_source = source->next;
            source->next = NULL;
            cursor->tail->next = source;
            cursor->tail = source;
            source = next_source;
            initializer_designator_cursor_advance(cursor);
        }
    }
    if (source != selected_item->next) selected_item->next = source;
    rcc_free(path);
}

/* C++20 designated initialization is narrower than the C designator
 * grammar shared by the parser: only direct non-static data members may be
 * named, names must follow declaration order, and one initializer list
 * cannot mix designated and positional clauses.  This check runs before
 * any C++ default-member normalization can reorder synthesized clauses. */
static void sema_validate_cxx_designated_initializer(
    Type* type, Expr* initializer) {
    bool saw_designated = false;
    bool saw_positional = false;
    int previous_index = -1;

    if (!rcc_parser_is_cxx_mode() || !type || !type->cxx_class ||
        (type->kind != TYPE_STRUCT && type->kind != TYPE_UNION) ||
        !initializer || initializer->kind != EXPR_COMPOUND ||
        initializer->compound_cxx_default_member_normalized) {
        return;
    }
    for (ExprList* item = initializer->compound_init; item;
         item = item->next) {
        if (item->designator_kind == INIT_DESIGNATOR_INDEX) {
            rcc_error(item->expr ? item->expr->loc : initializer->loc,
                      "C++ designated initializer cannot use an array designator");
            continue;
        }
        if (item->designator_kind == INIT_DESIGNATOR_FIELD) {
            int index = 0;
            TypeField* field = initializer_field(type, item->designator_field);
            for (TypeField* cursor = type->fields; cursor;
                 cursor = cursor->next, ++index) {
                if (cursor == field) break;
            }
            if (saw_positional) {
                rcc_error(item->expr ? item->expr->loc : initializer->loc,
                          "C++ designated initializer cannot follow a positional initializer");
            }
            if (!field) continue;
            if (field->cxx_access != 0u) {
                rcc_error(item->expr ? item->expr->loc : initializer->loc,
                          "C++ designated initializer names an inaccessible member '%s'",
                          item->designator_field);
            }
            if (type->kind == TYPE_UNION && saw_designated) {
                rcc_error(item->expr ? item->expr->loc : initializer->loc,
                          "C++ union designated initializer may name only one member");
            } else if (index <= previous_index) {
                rcc_error(item->expr ? item->expr->loc : initializer->loc,
                          "C++ designated initializers must follow declaration order");
            }
            previous_index = index;
            saw_designated = true;
        } else {
            if (saw_designated) {
                rcc_error(item->expr ? item->expr->loc : initializer->loc,
                          "C++ designated initializer cannot mix designated and positional initializers");
            }
            saw_positional = true;
        }
    }
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
    if (type->kind == TYPE_VECTOR) {
        if (type->array_len <= 0 || !type->base) {
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

static void normalize_designated_brace_elision(Type* type,
                                                Expr* initializer);

static void normalize_brace_elided_initializer(Type* type,
                                                Expr* initializer) {
    ExprList* source;
    ExprList* normalized = NULL;
    bool plain_sequence = true;

    if (!type || !initializer || initializer->kind != EXPR_COMPOUND ||
        (type->kind != TYPE_ARRAY && type->kind != TYPE_VECTOR &&
         type->kind != TYPE_STRUCT &&
         type->kind != TYPE_UNION)) {
        return;
    }
    plain_sequence = initializer_is_plain_sequence(initializer);
    if (!plain_sequence) {
        normalize_designated_brace_elision(type, initializer);
        return;
    }

    source = initializer->compound_init;
    if (type->kind == TYPE_VECTOR) {
        for (int index = 0; index < type->array_len && source; ++index) {
            exprlist_append_designated(&normalized, source->expr,
                                       INIT_DESIGNATOR_NONE, 0, NULL);
            source = source->next;
        }
    } else if (type->kind == TYPE_ARRAY) {
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

static void absorb_nested_designator_followups(Type* type,
                                                Expr* initializer) {
    if (rcc_parser_is_cxx_mode() || !type || !initializer ||
        initializer->kind != EXPR_COMPOUND) {
        return;
    }
    for (ExprList* item = initializer->compound_init; item;
         item = item->next) {
        Type* selected_type = NULL;
        if (item->designator_kind == INIT_DESIGNATOR_INDEX &&
            type->kind == TYPE_ARRAY) {
            selected_type = initializer_designator_target(type, item, NULL);
        } else if (item->designator_kind == INIT_DESIGNATOR_FIELD &&
                   (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION)) {
            selected_type = initializer_designator_target(type, item, NULL);
        }
        if (selected_type) {
            initializer_absorb_designator_followups(selected_type, item);
        }
    }
}

static void normalize_designated_brace_elision(Type* type,
                                                Expr* initializer) {
    ExprList* source;
    ExprList* normalized = NULL;
    TypeField* field_cursor;
    int64_t array_cursor = 0;
    bool has_designator = false;

    if (!type || !initializer || initializer->kind != EXPR_COMPOUND ||
        (type->kind != TYPE_ARRAY && type->kind != TYPE_STRUCT &&
         type->kind != TYPE_UNION)) {
        return;
    }
    for (ExprList* item = initializer->compound_init; item;
         item = item->next) {
        if (item->designator_kind != INIT_DESIGNATOR_NONE) {
            has_designator = true;
            break;
        }
    }
    if (!has_designator) return;

    source = initializer->compound_init;
    field_cursor = type->fields;
    while (source) {
        ExprList* item = source;
        ExprList* next_source = item->next;
        TypeField* selected_field = NULL;
        Type* selected_type = NULL;
        bool positional = item->designator_kind == INIT_DESIGNATOR_NONE;

        if (type->kind == TYPE_ARRAY) {
            if (item->designator_kind == INIT_DESIGNATOR_INDEX) {
                array_cursor = item->designator_index;
                selected_type = initializer_designator_target(type, item,
                                                               NULL);
            } else if (positional && type->base) {
                selected_type = type->base;
            }
        } else if (item->designator_kind == INIT_DESIGNATOR_FIELD) {
            selected_type = initializer_designator_target(
                type, item, &selected_field);
            field_cursor = selected_field;
        } else if (positional && field_cursor) {
            selected_field = field_cursor;
            selected_type = selected_field->type;
        }

        if (positional && selected_type &&
            initializer_is_aggregate_type(selected_type) && item->expr &&
            !initializer_directly_initializes(selected_type, item->expr)) {
            ExprList* segment_last = source;
            ExprList* after_segment;
            ExprList* probe;
            ExprList* stop;
            ExprList* nested_items = NULL;
            Expr* nested;

            while (segment_last->next &&
                   segment_last->next->designator_kind ==
                       INIT_DESIGNATOR_NONE) {
                segment_last = segment_last->next;
            }
            after_segment = segment_last->next;
            segment_last->next = NULL;
            probe = source;
            consume_brace_elided_subobject(selected_type, &probe);
            segment_last->next = after_segment;
            if (probe == source) {
                exprlist_append_designated(&normalized, item->expr,
                                           item->designator_kind,
                                           item->designator_index,
                                           item->designator_field);
                source = next_source;
            } else {
                stop = probe ? probe : after_segment;
                while (source != stop) {
                    ExprList* consumed_next = source->next;
                    exprlist_append_designated(&nested_items, source->expr,
                                               INIT_DESIGNATOR_NONE, 0, NULL);
                    source = consumed_next;
                }
                nested = expr_initializer_list(nested_items,
                                               initializer->loc);
                nested->compound_type = selected_type;
                nested->type = selected_type;
                normalize_brace_elided_initializer(selected_type, nested);
                exprlist_append_designated(&normalized, nested,
                                           INIT_DESIGNATOR_NONE, 0, NULL);
            }
        } else {
            exprlist_append_designated(&normalized, item->expr,
                                       item->designator_kind,
                                       item->designator_index,
                                       item->designator_field);
            source = next_source;
        }

        if (type->kind == TYPE_ARRAY) {
            if (item->designator_kind == INIT_DESIGNATOR_INDEX) {
                if (array_cursor < INT64_MAX) ++array_cursor;
            } else if (positional && array_cursor < INT64_MAX) {
                ++array_cursor;
            }
        } else if (selected_field) {
            field_cursor = type->kind == TYPE_UNION
                ? NULL : selected_field->next;
        }
    }
    initializer->compound_init = normalized;
}

static void sema_initializer(Type* type, Expr* initializer) {
    Expr* string;
    Expr* string_literal;
    if (!type || type->cxx_dependent || !initializer) return;
    sema_validate_cxx_designated_initializer(type, initializer);
    /* In C++ a parenthesized constructor argument list is stored in the
     * same compound node as a braced aggregate initializer.  Brace elision
     * must not rewrite that list into nested class objects before overload
     * resolution; doing so changes `Outer(&value, &value)` into two aggregate
     * initializers and loses the original argument types. */
    if (!(rcc_parser_is_cxx_mode() && type->cxx_class &&
          (type->cxx_class->has_user_constructor ||
           initializer->compound_paren_init))) {
        absorb_nested_designator_followups(type, initializer);
        normalize_brace_elided_initializer(type, initializer);
    }
    string_literal = initializer_string_literal(initializer);
    if (rcc_parser_is_cxx_mode() && type->kind == TYPE_ARRAY &&
        type->base && string_literal &&
        type->base->kind == TYPE_CHAR) {
        bool expects_utf8 = type->base->is_unsigned;
        if (expects_utf8 != string_literal->is_cxx_utf8_literal) {
            rcc_error(string_literal->loc,
                      "C++ character array initializer encoding does not match the element type");
            return;
        }
    }
    string = initializer_character_string(type, initializer);
    if (string) {
        sema_expr(string);
        initializer->type = type;
        return;
    }
    if (rcc_parser_is_cxx_mode() && type->kind == TYPE_PTR &&
        type->is_reference) {
        Expr* binding_expression = initializer;
        if (initializer->kind == EXPR_COMPOUND &&
            !initializer->compound_type) {
            ExprList* item = initializer->compound_init;
            if (!item || item->next ||
                item->designator_kind != INIT_DESIGNATOR_NONE ||
                !item->expr) {
                rcc_error(initializer->loc,
                          "C++ reference initializer list requires exactly one element");
                return;
            }
            binding_expression = item->expr;
        }
        sema_expr(binding_expression);
        if (!implicit_cast(binding_expression, type)) {
            rcc_error(initializer->loc,
                      "invalid C++ reference binding");
            return;
        }
        if (binding_expression != initializer) {
            *initializer = *binding_expression;
        }
        return;
    }
    if (initializer->kind != EXPR_COMPOUND) {
        Expr* contextual = initializer->kind == EXPR_ADDR
            ? initializer->unary_operand : initializer;
        bool contextual_function_pointer =
            sema_cxx_select_function_pointer_overload(type, contextual);
        if (contextual_function_pointer && contextual->type == type_int) {
            initializer->type = type_int;
            return;
        }
        if (!contextual_function_pointer || initializer->kind == EXPR_ADDR) {
            sema_expr(initializer);
        }
        if (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) {
            if (rcc_parser_is_cxx_mode() && type->cxx_class &&
                initializer->type && type_is_compatible(type, initializer->type) &&
                !sema_cxx_trivially_copyable(type, 0) &&
                sema_cxx_type_has_destructor_cleanup(type, 0)) {
                rcc_error(initializer->loc,
                          "C++ scope-cleanup object requires a validated direct constructor");
                return;
            }
            if (!type_is_compatible(type, initializer->type)) {
                rcc_error(initializer->loc,
                          "incompatible aggregate copy initialization");
            }
            return;
        }
        if (type->kind == TYPE_VECTOR) {
            if (!type_is_compatible(type, initializer->type)) {
                rcc_error(initializer->loc,
                          "incompatible vector copy initialization");
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
            } else if (rcc_parser_is_cxx_mode() &&
                       ((initializer->type &&
                         initializer->type->kind == TYPE_PTR &&
                         initializer->type->cxx_is_member_pointer) ||
                        (type->kind == TYPE_PTR &&
                         type->cxx_is_member_pointer))) {
                rcc_error(initializer->loc,
                          "invalid pointer-to-member conversion in initialization");
            } else {
                rcc_warning(initializer->loc,
                            "incompatible types in initialization");
            }
        }
        return;
    }
    initializer->type = type;
    /* A class with an ordinary converting constructor still receives an
     * implicit copy constructor.  Treat a same-type one-argument initializer
     * as that copy only after proving the complete object is trivially
     * copyable; otherwise the normal constructor overload path remains in
     * charge and reports unsupported user-defined copy semantics. */
    if (rcc_parser_is_cxx_mode() && type->cxx_class &&
        (type->kind == TYPE_STRUCT || type->kind == TYPE_UNION) &&
        initializer->compound_init &&
        !initializer->compound_init->next &&
        initializer->compound_init->designator_kind ==
            INIT_DESIGNATOR_NONE && initializer->compound_init->expr) {
        Expr* source = initializer->compound_init->expr;
        sema_expr(source);
        if (source->type && type_is_compatible(type, source->type) &&
            sema_cxx_trivially_copyable(type, 0)) {
            initializer->compound_copy_init = true;
            return;
        }
    }
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
        initializer->compound_constructor = sema_select_cxx_new_constructor_ex(
            type, &constructor_arguments, initializer->loc,
            !initializer->compound_copy_init);
        if (!initializer->compound_constructor) {
            rcc_error(initializer->loc,
                      "no safely lowerable constructor accepts the C++ initializer");
        }
        if (constructor_arguments != initializer->compound_init) {
            initializer->compound_init = constructor_arguments;
            initializer->compound_value_init = constructor_arguments == NULL;
        }
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
    if (type->kind == TYPE_VECTOR) {
        int64_t cursor = 0;
        for (ExprList* item = initializer->compound_init; item;
             item = item->next, ++cursor) {
            if (item->designator_kind != INIT_DESIGNATOR_NONE ||
                cursor >= type->array_len) {
                rcc_error(item->expr->loc,
                          "vector initializer has an invalid lane");
                continue;
            }
            sema_initializer(type->base, item->expr);
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
                initializer_absorb_designator_followups(type->base, item);
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
            initializer_absorb_designator_followups(field->type, item);
            sema_initializer(field->type, item->expr);
            cursor = field->next;
            ++initialized;
        }
        if (rcc_parser_is_cxx_mode() && type->cxx_class) {
            initializer->compound_constructor =
                sema_select_cxx_new_constructor(
                    type, &initializer->compound_init, initializer->loc);
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
        return type_reference(
            deduced,
            declaration->var_is_auto_rvalue_reference && !binds_lvalue);
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
    /* An explicit destructor is the complete-object cleanup callback.  It
     * already contains the validated wrapper operation and can therefore be
     * registered in the runtime exception frame; the compiler-side wrapper
     * expression is only the fallback for classes without such a body. */
    if (sema_cxx_destructor_function(declaration->type)) return;
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
    Decl* declaration;
    CxxCleanupPlan* cleanup_item;
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
            declaration = statement->decl;
            if (declaration && declaration->kind == DECL_VAR &&
                declaration->var_cleanup) {
                SemaCleanupPath* path = rcc_alloc(sizeof(*path));
                path->previous = *active;
                path->allocation_next = context->allocations;
                context->allocations = path;
                *active = path;
            }
            if (declaration && declaration->kind == DECL_VAR) {
                for (cleanup_item = declaration->var_cleanups;
                     cleanup_item; cleanup_item = cleanup_item->next) {
                    SemaCleanupPath* path = rcc_alloc(sizeof(*path));
                    path->previous = *active;
                    path->allocation_next = context->allocations;
                    context->allocations = path;
                    *active = path;
                }
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
        result->compound_cxx_default_member_normalized = true;
        return result;
    }
}

static void sema_cxx_synthesize_default_constructor_initializer(
    Decl* declaration) {
    Type* type = declaration ? declaration->type : NULL;
    CxxClass* cls = type ? type->cxx_class : NULL;
    Expr* initializer;
    if (!declaration || !type || !cls || !cls->has_user_constructor ||
        declaration->var_init || declaration->storage == STORAGE_EXTERN) {
        return;
    }
    initializer = expr_initializer_list(NULL, declaration->loc);
    initializer->compound_type = type;
    declaration->var_init = initializer;
}

static void sema_cxx_wrap_copy_constructor_initializer(Decl* declaration) {
    Type* type = declaration ? declaration->type : NULL;
    CxxClass* cls = type ? type->cxx_class : NULL;
    Expr* source;
    Expr* initializer;
    if (!declaration || !type || !cls || !cls->has_user_constructor ||
        !declaration->var_init ||
        declaration->var_init->kind == EXPR_COMPOUND) {
        return;
    }
    source = declaration->var_init;
    sema_expr(source);
    if (source->type && type_is_compatible(source->type, type)) return;
    initializer = expr_initializer_list(exprlist_new(source), source->loc);
    initializer->compound_type = type;
    initializer->compound_copy_init = true;
    declaration->var_init = initializer;
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

    if (sema_decltype_auto_expression_is_xvalue(expression)) {
        if (result->kind == TYPE_PTR && result->is_reference) return result;
        return type_reference(result, true);
    }

    /* These expression forms are lvalues.  Preserve that category for the
     * lowered reference ABI instead of silently copying the object value. */
    if ((expression->cxx_parenthesized &&
         sema_decltype_auto_expression_is_lvalue(expression)) ||
        (expression->kind == EXPR_COND &&
         expression->cxx_conditional_lvalue) ||
        expression->kind == EXPR_DEREF || expression->kind == EXPR_INDEX ||
        (expression->kind == EXPR_MEMBER &&
         !expression->cxx_member_xvalue) ||
        expression->kind == EXPR_PTR_MEMBER ||
        expression->kind == EXPR_STRING_LIT) {
        if (result->kind == TYPE_PTR && result->is_reference) return result;
        return type_reference(result, false);
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

static void sema_resolve_function_noexcept(Decl* declaration) {
    SemaConstexprScalar value;
    if (!declaration || !declaration->func_noexcept_expr) return;
    sema_expr(declaration->func_noexcept_expr);
    if (sema_eval_constexpr_scalar_expr(
            declaration->func_noexcept_expr, NULL, 0, &value) &&
        !value.is_floating) {
        declaration->func_is_noexcept = sema_constexpr_scalar_truth(&value);
    } else {
        /* The existing frontend accepts dependent/non-constant exception
         * specifications for later template resolution.  Until a concrete
         * constant value is available, retain the standard potentially-
         * throwing interpretation rather than treating it as noexcept. */
        declaration->func_is_noexcept = false;
    }
}

/* A generic lambda has no single function-pointer type until its call
 * operator is instantiated.  The bounded lowering supports captureless
 * variables that are directly invoked; reject an unused or escaped dependent
 * closure before code generation instead of emitting an unresolved template
 * symbol. */
static void sema_validate_stored_generic_lambda_stmt(Stmt* statement) {
    if (!statement) return;
    switch (statement->kind) {
        case STMT_BLOCK:
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                sema_validate_stored_generic_lambda_stmt(item->stmt);
            }
            break;
        case STMT_IF:
            sema_validate_stored_generic_lambda_stmt(statement->if_then);
            sema_validate_stored_generic_lambda_stmt(statement->if_else);
            break;
        case STMT_WHILE:
        case STMT_DO:
            sema_validate_stored_generic_lambda_stmt(statement->while_body);
            break;
        case STMT_FOR:
            sema_validate_stored_generic_lambda_stmt(statement->for_init);
            sema_validate_stored_generic_lambda_stmt(statement->for_body);
            break;
        case STMT_SWITCH:
            sema_validate_stored_generic_lambda_stmt(statement->switch_body);
            break;
        case STMT_CASE:
            sema_validate_stored_generic_lambda_stmt(statement->case_stmt);
            break;
        case STMT_DEFAULT:
            sema_validate_stored_generic_lambda_stmt(statement->default_stmt);
            break;
        case STMT_LABEL:
            sema_validate_stored_generic_lambda_stmt(statement->label_stmt);
            break;
        case STMT_TRY:
            sema_validate_stored_generic_lambda_stmt(statement->try_body);
            for (CxxCatch* handler = statement->try_catches; handler;
                 handler = handler->next) {
                sema_validate_stored_generic_lambda_stmt(handler->body);
            }
            break;
        case STMT_DECL:
            if (statement->decl && statement->decl->kind == DECL_VAR &&
                statement->decl->var_cxx_lambda_template &&
                !statement->decl->var_cxx_lambda_specialized) {
                rcc_error(statement->decl->loc,
                          "stored generic lambda must be directly invoked "
                          "in the bounded RCC++ profile");
            }
            break;
        default:
            break;
    }
}

static Type* sema_deduce_decltype_auto_type(Decl* declaration) {
    Type* deduced;
    if (!declaration || !declaration->var_init) {
        rcc_error(declaration ? declaration->loc : (SourceLoc){0},
                  "decltype(auto) variable requires an initializer");
        return type_int;
    }
    deduced = sema_decltype_auto_return_type(declaration->var_init);
    if (!deduced || deduced->kind == TYPE_VOID) {
        rcc_error(declaration->loc,
                  "decltype(auto) initializer does not have an object type");
        return type_int;
    }
    return deduced;
}

static void sema_validate_stored_generic_lambda_decl(Decl* declaration) {
    if (!declaration) return;
    if (declaration->kind == DECL_VAR &&
        declaration->var_cxx_lambda_template &&
        !declaration->var_cxx_lambda_specialized) {
        rcc_error(declaration->loc,
                  "stored generic lambda must be directly invoked in the "
                  "bounded RCC++ profile");
    } else if (declaration->kind == DECL_FUNC) {
        sema_validate_stored_generic_lambda_stmt(declaration->func_body);
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
                if (decl->var_init && decl->var_init->kind == EXPR_IDENT &&
                    decl->var_init->cxx_lambda_template &&
                    !decl->var_init->cxx_lambda_captures) {
                    decl->var_cxx_lambda_template =
                        decl->var_init->cxx_lambda_template;
                }
                decl->type = sema_deduce_auto_type(decl);
            } else if (decl->var_is_decltype_auto) {
                decl->type = sema_deduce_decltype_auto_type(decl);
            }
            {
                Expr* default_initializer =
                    sema_cxx_default_member_initializer(decl);
                if (default_initializer) decl->var_init = default_initializer;
            }
            if (rcc_parser_is_cxx_mode()) {
                sema_cxx_synthesize_default_constructor_initializer(decl);
                sema_cxx_wrap_copy_constructor_initializer(decl);
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
            sema_validate_restrict_type(decl->type, decl->loc);
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
            if (decl->is_weak &&
                (!decl->var_is_global ||
                 decl->storage == STORAGE_STATIC ||
                 decl->storage == STORAGE_AUTO ||
                 decl->storage == STORAGE_REGISTER)) {
                rcc_error(decl->loc,
                          "weak variable declaration requires external linkage");
            }
            if (decl->var_is_vla) {
                int word_size = g_opts.target_arch == ARCH_X64 ? 8 : 4;
                decl->var_vla_size_offset = decl->var_offset + word_size;
                decl->var_vla_extent_offset = decl->var_offset +
                    2 * word_size;
                decl->var_vla_extent_count =
                    sema_vla_dimension_count(decl->type);
            }

            if (decl->var_is_constinit) {
                bool has_static_duration =
                    is_global || decl->var_is_thread_local ||
                    decl->storage == STORAGE_STATIC ||
                    decl->storage == STORAGE_EXTERN;
                if (!has_static_duration) {
                    rcc_error(decl->loc,
                              "constinit variable requires static or thread storage duration");
                }
                if (decl->var_is_constexpr) {
                    rcc_error(decl->loc,
                              "constinit cannot be combined with constexpr");
                }
            }

            if (decl->var_init) {
                sema_initializer(decl->type, decl->var_init);
                if (decl->var_is_constinit) {
                    bool valid_constinit = false;
                    if (sema_constexpr_scalar_type(decl->type)) {
                        SemaConstexprScalar value;
                        valid_constinit =
                            sema_eval_constexpr_scalar_object(
                                decl->type, decl->var_init, NULL, 0, &value) &&
                            sema_constexpr_scalar_convert(
                                &value, decl->type, &value);
                    } else if (sema_constexpr_aggregate_type(decl->type)) {
                        valid_constinit = sema_validate_constexpr_object(
                            decl->type, decl->var_init);
                    }
                    if (!valid_constinit) {
                        rcc_error(decl->loc,
                                  "constinit variable initializer is not a supported constant expression");
                    }
                }
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
                                Expr* rebuilt = sema_constexpr_rebuild_pointer(
                                    decl->type, &folded, decl->loc);
                                /* Keep pointer constants symbolic.  This also
                                 * turns `constexpr Holder h; constexpr int*
                                 * p = h.field` into a normal relocatable
                                 * address expression instead of leaving a
                                 * byte-backed evaluator-only access in the
                                 * static initializer. */
                                if (rebuilt) *decl->var_init = *rebuilt;
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
                    } else if (sema_constexpr_aggregate_type(decl->type)) {
                        unsigned char* storage = ast_arena_alloc(
                            (size_t)decl->type->size);
                        if (sema_constexpr_materialize_object(
                                decl->type, decl->var_init, NULL, 0,
                                storage, (size_t)decl->type->size)) {
                            Expr* rebuilt = sema_constexpr_rebuild_object(
                                decl->type, storage,
                                (size_t)decl->type->size, decl->loc);
                            if (rebuilt) *decl->var_init = *rebuilt;
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
            bool saved_template_instance = current_func_template_instance;
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
            sema_validate_restrict_type(decl->type, decl->loc);
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
            if (decl->is_weak &&
                (!sym->is_global || decl->storage == STORAGE_STATIC)) {
                rcc_error(decl->loc,
                          "weak function declaration requires external linkage");
            }
            sema_resolve_function_noexcept(decl);

            if (decl->func_body) {
                sym->is_defined = true;

                /* Enter function scope */
                symtab_enter_function(g_symtab);
                current_func_ret = decl->type->ret_type;
                current_func_variadic = decl->type->variadic;
                current_func_auto_return_pending = decl->func_is_auto_return;
                current_func_template_instance =
                    decl->func_is_template_instance;
                current_func_last_param = NULL;

                /* Add parameters */
                int param_offset = 8;  /* After saved EBP and return address */
                if (g_opts.target_arch == ARCH_X86 &&
                    decl->type && decl->type->ret_type &&
                    (decl->type->ret_type->kind == TYPE_STRUCT ||
                     decl->type->ret_type->kind == TYPE_UNION ||
                     decl->type->ret_type->kind == TYPE_VECTOR)) {
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
                    if (p->decl) {
                        sema_validate_restrict_type(p->decl->type,
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
                current_func_template_instance = saved_template_instance;
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
            Symbol* previous = symtab_lookup_local(g_symtab, decl->name);
            sema_validate_array_parameter_type(decl->typedef_type,
                                               decl->loc, false);
            sema_validate_restrict_type(decl->typedef_type, decl->loc);
            if (sema_type_is_variably_modified(decl->typedef_type)) {
                sema_vla_bounds(decl->typedef_type, decl->loc);
                if (g_symtab->current == g_symtab->global) {
                    rcc_error(decl->loc,
                              "variably modified typedef is only valid at block scope");
                }
            }
            if (previous && previous->kind == SYM_TYPE &&
                type_is_compatible(previous->type, decl->typedef_type)) {
                previous->type = decl->typedef_type;
                previous->decl = decl;
            } else {
                symtab_define(g_symtab, decl->name, SYM_TYPE,
                              decl->typedef_type, decl->loc);
            }
            break;
        }

        case DECL_STRUCT:
        case DECL_UNION: {
            SymKind k = (decl->kind == DECL_STRUCT) ? SYM_STRUCT : SYM_UNION;
            for (TypeField* field = decl->type ? decl->type->fields : NULL;
                 field; field = field->next) {
                sema_validate_restrict_type(field->type, decl->loc);
            }
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
    current_ast = ast;
    current_cxx_namespace = NULL;
    current_func_template_instance = false;
    static_local_counter = 0u;
    cxx_exception_frame_counter = 0u;

    /* Process all top-level declarations */
    for (DeclList* d = ast->decls; d; d = d->next) {
        sema_decl(d->decl);
    }
    for (DeclList* d = ast->decls; d; d = d->next) {
        sema_validate_stored_generic_lambda_decl(d->decl);
    }

    valid = g_error_count == 0;
    symtab_free(g_symtab);
    g_symtab = NULL;
    current_cxx_namespace = NULL;
    current_ast = NULL;
    return valid;
}
