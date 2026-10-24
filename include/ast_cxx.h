/*
 * RCC++ - RinOS C++ Compiler
 * C++ AST Extensions
 */

#ifndef AST_CXX_H
#define AST_CXX_H

#include "ast.h"

/* Forward declarations */
typedef struct CxxClass CxxClass;
typedef struct CxxNamespace CxxNamespace;
typedef struct CxxTemplate CxxTemplate;
typedef struct CxxMethod CxxMethod;
typedef struct CxxConstructorInfo CxxConstructorInfo;
typedef struct CxxConstructorInitializer CxxConstructorInitializer;
typedef struct CxxDeductionGuide CxxDeductionGuide;
typedef struct CxxVtableEntry CxxVtableEntry;
typedef struct CxxSecondaryVtable CxxSecondaryVtable;
typedef struct CxxTypeAlias CxxTypeAlias;
typedef struct CxxClassAliasTemplate CxxClassAliasTemplate;
typedef struct CxxNestedClassTemplate CxxNestedClassTemplate;
typedef struct CxxLocalClassTemplate CxxLocalClassTemplate;

typedef struct CxxVirtualBaseInfo {
    CxxClass* base;
    int offset;
    bool public_path;
} CxxVirtualBaseInfo;

/* Access specifier */
typedef enum {
    ACCESS_PUBLIC,
    ACCESS_PROTECTED,
    ACCESS_PRIVATE
} AccessSpec;

struct CxxTypeAlias {
    const char* name;
    Type* type;
    AccessSpec access;
    SourceLoc loc;
    CxxTypeAlias* next;
};

struct CxxClassAliasTemplate {
    CxxTemplate* declaration;
    AccessSpec access;
    CxxClassAliasTemplate* next;
};

struct CxxNestedClassTemplate {
    CxxTemplate* declaration;
    AccessSpec access;
    CxxNestedClassTemplate* next;
};

/* Constructor facts retained until all class fields are known.  Only the
 * deliberately small, ABI-transparent subset accepted by parser_cxx.c is
 * lowered through the common aggregate backend. */
struct CxxConstructorInitializer {
    const char* field;
    Type* base_type_pattern;
    Type* inherited_base_type;
    int inherited_base_offset;
    Expr* value;
    /* Parenthesized mem-initializers retain their complete argument list.
     * `value` remains the first argument for the scalar aggregate verifier. */
    ExprList* arguments;
    CxxConstructorInfo* constructor;
    bool is_base_initializer;
    bool is_virtual_base_initializer;
    bool is_delegating_constructor;
    bool is_default_member_initializer;
    bool is_pack_expansion;
    bool has_inherited_base_layout;
    CxxConstructorInitializer* next;
};

struct CxxConstructorInfo {
    CxxMethod* method;
    int parameter_count;
    uint32_t lowerable_parameter_use_mask;
    TypeParam* parameters;
    CxxConstructorInitializer* initializers;
    int initializer_count;
    bool has_explicit_initializers;
    bool initializers_are_supported;
    bool body_is_empty;
    bool lowerable_parameter_use_mask_valid;
    bool is_deleted;
    bool is_defaulted;
    bool is_inherited;
    CxxConstructorInitializer* inherited_base_initializers;
    int inherited_selected_base_offset;
    CxxConstructorInfo* inherited_member_initializers;
    AccessSpec access;
    CxxConstructorInfo* next;
};

/* A bounded user-defined C++17 deduction guide.  The parameter declaration
 * list retains the guide's deduction patterns; return_type is either a
 * concrete class-template specialization or a dependent specialization that
 * is substituted from template_owner during CTAD. */
struct CxxDeductionGuide {
    CxxTemplate* template_owner;
    DeclList* parameters;
    Type* return_type;
    CxxDeductionGuide* next;
};

