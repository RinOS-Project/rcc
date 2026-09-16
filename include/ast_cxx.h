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
typedef struct CxxVtableEntry CxxVtableEntry;
typedef struct CxxSecondaryVtable CxxSecondaryVtable;

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

/* Constructor facts retained until all class fields are known.  Only the
 * deliberately small, ABI-transparent subset accepted by parser_cxx.c is
 * lowered through the common aggregate backend. */
struct CxxConstructorInitializer {
    const char* field;
    Expr* value;
    /* Parenthesized mem-initializers retain their complete argument list.
     * `value` remains the first argument for the scalar aggregate verifier. */
    ExprList* arguments;
    CxxConstructorInfo* constructor;
    bool is_base_initializer;
    bool is_virtual_base_initializer;
    bool is_delegating_constructor;
    bool is_default_member_initializer;
    CxxConstructorInitializer* next;
};

struct CxxConstructorInfo {
    CxxMethod* method;
    int parameter_count;
    TypeParam* parameters;
    CxxConstructorInitializer* initializers;
    int initializer_count;
    bool initializers_are_supported;
    bool body_is_empty;
    bool is_deleted;
    bool is_defaulted;
    AccessSpec access;
    CxxConstructorInfo* next;
};

/* C++ Class/Struct */
struct CxxClass {
    const char* name;
    bool is_struct;          /* struct vs class (default access) */
    bool has_user_constructor;
    bool has_nonpublic_field;
    bool has_static_field;
    bool has_field_initializer;
    CxxConstructorInfo* constructors;

    /* Base classes */
    struct {
        CxxClass* base;
        const char* base_name; /* Deferred source spelling, if unresolved. */
        AccessSpec access;
        bool is_virtual;
    } *bases;
    int base_count;
    /* Explicit `using Base::member` declarations restore a hidden base
     * overload into the derived member lookup set. */
    struct {
        const char* base_name;
        const char* member_name;
    } *using_base_members;
    int using_base_member_count;
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

    /* Fields list (TypeParam*) for struct compatibility */
    TypeParam* fields;

    /* Namespace context */
    CxxNamespace* ns;

    /* Template instantiation info */
    CxxTemplate* templ;
    Type** template_args;
    int template_arg_count;
    int64_t* template_value_args;
    bool* template_value_present;
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

    /* Template body - either class or function */
    enum {
        TMPL_CLASS,
        TMPL_FUNCTION
    } kind;
    union {
        CxxClass* class_def;
        Decl* func_def;
    };

    bool is_constexpr;
    bool is_noexcept;
    /* A parsed integral requires-clause.  The current frontend accepts
     * constant expressions over non-type template parameters; keeping the
     * expression in the template object lets overload/instantiation code
     * reject unsatisfied specializations before code generation. */
    Expr* constraint;
    enum {
        TMPL_FUNCTION_NONE,
        TMPL_FUNCTION_VERSIONED_STRUCT,
    } function_lowering;
    int64_t function_constant;

    /* Alternate storage for parsed class (used by parser_cxx.c) */
    CxxClass* templated_class;

    /* Explicit class-template specializations owned by this primary. */
    CxxTemplate* primary_template;
    CxxTemplate** specializations;
    int specialization_count;
    Type** specialization_args;
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
        int pack_count;
        int arg_count;
        void* instantiated;  /* CxxClass* or Decl* */
    } *instances;
    int instance_count;

    /* The parser supplies a function-template type pack immediately before
     * instantiation.  Keeping this transient state on the template avoids
     * changing the public legacy instantiate API; it is consumed
     * synchronously and copied into the instance cache. */
    Type** pending_pack_args;
    int pending_pack_count;
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
    bool is_constexpr;
    bool is_explicit;
    bool is_noexcept;
    bool is_deleted;
    bool is_defaulted;
    bool is_constructor;
    bool is_destructor;
    int vtable_index;        /* -1 if not virtual */
};

/* Name mangling */
char* cxx_mangle_name(const char* name, CxxNamespace* ns, CxxClass* cls);
char* cxx_mangle_function(Decl* func, CxxNamespace* ns, CxxClass* cls);
char* cxx_mangle_type(Type* type);

/* Class operations (core API) */
CxxClass* cxx_class_alloc(const char* name, bool is_struct);
void cxx_class_add_base_ptr(CxxClass* cls, CxxClass* base, AccessSpec access, bool is_virtual);
void cxx_class_add_using_base_member(CxxClass* cls, const char* base_name,
                                     const char* member_name);
void cxx_class_add_member(CxxClass* cls, Decl* decl, AccessSpec access, bool is_static);
void cxx_class_compute_layout(CxxClass* cls);
bool cxx_class_virtual_base_offset(CxxClass* cls, CxxClass* base,
                                   int* offset);
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
void cxx_namespace_add_using_namespace(CxxNamespace* ns, CxxNamespace* target);
void cxx_namespace_add_using_decl(CxxNamespace* ns, const char* qualified_name);

/* Template operations (core API) */
CxxTemplate* cxx_template_alloc(const char* name, TemplateParam* params, int count);
void* cxx_template_instantiate(CxxTemplate* tmpl, Type** args, int arg_count);
void* cxx_template_instantiate_with_values(CxxTemplate* tmpl, Type** args,
                                           const int64_t* value_args,
                                           const bool* value_present,
                                           int arg_count);
Expr* cxx_template_clone_expr(CxxTemplate* tmpl, Expr* expression,
                              Type** args, int arg_count);
Expr* cxx_template_clone_expr_with_values(
    CxxTemplate* tmpl, Expr* expression, Type** args, int arg_count,
    const int64_t* value_args, const bool* value_present);
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

/* Add field to class */
void cxx_class_add_field(CxxClass* cls, const char* name, Type* type, AccessSpec access);
/* Add a field together with its C++ default member initializer. */
void cxx_class_add_field_initializer(CxxClass* cls, const char* name,
                                     Type* type, AccessSpec access,
                                     Expr* initializer, bool is_bitfield,
                                     unsigned bit_width, bool is_static);

/* Add method to class */
void cxx_class_add_method(CxxClass* cls, CxxMethod* method);

/* Create method */
CxxMethod* cxx_method_new(const char* name, Type* return_type, DeclList* params, Stmt* body, SourceLoc loc);

/* Namespace creation with source location */
CxxNamespace* cxx_namespace_new(const char* name, SourceLoc loc);

/* Add class to namespace */
void cxx_namespace_add_class(CxxNamespace* ns, CxxClass* cls);

/* Add nested namespace */
void cxx_namespace_add_namespace(CxxNamespace* parent, CxxNamespace* child);

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
Expr* rcc_parse_cxx_qualified_template_member(void);
Stmt* rcc_parse_cxx_auto_local_declaration(void);
Expr* rcc_parse_cxx_special_expression(void);

#endif /* AST_CXX_H */
