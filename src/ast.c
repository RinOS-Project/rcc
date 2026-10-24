/*
 * RCC - RinOS C Compiler
 * AST construction utilities
 */

#include "rcc.h"
#include "ast.h"
#include "ast_cxx.h"
#include <limits.h>

#define AST_ARENA_BLOCK_SIZE (64u * 1024u)

typedef struct AstArenaBlock {
    struct AstArenaBlock* next;
    size_t used;
    size_t capacity;
    max_align_t alignment;
    unsigned char data[];
} AstArenaBlock;

static AstArenaBlock* ast_arena_blocks;
static bool ast_arena_exit_registered;

static void ast_arena_reset(void) {
    AstArenaBlock* block = ast_arena_blocks;
    while (block) {
        AstArenaBlock* next = block->next;
        rcc_free(block);
        block = next;
    }
    ast_arena_blocks = NULL;
}

static void ast_arena_reset_at_exit(void) {
    ast_arena_reset();
}

void* ast_arena_alloc(size_t size) {
    const size_t alignment = _Alignof(max_align_t);
    AstArenaBlock* block = ast_arena_blocks;
    size_t aligned_used;
    size_t capacity;
    if (size == 0u) size = 1u;
    if (size > SIZE_MAX - alignment) rcc_fatal("AST allocation is too large");
    aligned_used = block ? (block->used + alignment - 1u) & ~(alignment - 1u)
                         : 0u;
    if (!block || aligned_used > block->capacity ||
        size > block->capacity - aligned_used) {
        capacity = size > AST_ARENA_BLOCK_SIZE ? size : AST_ARENA_BLOCK_SIZE;
        if (capacity > SIZE_MAX - sizeof(*block)) {
            rcc_fatal("AST arena block is too large");
        }
        block = rcc_alloc(sizeof(*block) + capacity);
        block->next = ast_arena_blocks;
        block->capacity = capacity;
        ast_arena_blocks = block;
        aligned_used = 0u;
        if (!ast_arena_exit_registered) {
            if (atexit(ast_arena_reset_at_exit) != 0) {
                rcc_fatal("cannot register AST arena cleanup");
            }
            ast_arena_exit_registered = true;
        }
    }
    block->used = aligned_used + size;
    return block->data + aligned_used;
}

void* ast_arena_grow(void* pointer, size_t old_size, size_t new_size) {
    void* replacement = ast_arena_alloc(new_size);
    if (pointer && old_size != 0u) {
        memcpy(replacement, pointer, old_size < new_size ? old_size : new_size);
    }
    return replacement;
}

char* ast_arena_strdup(const char* text) {
    size_t length;
    char* copy;
    if (!text) return NULL;
    length = strlen(text) + 1u;
    copy = ast_arena_alloc(length);
    memcpy(copy, text, length);
    return copy;
}

/* Every allocation below belongs to the current translation unit. */
#define rcc_alloc ast_arena_alloc

/* ═══════════════════════════════════════
 * Built-in Types
 * ═══════════════════════════════════════ */

#define BUILTIN_TYPE(type_kind, type_size, type_align, unsigned_type) \
    { .kind = (type_kind), .size = (type_size), .align = (type_align), \
      .has_explicit_alignment = false, \
      .is_unsigned = (unsigned_type), .is_plain_char = false, \
      .is_char8 = false, .is_wchar = false, \
      .is_char16 = false, .is_char32 = false, \
      .is_const = false, \
      .is_volatile = false, .is_atomic = false, .cxx_is_class = false, \
      .cxx_nontrivial = false, .cxx_class = NULL, \
      .cxx_namespace = NULL, .cxx_scope_identity = NULL, \
      .cxx_vtable_size = 0, \
      .cxx_vtable_symbol = NULL, .cxx_typeinfo_symbol = NULL }

static Type builtin_void   = BUILTIN_TYPE(TYPE_VOID,   0, 1, false);
static Type builtin_bool   = BUILTIN_TYPE(TYPE_BOOL,   1, 1, true);
static Type builtin_char   = {
    .kind = TYPE_CHAR, .size = 1, .align = 1, .is_plain_char = true
};
static Type builtin_schar  = BUILTIN_TYPE(TYPE_CHAR,   1, 1, false);
static Type builtin_char8  = {
    .kind = TYPE_CHAR, .size = 1, .align = 1, .is_unsigned = true,
    .is_char8 = true
};
static Type builtin_wchar = {
    .kind = TYPE_INT, .size = 4, .align = 4, .is_wchar = true
};
static Type builtin_char16 = {
    .kind = TYPE_SHORT, .size = 2, .align = 2, .is_unsigned = true,
    .is_char16 = true
};
static Type builtin_char32 = {
    .kind = TYPE_INT, .size = 4, .align = 4, .is_unsigned = true,
    .is_char32 = true
};
static Type builtin_short  = BUILTIN_TYPE(TYPE_SHORT,  2, 2, false);
static Type builtin_int    = BUILTIN_TYPE(TYPE_INT,    4, 4, false);
static Type builtin_long   = BUILTIN_TYPE(TYPE_LONG,   4, 4, false); /* ILP32/LP64 */
static Type builtin_llong  = BUILTIN_TYPE(TYPE_LLONG,  8, 8, false);
static Type builtin_uchar  = BUILTIN_TYPE(TYPE_CHAR,   1, 1, true);
static Type builtin_ushort = BUILTIN_TYPE(TYPE_SHORT,  2, 2, true);
static Type builtin_uint   = BUILTIN_TYPE(TYPE_INT,    4, 4, true);
static Type builtin_ulong  = BUILTIN_TYPE(TYPE_LONG,   4, 4, true);
static Type builtin_ullong = BUILTIN_TYPE(TYPE_LLONG,  8, 8, true);
static Type builtin_float  = BUILTIN_TYPE(TYPE_FLOAT,  4, 4, false);
static Type builtin_double = BUILTIN_TYPE(TYPE_DOUBLE, 8, 8, false);
static Type builtin_nullptr = BUILTIN_TYPE(TYPE_NULLPTR, 4, 4, false);

#undef BUILTIN_TYPE

Type* type_void   = &builtin_void;
Type* type_bool   = &builtin_bool;
Type* type_char   = &builtin_char;
Type* type_schar  = &builtin_schar;
Type* type_char8  = &builtin_char8;
Type* type_wchar = &builtin_wchar;
Type* type_char16 = &builtin_char16;
Type* type_char32 = &builtin_char32;
Type* type_short  = &builtin_short;
Type* type_int    = &builtin_int;
Type* type_long   = &builtin_long;
Type* type_llong  = &builtin_llong;
Type* type_uchar  = &builtin_uchar;
Type* type_ushort = &builtin_ushort;
Type* type_uint   = &builtin_uint;
Type* type_ulong  = &builtin_ulong;
Type* type_ullong = &builtin_ullong;
Type* type_float  = &builtin_float;
Type* type_double = &builtin_double;
Type* type_nullptr = &builtin_nullptr;
static Type* builtin_cxx_type_info;

void type_configure_target(TargetArch architecture) {
    int long_size = architecture == ARCH_X64 ? 8 : 4;
    builtin_long.size = long_size;
    builtin_long.align = long_size;
    builtin_ulong.size = long_size;
    builtin_ulong.align = long_size;
    builtin_nullptr.size = long_size;
    builtin_nullptr.align = long_size;
    if (builtin_cxx_type_info) {
        builtin_cxx_type_info->size = long_size;
        builtin_cxx_type_info->align = long_size;
    }
}

Type* rcc_cxx_type_info_type(void) {
    if (!builtin_cxx_type_info) {
        builtin_cxx_type_info = type_struct("__rcc_type_info");
        builtin_cxx_type_info->size = g_opts.target_arch == ARCH_X64 ? 8 : 4;
        builtin_cxx_type_info->align = builtin_cxx_type_info->size;
        builtin_cxx_type_info->is_complete = true;
        builtin_cxx_type_info->is_const = true;
    }
    return builtin_cxx_type_info;
}

/* ═══════════════════════════════════════
 * Type Constructors
 * ═══════════════════════════════════════ */