/* C++ Class/Struct */
struct CxxClass {
    const char* name;
    bool is_struct;          /* struct vs class (default access) */
    bool is_final;           /* C++ final class cannot be used as a base. */
    bool is_deprecated;
    const char* deprecated_message;
    bool is_abstract;        /* A vtable slot remains pure virtual. */
    bool has_user_constructor;
    bool has_nonpublic_field;
    bool has_static_field;
    bool has_field_initializer;
    CxxConstructorInfo* constructors;

    /* Base classes */
    struct {
        CxxClass* base;
        const char* base_name; /* Deferred source spelling, if unresolved. */
        Type* type_pattern; /* Dependent base type retained until substitution. */
        AccessSpec access;
        bool is_virtual;
        bool is_pack_expansion;
    } *bases;
    int base_count;
    /* Explicit `using Base::member` declarations restore a hidden base
     * overload into the derived member lookup set. */
    struct {
        const char* base_name;
        const char* member_name;
        Type* conversion_type; /* Exact target in `using Base::operator T`. */
        Type* base_type_pattern; /* Dependent base retained until substitution. */
        AccessSpec access;
        SourceLoc loc;
    } *using_base_members;
    int using_base_member_count;
    /* Friend class declarations are retained as source-qualified names so a
     * later class definition can gain access without inventing a forward
     * declaration or changing the object ABI. */
    const char** friend_class_names;
    int friend_class_count;
    /* Byte offsets of non-virtual base subobjects after layout. */
    int* base_offsets;
    /* One shared subobject for every virtual base reachable from this class. */
    CxxVirtualBaseInfo* virtual_bases;
    int virtual_base_count;
    /* Size excluding virtual-base subobjects, used for embedding. */
    int nonvirtual_size;
    /* Hidden pointer used by conversions through a virtual-base path.  It is
     * placed at the end of the non-virtual portion so the existing primary
     * vptr ABI remains unchanged for polymorphic classes. */
    int virtual_base_pointer_offset;
    const char* virtual_base_table_symbol;

    /* Members */
    struct CxxMember {
        AccessSpec access;
        Decl* decl;          /* Can be DECL_VAR or DECL_FUNC */
        CxxMethod* method;    /* Non-NULL for parsed C++ methods. */
        bool is_static;
        bool is_virtual;
        bool is_pure_virtual;
        bool is_override;
        bool is_final;
        struct CxxMember* next;
    } *members;

    /* Virtual table info */
    int vtable_size;
    CxxVtableEntry* vtable;
    CxxSecondaryVtable* secondary_vtables;
    int secondary_vtable_count;
    CxxMethod* destructor_method;

    /* Type info */
    Type* type;
    int size;
    int align;
    int explicit_alignment;
    /* Active #pragma pack limit captured when the class definition starts. */
    int pack_alignment;

    /* Fields list (TypeParam*) for struct compatibility */
    TypeParam* fields;
    /* Nested `using Name = Type;` declarations used by dependent type
     * requirements and ordinary qualified type lookup. */
    CxxTypeAlias* type_aliases;
    /* Class-scope alias templates retain their access and template
     * declaration for qualified specialization lookup. */
    CxxClassAliasTemplate* alias_templates;
    /* Class templates declared directly in this class scope. */
    CxxNestedClassTemplate* nested_class_templates;

    /* Namespace context */
    CxxNamespace* ns;
    /* Lexical class owner for nested class declarations. */
    CxxClass* enclosing_class;

    /* Template instantiation info */
    CxxTemplate* templ;
    Type** template_args;
    int template_arg_count;
    int64_t* template_value_args;
    bool* template_value_present;
    /* Expanded integral non-type parameter-pack arguments for the bounded
     * class-template lowering.  The ordinary value arrays remain indexed by
     * declared parameters; these arrays preserve the complete argument
     * sequence when a class has `template<int... Ns>`. */
    int template_pack_count;
    int64_t* template_pack_values;
    bool* template_pack_value_present;
    CxxTemplate* template_identity_tmpl;
    Type** template_identity_args;
    int template_identity_arg_count;
    int64_t* template_identity_value_args;
    bool* template_identity_value_present;
};

