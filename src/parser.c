/*
 * RCC - RinOS C Compiler
 * Parser (Recursive Descent)
 */

#include "rcc.h"
#include "token.h"
#include "ast.h"
#include "ast_cxx.h"
#include <limits.h>
#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
extern void rcc_parser_cxx_set_expression_template_context(
    Expr*) __attribute__((weak));
#endif

/* Parser state - exported for parser_cxx.c */
typedef struct {
    Token* cur;
    Token* prev;
} Parser;

Parser parser;  /* Non-static for C++ parser access */
static bool check(TokenType type);
static bool check_next(TokenType type);
static bool parser_parenthesized_pointer_is_function(void);
static bool parser_parenthesized_declarator_starts_member_pointer(void);
static bool parser_type_is_variably_modified(Type* type);
static bool parser_cxx_type_trait_kind(const char* name,
                                      CxxTypeTraitKind* kind);
static bool parser_cxx_type_trait_value(CxxTypeTraitKind kind, Type* type,
                                        Type* other_type,
                                        CxxTypeTraitArgument* arguments,
                                        bool* value);
static bool parser_type_is_dependent(Type* type);

bool rcc_parser_cxx_parenthesized_declarator_starts_member_pointer(void) {
    return parser_parenthesized_declarator_starts_member_pointer();
}

/* The C frontend and C++ frontend share this parser translation unit.  Keep
 * the C-only executable independent of parser_cxx.c without providing fake
 * language implementations: an absent C++ hook is a missing extension and
 * the caller's ordinary diagnostic path remains responsible for the token. */
#if defined(__GNUC__)
#define RCC_OPTIONAL_CXX __attribute__((weak))
#else
#define RCC_OPTIONAL_CXX
#endif
extern Type* rcc_parse_cxx_direct_list_type(void) RCC_OPTIONAL_CXX;
extern void rcc_parser_cxx_record_call_context(Expr* expression)
    RCC_OPTIONAL_CXX;
extern Type* rcc_parse_cxx_type_name(void) RCC_OPTIONAL_CXX;
extern Type* rcc_parser_cxx_find_class_type(const char*) RCC_OPTIONAL_CXX;
extern void rcc_parser_cxx_complete_aggregate_type(Type*) RCC_OPTIONAL_CXX;
extern Type* rcc_parser_cxx_destructor_type(const char*) RCC_OPTIONAL_CXX;
extern Expr* rcc_parse_cxx_functional_cast(void) RCC_OPTIONAL_CXX;
extern bool rcc_parse_cxx_type_start(void) RCC_OPTIONAL_CXX;
extern void rcc_parser_cxx_bind_explicit_global_variable(Expr*)
    RCC_OPTIONAL_CXX;
extern bool rcc_parse_cxx_member_template_arguments(Expr*) RCC_OPTIONAL_CXX;
extern Expr* rcc_parse_cxx_template_call(void) RCC_OPTIONAL_CXX;
extern Expr* rcc_parse_cxx_concept_expression(void) RCC_OPTIONAL_CXX;
extern Expr* rcc_parse_cxx_dependent_member(void) RCC_OPTIONAL_CXX;
extern Expr* rcc_parse_cxx_qualified_template_member(void) RCC_OPTIONAL_CXX;
extern Expr* rcc_parse_cxx_member_pointer_address(void) RCC_OPTIONAL_CXX;
extern Stmt* rcc_parse_cxx_auto_local_declaration(void) RCC_OPTIONAL_CXX;
extern Stmt* rcc_parse_cxx_class_local_declaration(
    Type* base_type, int storage, bool is_inline, bool is_constexpr,
    bool is_constinit, bool is_thread_local, SourceLoc loc) RCC_OPTIONAL_CXX;
extern void rcc_parser_validate_cxx_object_type(Type* type, SourceLoc loc)
    RCC_OPTIONAL_CXX;
extern Stmt* rcc_parse_cxx_qualified_data_definition(
    Type* base_type, int storage, bool is_inline, bool is_constexpr,
    bool is_constinit, bool is_thread_local, SourceLoc loc) RCC_OPTIONAL_CXX;
extern Stmt* rcc_parse_cxx_qualified_member_definition(
    Type* return_type, bool is_inline, bool is_constexpr, bool is_consteval,
    SourceLoc loc) RCC_OPTIONAL_CXX;
extern Stmt* rcc_parse_cxx_qualified_constructor_definition(
    bool is_inline, bool is_constexpr, bool is_consteval,
    SourceLoc loc) RCC_OPTIONAL_CXX;
extern Stmt* rcc_parse_cxx_qualified_destructor_definition(
    bool is_inline, bool is_constexpr, bool is_consteval,
    SourceLoc loc) RCC_OPTIONAL_CXX;
extern Stmt* rcc_parse_cxx_qualified_conversion_definition(
    bool is_inline, bool is_constexpr, bool is_consteval,
    SourceLoc loc) RCC_OPTIONAL_CXX;
extern Stmt* rcc_parse_cxx_operator_declaration(
    Type* return_type, StorageClass storage, SourceLoc loc) RCC_OPTIONAL_CXX;
extern bool rcc_parse_cxx_deduction_guide(void) RCC_OPTIONAL_CXX;
extern Expr* rcc_parser_cxx_capture_expression(
    const char* name, SourceLoc loc) RCC_OPTIONAL_CXX;
extern Expr* rcc_parse_cxx_special_expression(void) RCC_OPTIONAL_CXX;
extern Expr* rcc_parse_cxx_lambda(void) RCC_OPTIONAL_CXX;
extern Expr* rcc_parse_cxx_fold_expression(void) RCC_OPTIONAL_CXX;
extern Expr* rcc_parse_cxx_requires_expression(void) RCC_OPTIONAL_CXX;
extern Stmt* rcc_parse_cxx_statement(void) RCC_OPTIONAL_CXX;
extern bool rcc_parser_cxx_collect_statement_attributes(void)
    RCC_OPTIONAL_CXX;
extern void rcc_parser_cxx_discard_statement_attributes(void)
    RCC_OPTIONAL_CXX;
extern bool rcc_parser_cxx_take_nodiscard_attribute(void) RCC_OPTIONAL_CXX;
extern bool rcc_parser_cxx_take_deprecated_attribute(
    const char** message) RCC_OPTIONAL_CXX;
extern bool rcc_parser_cxx_take_no_unique_address_attribute(void)
    RCC_OPTIONAL_CXX;
extern bool rcc_parser_cxx_take_weak_attribute(void) RCC_OPTIONAL_CXX;
extern bool rcc_parser_cxx_take_noreturn_attribute(void) RCC_OPTIONAL_CXX;
extern Stmt* rcc_parse_cxx_range_for_statement(void) RCC_OPTIONAL_CXX;
extern void rcc_parser_cxx_begin_constructor_initializer_parameters(
    DeclList* parameters) RCC_OPTIONAL_CXX;
extern void rcc_parser_cxx_end_constructor_initializer_parameters(void)
    RCC_OPTIONAL_CXX;
extern void rcc_parser_cxx_begin_function_parameters(DeclList* parameters)
    RCC_OPTIONAL_CXX;
extern void rcc_parser_cxx_end_function_parameters(void) RCC_OPTIONAL_CXX;
extern void rcc_parser_cxx_bind_value_identifier(Expr* expression)
    RCC_OPTIONAL_CXX;
extern void* rcc_parser_cxx_begin_function_friend_access(
    const char* name, Type* function_type) RCC_OPTIONAL_CXX;
extern void rcc_parser_cxx_end_function_friend_access(void* saved_context)
    RCC_OPTIONAL_CXX;
extern void* rcc_parser_cxx_begin_function_signature_access(bool file_scope)
    RCC_OPTIONAL_CXX;
extern void rcc_parser_cxx_end_function_signature_access(
    void* saved_context, const char* name, Type* function_type,
    DeclList* parameters, SourceLoc loc) RCC_OPTIONAL_CXX;
extern void* rcc_parser_cxx_mark_function_signature_alias_uses(void)
    RCC_OPTIONAL_CXX;
extern void rcc_parser_cxx_reset_function_signature_alias_uses(void* mark)
    RCC_OPTIONAL_CXX;
extern void rcc_parser_cxx_validate_function_signature_access(
    void* saved_context, const char* name, Type* function_type,
    DeclList* parameters, SourceLoc loc) RCC_OPTIONAL_CXX;
extern void rcc_parser_cxx_restore_function_signature_access(
    void* saved_context) RCC_OPTIONAL_CXX;
extern void rcc_parser_cxx_add_value_binding(const char* name, Type* type)
    RCC_OPTIONAL_CXX;
extern void* rcc_parser_cxx_using_scope_mark(void) RCC_OPTIONAL_CXX;
extern void rcc_parser_cxx_using_scope_restore(void* mark) RCC_OPTIONAL_CXX;
extern const char* rcc_parser_cxx_resolve_local_using(
    const char* name, SourceLoc loc) RCC_OPTIONAL_CXX;
extern const char* rcc_parser_cxx_current_namespace_identity(void)
    RCC_OPTIONAL_CXX;
extern bool rcc_parser_cxx_type_namespace_is_visible(
    const char*, const char*) RCC_OPTIONAL_CXX;
extern void rcc_parser_cxx_type_namespace_lookup_depths(
    const char*, const char*, int*, int*) RCC_OPTIONAL_CXX;
extern bool rcc_parser_cxx_type_namespace_matches_qualified_name(
    const char*, const char*) RCC_OPTIONAL_CXX;
extern bool rcc_parser_cxx_enum_namespace_is_visible(
    Type* enum_type, const char* namespace_name) RCC_OPTIONAL_CXX;

typedef struct ParserTypeName {
    const char* name;
    Type* type;
    bool is_maybe_unused;
    bool is_deprecated;
    const char* deprecated_message;
    const char* cxx_namespace;
    struct ParserTagScope* scope;
    uint32_t cxx_constructor_arity_mask;
    struct ParserTypeName* next;
} ParserTypeName;

typedef struct ParserTagName {
    const char* name;
    const char* cxx_namespace;
    TypeKind kind;
    Type* type;
    struct ParserTagScope* scope;
    struct ParserTagName* next;
} ParserTagName;

typedef struct ParserTagScope {
    struct ParserTagScope* parent;
} ParserTagScope;

typedef struct ParserEnumConstant {
    const char* name;
    int64_t value;
    Type* type;
    Type* expression_type;
    bool is_maybe_unused;
    bool is_deprecated;
    const char* deprecated_message;
    struct ParserEnumConstant* next;
} ParserEnumConstant;

static ParserTypeName* parser_type_names;
static ParserTagName* parser_tag_names;
static ParserTagScope* parser_tag_scope;
static ParserTagScope* parser_tag_root_scope;
static uint32_t parser_tag_identity_counter;
static ParserEnumConstant* parser_enum_constants;
static Type* parser_builtin_va_list_type;
static int parser_pack_alignment;
/* An aggregate's GNU packed attribute limits only its own members. Keep this
 * separate from #pragma pack so a nested aggregate definition does not
 * accidentally inherit the enclosing type's packed attribute. */
static int parser_aggregate_pack_alignment;
static int parser_pack_stack[32];
static int parser_pack_depth;
static bool parser_cxx_mode;
static const char* parser_ambiguous_type_name;
static int parser_cxx_standard = 20;
static bool parser_cxx_template_default_mode;
static int parser_cxx_template_default_depth;
static int parser_cxx_template_default_paren_base[64];
static int parser_cxx_template_paren_depth;
static const char* parser_function_name;
static const char* parser_function_name_stack[64];
static ParserTypeName* parser_function_type_names_stack[64];
static ParserTagName* parser_function_tag_names_stack[64];
static ParserTagScope* parser_function_tag_scope_stack[64];
static ParserEnumConstant* parser_function_enum_constants_stack[64];
static int parser_function_scope_depth;
static bool parser_last_declarator_parameter_pack;

bool rcc_parser_last_cxx_declarator_was_pack(void) {
    return parser_last_declarator_parameter_pack;
}

void rcc_parser_set_cxx_mode(bool enabled) {
    parser_cxx_mode = enabled;
}

bool rcc_parser_is_cxx_mode(void) {
    return parser_cxx_mode;
}

void rcc_parser_initialize_builtin_va_list_type(void) {
    if (g_opts.target_arch == ARCH_X64) {
        Type* record = type_struct("__rcc_sysv_va_list");
        record->size = 24;
        record->align = 8;
        record->is_complete = true;
        parser_builtin_va_list_type = type_array(record, 1);
    } else {
        parser_builtin_va_list_type = type_ptr(type_char);
    }
}

Type* rcc_parser_builtin_va_list_type(void) {
    return parser_builtin_va_list_type;
}

void rcc_parser_set_cxx_standard(int standard) {
    parser_cxx_standard = standard;
}

bool rcc_parser_cxx_standard_at_least(int standard) {
    return parser_cxx_mode && parser_cxx_standard >= standard;
}

void rcc_parser_function_scope_push(const char* name) {
    if (parser_function_scope_depth >=
        (int)(sizeof(parser_function_name_stack) /
              sizeof(parser_function_name_stack[0]))) {
        rcc_fatal("function scope nesting is too deep");
    }
    parser_function_name_stack[parser_function_scope_depth] =
        parser_function_name;
    parser_function_type_names_stack[parser_function_scope_depth] =
        parser_type_names;
    parser_function_tag_names_stack[parser_function_scope_depth] =
        parser_tag_names;
    parser_function_tag_scope_stack[parser_function_scope_depth] =
        parser_tag_scope;
    parser_function_enum_constants_stack[parser_function_scope_depth] =
        parser_enum_constants;
    ++parser_function_scope_depth;
    parser_function_name = name;
    parser_tag_scope = ast_arena_alloc(sizeof(*parser_tag_scope));
    parser_tag_scope->parent =
        parser_function_tag_scope_stack[parser_function_scope_depth - 1];
}

void rcc_parser_function_scope_pop(void) {
    if (parser_function_scope_depth <= 0) {
        rcc_fatal("function scope stack underflow");
    }
    --parser_function_scope_depth;
    parser_function_name = parser_function_name_stack[parser_function_scope_depth];
    parser_type_names =
        parser_function_type_names_stack[parser_function_scope_depth];
    parser_tag_names =
        parser_function_tag_names_stack[parser_function_scope_depth];
    parser_tag_scope =
        parser_function_tag_scope_stack[parser_function_scope_depth];
    parser_enum_constants =
        parser_function_enum_constants_stack[parser_function_scope_depth];
}

void rcc_parser_set_cxx_template_default_mode(bool enabled) {
    if (enabled) {
        if (parser_cxx_template_default_depth >=
            (int)(sizeof(parser_cxx_template_default_paren_base) /
                  sizeof(parser_cxx_template_default_paren_base[0]))) {
            rcc_fatal("C++ template-default parse nesting is too deep");
        }
        if (parser_cxx_template_default_depth == 0) {
            parser_cxx_template_paren_depth = 0;
        }
        parser_cxx_template_default_paren_base[
            parser_cxx_template_default_depth++] =
                parser_cxx_template_paren_depth;
    } else if (parser_cxx_template_default_depth > 0) {
        --parser_cxx_template_default_depth;
        if (parser_cxx_template_default_depth == 0) {
            parser_cxx_template_paren_depth = 0;
        }
    } else {
        parser_cxx_template_paren_depth = 0;
    }
    parser_cxx_template_default_mode =
        parser_cxx_template_default_depth > 0;
}

static bool parser_cxx_template_default_at_argument_boundary(void) {
    return parser_cxx_template_default_mode &&
        parser_cxx_template_default_depth > 0 &&
        parser_cxx_template_paren_depth ==
            parser_cxx_template_default_paren_base[
                parser_cxx_template_default_depth - 1];
}

static bool parser_pack_value_valid(int alignment) {
    return alignment == 0 || alignment == 1 || alignment == 2 ||
           alignment == 4 || alignment == 8 || alignment == 16;
}

static void parser_apply_pack(Token* directive) {
    int value = (int)directive->value.int_val;
    if (value == -1) {
        if (parser_pack_depth == 0) {
            rcc_warning(directive->loc, "#pragma pack(pop) without push");
        } else {
            parser_pack_alignment = parser_pack_stack[--parser_pack_depth];
        }
        return;
    }
    if (value >= 256) {
        int alignment = value - 256;
        if (!parser_pack_value_valid(alignment)) {
            rcc_error(directive->loc,
                      "unsupported #pragma pack alignment %d", alignment);
            return;
        }
        if (parser_pack_depth >=
            (int)(sizeof(parser_pack_stack) / sizeof(parser_pack_stack[0]))) {
            rcc_error(directive->loc, "#pragma pack stack overflow");
            return;
        }
        parser_pack_stack[parser_pack_depth++] = parser_pack_alignment;
        if (alignment != 0) parser_pack_alignment = alignment;
        return;
    }
    if (!parser_pack_value_valid(value)) {
        rcc_error(directive->loc,
                  "unsupported #pragma pack alignment %d", value);
        return;
    }
    parser_pack_alignment = value;
}

static bool parser_tag_scope_is_visible(ParserTagScope* scope);

typedef struct ParserCxxTypeLookup {
    Type* type;
    int lexical_depth;
    int depth;
    bool ambiguous;
} ParserCxxTypeLookup;

static int parser_tag_scope_lookup_depth(ParserTagScope* scope) {
    int depth = 0;
    for (ParserTagScope* visible = parser_tag_scope; visible;
         visible = visible->parent, ++depth) {
        if (visible == scope) return depth;
    }
    return -1;
}

static void parser_cxx_consider_type(ParserCxxTypeLookup* lookup,
                                     Type* candidate, const char* name,
                                     const char* current_namespace,
                                     ParserTagScope* declaration_scope) {
    int direct_depth = -1;
    int using_depth = -1;
    int candidate_depth;
    int lexical_depth = parser_tag_scope_lookup_depth(declaration_scope);
    if (!candidate || lexical_depth < 0 ||
        !rcc_parser_cxx_type_namespace_lookup_depths) return;
    rcc_parser_cxx_type_namespace_lookup_depths(
        name, current_namespace, &direct_depth, &using_depth);
    candidate_depth = direct_depth;
    if (using_depth >= 0 &&
        (candidate_depth < 0 || using_depth < candidate_depth)) {
        candidate_depth = using_depth;
    }
    if (candidate_depth < 0) return;
    if (!lookup->type || lexical_depth < lookup->lexical_depth ||
        (lexical_depth == lookup->lexical_depth &&
         candidate_depth < lookup->depth)) {
        lookup->type = candidate;
        lookup->lexical_depth = lexical_depth;
        lookup->depth = candidate_depth;
        lookup->ambiguous = false;
    } else if (lexical_depth == lookup->lexical_depth &&
               candidate_depth == lookup->depth &&
               candidate != lookup->type) {
        lookup->ambiguous = true;
    }
}

static Type* parser_lookup_type(const char* name) {
    ParserTypeName* entry;
    const char* current_namespace = NULL;
    Type* visible_type = NULL;
    size_t visible_namespace_length = 0u;
    bool found_visible_type = false;
    static Type* builtin_v4sf;
    static Type* builtin_v2df;
    static Type* builtin_v16qi;
    static Type* builtin_v8hi;
    static Type* builtin_v4si;
    static Type* builtin_v2di;
    parser_ambiguous_type_name = NULL;
    if (!parser_cxx_mode && g_opts.c_standard >= 23 && name &&
        strcmp(name, "__rcc_nullptr_t") == 0) {
        return type_nullptr;
    }
    if (name && strcmp(name, "__v4sf") == 0) {
        if (!builtin_v4sf) builtin_v4sf = type_vector(type_float, 4, 16);
        return builtin_v4sf;
    }
    if (name && strcmp(name, "__v2df") == 0) {
        if (!builtin_v2df) builtin_v2df = type_vector(type_double, 2, 16);
        return builtin_v2df;
    }
    if (name && strcmp(name, "__v16qi") == 0) {
        if (!builtin_v16qi) builtin_v16qi = type_vector(type_char, 16, 16);
        return builtin_v16qi;
    }
    if (name && strcmp(name, "__v8hi") == 0) {
        if (!builtin_v8hi) builtin_v8hi = type_vector(type_short, 8, 16);
        return builtin_v8hi;
    }
    if (name && strcmp(name, "__v4si") == 0) {
        if (!builtin_v4si) builtin_v4si = type_vector(type_int, 4, 16);
        return builtin_v4si;
    }
    if (name && strcmp(name, "__v2di") == 0) {
        if (!builtin_v2di) builtin_v2di = type_vector(type_llong, 2, 16);
        return builtin_v2di;
    }
    if (parser_cxx_mode && rcc_parser_cxx_current_namespace_identity) {
        current_namespace =
            rcc_parser_cxx_current_namespace_identity();
        if (current_namespace && !*current_namespace) {
            current_namespace = NULL;
        }
    }
    if (parser_cxx_mode) {
        ParserCxxTypeLookup lookup = {NULL, INT_MAX, INT_MAX, false};
        for (ParserTagName* tag = parser_tag_names; tag; tag = tag->next) {
            if (tag->name && strcmp(tag->name, name) == 0 &&
                (tag->kind == TYPE_ENUM || tag->kind == TYPE_STRUCT ||
                 tag->kind == TYPE_UNION) && tag->scope &&
                parser_tag_scope_is_visible(tag->scope)) {
                parser_cxx_consider_type(&lookup, tag->type,
                    tag->cxx_namespace ? tag->cxx_namespace : "",
                    current_namespace ? current_namespace : "",
                    tag->scope);
            }
        }
        for (entry = parser_type_names; entry; entry = entry->next) {
            if (entry->name && strcmp(entry->name, name) == 0) {
                parser_cxx_consider_type(&lookup, entry->type,
                    entry->cxx_namespace ? entry->cxx_namespace : "",
                    current_namespace ? current_namespace : "",
                    entry->scope);
            }
        }
        if (lookup.ambiguous) {
            parser_ambiguous_type_name = name;
            return NULL;
        }
        if (lookup.type) return lookup.type;
        /* Older C-only builds may not include the C++ namespace resolver. */
        if (!rcc_parser_cxx_type_namespace_lookup_depths) {
            for (ParserTagName* tag = parser_tag_names; tag;
                 tag = tag->next) {
                bool namespace_visible = !tag->cxx_namespace ||
                    !*tag->cxx_namespace ||
                    (rcc_parser_cxx_type_namespace_is_visible &&
                     rcc_parser_cxx_type_namespace_is_visible(
                         tag->cxx_namespace, current_namespace));
                if (tag->name && strcmp(tag->name, name) == 0 &&
                    (tag->kind == TYPE_ENUM || tag->kind == TYPE_STRUCT ||
                     tag->kind == TYPE_UNION) && namespace_visible &&
                    tag->scope && parser_tag_scope_is_visible(tag->scope)) {
                    return tag->type;
                }
            }
        } else {
            return NULL;
        }
    }
    for (entry = parser_type_names; entry; entry = entry->next) {
        size_t declaration_namespace_length;
        bool namespace_visible;
        if (!entry->name || strcmp(entry->name, name) != 0) continue;
        if (!parser_cxx_mode) return entry->type;
        declaration_namespace_length = entry->cxx_namespace
            ? strlen(entry->cxx_namespace) : 0u;
        if (declaration_namespace_length == 0u) {
            namespace_visible = true;
        } else {
            namespace_visible = rcc_parser_cxx_type_namespace_is_visible &&
                rcc_parser_cxx_type_namespace_is_visible(
                    entry->cxx_namespace, current_namespace);
        }
        if (!namespace_visible) continue;
        if (!found_visible_type || declaration_namespace_length >
                                       visible_namespace_length) {
            visible_type = entry->type;
            visible_namespace_length = declaration_namespace_length;
            found_visible_type = true;
        }
    }
    return visible_type;
}

static void parser_define_type(const char* name, Type* type) {
    ParserTypeName* entry = ast_arena_alloc(sizeof(*entry));
    entry->name = name;
    entry->type = type;
    entry->is_maybe_unused = false;
    entry->is_deprecated = false;
    entry->deprecated_message = NULL;
    entry->cxx_namespace = parser_cxx_mode &&
        rcc_parser_cxx_current_namespace_identity
        ? rcc_parser_cxx_current_namespace_identity() : NULL;
    entry->scope = parser_tag_scope;
    if (entry->cxx_namespace && !*entry->cxx_namespace) {
        entry->cxx_namespace = NULL;
    }
    entry->cxx_constructor_arity_mask = 0u;
    entry->next = parser_type_names;
    parser_type_names = entry;
}

static void parser_mark_latest_type_deprecated(const char* name,
                                              const char* message) {
    ParserTypeName* entry;
    for (entry = parser_type_names; entry; entry = entry->next) {
        if (entry->name && name && strcmp(entry->name, name) == 0 &&
            entry->scope == parser_tag_scope) {
            entry->is_deprecated = true;
            if (!entry->deprecated_message) {
                entry->deprecated_message = message;
            }
            return;
        }
    }
}

static void parser_mark_latest_type_maybe_unused(const char* name) {
    ParserTypeName* entry;
    for (entry = parser_type_names; entry; entry = entry->next) {
        if (entry->name && name && strcmp(entry->name, name) == 0 &&
            entry->scope == parser_tag_scope) {
            entry->is_maybe_unused = true;
            return;
        }
    }
}

static bool parser_current_type_is_deprecated(const char* name,
                                             const char** message) {
    ParserTypeName* entry;
    if (message) *message = NULL;
    for (entry = parser_type_names; entry; entry = entry->next) {
        if (entry->name && name && strcmp(entry->name, name) == 0 &&
            entry->scope == parser_tag_scope) {
            if (message) *message = entry->deprecated_message;
            return entry->is_deprecated;
        }
    }
    return false;
}

static bool parser_current_type_is_maybe_unused(const char* name) {
    ParserTypeName* entry;
    for (entry = parser_type_names; entry; entry = entry->next) {
        if (entry->name && name && strcmp(entry->name, name) == 0 &&
            entry->scope == parser_tag_scope) {
            return entry->is_maybe_unused;
        }
    }
    return false;
}

static bool parser_tag_scope_is_visible(ParserTagScope* scope);

static void parser_warn_deprecated_typedef(const char* name, SourceLoc loc) {
    ParserTypeName* entry;
    if (!name || parser_cxx_mode) return;
    for (entry = parser_type_names; entry; entry = entry->next) {
        if (entry->name && strcmp(entry->name, name) == 0 &&
            parser_tag_scope_is_visible(entry->scope)) {
            if (entry->is_deprecated) {
                if (entry->deprecated_message) {
                    rcc_warning(loc, "use of deprecated typedef '%s': %s",
                                name, entry->deprecated_message);
                } else {
                    rcc_warning(loc, "use of deprecated typedef '%s'", name);
                }
            }
            return;
        }
    }
}

static bool parser_tag_scope_is_visible(ParserTagScope* scope) {
    ParserTagScope* visible;
    for (visible = parser_tag_scope; visible; visible = visible->parent) {
        if (visible == scope) return true;
    }
    return false;
}

static bool parser_tag_has_definition(TypeKind kind) {
    (void)kind;
    return check(TOK_LBRACE);
}

static const char* parser_new_tag_scope_identity(void) {
    char buffer[64];
    int length;
    if (parser_tag_identity_counter == UINT32_MAX) {
        rcc_fatal("translation unit contains too many local tag types");
    }
    ++parser_tag_identity_counter;
    length = snprintf(buffer, sizeof(buffer), "__rcc_tag_scope_%u",
                      parser_tag_identity_counter);
    if (length < 0 || (size_t)length >= sizeof(buffer)) {
        rcc_fatal("local tag identity formatting failed");
    }
    return rcc_intern(buffer);
}

void rcc_parser_reset_type_scopes(void) {
    parser_type_names = NULL;
    parser_tag_names = NULL;
    parser_tag_identity_counter = 0u;
    parser_tag_root_scope = ast_arena_alloc(sizeof(*parser_tag_root_scope));
    parser_tag_root_scope->parent = NULL;
    parser_tag_scope = parser_tag_root_scope;
    parser_enum_constants = NULL;
    parser_function_name = NULL;
    parser_function_scope_depth = 0;
}

const char* rcc_parser_new_local_type_identity(void) {
    return parser_new_tag_scope_identity();
}

static Type* parser_tag_type(TypeKind kind, const char* name) {
    ParserTagName* entry;
    ParserTagName* visible = NULL;
    bool has_definition = parser_tag_has_definition(kind);
    const char* cxx_namespace = parser_cxx_mode &&
        rcc_parser_cxx_current_namespace_identity
        ? rcc_parser_cxx_current_namespace_identity() : NULL;
    if (name) {
        for (entry = parser_tag_names; entry; entry = entry->next) {
            bool same_namespace =
                (!entry->cxx_namespace && !cxx_namespace) ||
                (entry->cxx_namespace && cxx_namespace &&
                 strcmp(entry->cxx_namespace, cxx_namespace) == 0);
            if (entry->kind == kind && same_namespace && entry->scope &&
                parser_tag_scope_is_visible(entry->scope) &&
                strcmp(entry->name, name) == 0) {
                visible = entry;
                break;
            }
        }
        if (visible &&
            (visible->scope == parser_tag_scope || !has_definition)) {
            return visible->type;
        }
    }
    Type* type = kind == TYPE_STRUCT ? type_struct(name) :
                 kind == TYPE_UNION ? type_union(name) : type_enum(name);
    type->cxx_namespace = cxx_namespace;
    if ((parser_tag_scope && parser_tag_scope->parent) || !name) {
        type->cxx_scope_identity = parser_new_tag_scope_identity();
    }
    if (name) {
        entry = ast_arena_alloc(sizeof(*entry));
        entry->name = name;
        entry->cxx_namespace = cxx_namespace;
        entry->kind = kind;
        entry->type = type;
        entry->scope = parser_tag_scope;
        entry->next = parser_tag_names;
        parser_tag_names = entry;
    }
    return type;
}

static void parser_define_enum_constant(const char* name, int64_t value,
                                        Type* type, Type* expression_type,
                                        bool is_maybe_unused,
                                        bool is_deprecated,
                                        const char* deprecated_message) {
    ParserEnumConstant* entry = ast_arena_alloc(sizeof(*entry));
    entry->name = name;
    entry->value = value;
    entry->type = type;
    entry->expression_type = expression_type ? expression_type : type;
    entry->is_maybe_unused = is_maybe_unused;
    entry->is_deprecated = is_deprecated;
    entry->deprecated_message = deprecated_message;
    entry->next = parser_enum_constants;
    parser_enum_constants = entry;
}

void rcc_parser_apply_pragma_pack(Token* directive) {
    parser_apply_pack(directive);
}

int rcc_parser_pack_alignment(void) {
    return parser_pack_alignment;
}

static bool parser_enum_constant_name_matches(
    const ParserEnumConstant* entry, const char* name) {
    const char* spelling = name;
    size_t spelling_length;
    size_t enumerator_length;
    size_t namespace_length;
    char namespace_name[512];

    if (!entry || !entry->name || !spelling) return false;
    while (spelling[0] == ':' && spelling[1] == ':') spelling += 2;
    if (strcmp(entry->name, spelling) == 0) return true;

    spelling_length = strlen(spelling);
    enumerator_length = strlen(entry->name);
    if (spelling_length < 2u ||
        spelling_length - 2u < enumerator_length) {
        return false;
    }
    namespace_length = spelling_length - enumerator_length - 2u;
    if (spelling[namespace_length] != ':' ||
        spelling[namespace_length + 1u] != ':' ||
        strcmp(spelling + namespace_length + 2u, entry->name) != 0 ||
        namespace_length >= sizeof(namespace_name)) {
        return false;
    }
    memcpy(namespace_name, spelling, namespace_length);
    namespace_name[namespace_length] = '\0';
    return rcc_parser_cxx_enum_namespace_is_visible &&
           rcc_parser_cxx_enum_namespace_is_visible(
               entry->type, namespace_name);
}

static bool parser_lookup_enum_constant(const char* name, int64_t* value,
                                        Type** type, Type** expression_type,
                                        bool* is_deprecated,
                                        const char** deprecated_message) {
    ParserEnumConstant* entry;
    for (entry = parser_enum_constants; entry; entry = entry->next) {
        if (parser_enum_constant_name_matches(entry, name)) {
            if (value) *value = entry->value;
            if (type) *type = entry->type;
            if (expression_type) *expression_type = entry->expression_type;
            if (is_deprecated) *is_deprecated = entry->is_deprecated;
            if (deprecated_message) {
                *deprecated_message = entry->deprecated_message;
            }
            return true;
        }
    }
    return false;
}

/* ═══════════════════════════════════════
 * Parser Utilities
 * ═══════════════════════════════════════ */

static Token* peek(void) {
    return parser.cur;
}

static Token* previous(void) {
    return parser.prev;
}

static bool check(TokenType type) {
    return peek()->type == type;
}

static bool check_next(TokenType type) {
    return parser.cur && parser.cur->next &&
           parser.cur->next->type == type;
}

static bool at_end(void) {
    return check(TOK_EOF);
}

static Token* advance(void) {
    if (!at_end()) {
        parser.prev = parser.cur;
        parser.cur = parser.cur->next;
    }
    return previous();
}

static bool parser_split_cxx_template_close(void) {
    Token* shift;
    Token* second_close;
    if (!parser_cxx_mode || !parser.cur || parser.cur->type != TOK_RSHIFT) {
        return false;
    }
    shift = parser.cur;
    second_close = token_new(TOK_GT, shift->loc);
    if (!second_close) return false;
    ++second_close->loc.column;
    second_close->next = shift->next;
    shift->type = TOK_GT;
    shift->next = second_close;
    return true;
}

bool rcc_parser_cxx_split_template_close(void) {
    return parser_split_cxx_template_close();
}

static bool match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

static Token* expect(TokenType type, const char* msg) {
    if (type == TOK_GT && check(TOK_RSHIFT)) {
        (void)parser_split_cxx_template_close();
    }
    if (check(type)) {
        return advance();
    }
    rcc_error(peek()->loc, "expected %s, got '%s'", msg, token_type_str(peek()->type));
    return NULL;
}

/* A declaration can begin with a typedef name just as it can with a built-in
 * type keyword.  Keeping this test separate from is_type_start() is
 * intentional: recovery must not consume a known declaration start merely
 * because the preceding construct was malformed. */
static bool parser_is_recovery_declaration_start(const Token* token) {
    if (!token) return false;
    switch (token->type) {
        case TOK_TYPEDEF:
        case TOK_AUTO:
        case TOK_INT:
        case TOK_VOID:
        case TOK_CHAR:
        case TOK_SHORT:
        case TOK_LONG:
        case TOK_FLOAT:
        case TOK_DOUBLE:
        case TOK_SIGNED:
        case TOK_UNSIGNED:
        case TOK_STRUCT:
        case TOK_UNION:
        case TOK_ENUM:
        case TOK_CONST:
        case TOK_VOLATILE:
        case TOK_RESTRICT:
        case TOK_STATIC:
        case TOK_EXTERN:
        case TOK_REGISTER:
        case TOK_THREAD_LOCAL:
        case TOK__BOOL:
        case TOK__BITINT:
        case TOK__COMPLEX:
        case TOK__IMAGINARY:
        case TOK__ATOMIC:
        case TOK__NORETURN:
        case TOK__ALIGNAS:
        case TOK_STATIC_ASSERT:
        case TOK_INLINE:
        case TOK_ASM:
            return true;
        case TOK_CLASS:
        case TOK_NAMESPACE:
        case TOK_TEMPLATE:
        case TOK_TYPENAME:
        case TOK_USING:
        case TOK_FRIEND:
        case TOK_EXPLICIT:
        case TOK_MUTABLE:
        case TOK_CONSTEXPR:
        case TOK_CONSTEVAL:
        case TOK_CONSTINIT:
        case TOK_CONCEPT:
        case TOK_REQUIRES:
            return parser_cxx_mode;
        case TOK_BOOL:
        case TOK_WCHAR_T:
        case TOK_CHAR16_T:
        case TOK_CHAR32_T:
        case TOK_CHAR8_T:
            return parser_cxx_mode;
        case TOK_IDENT:
            return parser_lookup_type(token->value.str_val) != NULL;
        default:
            return false;
    }
}

static void synchronize(void) {
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;
    TokenType first_type;
    if (at_end()) return;
    first_type = advance()->type;
    if (first_type == TOK_LPAREN) ++paren_depth;
    else if (first_type == TOK_LBRACKET) ++bracket_depth;
    else if (first_type == TOK_LBRACE) ++brace_depth;
    if (first_type == TOK_SEMICOLON) return;
    while (!at_end()) {
        TokenType type = peek()->type;
        if (type == TOK_SEMICOLON && paren_depth == 0 &&
            bracket_depth == 0 && brace_depth == 0) {
            advance();
            return;
        }
        if (type == TOK_RPAREN) {
            if (paren_depth == 0) return;
            --paren_depth;
            advance();
            continue;
        }
        if (type == TOK_RBRACKET) {
            if (bracket_depth == 0) return;
            --bracket_depth;
            advance();
            continue;
        }
        if (type == TOK_RBRACE) {
            if (brace_depth == 0 && paren_depth == 0 && bracket_depth == 0) {
                return;
            }
            if (brace_depth > 0) --brace_depth;
            advance();
            continue;
        }
        if (paren_depth == 0 && bracket_depth == 0 && brace_depth == 0) {
            switch (type) {
                case TOK_IF:
                case TOK_WHILE:
                case TOK_DO:
                case TOK_FOR:
                case TOK_SWITCH:
                case TOK_BREAK:
                case TOK_CONTINUE:
                case TOK_GOTO:
                case TOK_RETURN:
                case TOK_TRY:
                case TOK_CATCH:
                case TOK_THROW:
                    return;
                default:
                    if (parser_is_recovery_declaration_start(peek())) {
                        return;
                    }
                    break;
            }
        }
        if (type == TOK_LPAREN) ++paren_depth;
        else if (type == TOK_LBRACKET) ++bracket_depth;
        else if (type == TOK_LBRACE) ++brace_depth;
        advance();
    }
}

/* Forward declarations */
Expr* parse_expression(void);  /* Exported for C++ parser */
static Expr* parse_assignment(void);
static Expr* parse_initializer(void);

/* C++ field parsing reuses the complete shared initializer grammar. */
Expr* rcc_parser_parse_initializer(void);
static Expr* parse_unary(void);

/* C++ fold parsing needs one unary operand for a binary-fold initializer,
 * without allowing the ordinary binary-expression parser to consume the fold
 * operator itself. */
Expr* rcc_parse_cxx_fold_operand(void) {
    return parse_unary();
}
static Stmt* parse_statement(void);
Stmt* parse_declaration(void);  /* Exported for C++ parser */
static Type* parse_type_spec(void);
static bool is_type_start(void);
static Type* parse_declarator(Type* base_type, const char** name,
                              DeclList** parameters);

static Type* generic_selection_type(Type* type) {
    return type_generic_selection_control(type);
}

/* Evaluate the integer-constant-expression subset required by C17
 * _Static_assert.  Values retain their type so unsigned comparisons and
 * width-limited wrap follow the same target model as semantic analysis. */
typedef struct {
    uint64_t bits;
    Type* type;
} IntegerConstantValue;

static uint64_t integer_type_mask(const Type* type) {
    unsigned bits = type_integer_width(type);
    if (bits == 0u) return 0u;
    return bits >= 64u ? UINT64_MAX : (UINT64_C(1) << bits) - 1u;
}

static int64_t integer_constant_signed(IntegerConstantValue value) {
    unsigned bits = type_integer_width(value.type);
    if (bits == 0u) bits = 64u;
    uint64_t masked = value.bits & integer_type_mask(value.type);
    if (bits < 64u && (masked & (UINT64_C(1) << (bits - 1u))) != 0u) {
        masked |= ~integer_type_mask(value.type);
    }
    return (int64_t)masked;
}

static IntegerConstantValue integer_constant_convert(
    IntegerConstantValue value, Type* target) {
    uint64_t bits = value.bits & integer_type_mask(value.type);
    if (value.type && !value.type->is_unsigned &&
        integer_constant_signed(value) < 0) {
        bits = (uint64_t)integer_constant_signed(value);
    }
    value.bits = bits & integer_type_mask(target);
    value.type = target;
    return value;
}

static bool parser_integer_constant_signed_range(Type* type,
                                                 int64_t* minimum,
                                                 int64_t* maximum) {
    unsigned width;
    uint64_t sign_bit;
    if (!type || type->is_unsigned || !type_is_integer(type) ||
        (!minimum && !maximum)) {
        return false;
    }
    width = type_integer_width(type);
    if (width == 0u || width > 64u) return false;
    if (width == 64u) {
        if (minimum) *minimum = INT64_MIN;
        if (maximum) *maximum = INT64_MAX;
        return true;
    }
    sign_bit = UINT64_C(1) << (width - 1u);
    if (minimum) *minimum = -(int64_t)sign_bit;
    if (maximum) *maximum = (int64_t)(sign_bit - 1u);
    return true;
}

static bool parser_integer_constant_signed_fits(Type* type, int64_t value) {
    int64_t minimum;
    int64_t maximum;
    return parser_integer_constant_signed_range(type, &minimum, &maximum) &&
           value >= minimum && value <= maximum;
}

static bool parser_integer_constant_add(int64_t left, int64_t right,
                                        int64_t* result) {
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_add_overflow(left, right, result);
#else
    if ((right > 0 && left > INT64_MAX - right) ||
        (right < 0 && left < INT64_MIN - right)) return false;
    *result = left + right;
    return true;
#endif
}

static bool parser_integer_constant_sub(int64_t left, int64_t right,
                                        int64_t* result) {
#if defined(__GNUC__) || defined(__clang__)
    return !__builtin_sub_overflow(left, right, result);
#else
    if ((right < 0 && left > INT64_MAX + right) ||
        (right > 0 && left < INT64_MIN + right)) return false;
    *result = left - right;
    return true;
#endif
}

static bool parser_integer_constant_mul(int64_t left, int64_t right,
                                        int64_t* result) {
    if (left > 0) {
        if ((right > 0 && left > INT64_MAX / right) ||
            (right < 0 && right < INT64_MIN / left)) return false;
    } else if (left < 0) {
        if ((right > 0 && left < INT64_MIN / right) ||
            (right < 0 && left < INT64_MAX / right)) return false;
    }
    *result = left * right;
    return true;
}

typedef struct ParserOverflowWide {
    uint64_t low;
    uint64_t high;
} ParserOverflowWide;

static uint64_t parser_overflow_p_operand_bits(
    IntegerConstantValue value) {
    unsigned width = value.type ? type_integer_width(value.type) : 0u;
    uint64_t mask = width == 0u ? 0u : integer_type_mask(value.type);
    uint64_t bits = value.bits & mask;
    if (value.type && !value.type->is_unsigned && width > 0u && width < 64u &&
        (bits & (UINT64_C(1) << (width - 1u))) != 0u) {
        bits |= ~mask;
    }
    return bits;
}

static ParserOverflowWide parser_overflow_p_add(
    ParserOverflowWide left, ParserOverflowWide right) {
    ParserOverflowWide result;
    result.low = left.low + right.low;
    result.high = left.high + right.high + (result.low < left.low);
    return result;
}

static ParserOverflowWide parser_overflow_p_subtract(
    ParserOverflowWide left, ParserOverflowWide right) {
    ParserOverflowWide result;
    result.low = left.low - right.low;
    result.high = left.high - right.high - (left.low < right.low);
    return result;
}

static ParserOverflowWide parser_overflow_p_multiply_unsigned(
    uint64_t left, uint64_t right) {
    const uint64_t mask = UINT64_C(0xffffffff);
    uint64_t left_low = left & mask;
    uint64_t left_high = left >> 32;
    uint64_t right_low = right & mask;
    uint64_t right_high = right >> 32;
    uint64_t word0 = left_low * right_low;
    uint64_t intermediate = left_high * right_low + (word0 >> 32);
    uint64_t word1 = intermediate & mask;
    uint64_t word2 = intermediate >> 32;
    ParserOverflowWide result;
    word1 += left_low * right_high;
    result.high = left_high * right_high + word2 + (word1 >> 32);
    result.low = (word1 << 32) | (word0 & mask);
    return result;
}

static ParserOverflowWide parser_overflow_p_negate(
    ParserOverflowWide value) {
    ParserOverflowWide result;
    result.low = ~value.low + 1u;
    result.high = ~value.high + (result.low == 0u);
    return result;
}

static bool parser_overflow_p_fits(
    ParserOverflowWide value, unsigned precision, bool result_unsigned) {
    uint64_t low_mask;
    uint64_t upper_low;
    bool negative;
    if (precision == 0u || precision > 64u) return false;
    if (result_unsigned) {
        return value.high == 0u &&
            (precision == 64u || (value.low >> precision) == 0u);
    }
    negative = ((value.low >> (precision - 1u)) & 1u) != 0u;
    if (precision == 64u) {
        return value.high == (negative ? UINT64_MAX : 0u);
    }
    low_mask = (UINT64_C(1) << precision) - 1u;
    upper_low = value.low & ~low_mask;
    return value.high == (negative ? UINT64_MAX : 0u) &&
        upper_low == (negative ? ~low_mask : 0u);
}

static bool parser_overflow_p_value(
    unsigned operation, IntegerConstantValue left,
    IntegerConstantValue right, unsigned precision, bool result_unsigned) {
    uint64_t left_bits = parser_overflow_p_operand_bits(left);
    uint64_t right_bits = parser_overflow_p_operand_bits(right);
    ParserOverflowWide exact;
    if (operation == 0u) {
        ParserOverflowWide left_wide = {
            left_bits,
            left.type && !left.type->is_unsigned &&
                    (left_bits >> 63) != 0u ? UINT64_MAX : 0u
        };
        ParserOverflowWide right_wide = {
            right_bits,
            right.type && !right.type->is_unsigned &&
                    (right_bits >> 63) != 0u ? UINT64_MAX : 0u
        };
        exact = parser_overflow_p_add(left_wide, right_wide);
    } else if (operation == 1u) {
        ParserOverflowWide left_wide = {
            left_bits,
            left.type && !left.type->is_unsigned &&
                    (left_bits >> 63) != 0u ? UINT64_MAX : 0u
        };
        ParserOverflowWide right_wide = {
            right_bits,
            right.type && !right.type->is_unsigned &&
                    (right_bits >> 63) != 0u ? UINT64_MAX : 0u
        };
        exact = parser_overflow_p_subtract(left_wide, right_wide);
    } else if (operation == 2u) {
        bool left_negative = left.type && !left.type->is_unsigned &&
                             (left_bits >> 63) != 0u;
        bool right_negative = right.type && !right.type->is_unsigned &&
                              (right_bits >> 63) != 0u;
        uint64_t left_magnitude = left_negative ? 0u - left_bits : left_bits;
        uint64_t right_magnitude = right_negative ? 0u - right_bits : right_bits;
        exact = parser_overflow_p_multiply_unsigned(
            left_magnitude, right_magnitude);
        if (left_negative != right_negative) {
            exact = parser_overflow_p_negate(exact);
        }
    } else {
        return true;
    }
    return !parser_overflow_p_fits(exact, precision, result_unsigned);
}

static bool parser_overflow_p_argument_has_side_effect(
    const Expr* expression) {
    if (!expression) return false;
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
        case EXPR_CXX_TYPE_TRAIT:
            return false;
        case EXPR_BUILTIN_TYPES_COMPAT:
        case EXPR_BUILTIN_OFFSETOF:
        case EXPR_CALL:
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
        case EXPR_VA_START:
        case EXPR_VA_END:
        case EXPR_VA_COPY:
        case EXPR_VA_ARG:
            return true;
        case EXPR_IDENT:
            return expression->type && expression->type->is_volatile;
        case EXPR_POS:
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
        case EXPR_ADDR:
        case EXPR_CAST:
            return parser_overflow_p_argument_has_side_effect(
                expression->kind == EXPR_CAST ? expression->cast_expr
                                                : expression->unary_operand);
        case EXPR_DEREF:
            return (expression->type && expression->type->is_volatile) ||
                parser_overflow_p_argument_has_side_effect(
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
        case EXPR_SPACESHIP:
        case EXPR_AND:
        case EXPR_OR:
        case EXPR_COMMA:
        case EXPR_CXX_MEMBER_PTR_DOT:
        case EXPR_CXX_MEMBER_PTR_ARROW:
            return (expression->type && expression->type->is_volatile) ||
                parser_overflow_p_argument_has_side_effect(
                    expression->binary_lhs) ||
                parser_overflow_p_argument_has_side_effect(
                    expression->binary_rhs);
        case EXPR_COND:
            return parser_overflow_p_argument_has_side_effect(
                       expression->cond_test) ||
                parser_overflow_p_argument_has_side_effect(
                       expression->cond_then) ||
                parser_overflow_p_argument_has_side_effect(
                       expression->cond_else);
        case EXPR_INDEX:
            return (expression->type && expression->type->is_volatile) ||
                parser_overflow_p_argument_has_side_effect(
                    expression->index_base) ||
                parser_overflow_p_argument_has_side_effect(
                    expression->index_expr);
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            return (expression->type && expression->type->is_volatile) ||
                parser_overflow_p_argument_has_side_effect(
                    expression->member_base);
        case EXPR_COMPOUND:
            for (const ExprList* item = expression->compound_init;
                 item; item = item->next) {
                if (parser_overflow_p_argument_has_side_effect(item->expr)) {
                    return true;
                }
            }
            return false;
        case EXPR_GENERIC:
        case EXPR_CXX_FOLD:
            return true;
        case EXPR_CXX_REQUIRES:
            return false;
    }
    return true;
}

static bool eval_integer_constant_typed(Expr* expr,
                                        IntegerConstantValue* value) {
    IntegerConstantValue left;
    IntegerConstantValue right;
    Type* common;
    uint64_t count;
    unsigned width;

    if (!expr || !value) return false;
    switch (expr->kind) {
        case EXPR_CALL: {
            const char* name = expr->call_func &&
                    expr->call_func->kind == EXPR_IDENT
                ? expr->call_func->ident_name : NULL;
            ExprList* first = expr->call_args;
            ExprList* second = first ? first->next : NULL;
            ExprList* third = second ? second->next : NULL;
            unsigned operation;
            unsigned precision;
            IntegerConstantValue left_value;
            IntegerConstantValue right_value;
            Type* result_type;
            if (!name || !first || !first->expr || !second ||
                !second->expr || !third || !third->expr || third->next) {
                return false;
            }
            if (parser_overflow_p_argument_has_side_effect(third->expr)) {
                return false;
            }
            if (strcmp(name, "__builtin_add_overflow_p") == 0) {
                operation = 0u;
            } else if (strcmp(name, "__builtin_sub_overflow_p") == 0) {
                operation = 1u;
            } else if (strcmp(name, "__builtin_mul_overflow_p") == 0) {
                operation = 2u;
            } else {
                return false;
            }
            result_type = third->expr->type;
            if (!result_type || !type_is_integer(result_type) ||
                result_type->size <= 0 || result_type->size > 8 ||
                !type_is_integer(first->expr->type) ||
                first->expr->type->size <= 0 ||
                first->expr->type->size > 8 ||
                !type_is_integer(second->expr->type) ||
                second->expr->type->size <= 0 ||
                second->expr->type->size > 8 ||
                !eval_integer_constant_typed(first->expr, &left_value) ||
                !eval_integer_constant_typed(second->expr, &right_value)) {
                return false;
            }
            precision = third->expr->member_field &&
                    third->expr->member_field->is_bitfield
                ? third->expr->member_field->bit_width
                : result_type->is_bit_precise
                    ? result_type->bit_width
                    : (unsigned)result_type->size * 8u;
            if (precision == 0u || precision > 64u) return false;
            value->bits = parser_overflow_p_value(
                operation, left_value, right_value, precision,
                result_type->is_unsigned) ? 1u : 0u;
            value->type = expr->type ? expr->type : type_int;
            return true;
        }
        case EXPR_INT_LIT:
            if (expr->is_cxx_nullptr) return false;
            value->bits = (uint64_t)expr->int_val &
                          integer_type_mask(expr->type);
            value->type = expr->type ? expr->type : type_int;
            return true;
        case EXPR_CHAR_LIT:
            value->bits = expr_char_code_unit(expr);
            value->type = expr->type ? expr->type : type_int;
            return true;
        case EXPR_IDENT:
            if (expr->ident_decl &&
                expr->ident_decl->kind == DECL_VAR &&
                expr->ident_decl->var_is_constexpr &&
                expr->ident_decl->var_init &&
                !expr->ident_decl->var_is_being_constant_evaluated) {
                Decl* declaration = expr->ident_decl;
                bool evaluated;
                declaration->var_is_being_constant_evaluated = true;
                evaluated = eval_integer_constant_typed(
                    declaration->var_init, value);
                declaration->var_is_being_constant_evaluated = false;
                return evaluated;
            }
            return false;
        case EXPR_MEMBER:
            if (expr->member_base && expr->member_base->type &&
                expr->member_base->type->cxx_class && expr->member_name) {
                for (struct CxxMember* member =
                         expr->member_base->type->cxx_class->members;
                     member; member = member->next) {
                    Decl* declaration = member->decl;
                    const char* declared_name =
                        declaration ? declaration->name : NULL;
                    const char* separator = declared_name
                        ? strrchr(declared_name, ':') : NULL;
                    if (separator && separator > declared_name &&
                        separator[-1] == ':') {
                        declared_name = separator + 1;
                    }
                    if (!member->is_static || !declaration ||
                        declaration->kind != DECL_VAR ||
                        !declared_name ||
                        strcmp(declared_name, expr->member_name) != 0 ||
                        !declaration->var_is_constexpr ||
                        !declaration->var_init ||
                        declaration->var_is_being_constant_evaluated) {
                        continue;
                    }
                    bool evaluated;
                    declaration->var_is_being_constant_evaluated = true;
                    evaluated = eval_integer_constant_typed(
                        declaration->var_init, value);
                    declaration->var_is_being_constant_evaluated = false;
                    return evaluated;
                }
            }
            return false;
        case EXPR_CXX_TYPE_TRAIT: {
            bool trait_value;
            if (!parser_cxx_type_trait_value(
                    expr->cxx_type_trait_kind,
                    expr->cxx_type_trait_type,
                    expr->cxx_type_trait_other_type,
                    expr->cxx_type_trait_arguments, &trait_value)) {
                return false;
            }
            value->bits = trait_value ? 1u : 0u;
            value->type = expr->type ? expr->type : type_bool;
            return true;
        }
        case EXPR_POS:
        case EXPR_NEG:
        case EXPR_BITNOT:
            if (!eval_integer_constant_typed(expr->unary_operand, &left)) {
                return false;
            }
            common = left.type;
            if (common->kind == TYPE_ENUM && !common->enum_is_scoped) {
                common = common->enum_underlying_type
                    ? common->enum_underlying_type : type_int;
            }
            if (common->kind < TYPE_INT) common = type_int;
            left = integer_constant_convert(left, common);
            if (expr->kind == EXPR_NEG && !common->is_unsigned) {
                int64_t minimum;
                if (!parser_integer_constant_signed_range(
                        common, &minimum, NULL) ||
                    integer_constant_signed(left) == minimum) {
                    return false;
                }
            }
            value->bits = expr->kind == EXPR_POS ? left.bits
                : expr->kind == EXPR_NEG ? 0u - left.bits : ~left.bits;
            value->bits &= integer_type_mask(common);
            value->type = common;
            return true;
        case EXPR_NOT:
            if (!eval_integer_constant_typed(expr->unary_operand, &left)) {
                return false;
            }
            value->bits = (left.bits & integer_type_mask(left.type)) == 0u;
            value->type = type_int;
            return true;
        case EXPR_SIZEOF: {
            Type* measured = expr->sizeof_type;
            if (!measured && expr->unary_operand) {
                measured = expr->unary_operand->type;
            }
            measured = type_remove_reference(measured);
            if (!measured || measured->size <= 0) return false;
            value->bits = (uint64_t)measured->size;
            value->type = type_uint;
            return true;
        }
        case EXPR_ALIGNOF: {
            Type* measured = type_remove_reference(expr->sizeof_type);
            if (!measured || measured->align <= 0) {
                return false;
            }
            value->bits = (uint64_t)measured->align;
            value->type = type_uint;
            return true;
        }
        case EXPR_CAST:
            if (!expr->cast_type || !type_is_integer(expr->cast_type) ||
                !eval_integer_constant_typed(expr->cast_expr, &left)) {
                return false;
            }
            *value = integer_constant_convert(left, expr->cast_type);
            return true;
        case EXPR_GENERIC: {
            Type* control = expr->generic_control
                ? generic_selection_type(expr->generic_control->type) : NULL;
            GenericAssociation* selected = NULL;
            GenericAssociation* fallback = NULL;
            if (!control) return false;
            for (GenericAssociation* association =
                     expr->generic_associations;
                 association; association = association->next) {
                if (!association->type) fallback = association;
                else if (type_is_compatible_qualified(
                             control, association->type)) {
                    selected = association;
                }
            }
            selected = selected ? selected : fallback;
            return selected &&
                   eval_integer_constant_typed(selected->expr, value);
        }
        case EXPR_AND:
            if (!eval_integer_constant_typed(expr->binary_lhs, &left)) {
                return false;
            }
            if ((left.bits & integer_type_mask(left.type)) == 0u) {
                value->bits = 0u;
            } else {
                if (!eval_integer_constant_typed(expr->binary_rhs, &right)) {
                    return false;
                }
                value->bits =
                    (right.bits & integer_type_mask(right.type)) != 0u;
            }
            value->type = type_int;
            return true;
        case EXPR_OR:
            if (!eval_integer_constant_typed(expr->binary_lhs, &left)) {
                return false;
            }
            if ((left.bits & integer_type_mask(left.type)) != 0u) {
                value->bits = 1u;
            } else {
                if (!eval_integer_constant_typed(expr->binary_rhs, &right)) {
                    return false;
                }
                value->bits =
                    (right.bits & integer_type_mask(right.type)) != 0u;
            }
            value->type = type_int;
            return true;
        case EXPR_COND:
            if (!eval_integer_constant_typed(expr->cond_test, &left)) {
                return false;
            }
            return eval_integer_constant_typed(
                (left.bits & integer_type_mask(left.type)) != 0u
                    ? expr->cond_then : expr->cond_else,
                value);
        default:
            break;
    }

    switch (expr->kind) {
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
        case EXPR_COMMA:
            break;
        default:
            return false;
    }
    if (!eval_integer_constant_typed(expr->binary_lhs, &left) ||
        !eval_integer_constant_typed(expr->binary_rhs, &right)) {
        return false;
    }
    if (expr->kind == EXPR_COMMA) {
        *value = right;
        return true;
    }
    if (expr->kind == EXPR_LSHIFT || expr->kind == EXPR_RSHIFT) {
        common = left.type->kind < TYPE_INT ? type_int : left.type;
        left = integer_constant_convert(left, common);
        if (right.type->is_unsigned) {
            count = right.bits & integer_type_mask(right.type);
        } else {
            int64_t signed_count = integer_constant_signed(right);
            if (signed_count < 0) return false;
            count = (uint64_t)signed_count;
        }
        width = type_integer_width(common);
        if (count >= width) return false;
        if (expr->kind == EXPR_LSHIFT) {
            if (!parser_cxx_mode && !common->is_unsigned) {
                int64_t maximum;
                int64_t signed_left = integer_constant_signed(left);
                if (signed_left < 0 ||
                    !parser_integer_constant_signed_range(
                        common, NULL, &maximum) ||
                    (count != 0u &&
                     signed_left > (maximum >> (unsigned)count))) {
                    return false;
                }
            }
            value->bits = (left.bits << (unsigned)count) &
                          integer_type_mask(common);
        } else if (common->is_unsigned) {
            value->bits = left.bits >> (unsigned)count;
        } else {
            value->bits = (uint64_t)(integer_constant_signed(left) >>
                                     (unsigned)count) &
                          integer_type_mask(common);
        }
        value->type = common;
        return true;
    }

    common = type_common(left.type, right.type);
    left = integer_constant_convert(left, common);
    right = integer_constant_convert(right, common);
    if (expr->kind >= EXPR_EQ && expr->kind <= EXPR_GE) {
        bool result;
        if (expr->kind == EXPR_EQ) result = left.bits == right.bits;
        else if (expr->kind == EXPR_NE) result = left.bits != right.bits;
        else if (common->is_unsigned) {
            if (expr->kind == EXPR_LT) result = left.bits < right.bits;
            else if (expr->kind == EXPR_GT) result = left.bits > right.bits;
            else if (expr->kind == EXPR_LE) result = left.bits <= right.bits;
            else result = left.bits >= right.bits;
        } else {
            int64_t signed_left = integer_constant_signed(left);
            int64_t signed_right = integer_constant_signed(right);
            if (expr->kind == EXPR_LT) result = signed_left < signed_right;
            else if (expr->kind == EXPR_GT) result = signed_left > signed_right;
            else if (expr->kind == EXPR_LE) result = signed_left <= signed_right;
            else result = signed_left >= signed_right;
        }
        value->bits = result;
        value->type = type_int;
        return true;
    }
    if (expr->kind == EXPR_SPACESHIP) {
        if (common->is_unsigned) {
            if (left.bits < right.bits) value->bits = UINT64_MAX;
            else if (left.bits > right.bits) value->bits = 1u;
            else value->bits = 0u;
        } else {
            int64_t signed_left = integer_constant_signed(left);
            int64_t signed_right = integer_constant_signed(right);
            if (signed_left < signed_right) value->bits = UINT64_MAX;
            else if (signed_left > signed_right) value->bits = 1u;
            else value->bits = 0u;
        }
        value->type = type_int;
        value->bits &= integer_type_mask(type_int);
        return true;
    }

    value->type = common;
    if (!common->is_unsigned &&
        (expr->kind == EXPR_ADD || expr->kind == EXPR_SUB ||
         expr->kind == EXPR_MUL)) {
        int64_t signed_left = integer_constant_signed(left);
        int64_t signed_right = integer_constant_signed(right);
        int64_t result;
        bool valid = expr->kind == EXPR_ADD
            ? parser_integer_constant_add(signed_left, signed_right,
                                          &result)
            : expr->kind == EXPR_SUB
                ? parser_integer_constant_sub(signed_left, signed_right,
                                              &result)
                : parser_integer_constant_mul(signed_left, signed_right,
                                              &result);
        if (!valid || !parser_integer_constant_signed_fits(common, result)) {
            return false;
        }
        value->bits = (uint64_t)result & integer_type_mask(common);
        return true;
    }
    switch (expr->kind) {
        case EXPR_ADD: value->bits = left.bits + right.bits; break;
        case EXPR_SUB: value->bits = left.bits - right.bits; break;
        case EXPR_MUL: value->bits = left.bits * right.bits; break;
        case EXPR_BITAND: value->bits = left.bits & right.bits; break;
        case EXPR_BITOR: value->bits = left.bits | right.bits; break;
        case EXPR_BITXOR: value->bits = left.bits ^ right.bits; break;
        case EXPR_DIV:
        case EXPR_MOD:
            if (right.bits == 0u) return false;
            if (common->is_unsigned) {
                value->bits = expr->kind == EXPR_DIV
                    ? left.bits / right.bits : left.bits % right.bits;
            } else {
                int64_t signed_left = integer_constant_signed(left);
                int64_t signed_right = integer_constant_signed(right);
                unsigned common_width = type_integer_width(common);
                int64_t minimum = common_width >= 64u
                    ? INT64_MIN
                    : -(INT64_C(1) << (common_width - 1u));
                if (signed_right == 0 ||
                    (signed_left == minimum && signed_right == -1)) {
                    return false;
                }
                value->bits = (uint64_t)(expr->kind == EXPR_DIV
                    ? signed_left / signed_right : signed_left % signed_right);
            }
            break;
        default:
            return false;
    }
    value->bits &= integer_type_mask(common);
    return true;
}

static bool eval_integer_constant(Expr* expr, int64_t* value) {
    IntegerConstantValue evaluated;
    if (!value || !eval_integer_constant_typed(expr, &evaluated)) return false;
    *value = evaluated.type && evaluated.type->is_unsigned
        ? (int64_t)(evaluated.bits & integer_type_mask(evaluated.type))
        : integer_constant_signed(evaluated);
    return true;
}

bool expr_eval_integer_constant(Expr* expr, int64_t* value) {
    return eval_integer_constant(expr, value);
}

bool expr_eval_integer_constant_u64(Expr* expr, uint64_t* value) {
    IntegerConstantValue evaluated;
    if (!value || !eval_integer_constant_typed(expr, &evaluated)) {
        return false;
    }
    if (evaluated.type && evaluated.type->is_unsigned) {
        *value = evaluated.bits & integer_type_mask(evaluated.type);
    } else {
        *value = (uint64_t)integer_constant_signed(evaluated);
    }
    return true;
}

static bool pending_weak_attribute;
static bool pending_naked_attribute;
static bool pending_c_noreturn_attribute;
static bool pending_c_maybe_unused_attribute;
static bool pending_c_fallthrough_attribute;
static SourceLoc pending_c_fallthrough_loc;
static bool pending_c_function_is_reproducible;
static bool pending_c_function_is_unsequenced;
static SourceLoc pending_c_function_type_attribute_loc;
static bool pending_c_nodiscard_attribute;
static const char* pending_c_nodiscard_message;
static bool pending_c_deprecated_attribute;
static const char* pending_c_deprecated_message;

static bool take_weak_attribute(void) {
    bool result = pending_weak_attribute;
    pending_weak_attribute = false;
    return result;
}

static bool take_naked_attribute(void) {
    bool result = pending_naked_attribute;
    pending_naked_attribute = false;
    return result;
}

static bool take_c_noreturn_attribute(void) {
    bool result = pending_c_noreturn_attribute;
    pending_c_noreturn_attribute = false;
    return result;
}

static bool take_c_maybe_unused_attribute(void) {
    bool result = pending_c_maybe_unused_attribute;
    pending_c_maybe_unused_attribute = false;
    return result;
}

static bool take_c_fallthrough_attribute(SourceLoc* loc) {
    bool result = pending_c_fallthrough_attribute;
    if (result && loc) *loc = pending_c_fallthrough_loc;
    pending_c_fallthrough_attribute = false;
    memset(&pending_c_fallthrough_loc, 0,
           sizeof(pending_c_fallthrough_loc));
    return result;
}

static void parser_accumulate_c_function_type_attributes(
    bool* reproducible, bool* unsequenced, SourceLoc* loc) {
    bool already_present = (reproducible && *reproducible) ||
                           (unsequenced && *unsequenced);
    if (pending_c_function_is_reproducible) {
        if (reproducible) *reproducible = true;
        if (loc && !already_present) {
            *loc = pending_c_function_type_attribute_loc;
            already_present = true;
        }
    }
    if (pending_c_function_is_unsequenced) {
        if (unsequenced) *unsequenced = true;
        if (loc && !already_present) {
            *loc = pending_c_function_type_attribute_loc;
        }
    }
    pending_c_function_is_reproducible = false;
    pending_c_function_is_unsequenced = false;
    memset(&pending_c_function_type_attribute_loc, 0,
           sizeof(pending_c_function_type_attribute_loc));
}

static void parser_apply_c_function_type_attributes(
    Type* type, bool reproducible, bool unsequenced, SourceLoc loc) {
    Type* function_type = type;
    if (!reproducible && !unsequenced) return;
    while (function_type &&
           (function_type->kind == TYPE_PTR ||
            function_type->kind == TYPE_ARRAY)) {
        function_type = function_type->base;
    }
    if (!function_type || function_type->kind != TYPE_FUNC) {
        rcc_error(loc,
                  "C23 [[reproducible]] and [[unsequenced]] require a function type");
        return;
    }
    function_type->c23_function_is_reproducible =
        function_type->c23_function_is_reproducible || reproducible;
    function_type->c23_function_is_unsequenced =
        function_type->c23_function_is_unsequenced || unsequenced;
}

static void parser_apply_pending_c_function_type_attributes(
    Type* type, SourceLoc fallback_loc) {
    bool reproducible = false;
    bool unsequenced = false;
    SourceLoc loc = fallback_loc;
    parser_accumulate_c_function_type_attributes(
        &reproducible, &unsequenced, &loc);
    parser_apply_c_function_type_attributes(
        type, reproducible, unsequenced, loc);
}

static bool take_c_nodiscard_attribute(const char** message) {
    bool result = pending_c_nodiscard_attribute;
    if (message) *message = result ? pending_c_nodiscard_message : NULL;
    pending_c_nodiscard_attribute = false;
    pending_c_nodiscard_message = NULL;
    return result;
}

static bool take_c_deprecated_attribute(const char** message) {
    bool result = pending_c_deprecated_attribute;
    if (message) {
        *message = result ? pending_c_deprecated_message : NULL;
    }
    pending_c_deprecated_attribute = false;
    pending_c_deprecated_message = NULL;
    return result;
}

static void parser_accumulate_c_nodiscard_attribute(
    bool* present, const char** message) {
    const char* parsed_message = NULL;
    if (take_c_nodiscard_attribute(&parsed_message)) {
        if (present) *present = true;
        if (message && !*message) *message = parsed_message;
    }
}

static void parser_accumulate_c_maybe_unused_attribute(bool* present) {
    if (take_c_maybe_unused_attribute() && present) *present = true;
}

static void parser_accumulate_c_deprecated_attribute(
    bool* present, const char** message) {
    const char* parsed_message = NULL;
    if (take_c_deprecated_attribute(&parsed_message)) {
        if (present) *present = true;
        if (message && !*message) *message = parsed_message;
    }
}

static bool parser_skip_c_attribute_arguments(SourceLoc loc) {
    int close_tokens[256];
    size_t depth = 1u;
    close_tokens[0] = TOK_RPAREN;
    advance();

    while (!at_end()) {
        int token = peek()->type;
        if (token == TOK_LPAREN || token == TOK_LBRACKET ||
            token == TOK_LBRACE) {
            if (depth >= sizeof(close_tokens) / sizeof(close_tokens[0])) {
                rcc_error(loc, "C23 attribute argument nesting is too deep");
                advance();
                return false;
            }
            close_tokens[depth++] = token == TOK_LPAREN ? TOK_RPAREN
                : token == TOK_LBRACKET ? TOK_RBRACKET : TOK_RBRACE;
        } else if (token == TOK_RPAREN || token == TOK_RBRACKET ||
                   token == TOK_RBRACE) {
            if (close_tokens[depth - 1u] != token) {
                rcc_error(peek()->loc,
                          "mismatched delimiter in C23 attribute argument");
                advance();
                return false;
            }
            if (--depth == 0u) {
                advance();
                return true;
            }
        }
        advance();
    }

    rcc_error(loc, "unterminated C23 attribute argument");
    return false;
}

static void parser_skip_c_attribute_sequence(void) {
    SourceLoc sequence_loc = peek()->loc;
    bool terminated = false;
    bool sequence_has_fallthrough = false;
    SourceLoc fallthrough_loc = sequence_loc;
    advance();
    advance();

    while (!at_end()) {
        bool is_noreturn = false;
        bool is_maybe_unused = false;
        bool is_fallthrough = false;
        bool is_reproducible = false;
        bool is_unsequenced = false;
        bool is_nodiscard = false;
        bool is_deprecated = false;
        bool has_prefix = false;
        bool has_arguments = false;
        bool valid_nodiscard_arguments = true;
        bool valid_deprecated_arguments = true;
        const char* nodiscard_message = NULL;
        const char* deprecated_message = NULL;
        SourceLoc attribute_loc = peek()->loc;

        if (check(TOK_RBRACKET) && check_next(TOK_RBRACKET)) {
            advance();
            advance();
            terminated = true;
            break;
        }
        if (match(TOK_COMMA)) continue;

        if (check(TOK_IDENT)) {
            const char* name = peek()->value.str_val;
            is_noreturn = strcmp(name, "noreturn") == 0 ||
                          strcmp(name, "_Noreturn") == 0 ||
                          strcmp(name, "__noreturn__") == 0;
            is_nodiscard = strcmp(name, "nodiscard") == 0;
            is_maybe_unused = strcmp(name, "maybe_unused") == 0;
            is_fallthrough = strcmp(name, "fallthrough") == 0;
            is_reproducible = strcmp(name, "reproducible") == 0;
            is_unsequenced = strcmp(name, "unsequenced") == 0;
            is_deprecated = strcmp(name, "deprecated") == 0;
            advance();
        } else if (match(TOK__NORETURN)) {
            /* Keywords used as attribute tokens are identifiers here. */
            is_noreturn = true;
        } else {
            rcc_error(peek()->loc, "expected C23 attribute token");
            advance();
            continue;
        }

        if (match(TOK_SCOPE)) {
            has_prefix = true;
            if (!check(TOK_IDENT)) {
                rcc_error(peek()->loc,
                          "expected attribute name after namespace prefix");
                continue;
            }
            advance();
        }
        if (check(TOK_LPAREN)) {
            has_arguments = true;
            if (is_maybe_unused && !has_prefix) {
                rcc_error(attribute_loc,
                          "C23 [[maybe_unused]] does not accept arguments");
            }
            if (is_fallthrough && !has_prefix) {
                rcc_error(attribute_loc,
                          "C23 [[fallthrough]] does not accept arguments");
            }
            if (is_reproducible && !has_prefix) {
                rcc_error(attribute_loc,
                          "C23 [[reproducible]] does not accept arguments");
            }
            if (is_unsequenced && !has_prefix) {
                rcc_error(attribute_loc,
                          "C23 [[unsequenced]] does not accept arguments");
            }
            if ((is_nodiscard || is_deprecated) && !has_prefix) {
                advance();
                if (check(TOK_STRING_LIT) && check_next(TOK_RPAREN)) {
                    const char* parsed_message = advance()->value.str_val;
                    if (is_nodiscard) nodiscard_message = parsed_message;
                    if (is_deprecated) deprecated_message = parsed_message;
                    advance();
                } else {
                    if (is_nodiscard) {
                        rcc_error(attribute_loc,
                                  "C23 [[nodiscard]] argument must be a string literal");
                        valid_nodiscard_arguments = false;
                    } else {
                        rcc_error(attribute_loc,
                                  "C23 [[deprecated]] argument must be a string literal");
                        valid_deprecated_arguments = false;
                    }
                    (void)parser_skip_c_attribute_arguments(attribute_loc);
                }
            } else {
                (void)parser_skip_c_attribute_arguments(attribute_loc);
            }
        }

        if (is_maybe_unused && !has_prefix) {
            if (g_opts.c_standard < 23) {
                rcc_error(attribute_loc,
                          "C23 [[maybe_unused]] requires -std=c23");
            } else if (!has_arguments) {
                pending_c_maybe_unused_attribute = true;
            }
        }

        if (is_fallthrough && !has_prefix) {
            if (g_opts.c_standard < 23) {
                rcc_error(attribute_loc,
                          "C23 [[fallthrough]] requires -std=c23");
            } else if (!has_arguments) {
                sequence_has_fallthrough = true;
                fallthrough_loc = attribute_loc;
            }
        }

        if (is_reproducible && !has_prefix) {
            if (g_opts.c_standard < 23) {
                rcc_error(attribute_loc,
                          "C23 [[reproducible]] requires -std=c23");
            } else if (!has_arguments) {
                pending_c_function_is_reproducible = true;
                pending_c_function_type_attribute_loc = attribute_loc;
            }
        }
        if (is_unsequenced && !has_prefix) {
            if (g_opts.c_standard < 23) {
                rcc_error(attribute_loc,
                          "C23 [[unsequenced]] requires -std=c23");
            } else if (!has_arguments) {
                pending_c_function_is_unsequenced = true;
                pending_c_function_type_attribute_loc = attribute_loc;
            }
        }

        if (is_noreturn && !has_prefix) {
            if (g_opts.c_standard < 23) {
                rcc_error(attribute_loc,
                          "C23 [[noreturn]] requires -std=c23");
            } else if (has_arguments) {
                rcc_error(attribute_loc,
                          "C23 [[noreturn]] does not accept arguments");
            } else {
                pending_c_noreturn_attribute = true;
            }
        }
        if (is_nodiscard && !has_prefix) {
            if (g_opts.c_standard < 23) {
                rcc_error(attribute_loc,
                          "C23 [[nodiscard]] requires -std=c23");
            } else if (valid_nodiscard_arguments) {
                pending_c_nodiscard_attribute = true;
                if (!pending_c_nodiscard_message) {
                    pending_c_nodiscard_message = nodiscard_message;
                }
            }
        }
        if (is_deprecated && !has_prefix) {
            if (g_opts.c_standard < 23) {
                rcc_error(attribute_loc,
                          "C23 [[deprecated]] requires -std=c23");
            } else if (valid_deprecated_arguments) {
                pending_c_deprecated_attribute = true;
                if (!pending_c_deprecated_message) {
                    pending_c_deprecated_message = deprecated_message;
                }
            }
        }

        if (match(TOK_COMMA)) continue;
        if (!(check(TOK_RBRACKET) && check_next(TOK_RBRACKET))) {
            rcc_error(peek()->loc,
                      "expected ',' or ']]' after C23 attribute");
            advance();
        }
    }

    if (!terminated) {
        rcc_error(sequence_loc, "unterminated C23 attribute sequence");
    } else if (sequence_has_fallthrough) {
        if (!check(TOK_SEMICOLON)) {
            rcc_error(fallthrough_loc,
                      "C23 [[fallthrough]] must be a standalone attribute declaration");
        } else {
            pending_c_fallthrough_attribute = true;
            pending_c_fallthrough_loc = fallthrough_loc;
        }
    }
}

static void skip_attributes(void) {
    for (;;) {
        if (!parser_cxx_mode && check(TOK_LBRACKET) &&
            check_next(TOK_LBRACKET)) {
            parser_skip_c_attribute_sequence();
            continue;
        }
        if (!match(TOK___ATTRIBUTE__)) break;
        SourceLoc attribute_loc = previous()->loc;
        bool group_weak = false;
        bool weak_arguments = false;
        bool group_naked = false;
        if (match(TOK_LPAREN)) {
            int depth = 1;
            bool current_attribute_weak = false;
            while (!at_end() && depth > 0) {
                if (check(TOK_IDENT) && depth == 2) {
                    current_attribute_weak =
                        strcmp(peek()->value.str_val, "weak") == 0;
                    if (current_attribute_weak) group_weak = true;
                    if (strcmp(peek()->value.str_val, "naked") == 0) {
                        group_naked = true;
                    }
                    advance();
                } else if (match(TOK_LPAREN)) {
                    if (depth == 2 && current_attribute_weak) {
                        weak_arguments = true;
                    }
                    depth++;
                } else if (match(TOK_RPAREN)) {
                    depth--;
                    if (depth == 2) current_attribute_weak = false;
                } else {
                    if (depth == 2 && match(TOK_COMMA)) {
                        current_attribute_weak = false;
                    } else {
                        advance();
                    }
                }
            }
            if (group_weak) {
                if (weak_arguments) {
                    rcc_error(attribute_loc,
                              "weak attribute does not accept arguments");
                } else {
                    pending_weak_attribute = true;
                }
            }
            if (group_naked) pending_naked_attribute = true;
        }
    }
}

static Type* parse_type_name_declarator(Type* base_type, SourceLoc loc) {
    Type* type = parse_declarator(base_type, NULL, NULL);
    if (!parser_cxx_mode) {
        skip_attributes();
        parser_apply_pending_c_function_type_attributes(type, loc);
    }
    return type;
}

/* ═══════════════════════════════════════
 * Expression Parsing
 * ═══════════════════════════════════════ */

static bool generic_association_type_valid(Type* type) {
    return type && type->kind != TYPE_VOID && type->kind != TYPE_FUNC &&
           type_is_complete(type);
}

static Expr* parse_generic_selection(SourceLoc loc) {
    Expr* control;
    GenericAssociation* associations = NULL;
    bool have_default = false;

    expect(TOK_LPAREN, "(");
    control = parse_assignment();
    expect(TOK_COMMA, ",");
    if (check(TOK_RPAREN)) {
        rcc_error(peek()->loc,
                  "generic selection requires at least one association");
    }
    while (!check(TOK_RPAREN) && !at_end()) {
        SourceLoc association_loc = peek()->loc;
        Type* association_type = NULL;
        Expr* association_expression;

        if (match(TOK_DEFAULT)) {
            if (have_default) {
                rcc_error(association_loc,
                          "generic selection has more than one default association");
            }
            have_default = true;
        } else if (is_type_start()) {
            association_type = parse_type_spec();
            association_type = parse_type_name_declarator(
                association_type, association_loc);
            if (parser_type_is_variably_modified(association_type)) {
                rcc_error(association_loc,
                          "generic association type cannot be variably modified");
            } else if (!generic_association_type_valid(association_type)) {
                rcc_error(association_loc,
                          "generic association requires a complete object type");
            }
            for (GenericAssociation* previous = associations; previous;
                 previous = previous->next) {
                if (previous->type &&
                    type_is_compatible_qualified(previous->type,
                                                 association_type)) {
                    rcc_error(association_loc,
                              "generic selection has compatible duplicate types");
                    break;
                }
            }
        } else {
            rcc_error(association_loc,
                      "expected type name or default in generic association");
            if (!check(TOK_COLON)) advance();
        }
        expect(TOK_COLON, ":");
        association_expression = parse_assignment();
        generic_association_append(&associations, association_type,
                                   association_expression,
                                   association_loc);
        if (!match(TOK_COMMA)) break;
    }
    expect(TOK_RPAREN, ")");
    return expr_generic(control, associations, loc);
}

static TypeField* parser_find_field(Type* aggregate, const char* name) {
    for (TypeField* field = aggregate ? aggregate->fields : NULL;
         field; field = field->next) {
        if (field->name && strcmp(field->name, name) == 0) return field;
    }
    return NULL;
}

Type* rcc_parser_lookup_type(const char* name) {
    Type* type = parser_lookup_type(name);
    ParserTagName* tag;
    Type* cxx_class_type;
    if (type || !name ||
        (parser_ambiguous_type_name &&
         strcmp(parser_ambiguous_type_name, name) == 0)) return type;
    /* A leading global-scope qualifier must bypass namespace lookup.  The
     * ordinary C++ lookup helper intentionally searches visible namespaces,
     * while `::name` denotes only a translation-unit-scope declaration. */
    if (name[0] == ':' && name[1] == ':' &&
        !strstr(name + 2, "::")) {
        const char* global_name = name + 2;
        for (ParserTypeName* entry = parser_type_names; entry;
             entry = entry->next) {
            if (entry->name && strcmp(entry->name, global_name) == 0 &&
                entry->scope == parser_tag_root_scope &&
                (!entry->cxx_namespace || !*entry->cxx_namespace)) {
                return entry->type;
            }
        }
        for (tag = parser_tag_names; tag; tag = tag->next) {
            if (tag->name && strcmp(tag->name, global_name) == 0 &&
                tag->scope == parser_tag_root_scope &&
                (!tag->cxx_namespace || !*tag->cxx_namespace) &&
                (tag->kind == TYPE_ENUM || tag->kind == TYPE_STRUCT ||
                 tag->kind == TYPE_UNION)) {
                return tag->type;
            }
        }
    }
    {
        const char* qualified_name = name;
        const char* final_separator;
        while (qualified_name[0] == ':' && qualified_name[1] == ':') {
            qualified_name += 2;
        }
        final_separator = strrchr(qualified_name, ':');
        if (final_separator && final_separator > qualified_name &&
            final_separator[-1] == ':') {
            size_t namespace_length =
                (size_t)(final_separator - qualified_name - 1);
            const char* type_name = final_separator + 1;
            char namespace_name[512];
            if (namespace_length < sizeof(namespace_name)) {
                memcpy(namespace_name, qualified_name, namespace_length);
                namespace_name[namespace_length] = '\0';
                for (ParserTypeName* entry = parser_type_names; entry;
                     entry = entry->next) {
                    bool namespace_matches = false;
                    if (entry->scope != parser_tag_root_scope ||
                        !entry->name ||
                        strcmp(entry->name, type_name) != 0) {
                        continue;
                    }
                    if (namespace_length == 0u) {
                        namespace_matches = !entry->cxx_namespace ||
                            !*entry->cxx_namespace;
                    } else if (entry->cxx_namespace &&
                               rcc_parser_cxx_type_namespace_matches_qualified_name) {
                        namespace_matches =
                            rcc_parser_cxx_type_namespace_matches_qualified_name(
                                entry->cxx_namespace, namespace_name);
                    }
                    if (namespace_matches) return entry->type;
                }
            }
            for (tag = parser_tag_names; tag; tag = tag->next) {
                if (tag->kind == TYPE_ENUM && tag->name &&
                    tag->cxx_namespace &&
                    tag->scope == parser_tag_root_scope &&
                    strlen(tag->cxx_namespace) == namespace_length &&
                    memcmp(tag->cxx_namespace, qualified_name,
                           namespace_length) == 0 &&
                    strcmp(tag->name, type_name) == 0) {
                    return tag->type;
                }
            }
        }
    }
    for (tag = parser_tag_names; tag; tag = tag->next) {
        if (tag->name && strcmp(tag->name, name) == 0 &&
            (tag->kind == TYPE_STRUCT || tag->kind == TYPE_UNION)) {
            return tag->type;
        }
    }
    if (rcc_parser_cxx_find_class_type) {
        cxx_class_type = rcc_parser_cxx_find_class_type(name);
        if (cxx_class_type) return cxx_class_type;
    }
    return NULL;
}

bool rcc_parser_type_lookup_is_ambiguous(const char* name) {
    return name && parser_ambiguous_type_name &&
           strcmp(name, parser_ambiguous_type_name) == 0;
}

void rcc_parser_define_type(const char* name, Type* type) {
    if (name && type) parser_define_type(name, type);
}

void* rcc_parser_type_scope_mark(void) {
    return parser_type_names;
}

void rcc_parser_type_scope_restore(void* mark) {
    parser_type_names = (ParserTypeName*)mark;
}

void rcc_parser_define_cxx_constructor_type(const char* name, Type* type,
                                            uint32_t arity_mask) {
    ParserTypeName* entry;
    if (!name || !type || arity_mask == 0u) return;
    type->cxx_constructor_arity_mask = arity_mask;
    parser_define_type(name, type);
    entry = parser_type_names;
    entry->cxx_constructor_arity_mask = arity_mask;
}

uint32_t rcc_parser_cxx_constructor_arity_mask(Type* type) {
    ParserTypeName* entry;
    for (entry = parser_type_names; entry; entry = entry->next) {
        if (entry->type == type && entry->cxx_constructor_arity_mask != 0u) {
            return entry->cxx_constructor_arity_mask;
        }
    }
    if (type && type->cxx_constructor_arity_mask != 0u) {
        return type->cxx_constructor_arity_mask;
    }
    return 0u;
}

void rcc_parser_validate_cxx_constructor_initializer(Type* type,
                                                     Expr* initializer) {
    uint32_t mask;
    unsigned arity = 0u;
    ExprList* item;
    if (!parser_cxx_mode || !type || type->cxx_dependent || !initializer ||
        initializer->kind != EXPR_COMPOUND) {
        return;
    }
    /* Union aggregates are initialized through aggregate-member selection,
     * not through the constructor arity mask.  Defer their initializer-list
     * validation to semantic analysis, which also handles the single active
     * member rule and exact-type trivial copies. */
    if (type->kind == TYPE_UNION && ast_cxx_is_aggregate(type)) return;
    if (initializer->compound_init &&
        !initializer->compound_init->next &&
        initializer->compound_init->designator_kind ==
            INIT_DESIGNATOR_NONE &&
        initializer->compound_init->expr &&
        initializer->compound_init->expr->kind == EXPR_CAST) {
        Expr* cast = initializer->compound_init->expr;
        Type* cast_type = cast->cast_type;
        if (cast_type && cast_type->kind == TYPE_PTR &&
            cast_type->is_reference && cast_type->is_rvalue_reference &&
            cast_type->base && type_is_compatible(cast_type->base, type)) {
            TypeMethod* move = type->move_constructor_method;
            Expr* source = cast->cast_expr;
            /* move_constructor_method records the SDK's structurally
             * validated release()-based lowering, not the presence of an
             * ordinary C++ move constructor.  Preserve every other xvalue
             * initializer for normal constructor overload resolution. */
            if (move && move->name && source &&
                source->kind == EXPR_IDENT) {
                initializer->compound_init->expr = expr_call(
                    expr_member(source, move->name, cast->loc), NULL,
                    cast->loc);
                return;
            }
        }
    }
    mask = rcc_parser_cxx_constructor_arity_mask(type);
    if (mask == 0u) return;
    if (!initializer->compound_value_init) {
        for (item = initializer->compound_init; item; item = item->next) {
            if (arity < 32u) ++arity;
        }
    }
    if (arity >= 32u || (mask & (UINT32_C(1) << arity)) == 0u) {
        rcc_error(initializer->loc,
                  "no safely lowerable constructor accepts %u argument%s",
                  arity, arity == 1u ? "" : "s");
    }
}

static bool parser_type_has_deferred_typeof_recursive(
    Type* type, Type** visited, unsigned* visited_count) {
    if (!type) return false;
    if (type->c_typeof_expression) return true;
    for (unsigned i = 0u; i < *visited_count; ++i) {
        if (visited[i] == type) return false;
    }
    if (*visited_count >= 128u) return true;
    visited[(*visited_count)++] = type;

    switch (type->kind) {
        case TYPE_PTR:
        case TYPE_ARRAY:
        case TYPE_VECTOR:
            return parser_type_has_deferred_typeof_recursive(
                type->base, visited, visited_count);
        case TYPE_FUNC:
            if (parser_type_has_deferred_typeof_recursive(
                    type->ret_type, visited, visited_count)) {
                return true;
            }
            for (TypeParam* parameter = type->params; parameter;
                 parameter = parameter->next) {
                if (parser_type_has_deferred_typeof_recursive(
                        parameter->type, visited, visited_count)) {
                    return true;
                }
            }
            return false;
        case TYPE_STRUCT:
        case TYPE_UNION:
            for (TypeField* field = type->fields; field;
                 field = field->next) {
                if (parser_type_has_deferred_typeof_recursive(
                        field->type, visited, visited_count)) {
                    return true;
                }
            }
            return false;
        default:
            return false;
    }
}

static bool parser_type_has_deferred_typeof(Type* type) {
    Type* visited[128];
    unsigned visited_count = 0u;
    return parser_type_has_deferred_typeof_recursive(
        type, visited, &visited_count);
}

static void parser_mark_typeof_shared_vla_bounds(Type* type) {
    if (!type) return;

    switch (type->kind) {
        case TYPE_PTR:
        case TYPE_ARRAY:
        case TYPE_VECTOR:
            if (type->kind == TYPE_ARRAY && type->array_bound) {
                type->array_bound->c_vla_shared_typeof_bound = true;
            }
            parser_mark_typeof_shared_vla_bounds(type->base);
            break;
        case TYPE_FUNC:
            parser_mark_typeof_shared_vla_bounds(type->ret_type);
            for (TypeParam* parameter = type->params; parameter;
                 parameter = parameter->next) {
                parser_mark_typeof_shared_vla_bounds(parameter->type);
            }
            break;
        default:
            break;
    }
}

static bool parser_c23_deferred_type(Type* type) {
    return !parser_cxx_mode && g_opts.c_standard >= 23 &&
           parser_type_has_deferred_typeof(type);
}

static void parser_append_offsetof_designator(
    TypeOffsetofDesignator*** tail, TypeOffsetofDesignator* designator) {
    **tail = designator;
    *tail = &designator->next;
}

static Expr* parse_builtin_offsetof(SourceLoc loc) {
    Type* current;
    int64_t offset = 0;
    bool deferred_type;
    TypeOffsetofDesignator* designators = NULL;
    TypeOffsetofDesignator** designator_tail = &designators;

    advance(); /* __builtin_offsetof */
    expect(TOK_LPAREN, "(");
    if (!is_type_start()) {
        rcc_error(peek()->loc, "__builtin_offsetof requires a type name");
        current = type_int;
    } else {
        current = parse_type_spec();
        current = parse_type_name_declarator(current, loc);
    }
    deferred_type = parser_c23_deferred_type(current);
    expect(TOK_COMMA, ",");

    if (!deferred_type && (!current || (current->kind != TYPE_STRUCT &&
                                       current->kind != TYPE_UNION))) {
        rcc_error(loc, "__builtin_offsetof requires an aggregate type");
    }
    while (!check(TOK_RPAREN) && !at_end()) {
        if (match(TOK_IDENT)) {
            Token* member = previous();
            if (deferred_type) {
                TypeOffsetofDesignator* designator = ast_arena_alloc(
                    sizeof(*designator));
                designator->kind = TYPE_OFFSETOF_MEMBER;
                designator->member_name = member->value.str_val;
                designator->loc = member->loc;
                designator->next = NULL;
                parser_append_offsetof_designator(&designator_tail,
                                                   designator);
            } else {
                TypeField* field = parser_find_field(
                    current, member->value.str_val);
                if (!field) {
                    rcc_error(member->loc,
                              "unknown member '%s' in offsetof",
                              member->value.str_val);
                    current = type_int;
                } else {
                    offset += field->offset;
                    if (field->is_bitfield) {
                        rcc_error(member->loc,
                                  "cannot compute offsetof for a bit-field");
                    }
                    current = field->type;
                }
            }
        } else if (match(TOK_LBRACKET)) {
            Expr* index_expression = parse_assignment();
            int64_t index = 0;
            if (deferred_type) {
                if (!eval_integer_constant(index_expression, &index) ||
                    index < 0) {
                    rcc_error(index_expression->loc,
                              "offsetof subscript is not a non-negative integer constant");
                } else {
                    TypeOffsetofDesignator* designator = ast_arena_alloc(
                        sizeof(*designator));
                    designator->kind = TYPE_OFFSETOF_INDEX;
                    designator->index = index;
                    designator->loc = index_expression->loc;
                    designator->next = NULL;
                    parser_append_offsetof_designator(&designator_tail,
                                                       designator);
                }
            } else if (!current || current->kind != TYPE_ARRAY) {
                rcc_error(previous()->loc,
                          "offsetof subscript requires an array member");
            } else if (!eval_integer_constant(index_expression, &index) ||
                       index < 0) {
                rcc_error(index_expression->loc,
                          "offsetof subscript is not a non-negative integer constant");
            } else {
                offset += index * current->base->size;
                current = current->base;
            }
            expect(TOK_RBRACKET, "]");
        } else {
            rcc_error(peek()->loc, "expected member designator in offsetof");
            advance();
        }
        if (!match(TOK_DOT) && !check(TOK_LBRACKET)) break;
    }
    expect(TOK_RPAREN, ")");

    if (deferred_type) {
        Expr* result = expr_int(0, loc);
        result->kind = EXPR_BUILTIN_OFFSETOF;
        result->builtin_offsetof_type = current;
        result->builtin_offsetof_designators = designators;
        result->type = g_opts.target_arch == ARCH_X64 ? type_ullong
                                                       : type_uint;
        return result;
    }
    Expr* result = expr_int(offset, loc);
    result->type = g_opts.target_arch == ARCH_X64 ? type_ullong : type_uint;
    return result;
}

static bool parser_builtin_types_compatible(Type* left, Type* right,
                                            bool top_level) {
    TypeParam* left_param;
    TypeParam* right_param;

    if (!left || !right || !type_is_compatible(left, right)) return false;
    if (left->is_atomic != right->is_atomic) return false;
    if (!top_level &&
        (left->is_const != right->is_const ||
         left->is_volatile != right->is_volatile ||
         left->is_restrict != right->is_restrict)) {
        return false;
    }
    if (left->kind == TYPE_PTR || left->kind == TYPE_ARRAY ||
        left->kind == TYPE_VECTOR) {
        return parser_builtin_types_compatible(left->base, right->base, false);
    }
    if (left->kind != TYPE_FUNC) return true;
    if (!left->has_prototype || !right->has_prototype) return true;
    if (!parser_builtin_types_compatible(left->ret_type, right->ret_type,
                                         true)) {
        return false;
    }
    left_param = left->params;
    right_param = right->params;
    while (left_param && right_param) {
        if (!parser_builtin_types_compatible(left_param->type,
                                             right_param->type, true)) {
            return false;
        }
        left_param = left_param->next;
        right_param = right_param->next;
    }
    return left_param == NULL && right_param == NULL;
}

static Expr* parse_builtin_types_compatible(SourceLoc loc) {
    Type* left = NULL;
    Type* right = NULL;
    Expr* result;

    advance(); /* __builtin_types_compatible_p */
    expect(TOK_LPAREN, "(");
    if (!is_type_start()) {
        rcc_error(peek()->loc,
                  "__builtin_types_compatible_p requires a type name");
    } else {
        left = parse_type_spec();
        left = parse_type_name_declarator(left, loc);
    }
    expect(TOK_COMMA, ",");
    if (!is_type_start()) {
        rcc_error(peek()->loc,
                  "__builtin_types_compatible_p requires a type name");
    } else {
        right = parse_type_spec();
        right = parse_type_name_declarator(right, loc);
    }
    expect(TOK_RPAREN, ")");
    if (parser_c23_deferred_type(left) ||
        parser_c23_deferred_type(right)) {
        result = expr_int(0, loc);
        result->kind = EXPR_BUILTIN_TYPES_COMPAT;
        result->builtin_types_compatible_left = left;
        result->builtin_types_compatible_right = right;
        result->type = type_int;
        return result;
    }
    result = expr_int(left && right &&
                          parser_builtin_types_compatible(left, right, true)
                          ? 1 : 0,
                      loc);
    result->type = type_int;
    return result;
}

static Expr* parse_builtin_cxx_type_trait(SourceLoc loc) {
    const char* name = advance()->value.str_val;
    Type* type = NULL;
    Type* other_type = NULL;
    CxxTypeTraitArgument* arguments = NULL;
    CxxTypeTraitArgument** arguments_tail = &arguments;
    CxxTypeTraitKind kind;
    bool value = false;

    expect(TOK_LPAREN, "(");
    if (!is_type_start()) {
        rcc_error(peek()->loc, "%s requires a type name", name);
    } else {
        type = parse_type_spec();
        type = parse_type_name_declarator(type, loc);
    }
    if (strcmp(name, "__is_base_of") == 0 ||
        strcmp(name, "__is_assignable") == 0 ||
        strcmp(name, "__is_nothrow_assignable") == 0 ||
        strcmp(name, "__is_trivially_assignable") == 0) {
        expect(TOK_COMMA, ",");
        if (!is_type_start()) {
            rcc_error(peek()->loc, "%s requires two type names", name);
        } else {
            other_type = parse_type_spec();
            other_type = parse_type_name_declarator(other_type, loc);
        }
    } else if (strcmp(name, "__is_trivially_constructible") == 0 ||
               strcmp(name, "__is_constructible") == 0 ||
               strcmp(name, "__is_nothrow_constructible") == 0) {
        while (match(TOK_COMMA)) {
            CxxTypeTraitArgument* argument = ast_arena_alloc(
                sizeof(*argument));
            argument->loc = peek()->loc;
            if (!is_type_start()) {
                rcc_error(peek()->loc,
                          "%s requires type names after the destination type",
                          name);
                break;
            }
            argument->type = parse_type_spec();
            argument->type = parse_type_name_declarator(
                argument->type, argument->loc);
            argument->is_pack_expansion = parser_cxx_mode &&
                (parser_last_declarator_parameter_pack ||
                 match(TOK_ELLIPSIS));
            argument->next = NULL;
            *arguments_tail = argument;
            arguments_tail = &argument->next;
        }
    }
    expect(TOK_RPAREN, ")");

    if (!type || !parser_cxx_type_trait_kind(name, &kind)) {
        Expr* result = expr_int(0, loc);
        result->type = type_bool;
        return result;
    }

    {
        bool dependent = parser_type_is_dependent(type) ||
                         parser_type_is_dependent(other_type);
        for (CxxTypeTraitArgument* argument = arguments; argument;
             argument = argument->next) {
            if (parser_type_is_dependent(argument->type) ||
                argument->is_pack_expansion) {
                dependent = true;
                break;
            }
        }
        if (dependent) {
            Expr* result = expr_int(0, loc);
            result->kind = EXPR_CXX_TYPE_TRAIT;
            result->cxx_type_trait_kind = kind;
            result->cxx_type_trait_type = type;
            result->cxx_type_trait_other_type = other_type;
            result->cxx_type_trait_arguments = arguments;
            result->type = type_bool;
            return result;
        }
    }

    if (!parser_cxx_type_trait_value(kind, type, other_type, arguments,
                                     &value) &&
        (kind == CXX_TYPE_TRAIT_ASSIGNABLE ||
         kind == CXX_TYPE_TRAIT_NOTHROW_ASSIGNABLE ||
         kind == CXX_TYPE_TRAIT_TRIVIALLY_ASSIGNABLE ||
         kind == CXX_TYPE_TRAIT_NOTHROW_CONSTRUCTIBLE ||
         kind == CXX_TYPE_TRAIT_HAS_NOTHROW_CONSTRUCTOR)) {
        Expr* result = expr_int(0, loc);
        result->kind = EXPR_CXX_TYPE_TRAIT;
        result->cxx_type_trait_kind = kind;
        result->cxx_type_trait_type = type;
        result->cxx_type_trait_other_type = other_type;
        result->type = type_bool;
        return result;
    }
    Expr* result = expr_int(value ? 1 : 0, loc);
    result->type = type_bool;
    return result;
}

static bool parser_cxx_type_trait_kind(const char* name,
                                      CxxTypeTraitKind* kind) {
    if (!name || !kind) return false;
    if (strcmp(name, "__is_pod") == 0) {
        *kind = CXX_TYPE_TRAIT_POD;
    } else if (strcmp(name, "__is_standard_layout") == 0) {
        *kind = CXX_TYPE_TRAIT_STANDARD_LAYOUT;
    } else if (strcmp(name, "__is_trivial") == 0) {
        *kind = CXX_TYPE_TRAIT_TRIVIAL;
    } else if (strcmp(name, "__is_trivially_copyable") == 0) {
        *kind = CXX_TYPE_TRAIT_TRIVIALLY_COPYABLE;
    } else if (strcmp(name, "__is_trivially_destructible") == 0) {
        *kind = CXX_TYPE_TRAIT_TRIVIALLY_DESTRUCTIBLE;
    } else if (strcmp(name, "__has_trivial_destructor") == 0) {
        *kind = CXX_TYPE_TRAIT_HAS_TRIVIAL_DESTRUCTOR;
    } else if (strcmp(name, "__has_trivial_constructor") == 0) {
        *kind = CXX_TYPE_TRAIT_HAS_TRIVIAL_CONSTRUCTOR;
    } else if (strcmp(name, "__has_trivial_assign") == 0) {
        *kind = CXX_TYPE_TRAIT_HAS_TRIVIAL_ASSIGN;
    } else if (strcmp(name, "__has_nothrow_constructor") == 0) {
        *kind = CXX_TYPE_TRAIT_HAS_NOTHROW_CONSTRUCTOR;
    } else if (strcmp(name, "__is_trivially_constructible") == 0) {
        *kind = CXX_TYPE_TRAIT_TRIVIALLY_CONSTRUCTIBLE;
    } else if (strcmp(name, "__is_constructible") == 0) {
        *kind = CXX_TYPE_TRAIT_CONSTRUCTIBLE;
    } else if (strcmp(name, "__is_nothrow_constructible") == 0) {
        *kind = CXX_TYPE_TRAIT_NOTHROW_CONSTRUCTIBLE;
    } else if (strcmp(name, "__is_assignable") == 0) {
        *kind = CXX_TYPE_TRAIT_ASSIGNABLE;
    } else if (strcmp(name, "__is_nothrow_assignable") == 0) {
        *kind = CXX_TYPE_TRAIT_NOTHROW_ASSIGNABLE;
    } else if (strcmp(name, "__is_trivially_assignable") == 0) {
        *kind = CXX_TYPE_TRAIT_TRIVIALLY_ASSIGNABLE;
    } else if (strcmp(name, "__has_virtual_destructor") == 0) {
        *kind = CXX_TYPE_TRAIT_HAS_VIRTUAL_DESTRUCTOR;
    } else if (strcmp(name, "__is_aggregate") == 0) {
        *kind = CXX_TYPE_TRAIT_AGGREGATE;
    } else if (strcmp(name, "__is_base_of") == 0) {
        *kind = CXX_TYPE_TRAIT_BASE_OF;
    } else if (strcmp(name, "__is_enum") == 0) {
        *kind = CXX_TYPE_TRAIT_ENUM;
    } else if (strcmp(name, "__is_class") == 0) {
        *kind = CXX_TYPE_TRAIT_CLASS;
    } else if (strcmp(name, "__is_union") == 0) {
        *kind = CXX_TYPE_TRAIT_UNION;
    } else if (strcmp(name, "__is_final") == 0) {
        *kind = CXX_TYPE_TRAIT_FINAL;
    } else if (strcmp(name, "__is_abstract") == 0) {
        *kind = CXX_TYPE_TRAIT_ABSTRACT;
    } else if (strcmp(name, "__is_polymorphic") == 0) {
        *kind = CXX_TYPE_TRAIT_POLYMORPHIC;
    } else if (strcmp(name, "__is_empty") == 0) {
        *kind = CXX_TYPE_TRAIT_EMPTY;
    } else if (strcmp(name, "__is_function") == 0) {
        *kind = CXX_TYPE_TRAIT_FUNCTION;
    } else {
        return false;
    }
    return true;
}

static void parser_type_dependency_push(Type*** pending, size_t* count,
                                        size_t* capacity, Type* type) {
    size_t next_capacity;
    if (!type) return;
    if (*count == *capacity) {
        next_capacity = *capacity ? *capacity * 2u : 8u;
        if (next_capacity < *capacity ||
            next_capacity > SIZE_MAX / sizeof(**pending)) {
            rcc_fatal("dependent type graph is too large");
        }
        *pending = ast_arena_grow(
            *pending, *capacity * sizeof(**pending),
            next_capacity * sizeof(**pending));
        *capacity = next_capacity;
    }
    (*pending)[(*count)++] = type;
}

static bool parser_type_is_dependent(Type* type) {
    Type** pending = NULL;
    size_t count = 0;
    size_t capacity = 0;
    size_t head = 0;
    parser_type_dependency_push(&pending, &count, &capacity, type);
    while (head < count) {
        Type* current = pending[head++];
        bool visited = false;
        for (size_t index = 0; index + 1u < head; ++index) {
            if (pending[index] == current) {
                visited = true;
                break;
            }
        }
        if (visited) continue;
        if (current->cxx_dependent) return true;
        switch (current->kind) {
            case TYPE_PTR:
            case TYPE_ARRAY:
            case TYPE_VECTOR:
                parser_type_dependency_push(
                    &pending, &count, &capacity, current->base);
                if (current->cxx_is_member_pointer) {
                    parser_type_dependency_push(
                        &pending, &count, &capacity,
                        current->cxx_member_pointer_owner);
                }
                break;
            case TYPE_FUNC:
                parser_type_dependency_push(
                    &pending, &count, &capacity, current->ret_type);
                for (TypeParam* parameter = current->params; parameter;
                     parameter = parameter->next) {
                    parser_type_dependency_push(
                        &pending, &count, &capacity, parameter->type);
                }
                break;
            case TYPE_ENUM:
                if (current->enum_has_fixed_underlying) {
                    parser_type_dependency_push(
                        &pending, &count, &capacity,
                        current->enum_underlying_type);
                }
                break;
            default:
                break;
        }
        if (current->cxx_dependent_alias_template) {
            parser_type_dependency_push(
                &pending, &count, &capacity,
                current->cxx_dependent_alias_owner);
        }
        if (current->cxx_dependent_class_template_owner) {
            parser_type_dependency_push(
                &pending, &count, &capacity,
                current->cxx_dependent_class_template_owner);
        }
        if (current->cxx_template && current->cxx_template_arg_count > 0) {
            for (int index = 0; index < current->cxx_template_arg_count;
                 ++index) {
                parser_type_dependency_push(
                    &pending, &count, &capacity,
                    current->cxx_template_args[index]);
            }
        }
    }
    return false;
}

static bool parser_cxx_type_trait_value(CxxTypeTraitKind kind, Type* type,
                                        Type* other_type,
                                        CxxTypeTraitArgument* arguments,
                                        bool* value) {
    if (!type || !value || parser_type_is_dependent(type) ||
        parser_type_is_dependent(other_type)) {
        return false;
    }
    for (CxxTypeTraitArgument* argument = arguments; argument;
         argument = argument->next) {
        if (parser_type_is_dependent(argument->type) ||
            argument->is_pack_expansion) {
            return false;
        }
    }
    switch (kind) {
        case CXX_TYPE_TRAIT_POD:
            *value = rcc_sema_cxx_pod(type);
            break;
        case CXX_TYPE_TRAIT_STANDARD_LAYOUT:
            *value = rcc_sema_cxx_standard_layout(type);
            break;
        case CXX_TYPE_TRAIT_TRIVIAL:
            *value = rcc_sema_cxx_trivial(type);
            break;
        case CXX_TYPE_TRAIT_TRIVIALLY_COPYABLE:
            *value = rcc_sema_cxx_trivially_copyable(type);
            break;
        case CXX_TYPE_TRAIT_TRIVIALLY_DESTRUCTIBLE:
            *value = rcc_sema_cxx_trivially_destructible(type);
            break;
        case CXX_TYPE_TRAIT_HAS_TRIVIAL_DESTRUCTOR:
            *value = rcc_sema_cxx_has_trivial_destructor(type);
            break;
        case CXX_TYPE_TRAIT_HAS_TRIVIAL_CONSTRUCTOR:
            *value = rcc_sema_cxx_has_trivial_constructor(type);
            break;
        case CXX_TYPE_TRAIT_HAS_TRIVIAL_ASSIGN:
            return rcc_sema_cxx_has_trivial_assign(type, value);
        case CXX_TYPE_TRAIT_HAS_NOTHROW_CONSTRUCTOR:
            return rcc_sema_cxx_has_nothrow_constructor(type, value);
        case CXX_TYPE_TRAIT_TRIVIALLY_CONSTRUCTIBLE:
            *value = rcc_sema_cxx_trivially_constructible(type, arguments);
            break;
        case CXX_TYPE_TRAIT_CONSTRUCTIBLE:
            *value = rcc_sema_cxx_constructible(type, arguments);
            break;
        case CXX_TYPE_TRAIT_NOTHROW_CONSTRUCTIBLE:
            return rcc_sema_cxx_nothrow_constructible(
                type, arguments, value);
        case CXX_TYPE_TRAIT_ASSIGNABLE:
            if (!other_type) return false;
            {
                int assignable = rcc_sema_cxx_assignable(type, other_type);
                if (assignable < 0) return false;
                *value = assignable != 0;
            }
            break;
        case CXX_TYPE_TRAIT_NOTHROW_ASSIGNABLE:
            if (!other_type) return false;
            return rcc_sema_cxx_nothrow_assignable(
                type, other_type, value);
        case CXX_TYPE_TRAIT_TRIVIALLY_ASSIGNABLE:
            if (!other_type) return false;
            return rcc_sema_cxx_trivially_assignable(
                type, other_type, value);
        case CXX_TYPE_TRAIT_HAS_VIRTUAL_DESTRUCTOR:
            *value = cxx_class_has_virtual_destructor(type->cxx_class);
            break;
        case CXX_TYPE_TRAIT_AGGREGATE:
            *value = type->kind == TYPE_ARRAY ? type->array_len >= 0
                                              : ast_cxx_is_aggregate(type);
            break;
        case CXX_TYPE_TRAIT_BASE_OF:
            *value = other_type && type->kind == TYPE_STRUCT &&
                     other_type->kind == TYPE_STRUCT && type->cxx_class &&
                     other_type->cxx_class &&
                     cxx_class_is_same_or_derived_from(
                         other_type->cxx_class, type->cxx_class);
            break;
        case CXX_TYPE_TRAIT_ENUM:
            *value = type->kind == TYPE_ENUM;
            break;
        case CXX_TYPE_TRAIT_CLASS:
            *value = type->kind == TYPE_STRUCT;
            break;
        case CXX_TYPE_TRAIT_UNION:
            *value = type->kind == TYPE_UNION;
            break;
        case CXX_TYPE_TRAIT_FINAL:
            *value = cxx_class_is_final(type->cxx_class);
            break;
        case CXX_TYPE_TRAIT_ABSTRACT:
            *value = cxx_class_is_abstract(type->cxx_class);
            break;
        case CXX_TYPE_TRAIT_POLYMORPHIC:
            *value = cxx_class_is_polymorphic(type->cxx_class);
            break;
        case CXX_TYPE_TRAIT_EMPTY:
            *value = cxx_class_is_empty(type->cxx_class);
            break;
        case CXX_TYPE_TRAIT_FUNCTION:
            *value = type->kind == TYPE_FUNC;
            break;
    }
    return true;
}

static bool parser_builtin_name(const char* name) {
    return check(TOK_IDENT) &&
           strcmp(peek()->value.str_val, name) == 0;
}

static Expr* parse_builtin_vararg(SourceLoc loc) {
    const char* name = advance()->value.str_val;
    ExprKind kind = strcmp(name, "__builtin_va_start") == 0
        ? EXPR_VA_START : strcmp(name, "__builtin_va_end") == 0
            ? EXPR_VA_END : strcmp(name, "__builtin_va_copy") == 0
                ? EXPR_VA_COPY : EXPR_VA_ARG;
    Expr* list;
    Expr* second = NULL;
    Type* argument_type = NULL;

    expect(TOK_LPAREN, "(");
    list = parse_assignment();
    if (kind == EXPR_VA_START) {
        if (match(TOK_COMMA)) second = parse_assignment();
    } else if (kind == EXPR_VA_COPY || kind == EXPR_VA_ARG) {
        expect(TOK_COMMA, ",");
        if (kind == EXPR_VA_ARG) {
            if (!is_type_start()) {
                rcc_error(peek()->loc,
                          "__builtin_va_arg requires a type name");
                argument_type = type_int;
            } else {
                argument_type = parse_type_spec();
                argument_type = parse_type_name_declarator(
                    argument_type, loc);
            }
        } else {
            second = parse_assignment();
        }
    }
    expect(TOK_RPAREN, ")");
    return expr_vararg(kind, list, second, argument_type, loc);
}

static void qualified_name_append(char** buffer, size_t* capacity,
                                  size_t* length, const char* text) {
    size_t text_length = strlen(text);
    size_t required;
    if (text_length > SIZE_MAX - *length - 1u) {
        rcc_fatal("qualified C++ identifier is too large");
    }
    required = *length + text_length + 1u;
    if (*capacity < required) {
        size_t new_capacity = *capacity ? *capacity : 32u;
        while (new_capacity < required) {
            if (new_capacity > SIZE_MAX / 2u) {
                new_capacity = required;
                break;
            }
            new_capacity *= 2u;
        }
        *buffer = rcc_realloc(*buffer, new_capacity);
        *capacity = new_capacity;
    }
    memcpy(*buffer + *length, text, text_length);
    *length += text_length;
    (*buffer)[*length] = '\0';
}

/* C++ expression parsing shares the mature C precedence parser.  Preserve a
 * qualified-id as one lookup key before the postfix/call layers consume it. */
static const char* parse_expression_qualified_name(SourceLoc loc) {
    char* buffer = NULL;
    size_t capacity = 0u;
    size_t length = 0u;
    const char* interned;

    /* Keep a leading global-scope operator in the lookup key.  Semantic
     * lookup must not reinterpret `::name` as an unqualified name in the
     * current namespace. */
    if (match(TOK_SCOPE)) {
        qualified_name_append(&buffer, &capacity, &length, "::");
    }
    if (!check(TOK_IDENT)) {
        rcc_error(peek()->loc, "expected identifier in qualified name");
        interned = rcc_intern(buffer ? buffer : "");
        rcc_free(buffer);
        return interned;
    }
    qualified_name_append(&buffer, &capacity, &length,
                          advance()->value.str_val);
    while (match(TOK_SCOPE)) {
        qualified_name_append(&buffer, &capacity, &length, "::");
        if (!check(TOK_IDENT)) {
            rcc_error(peek()->loc, "expected identifier after ::");
            break;
        }
        qualified_name_append(&buffer, &capacity, &length,
                              advance()->value.str_val);
    }
    (void)loc;
    interned = rcc_intern(buffer);
    rcc_free(buffer);
    return interned;
}

static Expr* parse_cxx_user_literal_call(const char* suffix,
                                         Expr* literal,
                                         Expr* length,
                                         SourceLoc loc) {
    char name[512];
    Expr* function;
    ExprList* arguments;
    int written;

    if (!suffix || suffix[0] != '_') return literal;
    written = snprintf(name, sizeof(name), "operator\"\"%s", suffix);
    if (written < 0 || (size_t)written >= sizeof(name)) {
        rcc_error(loc, "user-defined literal operator name is too long");
        return literal;
    }
    function = expr_ident(rcc_intern(name), loc);
    arguments = exprlist_new(literal);
    if (length) exprlist_append(&arguments, length);
    return expr_call(function, arguments, loc);
}

/* Parse the GCC vector-size attribute used by the intrinsic headers.  The
 * ordinary attribute skipper intentionally discards metadata, but vector
 * size changes the object type and must reach semantic analysis and codegen.
 * The 64-bit MMX spelling is retained as its scalar long-long ABI carrier;
 * 128-bit and larger vectors become first-class vector types. */
static Type* parse_declarator_attributes(Type* type) {
    while (match(TOK___ATTRIBUTE__)) {
        int vector_size = 0;
        bool weak_attribute = false;
        bool weak_arguments = false;
        SourceLoc attribute_loc = previous()->loc;
        if (!match(TOK_LPAREN)) continue;
        while (!check(TOK_RPAREN) && !at_end()) {
            if (match(TOK_LPAREN)) {
                Token* name = check(TOK_IDENT) ? advance() : NULL;
                bool is_vector_size = name &&
                    (strcmp(name->value.str_val, "vector_size") == 0 ||
                     strcmp(name->value.str_val, "__vector_size__") == 0);
                if (name && strcmp(name->value.str_val, "weak") == 0) {
                    weak_attribute = true;
                }
                if (match(TOK_LPAREN)) {
                    if (weak_attribute && !is_vector_size) {
                        weak_arguments = true;
                    }
                    Expr* size_expression = parse_assignment();
                    int64_t value = 0;
                    if (is_vector_size &&
                        eval_integer_constant(size_expression, &value) &&
                        value > 0 && value <= INT_MAX) {
                        vector_size = (int)value;
                    } else if (is_vector_size) {
                        rcc_error(attribute_loc,
                                  "vector_size attribute requires a positive integer constant");
                    }
                    expect(TOK_RPAREN, ") after vector_size argument");
                }
                while (!check(TOK_RPAREN) && !at_end()) advance();
                match(TOK_RPAREN);
            } else {
                advance();
            }
        }
        expect(TOK_RPAREN, ") after attribute list");
        if (weak_attribute) {
            if (weak_arguments) {
                rcc_error(attribute_loc,
                          "weak attribute does not accept arguments");
            } else {
                pending_weak_attribute = true;
            }
        }
        if (vector_size != 0 && type && type->size > 0) {
            if (vector_size == 8) {
                /* Keep the existing __m64 scalar carrier ABI. */
            } else if (vector_size < type->size ||
                       vector_size % type->size != 0) {
                rcc_error(attribute_loc,
                          "vector_size must be a positive multiple of the element size");
            } else {
                type = type_vector(type, vector_size / type->size,
                                   vector_size);
            }
        }
    }
    return type;
}

static const char* take_cxx_user_literal_suffix(void) {
    if (!parser_cxx_mode || !check(TOK_IDENT) ||
        !peek()->value.str_val || peek()->value.str_val[0] != '_') {
        return NULL;
    }
    return advance()->value.str_val;
}

/* Primary: literal, identifier, (expr) */
static Expr* parse_primary(void) {
    SourceLoc loc = peek()->loc;

    if (parser_cxx_mode && rcc_parse_cxx_requires_expression &&
        check(TOK_REQUIRES)) {
        return rcc_parse_cxx_requires_expression();
    }
    if (parser_cxx_mode && rcc_parse_cxx_special_expression &&
        (check(TOK_NEW) || check(TOK_DELETE))) {
        return rcc_parse_cxx_special_expression();
    }
    if (parser_cxx_mode && rcc_parse_cxx_lambda && check(TOK_LBRACKET)) {
        return rcc_parse_cxx_lambda();
    }
    if (parser_cxx_mode && match(TOK_THIS)) {
        return expr_ident("this", loc);
    }
    if ((parser_cxx_mode || g_opts.c_standard >= 23) &&
        match(TOK_NULLPTR)) {
        Expr* null_pointer = expr_int(0, loc);
        null_pointer->type = type_nullptr;
        null_pointer->is_cxx_nullptr = true;
        return null_pointer;
    }
    if (parser_cxx_mode && match(TOK_TRUE)) {
        return expr_int(1, loc);
    }
    if (parser_cxx_mode && match(TOK_FALSE)) {
        return expr_int(0, loc);
    }
    if (match(TOK_GENERIC)) {
        return parse_generic_selection(loc);
    }
    if (match(TOK_INT_LIT)) {
        Token* literal = previous();
        Expr* expression = literal->int_bitint_suffix
            ? expr_bitint_literal((uint64_t)literal->value.int_val,
                                  literal->int_unsigned_suffix, loc)
            : expr_integer_literal(
                  (uint64_t)literal->value.int_val, literal->int_base,
                  literal->int_unsigned_suffix, literal->int_long_suffix, loc);
        if (!expression) {
            rcc_error(loc,
                      "bit-precise integer literal exceeds BITINT_MAXWIDTH");
            expression = expr_int(0, loc);
        }
        if (parser_cxx_mode && literal->user_literal_suffix) {
            char name[512];
            Expr* function;
            int written = snprintf(name, sizeof(name), "operator\"\"%s",
                                   literal->user_literal_suffix);
            if (written < 0 || (size_t)written >= sizeof(name)) {
                rcc_error(loc, "user-defined literal operator name is too long");
                return expression;
            }
            function = expr_ident(rcc_intern(name), loc);
            return expr_call(function, exprlist_new(expression), loc);
        }
        return expression;
    }
    if (match(TOK_FLOAT_LIT)) {
        Token* literal = previous();
        Expr* expression = expr_float(literal->value.float_val, loc);
        if (literal->float_suffix) expression->type = type_float;
        else if (literal->float_long_suffix)
            expression->type = type_long_double;
        if (parser_cxx_mode && literal->user_literal_suffix) {
            return parse_cxx_user_literal_call(literal->user_literal_suffix,
                                               expression, NULL, loc);
        }
        return expression;
    }
    if (match(TOK_CHAR_LIT)) {
        Token* literal = previous();
        Expr* expression = literal->char_element_width > 1u
            ? expr_char_encoded((uint32_t)literal->value.int_val,
                                literal->char_element_width, loc)
            : literal->is_multichar_literal
                ? expr_int(literal->value.int_val, loc)
                : expr_char(literal->value.char_val, loc);
        expression->is_cxx_utf8_literal = literal->is_utf8_literal;
        expression->is_wide_literal = literal->is_wide_literal;
        if (literal->is_wide_literal &&
            (uint32_t)literal->value.int_val > (uint32_t)INT32_MAX) {
            expression->int_val = (int32_t)(uint32_t)literal->value.int_val;
        }
        if (parser_cxx_mode) {
            if (literal->char_element_width == 2u) {
                expression->type = type_char16;
            } else if (literal->char_element_width == 4u) {
                expression->type = literal->is_wide_literal
                    ? type_wchar : type_char32;
            } else {
                expression->type = literal->is_multichar_literal
                    ? type_int
                    : literal->is_utf8_literal &&
                          rcc_parser_cxx_standard_at_least(20)
                        ? type_char8 : type_char;
            }
            const char* suffix = take_cxx_user_literal_suffix();
            if (suffix) {
                return parse_cxx_user_literal_call(suffix, expression, NULL,
                                                   loc);
            }
        }
        return expression;
    }
    if (match(TOK_STRING_LIT)) {
        Token* literal = previous();
        Expr* expression = expr_string_encoded_n(
            literal->value.str_val, literal->string_length,
            literal->string_element_width, loc);
        expression->is_cxx_utf8_literal = literal->is_utf8_literal;
        expression->is_wide_literal = literal->is_wide_literal;
        if (parser_cxx_mode) {
            const char* suffix = take_cxx_user_literal_suffix();
            if (suffix) {
                Expr* length = expr_integer_literal(
                    (uint64_t)expression->str_length, 10u,
                    false, 0u, loc);
                return parse_cxx_user_literal_call(suffix, expression, length,
                                                   loc);
            }
        }
        return expression;
    }
    if (check(TOK_IDENT) &&
        (strcmp(peek()->value.str_val, "__func__") == 0 ||
         strcmp(peek()->value.str_val, "__FUNCTION__") == 0)) {
        Token* identifier = advance();
        if (!parser_function_name) {
            rcc_error(identifier->loc,
                      "%s is only valid within a function body",
                      identifier->value.str_val);
            return expr_int(0, loc);
        }
        return expr_string(parser_function_name, loc);
    }
    if (check(TOK_IDENT) && strcmp(peek()->value.str_val, "__FILE__") == 0) {
        Token* identifier = advance();
        if (!loc.filename) {
            rcc_error(identifier->loc,
                      "__FILE__ has no source filename at this location");
            return expr_int(0, loc);
        }
        return expr_string(loc.filename, loc);
    }
    if (check(TOK_IDENT) && strcmp(peek()->value.str_val, "__LINE__") == 0) {
        advance();
        return expr_int(loc.line, loc);
    }
    if (check(TOK_IDENT) &&
        strcmp(peek()->value.str_val, "__builtin_offsetof") == 0) {
        return parse_builtin_offsetof(loc);
    }
    if (check(TOK_IDENT) &&
        strcmp(peek()->value.str_val, "__builtin_types_compatible_p") == 0) {
        return parse_builtin_types_compatible(loc);
    }
    if (parser_cxx_mode &&
        (parser_builtin_name("__is_pod") ||
         parser_builtin_name("__is_standard_layout") ||
         parser_builtin_name("__is_trivial") ||
         parser_builtin_name("__is_trivially_copyable") ||
         parser_builtin_name("__is_trivially_destructible") ||
         parser_builtin_name("__has_trivial_destructor") ||
         parser_builtin_name("__has_trivial_constructor") ||
         parser_builtin_name("__has_trivial_assign") ||
         parser_builtin_name("__has_nothrow_constructor") ||
         parser_builtin_name("__is_trivially_constructible") ||
         parser_builtin_name("__is_constructible") ||
         parser_builtin_name("__is_nothrow_constructible") ||
         parser_builtin_name("__is_assignable") ||
         parser_builtin_name("__is_nothrow_assignable") ||
         parser_builtin_name("__is_trivially_assignable") ||
         parser_builtin_name("__has_virtual_destructor") ||
         parser_builtin_name("__is_aggregate") ||
         parser_builtin_name("__is_base_of") ||
         parser_builtin_name("__is_enum") ||
         parser_builtin_name("__is_class") ||
         parser_builtin_name("__is_union") ||
         parser_builtin_name("__is_final") ||
         parser_builtin_name("__is_abstract") ||
         parser_builtin_name("__is_polymorphic") ||
         parser_builtin_name("__is_empty") ||
         parser_builtin_name("__is_function"))) {
        return parse_builtin_cxx_type_trait(loc);
    }
    if (parser_builtin_name("__builtin_va_start") ||
        parser_builtin_name("__builtin_va_end") ||
        parser_builtin_name("__builtin_va_copy") ||
        parser_builtin_name("__builtin_va_arg")) {
        return parse_builtin_vararg(loc);
    }
    /* A dependent type parameter's qualified member spelling must be claimed
     * before the general qualified-name/template probes; those probes cannot
     * consume the disambiguating `template` keyword transactionally. */
    if (parser_cxx_mode && rcc_parse_cxx_dependent_member &&
        check(TOK_IDENT)) {
        Expr* dependent_member = rcc_parse_cxx_dependent_member();
        if (dependent_member) return dependent_member;
    }
    /* A small, structurally validated set of C++ function templates can be
     * expanded directly to the common expression AST.  The hook restores the
     * token cursor when the current spelling is not one of those templates. */
    if (parser_cxx_mode && rcc_parse_cxx_concept_expression &&
        (check(TOK_IDENT) || check(TOK_SCOPE))) {
        Expr* concept_call = rcc_parse_cxx_concept_expression();
        if (concept_call) return concept_call;
    }
    if (parser_cxx_mode && rcc_parse_cxx_template_call &&
        (check(TOK_IDENT) || check(TOK_SCOPE))) {
        Expr* template_call = rcc_parse_cxx_template_call();
        if (template_call) return template_call;
    }
    if (parser_cxx_mode && rcc_parse_cxx_qualified_template_member &&
        (check(TOK_IDENT) || check(TOK_SCOPE))) {
        Expr* template_member = rcc_parse_cxx_qualified_template_member();
        if (template_member) return template_member;
    }
    /* C++ aggregate direct-list initialization has the same storage and
     * initializer semantics as the compound-literal node already used by
     * the C backend.  Restrict this lowering to registered, complete C ABI
     * types; class construction remains with the C++ frontend. */
    if (parser_cxx_mode && rcc_parse_cxx_direct_list_type &&
        (check(TOK_IDENT) || check(TOK_SCOPE))) {
        Type* direct_type = rcc_parse_cxx_direct_list_type();
        if (direct_type) {
            Expr* initializer = parse_initializer();
            initializer->compound_type = direct_type;
            rcc_parser_validate_cxx_constructor_initializer(direct_type,
                                                            initializer);
            return initializer;
        }
    }
    if (parser_cxx_mode && check(TOK_IDENT) && parser.cur->next &&
        parser.cur->next->type == TOK_LBRACE) {
        Type* direct_type = parser_lookup_type(peek()->value.str_val);
        if (direct_type && type_is_complete(direct_type) &&
            direct_type->kind != TYPE_FUNC &&
            direct_type->kind != TYPE_VOID) {
            advance();
            Expr* initializer = parse_initializer();
            initializer->compound_type = direct_type;
            rcc_parser_validate_cxx_constructor_initializer(direct_type,
                                                            initializer);
            return initializer;
        }
    }
    /* A class temporary written as `C(args)` is distinct from an ordinary
     * function call.  Let the C++ frontend claim only a parser-known class
     * spelling; unknown identifiers are restored so normal call parsing is
     * unchanged. */
    if (parser_cxx_mode && rcc_parse_cxx_functional_cast) {
        Expr* functional_cast = rcc_parse_cxx_functional_cast();
        if (functional_cast) return functional_cast;
    }
    if (parser_cxx_mode &&
        (check(TOK_SCOPE) ||
         (check(TOK_IDENT) && parser.cur->next &&
          parser.cur->next->type == TOK_SCOPE))) {
        const char* qualified_name = parse_expression_qualified_name(loc);
        int64_t enum_value;
        Type* enum_type = NULL;
        Type* enum_expression_type = NULL;
        bool enum_is_deprecated = false;
        const char* enum_deprecated_message = NULL;
        if (parser_lookup_enum_constant(qualified_name, &enum_value,
                                        &enum_type, &enum_expression_type,
                                        &enum_is_deprecated,
                                        &enum_deprecated_message)) {
            Expr* value = expr_int(enum_value, loc);
            /* C enumerator identifiers have type int; C++ keeps the enum
             * type for the enumerator expression. */
            value->type = parser_cxx_mode && enum_type
                ? enum_expression_type : type_int;
            value->is_deprecated_enumerator = enum_is_deprecated;
            value->deprecated_enumerator_name = enum_is_deprecated
                ? qualified_name : NULL;
            value->deprecated_enumerator_message = enum_deprecated_message;
            return value;
        }
        Expr* identifier = expr_ident(qualified_name, loc);
        if (parser_cxx_mode && rcc_parser_cxx_bind_explicit_global_variable) {
            rcc_parser_cxx_bind_explicit_global_variable(identifier);
        }
        return identifier;
    }
    if (match(TOK_IDENT)) {
        int64_t enum_value;
        Type* enum_type = NULL;
        Type* enum_expression_type = NULL;
        bool enum_is_deprecated = false;
        const char* enum_deprecated_message = NULL;
        const char* local_using_name = NULL;
        if (parser_lookup_enum_constant(previous()->value.str_val,
                                        &enum_value, &enum_type,
                                        &enum_expression_type,
                                        &enum_is_deprecated,
                                        &enum_deprecated_message)) {
            Expr* value = expr_int(enum_value, loc);
            /* Match the language-specific enumerator expression type. */
            value->type = parser_cxx_mode && enum_type
                ? enum_expression_type : type_int;
            value->is_deprecated_enumerator = enum_is_deprecated;
            value->deprecated_enumerator_name = enum_is_deprecated
                ? previous()->value.str_val : NULL;
            value->deprecated_enumerator_message = enum_deprecated_message;
            return value;
        }
    if (parser_cxx_mode && rcc_parser_cxx_capture_expression) {
            Expr* capture = rcc_parser_cxx_capture_expression(
                previous()->value.str_val, loc);
            if (capture) return capture;
        }
        if (parser_cxx_mode && rcc_parser_cxx_resolve_local_using) {
            local_using_name = rcc_parser_cxx_resolve_local_using(
                previous()->value.str_val, loc);
        }
        Expr* identifier = expr_ident(
            local_using_name ? local_using_name : previous()->value.str_val,
            loc);
        if (parser_cxx_mode && rcc_parser_cxx_bind_value_identifier) {
            rcc_parser_cxx_bind_value_identifier(identifier);
        }
        return identifier;
    }
    if (parser_cxx_mode && rcc_parse_cxx_fold_expression) {
        Expr* fold = rcc_parse_cxx_fold_expression();
        if (fold) return fold;
    }
    if (match(TOK_LPAREN)) {
        bool tracks_template_parentheses =
            parser_cxx_template_default_mode;
        if (tracks_template_parentheses) {
            ++parser_cxx_template_paren_depth;
        }
        Expr* e = parse_expression();
        expect(TOK_RPAREN, ")");
        if (tracks_template_parentheses &&
            parser_cxx_template_paren_depth > 0) {
            --parser_cxx_template_paren_depth;
        }
        if (e) e->cxx_parenthesized = true;
        return e;
    }
    if (match(TOK__ALIGNOF)) {
        Type* type;
        expect(TOK_LPAREN, "(");
        if (!is_type_start()) {
            rcc_error(peek()->loc, "_Alignof requires a type name");
            type = type_int;
        } else {
            type = parse_type_spec();
            type = parse_type_name_declarator(type, loc);
        }
        expect(TOK_RPAREN, ")");
        if (!type_is_complete(type) || type->kind == TYPE_FUNC) {
            rcc_error(loc, "_Alignof requires a complete object type");
        }
        return expr_alignof_type(type, loc);
    }
    if (match(TOK_SIZEOF)) {
        if (parser_cxx_mode && match(TOK_ELLIPSIS)) {
            const char* pack_name = NULL;
            expect(TOK_LPAREN, "'(' after sizeof...");
            if (check(TOK_IDENT)) {
                pack_name = advance()->value.str_val;
            } else {
                rcc_error(peek()->loc,
                          "sizeof... requires a template parameter pack name");
            }
            expect(TOK_RPAREN, ")");
            return expr_sizeof_pack(pack_name, loc);
        }
        if (match(TOK_LPAREN)) {
            if (is_type_start()) {
                Type* type = parse_type_spec();
                type = parse_type_name_declarator(type, loc);
                expect(TOK_RPAREN, ")");
                return expr_sizeof_type(type, loc);
            } else {
                Expr* e = parse_expression();
                expect(TOK_RPAREN, ")");
                return expr_sizeof_expr(e, loc);
            }
        } else {
            Expr* e = parse_unary();
            return expr_sizeof_expr(e, loc);
        }
    }

    rcc_error(loc, "expected expression");
    return expr_int(0, loc);
}

static SourceLoc parse_postfix_call_location(const Expr* callee,
                                            SourceLoc fallback) {
    while (callee) {
        switch (callee->kind) {
            case EXPR_MEMBER:
            case EXPR_PTR_MEMBER:
                callee = callee->member_base;
                continue;
            case EXPR_INDEX:
                callee = callee->index_base;
                continue;
            default:
                return callee->loc;
        }
    }
    return fallback;
}

/* Postfix: a[i], a.m, a->m, a++, a--, f(args) */
static Expr* parse_postfix_tail(Expr* e) {
    while (1) {
        SourceLoc loc = peek()->loc;

        if (match(TOK_LBRACKET)) {
            Expr* index = parse_expression();
            expect(TOK_RBRACKET, "]");
            e = expr_index(e, index, loc);
        } else if (match(TOK_DOT)) {
            if (parser_cxx_mode) (void)match(TOK_TEMPLATE);
            if (parser_cxx_mode && match(TOK_TILDE)) {
                Token* destructor_name = expect(TOK_IDENT,
                                                "destructor name");
                if (destructor_name) {
                    e = expr_member(e, "~dtor", loc);
                    e->member_is_explicit_destructor = true;
                    e->member_destructor_name =
                        destructor_name->value.str_val;
                    e->member_destructor_type =
                        rcc_parser_cxx_destructor_type
                            ? rcc_parser_cxx_destructor_type(
                                  e->member_destructor_name)
                            : NULL;
                    if (!e->member_destructor_type) {
                        rcc_error(destructor_name->loc,
                                  "unknown pseudo-destructor type '%s'",
                                  e->member_destructor_name);
                    }
                }
            } else {
                Token* name = expect(TOK_IDENT, "member name");
                if (name) {
                    e = expr_member(e, name->value.str_val, loc);
                    if (parser_cxx_mode &&
                        rcc_parse_cxx_member_template_arguments) {
                        (void)rcc_parse_cxx_member_template_arguments(e);
                    }
                }
            }
        } else if (match(TOK_ARROW)) {
            if (parser_cxx_mode) (void)match(TOK_TEMPLATE);
            if (parser_cxx_mode && match(TOK_TILDE)) {
                Token* destructor_name = expect(TOK_IDENT,
                                                "destructor name");
                if (destructor_name) {
                    Expr* m = expr_member(e, "~dtor", loc);
                    m->kind = EXPR_PTR_MEMBER;
                    m->member_is_explicit_destructor = true;
                    m->member_destructor_name =
                        destructor_name->value.str_val;
                    m->member_destructor_type =
                        rcc_parser_cxx_destructor_type
                            ? rcc_parser_cxx_destructor_type(
                                  m->member_destructor_name)
                            : NULL;
                    if (!m->member_destructor_type) {
                        rcc_error(destructor_name->loc,
                                  "unknown pseudo-destructor type '%s'",
                                  m->member_destructor_name);
                    }
                    e = m;
                }
            } else {
                Token* name = expect(TOK_IDENT, "member name");
                if (name) {
                    Expr* m = expr_member(e, name->value.str_val, loc);
                    m->kind = EXPR_PTR_MEMBER;
                    if (parser_cxx_mode &&
                        rcc_parse_cxx_member_template_arguments) {
                        (void)rcc_parse_cxx_member_template_arguments(m);
                    }
                    e = m;
                }
            }
        } else if (match(TOK_INC)) {
            e = expr_unary(EXPR_POSTINC, e, loc);
        } else if (match(TOK_DEC)) {
            e = expr_unary(EXPR_POSTDEC, e, loc);
        } else if (match(TOK_LPAREN)) {
            /* Function call */
            SourceLoc call_loc = parse_postfix_call_location(e, loc);
            ExprList* args = NULL;
            if (!check(TOK_RPAREN)) {
                do {
                    Expr* arg = parse_assignment();
                    if (parser_cxx_mode && match(TOK_ELLIPSIS)) {
                        if (!arg) {
                            rcc_error(loc,
                                      "C++ pack expansion requires an expression pattern");
                        } else if (arg->kind == EXPR_IDENT &&
                                   arg->ident_name) {
                            arg->cxx_pack_expansion = true;
                            arg->cxx_pack_expansion_name = arg->ident_name;
                        } else {
                            arg->cxx_pack_expansion = true;
                            arg->cxx_pack_expansion_pattern = arg;
                        }
                    }
                    exprlist_append(&args, arg);
                } while (match(TOK_COMMA));
            }
            expect(TOK_RPAREN, ")");
            e = expr_call(e, args, call_loc);
            if (parser_cxx_mode && rcc_parser_cxx_record_call_context) {
                rcc_parser_cxx_record_call_context(e);
            }
        } else {
            break;
        }
    }

    return e;
}

static Expr* parse_postfix(void) {
    return parse_postfix_tail(parse_primary());
}

/* Unary: ++a, --a, &a, *a, +a, -a, ~a, !a */
static Expr* parse_unary(void) {
    SourceLoc loc = peek()->loc;

    if (check(TOK_IDENT) &&
        (strcmp(peek()->value.str_val, "__real__") == 0 ||
         strcmp(peek()->value.str_val, "__imag__") == 0)) {
        const char* component =
            strcmp(peek()->value.str_val, "__real__") == 0
                ? "__real__" : "__imag__";
        advance();
        return expr_member(parse_unary(), component, loc);
    }

    if (parser_cxx_mode && match(TOK_TYPEID)) {
        Token* operand_start_cur;
        Token* operand_start_prev;
        Type* operand_type = NULL;
        Expr* operand;

        expect(TOK_LPAREN, "(");
        operand_start_cur = parser.cur;
        operand_start_prev = parser.prev;
        if (rcc_parse_cxx_type_name && is_type_start()) {
            operand_type = rcc_parse_cxx_type_name();
            if (operand_type && match(TOK_RPAREN)) {
                return parse_postfix_tail(expr_cxx_typeid(
                    operand_type, NULL, true, loc));
            }
            /* A type-name parse is speculative here: `typeid(value)` may
             * begin with an identifier that is not a type in this scope. */
            parser.cur = operand_start_cur;
            parser.prev = operand_start_prev;
        }
        operand = parse_expression();
        expect(TOK_RPAREN, ")");
        return parse_postfix_tail(expr_cxx_typeid(
            NULL, operand, false, loc));
    }

    if (parser_cxx_mode && match(TOK_NOEXCEPT)) {
        Expr* operand;
        expect(TOK_LPAREN, "(");
        operand = parse_expression();
        expect(TOK_RPAREN, ")");
        return parse_postfix_tail(
            expr_unary(EXPR_NOEXCEPT, operand, loc));
    }

    if (parser_cxx_mode && rcc_parse_cxx_type_name &&
        (check(TOK_DYNAMIC_CAST) || check(TOK_CONST_CAST))) {
        TokenType cast_token = advance()->type;
        Type* cast_type;
        Expr* operand;
        expect(TOK_LT, "<");
        cast_type = rcc_parse_cxx_type_name();
        if (!cast_type) {
            rcc_error(peek()->loc, "C++ named cast requires a type name");
            cast_type = type_int;
        }
        expect(TOK_GT, ">");
        expect(TOK_LPAREN, "(");
        operand = parse_expression();
        expect(TOK_RPAREN, ")");
        Expr* cast = expr_cast(cast_type, operand, loc);
        cast->cxx_cast_kind = cast_token == TOK_DYNAMIC_CAST
            ? CXX_CAST_DYNAMIC : CXX_CAST_CONST;
        return parse_postfix_tail(cast);
    }

    /* The SDK's fixed-width wrappers only need value-preserving static and
     * reinterpret casts.  Lower both named forms to the existing typed cast
     * node so the 32/64-bit semantic and code-generation paths stay shared. */
    if (parser_cxx_mode && rcc_parse_cxx_type_name &&
        (check(TOK_STATIC_CAST) || check(TOK_REINTERPRET_CAST))) {
        TokenType cast_token = advance()->type;
        expect(TOK_LT, "<");
        Type* cast_type;
        if (parser_cxx_mode) {
            cast_type = rcc_parse_cxx_type_name();
            if (!cast_type) {
                rcc_error(peek()->loc, "C++ named cast requires a type name");
                cast_type = type_int;
            }
            if (check(TOK_LPAREN) &&
                parser_parenthesized_declarator_starts_member_pointer() &&
                parser_parenthesized_pointer_is_function()) {
                cast_type = parse_declarator(cast_type, NULL, NULL);
            }
        } else if (!is_type_start()) {
            rcc_error(peek()->loc, "C++ named cast requires a type name");
            cast_type = type_int;
        } else {
            cast_type = parse_type_spec();
            cast_type = parse_type_name_declarator(cast_type, loc);
        }
        expect(TOK_GT, ">");
        expect(TOK_LPAREN, "(");
        Expr* operand = parse_expression();
        expect(TOK_RPAREN, ")");
        Expr* cast = expr_cast(cast_type, operand, loc);
        cast->cxx_cast_kind = cast_token == TOK_STATIC_CAST
            ? CXX_CAST_STATIC : CXX_CAST_REINTERPRET;
        return parse_postfix_tail(cast);
    }

    if (check(TOK_LPAREN) && parser.cur->next) {
        Token* saved_cur = parser.cur;
        Token* saved_prev = parser.prev;
        advance();
        if (is_type_start()) {
            Type* cast_type = parse_type_spec();
            cast_type = parse_type_name_declarator(cast_type, loc);
            expect(TOK_RPAREN, ")");
            if (check(TOK_LBRACE)) {
                Expr* literal = parse_initializer();
                literal->compound_type = cast_type;
                return parse_postfix_tail(literal);
            }
            return expr_cast(cast_type, parse_unary(), loc);
        }
        parser.cur = saved_cur;
        parser.prev = saved_prev;
    }

    if (match(TOK_INC)) {
        Expr* e = parse_unary();
        return expr_unary(EXPR_PREINC, e, loc);
    }
    if (match(TOK_DEC)) {
        Expr* e = parse_unary();
        return expr_unary(EXPR_PREDEC, e, loc);
    }
    if (match(TOK_AMP)) {
        if (parser_cxx_mode && rcc_parse_cxx_member_pointer_address) {
            Expr* member_pointer = rcc_parse_cxx_member_pointer_address();
            if (member_pointer) return member_pointer;
        }
        Expr* e = parse_unary();
        Expr* address = expr_unary(EXPR_ADDR, e, loc);
        address->cxx_overloadable_address_of = parser_cxx_mode;
        return address;
    }
    if (match(TOK_STAR)) {
        Expr* e = parse_unary();
        return expr_unary(EXPR_DEREF, e, loc);
    }
    if (match(TOK_PLUS)) {
        Expr* e = parse_unary();
        Expr* expression = expr_unary(EXPR_POS, e, loc);
        if (parser_cxx_mode &&
            rcc_parser_cxx_set_expression_template_context) {
            rcc_parser_cxx_set_expression_template_context(expression);
        }
        return expression;
    }
    if (match(TOK_MINUS)) {
        Expr* e = parse_unary();
        return expr_unary(EXPR_NEG, e, loc);
    }
    if (match(TOK_TILDE)) {
        Expr* e = parse_unary();
        return expr_unary(EXPR_BITNOT, e, loc);
    }
    if (match(TOK_NOT)) {
        Expr* e = parse_unary();
        return expr_unary(EXPR_NOT, e, loc);
    }

    return parse_postfix();
}

/* Pointer-to-member operators bind more tightly than multiplication. */
static Expr* parse_pointer_to_member(void) {
    Expr* expression = parse_unary();
    while (parser_cxx_mode &&
           (check(TOK_DOT_STAR) || check(TOK_ARROW_STAR))) {
        Token* operation = advance();
        Expr* member_pointer = parse_unary();
        expression = expr_binary(
            operation->type == TOK_DOT_STAR
                ? EXPR_CXX_MEMBER_PTR_DOT
                : EXPR_CXX_MEMBER_PTR_ARROW,
            expression, member_pointer, operation->loc);
    }
    return expression;
}

/* Multiplicative: a * b, a / b, a % b */
static Expr* parse_multiplicative(void) {
    Expr* e = parse_pointer_to_member();

    while (1) {
        SourceLoc loc = peek()->loc;
        if (match(TOK_STAR)) {
            e = expr_binary(EXPR_MUL, e, parse_pointer_to_member(), loc);
        } else if (match(TOK_SLASH)) {
            e = expr_binary(EXPR_DIV, e, parse_pointer_to_member(), loc);
        } else if (match(TOK_PERCENT)) {
            e = expr_binary(EXPR_MOD, e, parse_pointer_to_member(), loc);
        } else {
            break;
        }
    }

    return e;
}

/* Additive: a + b, a - b */
static Expr* parse_additive(void) {
    Expr* e = parse_multiplicative();

    while (1) {
        SourceLoc loc = peek()->loc;
        if (match(TOK_PLUS)) {
            e = expr_binary(EXPR_ADD, e, parse_multiplicative(), loc);
        } else if (match(TOK_MINUS)) {
            e = expr_binary(EXPR_SUB, e, parse_multiplicative(), loc);
        } else {
            break;
        }
    }

    return e;
}

static bool parser_cxx_template_shift_closes_arguments(void) {
    Token* token = parser.cur;
    TokenType next_type;
    if (!parser_cxx_template_default_at_argument_boundary() || !token ||
        token->type != TOK_RSHIFT) {
        return false;
    }
    next_type = token->next ? token->next->type : TOK_EOF;
    /* In a non-type template argument, a trailing `>>` closes nested
     * template-ids when no right operand follows. Leave it for expect('>')
     * to split; parenthesized shifts and shifts with an operand still parse
     * as expressions. */
    return next_type == TOK_COMMA || next_type == TOK_GT ||
           next_type == TOK_RPAREN || next_type == TOK_RBRACKET ||
           next_type == TOK_RBRACE || next_type == TOK_LBRACE ||
           next_type == TOK_COLON || next_type == TOK_SEMICOLON ||
           next_type == TOK_EOF;
}

/* Shift: a << b, a >> b */
static Expr* parse_shift(void) {
    Expr* e = parse_additive();

    while (1) {
        SourceLoc loc = peek()->loc;
        if (match(TOK_LSHIFT)) {
            e = expr_binary(EXPR_LSHIFT, e, parse_additive(), loc);
        } else if (parser_cxx_template_shift_closes_arguments()) {
            break;
        } else if (match(TOK_RSHIFT)) {
            e = expr_binary(EXPR_RSHIFT, e, parse_additive(), loc);
        } else {
            break;
        }
    }

    return e;
}

/* Relational: a < b, a > b, a <= b, a >= b */
static Expr* parse_relational(void) {
    Expr* e = parse_shift();

    while (1) {
        SourceLoc loc = peek()->loc;
        /* At top level in a non-type template argument, `>` closes the
         * template-id. Parenthesized expressions may still use it as the
         * relational operator. */
        if (parser_cxx_template_default_at_argument_boundary() &&
            check(TOK_GT)) break;
        if (match(TOK_LT)) {
            e = expr_binary(EXPR_LT, e, parse_shift(), loc);
        } else if (match(TOK_GT)) {
            e = expr_binary(EXPR_GT, e, parse_shift(), loc);
        } else if (match(TOK_LE)) {
            e = expr_binary(EXPR_LE, e, parse_shift(), loc);
        } else if (match(TOK_GE)) {
            e = expr_binary(EXPR_GE, e, parse_shift(), loc);
        } else if (parser_cxx_mode && match(TOK_SPACESHIP)) {
            e = expr_binary(EXPR_SPACESHIP, e, parse_shift(), loc);
        } else {
            break;
        }
    }

    return e;
}

/* Equality: a == b, a != b */
static Expr* parse_equality(void) {
    Expr* e = parse_relational();

    while (1) {
        SourceLoc loc = peek()->loc;
        if (match(TOK_EQ)) {
            e = expr_binary(EXPR_EQ, e, parse_relational(), loc);
        } else if (match(TOK_NE)) {
            e = expr_binary(EXPR_NE, e, parse_relational(), loc);
        } else {
            break;
        }
    }

    return e;
}

/* Bitwise AND: a & b */
static Expr* parse_bitand(void) {
    Expr* e = parse_equality();

    while (match(TOK_AMP)) {
        SourceLoc loc = previous()->loc;
        e = expr_binary(EXPR_BITAND, e, parse_equality(), loc);
    }

    return e;
}

/* Bitwise XOR: a ^ b */
static Expr* parse_bitxor(void) {
    Expr* e = parse_bitand();

    while (match(TOK_CARET)) {
        SourceLoc loc = previous()->loc;
        e = expr_binary(EXPR_BITXOR, e, parse_bitand(), loc);
    }

    return e;
}

/* Bitwise OR: a | b */
static Expr* parse_bitor(void) {
    Expr* e = parse_bitxor();

    while (match(TOK_PIPE)) {
        SourceLoc loc = previous()->loc;
        e = expr_binary(EXPR_BITOR, e, parse_bitxor(), loc);
    }

    return e;
}

/* Logical AND: a && b */
static Expr* parse_logand(void) {
    Expr* e = parse_bitor();

    while (match(TOK_AND)) {
        SourceLoc loc = previous()->loc;
        e = expr_binary(EXPR_AND, e, parse_bitor(), loc);
    }

    return e;
}

/* Logical OR: a || b */
static Expr* parse_logor(void) {
    Expr* e = parse_logand();

    while (match(TOK_OR)) {
        SourceLoc loc = previous()->loc;
        e = expr_binary(EXPR_OR, e, parse_logand(), loc);
    }

    return e;
}

/* Conditional: a ? b : c */
static Expr* parse_conditional(void) {
    Expr* e = parse_logor();

    if (match(TOK_QUESTION)) {
        SourceLoc loc = previous()->loc;
        Expr* then_expr = parse_expression();
        expect(TOK_COLON, ":");
        Expr* else_expr = parse_conditional();
        e = expr_cond(e, then_expr, else_expr, loc);
    }

    return e;
}

/* Assignment: a = b, a += b, etc. */
static Expr* parse_assignment(void) {
    Expr* e = parse_conditional();

    SourceLoc loc = peek()->loc;
    ExprKind kind = -1;

    if (match(TOK_ASSIGN)) kind = EXPR_ASSIGN;
    else if (match(TOK_PLUS_ASSIGN)) kind = EXPR_ADD_ASSIGN;
    else if (match(TOK_MINUS_ASSIGN)) kind = EXPR_SUB_ASSIGN;
    else if (match(TOK_STAR_ASSIGN)) kind = EXPR_MUL_ASSIGN;
    else if (match(TOK_SLASH_ASSIGN)) kind = EXPR_DIV_ASSIGN;
    else if (match(TOK_PERCENT_ASSIGN)) kind = EXPR_MOD_ASSIGN;
    else if (match(TOK_AMP_ASSIGN)) kind = EXPR_AND_ASSIGN;
    else if (match(TOK_PIPE_ASSIGN)) kind = EXPR_OR_ASSIGN;
    else if (match(TOK_CARET_ASSIGN)) kind = EXPR_XOR_ASSIGN;
    else if (match(TOK_LSHIFT_ASSIGN)) kind = EXPR_LSHIFT_ASSIGN;
    else if (match(TOK_RSHIFT_ASSIGN)) kind = EXPR_RSHIFT_ASSIGN;

    if (kind != (ExprKind)-1) {
        Expr* rhs = parse_assignment();
        e = expr_binary(kind, e, rhs, loc);
    }

    return e;
}

/* Export the assignment-expression grammar required by C++ default
 * arguments.  Unlike parse_expression(), it deliberately leaves a top-level
 * comma for the enclosing parameter list. */
Expr* parse_assignment_expression(void) {
    return parse_assignment();
}

/* Comma expression: a, b */
Expr* parse_expression(void) {
    Expr* e = parse_assignment();

    while (match(TOK_COMMA)) {
        SourceLoc loc = previous()->loc;
        e = expr_binary(EXPR_COMMA, e, parse_assignment(), loc);
    }

    return e;
}

/* ═══════════════════════════════════════
 * Type Parsing
 * ═══════════════════════════════════════ */

static bool is_type_start(void) {
    if (parser_cxx_mode && rcc_parse_cxx_type_start &&
        rcc_parse_cxx_type_start()) return true;
    if (!parser_cxx_mode && g_opts.c_standard >= 23 &&
        (check(TOK_TYPEOF) || check(TOK_TYPEOF_UNQUAL) ||
         check(TOK__BITINT))) return true;
    switch (peek()->type) {
        case TOK_DECLTYPE:
            return parser_cxx_mode;
        case TOK_TYPEDEF:
        case TOK_VOID:
        case TOK_CHAR:
        case TOK_SHORT:
        case TOK_INT:
        case TOK_LONG:
        case TOK_FLOAT:
        case TOK_DOUBLE:
        case TOK_SIGNED:
        case TOK_UNSIGNED:
        case TOK_STRUCT:
        case TOK_UNION:
        case TOK_ENUM:
        case TOK_CONST:
        case TOK_VOLATILE:
        case TOK_RESTRICT:
        case TOK_STATIC:
        case TOK_EXTERN:
        case TOK_REGISTER:
        case TOK_THREAD_LOCAL:
        case TOK__BOOL:
        case TOK__BITINT:
        case TOK__COMPLEX:
        case TOK__IMAGINARY:
        case TOK__ATOMIC:
        case TOK__NORETURN:
        case TOK___BUILTIN_VA_LIST:
        case TOK___ATTRIBUTE__:
        case TOK___INLINE__:
        case TOK_INLINE:
            return true;
        default:
            return check(TOK_IDENT) &&
                   parser_lookup_type(peek()->value.str_val) != NULL;
    }
}

typedef struct ParsedInitializerDesignator {
    InitDesignatorKind kind;
    int64_t index;
    const char* field;
    SourceLoc loc;
    struct ParsedInitializerDesignator* next;
} ParsedInitializerDesignator;

static Expr* parse_initializer(void) {
    ExprList* items = NULL;
    SourceLoc loc;
    bool value_init = false;
    if (!match(TOK_LBRACE)) return parse_assignment();
    loc = previous()->loc;
    if (check(TOK_RBRACE)) {
        if (parser_cxx_mode) {
            /* C++ value-initialization zeroes scalars and aggregates.  The
             * existing {0} semantic/codegen path already implements that
             * contract for both target ABIs. */
            exprlist_append_designated(
                &items, expr_int(0, loc), INIT_DESIGNATOR_NONE, 0, NULL);
            value_init = true;
        } else if (g_opts.c_standard >= 23) {
            /* C23 empty initialization zero-initializes the object.  Lower it
             * through the existing {0} initializer path so aggregate tails
             * keep the same zero-initialization semantics. */
            exprlist_append_designated(
                &items, expr_int(0, loc), INIT_DESIGNATOR_NONE, 0, NULL);
        } else {
            rcc_error(loc, "empty initializer list is not valid C17");
        }
    } else {
        for (;;) {
            ParsedInitializerDesignator* designators = NULL;
            ParsedInitializerDesignator** designator_tail = &designators;
            unsigned designator_count = 0u;
            while (check(TOK_DOT) || check(TOK_LBRACKET)) {
                ParsedInitializerDesignator* designator =
                    rcc_alloc(sizeof(*designator));
                if (match(TOK_DOT)) {
                    Token* name;
                    designator->kind = INIT_DESIGNATOR_FIELD;
                    designator->loc = previous()->loc;
                    name = expect(TOK_IDENT, "field designator");
                    if (name) designator->field = name->value.str_val;
                } else {
                    Expr* index_expression;
                    match(TOK_LBRACKET);
                    designator->kind = INIT_DESIGNATOR_INDEX;
                    designator->loc = previous()->loc;
                    index_expression = parse_assignment();
                    if (!eval_integer_constant(index_expression,
                                               &designator->index) ||
                        designator->index < 0) {
                        rcc_error(
                            index_expression->loc,
                            "array designator must be a non-negative integer constant");
                        designator->index = 0;
                    }
                    expect(TOK_RBRACKET, "]");
                }
                ++designator_count;
                *designator_tail = designator;
                designator_tail = &designator->next;
            }
            if (parser_cxx_mode && designator_count > 0u &&
                !rcc_parser_cxx_standard_at_least(20)) {
                rcc_error(loc,
                          "C++ designated initializers require C++20 or newer");
            }
            if (parser_cxx_mode && designator_count > 1u) {
                rcc_error(loc,
                          "C++ designated initializer cannot use nested designators");
            }
            if (designators) {
                expect(TOK_ASSIGN, "=");
            }
            {
                Expr* value = parse_initializer();
                if (designators) {
                    ParsedInitializerDesignator* reversed = NULL;
                    ParsedInitializerDesignator* item = designators->next;
                    while (item) {
                        ParsedInitializerDesignator* next = item->next;
                        item->next = reversed;
                        reversed = item;
                        item = next;
                    }
                    while (reversed) {
                        ParsedInitializerDesignator* next = reversed->next;
                        ExprList* nested = NULL;
                        exprlist_append_designated(
                            &nested, value, reversed->kind,
                            reversed->index, reversed->field);
                        value = expr_initializer_list(nested, reversed->loc);
                        value->compound_designator_wrapper = true;
                        rcc_free(reversed);
                        reversed = next;
                    }
                    exprlist_append_designated(
                        &items, value, designators->kind,
                        designators->index, designators->field);
                    rcc_free(designators);
                } else {
                    exprlist_append_designated(
                        &items, value, INIT_DESIGNATOR_NONE, 0, NULL);
                }
            }
            if (!match(TOK_COMMA) || check(TOK_RBRACE)) break;
        }
    }
    expect(TOK_RBRACE, "}");
    Expr* initializer = expr_initializer_list(items, loc);
    initializer->compound_value_init = value_init;
    return initializer;
}

Expr* rcc_parser_parse_initializer(void) {
    return parse_initializer();
}

static int parser_align_up(int value, int alignment) {
    if (alignment <= 1) return value;
    return (value + alignment - 1) & ~(alignment - 1);
}

static int parser_effective_pack_alignment(void) {
    int alignment = parser_pack_alignment;
    if (parser_aggregate_pack_alignment > 0 &&
        (alignment == 0 || parser_aggregate_pack_alignment < alignment)) {
        alignment = parser_aggregate_pack_alignment;
    }
    return alignment;
}

static int parse_explicit_alignment(void) {
    SourceLoc loc = peek()->loc;
    Type* alignment_type;
    Expr* alignment_expression;
    int64_t value;
    int alignment = 0;

    expect(TOK__ALIGNAS, "_Alignas");
    expect(TOK_LPAREN, "'(' after _Alignas");
    if (is_type_start()) {
        alignment_type = parse_type_spec();
        alignment_type = parse_type_name_declarator(alignment_type, loc);
        alignment = alignment_type && alignment_type->align > 0
            ? alignment_type->align : 0;
    } else {
        alignment_expression = parse_assignment();
        if (!expr_eval_integer_constant(alignment_expression, &value) ||
            value < 0 || value > INT_MAX) {
            rcc_error(loc,
                      "_Alignas requires a non-negative integer constant or type");
        } else {
            alignment = (int)value;
        }
    }
    expect(TOK_RPAREN, "')' after _Alignas");
    if (alignment != 0 &&
        (alignment < 1 ||
         alignment > (int)RCC_MAX_SUPPORTED_ALIGNMENT ||
         (alignment & (alignment - 1)) != 0)) {
        rcc_error(loc,
                  "_Alignas alignment must be a power of two no greater than 16384");
        alignment = 0;
    }
    return alignment;
}

static Type* apply_explicit_alignment(Type* type, int alignment,
                                      SourceLoc loc) {
    Type* aligned;
    if (!type || alignment <= type->align) return type;
    if (type->kind == TYPE_FUNC) {
        rcc_error(loc, "_Alignas cannot apply to a function declaration");
        return type;
    }
    aligned = ast_arena_alloc(sizeof(*aligned));
    *aligned = *type;
    aligned->align = alignment;
    aligned->has_explicit_alignment = true;
    return aligned;
}

int rcc_parser_parse_explicit_alignment(void) {
    return parse_explicit_alignment();
}

Type* rcc_parser_apply_explicit_alignment(Type* type, int alignment,
                                           SourceLoc loc) {
    return apply_explicit_alignment(type, alignment, loc);
}

static bool enum_value_fits_fixed_underlying(int64_t value,
                                             bool value_is_unsigned,
                                             const Type* enum_type) {
    const Type* underlying;
    unsigned width;
    uint64_t unsigned_max;
    int64_t signed_min;
    int64_t signed_max;

    if (!enum_type || !enum_type->enum_has_fixed_underlying) return true;
    underlying = enum_type->enum_underlying_type;
    if (!underlying || underlying->size <= 0 || underlying->size > 8) {
        return false;
    }
    width = type_integer_width(underlying);
    unsigned_max = width >= 64u
        ? UINT64_MAX : (UINT64_C(1) << width) - 1u;
    if (underlying->is_unsigned) {
        return (value_is_unsigned || value >= 0) &&
               (uint64_t)value <= unsigned_max;
    }
    if (value_is_unsigned) {
        uint64_t signed_limit = width >= 64u
            ? (uint64_t)INT64_MAX
            : (UINT64_C(1) << (width - 1u)) - 1u;
        return (uint64_t)value <= signed_limit;
    }
    if (width >= 64u) return true;
    signed_min = -(INT64_C(1) << (width - 1u));
    signed_max = (INT64_C(1) << (width - 1u)) - 1;
    return value >= signed_min && value <= signed_max;
}

static uint64_t enum_integer_type_max(const Type* type) {
    unsigned width = type_integer_width(type);
    if (width == 0u) return 0u;
    if (type->kind == TYPE_BOOL) return 1u;
    if (type->is_unsigned) {
        return width >= 64u ? UINT64_MAX
                            : (UINT64_C(1) << width) - 1u;
    }
    return width >= 64u ? (uint64_t)INT64_MAX
                        : (UINT64_C(1) << (width - 1u)) - 1u;
}

static int64_t enum_integer_type_min(const Type* type) {
    unsigned width = type_integer_width(type);
    if (!type || type->is_unsigned || width == 0u) return 0;
    if (width >= 64u) return INT64_MIN;
    return -(INT64_C(1) << (width - 1u));
}

static bool enum_value_fits_integer_type(int64_t value,
                                         bool value_is_unsigned,
                                         const Type* type) {
    uint64_t maximum;
    if (!type || !type_is_integer((Type*)type) || type->size <= 0 ||
        type->size > 8) {
        return false;
    }
    maximum = enum_integer_type_max(type);
    if (type->is_unsigned) {
        return (value_is_unsigned || value >= 0) &&
               (uint64_t)value <= maximum;
    }
    if (value_is_unsigned) return (uint64_t)value <= maximum;
    return value >= enum_integer_type_min(type) &&
           value <= (int64_t)maximum;
}

static Type* enum_integer_type_for_value(int64_t value,
                                         bool value_is_unsigned) {
    Type* candidates[] = {
        type_int, type_uint, type_long, type_ulong, type_llong, type_ullong
    };
    for (size_t index = 0u;
         index < sizeof(candidates) / sizeof(candidates[0]); ++index) {
        if (enum_value_fits_integer_type(value, value_is_unsigned,
                                        candidates[index])) {
            return candidates[index];
        }
    }
    return NULL;
}

static bool enum_integer_type_fits_all_values(const Type* integer_type,
                                              const Type* enum_type) {
    if (!enum_type) return false;
    for (int index = 0; index < enum_type->enum_constant_count; ++index) {
        const EnumConstantInfo* item = &enum_type->enum_constants[index];
        if (!enum_value_fits_integer_type(item->value, item->is_unsigned,
                                          integer_type)) {
            return false;
        }
    }
    return true;
}

static Type* enum_select_underlying_type(Type* enum_type, SourceLoc loc) {
    Type* candidates[] = {
        type_int, type_uint, type_long, type_ulong, type_llong, type_ullong
    };
    Type* selected = NULL;
    for (size_t index = 0u;
         index < sizeof(candidates) / sizeof(candidates[0]); ++index) {
        if (enum_integer_type_fits_all_values(candidates[index], enum_type)) {
            selected = candidates[index];
            break;
        }
    }
    if (!selected) {
        rcc_error(loc,
                  "no supported integer type can represent all enumerators");
        selected = type_int;
    }
    enum_type->enum_underlying_type = selected;
    enum_type->size = selected->size;
    enum_type->align = selected->align;
    enum_type->is_unsigned = selected->is_unsigned;
    return selected;
}

static bool enum_next_unfixed_value(int64_t value, Type* value_type,
                                    int64_t* next_value,
                                    bool* next_is_unsigned,
                                    Type** next_type) {
    uint64_t current;
    uint64_t maximum;
    if (!value_type || !type_is_integer(value_type) ||
        value_type->size <= 0 || value_type->size > 8 ||
        !next_value || !next_is_unsigned || !next_type) {
        return false;
    }
    maximum = enum_integer_type_max(value_type);
    if (value_type->is_unsigned) {
        current = (uint64_t)value;
        if (current < maximum) {
            *next_value = (int64_t)(current + 1u);
            *next_is_unsigned = true;
            *next_type = value_type;
            return true;
        }
        if (current == UINT64_MAX) return false;
        current += 1u;
        *next_value = (int64_t)current;
        *next_type = enum_integer_type_for_value(*next_value, true);
        if (!*next_type) return false;
        *next_is_unsigned = (*next_type)->is_unsigned;
        return true;
    }
    if (value < (int64_t)maximum) {
        *next_value = value + 1;
        *next_is_unsigned = false;
        *next_type = value_type;
        return true;
    }
    if (value == INT64_MAX) return false;
    *next_value = value + 1;
    *next_type = enum_integer_type_for_value(*next_value, false);
    if (!*next_type) return false;
    *next_is_unsigned = (*next_type)->is_unsigned;
    return true;
}

static int64_t parse_enum_value(Type* enum_type, int64_t fallback,
                                bool* result_is_unsigned,
                                Type** result_type) {
    Expr* expression = parse_conditional();
    IntegerConstantValue evaluated;
    int64_t value;
    uint64_t bits;
    if (!eval_integer_constant_typed(expression, &evaluated)) {
        rcc_error(expression ? expression->loc : peek()->loc,
                  "enumerator value is not an integer constant expression");
        return fallback;
    }
    if (result_is_unsigned) *result_is_unsigned = false;
    if (result_type) *result_type = evaluated.type;
    bits = evaluated.bits & integer_type_mask(evaluated.type);
    if (!parser_cxx_mode) {
        unsigned int_bits = type_int && type_int->size > 0
            ? (unsigned)type_int->size * 8u : 32u;
        int64_t minimum = -(INT64_C(1) << (int_bits - 1u));
        int64_t maximum = (INT64_C(1) << (int_bits - 1u)) - 1;
        if ((evaluated.type && evaluated.type->is_unsigned &&
             bits > (uint64_t)maximum) ||
            (evaluated.type && !evaluated.type->is_unsigned &&
             (integer_constant_signed(evaluated) < minimum ||
              integer_constant_signed(evaluated) > maximum))) {
            rcc_error(expression->loc,
                      "enumerator value is not representable as int");
            return fallback;
        }
        value = evaluated.type && evaluated.type->is_unsigned
            ? (int64_t)bits : integer_constant_signed(evaluated);
    } else {
        bool is_unsigned = evaluated.type && evaluated.type->is_unsigned;
        bool full_unsigned_underlying =
            enum_type && enum_type->enum_has_fixed_underlying &&
            enum_type->enum_underlying_type &&
            enum_type->enum_underlying_type->is_unsigned &&
            enum_type->enum_underlying_type->size == 8;
        if (is_unsigned && bits > (uint64_t)INT64_MAX &&
            !full_unsigned_underlying &&
            (!enum_type || enum_type->enum_has_fixed_underlying)) {
            if (enum_type && enum_type->enum_has_fixed_underlying) {
                rcc_error(expression->loc,
                          "enumerator value is not representable in its fixed underlying type");
            } else {
                rcc_error(expression->loc,
                          "enumerator value exceeds the supported 64-bit range");
            }
            return fallback;
        }
        value = evaluated.type && evaluated.type->is_unsigned
            ? (int64_t)bits : integer_constant_signed(evaluated);
        if (!enum_value_fits_fixed_underlying(value, is_unsigned,
                                               enum_type)) {
            rcc_error(expression->loc,
                      "enumerator value is not representable in its fixed underlying type");
            return fallback;
        }
        if (result_is_unsigned) *result_is_unsigned = is_unsigned;
        if (result_type && evaluated.type &&
            evaluated.type->kind == TYPE_ENUM &&
            evaluated.type->enum_underlying_type) {
            *result_type = evaluated.type->enum_underlying_type;
        }
    }
    return value;
}

static bool enum_value_fits_c_int(int64_t value) {
    unsigned int_bits = type_int && type_int->size > 0
        ? (unsigned)type_int->size * 8u : 32u;
    int64_t minimum = -(INT64_C(1) << (int_bits - 1u));
    int64_t maximum = (INT64_C(1) << (int_bits - 1u)) - 1;
    return value >= minimum && value <= maximum;
}

static void parser_complete_enum_constant_types(Type* enum_type) {
    for (ParserEnumConstant* item = parser_enum_constants; item;
         item = item->next) {
        if (item->type == enum_type) item->expression_type = enum_type;
    }
}

static void parse_enum_body(Type* enum_type, bool scoped,
                            const char* enum_tag) {
    int64_t next_value = 0;
    Type* next_value_type = type_int;
    bool next_value_is_unsigned = false;
    bool next_value_valid = true;
    bool next_value_exceeds_underlying = false;
    while (!check(TOK_RBRACE) && !at_end()) {
        Token* name = expect(TOK_IDENT, "enumerator name");
        bool is_maybe_unused = false;
        bool is_deprecated = false;
        const char* deprecated_message = NULL;
        SourceLoc attribute_loc = name ? name->loc : peek()->loc;
        if (!parser_cxx_mode) {
            skip_attributes();
            parser_apply_pending_c_function_type_attributes(NULL,
                                                            attribute_loc);
            parser_accumulate_c_maybe_unused_attribute(&is_maybe_unused);
            is_deprecated = take_c_deprecated_attribute(
                &deprecated_message);
        }
        int64_t value;
        bool value_is_unsigned = false;
        Type* value_type_before_close = NULL;
        bool explicit_value = match(TOK_ASSIGN);
        if (explicit_value) {
            Type* initializer_type = NULL;
            value = parse_enum_value(enum_type,
                                     next_value_valid ? next_value : 0,
                                     &value_is_unsigned, &initializer_type);
            value_type_before_close =
                parser_cxx_mode && enum_type->enum_has_fixed_underlying
                    ? enum_type->enum_underlying_type
                    : (initializer_type ? initializer_type : type_int);
            next_value_valid = true;
            next_value_exceeds_underlying = false;
        } else if (!next_value_valid) {
            rcc_error(name ? name->loc : peek()->loc,
                      next_value_exceeds_underlying
                          ? "implicit enumerator value is not representable in its fixed underlying type"
                          : "implicit enumerator value exceeds the supported 64-bit range");
            value = 0;
            value_type_before_close = next_value_type;
        } else {
            value = next_value;
            value_is_unsigned = next_value_is_unsigned;
            value_type_before_close =
                parser_cxx_mode && enum_type->enum_has_fixed_underlying
                    ? enum_type->enum_underlying_type : next_value_type;
            if (!parser_cxx_mode && !enum_value_fits_c_int(value)) {
                rcc_error(name ? name->loc : peek()->loc,
                          "enumerator value is not representable as int");
            } else if (parser_cxx_mode &&
                       !enum_value_fits_fixed_underlying(
                           value, value_is_unsigned, enum_type)) {
                rcc_error(name ? name->loc : peek()->loc,
                          "implicit enumerator value is not representable in its fixed underlying type");
            }
        }
        if (name) {
            char qualified[512];
            const char* spelling = name->value.str_val;
            if (scoped && enum_tag) {
                int written = snprintf(qualified, sizeof(qualified),
                                       "%s::%s", enum_tag, spelling);
                if (written < 0 || (size_t)written >= sizeof(qualified)) {
                    rcc_error(name->loc,
                              "scoped enum constant name exceeds compiler limit");
                } else {
                    spelling = rcc_intern(qualified);
                }
            }
            parser_define_enum_constant(
                spelling, value, enum_type,
                parser_cxx_mode ? value_type_before_close : type_int,
                is_maybe_unused,
                is_deprecated, deprecated_message);
            enum_type->enum_constants = ast_arena_grow(
                enum_type->enum_constants,
                sizeof(*enum_type->enum_constants) *
                    (size_t)enum_type->enum_constant_count,
                sizeof(*enum_type->enum_constants) *
                    (size_t)(enum_type->enum_constant_count + 1));
            enum_type->enum_constants[
                enum_type->enum_constant_count].name = name->value.str_val;
            enum_type->enum_constants[
                enum_type->enum_constant_count].value = value;
            enum_type->enum_constants[
                enum_type->enum_constant_count].is_unsigned =
                    value_is_unsigned;
            enum_type->enum_constants[
                enum_type->enum_constant_count].c23_is_maybe_unused =
                    is_maybe_unused;
            enum_type->enum_constants[
                enum_type->enum_constant_count].is_deprecated =
                    is_deprecated;
            enum_type->enum_constants[
                enum_type->enum_constant_count].deprecated_message =
                    deprecated_message;
            ++enum_type->enum_constant_count;
        }
        if (parser_cxx_mode && enum_type->enum_has_fixed_underlying &&
            enum_type->enum_underlying_type &&
            enum_type->enum_underlying_type->is_unsigned) {
            unsigned width = type_integer_width(
                enum_type->enum_underlying_type);
            uint64_t maximum = width >= 64u
                ? UINT64_MAX : (UINT64_C(1) << width) - 1u;
            uint64_t current = (uint64_t)value;
            if (!value_is_unsigned && value < 0) {
                next_value_valid = false;
                next_value_exceeds_underlying = true;
                next_value_is_unsigned = true;
            } else if (current == maximum) {
                next_value_valid = false;
                next_value_exceeds_underlying = true;
                next_value_is_unsigned = true;
            } else {
                next_value = (int64_t)(current + 1u);
                next_value_is_unsigned = true;
                next_value_valid = enum_value_fits_fixed_underlying(
                    next_value, true, enum_type);
                next_value_exceeds_underlying = !next_value_valid;
            }
            next_value_type = enum_type->enum_underlying_type;
        } else if (parser_cxx_mode &&
                   enum_type->enum_has_fixed_underlying) {
            if (value == INT64_MAX) {
                next_value_valid = false;
                next_value_exceeds_underlying = false;
            } else {
                next_value = value + 1;
                next_value_is_unsigned = false;
                next_value_valid = enum_value_fits_fixed_underlying(
                    next_value, false, enum_type);
                next_value_exceeds_underlying = !next_value_valid;
            }
            next_value_type = enum_type->enum_underlying_type;
        } else if (parser_cxx_mode) {
            Type* preceding_type = value_type_before_close;
            if (preceding_type && preceding_type->kind == TYPE_ENUM &&
                preceding_type->enum_underlying_type) {
                preceding_type = preceding_type->enum_underlying_type;
            }
            next_value_valid = enum_next_unfixed_value(
                value, preceding_type, &next_value,
                &next_value_is_unsigned, &next_value_type);
            next_value_exceeds_underlying = false;
        } else if (value == INT64_MAX) {
            next_value_valid = false;
            next_value_exceeds_underlying = false;
        } else {
            next_value = value + 1;
            next_value_is_unsigned = false;
            next_value_type = type_int;
            next_value_valid = true;
            next_value_exceeds_underlying = false;
        }
        if (!match(TOK_COMMA)) break;
    }
    if (parser_cxx_mode && !enum_type->enum_has_fixed_underlying) {
        SourceLoc close_loc = peek()->loc;
        enum_select_underlying_type(enum_type, close_loc);
    }
    expect(TOK_RBRACE, "}");
    parser_complete_enum_constant_types(enum_type);
}

static TypeField* parser_append_field(Type* aggregate, const char* name,
                                      Type* type) {
    TypeField* field = ast_arena_alloc(sizeof(*field));
    TypeField** tail = &aggregate->fields;
    int alignment = type && type->align > 0 ? type->align : 1;
    int size = type && type->size > 0 ? type->size : 0;
    {
        int pack_alignment = parser_effective_pack_alignment();
        if (pack_alignment > 0 && alignment > pack_alignment) {
            alignment = pack_alignment;
        }
    }
    field->name = name;
    field->type = type;
    field->is_bitfield = false;
    field->bit_width = 0u;
    field->bit_offset = 0u;
    field->from_virtual_base = false;
    field->virtual_base_owner = NULL;
    field->virtual_base_member_offset = -1;
    field->cxx_access = 0u;
    field->is_deprecated = false;
    field->deprecated_message = NULL;
    field->c23_is_maybe_unused = false;
    field->next = NULL;
    while (*tail) tail = &(*tail)->next;
    if (aggregate->kind == TYPE_UNION) {
        field->offset = 0;
        if (size > aggregate->size) aggregate->size = size;
    } else {
        field->offset = parser_align_up(aggregate->size, alignment);
        aggregate->size = field->offset + size;
    }
    if (alignment > aggregate->align) aggregate->align = alignment;
    *tail = field;
    return field;
}

typedef struct ParserBitfieldLayout {
    bool active;
    int offset;
    int size;
    int alignment;
    unsigned used;
} ParserBitfieldLayout;

static void parser_reset_bitfield_layout(ParserBitfieldLayout* layout) {
    layout->active = false;
    layout->offset = 0;
    layout->size = 0;
    layout->alignment = 0;
    layout->used = 0u;
}

static TypeField* parser_append_bitfield(Type* aggregate,
                                         ParserBitfieldLayout* layout,
                                         const char* name,
                                         Type* type,
                                         int64_t width_value,
                                         SourceLoc loc) {
    TypeField* field;
    TypeField** tail;
    int alignment;
    int size;
    unsigned storage_bits;
    unsigned width;

    if (!type || (!type_is_integer(type) && type->kind != TYPE_ENUM)) {
        rcc_error(loc, "bit-field type must be an integer or enum type");
        parser_reset_bitfield_layout(layout);
        return NULL;
    }
    size = type->size;
    if (size <= 0 || size > 8) {
        rcc_error(loc,
                  "bit-field type width of %d bytes is not supported",
                  size);
        parser_reset_bitfield_layout(layout);
        return NULL;
    }
    storage_bits = (unsigned)size * 8u;
    if (width_value < 0 ||
        (uint64_t)width_value > type_integer_width(type)) {
        rcc_error(loc,
                  "bit-field width %lld exceeds its %u-bit declared type",
                  (long long)width_value, type_integer_width(type));
        parser_reset_bitfield_layout(layout);
        return NULL;
    }
    width = (unsigned)width_value;
    if (width == 0u) {
        if (name) {
            rcc_error(loc, "named bit-field cannot have zero width");
        }
        parser_reset_bitfield_layout(layout);
        if (aggregate->kind == TYPE_STRUCT) {
            alignment = type->align > 0 ? type->align : 1;
            {
                int pack_alignment = parser_effective_pack_alignment();
                if (pack_alignment > 0 && alignment > pack_alignment) {
                    alignment = pack_alignment;
                }
            }
            aggregate->size = parser_align_up(aggregate->size, alignment);
            if (alignment > aggregate->align) aggregate->align = alignment;
        }
        return NULL;
    }

    alignment = type->align > 0 ? type->align : 1;
    {
        int pack_alignment = parser_effective_pack_alignment();
        if (pack_alignment > 0 && alignment > pack_alignment) {
            alignment = pack_alignment;
        }
    }
    if (aggregate->kind == TYPE_UNION) {
        parser_reset_bitfield_layout(layout);
        layout->offset = 0;
        layout->size = size;
        layout->alignment = alignment;
        layout->used = width;
        if (size > aggregate->size) aggregate->size = size;
    } else if (!layout->active || layout->size != size ||
               layout->alignment != alignment || layout->used + width > storage_bits) {
        layout->active = true;
        layout->offset = parser_align_up(aggregate->size, alignment);
        layout->size = size;
        layout->alignment = alignment;
        layout->used = 0u;
        aggregate->size = layout->offset + size;
    }
    if (alignment > aggregate->align) aggregate->align = alignment;

    /* Unnamed non-zero fields still consume storage, but cannot be selected
     * by a member expression or by an initializer designator. */
    if (!name) {
        layout->used += width;
        return NULL;
    }
    field = ast_arena_alloc(sizeof(*field));
    field->name = name;
    field->type = type;
    field->offset = layout->offset;
    field->is_bitfield = true;
    field->bit_width = width;
    field->bit_offset = layout->used;
    field->from_virtual_base = false;
    field->virtual_base_owner = NULL;
    field->virtual_base_member_offset = -1;
    field->cxx_access = 0u;
    field->is_deprecated = false;
    field->deprecated_message = NULL;
    field->c23_is_maybe_unused = false;
    field->next = NULL;
    tail = &aggregate->fields;
    while (*tail) tail = &(*tail)->next;
    *tail = field;
    layout->used += width;
    return field;
}

/* C17 does not permit a variably modified type as a struct or union member.
 * This includes a pointer whose pointed-to type is a VLA; the member itself
 * remains a fixed-size pointer, but its declared type is still variably
 * modified and may not be stored in an aggregate definition. */
static bool parser_type_is_variably_modified(Type* type) {
    if (!type) return false;
    if (type->kind == TYPE_ARRAY) {
        return type->array_bound != NULL || type->array_unspecified_bound ||
               parser_type_is_variably_modified(type->base);
    }
    if (type->kind == TYPE_PTR) {
        return parser_type_is_variably_modified(type->base);
    }
    return false;
}

bool rcc_parser_import_enum_constants(Type* enum_type, SourceLoc loc) {
    if (!enum_type || enum_type->kind != TYPE_ENUM) {
        rcc_error(loc, "using enum requires a declared enumeration type");
        return false;
    }
    for (int index = 0; index < enum_type->enum_constant_count; ++index) {
        const EnumConstantInfo* item = &enum_type->enum_constants[index];
        const char* name = item->name;
        int64_t value = item->value;
        int64_t existing_value;
        Type* existing_type = NULL;
        if (parser_lookup_enum_constant(name, &existing_value,
                                        &existing_type, NULL, NULL, NULL)) {
            if (existing_type != enum_type || existing_value != value) {
                rcc_error(loc,
                          "using enum introduces a conflicting enumerator '%s'",
                          name);
            }
            continue;
        }
        parser_define_enum_constant(name, value, enum_type, enum_type,
                                    item->c23_is_maybe_unused,
                                    item->is_deprecated,
                                    item->deprecated_message);
    }
    return true;
}

void* rcc_parser_enum_scope_mark(void) {
    return parser_enum_constants;
}

void rcc_parser_enum_scope_restore(void* mark) {
    parser_enum_constants = (ParserEnumConstant*)mark;
}

static bool parser_is_flexible_array(Type* type) {
    return type && type->kind == TYPE_ARRAY && type->array_len == -1 &&
           !type->array_bound && !type->array_unspecified_bound;
}

static void parser_validate_flexible_array_members(Type* aggregate) {
    TypeField* field;
    TypeField* last_named = NULL;
    int named_count = 0;
    if (!aggregate || (aggregate->kind != TYPE_STRUCT &&
                       aggregate->kind != TYPE_UNION)) return;
    for (field = aggregate->fields; field; field = field->next) {
        if (field->name) {
            last_named = field;
            ++named_count;
        }
    }
    for (field = aggregate->fields; field; field = field->next) {
        Type* element;
        if (!parser_is_flexible_array(field->type)) continue;
        if (aggregate->kind == TYPE_UNION) {
            rcc_error(peek()->loc,
                      "flexible array member is not allowed in a union");
        } else if (named_count == 1) {
            rcc_error(peek()->loc,
                      "flexible array member requires another named member");
        }
        if (aggregate->kind == TYPE_STRUCT && field != last_named) {
            rcc_error(peek()->loc,
                      "flexible array member must be the last member of a struct");
        }
        element = field->type->base;
        if (!element || element->size <= 0 || !type_is_complete(element)) {
            rcc_error(peek()->loc,
                      "flexible array member has an incomplete element type");
        }
    }
}

static bool parser_type_has_array_parameter_spec(Type* type) {
    if (!type) return false;
    if (type->kind == TYPE_ARRAY) {
        return type->array_parameter_static ||
               type->array_unspecified_bound ||
               type->array_parameter_const ||
               type->array_parameter_volatile ||
               type->array_parameter_restrict ||
               parser_type_has_array_parameter_spec(type->base);
    }
    if (type->kind == TYPE_PTR) {
        return parser_type_has_array_parameter_spec(type->base);
    }
    return false;
}

static void parser_append_anonymous_fields(Type* aggregate, Type* anonymous) {
    TypeField** tail = &aggregate->fields;
    int alignment = anonymous && anonymous->align > 0 ? anonymous->align : 1;
    int size = anonymous && anonymous->size > 0 ? anonymous->size : 0;
    {
        int pack_alignment = parser_effective_pack_alignment();
        if (pack_alignment > 0 && alignment > pack_alignment) {
            alignment = pack_alignment;
        }
    }
    int base_offset = aggregate->kind == TYPE_UNION
        ? 0 : parser_align_up(aggregate->size, alignment);
    while (*tail) tail = &(*tail)->next;
    for (TypeField* source = anonymous ? anonymous->fields : NULL;
         source; source = source->next) {
        TypeField* field = ast_arena_alloc(sizeof(*field));
        TypeField* existing;
        for (existing = aggregate->fields; existing;
             existing = existing->next) {
            if (existing->name && source->name &&
                strcmp(existing->name, source->name) == 0) {
                rcc_error(peek()->loc,
                          "duplicate member '%s' from anonymous aggregate",
                          source->name);
                break;
            }
        }
        field->name = source->name;
        field->type = source->type;
        field->offset = base_offset + source->offset;
        field->is_bitfield = source->is_bitfield;
        field->bit_width = source->bit_width;
        field->bit_offset = source->bit_offset;
        field->from_virtual_base = source->from_virtual_base;
        field->virtual_base_owner = source->virtual_base_owner;
        field->virtual_base_member_offset =
            source->virtual_base_member_offset;
        field->initializer = source->initializer;
        field->cxx_access = source->cxx_access;
        field->cxx_declaring_class = source->cxx_declaring_class;
        field->is_deprecated = source->is_deprecated;
        field->deprecated_message = source->deprecated_message;
        field->c23_is_maybe_unused = source->c23_is_maybe_unused;
        field->next = NULL;
        *tail = field;
        tail = &field->next;
    }
    if (aggregate->kind == TYPE_UNION) {
        if (size > aggregate->size) aggregate->size = size;
    } else {
        aggregate->size = base_offset + size;
    }
    if (alignment > aggregate->align) aggregate->align = alignment;
}

typedef struct ParserAggregateAttributes {
    bool packed;
    int alignment;
    bool maybe_unused;
    bool nodiscard;
    const char* nodiscard_message;
    SourceLoc nodiscard_loc;
    bool deprecated;
    const char* deprecated_message;
    SourceLoc deprecated_loc;
} ParserAggregateAttributes;

static void parser_parse_c_type_attributes(
    ParserAggregateAttributes* attributes) {
    if (!attributes || parser_cxx_mode) return;
    while (check(TOK_LBRACKET) && check_next(TOK_LBRACKET)) {
        SourceLoc loc = peek()->loc;
        const char* message = NULL;
        parser_skip_c_attribute_sequence();
        if (take_c_noreturn_attribute()) {
            rcc_error(loc,
                      "C23 [[noreturn]] requires a function declaration");
        }
        parser_accumulate_c_maybe_unused_attribute(
            &attributes->maybe_unused);
        if (take_c_nodiscard_attribute(&message)) {
            attributes->nodiscard = true;
            if (!attributes->nodiscard_message) {
                attributes->nodiscard_message = message;
            }
            attributes->nodiscard_loc = loc;
        }
        if (take_c_deprecated_attribute(&message)) {
            attributes->deprecated = true;
            if (!attributes->deprecated_message) {
                attributes->deprecated_message = message;
            }
            attributes->deprecated_loc = loc;
        }
    }
}

static void parser_skip_attribute_arguments(void) {
    int depth = 1;
    while (!at_end() && depth > 0) {
        if (match(TOK_LPAREN)) {
            ++depth;
        } else if (match(TOK_RPAREN)) {
            --depth;
        } else {
            advance();
        }
    }
}

/* Parse GNU attributes attached to a struct/union specifier.  Attributes on
 * the tag affect member layout, while an aligned attribute also changes the
 * completed type's alignment and rounded size.  Unrelated attributes remain
 * intentionally ignorable, as they are in skip_attributes(). */
static void parser_parse_aggregate_attributes(
    ParserAggregateAttributes* attributes) {
    parser_parse_c_type_attributes(attributes);
    while (match(TOK___ATTRIBUTE__)) {
        SourceLoc attribute_loc = previous()->loc;
        if (!match(TOK_LPAREN)) continue;
        if (!match(TOK_LPAREN)) {
            parser_skip_attribute_arguments();
            continue;
        }
        while (!check(TOK_RPAREN) && !at_end()) {
            Token* name;
            bool is_packed;
            bool is_aligned;
            if (match(TOK_COMMA)) continue;
            if (!check(TOK_IDENT)) {
                advance();
                continue;
            }
            name = advance();
            is_packed = strcmp(name->value.str_val, "packed") == 0 ||
                        strcmp(name->value.str_val, "__packed__") == 0;
            is_aligned = strcmp(name->value.str_val, "aligned") == 0 ||
                         strcmp(name->value.str_val, "__aligned__") == 0;
            if (match(TOK_LPAREN)) {
                if (is_aligned) {
                    Expr* alignment_expression = NULL;
                    int64_t alignment_value = 0;
                    if (!check(TOK_RPAREN)) {
                        alignment_expression = parse_assignment();
                    }
                    if (alignment_expression &&
                        (!eval_integer_constant(alignment_expression,
                                                &alignment_value) ||
                         alignment_value <= 0 ||
                         alignment_value >
                             (int64_t)RCC_MAX_SUPPORTED_ALIGNMENT ||
                         (alignment_value & (alignment_value - 1)) != 0)) {
                        rcc_error(attribute_loc,
                                  "aligned attribute requires a positive power-of-two constant no greater than 16384");
                    } else if (alignment_expression &&
                               alignment_value > attributes->alignment) {
                        attributes->alignment = (int)alignment_value;
                    }
                    expect(TOK_RPAREN, "')' after aligned attribute argument");
                } else {
                    parser_skip_attribute_arguments();
                }
            } else if (is_aligned && attributes->alignment < 16) {
                /* GCC's argument-less aligned attribute requests the target's
                 * maximum natural alignment. RinOS x86 ABIs cap it at 16. */
                attributes->alignment = 16;
            }
            if (is_packed) attributes->packed = true;
        }
        expect(TOK_RPAREN, "')' after attribute list");
        expect(TOK_RPAREN, "')' after __attribute__");
    }
    parser_parse_c_type_attributes(attributes);
}

static void parser_relayout_packed_aggregate(Type* aggregate) {
    ParserBitfieldLayout bitfield_layout;
    if (!aggregate || (aggregate->kind != TYPE_STRUCT &&
                       aggregate->kind != TYPE_UNION) ||
        !aggregate->is_complete) {
        return;
    }
    aggregate->size = 0;
    aggregate->align = 1;
    parser_reset_bitfield_layout(&bitfield_layout);
    for (TypeField* field = aggregate->fields; field; field = field->next) {
        int size = field->type && field->type->size > 0
            ? field->type->size : 0;
        if (field->is_bitfield) {
            unsigned storage_bits = (unsigned)size * 8u;
            if (aggregate->kind == TYPE_UNION) {
                parser_reset_bitfield_layout(&bitfield_layout);
                bitfield_layout.offset = 0;
                bitfield_layout.size = size;
                bitfield_layout.alignment = 1;
                bitfield_layout.used = field->bit_width;
                field->offset = 0;
                field->bit_offset = 0u;
                if (size > aggregate->size) aggregate->size = size;
            } else {
                if (!bitfield_layout.active ||
                    bitfield_layout.size != size ||
                    bitfield_layout.used + field->bit_width > storage_bits) {
                    bitfield_layout.active = true;
                    bitfield_layout.offset = aggregate->size;
                    bitfield_layout.size = size;
                    bitfield_layout.alignment = 1;
                    bitfield_layout.used = 0u;
                    aggregate->size += size;
                }
                field->offset = bitfield_layout.offset;
                field->bit_offset = bitfield_layout.used;
                bitfield_layout.used += field->bit_width;
            }
            continue;
        }

        parser_reset_bitfield_layout(&bitfield_layout);
        if (aggregate->kind == TYPE_UNION) {
            field->offset = 0;
            if (size > aggregate->size) aggregate->size = size;
        } else {
            field->offset = aggregate->size;
            aggregate->size += size;
        }
    }
}

static void parser_apply_aggregate_attributes(
    Type* aggregate, const ParserAggregateAttributes* attributes,
    SourceLoc attribute_loc, bool is_definition) {
    if (attributes->packed) parser_relayout_packed_aggregate(aggregate);
    if (attributes->alignment > 0 && aggregate) {
        if (attributes->alignment > aggregate->align) {
            aggregate->align = attributes->alignment;
        }
        aggregate->has_explicit_alignment = true;
        if (aggregate->is_complete) {
            aggregate->size = parser_align_up(aggregate->size,
                                              aggregate->align);
        }
    }
    if (attributes->nodiscard) {
        if (!is_definition || !aggregate ||
            (aggregate->kind != TYPE_STRUCT &&
             aggregate->kind != TYPE_UNION &&
             aggregate->kind != TYPE_ENUM)) {
            rcc_error(attributes->nodiscard_loc.filename
                          ? attributes->nodiscard_loc : attribute_loc,
                      "C23 [[nodiscard]] requires a structure, union, or enum definition");
        } else {
            aggregate->is_nodiscard = true;
            if (!aggregate->nodiscard_message) {
                aggregate->nodiscard_message =
                    attributes->nodiscard_message;
            }
        }
    }
    if (attributes->maybe_unused && aggregate) {
        aggregate->c23_tag_is_maybe_unused = true;
    }
    if (attributes->deprecated && aggregate) {
        aggregate->c23_tag_is_deprecated = true;
        if (!aggregate->c23_tag_deprecated_message) {
            aggregate->c23_tag_deprecated_message =
                attributes->deprecated_message;
        }
    }
    (void)attribute_loc;
}

static void parser_warn_deprecated_tag(Type* type, Token* tag) {
    const char* kind;
    if (!type || !type->c23_tag_is_deprecated || !tag ||
        parser_cxx_mode) {
        return;
    }
    kind = type->kind == TYPE_ENUM ? "enum" :
           type->kind == TYPE_UNION ? "union" : "struct";
    if (type->c23_tag_deprecated_message) {
        rcc_warning(tag->loc, "use of deprecated %s '%s': %s", kind,
                    tag->value.str_val,
                    type->c23_tag_deprecated_message);
    } else {
        rcc_warning(tag->loc, "use of deprecated %s '%s'", kind,
                    tag->value.str_val);
    }
}

static void parse_aggregate_body(Type* aggregate) {
    ParserBitfieldLayout bitfield_layout;
    bool union_has_default_member_initializer = false;
    aggregate->size = 0;
    aggregate->align = 1;
    aggregate->fields = NULL;
    parser_reset_bitfield_layout(&bitfield_layout);
    while (!check(TOK_RBRACE) && !at_end()) {
        Type* field_base;
        bool declaration_is_maybe_unused = false;
        bool declaration_is_deprecated = false;
        bool declaration_type_is_reproducible = false;
        bool declaration_type_is_unsequenced = false;
        SourceLoc declaration_type_attribute_loc = peek()->loc;
        const char* declaration_deprecated_message = NULL;
        int explicit_alignment = 0;
        SourceLoc alignment_loc = peek()->loc;
        if (match(TOK_PRAGMA_PACK)) {
            parser_apply_pack(previous());
            continue;
        }
        while (check(TOK__ALIGNAS)) {
            int alignment = parse_explicit_alignment();
            if (alignment > explicit_alignment) {
                explicit_alignment = alignment;
            }
        }
        skip_attributes();
        parser_accumulate_c_function_type_attributes(
            &declaration_type_is_reproducible,
            &declaration_type_is_unsequenced,
            &declaration_type_attribute_loc);
        parser_accumulate_c_maybe_unused_attribute(
            &declaration_is_maybe_unused);
        if (take_weak_attribute()) {
            rcc_error(peek()->loc,
                      "weak attribute requires a file-scope declaration");
        }
        if (take_c_noreturn_attribute()) {
            rcc_error(peek()->loc,
                      "C23 [[noreturn]] requires a function declaration");
        }
        if (take_c_nodiscard_attribute(NULL)) {
            rcc_error(peek()->loc,
                      "C23 [[nodiscard]] requires a function or type definition");
        }
        field_base = parse_type_spec();
        if (!field_base) {
            rcc_error(peek()->loc, "expected field type specifier");
            if (check(TOK_IDENT)) advance();
            field_base = type_int;
        }
        skip_attributes();
        parser_accumulate_c_function_type_attributes(
            &declaration_type_is_reproducible,
            &declaration_type_is_unsequenced,
            &declaration_type_attribute_loc);
        parser_accumulate_c_maybe_unused_attribute(
            &declaration_is_maybe_unused);
        parser_accumulate_c_deprecated_attribute(
            &declaration_is_deprecated, &declaration_deprecated_message);
        parser_apply_c_function_type_attributes(
            field_base, declaration_type_is_reproducible,
            declaration_type_is_unsequenced,
            declaration_type_attribute_loc);
        do {
            const char* field_name = NULL;
            bool field_is_maybe_unused = declaration_is_maybe_unused;
            bool field_is_deprecated = declaration_is_deprecated;
            const char* field_deprecated_message =
                declaration_deprecated_message;
            Type* field_type = parse_declarator(field_base, &field_name, NULL);
            Expr* field_initializer = NULL;
            skip_attributes();
            parser_apply_pending_c_function_type_attributes(
                field_type, field_name ? previous()->loc : peek()->loc);
            parser_accumulate_c_maybe_unused_attribute(
                &field_is_maybe_unused);
            parser_accumulate_c_deprecated_attribute(
                &field_is_deprecated, &field_deprecated_message);
            if (explicit_alignment != 0) {
                field_type = rcc_parser_apply_explicit_alignment(
                    field_type, explicit_alignment, alignment_loc);
            }
            if (parser_type_has_array_parameter_spec(field_type)) {
                rcc_error(previous()->loc,
                          "array parameter qualifiers are only valid in function parameter declarations");
            }
            if (parser_type_is_variably_modified(field_type)) {
                rcc_error(previous()->loc,
                          "variably modified type is not allowed for struct/union member");
            }
            if (match(TOK_COLON)) {
                Expr* width_expression = parse_assignment();
                int64_t width_value = 0;
                if (!eval_integer_constant(width_expression, &width_value)) {
                    rcc_error(width_expression ? width_expression->loc : previous()->loc,
                              "bit-field width must be an integer constant");
                }
                TypeField* field = parser_append_bitfield(
                    aggregate, &bitfield_layout, field_name, field_type,
                    width_value, width_expression ? width_expression->loc
                                                  : previous()->loc);
                if (field) {
                    field->c23_is_maybe_unused = field_is_maybe_unused;
                    field->is_deprecated = field_is_deprecated;
                    field->deprecated_message = field_deprecated_message;
                }
                continue;
            }
            if (!field_name) {
                if ((field_type->kind == TYPE_STRUCT ||
                     field_type->kind == TYPE_UNION) &&
                    field_type->is_complete && check(TOK_SEMICOLON)) {
                    parser_append_anonymous_fields(aggregate, field_type);
                } else {
                    rcc_error(peek()->loc, "expected field name");
                }
                break;
            }
            parser_reset_bitfield_layout(&bitfield_layout);
            if (parser_cxx_mode && aggregate->kind == TYPE_UNION) {
                if (match(TOK_ASSIGN) || check(TOK_LBRACE)) {
                    field_initializer = rcc_parser_parse_initializer();
                    if (union_has_default_member_initializer) {
                        rcc_error(field_initializer
                                      ? field_initializer->loc
                                      : peek()->loc,
                                  "a union may have only one default member initializer");
                    }
                    union_has_default_member_initializer = true;
                }
            }
            TypeField* field = parser_append_field(
                aggregate, field_name, field_type);
            field->initializer = field_initializer;
            field->c23_is_maybe_unused = field_is_maybe_unused;
            field->is_deprecated = field_is_deprecated;
            field->deprecated_message = field_deprecated_message;
        } while (match(TOK_COMMA));
        expect(TOK_SEMICOLON, ";");
    }
    expect(TOK_RBRACE, "}");
    parser_validate_flexible_array_members(aggregate);
    aggregate->size = parser_align_up(aggregate->size, aggregate->align);
    aggregate->is_complete = true;
}

static Type* parser_qualify_type(Type* type, bool is_const,
                                 bool is_volatile, bool is_atomic,
                                 bool is_restrict) {
    Type* qualified;
    if (!type || (!is_const && !is_volatile && !is_atomic &&
                  !is_restrict)) return type;
    if (type->kind == TYPE_ARRAY) {
        /* C17 6.7.3p9 applies array type qualifiers to the element type,
         * rather than making a separately qualified array type. This also
         * matters when the array type came from a typedef: `const A a` must
         * make `a[i]` const even though the qualifier was parsed after the
         * array declarator had already been formed. */
        qualified = ast_arena_alloc(sizeof(*qualified));
        *qualified = *type;
        qualified->base = parser_qualify_type(
            type->base, type->is_const || is_const,
            type->is_volatile || is_volatile,
            type->is_atomic || is_atomic,
            type->is_restrict || is_restrict);
        qualified->is_const = false;
        qualified->is_volatile = false;
        qualified->is_atomic = false;
        qualified->is_restrict = false;
        return qualified;
    }
    qualified = ast_arena_alloc(sizeof(*qualified));
    *qualified = *type;
    qualified->is_const = qualified->is_const || is_const;
    qualified->is_volatile = qualified->is_volatile || is_volatile;
    qualified->is_atomic = qualified->is_atomic || is_atomic;
    qualified->is_restrict = qualified->is_restrict || is_restrict;
    return qualified;
}

static Type* parser_unqual_type(Type* type) {
    Type* unqualified;
    if (!type) return NULL;

    if (type->kind == TYPE_ARRAY) {
        Type* element = parser_unqual_type(type->base);
        if (element == type->base && !type->is_const &&
            !type->is_volatile && !type->is_atomic &&
            !type->is_restrict) {
            return type;
        }
        unqualified = ast_arena_alloc(sizeof(*unqualified));
        *unqualified = *type;
        unqualified->base = element;
        unqualified->is_const = false;
        unqualified->is_volatile = false;
        unqualified->is_atomic = false;
        unqualified->is_restrict = false;
        return unqualified;
    }

    if (!type->is_const && !type->is_volatile && !type->is_atomic &&
        !type->is_restrict) {
        return type;
    }
    unqualified = ast_arena_alloc(sizeof(*unqualified));
    *unqualified = *type;
    unqualified->is_const = false;
    unqualified->is_volatile = false;
    unqualified->is_atomic = false;
    unqualified->is_restrict = false;
    return unqualified;
}

static Type* parser_atomic_type(Type* type, SourceLoc loc) {
    Type* atomic;
    if (!type || !type_is_scalar(type) || type->kind == TYPE_NULLPTR ||
        type->is_atomic || type->is_const || type->is_volatile) {
        rcc_error(loc,
                  "_Atomic requires an unqualified scalar object type");
        return type;
    }
    atomic = ast_arena_alloc(sizeof(*atomic));
    *atomic = *type;
    atomic->is_atomic = true;
    return atomic;
}

static Type* parser_parse_aggregate_type_spec(TypeKind kind) {
    ParserAggregateAttributes attributes = { 0 };
    Token* tag = NULL;
    Type* aggregate = NULL;
    bool is_definition = false;
    int saved_aggregate_pack_alignment = parser_aggregate_pack_alignment;
    SourceLoc attribute_loc = peek()->loc;

    parser_parse_aggregate_attributes(&attributes);
    if (check(TOK_IDENT)) tag = advance();
    parser_parse_aggregate_attributes(&attributes);

    if ((kind == TYPE_STRUCT || kind == TYPE_UNION) &&
        parser_cxx_mode && tag &&
        rcc_parser_cxx_find_class_type) {
        aggregate = rcc_parser_cxx_find_class_type(tag->value.str_val);
        if (aggregate && aggregate->kind != kind) {
            aggregate = NULL;
        }
    }
    if (!aggregate) {
        aggregate = parser_tag_type(kind,
                                    tag ? tag->value.str_val : NULL);
    }
    parser_warn_deprecated_tag(aggregate, tag);

    if (check(TOK_LBRACE)) {
        is_definition = true;
        parser_aggregate_pack_alignment = attributes.packed ? 1 : 0;
        advance();
        parse_aggregate_body(aggregate);
        parser_aggregate_pack_alignment = saved_aggregate_pack_alignment;
        if (parser_cxx_mode && aggregate->cxx_class &&
            rcc_parser_cxx_complete_aggregate_type) {
            rcc_parser_cxx_complete_aggregate_type(aggregate);
        }
    }

    /* GNU permits a type attribute after the closing brace as well, e.g.
     * `struct T { ... } __attribute__((aligned(8))) T;`. */
    parser_parse_aggregate_attributes(&attributes);
    parser_apply_aggregate_attributes(
        aggregate, &attributes, attribute_loc, is_definition);
    return aggregate;
}

static Type* parse_type_spec(void) {
    Token* spec_start = parser.cur;
    Token* spec_prev = parser.prev;
    Type* t = NULL;
    bool is_unsigned = false;
    bool saw_sign = false;
    bool is_const = false;
    bool is_volatile = false;
    bool is_atomic = false;
    SourceLoc atomic_qualifier_loc = { 0 };
    bool is_restrict = false;
    bool saw_complex = false;
    bool saw_imaginary = false;
    int long_count = 0;
    bool is_short = false;

    while (1) {
        if (match(TOK_CONST)) {
            is_const = true;
        } else if (match(TOK_VOLATILE)) {
            is_volatile = true;
        } else if (match(TOK_RESTRICT)) {
            is_restrict = true;
        } else if (match(TOK_UNSIGNED)) {
            is_unsigned = true;
            saw_sign = true;
        } else if (match(TOK_SIGNED)) {
            is_unsigned = false;
            saw_sign = true;
        } else if (match(TOK_LONG)) {
            long_count++;
        } else if (match(TOK_SHORT)) {
            is_short = true;
        } else if (match(TOK__COMPLEX)) {
            saw_complex = true;
        } else if (match(TOK__IMAGINARY)) {
            rcc_error(previous()->loc,
                      "_Imaginary is not supported by the RinOS floating-point ABI");
            saw_imaginary = true;
        } else if (match(TOK__ATOMIC)) {
            SourceLoc atomic_loc = previous()->loc;
            if (match(TOK_LPAREN)) {
                Type* atomic_base;
                if (!is_type_start()) {
                    rcc_error(peek()->loc,
                              "_Atomic(type-name) requires a type name");
                    atomic_base = type_int;
                } else {
                    atomic_base = parse_type_spec();
                    atomic_base = parse_type_name_declarator(
                        atomic_base, atomic_loc);
                }
                expect(TOK_RPAREN, "')' after _Atomic type-name");
                t = parser_atomic_type(atomic_base, atomic_loc);
            } else {
                is_atomic = true;
                atomic_qualifier_loc = atomic_loc;
            }
        } else {
            break;
        }
    }

    if (!t && !parser_cxx_mode && g_opts.c_standard >= 23 &&
        (check(TOK_TYPEOF) || check(TOK_TYPEOF_UNQUAL))) {
        TokenType typeof_operator = advance()->type;
        Type* operand_type;

        expect(TOK_LPAREN, "'(' after typeof operator");
        if (is_type_start()) {
            operand_type = parse_type_spec();
            operand_type = parse_declarator(operand_type, NULL, NULL);
            expect(TOK_RPAREN, "')' after typeof type-name");
            parser_mark_typeof_shared_vla_bounds(operand_type);
            t = typeof_operator == TOK_TYPEOF_UNQUAL
                ? parser_unqual_type(operand_type) : operand_type;
        } else {
            Expr* expression_operand = parse_expression();
            Type* deferred = ast_arena_alloc(sizeof(*deferred));
            *deferred = *type_int;
            deferred->c_typeof_expression = expression_operand;
            deferred->c_typeof_resolution_cached = false;
            deferred->c_typeof_unqual_expression =
                typeof_operator == TOK_TYPEOF_UNQUAL;
            expect(TOK_RPAREN, "')' after typeof expression");
            t = deferred;
        }
    }

    if (t) {
        /* A parenthesized _Atomic(type-name) already supplied the complete
         * type specifier.  Do not reinterpret the following declarator
         * identifier as a typedef-name lookup. */
    } else if (saw_imaginary) {
        /* Consume the optional scalar component after the diagnostic so
         * parser recovery does not reinterpret it as a declarator. */
        if (match(TOK_FLOAT)) {
            t = type_float;
        } else if (match(TOK_DOUBLE)) {
            t = type_double;
        } else if (match(TOK_INT)) {
            t = type_int;
        } else {
            t = type_double;
        }
    } else if (long_count != 0 && match(TOK_DOUBLE)) {
        if (long_count > 1 || is_unsigned || saw_sign || is_short) {
            rcc_error(previous()->loc,
                      "invalid qualifiers on long double type");
        }
        t = type_long_double;
    } else if (match(TOK__BITINT)) {
        Expr* width_expression;
        int64_t width_value = 0;
        SourceLoc bitint_loc = previous()->loc;
        expect(TOK_LPAREN, "'(' after _BitInt");
        width_expression = parse_assignment();
        expect(TOK_RPAREN, "')' after _BitInt width");
        if (parser_cxx_mode || g_opts.c_standard < 23) {
            rcc_error(bitint_loc, "_BitInt requires C23 mode");
        }
        if (long_count != 0 || is_short) {
            rcc_error(bitint_loc,
                      "_BitInt cannot be combined with short or long");
        }
        if (!eval_integer_constant(width_expression, &width_value) ||
            width_value < 1 || width_value > 64 ||
            (!is_unsigned && width_value < 2)) {
            rcc_error(bitint_loc,
                      "_BitInt width must be between 1 and 64 (signed widths start at 2)");
            t = type_bitint(is_unsigned ? 1u : 2u, is_unsigned);
        } else {
            t = type_bitint((unsigned)width_value, is_unsigned);
        }
    } else if (match(TOK_VOID)) {
        t = type_void;
    } else if (parser_cxx_mode && match(TOK_BOOL)) {
        t = type_bool;
    } else if (parser_cxx_mode && match(TOK_WCHAR_T)) {
        if (is_unsigned || saw_sign || long_count != 0 || is_short) {
            rcc_error(previous()->loc,
                      "integer sign/width specifier is invalid on wchar_t");
        }
        t = type_wchar;
    } else if (parser_cxx_mode && match(TOK_CHAR16_T)) {
        if (is_unsigned || saw_sign || long_count != 0 || is_short) {
            rcc_error(previous()->loc,
                      "integer sign/width specifier is invalid on char16_t");
        }
        t = type_char16;
    } else if (parser_cxx_mode && match(TOK_CHAR32_T)) {
        if (is_unsigned || saw_sign || long_count != 0 || is_short) {
            rcc_error(previous()->loc,
                      "integer sign/width specifier is invalid on char32_t");
        }
        t = type_char32;
    } else if (parser_cxx_mode && match(TOK_CHAR8_T)) {
        if (is_unsigned || saw_sign || long_count != 0 || is_short) {
            rcc_error(previous()->loc,
                      "integer sign/width specifier is invalid on char8_t");
        }
        t = type_char8;
    } else if (match(TOK_CHAR)) {
        t = is_unsigned ? type_uchar : saw_sign ? type_schar : type_char;
    } else if (match(TOK_INT) || long_count > 0 || is_short) {
        if (is_short) {
            t = is_unsigned ? type_ushort : type_short;
        } else if (long_count > 1) {
            t = is_unsigned ? type_ullong : type_llong;
        } else if (long_count == 1) {
            t = is_unsigned ? type_ulong : type_long;
        } else {
            t = is_unsigned ? type_uint : type_int;
        }
    } else if (match(TOK_FLOAT)) {
        if (is_unsigned || saw_sign || long_count != 0 || is_short) {
            rcc_error(previous()->loc,
                      "invalid integer qualifier on float type");
        }
        t = type_float;
    } else if (match(TOK_DOUBLE)) {
        if (is_unsigned || saw_sign || is_short) {
            rcc_error(previous()->loc,
                      "invalid integer qualifier on double type");
        }
        if (long_count > 1) {
            rcc_error(previous()->loc,
                      "long double cannot contain repeated long specifiers");
        }
        t = long_count ? type_long_double : type_double;
    } else if (match(TOK__BOOL)) {
        t = type_bool;
    } else if (match(TOK___BUILTIN_VA_LIST)) {
        t = parser_builtin_va_list_type;
    } else if (match(TOK_STRUCT)) {
        t = parser_parse_aggregate_type_spec(TYPE_STRUCT);
    } else if (match(TOK_UNION)) {
        t = parser_parse_aggregate_type_spec(TYPE_UNION);
    } else if (match(TOK_ENUM)) {
        ParserAggregateAttributes attributes = { 0 };
        bool scoped = false;
        bool has_underlying_type = false;
        bool is_definition = false;
        Token* tag = NULL;
        Type* underlying = NULL;
        SourceLoc enum_loc = previous()->loc;
        if (parser_cxx_mode && (match(TOK_CLASS) || match(TOK_STRUCT))) {
            scoped = true;
        }
        parser_parse_c_type_attributes(&attributes);
        if (check(TOK_IDENT)) {
            tag = advance();
        }
        parser_parse_c_type_attributes(&attributes);
        if (parser_cxx_mode && match(TOK_COLON)) {
            has_underlying_type = true;
            underlying = parse_type_spec();
        }
        t = parser_tag_type(TYPE_ENUM, tag ? tag->value.str_val : NULL);
        t->enum_is_scoped = scoped;
        if (has_underlying_type) {
            if (!underlying || !type_is_integer(underlying)) {
                rcc_error(previous()->loc,
                          "C++ enum underlying type must be an integer type");
            } else {
                if (t->enum_has_fixed_underlying &&
                    t->enum_underlying_type != underlying) {
                    rcc_error(tag ? tag->loc : previous()->loc,
                              "C++ enum underlying type does not match its previous declaration");
                }
                t->enum_has_fixed_underlying = true;
                t->enum_underlying_type = underlying;
                t->size = underlying->size;
                t->align = underlying->align;
                t->is_unsigned = underlying->is_unsigned;
            }
        } else if (parser_cxx_mode && scoped) {
            if (t->enum_has_fixed_underlying &&
                t->enum_underlying_type != type_int) {
                rcc_error(tag ? tag->loc : previous()->loc,
                          "C++ enum underlying type does not match its previous declaration");
            }
            t->enum_has_fixed_underlying = true;
            t->enum_underlying_type = type_int;
            t->size = type_int->size;
            t->align = type_int->align;
            t->is_unsigned = type_int->is_unsigned;
        }
        if (match(TOK_LBRACE)) {
            is_definition = true;
            parse_enum_body(t, scoped, tag ? tag->value.str_val : NULL);
        }
        parser_parse_c_type_attributes(&attributes);
        parser_warn_deprecated_tag(t, tag);
        parser_apply_aggregate_attributes(
            t, &attributes, enum_loc, is_definition);
        if (parser_cxx_mode && tag) {
            parser_define_type(tag->value.str_val, t);
        }
    } else if (parser_cxx_mode && rcc_parse_cxx_type_name &&
               (check(TOK_DECLTYPE) ||
                (rcc_parse_cxx_type_start && rcc_parse_cxx_type_start()))) {
        /* The C++ type parser owns cv-qualifiers and reference/pointer
         * operators as one type-id.  Re-entering it after this common parser
         * consumed a leading `const`/`volatile` incorrectly qualified the
         * outer reference or pointer instead of its referred-to object. */
        if (!is_atomic && !is_restrict && !saw_sign &&
            long_count == 0 && !is_short) {
            parser.cur = spec_start;
            parser.prev = spec_prev;
            return rcc_parse_cxx_type_name();
        }
        t = rcc_parse_cxx_type_name();
    } else if (check(TOK_IDENT)) {
        const char* name = peek()->value.str_val;
        t = parser_lookup_type(name);
        if (t) {
            parser_warn_deprecated_typedef(name, peek()->loc);
            advance();
        } else if (parser_cxx_mode && saw_sign) {
            /* C++ accepts the C-compatible shorthand `unsigned value` and
             * `signed value`.  The identifier is the declarator, not a
             * missing type-name; leave it for parse_declarator and let the
             * final qualifier fallback select int/unsigned int. */
        } else if (parser_cxx_mode) {
            rcc_error(peek()->loc, "unknown C++ type name '%s'", name);
            advance();
            t = type_int;
        }
    } else {
        /* Default to int */
        t = is_unsigned ? type_uint : type_int;
    }

    /* C permits the complex qualifier before or after its floating component. */
    while (match(TOK__COMPLEX) || match(TOK__IMAGINARY)) {
        if (previous()->type == TOK__COMPLEX) {
            if (saw_complex) {
                rcc_error(previous()->loc,
                          "duplicate _Complex type specifier");
            }
            saw_complex = true;
        } else {
            rcc_error(previous()->loc,
                      "_Imaginary is not supported by the RinOS floating-point ABI");
            saw_imaginary = true;
        }
    }

    /* C17 permits signed and unsigned without an explicit int.  When the
     * next identifier is the declarator rather than a typedef name, the
     * typedef lookup branch above intentionally leaves t unset. */
    if (!t && saw_sign) {
        t = is_unsigned ? type_uint : type_int;
    }

    if (saw_complex) {
        if (!type_is_floating(t)) {
            rcc_error(spec_start->loc,
                      "_Complex requires float, double, or long double");
            t = type_complex(type_double);
        } else {
            t = type_complex(t);
        }
    }

    /* Declaration specifiers permit qualifiers on either side of the type
     * specifier (for example, both const int and int const). */
    while (match(TOK_CONST) || match(TOK_VOLATILE) ||
           match(TOK_RESTRICT)) {
        if (previous()->type == TOK_CONST) is_const = true;
        else if (previous()->type == TOK_VOLATILE) is_volatile = true;
        else is_restrict = true;
    }
    if (is_atomic && t &&
        (t->kind == TYPE_ARRAY || t->kind == TYPE_FUNC)) {
        rcc_error(atomic_qualifier_loc,
                  "_Atomic qualifier cannot modify an array or function type");
        is_atomic = false;
    }
    t = parser_qualify_type(t, is_const, is_volatile, is_atomic,
                            is_restrict);

    return t;
}

static TypeParam* parser_type_params(DeclList* parameters, bool* variadic) {
    TypeParam* head = NULL;
    TypeParam** tail = &head;
    DeclList* item;
    (void)variadic;
    for (item = parameters; item; item = item->next) {
        TypeParam* param = ast_arena_alloc(sizeof(*param));
        param->name = item->decl->name;
        param->type = item->decl->type;
        param->is_pack = item->decl->param_is_pack;
        param->is_bitfield = false;
        param->bit_width = 0u;
        param->is_static = false;
        param->initializer = NULL;
        param->is_deprecated = item->decl->param_is_deprecated;
        param->deprecated_message = item->decl->param_deprecated_message;
        param->cxx_access = 0u;
        param->next = NULL;
        *tail = param;
        tail = &param->next;
    }
    return head;
}

static DeclList* parse_parameter_list(bool* variadic) {
    DeclList* parameters = NULL;
    int parameter_index = 0;
    *variadic = false;
    if (check(TOK_VOID) && parser.cur->next &&
        parser.cur->next->type == TOK_RPAREN) {
        advance();
        return NULL;
    }
    while (!check(TOK_RPAREN) && !at_end()) {
        Type* parameter_base;
        Type* parameter_type;
        Type* parameter_array_type = NULL;
        const char* parameter_name = NULL;
        Expr* parameter_default = NULL;
        bool parameter_is_maybe_unused = false;
        bool parameter_is_deprecated = false;
        bool parameter_type_is_reproducible = false;
        bool parameter_type_is_unsequenced = false;
        SourceLoc parameter_type_attribute_loc = peek()->loc;
        const char* parameter_deprecated_message = NULL;
        Decl* parameter;
        if (match(TOK_ELLIPSIS)) {
            if (parameter_index == 0 && !parser_cxx_mode) {
                rcc_error(previous()->loc,
                          "ellipsis requires at least one named parameter");
            }
            *variadic = true;
            break;
        }
        skip_attributes();
        parser_accumulate_c_function_type_attributes(
            &parameter_type_is_reproducible,
            &parameter_type_is_unsequenced,
            &parameter_type_attribute_loc);
        parser_accumulate_c_maybe_unused_attribute(
            &parameter_is_maybe_unused);
        parser_accumulate_c_deprecated_attribute(
            &parameter_is_deprecated, &parameter_deprecated_message);
        parameter_base = parse_type_spec();
        if (!parameter_base) {
            rcc_error(peek()->loc, "expected parameter type specifier");
            if (check(TOK_IDENT)) advance();
            parameter_base = type_int;
        }
        skip_attributes();
        parser_accumulate_c_function_type_attributes(
            &parameter_type_is_reproducible,
            &parameter_type_is_unsequenced,
            &parameter_type_attribute_loc);
        parser_accumulate_c_maybe_unused_attribute(
            &parameter_is_maybe_unused);
        parser_accumulate_c_deprecated_attribute(
            &parameter_is_deprecated, &parameter_deprecated_message);
        parser_apply_c_function_type_attributes(
            parameter_base, parameter_type_is_reproducible,
            parameter_type_is_unsequenced,
            parameter_type_attribute_loc);
        parameter_type = parse_declarator(parameter_base, &parameter_name, NULL);
        /* In a function type pattern, `Args...` is a type-pack expansion,
         * distinct from the final C-style `...` handled at the top of this
         * loop.  parse_declarator also records the named `Args... name` form. */
        if (parser_cxx_mode && match(TOK_ELLIPSIS)) {
            parser_last_declarator_parameter_pack = true;
        }
        /* GNU parameter attributes follow the declarator, as in
         * `int callback(void *context __attribute__((unused)))`. */
        skip_attributes();
        parser_apply_pending_c_function_type_attributes(
            parameter_type, parameter_type_attribute_loc);
        if (take_weak_attribute()) {
            rcc_error(previous()->loc,
                      "weak attribute requires a file-scope declaration");
        }
        if (take_c_noreturn_attribute()) {
            rcc_error(previous()->loc,
                      "C23 [[noreturn]] requires a function declaration");
        }
        if (take_c_nodiscard_attribute(NULL)) {
            rcc_error(previous()->loc,
                      "C23 [[nodiscard]] requires a function or type definition");
        }
        parser_accumulate_c_deprecated_attribute(
            &parameter_is_deprecated, &parameter_deprecated_message);
        if (parameter_is_deprecated) {
            rcc_warning(previous()->loc,
                        "C23 [[deprecated]] on a parameter violates the attribute appertainment constraint; accepting it as an extension");
        }
        if (parameter_type->kind == TYPE_ARRAY) {
            parameter_array_type = parameter_type;
            {
                Type* adjusted = type_ptr(parameter_type->base);
                adjusted->is_const = parameter_type->is_const;
                adjusted->is_volatile = parameter_type->is_volatile;
                adjusted->is_const = adjusted->is_const ||
                    parameter_type->array_parameter_const;
                adjusted->is_volatile = adjusted->is_volatile ||
                    parameter_type->array_parameter_volatile;
                adjusted->is_restrict =
                    parameter_type->array_parameter_restrict;
                parameter_type = adjusted;
            }
        } else if (parameter_type->kind == TYPE_FUNC) {
            parameter_type = type_ptr(parameter_type);
        } else if (parser_type_is_variably_modified(parameter_type)) {
            /* A pointer-to-VLA parameter keeps its pointer ABI type, but the
             * original variably modified type is needed for bound analysis
             * and for multidimensional stride lowering. */
            parameter_array_type = parameter_type;
        }
        if (parser_cxx_mode && match(TOK_ASSIGN)) {
            parameter_default = parse_assignment();
        }
        parameter = decl_param(parameter_name, parameter_type,
                               parameter_index++, peek()->loc);
        parameter->param_is_pack = parser_last_declarator_parameter_pack;
        parameter->param_array_type = parameter_array_type;
        parameter->c23_is_maybe_unused = parameter_is_maybe_unused;
        parameter->param_default = parameter_default;
        parameter->param_is_deprecated = parameter_is_deprecated;
        parameter->param_deprecated_message = parameter_deprecated_message;
        decllist_append(&parameters, parameter);
        if (!match(TOK_COMMA)) break;
        if (check(TOK_RPAREN)) {
            rcc_error(peek()->loc,
                      "expected parameter declaration after ','");
            break;
        }
    }
    return parameters;
}

/* C still permits the obsolescent identifier-list form in a function
 * declarator.  It is distinguishable from a typed prototype when the tokens
 * inside the parentheses are only identifiers separated by commas.  A known
 * typedef name remains a type specifier, so do not steal that spelling from
 * the normal prototype parser. */
static bool parser_is_old_style_parameter_list(void) {
    Token* cursor = parser.cur;
    if (parser_cxx_mode || !cursor || cursor->type == TOK_RPAREN ||
        cursor->type != TOK_IDENT ||
        parser_lookup_type(cursor->value.str_val) != NULL) {
        return false;
    }
    for (;;) {
        cursor = cursor->next;
        if (!cursor || cursor->type == TOK_RPAREN) return true;
        if (cursor->type != TOK_COMMA) return false;
        cursor = cursor->next;
        if (!cursor || cursor->type != TOK_IDENT ||
            parser_lookup_type(cursor->value.str_val) != NULL) {
            return false;
        }
    }
}

static DeclList* parse_old_style_parameter_list(void) {
    DeclList* parameters = NULL;
    int parameter_index = 0;
    while (!check(TOK_RPAREN) && !at_end()) {
        Token* identifier = expect(TOK_IDENT, "old-style parameter name");
        if (identifier) {
            Decl* parameter = decl_param(identifier->value.str_val, type_int,
                                         parameter_index++, identifier->loc);
            decllist_append(&parameters, parameter);
        }
        if (!match(TOK_COMMA)) break;
    }
    return parameters;
}

typedef struct ParsedPointerLevel {
    bool is_const;
    bool is_volatile;
    bool is_restrict;
    bool is_reference;
    bool is_rvalue_reference;
    Type* member_pointer_owner;
    struct ParsedPointerLevel* next;
} ParsedPointerLevel;

/* A parenthesized pointer declarator is also the spelling used for a
 * pointer-to-function.  Decide which grammar follows the closing parenthesis
 * before taking the function-pointer shortcut; otherwise a valid pointer to
 * a VLA such as `int (*rows)[count]` is mistaken for a function declarator. */
static bool parser_parenthesized_pointer_is_function(void) {
    Token* token = parser.cur;
    int depth = 0;
    if (!token || token->type != TOK_LPAREN) return false;
    for (; token; token = token->next) {
        if (token->type == TOK_LPAREN) {
            ++depth;
        } else if (token->type == TOK_RPAREN) {
            --depth;
            if (depth == 0) {
                return token->next && token->next->type == TOK_LPAREN;
            }
        }
    }
    return false;
}

static bool parser_parenthesized_declarator_starts_member_pointer(void) {
    Token* cursor = parser.cur ? parser.cur->next : NULL;
    if (!parser_cxx_mode || !cursor || cursor->type != TOK_IDENT) {
        return false;
    }
    while (cursor && cursor->type == TOK_IDENT && cursor->next &&
           cursor->next->type == TOK_SCOPE && cursor->next->next) {
        if (cursor->next->next->type == TOK_STAR) return true;
        if (cursor->next->next->type != TOK_IDENT) return false;
        cursor = cursor->next->next;
    }
    return false;
}

static ParsedPointerLevel* parse_pointer_levels(void) {
    ParsedPointerLevel* levels = NULL;
    ParsedPointerLevel** tail = &levels;
    bool saw_reference = false;
    while (match(TOK_STAR) ||
           (parser_cxx_mode && (match(TOK_AMP) || match(TOK_AND)))) {
        ParsedPointerLevel* level = ast_arena_alloc(sizeof(*level));
        level->is_reference = previous()->type != TOK_STAR;
        level->is_rvalue_reference = previous()->type == TOK_AND;
        if (level->is_reference) {
            if (saw_reference) {
                rcc_error(previous()->loc,
                          "a C++ declarator cannot directly bind a reference "
                          "to a reference");
            }
            saw_reference = true;
        }
        while (check(TOK_CONST) || check(TOK_VOLATILE) ||
               check(TOK_RESTRICT)) {
            if (match(TOK_CONST)) level->is_const = true;
            else if (match(TOK_VOLATILE)) level->is_volatile = true;
            else if (match(TOK_RESTRICT)) level->is_restrict = true;
        }
        /* GNU attributes may appear between a pointer operator and the
         * declared identifier, for example `void* __attribute__((used)) f`.
         * Keep them pending for parse_declaration/parameter parsing to apply
         * after the complete declarator is known. */
        while (check(TOK___ATTRIBUTE__)) skip_attributes();
        *tail = level;
        tail = &level->next;
    }
    return levels;
}

/* Parse the pointer layer inside `int (Owner::*method)(Args...)`.  This is
 * separate from ordinary pointer levels because the class owner is part of
 * the pointer type's semantic identity. */
static bool parse_member_pointer_level(ParsedPointerLevel** level_out) {
    Token* cursor = parser.cur;
    Token* star = NULL;
    char owner_name[256];
    size_t owner_length = 0u;
    Type* owner;
    ParsedPointerLevel* level;
    if (level_out) *level_out = NULL;
    if (!parser_cxx_mode || !cursor || cursor->type != TOK_IDENT) {
        return false;
    }
    owner_name[0] = '\0';
    while (cursor && cursor->type == TOK_IDENT && cursor->next &&
           cursor->next->type == TOK_SCOPE && cursor->next->next) {
        size_t length = strlen(cursor->value.str_val);
        if (owner_length + length + 2u >= sizeof(owner_name)) return false;
        memcpy(owner_name + owner_length, cursor->value.str_val, length);
        owner_length += length;
        if (cursor->next->next->type == TOK_STAR) {
            star = cursor->next->next;
            break;
        }
        if (cursor->next->next->type != TOK_IDENT) return false;
        memcpy(owner_name + owner_length, "::", 2u);
        owner_length += 2u;
        cursor = cursor->next->next;
    }
    if (!star) return false;
    owner_name[owner_length] = '\0';
    owner = rcc_parser_lookup_type(owner_name);
    if (!owner || (owner->kind != TYPE_STRUCT &&
                   owner->kind != TYPE_UNION)) {
        return false;
    }
    while (parser.cur != star->next) advance();
    level = ast_arena_alloc(sizeof(*level));
    level->member_pointer_owner = owner;
    while (match(TOK_CONST) || match(TOK_VOLATILE) ||
           match(TOK_RESTRICT)) {
        if (previous()->type == TOK_CONST) level->is_const = true;
        else if (previous()->type == TOK_VOLATILE) {
            level->is_volatile = true;
        } else {
            level->is_restrict = true;
        }
    }
    if (level_out) *level_out = level;
    return true;
}

static Type* apply_pointer_levels(Type* type,
                                  ParsedPointerLevel* levels) {
    for (ParsedPointerLevel* level = levels; level; level = level->next) {
        type = level->is_reference
            ? type_reference(type, level->is_rvalue_reference)
            : type_ptr(type);
        if (!type) return NULL;
        if (level->member_pointer_owner) {
            type_cxx_member_pointer(type, level->member_pointer_owner);
        }
        type->is_const = level->is_const;
        type->is_volatile = level->is_volatile;
        type->is_restrict = level->is_restrict;
    }
    return type;
}

static Type* parse_declarator(Type* base_type, const char** name,
                              DeclList** parameters) {
    Type* type = base_type;
    ParsedPointerLevel* leading_pointers;
    int array_lengths[32];
    Expr* array_bounds[32];
    SourceLoc array_locs[32];
    bool array_unspecified[32];
    bool array_statics[32];
    bool array_consts[32];
    bool array_volatiles[32];
    bool array_restricts[32];
    int array_count = 0;
    ParsedPointerLevel* parenthesized_pointers = NULL;
    bool parenthesized_pointer = false;
    bool declarator_parameter_pack = false;
    if (name) *name = NULL;
    if (parameters) *parameters = NULL;
    parser_last_declarator_parameter_pack = false;

    leading_pointers = parse_pointer_levels();

    /* A data-member pointer's member type can itself have pointer layers:
     * `int* Owner::*member`. Parse the ordinary pointer layers before the
     * owner as part of that member type, then parse any layers after `::*`
     * as pointers/references to the completed member-pointer type. */
    if (parser_cxx_mode && check(TOK_IDENT)) {
        Token* cursor = parser.cur;
        Token* star = NULL;
        char owner_name[256];
        size_t owner_length = 0u;
        owner_name[0] = '\0';
        while (cursor && cursor->type == TOK_IDENT && cursor->next &&
               cursor->next->type == TOK_SCOPE && cursor->next->next) {
            Token* segment = cursor;
            if (cursor->next->next->type == TOK_STAR) {
                size_t segment_length = strlen(segment->value.str_val);
                if (owner_length + segment_length >= sizeof(owner_name)) {
                    break;
                }
                memcpy(owner_name + owner_length, segment->value.str_val,
                       segment_length);
                owner_length += segment_length;
                owner_name[owner_length] = '\0';
                star = cursor->next->next;
                break;
            }
            if (cursor->next->next->type != TOK_IDENT) break;
            {
                size_t segment_length = strlen(segment->value.str_val);
                if (owner_length + segment_length + 2u >=
                    sizeof(owner_name)) {
                    break;
                }
                memcpy(owner_name + owner_length, segment->value.str_val,
                       segment_length);
                owner_length += segment_length;
                memcpy(owner_name + owner_length, "::", 2u);
                owner_length += 2u;
                owner_name[owner_length] = '\0';
            }
            cursor = cursor->next->next;
        }
        if (star) {
            Type* owner = rcc_parser_lookup_type(owner_name);
            if (owner && (owner->kind == TYPE_STRUCT ||
                          owner->kind == TYPE_UNION)) {
                type = apply_pointer_levels(type, leading_pointers);
                leading_pointers = NULL;
                while (parser.cur != star->next) advance();
                type = type_ptr(type);
                type_cxx_member_pointer(type, owner);
                while (match(TOK_CONST) || match(TOK_VOLATILE) ||
                       match(TOK_RESTRICT)) {
                    if (previous()->type == TOK_CONST) type->is_const = true;
                    else if (previous()->type == TOK_VOLATILE) {
                        type->is_volatile = true;
                    } else type->is_restrict = true;
                }
                leading_pointers = parse_pointer_levels();
            }
        }
    }

    /* Function-pointer declarator: return_type (*name)(parameters). */
    if (check(TOK_LPAREN) && parser.cur->next &&
        (parser.cur->next->type == TOK_STAR ||
         (parser_cxx_mode && parser.cur->next->type == TOK_AMP) ||
         parser_parenthesized_declarator_starts_member_pointer()) &&
        parser_parenthesized_pointer_is_function()) {
        ParsedPointerLevel* nested_pointers;
        DeclList* function_parameters = NULL;
        bool variadic = false;
        bool has_prototype;
        advance();
        if (!parse_member_pointer_level(&nested_pointers)) {
            nested_pointers = parse_pointer_levels();
        }
        declarator_parameter_pack = parser_cxx_mode && match(TOK_ELLIPSIS);
        if (check(TOK_IDENT)) {
            Token* identifier = advance();
            if (name) *name = identifier->value.str_val;
        }
        /* In `(*callbacks[Count])(Args...)`, the array suffix belongs to
         * the pointer declarator inside the parentheses.  Capture it here
         * and apply it outside the parsed function-pointer type below. */
        while (match(TOK_LBRACKET)) {
            int length = -1;
            Expr* bound_expression = NULL;
            bool unspecified_bound = false;
            bool parameter_static = match(TOK_STATIC);
            bool parameter_const = false;
            bool parameter_volatile = false;
            bool parameter_restrict = false;
            while (check(TOK_CONST) || check(TOK_VOLATILE) ||
                   check(TOK_RESTRICT)) {
                if (match(TOK_CONST)) parameter_const = true;
                else if (match(TOK_VOLATILE)) parameter_volatile = true;
                else if (match(TOK_RESTRICT)) parameter_restrict = true;
            }
            if (match(TOK_STAR)) {
                length = -2;
                unspecified_bound = true;
            } else if (!check(TOK_RBRACKET)) {
                Expr* bound = parse_assignment();
                int64_t constant = 0;
                if (eval_integer_constant(bound, &constant)) {
                    if (constant <= 0 || constant > INT_MAX) {
                        rcc_error(bound->loc,
                                  "array bound is not a positive representable integer constant");
                    } else {
                        length = (int)constant;
                    }
                } else {
                    length = -2;
                    bound_expression = bound;
                }
            }
            expect(TOK_RBRACKET, "]");
            if (array_count >= (int)(sizeof(array_lengths) /
                                     sizeof(array_lengths[0]))) {
                rcc_error(previous()->loc, "array declarator is too deep");
            } else {
                array_lengths[array_count] = length;
                array_bounds[array_count] = bound_expression;
                array_locs[array_count] = previous()->loc;
                array_unspecified[array_count] = unspecified_bound;
                array_statics[array_count] = parameter_static;
                array_consts[array_count] = parameter_const;
                array_volatiles[array_count] = parameter_volatile;
                array_restricts[array_count] = parameter_restrict;
                ++array_count;
            }
        }
        expect(TOK_RPAREN, ")");
        expect(TOK_LPAREN, "(");
        has_prototype = parser_cxx_mode || !check(TOK_RPAREN);
        if (parser_is_old_style_parameter_list()) {
            function_parameters = parse_old_style_parameter_list();
            has_prototype = false;
        } else {
            function_parameters = parse_parameter_list(&variadic);
        }
        expect(TOK_RPAREN, ")");
        type = type_func(apply_pointer_levels(base_type, leading_pointers),
                         parser_type_params(function_parameters, &variadic),
                         variadic);
        type->has_prototype = has_prototype;
        if (nested_pointers && nested_pointers->member_pointer_owner) {
            while (match(TOK_CONST) || match(TOK_VOLATILE)) {
                if (previous()->type == TOK_CONST) type->is_const = true;
                else type->is_volatile = true;
            }
            if (match(TOK_AMP)) {
                type->function_ref_qualifier = CXX_REF_QUAL_LVALUE;
            } else if (match(TOK_AND)) {
                type->function_ref_qualifier = CXX_REF_QUAL_RVALUE;
            }
        }
        if (parser_cxx_mode && match(TOK_NOEXCEPT)) {
            type->function_is_noexcept = true;
            if (match(TOK_LPAREN)) {
                Expr* noexcept_expression = parse_expression();
                int64_t noexcept_value = 0;
                expect(TOK_RPAREN, ")");
                if (!noexcept_expression ||
                    !eval_integer_constant(noexcept_expression,
                                           &noexcept_value)) {
                    rcc_error(noexcept_expression
                                  ? noexcept_expression->loc
                                  : previous()->loc,
                              "function-pointer noexcept requires an integral constant expression");
                } else {
                    type->function_is_noexcept = noexcept_value != 0;
                }
            }
        }
        type = apply_pointer_levels(type, nested_pointers);
        while (array_count > 0) {
            int length = array_lengths[--array_count];
            if (length > 0 && type->size > 0 &&
                length > INT_MAX / type->size) {
                rcc_error(array_locs[array_count],
                          "array bound is too large for the complete element type");
                length = -1;
            }
            type = type_array(type, length);
            type->array_bound = array_bounds[array_count];
            type->array_unspecified_bound =
                array_unspecified[array_count];
            type->array_parameter_static = array_statics[array_count];
            type->array_parameter_const = array_consts[array_count];
            type->array_parameter_volatile =
                array_volatiles[array_count];
            type->array_parameter_restrict =
                array_restricts[array_count];
        }
        if (parameters) *parameters = function_parameters;
        parser_last_declarator_parameter_pack = declarator_parameter_pack;
        return type;
    }

    /* Handle the other parenthesized pointer declarators before parsing
     * suffixes.  Array suffixes bind to the declarator inside the group, and
     * the pointer levels inside the group are applied afterwards. */
    if (check(TOK_LPAREN) && parser.cur->next &&
        (parser.cur->next->type == TOK_STAR ||
         (parser_cxx_mode && parser.cur->next->type == TOK_AMP) ||
         (parser_cxx_mode && parser.cur->next->type == TOK_AND) ||
         parser_parenthesized_declarator_starts_member_pointer())) {
        advance();
        if (!parse_member_pointer_level(&parenthesized_pointers)) {
            parenthesized_pointers = parse_pointer_levels();
        }
        declarator_parameter_pack = parser_cxx_mode && match(TOK_ELLIPSIS);
        if (check(TOK_IDENT)) {
            Token* identifier = advance();
            if (name) *name = identifier->value.str_val;
        }
        expect(TOK_RPAREN, ")");
        parenthesized_pointer = true;
    }

    type = apply_pointer_levels(type, leading_pointers);
    if (parser_cxx_mode && match(TOK_ELLIPSIS)) {
        declarator_parameter_pack = true;
    }
    if (!parenthesized_pointer && check(TOK_IDENT)) {
        Token* identifier = advance();
        if (name) *name = identifier->value.str_val;
    }
    if (parser_cxx_mode && match(TOK_ELLIPSIS)) {
        declarator_parameter_pack = true;
    }

    /* C23 permits an entity attribute after the declared identifier and
     * before a function declarator suffix, e.g. `int f [[nodiscard]] (void)`. */
    if (!parser_cxx_mode && check(TOK_LBRACKET) &&
        check_next(TOK_LBRACKET)) {
        parser_skip_c_attribute_sequence();
    }

    for (;;) {
        if (!parser_cxx_mode && check(TOK_LBRACKET) &&
            check_next(TOK_LBRACKET)) {
            break;
        }
        if (match(TOK_LBRACKET)) {
            int length = -1;
            Expr* bound_expression = NULL;
            bool unspecified_bound = false;
            bool parameter_static = match(TOK_STATIC);
            bool parameter_const = false;
            bool parameter_volatile = false;
            bool parameter_restrict = false;
            while (check(TOK_CONST) || check(TOK_VOLATILE) ||
                   check(TOK_RESTRICT)) {
                if (match(TOK_CONST)) parameter_const = true;
                else if (match(TOK_VOLATILE)) parameter_volatile = true;
                else if (match(TOK_RESTRICT)) parameter_restrict = true;
            }
            if (match(TOK_STAR)) {
                length = -2;
                unspecified_bound = true;
            } else if (!check(TOK_RBRACKET)) {
                Expr* bound = parse_assignment();
                int64_t constant = 0;
                if (eval_integer_constant(bound, &constant)) {
                    if (constant <= 0 || constant > INT_MAX) {
                        rcc_error(bound->loc,
                                  "array bound is not a positive representable integer constant");
                    } else {
                        length = (int)constant;
                    }
                } else {
                    /* A non-constant bound is a C17 variable-length array
                     * dimension. Its semantic type and runtime extent are
                     * checked after parameter/local scopes are available. */
                    length = -2;
                    bound_expression = bound;
                }
            }
            expect(TOK_RBRACKET, "]");
            if (array_count >= (int)(sizeof(array_lengths) /
                                     sizeof(array_lengths[0]))) {
                rcc_error(previous()->loc, "array declarator is too deep");
            } else {
                array_lengths[array_count] = length;
                array_bounds[array_count] = bound_expression;
                array_locs[array_count] = previous()->loc;
                array_unspecified[array_count] = unspecified_bound;
                array_statics[array_count] = parameter_static;
                array_consts[array_count] = parameter_const;
                array_volatiles[array_count] = parameter_volatile;
                array_restricts[array_count] = parameter_restrict;
                ++array_count;
            }
        } else if (parser_cxx_mode && !parenthesized_pointer &&
                   array_count > 0 && check(TOK_LPAREN)) {
            /* C++20 permits an array aggregate initializer directly after
             * the declarator (`int values[3](1, 2, 3)`).  A plain array
             * declarator cannot form a function type; leave this token for
             * parse_declaration, while parenthesized pointer declarators
             * retain the ordinary array-of-function-pointer grammar. */
            break;
        } else if (match(TOK_LPAREN)) {
            bool variadic = false;
            bool has_prototype = parser_cxx_mode || !check(TOK_RPAREN);
            DeclList* function_parameters;
            if (parser_is_old_style_parameter_list()) {
                function_parameters = parse_old_style_parameter_list();
                has_prototype = false;
            } else {
                function_parameters = parse_parameter_list(&variadic);
            }
            expect(TOK_RPAREN, ")");
            type = type_func(type,
                             parser_type_params(function_parameters, &variadic),
                             variadic);
            type->has_prototype = has_prototype;
            if (parser_cxx_mode && match(TOK_NOEXCEPT)) {
                type->function_is_noexcept = true;
                if (match(TOK_LPAREN)) {
                    Expr* noexcept_expression = parse_expression();
                    int64_t noexcept_value = 0;
                    expect(TOK_RPAREN, ")");
                    if (!noexcept_expression ||
                        !eval_integer_constant(noexcept_expression,
                                               &noexcept_value)) {
                        rcc_error(noexcept_expression
                                      ? noexcept_expression->loc
                                      : previous()->loc,
                                  "function type noexcept requires an integral constant expression");
                    } else {
                        type->function_is_noexcept = noexcept_value != 0;
                    }
                }
            }
            if (parameters) *parameters = function_parameters;
        } else {
            break;
        }
    }
    /* Array declarator suffixes bind from the identifier outward.  Applying
     * them in source order reverses `int a[2][3]` into [3][2]; retain the
     * parsed suffixes and construct the type from the inside out instead. */
    while (array_count > 0) {
        int length = array_lengths[--array_count];
        if (length > 0 && type->size > 0 &&
            length > INT_MAX / type->size) {
            rcc_error(array_locs[array_count],
                      "array bound is too large for the complete element type");
            length = -1;
        }
        type = type_array(type, length);
        type->array_bound = array_bounds[array_count];
        type->array_unspecified_bound = array_unspecified[array_count];
        type->array_parameter_static = array_statics[array_count];
        type->array_parameter_const = array_consts[array_count];
        type->array_parameter_volatile = array_volatiles[array_count];
        type->array_parameter_restrict = array_restricts[array_count];
    }
    if (parenthesized_pointers) {
        type = apply_pointer_levels(type, parenthesized_pointers);
    }
    parser_last_declarator_parameter_pack = declarator_parameter_pack;
    return type;
}

Type* rcc_parser_parse_cxx_declarator(Type* base_type, const char** name,
                                      DeclList** parameters) {
    return parse_declarator(base_type, name, parameters);
}

/* ═══════════════════════════════════════
 * Statement Parsing
 * ═══════════════════════════════════════ */

static Stmt* parse_block(void) {
    SourceLoc loc = previous()->loc;
    StmtList* stmts = NULL;
    Token* preserved_recovery_boundary = NULL;
    ParserEnumConstant* saved_enum_constants = parser_enum_constants;
    ParserTagName* saved_tag_names = parser_tag_names;
    ParserTagScope* saved_tag_scope = parser_tag_scope;
    ParserTagScope* block_tag_scope = ast_arena_alloc(
        sizeof(*block_tag_scope));
    void* saved_type_names = rcc_parser_type_scope_mark();
    void* saved_cxx_using = parser_cxx_mode &&
                            rcc_parser_cxx_using_scope_mark
        ? rcc_parser_cxx_using_scope_mark() : NULL;

    block_tag_scope->parent = parser_tag_scope;
    parser_tag_scope = block_tag_scope;

    while (!check(TOK_RBRACE) && !at_end()) {
        Token* iteration_start = parser.cur;
        int errors_before = g_error_count;
        bool preserve_recovery_boundary = false;
        Stmt* s = parser_cxx_mode && rcc_parse_cxx_statement
            ? rcc_parse_cxx_statement() : parse_declaration();
        if (s) {
            stmtlist_append(&stmts, s);
        }
        if (g_error_count > errors_before && parser.cur == iteration_start) {
            /* A missing terminator commonly leaves the next declaration's
             * type token at the cursor.  Leave that token for the next parse
             * iteration; if that declaration also makes no progress, the
             * same boundary is consumed on the following attempt. */
            if (parser_is_recovery_declaration_start(parser.cur) &&
                preserved_recovery_boundary != parser.cur) {
                preserved_recovery_boundary = parser.cur;
                preserve_recovery_boundary = true;
            } else {
                synchronize();
                preserved_recovery_boundary = NULL;
            }
        } else if (parser.cur != iteration_start ||
                   g_error_count == errors_before) {
            preserved_recovery_boundary = NULL;
        }
        if (parser.cur == iteration_start && !at_end() &&
            !check(TOK_RBRACE) && !preserve_recovery_boundary) {
            advance();
        }
    }

    expect(TOK_RBRACE, "}");
    parser_enum_constants = saved_enum_constants;
    parser_tag_names = saved_tag_names;
    parser_tag_scope = saved_tag_scope;
    rcc_parser_type_scope_restore(saved_type_names);
    if (parser_cxx_mode && rcc_parser_cxx_using_scope_restore) {
        rcc_parser_cxx_using_scope_restore(saved_cxx_using);
    }
    return stmt_block(stmts, loc);
}

static bool cxx_selection_has_init(void) {
    Token* token;
    int depth = 0;
    if (!parser_cxx_mode || !parser.cur) return false;
    for (token = parser.cur; token; token = token->next) {
        if (token->type == TOK_LPAREN || token->type == TOK_LBRACKET ||
            token->type == TOK_LBRACE) {
            ++depth;
        } else if (token->type == TOK_RPAREN) {
            if (depth == 0) break;
            --depth;
        } else if (token->type == TOK_RBRACKET ||
                   token->type == TOK_RBRACE) {
            if (depth > 0) --depth;
        } else if (token->type == TOK_SEMICOLON && depth == 0) {
            return true;
        }
    }
    return false;
}

static Stmt* cxx_wrap_selection_init(Stmt* init, Stmt* selection,
                                     SourceLoc loc) {
    StmtList* statements = NULL;
    if (!init) return selection;
    stmtlist_append(&statements, init);
    stmtlist_append(&statements, selection);
    return stmt_block(statements, loc);
}

static Stmt* parse_if_stmt(void) {
    SourceLoc loc = previous()->loc;
    bool is_constexpr = parser_cxx_mode && match(TOK_CONSTEXPR);
    if (is_constexpr && !rcc_parser_cxx_standard_at_least(17)) {
        rcc_error(loc, "if constexpr requires C++17 or newer");
    }
    Stmt* init = NULL;
    expect(TOK_LPAREN, "(");
    if (cxx_selection_has_init()) {
        if (!rcc_parser_cxx_standard_at_least(17)) {
            rcc_error(loc,
                      "selection-statement initializers require C++17 or newer");
        }
        if (is_type_start() || check(TOK_AUTO)) {
            init = parse_declaration();
        } else {
            SourceLoc init_loc = peek()->loc;
            Expr* init_expr = parse_expression();
            expect(TOK_SEMICOLON, ";");
            init = stmt_expr(init_expr, init_expr ? init_expr->loc : init_loc);
        }
    }
    Expr* cond = parse_expression();
    expect(TOK_RPAREN, ")");

    Stmt* then_stmt = parse_statement();
    Stmt* else_stmt = NULL;

    if (match(TOK_ELSE)) {
        else_stmt = parse_statement();
    }

    Stmt* statement = stmt_if(cond, then_stmt, else_stmt, loc);
    statement->if_is_constexpr = is_constexpr;
    return cxx_wrap_selection_init(init, statement, loc);
}

static Stmt* parse_while_stmt(void) {
    SourceLoc loc = previous()->loc;
    expect(TOK_LPAREN, "(");
    Expr* cond = parse_expression();
    expect(TOK_RPAREN, ")");
    Stmt* body = parse_statement();
    return stmt_while(cond, body, loc);
}

static Stmt* parse_do_stmt(void) {
    SourceLoc loc = previous()->loc;
    Stmt* body = parse_statement();
    expect(TOK_WHILE, "while");
    expect(TOK_LPAREN, "(");
    Expr* cond = parse_expression();
    expect(TOK_RPAREN, ")");
    expect(TOK_SEMICOLON, ";");
    return stmt_do(body, cond, loc);
}

static Stmt* parse_for_stmt(void) {
    SourceLoc loc = previous()->loc;
    expect(TOK_LPAREN, "(");

    Stmt* init = NULL;
    if (!check(TOK_SEMICOLON)) {
        if (is_type_start() || (parser_cxx_mode && check(TOK_AUTO))) {
            init = parse_declaration();
        } else {
            Expr* e = parse_expression();
            expect(TOK_SEMICOLON, ";");
            init = stmt_expr(e, e->loc);
        }
    } else {
        advance();  /* consume ; */
    }

    Expr* cond = NULL;
    if (!check(TOK_SEMICOLON)) {
        cond = parse_expression();
    }
    expect(TOK_SEMICOLON, ";");

    Expr* inc = NULL;
    if (!check(TOK_RPAREN)) {
        inc = parse_expression();
    }
    expect(TOK_RPAREN, ")");

    Stmt* body = parse_statement();
    return stmt_for(init, cond, inc, body, loc);
}

static Stmt* parse_switch_stmt(void) {
    SourceLoc loc = previous()->loc;
    Stmt* init = NULL;
    expect(TOK_LPAREN, "(");
    if (cxx_selection_has_init()) {
        if (!rcc_parser_cxx_standard_at_least(17)) {
            rcc_error(loc,
                      "selection-statement initializers require C++17 or newer");
        }
        if (is_type_start() || check(TOK_AUTO)) {
            init = parse_declaration();
        } else {
            SourceLoc init_loc = peek()->loc;
            Expr* init_expr = parse_expression();
            expect(TOK_SEMICOLON, ";");
            init = stmt_expr(init_expr, init_expr ? init_expr->loc : init_loc);
        }
    }
    Expr* expr = parse_expression();
    expect(TOK_RPAREN, ")");
    Stmt* body = parse_statement();
    return cxx_wrap_selection_init(init, stmt_switch(expr, body, loc), loc);
}

static Stmt* parse_return_stmt(void) {
    SourceLoc loc = previous()->loc;
    Expr* val = NULL;

    if (!check(TOK_SEMICOLON)) {
        val = parser_cxx_mode && check(TOK_LBRACE)
            ? parse_initializer() : parse_expression();
    }
    expect(TOK_SEMICOLON, ";");

    return stmt_return(val, loc);
}

static Stmt* parse_statement(void) {
    /* Standard statement attributes may precede a control statement, a label,
     * an empty statement, or a declaration. Preserve declaration attributes
     * until parse_declaration can attach them to the resulting Decl. */
    bool has_cxx_attributes = false;
    bool has_c_attributes = false;
    bool c_label_ahead = false;
    if (parser_cxx_mode && rcc_parser_cxx_collect_statement_attributes) {
        has_cxx_attributes =
            rcc_parser_cxx_collect_statement_attributes();
    }
    if (!parser_cxx_mode && check(TOK_LBRACKET) && check_next(TOK_LBRACKET)) {
        skip_attributes();
        has_c_attributes = true;
    }
    if (has_cxx_attributes && !is_type_start() && !check(TOK_AUTO) &&
        rcc_parser_cxx_discard_statement_attributes) {
        rcc_parser_cxx_discard_statement_attributes();
    }
    SourceLoc loc = peek()->loc;

    if (!parser_cxx_mode &&
        ((check(TOK_IDENT) && parser.cur->next &&
          parser.cur->next->type == TOK_COLON) ||
         check(TOK_CASE) || check(TOK_DEFAULT))) {
        c_label_ahead = true;
    }
    SourceLoc fallthrough_loc = loc;
    bool has_c_fallthrough_attribute =
        take_c_fallthrough_attribute(&fallthrough_loc);
    if (has_c_attributes && !is_type_start() && !c_label_ahead) {
        if (take_c_maybe_unused_attribute()) {
            rcc_error(loc,
                      "C23 [[maybe_unused]] is only valid on an entity declaration or label");
        }
        if (take_c_noreturn_attribute()) {
            rcc_error(loc,
                      "C23 [[noreturn]] requires a function declaration");
        }
        if (take_c_nodiscard_attribute(NULL)) {
            rcc_error(loc,
                      "C23 [[nodiscard]] requires a function or type definition");
        }
        if (take_c_deprecated_attribute(NULL)) {
            rcc_error(loc,
                      "C23 [[deprecated]] requires an entity declaration");
        }
        parser_apply_pending_c_function_type_attributes(NULL, loc);
    } else if (has_c_attributes && c_label_ahead) {
        parser_apply_pending_c_function_type_attributes(NULL, loc);
    }

    if (has_c_fallthrough_attribute) {
        if (match(TOK_SEMICOLON)) {
            Stmt* fallthrough = stmt_null(fallthrough_loc);
            fallthrough->c23_is_fallthrough = true;
            return fallthrough;
        }
        rcc_error(fallthrough_loc,
                  "C23 [[fallthrough]] must be a standalone attribute declaration");
    }

    /* C++ exception statements are parsed by the C++ frontend even inside
     * free-function bodies, which otherwise use this shared C statement
     * parser.  The frontend retains the construct for diagnostics instead
     * of allowing the C expression parser to reinterpret `try`/`throw`. */
    if (parser_cxx_mode && rcc_parse_cxx_statement &&
        (check(TOK_TRY) || check(TOK_THROW))) {
        return rcc_parse_cxx_statement();
    }

    if (match(TOK_LBRACE)) {
        return parse_block();
    }
    if (match(TOK_IF)) {
        return parse_if_stmt();
    }
    if (match(TOK_WHILE)) {
        return parse_while_stmt();
    }
    if (match(TOK_DO)) {
        return parse_do_stmt();
    }
    if (parser_cxx_mode && rcc_parse_cxx_range_for_statement &&
        check(TOK_FOR)) {
        Stmt* range_for = rcc_parse_cxx_range_for_statement();
        if (range_for) return range_for;
    }
    if (match(TOK_FOR)) {
        return parse_for_stmt();
    }
    if (match(TOK_SWITCH)) {
        return parse_switch_stmt();
    }
    if (match(TOK_CASE)) {
        bool label_is_maybe_unused = take_c_maybe_unused_attribute();
        if (take_c_noreturn_attribute()) {
            rcc_error(loc,
                      "C23 [[noreturn]] requires a function declaration");
        }
        if (take_c_nodiscard_attribute(NULL)) {
            rcc_error(loc,
                      "C23 [[nodiscard]] requires a function or type definition");
        }
        if (take_c_deprecated_attribute(NULL)) {
            rcc_error(loc,
                      "C23 [[deprecated]] requires an entity declaration");
        }
        /* A case label requires a conditional-expression, not the wider
         * comma/assignment expression grammar. */
        Expr* val = parse_conditional();
        expect(TOK_COLON, ":");
        Stmt* s = parse_statement();
        Stmt* label = stmt_case(val, s, loc);
        label->c23_is_maybe_unused = label_is_maybe_unused;
        return label;
    }
    if (match(TOK_DEFAULT)) {
        bool label_is_maybe_unused = take_c_maybe_unused_attribute();
        if (take_c_noreturn_attribute()) {
            rcc_error(loc,
                      "C23 [[noreturn]] requires a function declaration");
        }
        if (take_c_nodiscard_attribute(NULL)) {
            rcc_error(loc,
                      "C23 [[nodiscard]] requires a function or type definition");
        }
        if (take_c_deprecated_attribute(NULL)) {
            rcc_error(loc,
                      "C23 [[deprecated]] requires an entity declaration");
        }
        expect(TOK_COLON, ":");
        Stmt* s = parse_statement();
        Stmt* label = stmt_default(s, loc);
        label->c23_is_maybe_unused = label_is_maybe_unused;
        return label;
    }
    if (match(TOK_BREAK)) {
        expect(TOK_SEMICOLON, ";");
        return stmt_break(loc);
    }
    if (match(TOK_CONTINUE)) {
        expect(TOK_SEMICOLON, ";");
        return stmt_continue(loc);
    }
    if (match(TOK_RETURN)) {
        return parse_return_stmt();
    }
    if (match(TOK_GOTO)) {
        Token* label = expect(TOK_IDENT, "label");
        expect(TOK_SEMICOLON, ";");
        return stmt_goto(label ? label->value.str_val : "", loc);
    }
    if (match(TOK_SEMICOLON)) {
        return stmt_null(loc);
    }

    /* Inline assembly: asm [volatile] ( "template" [: outputs [: inputs [: clobbers]]] ) ; */
    if (match(TOK_ASM)) {
        bool is_volatile = false;
        bool is_basic = true;
        if (match(TOK___VOLATILE__) || match(TOK_VOLATILE)) {
            is_volatile = true;
        }

        expect(TOK_LPAREN, "(");

        /* Parse template string */
        Token* templ_tok = expect(TOK_STRING_LIT, "assembly template");
        const char* templ = templ_tok ? templ_tok->value.str_val : "";
        char* joined_template = NULL;
        size_t template_length = strlen(templ);
        while (check(TOK_STRING_LIT)) {
            Token* adjacent = advance();
            const char* text = adjacent->value.str_val;
            size_t adjacent_length = strlen(text);
            char* grown = rcc_realloc(
                joined_template,
                template_length + adjacent_length + 1u);
            if (!joined_template) {
                memcpy(grown, templ, template_length);
            }
            memcpy(grown + template_length, text, adjacent_length + 1u);
            joined_template = grown;
            template_length += adjacent_length;
        }
        if (joined_template) templ = joined_template;

        AsmOperand* outputs = NULL;
        AsmOperand* inputs = NULL;
        AsmClobber* clobbers = NULL;

        /* Parse output operands.  TOK_SCOPE is the lexer token for GCC's
         * compact empty-output spelling "::"; it means the first colon and
         * the input separator were written without whitespace. */
        bool compact_empty_outputs = match(TOK_SCOPE);
        if (compact_empty_outputs || match(TOK_COLON)) {
            is_basic = false;
            AsmOperand** out_tail = &outputs;
            while (!compact_empty_outputs && check(TOK_STRING_LIT)) {
                Token* constraint = advance();
                expect(TOK_LPAREN, "(");
                Expr* expr = parse_expression();
                expect(TOK_RPAREN, ")");

                AsmOperand* op = asm_operand_new(constraint->value.str_val, expr);
                *out_tail = op;
                out_tail = &op->next;

                if (!match(TOK_COMMA)) break;
            }

            /* Parse input operands */
            if (compact_empty_outputs || match(TOK_COLON)) {
                AsmOperand** in_tail = &inputs;
                while (check(TOK_STRING_LIT)) {
                    Token* constraint = advance();
                    expect(TOK_LPAREN, "(");
                    Expr* expr = parse_expression();
                    expect(TOK_RPAREN, ")");

                    AsmOperand* op = asm_operand_new(constraint->value.str_val, expr);
                    *in_tail = op;
                    in_tail = &op->next;

                    if (!match(TOK_COMMA)) break;
                }

                /* Parse clobbers */
                if (match(TOK_COLON)) {
                    AsmClobber** clob_tail = &clobbers;
                    while (check(TOK_STRING_LIT)) {
                        Token* reg = advance();
                        AsmClobber* cl = asm_clobber_new(reg->value.str_val);
                        *clob_tail = cl;
                        clob_tail = &cl->next;

                        if (!match(TOK_COMMA)) break;
                    }
                }
            }
        }

        expect(TOK_RPAREN, ")");
        expect(TOK_SEMICOLON, ";");
        return stmt_asm(templ, outputs, inputs, clobbers, is_volatile,
                        is_basic, loc);
    }

    /* __extension__ - just skip it */
    if (match(TOK___EXTENSION__)) {
        return parse_statement();
    }

    /* Label? */
    if (c_label_ahead ||
        (check(TOK_IDENT) && parser.cur->next &&
         parser.cur->next->type == TOK_COLON)) {
        Token* name = advance();
        bool label_is_maybe_unused = take_c_maybe_unused_attribute();
        if (take_c_noreturn_attribute()) {
            rcc_error(name->loc,
                      "C23 [[noreturn]] requires a function declaration");
        }
        if (take_c_nodiscard_attribute(NULL)) {
            rcc_error(name->loc,
                      "C23 [[nodiscard]] requires a function or type definition");
        }
        if (take_c_deprecated_attribute(NULL)) {
            rcc_error(name->loc,
                      "C23 [[deprecated]] requires an entity declaration");
        }
        advance();  /* consume : */
        Stmt* s = parse_statement();
        Stmt* label = stmt_label(name->value.str_val, s, loc);
        label->c23_is_maybe_unused = label_is_maybe_unused;
        return label;
    }

    /* Expression statement */
    Expr* e = parse_expression();
    expect(TOK_SEMICOLON, ";");
    return stmt_expr(e, loc);
}

/* ═══════════════════════════════════════
 * Declaration Parsing
 * ═══════════════════════════════════════ */

static Stmt* parser_declaration_list_result(StmtList* declarations,
                                            SourceLoc loc) {
    Stmt* result;
    if (!declarations) return stmt_null(loc);
    if (!declarations->next) return declarations->stmt;
    result = stmt_block(declarations, loc);
    /* A comma-separated declaration is one source-scope declaration list,
     * not a nested compound statement.  Keeping this marker prevents the
     * semantic and code-generation scope stacks from being changed while
     * still reusing the existing statement-list representation. */
    result->block_no_scope = true;
    return result;
}

static Expr* parser_parse_variable_initializer(Type* type, SourceLoc loc) {
    Expr* init = NULL;
    if (match(TOK_ASSIGN)) {
        init = parse_initializer();
    } else if (parser_cxx_mode && check(TOK_LBRACE)) {
        init = parse_initializer();
    } else if (parser_cxx_mode && type && type->kind == TYPE_ARRAY &&
               match(TOK_LPAREN)) {
        ExprList* arguments = NULL;
        if (!check(TOK_RPAREN)) {
            do {
                exprlist_append(&arguments, parse_assignment_expression());
            } while (match(TOK_COMMA));
        }
        expect(TOK_RPAREN, ")");
        if (!rcc_parser_cxx_standard_at_least(20)) {
            rcc_error(loc,
                      "C++20 aggregate parenthesized initialization requires C++20 or newer");
        }
        init = expr_initializer_list(arguments, loc);
        init->compound_type = type;
        init->compound_paren_init = true;
    }
    /* A declaration-level C++ braced initializer carries the declared class
     * type, just like a direct-list expression parsed in an expression or
     * local-declaration context. */
    if (parser_cxx_mode && init && init->kind == EXPR_COMPOUND &&
        !init->compound_type) {
        init->compound_type = type;
    }
    if (parser_cxx_mode && rcc_parser_validate_cxx_object_type) {
        rcc_parser_validate_cxx_object_type(type, loc);
    }
    rcc_parser_validate_cxx_constructor_initializer(type, init);
    return init;
}

static Decl* parser_make_variable_decl(const char* name, Type* type,
                                       Expr* init, SourceLoc loc,
                                       StorageClass storage,
                                       bool is_thread_local,
                                       bool is_constexpr,
                                       bool is_constinit,
                                       bool is_inline, bool is_noreturn,
                                       bool is_consteval) {
    Decl* declaration = decl_var(name, type, init, loc);
    declaration->storage = storage;
    declaration->var_is_thread_local = is_thread_local;
    declaration->var_is_constexpr = is_constexpr;
    declaration->var_is_constinit = is_constinit;
    declaration->var_is_inline = is_inline;
    if (parser_cxx_mode && is_inline &&
        !rcc_parser_cxx_standard_at_least(17)) {
        rcc_error(loc, "inline variables require C++17 or newer");
    }
    if (is_noreturn) {
        rcc_error(loc, "_Noreturn declaration must declare a function");
    }
    if (is_consteval) {
        rcc_error(loc, "consteval declaration must declare a function");
    }
    if (parser_cxx_mode && rcc_parser_cxx_add_value_binding) {
        rcc_parser_cxx_add_value_binding(name, type);
    }
    return declaration;
}

static const char* parser_parse_declarator_asm_name(StorageClass storage,
                                                    Type* type,
                                                    SourceLoc loc) {
    Token* name;
    if (!match(TOK_ASM)) return NULL;
    if (storage == STORAGE_REGISTER && type && type->kind == TYPE_FUNC) {
        rcc_error(loc,
                  "GNU register asm name cannot be applied to a function");
    }
    if (storage != STORAGE_REGISTER &&
        (!type || type->kind != TYPE_FUNC)) {
        rcc_error(loc,
                  "GNU asm names on declarations are supported for functions and register variables only");
    }
    expect(TOK_LPAREN, "(");
    name = expect(TOK_STRING_LIT, "register name string literal");
    expect(TOK_RPAREN, ")");
    return name ? name->value.str_val : NULL;
}

static Decl* parser_make_function_decl(const char* name, Type* type,
                                       DeclList* parameters, Stmt* body,
                                       SourceLoc loc, StorageClass storage,
                                       bool is_inline, bool is_constexpr,
                                       bool is_noreturn, bool is_noexcept,
                                       Expr* noexcept_expr,
                                       bool is_consteval) {
    Decl* declaration = decl_func(name, type, parameters, body, loc);
    declaration->storage = storage;
    declaration->func_is_inline = is_inline;
    declaration->func_is_constexpr = is_constexpr;
    declaration->func_is_noreturn = is_noreturn;
    declaration->func_is_noexcept = is_noexcept;
    declaration->func_noexcept_expr = noexcept_expr;
    declaration->func_is_consteval = is_consteval;
    return declaration;
}

static void parser_apply_cxx_declaration_attributes(
    Decl* declaration, bool nodiscard, bool deprecated,
    const char* deprecated_message) {
    if (!declaration) return;
    if (declaration->kind == DECL_FUNC) {
        declaration->func_is_nodiscard = nodiscard;
        declaration->func_is_deprecated = deprecated;
        declaration->func_deprecated_message = deprecated_message;
    } else if (declaration->kind == DECL_VAR) {
        if (nodiscard) {
            rcc_error(declaration->loc,
                      "[[nodiscard]] requires a function or type declaration");
        }
        declaration->var_is_deprecated = deprecated;
        declaration->var_deprecated_message = deprecated_message;
    }
}

static void parser_apply_c_nodiscard_attribute(
    Decl* declaration, bool nodiscard, const char* message) {
    if (!nodiscard) return;
    if (declaration && declaration->kind == DECL_FUNC) {
        declaration->func_is_nodiscard = true;
        if (!declaration->func_nodiscard_message) {
            declaration->func_nodiscard_message = message;
        }
        return;
    }
    rcc_error(declaration ? declaration->loc : peek()->loc,
              "C23 [[nodiscard]] requires a function or type definition");
}

static void parser_apply_c_deprecated_attribute(
    Decl* declaration, bool deprecated, const char* message) {
    if (!deprecated) return;
    if (declaration && declaration->kind == DECL_FUNC) {
        declaration->func_is_deprecated = true;
        if (!declaration->func_deprecated_message) {
            declaration->func_deprecated_message = message;
        }
    } else if (declaration && declaration->kind == DECL_VAR) {
        declaration->var_is_deprecated = true;
        if (!declaration->var_deprecated_message) {
            declaration->var_deprecated_message = message;
        }
    } else {
        rcc_error(declaration ? declaration->loc : peek()->loc,
                  "C23 [[deprecated]] currently requires an object or function declaration");
    }
}

static void parser_apply_c_maybe_unused_attribute(
    Decl* declaration, bool maybe_unused) {
    if (!maybe_unused) return;
    if (declaration && (declaration->kind == DECL_FUNC ||
                        declaration->kind == DECL_VAR ||
                        declaration->kind == DECL_PARAM ||
                        declaration->kind == DECL_TYPEDEF)) {
        declaration->c23_is_maybe_unused = true;
        return;
    }
    rcc_error(declaration ? declaration->loc : peek()->loc,
              "C23 [[maybe_unused]] requires an object, function, or typedef declaration");
}

static void parser_apply_cxx_statement_attributes(
    Stmt* statement, bool nodiscard, bool deprecated,
    const char* deprecated_message) {
    if (!statement) return;
    if (statement->kind == STMT_DECL) {
        parser_apply_cxx_declaration_attributes(
            statement->decl, nodiscard, deprecated, deprecated_message);
    } else if (statement->kind == STMT_BLOCK) {
        for (StmtList* item = statement->block_stmts; item; item = item->next) {
            parser_apply_cxx_statement_attributes(
                item->stmt, nodiscard, deprecated, deprecated_message);
        }
    }
}

static void parser_apply_old_style_parameter_decl(
    Decl* parameter_declaration, DeclList* parameters, Type* function_type,
    SourceLoc loc) {
    DeclList* parameter;
    Type* parameter_type;
    if (!parameter_declaration || !parameters || !function_type) return;
    if (parameter_declaration->kind != DECL_VAR &&
        parameter_declaration->kind != DECL_FUNC) {
        rcc_error(loc,
                  "old-style parameter declaration must declare one parameter");
        return;
    }
    parameter = parameters;
    while (parameter &&
           (!parameter->decl->name ||
            strcmp(parameter->decl->name, parameter_declaration->name) != 0)) {
        parameter = parameter->next;
    }
    if (!parameter) {
        rcc_error(parameter_declaration->loc,
                  "old-style parameter declaration names an unknown parameter '%s'",
                  parameter_declaration->name);
        return;
    }
    if (parameter_declaration->kind == DECL_VAR &&
        parameter_declaration->var_init) {
        rcc_error(parameter_declaration->loc,
                  "old-style parameter declaration cannot have an initializer");
    }
    parameter_type = parameter_declaration->type;
    if (parameter_type && parameter_type->kind == TYPE_ARRAY) {
        parameter_type = type_ptr(parameter_type->base);
    } else if (parameter_type && parameter_type->kind == TYPE_FUNC) {
        parameter_type = type_ptr(parameter_type);
    }
    parameter->decl->type = parameter_type;
    {
        TypeParam* type_parameter = function_type->params;
        while (type_parameter && type_parameter->name &&
               strcmp(type_parameter->name, parameter_declaration->name) != 0) {
            type_parameter = type_parameter->next;
        }
        if (type_parameter) type_parameter->type = parameter_type;
    }
}

Stmt* parse_declaration(void) {
    bool is_typedef = false;
    bool is_inline = false;
    bool is_constexpr = false;
    bool is_consteval = false;
    bool is_constinit = false;
    bool is_noreturn = false;
    bool is_maybe_unused = false;
    bool declaration_type_is_reproducible = false;
    bool declaration_type_is_unsequenced = false;
    SourceLoc declaration_type_attribute_loc = {0};
    bool is_nodiscard = false;
    bool is_deprecated = false;
    bool is_noexcept = false;
    Expr* noexcept_expr = NULL;
    bool is_thread_local = false;
    bool is_weak = false;
    bool is_naked = false;
    bool declarator_is_noreturn = false;
    bool declarator_is_maybe_unused = false;
    bool declarator_is_nodiscard = false;
    bool declarator_is_deprecated = false;
    const char* nodiscard_message = NULL;
    const char* declarator_nodiscard_message = NULL;
    const char* deprecated_message = NULL;
    const char* declarator_deprecated_message = NULL;
    bool cxx_is_nodiscard = false;
    bool cxx_is_deprecated = false;
    const char* cxx_deprecated_message = NULL;
    const char* declaration_name = NULL;
    DeclList* parameters = NULL;
    Type* base_type;
    Type* type;
    Decl* declaration;
    const char* register_asm_name = NULL;
    void* friend_signature_context = NULL;
    void* friend_signature_base_alias_mark = NULL;
    void* friend_access_mark = NULL;
    bool friend_access_context_active = false;
    int explicit_alignment = 0;

    if (parser_cxx_mode && rcc_parser_cxx_collect_statement_attributes) {
        (void)rcc_parser_cxx_collect_statement_attributes();
    }
    if (parser_cxx_mode && rcc_parser_cxx_take_nodiscard_attribute) {
        cxx_is_nodiscard = rcc_parser_cxx_take_nodiscard_attribute();
    }
    if (parser_cxx_mode && rcc_parser_cxx_take_deprecated_attribute) {
        cxx_is_deprecated =
            rcc_parser_cxx_take_deprecated_attribute(
                &cxx_deprecated_message);
    }
    if (parser_cxx_mode && rcc_parser_cxx_take_weak_attribute) {
        is_weak = rcc_parser_cxx_take_weak_attribute();
    }
    if (parser_cxx_mode && rcc_parser_cxx_take_no_unique_address_attribute &&
        rcc_parser_cxx_take_no_unique_address_attribute()) {
        rcc_error(peek()->loc,
                  "[[no_unique_address]] requires a class data member");
    }
    if (parser_cxx_mode && rcc_parser_cxx_take_noreturn_attribute) {
        is_noreturn = rcc_parser_cxx_take_noreturn_attribute();
    }

    while (check(TOK__ALIGNAS)) {
        int alignment = parse_explicit_alignment();
        if (alignment > explicit_alignment) explicit_alignment = alignment;
    }

    if (parser_cxx_mode && rcc_parse_cxx_auto_local_declaration &&
        (check(TOK_AUTO) || (check(TOK_CONST) && parser.cur->next &&
                             parser.cur->next->type == TOK_AUTO) ||
         (check(TOK_DECLTYPE) && parser.cur->next &&
          parser.cur->next->type == TOK_LPAREN && parser.cur->next->next &&
          parser.cur->next->next->type == TOK_AUTO &&
          parser.cur->next->next->next &&
          parser.cur->next->next->next->type == TOK_RPAREN))) {
        Stmt* auto_declaration = rcc_parse_cxx_auto_local_declaration();
        if (auto_declaration) {
            parser_apply_cxx_statement_attributes(
                auto_declaration, cxx_is_nodiscard, cxx_is_deprecated,
                cxx_deprecated_message);
            return auto_declaration;
        }
    }

    if (check(TOK_STATIC_ASSERT)) {
        SourceLoc assertion_loc = advance()->loc;
        Expr* condition;
        Token* message = NULL;

        expect(TOK_LPAREN, "(");
        condition = parse_assignment();
        if (match(TOK_COMMA)) {
            message = expect(TOK_STRING_LIT, "static assertion message");
        }
        expect(TOK_RPAREN, ")");
        expect(TOK_SEMICOLON, ";");
        return stmt_decl(decl_static_assert(
            condition, message ? message->value.str_val : NULL,
            assertion_loc), assertion_loc);
    }

    skip_attributes();
    parser_accumulate_c_function_type_attributes(
        &declaration_type_is_reproducible,
        &declaration_type_is_unsequenced,
        &declaration_type_attribute_loc);
    is_noreturn = is_noreturn || take_c_noreturn_attribute();
    parser_accumulate_c_maybe_unused_attribute(&is_maybe_unused);
    parser_accumulate_c_nodiscard_attribute(
        &is_nodiscard, &nodiscard_message);
    parser_accumulate_c_deprecated_attribute(
        &is_deprecated, &deprecated_message);
    is_weak = is_weak || take_weak_attribute();
    is_naked = take_naked_attribute();
    if (!parser_cxx_mode && is_maybe_unused &&
        ((check(TOK_IDENT) && parser.cur->next &&
          parser.cur->next->type == TOK_COLON) ||
         check(TOK_CASE) || check(TOK_DEFAULT))) {
        pending_c_maybe_unused_attribute = true;
        is_maybe_unused = false;
    }
    if (parser_cxx_mode && match(TOK_CONSTEXPR)) is_constexpr = true;
    if (parser_cxx_mode && match(TOK_CONSTEVAL)) {
        is_constexpr = true;
        is_consteval = true;
    }
    if (parser_cxx_mode && match(TOK_CONSTINIT)) is_constinit = true;
    if (parser_cxx_mode && rcc_parse_cxx_qualified_constructor_definition) {
        Stmt* constructor_definition =
            rcc_parse_cxx_qualified_constructor_definition(
                false, is_constexpr, is_consteval, peek()->loc);
        if (constructor_definition) return constructor_definition;
        Stmt* destructor_definition =
            rcc_parse_cxx_qualified_destructor_definition(
                false, is_constexpr, is_consteval, peek()->loc);
        if (destructor_definition) return destructor_definition;
        Stmt* conversion_definition =
            rcc_parse_cxx_qualified_conversion_definition(
                false, is_constexpr, is_consteval, peek()->loc);
        if (conversion_definition) return conversion_definition;
    }
    if (parser_cxx_mode &&
        rcc_parser_cxx_begin_function_signature_access) {
        friend_signature_context =
            rcc_parser_cxx_begin_function_signature_access(
                parser_function_scope_depth == 0);
    }
    if (!is_type_start()) {
        if (is_noreturn) {
            rcc_error(peek()->loc,
                      "C23 [[noreturn]] requires a function declaration");
        }
        if (is_nodiscard) {
            rcc_error(peek()->loc,
                      "C23 [[nodiscard]] requires a function or type definition");
        }
        if (is_deprecated) {
            rcc_error(peek()->loc,
                      "C23 [[deprecated]] requires an object or function declaration");
        }
        if (is_maybe_unused) {
            rcc_error(peek()->loc,
                      "C23 [[maybe_unused]] requires an object, function, typedef, tag, enumerator, member, or label declaration");
        }
        parser_apply_c_function_type_attributes(
            NULL, declaration_type_is_reproducible,
            declaration_type_is_unsequenced,
            declaration_type_attribute_loc);
        parser_apply_pending_c_function_type_attributes(NULL, peek()->loc);
        if (friend_signature_context &&
            rcc_parser_cxx_end_function_signature_access) {
            rcc_parser_cxx_end_function_signature_access(
                friend_signature_context, NULL, NULL, NULL, peek()->loc);
        }
        return parse_statement();
    }

    SourceLoc loc = peek()->loc;
    StorageClass storage = STORAGE_NONE;

    /* Declaration specifiers may legally appear in either order. */
    for (;;) {
        if (match(TOK_TYPEDEF)) is_typedef = true;
        else if (match(TOK_STATIC)) storage = STORAGE_STATIC;
        else if (match(TOK_EXTERN)) storage = STORAGE_EXTERN;
        else if (match(TOK_REGISTER)) storage = STORAGE_REGISTER;
        else if (match(TOK_AUTO)) storage = STORAGE_AUTO;
        else if (match(TOK_THREAD_LOCAL)) is_thread_local = true;
        else if (parser_cxx_mode && match(TOK_CONSTINIT)) is_constinit = true;
        else if (parser_cxx_mode && match(TOK_CONSTEXPR)) is_constexpr = true;
        else if (parser_cxx_mode && match(TOK_CONSTEVAL)) {
            is_constexpr = true;
            is_consteval = true;
        }
        else if (match(TOK__NORETURN)) {
            if (parser_cxx_mode) {
                rcc_error(previous()->loc,
                          "_Noreturn is only valid in C declarations");
            }
            is_noreturn = true;
        }
        else if (check(TOK__ALIGNAS)) {
            int alignment = parse_explicit_alignment();
            if (alignment > explicit_alignment) explicit_alignment = alignment;
        }
        else if (check(TOK___ATTRIBUTE__)) {
            skip_attributes();
            is_noreturn = is_noreturn || take_c_noreturn_attribute();
            is_weak = is_weak || take_weak_attribute();
            is_naked = is_naked || take_naked_attribute();
        } else if (!parser_cxx_mode && check(TOK_LBRACKET) &&
                   check_next(TOK_LBRACKET)) {
            skip_attributes();
            parser_accumulate_c_function_type_attributes(
                &declaration_type_is_reproducible,
                &declaration_type_is_unsequenced,
                &declaration_type_attribute_loc);
            is_noreturn = is_noreturn || take_c_noreturn_attribute();
            parser_accumulate_c_maybe_unused_attribute(&is_maybe_unused);
            parser_accumulate_c_nodiscard_attribute(
                &is_nodiscard, &nodiscard_message);
        }
        else if (match(TOK_INLINE) || match(TOK___INLINE__)) is_inline = true;
        else break;
    }

    if (parser_cxx_mode && rcc_parse_cxx_qualified_constructor_definition) {
        Stmt* constructor_definition =
            rcc_parse_cxx_qualified_constructor_definition(
                is_inline, is_constexpr, is_consteval, loc);
        if (constructor_definition) {
            if (friend_signature_context &&
                rcc_parser_cxx_end_function_signature_access) {
                rcc_parser_cxx_end_function_signature_access(
                    friend_signature_context, NULL, NULL, NULL, loc);
            }
            return constructor_definition;
        }
        Stmt* destructor_definition =
            rcc_parse_cxx_qualified_destructor_definition(
                is_inline, is_constexpr, is_consteval, loc);
        if (destructor_definition) {
            if (friend_signature_context &&
                rcc_parser_cxx_end_function_signature_access) {
                rcc_parser_cxx_end_function_signature_access(
                    friend_signature_context, NULL, NULL, NULL, loc);
            }
            return destructor_definition;
        }
        Stmt* conversion_definition =
            rcc_parse_cxx_qualified_conversion_definition(
                is_inline, is_constexpr, is_consteval, loc);
        if (conversion_definition) {
            if (friend_signature_context &&
                rcc_parser_cxx_end_function_signature_access) {
                rcc_parser_cxx_end_function_signature_access(
                    friend_signature_context, NULL, NULL, NULL, loc);
            }
            return conversion_definition;
        }
    }

    base_type = parse_type_spec();
    if (!base_type) {
        if (friend_signature_context &&
            rcc_parser_cxx_end_function_signature_access) {
            rcc_parser_cxx_end_function_signature_access(
                friend_signature_context, NULL, NULL, NULL, peek()->loc);
        }
        rcc_error(loc, "expected type specifier");
        synchronize();
        return NULL;
    }
    /* GNU attributes may follow the declaration's type specifier and still
     * precede its declarator, as in `void __attribute__((naked)) f(void)`.
     * Consume them here before parse_declarator sees the attribute token as a
     * missing identifier. */
    while (check(TOK___ATTRIBUTE__)) {
        skip_attributes();
        if (!parser_cxx_mode) {
            parser_accumulate_c_function_type_attributes(
                &declaration_type_is_reproducible,
                &declaration_type_is_unsequenced,
                &declaration_type_attribute_loc);
            is_noreturn = is_noreturn || take_c_noreturn_attribute();
            parser_accumulate_c_maybe_unused_attribute(&is_maybe_unused);
            parser_accumulate_c_nodiscard_attribute(
                &is_nodiscard, &nodiscard_message);
            parser_accumulate_c_deprecated_attribute(
                &is_deprecated, &deprecated_message);
        }
        is_weak = is_weak || take_weak_attribute();
        is_naked = is_naked || take_naked_attribute();
    }
    parser_apply_c_function_type_attributes(
        base_type, declaration_type_is_reproducible,
        declaration_type_is_unsequenced, declaration_type_attribute_loc);
    declaration_type_is_reproducible = false;
    declaration_type_is_unsequenced = false;
    memset(&declaration_type_attribute_loc, 0,
           sizeof(declaration_type_attribute_loc));
    if (friend_signature_context &&
        rcc_parser_cxx_mark_function_signature_alias_uses) {
        friend_signature_base_alias_mark =
            rcc_parser_cxx_mark_function_signature_alias_uses();
    }

    /* C declaration-specifiers may place a function specifier after the type
     * specifier (for example `void _Noreturn f(void)`).  Keep consuming it
     * here instead of letting the declarator parser reinterpret it as a name. */
    while (match(TOK__NORETURN)) {
        if (parser_cxx_mode) {
            rcc_error(previous()->loc,
                      "_Noreturn is only valid in C declarations");
        }
        is_noreturn = true;
    }
    while (!parser_cxx_mode && check(TOK_LBRACKET) &&
           check_next(TOK_LBRACKET)) {
        SourceLoc attribute_loc = peek()->loc;
        bool type_is_reproducible = false;
        bool type_is_unsequenced = false;
        SourceLoc type_attribute_loc = attribute_loc;
        skip_attributes();
        parser_accumulate_c_function_type_attributes(
            &type_is_reproducible, &type_is_unsequenced,
            &type_attribute_loc);
        parser_apply_c_function_type_attributes(
            base_type, type_is_reproducible, type_is_unsequenced,
            type_attribute_loc);
        is_noreturn = is_noreturn || take_c_noreturn_attribute();
        if (take_c_nodiscard_attribute(NULL)) {
            rcc_error(attribute_loc,
                      "C23 [[nodiscard]] requires a function or type definition");
        }
        parser_accumulate_c_maybe_unused_attribute(&is_maybe_unused);
        parser_accumulate_c_deprecated_attribute(
            &is_deprecated, &deprecated_message);
    }

    /* A standalone aggregate declaration has no declarator. Enum constants
     * were registered while parsing its body. */
    if (match(TOK_SEMICOLON)) {
        parser_apply_pending_c_function_type_attributes(base_type, loc);
        if (is_maybe_unused) {
            if (base_type && (base_type->kind == TYPE_STRUCT ||
                              base_type->kind == TYPE_UNION ||
                              base_type->kind == TYPE_ENUM)) {
                base_type->c23_tag_is_maybe_unused = true;
            } else {
                rcc_error(loc,
                          "C23 [[maybe_unused]] requires a declaration of an entity");
            }
        }
        if (is_nodiscard) {
            rcc_error(loc,
                      "C23 [[nodiscard]] requires a function or type definition");
        }
        if (is_deprecated) {
            rcc_error(loc,
                      "C23 [[deprecated]] requires an object or function declaration");
        }
        if (friend_signature_context &&
            rcc_parser_cxx_end_function_signature_access) {
            rcc_parser_cxx_end_function_signature_access(
                friend_signature_context, NULL, NULL, NULL, loc);
        }
        return stmt_null(loc);
    }

    if (parser_cxx_mode && check(TOK_STAR)) {
        Token* probe = parser.cur;
        bool leads_to_operator = false;
        while (probe && probe->type == TOK_STAR) {
            probe = probe->next;
            while (probe && (probe->type == TOK_CONST ||
                             probe->type == TOK_VOLATILE ||
                             probe->type == TOK_RESTRICT)) {
                probe = probe->next;
            }
        }
        leads_to_operator = probe && probe->type == TOK_OPERATOR;
        if (leads_to_operator) {
            Type* pointer_return_type = base_type;
            do {
                Type* pointer_type;
                bool is_const = false;
                bool is_volatile = false;
                bool is_restrict = false;
                advance();
                pointer_type = type_ptr(pointer_return_type);
                while (match(TOK_CONST) || match(TOK_VOLATILE) ||
                       match(TOK_RESTRICT)) {
                    if (previous()->type == TOK_CONST) is_const = true;
                    else if (previous()->type == TOK_VOLATILE) {
                        is_volatile = true;
                    } else {
                        is_restrict = true;
                    }
                }
                pointer_type->is_const = is_const;
                pointer_type->is_volatile = is_volatile;
                pointer_type->is_restrict = is_restrict;
                pointer_return_type = pointer_type;
            } while (check(TOK_STAR));
            base_type = pointer_return_type;
        }
    }
    if (parser_cxx_mode && rcc_parse_cxx_operator_declaration &&
        check(TOK_OPERATOR)) {
        Stmt* operator_declaration = rcc_parse_cxx_operator_declaration(
            base_type, storage, loc);
        if (operator_declaration) {
            Decl* operator_decl = operator_declaration->kind == STMT_DECL
                ? operator_declaration->decl : NULL;
            if (friend_signature_context &&
                rcc_parser_cxx_end_function_signature_access) {
                rcc_parser_cxx_end_function_signature_access(
                    friend_signature_context,
                    operator_decl ? operator_decl->name : NULL,
                    operator_decl ? operator_decl->type : NULL,
                    NULL, loc);
            }
            return operator_declaration;
        }
    }

    if (!is_typedef && parser_cxx_mode &&
        rcc_parse_cxx_qualified_member_definition) {
        Stmt* member_definition =
            rcc_parse_cxx_qualified_member_definition(
                base_type, is_inline, is_constexpr, is_consteval, loc);
        if (member_definition) {
            if (friend_signature_context &&
                rcc_parser_cxx_end_function_signature_access) {
                rcc_parser_cxx_end_function_signature_access(
                    friend_signature_context, NULL, NULL, NULL, loc);
            }
            return member_definition;
        }
    }

    /* C++ direct initialization uses a different declarator grammar from C:
     * `Pair value(7, 11);` is an object declaration, not a function returning
     * Pair.  Let the C++ parser consume this form only after the declaration
     * specifiers have been collected, so storage qualifiers are preserved. */
    if (!is_typedef && parser_cxx_mode &&
        rcc_parse_cxx_class_local_declaration) {
        Stmt* class_declaration = rcc_parse_cxx_class_local_declaration(
            base_type, storage, is_inline, is_constexpr, is_constinit,
            is_thread_local, loc);
        if (class_declaration) {
            if (friend_signature_context &&
                rcc_parser_cxx_end_function_signature_access) {
                rcc_parser_cxx_end_function_signature_access(
                    friend_signature_context, NULL, NULL, NULL, loc);
            }
            return class_declaration;
        }
    }

    if (!is_typedef && parser_cxx_mode &&
        rcc_parse_cxx_qualified_data_definition) {
        Stmt* qualified_definition = rcc_parse_cxx_qualified_data_definition(
            base_type, storage, is_inline, is_constexpr, is_constinit,
            is_thread_local, loc);
        if (qualified_definition) {
            if (friend_signature_context &&
                rcc_parser_cxx_end_function_signature_access) {
                rcc_parser_cxx_end_function_signature_access(
                    friend_signature_context, NULL, NULL, NULL, loc);
            }
            return qualified_definition;
        }
    }

    type = parse_declarator(base_type, &declaration_name, &parameters);
    type = parse_declarator_attributes(type);
    register_asm_name = parser_parse_declarator_asm_name(storage, type, loc);
    is_weak = is_weak || take_weak_attribute();
    type = apply_explicit_alignment(type, explicit_alignment, loc);
    if (parser_cxx_mode && type && type->kind == TYPE_FUNC &&
        match(TOK_NOEXCEPT)) {
        if (match(TOK_LPAREN)) {
            int64_t noexcept_value = 0;
            noexcept_expr = parse_expression();
            expect(TOK_RPAREN, ")");
            if (noexcept_expr &&
                eval_integer_constant(noexcept_expr, &noexcept_value)) {
                is_noexcept = noexcept_value != 0;
            } else if (noexcept_expr &&
                       (!noexcept_expr->type ||
                        !noexcept_expr->type->cxx_dependent)) {
                rcc_error(noexcept_expr->loc,
                          "function noexcept requires a constant expression");
            }
        } else {
            is_noexcept = true;
        }
        type->function_is_noexcept = is_noexcept;
    }
    skip_attributes();
    parser_apply_pending_c_function_type_attributes(type, loc);
    declarator_is_noreturn = take_c_noreturn_attribute();
    parser_accumulate_c_maybe_unused_attribute(
        &declarator_is_maybe_unused);
    parser_accumulate_c_nodiscard_attribute(
        &declarator_is_nodiscard, &declarator_nodiscard_message);
    parser_accumulate_c_deprecated_attribute(
        &declarator_is_deprecated, &declarator_deprecated_message);
    is_weak = is_weak || take_weak_attribute();
    is_naked = is_naked || take_naked_attribute();
    if (friend_signature_context) {
        if (is_typedef || !type || type->kind != TYPE_FUNC) {
            if (rcc_parser_cxx_end_function_signature_access) {
                rcc_parser_cxx_end_function_signature_access(
                    friend_signature_context, NULL, NULL, NULL, loc);
            }
            friend_signature_context = NULL;
        } else if (check(TOK_LBRACE)) {
            if (rcc_parser_cxx_end_function_signature_access) {
                rcc_parser_cxx_end_function_signature_access(
                    friend_signature_context, declaration_name, type,
                    parameters, loc);
            }
            friend_signature_context = NULL;
        } else if (rcc_parser_cxx_validate_function_signature_access) {
            rcc_parser_cxx_validate_function_signature_access(
                friend_signature_context, declaration_name, type,
                parameters, loc);
        }
    }
    if (!declaration_name) {
        if (friend_signature_context &&
            rcc_parser_cxx_restore_function_signature_access) {
            rcc_parser_cxx_restore_function_signature_access(
                friend_signature_context);
            friend_signature_context = NULL;
        }
        rcc_error(loc, "expected identifier");
        synchronize();
        return NULL;
    }

    if (is_typedef) {
        StmtList* declarations = NULL;
        bool typedef_is_maybe_unused = is_maybe_unused ||
            declarator_is_maybe_unused ||
            parser_current_type_is_maybe_unused(declaration_name);
        const char* previous_typedef_message = NULL;
        bool previous_typedef_is_deprecated =
            parser_current_type_is_deprecated(
                declaration_name, &previous_typedef_message);
        bool typedef_is_deprecated = is_deprecated ||
            declarator_is_deprecated || previous_typedef_is_deprecated;
        const char* typedef_deprecated_message =
            declarator_deprecated_message
                ? declarator_deprecated_message
                : deprecated_message ? deprecated_message
                                     : previous_typedef_message;
        if (is_nodiscard || declarator_is_nodiscard) {
            rcc_error(loc,
                      "C23 [[nodiscard]] requires a function or type definition");
        }
        if (is_noreturn || declarator_is_noreturn) {
            rcc_error(loc,
                      "C23 [[noreturn]] requires a function declaration");
        }
        if (is_thread_local) {
            rcc_error(loc, "thread-local storage is not valid on a typedef");
        }
        if (is_constinit) {
            rcc_error(loc, "constinit declaration must declare a variable");
        }
        parser_define_type(declaration_name, type);
        if (typedef_is_maybe_unused) {
            parser_mark_latest_type_maybe_unused(declaration_name);
        }
        if (typedef_is_deprecated) {
            parser_mark_latest_type_deprecated(
                declaration_name, typedef_deprecated_message);
        }
        Decl* typedef_declaration = decl_typedef(
            declaration_name, type, loc);
        typedef_declaration->typedef_is_deprecated =
            typedef_is_deprecated;
        typedef_declaration->typedef_deprecated_message =
            typedef_deprecated_message;
        parser_apply_c_maybe_unused_attribute(
            typedef_declaration, typedef_is_maybe_unused);
        stmtlist_append(&declarations,
                        stmt_decl(typedef_declaration, loc));
        while (match(TOK_COMMA)) {
            const char* next_name = NULL;
            DeclList* next_parameters = NULL;
            bool next_is_deprecated = is_deprecated;
            bool next_is_maybe_unused = is_maybe_unused;
            const char* next_deprecated_message = deprecated_message;
            Type* next_type = parse_declarator(
                base_type, &next_name, &next_parameters);
            (void)next_parameters;
            next_type = parse_declarator_attributes(next_type);
            next_type = apply_explicit_alignment(
                next_type, explicit_alignment, loc);
            skip_attributes();
            parser_apply_pending_c_function_type_attributes(next_type, loc);
            parser_accumulate_c_maybe_unused_attribute(
                &next_is_maybe_unused);
            next_is_maybe_unused = next_is_maybe_unused ||
                parser_current_type_is_maybe_unused(next_name);
            parser_accumulate_c_deprecated_attribute(
                &next_is_deprecated, &next_deprecated_message);
            if (!next_name) {
                rcc_error(loc, "expected identifier in declaration list");
                synchronize();
                break;
            }
            {
                const char* previous_message = NULL;
                bool was_deprecated = parser_current_type_is_deprecated(
                    next_name, &previous_message);
                next_is_deprecated = next_is_deprecated || was_deprecated;
                if (!next_deprecated_message) {
                    next_deprecated_message = previous_message;
                }
            }
            parser_define_type(next_name, next_type);
            if (next_is_maybe_unused) {
                parser_mark_latest_type_maybe_unused(next_name);
            }
            if (next_is_deprecated) {
                parser_mark_latest_type_deprecated(
                    next_name, next_deprecated_message);
            }
            Decl* next_typedef_declaration = decl_typedef(
                next_name, next_type, loc);
            next_typedef_declaration->typedef_is_deprecated =
                next_is_deprecated;
            next_typedef_declaration->typedef_deprecated_message =
                next_deprecated_message;
            parser_apply_c_maybe_unused_attribute(
                next_typedef_declaration, next_is_maybe_unused);
            stmtlist_append(&declarations,
                            stmt_decl(next_typedef_declaration, loc));
        }
        expect(TOK_SEMICOLON, ";");
        return parser_declaration_list_result(declarations, loc);
    }

    /* Function declaration? */
    if (type->kind == TYPE_FUNC) {
        Stmt* body = NULL;
        if (is_constinit) {
            rcc_error(loc, "constinit declaration must declare a variable");
        }
        if (is_thread_local) {
            rcc_error(loc, "thread-local storage is not valid on a function");
        }
        /* In an old-style definition, parameter declarations appear between
         * the identifier-list declarator and the compound statement.  Parse
         * them through the ordinary declaration path, then transfer their
         * adjusted types to both the Decl parameters and the function type's
         * ABI parameter list. */
        if (!parser_cxx_mode && !type->has_prototype && parameters &&
            !check(TOK_LBRACE) && !check(TOK_SEMICOLON)) {
            while (!check(TOK_LBRACE) && !check(TOK_SEMICOLON) &&
                   !at_end()) {
                Stmt* parameter_statement;
                if (!is_type_start()) {
                    rcc_error(peek()->loc,
                              "expected old-style parameter declaration");
                    synchronize();
                    break;
                }
                parameter_statement = parse_declaration();
                if (!parameter_statement) {
                    rcc_error(loc,
                              "old-style parameter declaration must declare one parameter");
                    continue;
                }
                if (parameter_statement->kind == STMT_BLOCK &&
                    parameter_statement->block_no_scope) {
                    for (StmtList* item = parameter_statement->block_stmts;
                         item; item = item->next) {
                        if (item->stmt && item->stmt->kind == STMT_DECL) {
                            parser_apply_old_style_parameter_decl(
                                item->stmt->decl, parameters, type, loc);
                        } else {
                            rcc_error(loc,
                                      "old-style parameter declaration must declare one parameter");
                        }
                    }
                } else if (parameter_statement->kind == STMT_DECL) {
                    parser_apply_old_style_parameter_decl(
                        parameter_statement->decl, parameters, type, loc);
                } else {
                    rcc_error(loc,
                              "old-style parameter declaration must declare one parameter");
                }
            }
        }
        if (match(TOK_LBRACE)) {
            if (parser_cxx_mode &&
                rcc_parser_cxx_begin_function_friend_access) {
                friend_access_mark =
                    rcc_parser_cxx_begin_function_friend_access(
                        declaration_name, type);
                friend_access_context_active = true;
            }
            if (parser_cxx_mode && rcc_parser_cxx_begin_function_parameters) {
                rcc_parser_cxx_begin_function_parameters(parameters);
            }
            rcc_parser_function_scope_push(declaration_name);
            body = parse_block();
            rcc_parser_function_scope_pop();
            if (friend_access_context_active &&
                rcc_parser_cxx_end_function_friend_access) {
                rcc_parser_cxx_end_function_friend_access(
                    friend_access_mark);
            }
            if (parser_cxx_mode && rcc_parser_cxx_end_function_parameters) {
                rcc_parser_cxx_end_function_parameters();
            }
        } else {
            StmtList* declarations = NULL;
            declaration = parser_make_function_decl(
                declaration_name, type, parameters, NULL, loc, storage,
                is_inline, is_constexpr,
                is_noreturn || declarator_is_noreturn, is_noexcept,
                noexcept_expr, is_consteval);
            if (register_asm_name) {
                declaration->link_name = register_asm_name;
            }
            parser_apply_cxx_declaration_attributes(
                declaration, cxx_is_nodiscard, cxx_is_deprecated,
                cxx_deprecated_message);
            parser_apply_c_nodiscard_attribute(
                declaration, is_nodiscard || declarator_is_nodiscard,
                declarator_nodiscard_message
                    ? declarator_nodiscard_message : nodiscard_message);
            parser_apply_c_deprecated_attribute(
                declaration, is_deprecated || declarator_is_deprecated,
                declarator_deprecated_message
                    ? declarator_deprecated_message : deprecated_message);
            parser_apply_c_maybe_unused_attribute(
                declaration, is_maybe_unused || declarator_is_maybe_unused);
            declaration->is_weak = is_weak;
            stmtlist_append(&declarations, stmt_decl(declaration, loc));
            while (match(TOK_COMMA)) {
                const char* next_name = NULL;
                DeclList* next_parameters = NULL;
                bool next_is_weak;
                bool next_is_noreturn;
                bool next_is_maybe_unused = is_maybe_unused;
                bool next_is_nodiscard = is_nodiscard;
                bool next_is_deprecated = is_deprecated;
                const char* next_nodiscard_message = nodiscard_message;
                const char* next_deprecated_message = deprecated_message;
                if (friend_signature_context &&
                    rcc_parser_cxx_reset_function_signature_alias_uses) {
                    rcc_parser_cxx_reset_function_signature_alias_uses(
                        friend_signature_base_alias_mark);
                }
                skip_attributes();
                next_is_weak = take_weak_attribute();
                next_is_noreturn =
                    is_noreturn || take_c_noreturn_attribute();
                parser_accumulate_c_maybe_unused_attribute(
                    &next_is_maybe_unused);
                parser_accumulate_c_nodiscard_attribute(
                    &next_is_nodiscard, &next_nodiscard_message);
                parser_accumulate_c_deprecated_attribute(
                    &next_is_deprecated, &next_deprecated_message);
                Type* next_type = parse_declarator(
                    base_type, &next_name, &next_parameters);
                next_type = parse_declarator_attributes(next_type);
                skip_attributes();
                parser_apply_pending_c_function_type_attributes(
                    next_type, loc);
                next_is_noreturn =
                    next_is_noreturn || take_c_noreturn_attribute();
                parser_accumulate_c_maybe_unused_attribute(
                    &next_is_maybe_unused);
                parser_accumulate_c_nodiscard_attribute(
                    &next_is_nodiscard, &next_nodiscard_message);
                parser_accumulate_c_deprecated_attribute(
                    &next_is_deprecated, &next_deprecated_message);
                const char* next_register_asm_name =
                    parser_parse_declarator_asm_name(storage, next_type, loc);
                next_is_weak = next_is_weak || take_weak_attribute();
                next_type = apply_explicit_alignment(
                    next_type, explicit_alignment, loc);
                if (!next_name) {
                    if (friend_signature_context &&
                        rcc_parser_cxx_restore_function_signature_access) {
                        rcc_parser_cxx_restore_function_signature_access(
                            friend_signature_context);
                        friend_signature_context = NULL;
                    }
                    rcc_error(loc, "expected identifier in declaration list");
                    synchronize();
                    break;
                }
                if (friend_signature_context &&
                    rcc_parser_cxx_validate_function_signature_access) {
                    rcc_parser_cxx_validate_function_signature_access(
                        friend_signature_context,
                        next_type && next_type->kind == TYPE_FUNC
                            ? next_name : NULL,
                        next_type && next_type->kind == TYPE_FUNC
                            ? next_type : NULL,
                        next_parameters, loc);
                }
                if (next_type && next_type->kind == TYPE_FUNC) {
                    Decl* next_declaration;
                    if (is_thread_local) {
                        rcc_error(loc,
                                  "thread-local storage is not valid on a function");
                    }
                    next_declaration = parser_make_function_decl(
                        next_name, next_type, next_parameters, NULL, loc,
                        storage, is_inline, is_constexpr, next_is_noreturn,
                        is_noexcept, noexcept_expr, is_consteval);
                    if (next_register_asm_name) {
                        next_declaration->link_name = next_register_asm_name;
                    }
                    parser_apply_cxx_declaration_attributes(
                        next_declaration, cxx_is_nodiscard,
                        cxx_is_deprecated, cxx_deprecated_message);
                    parser_apply_c_nodiscard_attribute(
                        next_declaration, next_is_nodiscard,
                        next_nodiscard_message);
                    parser_apply_c_deprecated_attribute(
                        next_declaration, next_is_deprecated,
                        next_deprecated_message);
                    parser_apply_c_maybe_unused_attribute(
                        next_declaration, next_is_maybe_unused);
                    next_declaration->is_weak = next_is_weak;
                    stmtlist_append(&declarations,
                                    stmt_decl(next_declaration, loc));
                } else {
                    Expr* next_init = parser_parse_variable_initializer(
                        next_type, loc);
                    Decl* next_declaration = parser_make_variable_decl(
                        next_name, next_type, next_init, loc, storage,
                        is_thread_local, is_constexpr, is_constinit,
                        is_inline, next_is_noreturn, is_consteval);
                    next_declaration->var_register_name =
                        next_register_asm_name;
                    parser_apply_cxx_declaration_attributes(
                        next_declaration, cxx_is_nodiscard,
                        cxx_is_deprecated, cxx_deprecated_message);
                    parser_apply_c_nodiscard_attribute(
                        next_declaration, next_is_nodiscard,
                        next_nodiscard_message);
                    parser_apply_c_deprecated_attribute(
                        next_declaration, next_is_deprecated,
                        next_deprecated_message);
                    parser_apply_c_maybe_unused_attribute(
                        next_declaration, next_is_maybe_unused);
                    next_declaration->is_weak = next_is_weak;
                    stmtlist_append(&declarations,
                                    stmt_decl(next_declaration, loc));
                }
            }
            expect(TOK_SEMICOLON, ";");
            if (friend_signature_context &&
                rcc_parser_cxx_restore_function_signature_access) {
                rcc_parser_cxx_restore_function_signature_access(
                    friend_signature_context);
                friend_signature_context = NULL;
            }
            return parser_declaration_list_result(declarations, loc);
        }

        declaration = parser_make_function_decl(
            declaration_name, type, parameters, body, loc, storage,
            is_inline, is_constexpr,
            is_noreturn || declarator_is_noreturn, is_noexcept,
            noexcept_expr, is_consteval);
        if (register_asm_name) {
            declaration->link_name = register_asm_name;
        }
        parser_apply_cxx_declaration_attributes(
            declaration, cxx_is_nodiscard, cxx_is_deprecated,
            cxx_deprecated_message);
        parser_apply_c_nodiscard_attribute(
            declaration, is_nodiscard || declarator_is_nodiscard,
            declarator_nodiscard_message
                ? declarator_nodiscard_message : nodiscard_message);
        parser_apply_c_deprecated_attribute(
            declaration, is_deprecated || declarator_is_deprecated,
            declarator_deprecated_message
                ? declarator_deprecated_message : deprecated_message);
        parser_apply_c_maybe_unused_attribute(
            declaration, is_maybe_unused || declarator_is_maybe_unused);
        declaration->is_weak = is_weak;
        declaration->func_is_naked = is_naked;
        return stmt_decl(declaration, loc);
    }

    /* Variable declarators share the declaration specifiers but retain their
     * own pointer/array type and initializer. */
    {
        StmtList* declarations = NULL;
        Expr* init = parser_parse_variable_initializer(type, loc);
        declaration = parser_make_variable_decl(
            declaration_name, type, init, loc, storage, is_thread_local,
            is_constexpr, is_constinit, is_inline,
            is_noreturn || declarator_is_noreturn, is_consteval);
        declaration->var_register_name = register_asm_name;
        parser_apply_cxx_declaration_attributes(
            declaration, cxx_is_nodiscard, cxx_is_deprecated,
            cxx_deprecated_message);
        parser_apply_c_nodiscard_attribute(
            declaration, is_nodiscard || declarator_is_nodiscard,
            declarator_nodiscard_message
                ? declarator_nodiscard_message : nodiscard_message);
        parser_apply_c_deprecated_attribute(
            declaration, is_deprecated || declarator_is_deprecated,
            declarator_deprecated_message
                ? declarator_deprecated_message : deprecated_message);
        parser_apply_c_maybe_unused_attribute(
            declaration, is_maybe_unused || declarator_is_maybe_unused);
        declaration->is_weak = is_weak;
        stmtlist_append(&declarations, stmt_decl(declaration, loc));

        while (match(TOK_COMMA)) {
            const char* next_name = NULL;
            DeclList* next_parameters = NULL;
            bool next_is_weak;
            bool next_is_noreturn;
            bool next_is_maybe_unused = is_maybe_unused;
            bool next_is_nodiscard = is_nodiscard;
            bool next_is_deprecated = is_deprecated;
            const char* next_nodiscard_message = nodiscard_message;
            const char* next_deprecated_message = deprecated_message;
            skip_attributes();
            next_is_weak = take_weak_attribute();
            next_is_noreturn = is_noreturn || take_c_noreturn_attribute();
            parser_accumulate_c_maybe_unused_attribute(
                &next_is_maybe_unused);
            parser_accumulate_c_nodiscard_attribute(
                &next_is_nodiscard, &next_nodiscard_message);
            parser_accumulate_c_deprecated_attribute(
                &next_is_deprecated, &next_deprecated_message);
            Type* next_type = parse_declarator(
                base_type, &next_name, &next_parameters);
            next_type = parse_declarator_attributes(next_type);
            skip_attributes();
            parser_apply_pending_c_function_type_attributes(next_type, loc);
            next_is_noreturn =
                next_is_noreturn || take_c_noreturn_attribute();
            parser_accumulate_c_maybe_unused_attribute(
                &next_is_maybe_unused);
            parser_accumulate_c_nodiscard_attribute(
                &next_is_nodiscard, &next_nodiscard_message);
            parser_accumulate_c_deprecated_attribute(
                &next_is_deprecated, &next_deprecated_message);
            const char* next_register_asm_name =
                parser_parse_declarator_asm_name(storage, next_type, loc);
            next_is_weak = next_is_weak || take_weak_attribute();
            next_type = apply_explicit_alignment(
                next_type, explicit_alignment, loc);
            if (!next_name) {
                rcc_error(loc, "expected identifier in declaration list");
                synchronize();
                break;
            }
            if (next_type && next_type->kind == TYPE_FUNC) {
                Decl* next_declaration;
                if (is_thread_local) {
                    rcc_error(loc,
                              "thread-local storage is not valid on a function");
                }
                next_declaration = decl_func(next_name, next_type,
                                             next_parameters, NULL, loc);
                if (next_register_asm_name) {
                    next_declaration->link_name = next_register_asm_name;
                }
                parser_apply_cxx_declaration_attributes(
                    next_declaration, cxx_is_nodiscard, cxx_is_deprecated,
                    cxx_deprecated_message);
                parser_apply_c_nodiscard_attribute(
                    next_declaration, next_is_nodiscard,
                    next_nodiscard_message);
                parser_apply_c_deprecated_attribute(
                    next_declaration, next_is_deprecated,
                    next_deprecated_message);
                parser_apply_c_maybe_unused_attribute(
                    next_declaration, next_is_maybe_unused);
                next_declaration->is_weak = next_is_weak;
                stmtlist_append(&declarations,
                                stmt_decl(next_declaration, loc));
                continue;
            }
            init = parser_parse_variable_initializer(next_type, loc);
            declaration = parser_make_variable_decl(
                next_name, next_type, init, loc, storage, is_thread_local,
                is_constexpr, is_constinit, is_inline, next_is_noreturn,
                is_consteval);
            declaration->var_register_name = next_register_asm_name;
            parser_apply_cxx_declaration_attributes(
                declaration, cxx_is_nodiscard, cxx_is_deprecated,
                cxx_deprecated_message);
            parser_apply_c_nodiscard_attribute(
                declaration, next_is_nodiscard,
                next_nodiscard_message);
            parser_apply_c_deprecated_attribute(
                declaration, next_is_deprecated,
                next_deprecated_message);
            parser_apply_c_maybe_unused_attribute(
                declaration, next_is_maybe_unused);
            declaration->is_weak = next_is_weak;
            stmtlist_append(&declarations, stmt_decl(declaration, loc));
        }
        expect(TOK_SEMICOLON, ";");
        return parser_declaration_list_result(declarations, loc);
    }
}

/* ═══════════════════════════════════════
 * Top-level Parsing
 * ═══════════════════════════════════════ */

static Stmt* parse_toplevel(void) {
    if (match(TOK_PRAGMA_PACK)) {
        parser_apply_pack(previous());
        return NULL;
    }
    Stmt* s = parse_declaration();
    parser_apply_pending_c_function_type_attributes(
        NULL, s ? s->loc : peek()->loc);
    if (s && s->kind == STMT_NULL && s->c23_is_fallthrough) {
        rcc_error(s->loc,
                  "C23 [[fallthrough]] requires an enclosing switch statement");
    }
    return s;
}

/* Main parser function */
AST* rcc_parse(TokenList* tokens) {
    rcc_parser_set_cxx_mode(false);
    rcc_parser_initialize_builtin_va_list_type();
    pending_weak_attribute = false;
    pending_naked_attribute = false;
    pending_c_noreturn_attribute = false;
    pending_c_maybe_unused_attribute = false;
    pending_c_fallthrough_attribute = false;
    memset(&pending_c_fallthrough_loc, 0,
           sizeof(pending_c_fallthrough_loc));
    pending_c_function_is_reproducible = false;
    pending_c_function_is_unsequenced = false;
    memset(&pending_c_function_type_attribute_loc, 0,
           sizeof(pending_c_function_type_attribute_loc));
    pending_c_nodiscard_attribute = false;
    pending_c_nodiscard_message = NULL;
    pending_c_deprecated_attribute = false;
    pending_c_deprecated_message = NULL;
    parser.cur = tokens->head;
    parser.prev = NULL;
    rcc_parser_reset_type_scopes();
    parser_pack_alignment = 0;
    parser_aggregate_pack_alignment = 0;
    parser_pack_depth = 0;

    AST* ast = ast_new();
    Token* preserved_recovery_boundary = NULL;

    while (!at_end()) {
        Token* iteration_start = parser.cur;
        int errors_before = g_error_count;
        bool preserve_recovery_boundary = false;
        Stmt* s = parse_toplevel();
        if (s && s->kind == STMT_DECL) {
            ast_add_decl(ast, s->decl);
        } else if (s && s->kind == STMT_BLOCK && s->block_no_scope) {
            for (StmtList* item = s->block_stmts; item; item = item->next) {
                if (item->stmt && item->stmt->kind == STMT_DECL) {
                    ast_add_decl(ast, item->stmt->decl);
                }
            }
        }
        if (g_error_count > errors_before && parser.cur == iteration_start) {
            if (parser_is_recovery_declaration_start(parser.cur) &&
                preserved_recovery_boundary != parser.cur) {
                preserved_recovery_boundary = parser.cur;
                preserve_recovery_boundary = true;
            } else {
                synchronize();
                preserved_recovery_boundary = NULL;
            }
        } else if (parser.cur != iteration_start ||
                   g_error_count == errors_before) {
            preserved_recovery_boundary = NULL;
        }
        if (parser.cur == iteration_start && !at_end() &&
            !preserve_recovery_boundary) {
            advance();
        }
    }

    return ast;
}