Type* type_ptr(Type* base) {
    Type* t = rcc_alloc(sizeof(Type));
    t->kind = TYPE_PTR;
    t->size = g_opts.target_arch == ARCH_X64 ? 8 : 4;
    t->align = t->size;
    t->has_explicit_alignment = false;
    t->is_atomic = false;
    t->cxx_is_member_pointer = false;
    t->cxx_member_pointer_owner = NULL;
    t->base = base;
    t->array_bound = NULL;
    t->array_unspecified_bound = false;
    t->array_parameter_static = false;
    t->array_parameter_const = false;
    t->array_parameter_volatile = false;
    t->array_parameter_restrict = false;
    t->cxx_is_class = false;
    t->cxx_nontrivial = false;
    t->cxx_dependent = false;
    t->cxx_dependent_alias_template = false;
    t->cxx_class = NULL;
    t->cxx_namespace = NULL;
    t->cxx_scope_identity = NULL;
    t->cxx_vtable_size = 0;
    t->cxx_vtable_symbol = NULL;
    t->cxx_typeinfo_symbol = NULL;
    return t;
}

void type_cxx_member_pointer(Type* pointer, Type* owner) {
    int word_size = g_opts.target_arch == ARCH_X64 ? 8 : 4;
    if (!pointer || pointer->kind != TYPE_PTR || !owner) return;
    pointer->cxx_is_member_pointer = true;
    pointer->cxx_member_pointer_owner = owner;
    pointer->size = pointer->base && pointer->base->kind == TYPE_FUNC
        ? word_size * 2 : word_size;
    pointer->align = word_size;
}

bool type_is_cxx_member_function_pointer(const Type* type) {
    return type && type->kind == TYPE_PTR &&
           type->cxx_is_member_pointer && type->base &&
           type->base->kind == TYPE_FUNC;
}

Type* type_reference(Type* base, bool rvalue_reference) {
    Type* reference;
    while (base && base->kind == TYPE_PTR && base->is_reference) {
        /* `&` dominates `&&`; only && combined with && remains &&. */
        rvalue_reference = rvalue_reference && base->is_rvalue_reference;
        base = base->base;
    }
    if (base == NULL) return NULL;
    reference = type_ptr(base);
    reference->is_reference = true;
    reference->is_rvalue_reference = rvalue_reference;
    return reference;
}

Type* type_array(Type* base, int len) {
    Type* t = rcc_alloc(sizeof(Type));
    t->kind = TYPE_ARRAY;
    t->size = len > 0 ? base->size * len : 0;
    t->align = base->align;
    t->has_explicit_alignment = false;
    t->is_atomic = false;
    t->base = base;
    t->array_len = len;
    t->array_bound = NULL;
    t->array_unspecified_bound = false;
    t->array_parameter_static = false;
    t->array_parameter_const = false;
    t->array_parameter_volatile = false;
    t->array_parameter_restrict = false;
    t->cxx_is_class = false;
    t->cxx_nontrivial = false;
    t->cxx_dependent = false;
    t->cxx_dependent_alias_template = false;
    t->cxx_class = NULL;
    t->cxx_namespace = NULL;
    t->cxx_scope_identity = NULL;
    t->cxx_vtable_size = 0;
    t->cxx_vtable_symbol = NULL;
    t->cxx_typeinfo_symbol = NULL;
    return t;
}

Type* type_vector(Type* base, int lanes, int vector_size) {
    Type* t = rcc_alloc(sizeof(Type));
    t->kind = TYPE_VECTOR;
    t->size = vector_size;
    t->align = vector_size >= 16 ? 16 : vector_size;
    t->has_explicit_alignment = false;
    t->is_atomic = false;
    t->base = base;
    t->array_len = lanes;
    t->array_bound = NULL;
    t->array_unspecified_bound = false;
    t->array_parameter_static = false;
    t->array_parameter_const = false;
    t->array_parameter_volatile = false;
    t->array_parameter_restrict = false;
    t->cxx_is_class = false;
    t->cxx_nontrivial = false;
    t->cxx_dependent = false;
    t->cxx_class = NULL;
    t->cxx_namespace = NULL;
    t->cxx_scope_identity = NULL;
    t->cxx_vtable_size = 0;
    t->cxx_vtable_symbol = NULL;
    t->cxx_typeinfo_symbol = NULL;
    return t;
}

Type* type_func(Type* ret, TypeParam* params, bool variadic) {
    Type* t = rcc_alloc(sizeof(Type));
    t->kind = TYPE_FUNC;
    t->size = 0;
    t->align = 1;
    t->has_explicit_alignment = false;
    t->is_atomic = false;
    t->function_is_noexcept = false;
    t->function_ref_qualifier = CXX_REF_QUAL_NONE;
    t->ret_type = ret;
    t->params = params;
    t->variadic = variadic;
    t->has_prototype = true;
    t->cxx_is_class = false;
    t->cxx_nontrivial = false;
    t->cxx_dependent = false;
    t->cxx_class = NULL;
    t->cxx_namespace = NULL;
    t->cxx_scope_identity = NULL;
    t->cxx_vtable_size = 0;
    t->cxx_vtable_symbol = NULL;
    t->cxx_typeinfo_symbol = NULL;
    return t;
}

Type* type_struct(const char* tag) {
    Type* t = rcc_alloc(sizeof(Type));
    t->kind = TYPE_STRUCT;
    t->size = 0;
    t->align = 1;
    t->has_explicit_alignment = false;
    t->is_atomic = false;
    t->tag = tag;
    t->fields = NULL;
    t->is_complete = false;
    t->cxx_is_class = false;
    t->cxx_nontrivial = false;
    t->cxx_dependent = false;
    t->cxx_dependent_alias_template = false;
    t->cxx_class = NULL;
    t->cxx_namespace = NULL;
    t->cxx_scope_identity = NULL;
    t->cxx_vtable_size = 0;
    t->cxx_vtable_symbol = NULL;
    t->cxx_typeinfo_symbol = NULL;
    return t;
}

Type* type_union(const char* tag) {
    Type* t = rcc_alloc(sizeof(Type));
    t->kind = TYPE_UNION;
    t->size = 0;
    t->align = 1;
    t->has_explicit_alignment = false;
    t->is_atomic = false;
    t->tag = tag;
    t->fields = NULL;
    t->is_complete = false;
    t->cxx_is_class = false;
    t->cxx_nontrivial = false;
    t->cxx_dependent = false;
    t->cxx_dependent_alias_template = false;
    t->cxx_class = NULL;
    t->cxx_namespace = NULL;
    t->cxx_scope_identity = NULL;
    t->cxx_vtable_size = 0;
    t->cxx_vtable_symbol = NULL;
    t->cxx_typeinfo_symbol = NULL;
    return t;
}

Type* type_enum(const char* tag) {
    Type* t = rcc_alloc(sizeof(Type));
    t->kind = TYPE_ENUM;
    t->size = 4;
    t->align = 4;
    t->has_explicit_alignment = false;
    t->is_atomic = false;
    t->enum_tag = tag;
    t->enum_is_scoped = false;
    t->enum_has_fixed_underlying = false;
    t->enum_underlying_type = NULL;
    t->enum_constants = NULL;
    t->enum_constant_count = 0;
    t->cxx_is_class = false;
    t->cxx_nontrivial = false;
    t->cxx_class = NULL;
    t->cxx_namespace = NULL;
    t->cxx_scope_identity = NULL;
    t->cxx_vtable_size = 0;
    t->cxx_vtable_symbol = NULL;
    t->cxx_typeinfo_symbol = NULL;
    return t;
}

/* ═══════════════════════════════════════
 * Type Utilities
 * ═══════════════════════════════════════ */

bool type_is_integer(Type* t) {
    return t && t->kind >= TYPE_BOOL && t->kind <= TYPE_LLONG;
}

bool type_is_floating(Type* t) {
    return t && (t->kind == TYPE_FLOAT || t->kind == TYPE_DOUBLE);
}

bool type_is_arithmetic(Type* t) {
    return type_is_integer(t) || type_is_floating(t);
}

bool type_is_scalar(Type* t) {
    return t && (type_is_arithmetic(t) || t->kind == TYPE_PTR ||
           t->kind == TYPE_NULLPTR);
}

bool type_is_pointer(Type* t) {
    return t && t->kind == TYPE_PTR && !t->cxx_is_member_pointer;
}

bool type_is_array(Type* t) {
    return t && t->kind == TYPE_ARRAY;
}

bool type_is_vector(Type* t) {
    return t && t->kind == TYPE_VECTOR;
}

bool type_is_function(Type* t) {
    return t && t->kind == TYPE_FUNC;
}

bool type_is_complete(Type* t) {
    if (!t) return false;
    if (t->kind == TYPE_VOID) return false;
    if (t->kind == TYPE_ARRAY && t->array_len < 0 &&
        !t->array_bound) return false;
    if ((t->kind == TYPE_STRUCT || t->kind == TYPE_UNION) && !t->is_complete) return false;
    return true;
}