/* C++ Namespace */
struct CxxNamespace {
    const char* name;
    CxxNamespace* parent;
    bool is_inline_namespace;
    /* Unnamed namespaces keep their source spelling for lookup/mangling, but
     * RTTI needs a translation-unit identity so internal types never merge
     * across object files. */
    bool is_anonymous_namespace;
    const char* anonymous_typeinfo_identity;

    /* Declarations in this namespace */
    DeclList* decls;

    /* Classes in this namespace */
    CxxClass** classes;
    int class_count;

    /* Templates declared directly in this namespace. */
    CxxTemplate** templates;
    int template_count;

    /* Nested namespaces */
    CxxNamespace* children;
    CxxNamespace* next;      /* sibling */

    /* Namespace aliases preserve the canonical target namespace for lookup;
     * declarations continue to use the target's ABI-qualified spelling. */
    const char** namespace_alias_names;
    CxxNamespace** namespace_alias_targets;
    int namespace_alias_count;

    /* Names introduced by using-directives/declarations.  Semantic analysis
     * consults this metadata instead of manufacturing duplicate symbols. */
    CxxNamespace** using_namespaces;
    int using_namespace_count;
    const char** using_declarations;
    int using_declaration_count;
};

struct CxxVtableEntry {
    const char* name;
    CxxMethod* method;
    int offset;
    const char* entry_symbol;
};

struct CxxSecondaryVtable {
    CxxClass* base;
    int base_index;
    bool is_virtual_base;
    int virtual_base_index;
    const char* symbol;
    int size;
    CxxVtableEntry* entries;
};

/* Template parameter */
typedef struct {
    enum {
        TPARAM_TYPE,         /* typename T */
        TPARAM_NONTYPE,      /* int N */
        TPARAM_TEMPLATE      /* template<...> class T */
    } kind;
    const char* name;
    Type* type;              /* For non-type parameters */
    bool is_pack;            /* `typename... Ts` / supported type pack. */
    bool has_default;
    CxxTemplate* template_signature; /* TPARAM_TEMPLATE parameter list. */
    /* Parameter scope in which a non-type default expression was parsed. */
    CxxTemplate* default_context;
    /* For TPARAM_TEMPLATE, default_type is a class-template carrier. */
    union {
        Type* default_type;
        Expr* default_value;
    };
} TemplateParam;

/* C++ Template */
struct CxxTemplate {
    const char* name;
    CxxNamespace* ns;
    TemplateParam* params;
    int param_count;
    /* Classes granting friendship to a function template.  The list is
     * attached to each instantiated function declaration so normal member
     * access checking applies to that specialization. */
    CxxFriendAccess* friend_access;
    /* Class-scope friends are found by ADL until a namespace declaration
     * makes the template visible to ordinary lookup. */
    bool is_hidden_friend;

    /* Template body - either class or function */
    enum {
        TMPL_CLASS,
        TMPL_FUNCTION,
        TMPL_VARIABLE,
        TMPL_ALIAS,
        TMPL_DEDUCTION_GUIDE
    } kind;
    union {
        CxxClass* class_def;
        Decl* func_def;
        Decl* var_def;
    };
    /* Preserve member qualifiers and owning-class ABI metadata when a
     * function-template specialization is selected. */
    CxxMethod* member_method;

    /* The expanded type of a bounded alias template.  Alias templates do
     * not have an object or function body; their specialization is resolved
     * before semantic analysis sees the enclosing declaration. */
    Type* alias_type;
    /* Templates declared in class scope may refer to their enclosing
     * class-template parameters. Their own parameters occupy the leading
     * substitution slots while the class pattern is being parsed. */
    CxxTemplate* enclosing_template;
    /* Lexical class owner, including when it is not itself a template. */
    CxxClass* enclosing_class;

