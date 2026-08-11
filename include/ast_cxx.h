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

/* Access specifier */
typedef enum {
    ACCESS_PUBLIC,
    ACCESS_PROTECTED,
    ACCESS_PRIVATE
} AccessSpec;

/* C++ Class/Struct */
struct CxxClass {
    const char* name;
    bool is_struct;          /* struct vs class (default access) */

    /* Base classes */
    struct {
        CxxClass* base;
        AccessSpec access;
        bool is_virtual;
    } *bases;
    int base_count;

    /* Members */
    struct CxxMember {
        AccessSpec access;
        Decl* decl;          /* Can be DECL_VAR or DECL_FUNC */
        bool is_static;
        bool is_virtual;
        bool is_pure_virtual;
        bool is_override;
        bool is_final;
        struct CxxMember* next;
    } *members;

    /* Virtual table info */
    int vtable_size;
    struct {
        const char* name;
        CxxMethod* method;
        int offset;
    } *vtable;

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
    bool has_default;
    union {
        Type* default_type;
        Expr* default_value;
    };
} TemplateParam;

/* C++ Template */
struct CxxTemplate {
    const char* name;
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

    /* Alternate storage for parsed class (used by parser_cxx.c) */
    CxxClass* templated_class;

    /* Instantiations */
    struct {
        Type** args;
        int arg_count;
        void* instantiated;  /* CxxClass* or Decl* */
    } *instances;
    int instance_count;
};

/* C++ Method (extends Decl) */
struct CxxMethod {
    Decl* decl;              /* Base function declaration */
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
void cxx_class_add_member(CxxClass* cls, Decl* decl, AccessSpec access, bool is_static);
void cxx_class_compute_layout(CxxClass* cls);
void cxx_class_build_vtable(CxxClass* cls);

/* Namespace operations (core API) */
CxxNamespace* cxx_namespace_alloc(const char* name, CxxNamespace* parent);
CxxNamespace* cxx_namespace_lookup(CxxNamespace* root, const char* name);
void cxx_namespace_add_decl(CxxNamespace* ns, Decl* decl);
void cxx_namespace_add_template(CxxNamespace* ns, CxxTemplate* tmpl);

/* Template operations (core API) */
CxxTemplate* cxx_template_alloc(const char* name, TemplateParam* params, int count);
void* cxx_template_instantiate(CxxTemplate* tmpl, Type** args, int arg_count);

/* Global C++ state */
extern CxxNamespace* g_global_namespace;

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

#endif /* AST_CXX_H */