static bool cxx_template_class_instances_compatible(
    const CxxClass* left, const CxxClass* right) {
    const CxxTemplate* left_template;
    const CxxTemplate* right_template;
    Type** left_arguments;
    Type** right_arguments;
    int left_argument_count;
    int right_argument_count;
    int64_t* left_values;
    int64_t* right_values;
    bool* left_value_present;
    bool* right_value_present;
    if (!left || !right) return false;
    left_template = left->template_identity_tmpl
        ? left->template_identity_tmpl : left->templ;
    right_template = right->template_identity_tmpl
        ? right->template_identity_tmpl : right->templ;
    if (!left_template || left_template != right_template) return false;
    left_arguments = left->template_identity_tmpl
        ? left->template_identity_args : left->template_args;
    right_arguments = right->template_identity_tmpl
        ? right->template_identity_args : right->template_args;
    left_argument_count = left->template_identity_tmpl
        ? left->template_identity_arg_count : left->template_arg_count;
    right_argument_count = right->template_identity_tmpl
        ? right->template_identity_arg_count : right->template_arg_count;
    left_values = left->template_identity_tmpl
        ? left->template_identity_value_args : left->template_value_args;
    right_values = right->template_identity_tmpl
        ? right->template_identity_value_args : right->template_value_args;
    left_value_present = left->template_identity_tmpl
        ? left->template_identity_value_present : left->template_value_present;
    right_value_present = right->template_identity_tmpl
        ? right->template_identity_value_present : right->template_value_present;
    if (left_argument_count < 0 ||
        left_argument_count != right_argument_count) {
        return false;
    }
    for (int index = 0; index < left_argument_count; ++index) {
        int parameter_index = index < left_template->param_count
            ? index : left_template->param_count - 1;
        const TemplateParam* parameter;
        if (parameter_index < 0 || !left_template->params) return false;
        parameter = &left_template->params[parameter_index];
        if (index >= left_template->param_count && !parameter->is_pack) {
            return false;
        }
        if (parameter->kind == TPARAM_TYPE) {
            if (!left_arguments || !right_arguments ||
                !type_is_same_template_argument(left_arguments[index],
                                                right_arguments[index])) {
                return false;
            }
        } else if (parameter->kind == TPARAM_NONTYPE) {
            if (!left_values || !right_values || !left_value_present ||
                !right_value_present || !left_value_present[index] ||
                !right_value_present[index] ||
                left_values[index] != right_values[index]) {
                return false;
            }
        } else if (parameter->kind == TPARAM_TEMPLATE) {
            if (!left_arguments || !right_arguments ||
                !left_arguments[index] || !right_arguments[index] ||
                left_arguments[index]->cxx_template !=
                    right_arguments[index]->cxx_template) {
                return false;
            }
        } else {
            return false;
        }
    }
    return true;
}

bool type_is_compatible(Type* a, Type* b) {
    TypeParam* ap;
    TypeParam* bp;
    if (!a || !b) return false;
    if (a == b) return true;
    if (a->kind != b->kind) return false;
    if (a->is_plain_char != b->is_plain_char ||
        a->is_char8 != b->is_char8 ||
        a->is_wchar != b->is_wchar ||
        a->is_char16 != b->is_char16 ||
        a->is_char32 != b->is_char32) return false;
    if (a->is_reference != b->is_reference ||
        a->is_rvalue_reference != b->is_rvalue_reference) {
        return false;
    }
    if (type_is_integer(a) && a->is_unsigned != b->is_unsigned) return false;
    if (a->kind == TYPE_PTR) {
        return a->cxx_is_member_pointer == b->cxx_is_member_pointer &&
               (!a->cxx_is_member_pointer ||
                type_is_compatible(a->cxx_member_pointer_owner,
                                   b->cxx_member_pointer_owner)) &&
               type_is_compatible(a->base, b->base);
    }
    if (a->kind == TYPE_ARRAY) {
        return (a->array_len < 0 || b->array_len < 0 ||
                a->array_len == b->array_len) &&
               type_is_compatible(a->base, b->base);
    }
    if (a->kind == TYPE_VECTOR) {
        return a->size == b->size && a->array_len == b->array_len &&
               type_is_compatible(a->base, b->base);
    }
    if (a->kind == TYPE_FUNC) {
        /* C++ pointer-to-member function types retain the member function's
         * cv qualification on this function-type carrier. */
        if (a->is_const != b->is_const ||
            a->is_volatile != b->is_volatile ||
            a->function_is_noexcept != b->function_is_noexcept ||
            a->function_ref_qualifier != b->function_ref_qualifier) {
            return false;
        }
        if (!type_is_compatible(a->ret_type, b->ret_type)) return false;
        if (!a->has_prototype || !b->has_prototype) {
            Type* prototype = a->has_prototype ? a : b;
            if (prototype->variadic) return false;
            for (TypeParam* parameter = prototype->params; parameter;
                 parameter = parameter->next) {
                Type* promoted = parameter->type;
                if (promoted->kind == TYPE_FLOAT) promoted = type_double;
                if (promoted->kind == TYPE_ENUM || promoted->kind < TYPE_INT) {
                    promoted = type_int;
                }
                if (!type_is_compatible(promoted, parameter->type)) {
                    return false;
                }
            }
            return true;
        }
        if (a->variadic != b->variadic) return false;
        ap = a->params;
        bp = b->params;
        while (ap && bp) {
            if (ap->is_pack != bp->is_pack ||
                !type_is_compatible(ap->type, bp->type)) return false;
            ap = ap->next;
            bp = bp->next;
        }
        return ap == NULL && bp == NULL;
    }
    if (a->kind == TYPE_STRUCT || a->kind == TYPE_UNION) {
        /* C++ class identity is declaration-based.  Comparing only the tag
         * merges unrelated classes with the same spelling in different
         * namespaces (and can then incorrectly reuse a class-template
         * specialization). */
        if (a->cxx_class || b->cxx_class) {
            return a->cxx_class && b->cxx_class &&
                   (a->cxx_class == b->cxx_class ||
                    cxx_template_class_instances_compatible(
                        a->cxx_class, b->cxx_class));
        }
        if (a->tag || b->tag) {
            return a->tag && b->tag && strcmp(a->tag, b->tag) == 0 &&
                   ((!a->cxx_scope_identity && !b->cxx_scope_identity) ||
                    (a->cxx_scope_identity && b->cxx_scope_identity &&
                     strcmp(a->cxx_scope_identity,
                            b->cxx_scope_identity) == 0)) &&
                   ((!a->cxx_namespace && !b->cxx_namespace) ||
                    (a->cxx_namespace && b->cxx_namespace &&
                     strcmp(a->cxx_namespace, b->cxx_namespace) == 0));
        }
        /* A qualified copy of an anonymous aggregate retains the same field
         * graph even though it has no tag to compare. */
        return a->fields == b->fields;
    }
    if (a->kind == TYPE_ENUM) {
        return a->enum_tag && b->enum_tag &&
               strcmp(a->enum_tag, b->enum_tag) == 0 &&
               ((!a->cxx_scope_identity && !b->cxx_scope_identity) ||
                (a->cxx_scope_identity && b->cxx_scope_identity &&
                 strcmp(a->cxx_scope_identity,
                        b->cxx_scope_identity) == 0)) &&
               ((!a->cxx_namespace && !b->cxx_namespace) ||
                (a->cxx_namespace && b->cxx_namespace &&
                 strcmp(a->cxx_namespace, b->cxx_namespace) == 0));
    }
    return true;
}

/* Generic-association matching needs the C-compatible type relation with
 * qualifiers retained at nested pointer/array/function layers. The general
 * compatibility predicate intentionally erases those qualifiers for other
 * semantic contexts. */
static bool type_is_compatible_qualified_impl(
    Type* a, Type* b, bool ignore_top_qualifiers, unsigned depth) {
    if (!a || !b || depth >= 128u || !type_is_compatible(a, b)) {
        return false;
    }
    if (a->kind != TYPE_FUNC && !ignore_top_qualifiers &&
        (a->is_const != b->is_const ||
         a->is_volatile != b->is_volatile ||
         a->is_atomic != b->is_atomic ||
         a->is_restrict != b->is_restrict)) {
        return false;
    }
    switch (a->kind) {
        case TYPE_PTR:
        case TYPE_ARRAY:
        case TYPE_VECTOR:
            return type_is_compatible_qualified_impl(
                a->base, b->base, false, depth + 1u);
        case TYPE_FUNC: {
            TypeParam* left;
            TypeParam* right;
            if (!type_is_compatible_qualified_impl(
                    a->ret_type, b->ret_type, false, depth + 1u)) {
                return false;
            }
            if (!a->has_prototype || !b->has_prototype) return true;
            left = a->params;
            right = b->params;
            while (left && right) {
                if (left->is_pack != right->is_pack ||
                    !type_is_compatible_qualified_impl(
                        left->type, right->type, true, depth + 1u)) {
                    return false;
                }
                left = left->next;
                right = right->next;
            }
            return left == NULL && right == NULL;
        }
        default:
            return true;
    }
}