    bool is_constexpr;
    bool is_noexcept;
    bool is_concept;
    /* A parsed integral requires-clause.  The current frontend accepts
     * constant expressions over non-type template parameters; keeping the
     * expression in the template object lets overload/instantiation code
     * reject unsatisfied specializations before code generation. */
    Expr* constraint;
    /* Parameter names/scope used by the parsed requires-clause. */
    CxxTemplate* constraint_context;
    enum {
        TMPL_FUNCTION_NONE,
        TMPL_FUNCTION_VERSIONED_STRUCT,
    } function_lowering;
    int64_t function_constant;

    /* Alternate storage for parsed class (used by parser_cxx.c) */
    CxxClass* templated_class;

    /* Function-template local classes are represented by private class
     * templates whose parameters mirror the enclosing function template.
     * Their source type is replaced only while the enclosing specialization
     * is cloned, so each function-template argument list gets its own class
     * identity and substituted layout. */
    CxxLocalClassTemplate* local_classes;
    int local_class_count;
    bool is_local_class_template;
    CxxClass* local_class_pattern;
    CxxClass* local_class_instance;
    /* Concrete class currently receiving substituted fields and methods. */
    CxxClass* active_class_instance;

    CxxDeductionGuide* deduction_guides;

    /* Explicit class-template specializations owned by this primary. */
    CxxTemplate* primary_template;
    CxxTemplate** specializations;
    int specialization_count;
    Type** specialization_args;
    /* Type-argument pack expansions in a class partial-specialization
     * pattern, indexed by the primary template argument position. */
    bool* specialization_arg_pack_expansions;
    /* Owner class-template-id pattern for an out-of-class nested member
     * specialization, when the owner itself is still dependent. */
    Type* specialization_owner_pattern;
    /* Non-type arguments in a class-specialization pattern.  The parallel
     * array is indexed by the primary template's argument position; NULL
     * entries denote type arguments. */
    Expr** specialization_value_args;
    int specialization_arg_count;

    /* Instantiations */
    struct {
        Type** args;
        int64_t* value_args;
        bool* value_present;
        Type** pack_args;
        int64_t* pack_values;
        bool* pack_value_present;
        int* pack_offsets;
        int* pack_counts;
        int pack_count;
        int arg_count;
        CxxClass* owner_class;
        void* instantiated;  /* CxxClass* or Decl* */
    } *instances;
    int instance_count;

    /* The parser supplies a function-template type pack immediately before
     * instantiation.  Keeping this transient state on the template avoids
     * changing the public legacy instantiate API; it is consumed
     * synchronously and copied into the instance cache. */
    Type** pending_pack_args;
    /* Parallel to pending_pack_args for dependent forwarding.  A true entry
     * is an unexpanded pack pattern that must remain an expansion in the
     * substituted type. */
    bool* pending_pack_expansions;
    int64_t* pending_pack_values;
    bool* pending_pack_value_present;
    Expr** pending_pack_value_expressions;
    int pending_pack_count;
    /* Function-template deduction may keep several parameter packs in one
     * contiguous argument buffer. These optional per-parameter ranges let
     * substitution expand each pack from its own slice. */
    int* pending_pack_offsets;
    int* pending_pack_counts;
    /* Candidate matching substitutes signatures before overload selection;
     * member-function bodies are cloned only for the selected candidate. */
    bool defer_function_body_clone;
    /* While cloning a local class method, this names the method's own
     * parameter list so pack expressions bind to that method's parameter pack. */
    DeclList* active_pack_parameters;
    /* Pack expansion context propagated into dependent type operands while a
     * fold is partially instantiated by an enclosing class template. */
    bool active_pack_expansion;
    int active_pack_expansion_index;
    int active_pack_expansion_count;
};

struct CxxLocalClassTemplate {
    CxxClass* pattern;
    CxxTemplate* templ;
};

/* C++ Method (extends Decl) */
struct CxxMethod {
    Decl* decl;              /* Base function declaration */
    const char* source_name; /* Stable source spelling after ABI mangling. */
    CxxClass* owner;         /* Owning class */
    AccessSpec access;
    bool is_static;
    bool is_virtual;
    bool is_pure_virtual;
    bool is_override;
    bool is_final;
    bool is_const;           /* const member function */
    bool is_volatile;        /* volatile member function */
    CxxRefQualifier ref_qualifier;
    bool is_constexpr;
    bool is_explicit;
    /* A C++20 `explicit(bool)` that depends on enclosing template arguments
     * is retained until class-template specialization substitutes them. */
    Expr* explicit_condition;
    bool is_noexcept;
    bool is_deleted;
    bool is_defaulted;
    bool is_constructor;
    bool is_destructor;
    CxxTemplate* member_template;
    CxxConstructorInfo* constructor_info;
    int vtable_index;        /* -1 if not virtual */
};

/* C++ virtual dispatch uses the source name, parameter types, and cv
 * qualifier as the slot identity; the return type is checked separately by
 * override validation.  Keep these checks shared by the parser and vtable
 * builder so an invalid declaration cannot be silently assigned a base slot. */
bool cxx_method_virtual_signature_matches(const CxxMethod* derived,
                                          const CxxMethod* base);
bool cxx_method_override_signature_matches(const CxxMethod* derived,
                                           const CxxMethod* base);
bool rcc_parser_cxx_eval_template_integer_expression(
    Expr* expression, CxxTemplate* tmpl, const int64_t* values,
    const bool* value_present, int64_t* result);
bool rcc_parser_cxx_resolve_dependent_static_integer_members(
    Expr** expression_pointer, CxxTemplate* tmpl, Type** arguments,
    const int64_t* values, const bool* value_present, unsigned depth);
bool rcc_parser_cxx_template_constraint_satisfied(
    CxxTemplate* tmpl, Type** arguments, const int64_t* values,
    const bool* value_present, SourceLoc loc);
bool rcc_parser_cxx_template_template_argument_matches(
    const TemplateParam* parameter, const CxxTemplate* actual);
Type* rcc_cxx_template_substitute_type(
    CxxTemplate* tmpl, Type* type, Type** arguments, int argument_count,
    const int64_t* value_args, const bool* value_present);
Type* rcc_parser_cxx_instantiate_dependent_class_template(
    CxxTemplate* tmpl, CxxClass* owner_class, Type** arguments,
    const int64_t* value_args, const bool* value_present,
    int argument_count, SourceLoc loc);

/* Name mangling */
char* cxx_mangle_name(const char* name, CxxNamespace* ns, CxxClass* cls);
char* cxx_mangle_function(Decl* func, CxxNamespace* ns, CxxClass* cls);
char* cxx_mangle_type(Type* type);

/* Class operations (core API) */
CxxClass* cxx_class_alloc(const char* name, bool is_struct);
bool cxx_structural_class_value_supported_subset(const Type* type);
bool cxx_structural_class_pack_member_values(
    const Type* type, const int64_t* member_values, int member_count,
    int64_t* packed_value);
bool cxx_structural_class_unpack_member_value(
    const Type* type, int64_t packed_value, const char* member_name,
    int64_t* member_value, Type** member_type);
typedef enum {
    CXX_STRUCTURAL_POINTER_PATH_ARRAY_INDEX,
    CXX_STRUCTURAL_POINTER_PATH_MEMBER
} CxxStructuralPointerPathKind;

typedef struct CxxStructuralPointerPathElement {
    CxxStructuralPointerPathKind kind;
    int64_t array_index;
    const Type* owner_type;
    const TypeField* member_field;
} CxxStructuralPointerPathElement;

/* Structural pointer members are represented in the parser's packed value
 * by a translation-unit-local token. The token resolves to a declaration,
 * byte offset, and typed member/array subobject path; it is never a host
 * pointer or a target-machine address. */