bool type_is_compatible_qualified(Type* a, Type* b) {
    return type_is_compatible_qualified_impl(a, b, false, 0u);
}

Type* type_generic_selection_control(Type* type) {
    Type* adjusted;
    if (!type) return NULL;
    if (type->kind == TYPE_ARRAY) return type_ptr(type->base);
    if (type->kind == TYPE_FUNC) return type_ptr(type);
    if (!type->is_const && !type->is_volatile && !type->is_atomic &&
        !type->is_restrict) {
        return type;
    }
    adjusted = rcc_alloc(sizeof(*adjusted));
    *adjusted = *type;
    adjusted->is_const = false;
    adjusted->is_volatile = false;
    adjusted->is_atomic = false;
    adjusted->is_restrict = false;
    return adjusted;
}

bool type_is_same_template_argument(Type* a, Type* b) {
    TypeParam* left_parameter;
    TypeParam* right_parameter;
    if (!a || !b || a->kind != b->kind ||
        a->is_const != b->is_const || a->is_volatile != b->is_volatile ||
        a->is_atomic != b->is_atomic || a->is_restrict != b->is_restrict ||
        a->is_reference != b->is_reference ||
        a->is_rvalue_reference != b->is_rvalue_reference ||
        a->is_unsigned != b->is_unsigned ||
        a->is_plain_char != b->is_plain_char || a->is_char8 != b->is_char8 ||
        a->is_wchar != b->is_wchar ||
        a->is_char16 != b->is_char16 || a->is_char32 != b->is_char32 ||
        a->cxx_is_member_pointer != b->cxx_is_member_pointer ||
        a->cxx_dependent_underlying_type !=
            b->cxx_dependent_underlying_type ||
        a->cxx_dependent_alias_template !=
            b->cxx_dependent_alias_template) {
        return false;
    }
    if (a == b) return true;
    if (!!a->cxx_dependent_class_template_owner !=
            !!b->cxx_dependent_class_template_owner ||
        (a->cxx_dependent_class_template_owner &&
         !type_is_same_template_argument(
             a->cxx_dependent_class_template_owner,
             b->cxx_dependent_class_template_owner))) {
        return false;
    }
    if (a->cxx_dependent_underlying_type &&
        !type_is_same_template_argument(a->cxx_underlying_type_operand,
                                        b->cxx_underlying_type_operand)) {
        return false;
    }
    switch (a->kind) {
        case TYPE_PTR:
            return (!a->cxx_is_member_pointer ||
                    type_is_same_template_argument(
                        a->cxx_member_pointer_owner,
                        b->cxx_member_pointer_owner)) &&
                   type_is_same_template_argument(a->base, b->base);
        case TYPE_ARRAY:
            return a->array_len == b->array_len &&
                   a->array_unspecified_bound ==
                       b->array_unspecified_bound &&
                   type_is_same_template_argument(a->base, b->base);
        case TYPE_VECTOR:
            return a->size == b->size && a->array_len == b->array_len &&
                   type_is_same_template_argument(a->base, b->base);
        case TYPE_FUNC:
            if (a->function_is_noexcept != b->function_is_noexcept ||
                a->function_ref_qualifier != b->function_ref_qualifier ||
                a->variadic != b->variadic ||
                a->has_prototype != b->has_prototype ||
                !type_is_same_template_argument(a->ret_type, b->ret_type)) {
                return false;
            }
            left_parameter = a->params;
            right_parameter = b->params;
            while (left_parameter && right_parameter) {
                if (left_parameter->is_pack != right_parameter->is_pack ||
                    !type_is_same_template_argument(left_parameter->type,
                                                    right_parameter->type)) {
                    return false;
                }
                left_parameter = left_parameter->next;
                right_parameter = right_parameter->next;
            }
            return !left_parameter && !right_parameter;
        case TYPE_STRUCT:
        case TYPE_UNION:
            {
                bool has_pack_expansion = false;
                if (a->cxx_template_arg_pack_expansions ||
                    b->cxx_template_arg_pack_expansions) {
                    if (!a->cxx_template_arg_pack_expansions ||
                        !b->cxx_template_arg_pack_expansions ||
                        a->cxx_template != b->cxx_template ||
                        a->cxx_template_arg_count !=
                            b->cxx_template_arg_count) {
                        return false;
                    }
                    for (int index = 0; index < a->cxx_template_arg_count;
                         ++index) {
                        if (a->cxx_template_arg_pack_expansions[index] !=
                            b->cxx_template_arg_pack_expansions[index]) {
                            return false;
                        }
                        has_pack_expansion = has_pack_expansion ||
                            a->cxx_template_arg_pack_expansions[index];
                    }
                    if (has_pack_expansion) {
                        if (a->cxx_template_arg_count > 0 &&
                            (!a->cxx_template_args ||
                             !b->cxx_template_args)) {
                            return false;
                        }
                        for (int index = 0;
                             index < a->cxx_template_arg_count; ++index) {
                            if (!type_is_same_template_argument(
                                    a->cxx_template_args[index],
                                    b->cxx_template_args[index])) {
                                return false;
                            }
                        }
                        return true;
                    }
                }
            }
            if (a->cxx_class || b->cxx_class) {
                return a->cxx_class && b->cxx_class &&
                       (a->cxx_class == b->cxx_class ||
                        cxx_template_class_instances_compatible(
                            a->cxx_class, b->cxx_class));
            }
            return type_is_compatible(a, b);
        default:
            return type_is_compatible(a, b);
    }
}

bool ast_cxx_is_aggregate(const Type* type) {
    const CxxClass* cls;
    if (!type || (type->kind != TYPE_STRUCT && type->kind != TYPE_UNION) ||
        !type->is_complete) {
        return false;
    }
    cls = type->cxx_class;
    /* Plain C++ unions use the shared C aggregate type representation and do
     * not have a CxxClass node.  They have no bases or access-controlled
     * members in that representation, so their single active member follows
     * the aggregate path below. */
    if (type->kind == TYPE_UNION && !cls) return true;
    if (!cls) return false;
    if (type->kind == TYPE_UNION && cls->base_count != 0) return false;
    /* Aggregate classes acquired base subobjects in C++17.  Keep the older
     * standard's class aggregate rules when shared initializer code asks. */
    if (cls->base_count > 0 && !rcc_parser_cxx_standard_at_least(17)) {
        return false;
    }
    if (cls->has_user_constructor || cls->has_nonpublic_field ||
        cls->vtable_size != 0 || cls->virtual_base_count != 0) {
        return false;
    }
    for (int index = 0; index < cls->base_count; ++index) {
        if (!cls->bases[index].base || cls->bases[index].is_virtual ||
            cls->bases[index].access != ACCESS_PUBLIC) {
            return false;
        }
    }
    return true;
}

int ast_cxx_aggregate_element_count(const Type* type) {
    const CxxClass* cls;
    int count;
    if (!ast_cxx_is_aggregate(type)) return 0;
    cls = type->cxx_class;
    if (type->kind == TYPE_UNION) {
        for (const TypeField* field = type->fields; field;
             field = field->next) {
            if (field->cxx_declaring_class == cls) return 1;
        }
        return 0;
    }
    count = cls->base_count;
    for (const TypeField* field = type->fields; field; field = field->next) {
        if (field->cxx_declaring_class == cls) {
            if (count == INT_MAX) return 0;
            ++count;
        }
    }
    return count;
}