bool cxx_structural_pointer_value_register(
    Decl* declaration, int64_t byte_offset, bool has_subobject_path,
    bool one_past, bool array_decay,
    const CxxStructuralPointerPathElement* subobject_path,
    size_t subobject_path_length, int64_t* token_out);
Decl* cxx_structural_pointer_value_declaration(int64_t token);
bool cxx_structural_pointer_value_location(
    int64_t token, int64_t* byte_offset, bool* has_subobject_path,
    bool* one_past, bool* array_decay_out,
    const CxxStructuralPointerPathElement** subobject_path_out,
    size_t* subobject_path_length_out);
bool rcc_parser_cxx_structural_pointer_conversion(
    Type* source_pointee, Type* target_pointee,
    int64_t* adjustment_out);
bool cxx_structural_member_pointer_value_register(
    CxxClass* owner, const char* member_name, TypeMethod* method,
    int64_t offset, int64_t* token_out);
bool cxx_structural_member_pointer_value_resolve(
    int64_t token, CxxClass** owner_out, const char** member_name_out,
    int64_t* offset_out, TypeMethod** method_out);
bool cxx_structural_class_value_record_resolve(
    const Type* type, int64_t token, const int64_t** member_values_out,
    int* member_count_out);
bool cxx_structural_class_unpack_member_path(
    const Type* type, int64_t packed_value,
    const char* const* member_names, int member_name_count,
    int64_t* member_value, Type** member_type);
bool cxx_structural_class_unpack_array_member_path(
    const Type* type, int64_t packed_value,
    const char* const* member_names, int member_name_count,
    int array_element_index, int64_t* member_value, Type** member_type);
bool cxx_structural_class_unpack_multidimensional_array_path(
    const Type* type, int64_t packed_value,
    const char* const* member_names, int member_name_count,
    const int* array_indices, int array_index_count,
    int64_t* member_value, Type** member_type);
void cxx_class_add_base_ptr(CxxClass* cls, CxxClass* base, AccessSpec access, bool is_virtual);
void cxx_class_add_using_base_member(CxxClass* cls, const char* base_name,
                                     const char* member_name,
                                     Type* conversion_type,
                                     Type* base_type_pattern,
                                     AccessSpec access, SourceLoc loc);
void cxx_class_add_friend_class(CxxClass* cls, const char* friend_name);
void cxx_class_add_type_alias(CxxClass* cls, const char* name, Type* type,
                              AccessSpec access, SourceLoc loc);
CxxTypeAlias* cxx_class_find_type_alias(CxxClass* cls, const char* name);
CxxClassAliasTemplate* cxx_class_add_alias_template(
    CxxClass* cls, CxxTemplate* declaration, AccessSpec access,
    SourceLoc loc);
CxxClassAliasTemplate* cxx_class_find_alias_template(
    CxxClass* cls, const char* name);
CxxNestedClassTemplate* cxx_class_add_nested_template(
    CxxClass* cls, CxxTemplate* declaration, AccessSpec access,
    SourceLoc loc);
CxxNestedClassTemplate* cxx_class_find_nested_template(
    CxxClass* cls, const char* name);
CxxClassAliasTemplate* cxx_class_find_inherited_alias_template(
    CxxClass* cls, const char* name, CxxClass* access_context,
    CxxClass** declaring_class, bool* ambiguous, bool* accessible);
bool cxx_class_is_same_or_derived_from(CxxClass* cls, CxxClass* target);
CxxTypeAlias* cxx_class_find_inherited_type_alias(
    CxxClass* cls, const char* name, CxxClass* access_context,
    bool* ambiguous, bool* accessible);
bool cxx_class_resolve_nested_type_path(
    CxxClass* cls, const char* path, CxxClass* access_context,
    Type** resolved_type, bool* ambiguous, bool* accessible);
CxxTypeAlias* cxx_class_find_direct_public_base_type_alias(
    CxxClass* cls, const char* name, bool* ambiguous);