bool ast_cxx_aggregate_element(const Type* type, int index,
                               Type** element_type, int* element_offset,
                               TypeField** member_field) {
    const CxxClass* cls;
    int member_index;
    if (element_type) *element_type = NULL;
    if (element_offset) *element_offset = 0;
    if (member_field) *member_field = NULL;
    if (index < 0 || !ast_cxx_is_aggregate(type)) return false;
    cls = type->cxx_class;
    if (type->kind == TYPE_UNION) {
        if (index != 0) return false;
        for (TypeField* field = type->fields; field; field = field->next) {
            if (field->cxx_declaring_class != cls) continue;
            if (element_type) *element_type = field->type;
            if (element_offset) *element_offset = field->offset;
            if (member_field) *member_field = field;
            return field->type != NULL;
        }
        return false;
    }
    if (index < cls->base_count) {
        const CxxClass* base = cls->bases[index].base;
        if (!base || !base->type || !cls->base_offsets ||
            cls->base_offsets[index] < 0) {
            return false;
        }
        if (element_type) *element_type = base->type;
        if (element_offset) *element_offset = cls->base_offsets[index];
        return true;
    }
    member_index = cls->base_count;
    for (TypeField* field = type->fields; field; field = field->next) {
        if (field->cxx_declaring_class != cls) continue;
        if (member_index == index) {
            if (element_type) *element_type = field->type;
            if (element_offset) *element_offset = field->offset;
            if (member_field) *member_field = field;
            return field->type != NULL;
        }
        ++member_index;
    }
    return false;
}

TypeField* ast_cxx_aggregate_member(const Type* type, const char* name) {
    const CxxClass* cls;
    if (!name || !ast_cxx_is_aggregate(type)) return NULL;
    cls = type->cxx_class;
    for (TypeField* field = type->fields; field; field = field->next) {
        if (field->cxx_declaring_class == cls && field->name &&
            strcmp(field->name, name) == 0) {
            return field;
        }
    }
    return NULL;
}

Type* type_common(Type* a, Type* b) {
    Type* signed_type;
    Type* unsigned_type;

    /* Usual arithmetic conversions */
    if (a->kind == TYPE_DOUBLE || b->kind == TYPE_DOUBLE) return type_double;
    if (a->kind == TYPE_FLOAT || b->kind == TYPE_FLOAT) return type_float;

    /* Integer promotions */
    if (a->kind < TYPE_INT) a = type_int;
    if (b->kind < TYPE_INT) b = type_int;

    /* Same type */
    if (a->kind == b->kind && a->is_unsigned == b->is_unsigned) return a;

    if (a->is_unsigned == b->is_unsigned) {
        return a->kind > b->kind ? a : b;
    }

    unsigned_type = a->is_unsigned ? a : b;
    signed_type = a->is_unsigned ? b : a;
    if (unsigned_type->kind >= signed_type->kind) return unsigned_type;
    if (signed_type->size > unsigned_type->size) return signed_type;

    switch (signed_type->kind) {
        case TYPE_INT: return type_uint;
        case TYPE_LONG: return type_ulong;
        case TYPE_LLONG: return type_ullong;
        default: return unsigned_type;
    }
}

/* ═══════════════════════════════════════
 * Expression Constructors
 * ═══════════════════════════════════════ */

Expr* expr_int(int64_t val, SourceLoc loc) {
    Expr* e = rcc_alloc(sizeof(Expr));
    e->kind = EXPR_INT_LIT;
    e->loc = loc;
    e->int_val = val;
    e->type = type_int;
    return e;
}

static bool integer_literal_fits(Type* type, uint64_t value) {
    unsigned bits;
    uint64_t maximum;
    if (!type || !type_is_integer(type) || type->size <= 0) return false;
    bits = (unsigned)type->size * 8u;
    if (bits > 64u) return false;
    if (type->is_unsigned) {
        maximum = bits == 64u ? UINT64_MAX : (UINT64_C(1) << bits) - 1u;
    } else {
        maximum = bits == 64u ? (uint64_t)INT64_MAX
                              : (UINT64_C(1) << (bits - 1u)) - 1u;
    }
    return value <= maximum;
}

Expr* expr_integer_literal(uint64_t val, unsigned base,
                           bool unsigned_suffix, unsigned long_suffix,
                           SourceLoc loc) {
    Type* candidates[6];
    size_t count = 0u;
    bool decimal = base == 10u;
    Type* selected = NULL;

#define ADD_LITERAL_CANDIDATE(candidate) candidates[count++] = (candidate)
    if (unsigned_suffix) {
        if (long_suffix == 0u) {
            ADD_LITERAL_CANDIDATE(type_uint);
            ADD_LITERAL_CANDIDATE(type_ulong);
            ADD_LITERAL_CANDIDATE(type_ullong);
        } else if (long_suffix == 1u) {
            ADD_LITERAL_CANDIDATE(type_ulong);
            ADD_LITERAL_CANDIDATE(type_ullong);
        } else {
            ADD_LITERAL_CANDIDATE(type_ullong);
        }
    } else if (long_suffix == 0u) {
        ADD_LITERAL_CANDIDATE(type_int);
        if (!decimal) ADD_LITERAL_CANDIDATE(type_uint);
        ADD_LITERAL_CANDIDATE(type_long);
        if (!decimal) ADD_LITERAL_CANDIDATE(type_ulong);
        ADD_LITERAL_CANDIDATE(type_llong);
        if (!decimal) ADD_LITERAL_CANDIDATE(type_ullong);
    } else if (long_suffix == 1u) {
        ADD_LITERAL_CANDIDATE(type_long);
        if (!decimal) ADD_LITERAL_CANDIDATE(type_ulong);
        ADD_LITERAL_CANDIDATE(type_llong);
        if (!decimal) ADD_LITERAL_CANDIDATE(type_ullong);
    } else {
        ADD_LITERAL_CANDIDATE(type_llong);
        if (!decimal) ADD_LITERAL_CANDIDATE(type_ullong);
    }
#undef ADD_LITERAL_CANDIDATE

    for (size_t i = 0u; i < count; ++i) {
        if (integer_literal_fits(candidates[i], val)) {
            selected = candidates[i];
            break;
        }
    }
    if (!selected) {
        rcc_error(loc, "integer literal has no representable C17 type");
        selected = type_ullong;
    }

    {
        Expr* expression = expr_int((int64_t)val, loc);
        expression->type = selected;
        return expression;
    }
}

Expr* expr_float(double val, SourceLoc loc) {
    Expr* e = rcc_alloc(sizeof(Expr));
    e->kind = EXPR_FLOAT_LIT;
    e->loc = loc;
    e->float_val = val;
    e->type = type_double;
    return e;
}

Expr* expr_char(char val, SourceLoc loc) {
    Expr* e = rcc_alloc(sizeof(Expr));
    e->kind = EXPR_CHAR_LIT;
    e->loc = loc;
    e->char_val = val;
    e->char_element_width = 1u;
    e->type = type_int;  /* char literals have type int in C */
    return e;
}

Expr* expr_char_encoded(uint32_t val, uint8_t element_width, SourceLoc loc) {
    Expr* e;
    if ((element_width != 2u && element_width != 4u) ||
        (element_width == 2u && val > UINT16_MAX) ||
        (element_width == 4u && val > UINT32_MAX)) {
        rcc_fatal("invalid encoded character literal value");
    }
    e = rcc_alloc(sizeof(Expr));
    e->kind = EXPR_CHAR_LIT;
    e->loc = loc;
    e->int_val = (int64_t)val;
    e->char_element_width = element_width;
    e->type = element_width == 2u ? type_char16 : type_char32;
    return e;
}

uint32_t expr_char_code_unit(const Expr* expr) {
    if (!expr || expr->kind != EXPR_CHAR_LIT) return 0u;
    if (expr->char_element_width == 2u ||
        expr->char_element_width == 4u) {
        return (uint32_t)expr->int_val;
    }
    if (expr->type == type_int && !expr->is_cxx_utf8_literal) {
        /* In C, an ordinary character constant has type int after the
         * execution byte is converted through RinOS's signed plain char. */
        return (uint32_t)(int32_t)(int8_t)(uint8_t)expr->char_val;
    }
    return (uint8_t)expr->char_val;
}

Expr* expr_string(const char* val, SourceLoc loc) {
    return expr_string_n(val, strlen(val), loc);
}

Expr* expr_string_n(const char* val, size_t length, SourceLoc loc) {
    return expr_string_encoded_n(val, length, 1u, loc);
}

Expr* expr_string_encoded_n(const char* val, size_t byte_length,
                            uint8_t element_width, SourceLoc loc) {
    Type* element_type;
    size_t code_unit_count;
    Expr* e = rcc_alloc(sizeof(Expr));
    if (element_width != 1u && element_width != 2u &&
        element_width != 4u) {
        rcc_fatal("invalid string literal code-unit width %u",
                  (unsigned)element_width);
    }
    if (byte_length % element_width != 0u ||
        byte_length > SIZE_MAX - element_width) {
        rcc_fatal("invalid encoded string literal size");
    }
    code_unit_count = byte_length / element_width;
    if (code_unit_count >= INT_MAX) {
        rcc_fatal("string literal exceeds array length limit");
    }
    e->kind = EXPR_STRING_LIT;
    e->loc = loc;
    e->str_val = rcc_alloc(byte_length + element_width);
    memcpy((void*)e->str_val, val, byte_length);
    memset((char*)e->str_val + byte_length, 0, element_width);
    e->str_length = code_unit_count;
    e->str_byte_length = byte_length;
    e->str_element_width = element_width;
    element_type = element_width == 1u ? type_char
                 : element_width == 2u ? type_ushort : type_uint;
    e->type = type_array(element_type, (int)code_unit_count + 1);
    return e;
}

Expr* expr_ident(const char* name, SourceLoc loc) {
    Expr* e = rcc_alloc(sizeof(Expr));
    e->kind = EXPR_IDENT;
    e->loc = loc;
    e->ident_name = name;
    e->ident_decl = NULL;
    e->type = NULL;  /* Set during sema */
    return e;
}

Expr* expr_cxx_this(SourceLoc loc) {
    Expr* e = rcc_alloc(sizeof(Expr));
    e->kind = EXPR_CXX_THIS;
    e->loc = loc;
    e->type = NULL;
    e->cxx_this_stack_offset = -1;
    return e;
}

Expr* expr_unary(ExprKind kind, Expr* operand, SourceLoc loc) {
    Expr* e = rcc_alloc(sizeof(Expr));
    e->kind = kind;
    e->loc = loc;
    e->unary_operand = operand;
    e->cxx_noexcept_value_valid = false;
    e->cxx_noexcept_value = false;
    e->cxx_member_pointer_form = false;
    e->cxx_member_pointer_form_overload_set = false;
    e->cxx_member_pointer_form_access = 0u;
    e->cxx_member_pointer_form_declaring_class = NULL;
    e->cxx_member_pointer_form_designating_class = NULL;
    e->type = NULL;  /* Set during sema */
    return e;
}

Expr* expr_binary(ExprKind kind, Expr* lhs, Expr* rhs, SourceLoc loc) {
    Expr* e = rcc_alloc(sizeof(Expr));
    e->kind = kind;
    e->loc = loc;
    e->binary_lhs = lhs;
    e->binary_rhs = rhs;
    e->type = NULL;  /* Set during sema */
    return e;
}

Expr* expr_cond(Expr* test, Expr* then_expr, Expr* else_expr, SourceLoc loc) {
    Expr* e = rcc_alloc(sizeof(Expr));
    e->kind = EXPR_COND;
    e->loc = loc;
    e->cond_test = test;
    e->cond_then = then_expr;
    e->cond_else = else_expr;
    e->type = NULL;
    return e;
}

Expr* expr_call(Expr* func, ExprList* args, SourceLoc loc) {
    Expr* e = rcc_alloc(sizeof(Expr));
    e->kind = EXPR_CALL;
    e->loc = loc;
    e->call_func = func;
    e->call_args = args;
    e->call_result_offset = 0;
    e->call_abi_function_type = NULL;
    e->call_member_pointer_application = NULL;
    e->call_method = NULL;
    e->cxx_call_is_noexcept = false;
    e->cxx_temporary_source = NULL;
    e->cxx_temporary_owner = NULL;
    e->cxx_temporary_cleanups = NULL;
    e->cxx_typeinfo_hash_code = false;
    e->cxx_typeinfo_name = false;
    e->cxx_typeinfo_before = false;
    e->call_is_virtual = false;
    e->call_virtual_index = -1;
    e->call_virtual_object = NULL;
    e->call_is_new = false;
    e->call_new_value_init = false;
    e->call_new_is_array = false;
    e->call_new_brace_init = false;
    e->call_new_array_cookie = false;
    e->call_new_default_member_initializers = false;
    e->call_new_type = NULL;
    e->call_new_count = NULL;
    e->call_new_args = NULL;
    e->call_new_constructor = NULL;
    e->call_new_default_constructor = NULL;
    e->call_new_default_args = NULL;
    e->call_is_delete = false;
    e->call_delete_is_array = false;
    e->call_delete_cleanup = NULL;
    e->call_delete_destructor = NULL;
    e->call_delete_array_cleanup = NULL;
    e->call_delete_array_destructor = NULL;
    e->call_delete_cleanup_field = NULL;
    e->call_delete_array_cleanup_field = NULL;
    e->call_delete_cleanup_invalid = 0;
    e->call_delete_array_cleanup_invalid = 0;
    e->call_delete_object_type = NULL;
    e->type = NULL;
    return e;
}

Expr* expr_index(Expr* base, Expr* index, SourceLoc loc) {
    Expr* e = rcc_alloc(sizeof(Expr));
    e->kind = EXPR_INDEX;
    e->loc = loc;
    e->index_base = base;
    e->index_expr = index;
    e->type = NULL;
    return e;
}

Expr* expr_member(Expr* base, const char* name, SourceLoc loc) {
    Expr* e = rcc_alloc(sizeof(Expr));
    e->kind = EXPR_MEMBER;
    e->loc = loc;
    e->member_base = base;
    e->member_name = name;
    e->member_field = NULL;
    e->type = NULL;
    return e;
}

Expr* expr_cast(Type* type, Expr* expr, SourceLoc loc) {
    Expr* e = rcc_alloc(sizeof(Expr));
    e->kind = EXPR_CAST;
    e->loc = loc;
    e->cast_expr = expr;
    e->cast_type = type;
    e->type = type;
    return e;
}

Expr* expr_cxx_typeid(Type* operand_type, Expr* operand,
                      bool operand_is_type, SourceLoc loc) {
    Expr* e = rcc_alloc(sizeof(Expr));
    e->kind = EXPR_CXX_TYPEID;
    e->loc = loc;
    e->cxx_typeid_operand_type = operand_type;
    e->cxx_typeid_operand = operand;
    e->cxx_typeid_symbol = NULL;
    e->cxx_typeid_is_type = operand_is_type;
    e->cxx_typeid_dynamic = false;
    e->type = NULL;
    return e;
}

Expr* expr_sizeof_expr(Expr* expr, SourceLoc loc) {
    Expr* e = rcc_alloc(sizeof(Expr));
    e->kind = EXPR_SIZEOF;
    e->loc = loc;
    e->unary_operand = expr;
    e->sizeof_type = NULL;
    e->type = type_uint;
    return e;
}

Expr* expr_sizeof_type(Type* type, SourceLoc loc) {
    Expr* e = rcc_alloc(sizeof(Expr));
    e->kind = EXPR_SIZEOF;
    e->loc = loc;
    e->unary_operand = NULL;
    e->sizeof_type = type;
    e->type = type_uint;
    return e;
}

Expr* expr_sizeof_pack(const char* name, SourceLoc loc) {
    Expr* e = rcc_alloc(sizeof(Expr));
    e->kind = EXPR_SIZEOF;
    e->loc = loc;
    e->unary_operand = NULL;
    e->sizeof_type = NULL;
    e->sizeof_pack_name = name;
    e->type = type_uint;
    return e;
}

Expr* expr_cxx_fold(const char* pack_name, ExprKind operator_kind,
                    bool left_fold, SourceLoc loc) {
    Expr* e = rcc_alloc(sizeof(*e));
    e->kind = EXPR_CXX_FOLD;
    e->loc = loc;
    e->cxx_fold_pack_name = pack_name;
    e->cxx_fold_operator = operator_kind;
    e->cxx_fold_left = left_fold;
    e->cxx_fold_init = NULL;
    e->cxx_fold_pattern = NULL;
    e->type = NULL;
    return e;
}

Expr* expr_alignof_type(Type* type, SourceLoc loc) {
    Expr* expression = rcc_alloc(sizeof(*expression));
    expression->kind = EXPR_ALIGNOF;
    expression->loc = loc;
    expression->unary_operand = NULL;
    expression->sizeof_type = type;
    expression->type = type_uint;
    return expression;
}

Expr* expr_generic(Expr* control, GenericAssociation* associations,
                   SourceLoc loc) {
    Expr* expression = rcc_alloc(sizeof(*expression));
    expression->kind = EXPR_GENERIC;
    expression->loc = loc;
    expression->generic_control = control;
    expression->generic_associations = associations;
    return expression;
}

Expr* expr_cxx_requires(ExprList* requirements, SourceLoc loc) {
    Expr* e = rcc_alloc(sizeof(Expr));
    e->kind = EXPR_CXX_REQUIRES;
    e->loc = loc;
    e->cxx_requires_items = requirements;
    e->cxx_requires_params = NULL;
    e->cxx_requires_types = NULL;
    e->cxx_requires_nested = NULL;
    e->cxx_requires_compound = NULL;
    e->type = NULL;
    return e;
}