void cxx_class_add_member(CxxClass* cls, Decl* decl, AccessSpec access, bool is_static);
void cxx_class_compute_layout(CxxClass* cls);
void cxx_class_apply_explicit_alignment(CxxClass* cls, int alignment,
                                        SourceLoc loc);
bool cxx_class_virtual_base_offset(CxxClass* cls, CxxClass* base,
                                   int* offset);
bool cxx_class_is_abstract(const CxxClass* cls);
bool cxx_class_is_final(const CxxClass* cls);
bool cxx_class_is_polymorphic(const CxxClass* cls);
bool cxx_class_has_virtual_destructor(const CxxClass* cls);
bool cxx_class_is_empty(const CxxClass* cls);
void cxx_class_build_vtable(CxxClass* cls);

/* Namespace operations (core API) */
CxxNamespace* cxx_namespace_alloc(const char* name, CxxNamespace* parent);
CxxNamespace* cxx_namespace_lookup(CxxNamespace* root, const char* name);
CxxNamespace* cxx_namespace_find(CxxNamespace* root, const char* qualified_name);
CxxNamespace* cxx_namespace_for_decl_name(CxxNamespace* root,
                                           const char* qualified_name);
const char* cxx_namespace_qualified_name(CxxNamespace* ns);
void cxx_namespace_add_decl(CxxNamespace* ns, Decl* decl);
void cxx_namespace_add_template(CxxNamespace* ns, CxxTemplate* tmpl);
bool cxx_function_template_friend_signature_matches(
    const CxxTemplate* candidate, const CxxTemplate* current,
    const char* function_name, Type* function_type,
    DeclList* function_parameters, bool is_consteval,
    Expr* noexcept_expression);
void cxx_namespace_add_using_namespace(CxxNamespace* ns, CxxNamespace* target);
void cxx_namespace_add_using_decl(CxxNamespace* ns, const char* qualified_name);

/* Template operations (core API) */
CxxTemplate* cxx_template_alloc(const char* name, TemplateParam* params, int count);
void* cxx_template_instantiate(CxxTemplate* tmpl, Type** args, int arg_count);
void* cxx_template_instantiate_with_values(CxxTemplate* tmpl, Type** args,
                                           const int64_t* value_args,
                                           const bool* value_present,
                                           int arg_count);
bool rcc_cxx_complete_function_template_body(Decl* instance);
Expr* cxx_template_clone_expr(CxxTemplate* tmpl, Expr* expression,
                              Type** args, int arg_count);
Expr* cxx_template_clone_expr_with_values(
    CxxTemplate* tmpl, Expr* expression, Type** args, int arg_count,
    const int64_t* value_args, const bool* value_present);
/* Compare two already-substituted expressions as template-parameter mappings.
 * Constraint ordering uses this only after confirming that both atomic
 * constraints originate from the same source expression. */
bool cxx_template_constraint_mapping_matches(
    const Expr* left, const CxxTemplate* left_template,
    const Expr* right, const CxxTemplate* right_template);
bool cxx_template_type_parameter_mapping_matches(
    Type* left, const CxxTemplate* left_template,
    Type* right, const CxxTemplate* right_template);
Expr* cxx_template_clone_pack_expansion(
    CxxTemplate* tmpl, Expr* pattern, const char* pack_name, int pack_index,
    Type** args, int arg_count, const int64_t* value_args,
    const bool* value_present);
Stmt* cxx_template_clone_stmt(CxxTemplate* tmpl, Stmt* statement,
                              Type** args, int arg_count);
Stmt* cxx_template_clone_stmt_with_values(
    CxxTemplate* tmpl, Stmt* statement, Type** args, int arg_count,
    const int64_t* value_args, const bool* value_present);

/* Parser-owned class-template substitution used by the public template API.
 * The returned class is the cached specialization, not merely its Type. */