Expr* expr_vararg(ExprKind kind, Expr* list, Expr* second, Type* type,
                  SourceLoc loc) {
    Expr* expression = rcc_alloc(sizeof(*expression));
    expression->kind = kind;
    expression->loc = loc;
    expression->va_list_operand = list;
    expression->va_second_operand = second;
    expression->va_arg_type = type;
    expression->va_arg_result_offset = 0;
    expression->type = kind == EXPR_VA_ARG ? type : type_void;
    return expression;
}

void generic_association_append(GenericAssociation** list, Type* type,
                                Expr* expression, SourceLoc loc) {
    GenericAssociation* association = rcc_alloc(sizeof(*association));
    GenericAssociation** tail = list;
    association->type = type;
    association->expr = expression;
    association->loc = loc;
    while (*tail) tail = &(*tail)->next;
    *tail = association;
}

Expr* expr_initializer_list(ExprList* items, SourceLoc loc) {
    Expr* e = rcc_alloc(sizeof(Expr));
    e->kind = EXPR_COMPOUND;
    e->loc = loc;
    e->compound_type = NULL;
    e->compound_init = items;
    e->compound_offset = 0;
    e->compound_value_init = false;
    e->compound_copy_init = false;
    e->compound_paren_init = false;
    e->compound_designator_wrapper = false;
    e->compound_cxx_default_member_normalized = false;
    e->compound_static_symbol = NULL;
    e->compound_constructor = NULL;
    e->type = NULL;
    return e;
}

/* ═══════════════════════════════════════
 * Statement Constructors
 * ═══════════════════════════════════════ */

Stmt* stmt_expr(Expr* expr, SourceLoc loc) {
    Stmt* s = rcc_alloc(sizeof(Stmt));
    s->kind = STMT_EXPR;
    s->loc = loc;
    s->expr = expr;
    return s;
}

Stmt* stmt_block(StmtList* stmts, SourceLoc loc) {
    Stmt* s = rcc_alloc(sizeof(Stmt));
    s->kind = STMT_BLOCK;
    s->loc = loc;
    s->block_no_scope = false;
    s->block_stmts = stmts;
    return s;
}

Stmt* stmt_if(Expr* cond, Stmt* then_stmt, Stmt* else_stmt, SourceLoc loc) {
    Stmt* s = rcc_alloc(sizeof(Stmt));
    s->kind = STMT_IF;
    s->loc = loc;
    s->if_is_constexpr = false;
    s->if_cond = cond;
    s->if_then = then_stmt;
    s->if_else = else_stmt;
    return s;
}

Stmt* stmt_while(Expr* cond, Stmt* body, SourceLoc loc) {
    Stmt* s = rcc_alloc(sizeof(Stmt));
    s->kind = STMT_WHILE;
    s->loc = loc;
    s->while_cond = cond;
    s->while_body = body;
    return s;
}

Stmt* stmt_do(Stmt* body, Expr* cond, SourceLoc loc) {
    Stmt* s = rcc_alloc(sizeof(Stmt));
    s->kind = STMT_DO;
    s->loc = loc;
    s->while_body = body;
    s->while_cond = cond;
    return s;
}

Stmt* stmt_for(Stmt* init, Expr* cond, Expr* inc, Stmt* body, SourceLoc loc) {
    Stmt* s = rcc_alloc(sizeof(Stmt));
    s->kind = STMT_FOR;
    s->loc = loc;
    s->for_init = init;
    s->for_cond = cond;
    s->for_inc = inc;
    s->for_body = body;
    return s;
}

Stmt* stmt_switch(Expr* expr, Stmt* body, SourceLoc loc) {
    Stmt* s = rcc_alloc(sizeof(Stmt));
    s->kind = STMT_SWITCH;
    s->loc = loc;
    s->switch_expr = expr;
    s->switch_body = body;
    return s;
}

Stmt* stmt_case(Expr* val, Stmt* stmt, SourceLoc loc) {
    Stmt* s = rcc_alloc(sizeof(Stmt));
    s->kind = STMT_CASE;
    s->loc = loc;
    s->case_val = val;
    s->case_stmt = stmt;
    return s;
}

Stmt* stmt_default(Stmt* stmt, SourceLoc loc) {
    Stmt* s = rcc_alloc(sizeof(Stmt));
    s->kind = STMT_DEFAULT;
    s->loc = loc;
    s->default_stmt = stmt;
    return s;
}

Stmt* stmt_break(SourceLoc loc) {
    Stmt* s = rcc_alloc(sizeof(Stmt));
    s->kind = STMT_BREAK;
    s->loc = loc;
    return s;
}

Stmt* stmt_continue(SourceLoc loc) {
    Stmt* s = rcc_alloc(sizeof(Stmt));
    s->kind = STMT_CONTINUE;
    s->loc = loc;
    return s;
}

Stmt* stmt_return(Expr* val, SourceLoc loc) {
    Stmt* s = rcc_alloc(sizeof(Stmt));
    s->kind = STMT_RETURN;
    s->loc = loc;
    s->return_reference_result = false;
    s->return_reference_temporary_offset = 0;
    s->return_val = val;
    return s;
}

Stmt* stmt_goto(const char* label, SourceLoc loc) {
    Stmt* s = rcc_alloc(sizeof(Stmt));
    s->kind = STMT_GOTO;
    s->loc = loc;
    s->goto_label = label;
    return s;
}

Stmt* stmt_label(const char* name, Stmt* stmt, SourceLoc loc) {
    Stmt* s = rcc_alloc(sizeof(Stmt));
    s->kind = STMT_LABEL;
    s->loc = loc;
    s->label_name = name;
    s->label_stmt = stmt;
    return s;
}

Stmt* stmt_decl(Decl* decl, SourceLoc loc) {
    Stmt* s = rcc_alloc(sizeof(Stmt));
    s->kind = STMT_DECL;
    s->loc = loc;
    s->decl = decl;
    return s;
}

Stmt* stmt_null(SourceLoc loc) {
    Stmt* s = rcc_alloc(sizeof(Stmt));
    s->kind = STMT_NULL;
    s->loc = loc;
    return s;
}

Stmt* stmt_asm(const char* templ, AsmOperand* outputs, AsmOperand* inputs,
               AsmClobber* clobbers, bool is_volatile, SourceLoc loc) {
    Stmt* s = rcc_alloc(sizeof(Stmt));
    s->kind = STMT_ASM;
    s->loc = loc;
    s->asm_template = templ;
    s->asm_outputs = outputs;
    s->asm_inputs = inputs;
    s->asm_clobbers = clobbers;
    s->asm_volatile = is_volatile;
    return s;
}

Stmt* stmt_try(Stmt* body, CxxCatch* catches, SourceLoc loc) {
    Stmt* s = rcc_alloc(sizeof(Stmt));
    s->kind = STMT_TRY;
    s->loc = loc;
    s->try_body = body;
    s->try_catches = catches;
    s->try_frame_offset = 0;
    s->try_frame_size = 0;
    return s;
}

Stmt* stmt_throw(Expr* expression, SourceLoc loc) {
    Stmt* s = rcc_alloc(sizeof(Stmt));
    s->kind = STMT_THROW;
    s->loc = loc;
    s->throw_expr = expression;
    return s;
}

AsmOperand* asm_operand_new(const char* constraint, Expr* expr) {
    AsmOperand* op = rcc_alloc(sizeof(AsmOperand));
    op->constraint = constraint;
    op->expr = expr;
    op->assigned_gpr = -1;
    op->next = NULL;
    return op;
}

AsmClobber* asm_clobber_new(const char* reg) {
    AsmClobber* cl = rcc_alloc(sizeof(AsmClobber));
    cl->reg = reg;
    cl->next = NULL;
    return cl;
}

/* ═══════════════════════════════════════
 * Declaration Constructors
 * ═══════════════════════════════════════ */