CxxClass* rcc_cxx_instantiate_class_template(CxxTemplate* tmpl,
                                              Type** args,
                                              const int64_t* value_args,
                                              const bool* value_present,
                                              int arg_count, SourceLoc loc);
CxxClass* rcc_cxx_instantiate_class_template_for_owner(
    CxxTemplate* tmpl, Type** args, const int64_t* value_args,
    const bool* value_present, int arg_count, CxxClass* owner_class,
    SourceLoc loc);

/* Global C++ state */
extern CxxNamespace* g_global_namespace;
CxxNamespace* cxx_namespace_global(void);

/* Initialize C++ subsystem */
void cxx_init(void);

/* ═══════════════════════════════════════
 * Parser API (for parser_cxx.c)
 * ═══════════════════════════════════════ */

/* Class creation with source location */
CxxClass* cxx_class_new(const char* name, SourceLoc loc);

/* Add base class by name (deferred resolution) */
void cxx_class_add_base(CxxClass* cls, const char* base_name, AccessSpec access);
void cxx_class_add_base_pattern(CxxClass* cls, Type* type_pattern,
                                const char* base_name, AccessSpec access,
                                bool is_virtual, bool is_pack_expansion);

/* Add field to class */
void cxx_class_add_field(CxxClass* cls, const char* name, Type* type, AccessSpec access);
/* Add a field together with its C++ default member initializer. */
void cxx_class_add_field_initializer(CxxClass* cls, const char* name,
                                     Type* type, AccessSpec access,
                                     Expr* initializer, bool is_bitfield,
                                     unsigned bit_width, bool is_static,
                                     bool is_deprecated,
                                     const char* deprecated_message,
                                     bool no_unique_address);

/* Add method to class */
void cxx_class_add_method(CxxClass* cls, CxxMethod* method);

/* Create method */
CxxMethod* cxx_method_new(const char* name, Type* return_type, DeclList* params, Stmt* body, SourceLoc loc);

/* Namespace creation with source location */
CxxNamespace* cxx_namespace_new(const char* name, SourceLoc loc);
const char* cxx_namespace_typeinfo_identity(CxxNamespace* ns);

/* Add class to namespace */
void cxx_namespace_add_class(CxxNamespace* ns, CxxClass* cls);

/* Add nested namespace */
void cxx_namespace_add_namespace(CxxNamespace* parent, CxxNamespace* child);
bool cxx_namespace_add_alias(CxxNamespace* ns, const char* name,
                             CxxNamespace* target);

/* Template creation with source location */
CxxTemplate* cxx_template_new(SourceLoc loc);

/* Add type parameter to template */
void cxx_template_add_type_param(CxxTemplate* tmpl, const char* name);

/* Add value parameter to template */
void cxx_template_add_value_param(CxxTemplate* tmpl, const char* name, Type* type);

/* C++ Parser entry point */
struct TokenList;
AST* rcc_parse_cxx(struct TokenList* tokens);

/* Common-expression parser hook for a known class-template specialization
 * followed by direct-list initialization.  Returns NULL without consuming
 * tokens when the current spelling is not such a type. */
Type* rcc_parse_cxx_direct_list_type(void);
Type* rcc_parse_cxx_type_name(void);
bool rcc_parse_cxx_type_start(void);
Expr* rcc_parse_cxx_template_call(void);
Expr* rcc_parse_cxx_concept_expression(void);
Expr* rcc_parse_cxx_dependent_member(void);
Expr* rcc_parse_cxx_qualified_template_member(void);
Stmt* rcc_parse_cxx_auto_local_declaration(void);
Expr* rcc_parse_cxx_special_expression(void);
/* Resolve a function-template overload set from a contextual function
 * pointer type.  A NULL target performs lookup only and reports whether the
 * name denotes one or more function templates. */
Decl* rcc_parser_cxx_resolve_function_pointer_template(
    const char* name, CxxNamespace* scope, Type* target,
    bool* has_templates, bool* is_ambiguous, int* selected_rank);

#endif /* AST_CXX_H */