Decl* decl_var(const char* name, Type* type, Expr* init, SourceLoc loc) {
    Decl* d = rcc_alloc(sizeof(Decl));
    d->kind = DECL_VAR;
    d->name = name;
    d->link_name = name;
    d->type = type;
    d->loc = loc;
    d->is_weak = false;
    d->param_default = NULL;
    d->var_init = init;
    d->var_debug_const_value = 0;
    d->var_debug_const_unsigned_value = 0u;
    d->var_debug_const_float_bits = 0u;
    d->var_debug_const_float_size = 0u;
    d->var_debug_const_value_float = false;
    d->var_debug_const_value_unsigned = false;
    d->var_debug_const_value_valid = false;
    d->var_offset = 0;
    d->var_register_name = NULL;
    d->var_reference_temporary_offset = 0;
    d->var_reference_temporary_owner = NULL;
    d->var_reference_temporary_source = NULL;
    d->var_reference_temporary_guard = NULL;
    d->var_dynamic_initializer = false;
    d->var_vla_size_offset = 0;
    d->var_vla_extent_offset = 0;
    d->var_vla_extent_count = 0;
    d->var_vla_scope_offset = 0;
    d->var_is_global = false;
    d->var_is_static_local = false;
    d->var_is_block_extern = false;
    d->var_is_thread_local = false;
    d->var_is_auto = false;
    d->var_is_decltype_auto = false;
    d->var_is_auto_reference = false;
    d->var_is_auto_rvalue_reference = false;
    d->var_is_auto_pointer = false;
    d->var_is_auto_const = false;
    d->var_is_constexpr = false;
    d->var_is_constinit = false;
    d->var_is_inline = false;
    d->var_is_deprecated = false;
    d->var_deprecated_message = NULL;
    d->var_cleanup = NULL;
    d->var_cleanups = NULL;
    return d;
}

Decl* decl_func(const char* name, Type* type, DeclList* params, Stmt* body, SourceLoc loc) {
    Decl* d = rcc_alloc(sizeof(Decl));
    d->kind = DECL_FUNC;
    d->name = name;
    d->link_name = name;
    d->type = type;
    d->loc = loc;
    d->is_weak = false;
    d->param_default = NULL;
    d->func_params = params;
    d->func_body = body;
    d->func_this_param = NULL;
    d->func_method_owner = NULL;
    d->func_friend_access = NULL;
    d->func_is_hidden_friend = false;
    d->func_is_inline = false;
    d->func_is_naked = false;
    d->func_is_defined = (body != NULL);
    d->func_is_template_instance = false;
    d->func_template_owner = NULL;
    d->func_template_args = NULL;
    d->func_template_arg_count = 0;
    d->func_template_value_args = NULL;
    d->func_template_value_present = NULL;
    d->func_template_pack_args = NULL;
    d->func_template_pack_values = NULL;
    d->func_template_pack_value_present = NULL;
    d->func_template_pack_count = -1;
    d->func_template_pack_offsets = NULL;
    d->func_template_pack_counts = NULL;
    d->func_has_explicit_variadic_ellipsis = false;
    d->func_has_cxx_linkage = false;
    d->func_has_local_linkage = false;
    d->func_is_cxx_method = false;
    d->func_is_cxx_constructor = false;
    d->func_is_cxx_destructor = false;
    d->func_is_constexpr = false;
    d->func_is_consteval = false;
    d->func_is_noreturn = false;
    d->func_is_noexcept = false;
    d->func_is_deprecated = false;
    d->func_deprecated_message = NULL;
    d->func_noexcept_expr = NULL;
    d->func_is_auto_return = false;
    d->func_is_decltype_auto_return = false;
    d->func_cxx_namespace = NULL;
    d->func_cxx_namespace_scope = NULL;
    d->func_overload_next = NULL;
    return d;
}

Decl* decl_param(const char* name, Type* type, int index, SourceLoc loc) {
    Decl* d = rcc_alloc(sizeof(Decl));
    d->kind = DECL_PARAM;
    d->name = name;
    d->link_name = name;
    d->type = type;
    d->loc = loc;
    d->is_weak = false;
    d->param_default = NULL;
    d->param_index = index;
    d->param_array_type = NULL;
    d->param_is_pack = false;
    d->param_is_cxx_indirect = false;
    return d;
}

Decl* decl_typedef(const char* name, Type* type, SourceLoc loc) {
    Decl* d = rcc_alloc(sizeof(Decl));
    d->kind = DECL_TYPEDEF;
    d->name = name;
    d->link_name = name;
    d->type = type;
    d->loc = loc;
    d->is_weak = false;
    d->param_default = NULL;
    d->typedef_type = type;
    return d;
}

Decl* decl_struct(const char* name, DeclList* fields, SourceLoc loc) {
    Decl* d = rcc_alloc(sizeof(Decl));
    d->kind = DECL_STRUCT;
    d->name = name;
    d->link_name = name;
    d->loc = loc;
    d->is_weak = false;
    d->param_default = NULL;
    d->struct_fields = fields;
    return d;
}

Decl* decl_union(const char* name, DeclList* fields, SourceLoc loc) {
    Decl* d = rcc_alloc(sizeof(Decl));
    d->kind = DECL_UNION;
    d->name = name;
    d->link_name = name;
    d->loc = loc;
    d->is_weak = false;
    d->param_default = NULL;
    d->struct_fields = fields;
    return d;
}

Decl* decl_enum(const char* name, DeclList* consts, SourceLoc loc) {
    Decl* d = rcc_alloc(sizeof(Decl));
    d->kind = DECL_ENUM;
    d->name = name;
    d->link_name = name;
    d->loc = loc;
    d->is_weak = false;
    d->param_default = NULL;
    d->enum_consts = consts;
    return d;
}

Decl* decl_enum_const(const char* name, int64_t val, SourceLoc loc) {
    Decl* d = rcc_alloc(sizeof(Decl));
    d->kind = DECL_ENUM_CONST;
    d->name = name;
    d->link_name = name;
    d->loc = loc;
    d->is_weak = false;
    d->type = type_int;
    d->param_default = NULL;
    d->enum_val = val;
    return d;
}

Decl* decl_static_assert(Expr* expression, const char* message,
                         SourceLoc loc) {
    Decl* d = rcc_alloc(sizeof(Decl));
    d->kind = DECL_STATIC_ASSERT;
    d->name = NULL;
    d->link_name = NULL;
    d->type = type_int;
    d->loc = loc;
    d->is_weak = false;
    d->param_default = NULL;
    d->static_assert_expr = expression;
    d->static_assert_message = message;
    return d;
}

const char* decl_link_name(const Decl* decl) {
    if (!decl) return NULL;
    return decl->link_name ? decl->link_name : decl->name;
}

/* ═══════════════════════════════════════
 * AST
 * ═══════════════════════════════════════ */

AST* ast_new(void) {
    AST* ast = rcc_alloc(sizeof(AST));
    ast->decls = NULL;
    return ast;
}

void ast_add_decl(AST* ast, Decl* decl) {
    decllist_append(&ast->decls, decl);
}

/* ═══════════════════════════════════════
 * List Utilities
 * ═══════════════════════════════════════ */

ExprList* exprlist_new(Expr* expr) {
    ExprList* list = rcc_alloc(sizeof(ExprList));
    list->expr = expr;
    list->cxx_temporary_owner = NULL;
    list->cxx_temporary_cleanups = NULL;
    list->cxx_temporary_is_parameter_object = false;
    list->cxx_temporary_next = NULL;
    list->next = NULL;
    return list;
}

void exprlist_append(ExprList** list, Expr* expr) {
    ExprList* node = exprlist_new(expr);
    if (!*list) {
        *list = node;
    } else {
        ExprList* p = *list;
        while (p->next) p = p->next;
        p->next = node;
    }
}

void exprlist_append_designated(ExprList** list, Expr* expr,
                                InitDesignatorKind kind, int64_t index,
                                const char* field) {
    ExprList* node = exprlist_new(expr);
    node->designator_kind = kind;
    node->designator_index = index;
    node->designator_field = field;
    if (!*list) {
        *list = node;
    } else {
        ExprList* item = *list;
        while (item->next) item = item->next;
        item->next = node;
    }
}

int exprlist_len(ExprList* list) {
    int n = 0;
    while (list) {
        n++;
        list = list->next;
    }
    return n;
}

StmtList* stmtlist_new(Stmt* stmt) {
    StmtList* list = rcc_alloc(sizeof(StmtList));
    list->stmt = stmt;
    list->next = NULL;
    return list;
}

void stmtlist_append(StmtList** list, Stmt* stmt) {
    StmtList* node = stmtlist_new(stmt);
    if (!*list) {
        *list = node;
    } else {
        StmtList* p = *list;
        while (p->next) p = p->next;
        p->next = node;
    }
}

DeclList* decllist_new(Decl* decl) {
    DeclList* list = rcc_alloc(sizeof(DeclList));
    list->decl = decl;
    list->next = NULL;
    return list;
}

void decllist_append(DeclList** list, Decl* decl) {
    DeclList* node = decllist_new(decl);
    if (!*list) {
        *list = node;
    } else {
        DeclList* p = *list;
        while (p->next) p = p->next;
        p->next = node;
    }
}
