/*
 * RCC - RinOS C Compiler
 * Abstract Syntax Tree (AST) definitions
 */

#ifndef AST_H
#define AST_H

#include "rcc.h"

/* Forward declarations */
typedef struct Type Type;
typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct Decl Decl;
typedef struct DeclList DeclList;
typedef struct TypeList TypeList;
typedef struct CxxCompoundRequirement CxxCompoundRequirement;
typedef struct GenericAssociation GenericAssociation;
typedef struct TypeMethod TypeMethod;
typedef struct CxxCatch CxxCatch;
typedef struct CxxCleanupPlan CxxCleanupPlan;
struct Token;
struct CxxClass;
struct CxxNamespace;
struct CxxTemplate;

/* ═══════════════════════════════════════
 * Type System
 * ═══════════════════════════════════════ */

typedef enum {
    TYPE_VOID,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_SHORT,
    TYPE_INT,
    TYPE_LONG,
    TYPE_LLONG,
    TYPE_FLOAT,
    TYPE_DOUBLE,
    TYPE_PTR,
    TYPE_ARRAY,
    TYPE_VECTOR,
    TYPE_FUNC,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ENUM,
    TYPE_NULLPTR,
} TypeKind;

typedef struct TypeField {
    const char* name;
    Type* type;
    int offset;
    /* C bit-field storage metadata.  An unnamed bit-field is represented by
     * layout only and is intentionally absent from the named field list. */
    bool is_bitfield;
    unsigned bit_width;
    unsigned bit_offset;
    /* True when this field is exposed from a virtual-base subobject. */
    bool from_virtual_base;
    /* Origin and in-subobject displacement for fields exposed through a
     * virtual base.  `offset` remains the complete-class layout offset; these
     * fields let member access use the current most-derived vbtable instead. */
    struct CxxClass* virtual_base_owner;
    int virtual_base_member_offset;
    /* C++ default member initializer, if one was declared in the class. */
    Expr* initializer;
    bool is_deprecated;          /* C++ deprecated data member. */
    const char* deprecated_message;
    /* C++20 [[no_unique_address]] on a non-static data member. */
    bool cxx_no_unique_address;
    /* 0 public/C, 1 protected, 2 private.  Kept numeric here so the common
     * C AST does not depend on the C++ extension header. */
    unsigned char cxx_access;
    /* Class that declared this non-static C++ data member.  Inherited
     * TypeFields retain this owner so `&Derived::inherited_member` has the
     * standard pointer-to-member owner type. */
    struct CxxClass* cxx_declaring_class;
    struct TypeField* next;
} TypeField;

typedef struct TypeParam {
    const char* name;
    Type* type;
    bool is_bitfield;
    unsigned bit_width;
    bool is_static;
    /* Used by C++ class fields; function parameters set this to false. */
    Expr* initializer;
    bool is_deprecated;          /* C++ deprecated data member. */
    const char* deprecated_message;
    /* C++20 [[no_unique_address]] on a class field. */
    bool cxx_no_unique_address;
    unsigned char cxx_access;
    struct TypeParam* next;
} TypeParam;

typedef struct EnumConstantInfo {
    const char* name;
    int64_t value;
    bool is_unsigned;
} EnumConstantInfo;

typedef enum {
    TYPE_METHOD_FIELD,
    TYPE_METHOD_FIELD_EQ_CONSTANT,
    TYPE_METHOD_FIELD_NE_CONSTANT,
    TYPE_METHOD_FIELD_RELEASE,
    TYPE_METHOD_FIELD_CLOSE,
    TYPE_METHOD_FUNCTION,
} TypeMethodKind;

/* C++ ref-qualifiers affect member overload viability and ABI naming. */
typedef enum {
    CXX_REF_QUAL_NONE,
    CXX_REF_QUAL_LVALUE,
    CXX_REF_QUAL_RVALUE,
} CxxRefQualifier;

/* A structurally validated C++ zero-argument method that can be expanded by
 * the common backend without exposing private representation as a member. */
struct TypeMethod {
    const char* name;
    Type* return_type;
    TypeField* field;
    Decl* function_decl; /* Non-NULL for a lowered ordinary C++ method. */
    /* Source declaration for a validated inline lowering.  Inline accessors
     * have no emitted function declaration, but their exception
     * specification still has to be resolved at the call site. */
    Decl* source_decl;
    TypeMethodKind kind;
    int64_t constant;
    const char* cleanup_function;
    TypeField* result_field;
    int64_t success_constant;
    unsigned char cxx_access;
    bool is_explicit;
    bool is_noexcept;
    CxxRefQualifier ref_qualifier;
    /* For a lowered member function, identify the object type expected by
     * the ABI and the byte adjustment needed to reach it from the lookup
     * object's address.  Ordinary methods use adjustment zero; inherited
     * methods retain their base owner here. */
    Type* this_owner;
    int this_adjustment;
    bool is_virtual;
    int vtable_index;
    const char* vtable_symbol;
    TypeMethod* next;
};

struct Type {
    TypeKind kind;
    int size;           /* Size in bytes */
    int align;          /* Alignment */
    bool has_explicit_alignment;
    bool is_unsigned;
    /* TYPE_CHAR has three distinct language types with identical storage:
     * plain char, signed char, unsigned char, plus C++20 char8_t. */
    bool is_plain_char;
    bool is_char8;
    bool is_const;
    bool is_volatile;
    bool is_atomic;          /* C17 _Atomic-qualified scalar object type. */
    bool is_restrict;
    bool is_reference;        /* C++ lvalue/rvalue reference ABI carrier. */
    bool is_rvalue_reference;
    bool cxx_is_member_pointer;
    Type* cxx_member_pointer_owner;
    bool cxx_is_class;
    bool cxx_nontrivial;
    bool cxx_dependent;
    /* C++ fixed-underlying-type enums retain the actual integer type so the
     * parser can validate every enumerator against its signedness and width. */
    bool enum_has_fixed_underlying;
    Type* enum_underlying_type;
    struct CxxClass* cxx_class;
    /* Lowerable C++ constructor arities remain attached to the type so
     * template instances survive parser-scope restoration. */
    uint32_t cxx_constructor_arity_mask;
    struct CxxTemplate* cxx_template;
    int cxx_template_param_index;
    /* Dependent nested type name such as `T::value_type`.  The owner
     * parameter is identified by cxx_template_param_index and is resolved
     * only after template substitution. */
    const char* cxx_dependent_member_name;
    Type** cxx_template_args;
    int cxx_template_arg_count;
    /* Dependent template-template specializations may carry integral
     * arguments such as `C<N>` even though their type carriers are all
     * represented in cxx_template_args.  Keep the source expressions in a
     * parallel bounded array so outer template substitution can evaluate
     * them before class instantiation. */
    Expr** cxx_template_value_args;
    /* Fully qualified namespace owning a C++ class type, or NULL for the
     * global namespace and non-class C types. */
    const char* cxx_namespace;
    /* Unique translation-unit identity for local or unnamed tag types. */
    const char* cxx_scope_identity;
    int cxx_vtable_size;
    const char* cxx_vtable_symbol;
    /* Stable translation-unit identity used by the bounded C++ RTTI table. */
    const char* cxx_typeinfo_symbol;
    /* Structurally validated C++ scope cleanup.  NULL for ordinary types. */
    const char* cleanup_function;
    TypeField* cleanup_field;
    int64_t cleanup_invalid;
    /* Ownership-transfer constructor lowered through a validated release
     * accessor.  NULL unless parser_cxx.c proved the exact move pattern. */
    TypeMethod* move_constructor_method;
    /* Ownership-transfer assignment lowered through the same validated
     * release accessor.  The frontend additionally proves the SDK close and
     * self-assignment-guard bodies before setting this metadata. */
    TypeMethod* move_assignment_method;

    union {
        /* TYPE_PTR, TYPE_ARRAY, TYPE_VECTOR */
        struct {
            Type* base;
            int array_len;      /* -1 for flexible array */
            Expr* array_bound;  /* non-NULL for a runtime VLA bound */
            bool array_unspecified_bound; /* C prototype-scope `[*]` */
            bool array_parameter_static;
            bool array_parameter_const;
            bool array_parameter_volatile;
            bool array_parameter_restrict;
        };
        /* TYPE_FUNC */
        struct {
            Type* ret_type;
            TypeParam* params;
            bool variadic;
            bool has_prototype;
        };
        /* TYPE_STRUCT, TYPE_UNION */
        struct {
            const char* tag;
            TypeField* fields;
            TypeMethod* methods;
            bool is_complete;
        };
        /* TYPE_ENUM */
        struct {
            const char* enum_tag;
            bool enum_is_scoped;  /* C++ enum class/enum struct. */
            EnumConstantInfo* enum_constants;
            int enum_constant_count;
        };
    };
};

/* Built-in types */
extern Type* type_void;
extern Type* type_bool;
extern Type* type_char;
extern Type* type_schar;
extern Type* type_char8;
extern Type* type_short;
extern Type* type_int;
extern Type* type_long;
extern Type* type_llong;
extern Type* type_uchar;
extern Type* type_ushort;
extern Type* type_uint;
extern Type* type_ulong;
extern Type* type_ullong;
extern Type* type_float;
extern Type* type_double;
extern Type* type_nullptr;

/* Configure target-dependent fundamental widths after option parsing and
 * before lexing/parsing a translation unit. */
void type_configure_target(TargetArch architecture);
void rcc_parser_set_cxx_mode(bool enabled);
void rcc_parser_initialize_builtin_va_list_type(void);
Type* rcc_parser_builtin_va_list_type(void);
bool rcc_parser_is_cxx_mode(void);
void rcc_parser_set_cxx_standard(int standard);
bool rcc_parser_cxx_standard_at_least(int standard);
void rcc_parser_function_scope_push(const char* name);
void rcc_parser_function_scope_pop(void);
void rcc_parser_reset_type_scopes(void);
const char* rcc_parser_new_local_type_identity(void);
void rcc_parser_apply_pragma_pack(struct Token* directive);
int rcc_parser_pack_alignment(void);
int rcc_parser_parse_explicit_alignment(void);
Type* rcc_parser_apply_explicit_alignment(Type* type, int alignment,
                                           SourceLoc loc);
Type* rcc_parser_lookup_type(const char* name);
void rcc_parser_define_type(const char* name, Type* type);
void* rcc_parser_type_scope_mark(void);
void rcc_parser_type_scope_restore(void* mark);
bool rcc_parser_import_enum_constants(Type* enum_type, SourceLoc loc);
void* rcc_parser_enum_scope_mark(void);
void rcc_parser_enum_scope_restore(void* mark);
Type* rcc_parser_parse_cxx_declarator(Type* base_type, const char** name,
                                      DeclList** parameters);
void rcc_parser_define_cxx_constructor_type(const char* name, Type* type,
                                            uint32_t arity_mask);
uint32_t rcc_parser_cxx_constructor_arity_mask(Type* type);
void rcc_parser_validate_cxx_constructor_initializer(Type* type,
                                                     Expr* initializer);
Type* rcc_parse_cxx_direct_list_type(void);
Type* rcc_parse_cxx_type_name(void);
bool rcc_parse_cxx_type_start(void);
Expr* rcc_parse_cxx_template_call(void);
Expr* rcc_parse_cxx_concept_expression(void);
Expr* rcc_parse_cxx_dependent_member(void);
Expr* rcc_parse_cxx_qualified_template_member(void);
Stmt* rcc_parse_cxx_auto_local_declaration(void);
Stmt* rcc_parse_cxx_class_local_declaration(Type* base_type,
                                            int storage, bool is_inline,
                                            bool is_constexpr,
                                            bool is_constinit,
                                            bool is_thread_local,
                                            SourceLoc loc);
void rcc_parser_validate_cxx_object_type(Type* type, SourceLoc loc);
Expr* rcc_parse_cxx_special_expression(void);
Stmt* rcc_parse_cxx_statement(void);
void* rcc_parser_cxx_using_scope_mark(void);
void rcc_parser_cxx_using_scope_restore(void* mark);
const char* rcc_parser_cxx_resolve_local_using(const char* name,
                                               SourceLoc loc);

/* Translation-unit lifetime storage. AST/parser nodes are bulk-released at
 * process exit by the single-shot host compiler. */
void* ast_arena_alloc(size_t size);
void* ast_arena_grow(void* pointer, size_t old_size, size_t new_size);
char* ast_arena_strdup(const char* text);

/* Type constructors */
Type* type_ptr(Type* base);
/* Construct a C++ reference type and apply the standard reference-collapse
 * rule when `base` is already a reference type. */
Type* type_reference(Type* base, bool rvalue_reference);
Type* type_array(Type* base, int len);
Type* type_vector(Type* base, int lanes, int vector_size);
Type* type_func(Type* ret, TypeParam* params, bool variadic);
Type* type_struct(const char* tag);
Type* type_union(const char* tag);
Type* type_enum(const char* tag);
Type* rcc_cxx_type_info_type(void);

/* Type utilities */
bool type_is_integer(Type* t);
bool type_is_floating(Type* t);
bool type_is_arithmetic(Type* t);
bool type_is_scalar(Type* t);
bool type_is_pointer(Type* t);
bool type_is_array(Type* t);
bool type_is_vector(Type* t);
bool type_is_function(Type* t);
bool type_is_complete(Type* t);
bool type_is_compatible(Type* a, Type* b);
bool ast_cxx_is_aggregate(const Type* type);
int ast_cxx_aggregate_element_count(const Type* type);
bool ast_cxx_aggregate_element(const Type* type, int index,
                              Type** element_type, int* element_offset,
                              TypeField** member_field);
TypeField* ast_cxx_aggregate_member(const Type* type, const char* name);
Type* type_common(Type* a, Type* b);

/* ═══════════════════════════════════════
 * Expressions
 * ═══════════════════════════════════════ */

typedef enum {
    /* Literals */
    EXPR_INT_LIT,
    EXPR_FLOAT_LIT,
    EXPR_CHAR_LIT,
    EXPR_STRING_LIT,

    /* Primary */
    EXPR_IDENT,
    EXPR_CXX_THIS,         /* Synthetic current object for constructor calls. */

    /* Unary */
    EXPR_NEG,           /* -x */
    EXPR_NOT,           /* !x */
    EXPR_BITNOT,        /* ~x */
    EXPR_ADDR,          /* &x */
    EXPR_DEREF,         /* *x */
    EXPR_PREINC,        /* ++x */
    EXPR_PREDEC,        /* --x */
    EXPR_POSTINC,       /* x++ */
    EXPR_POSTDEC,       /* x-- */
    EXPR_SIZEOF,        /* sizeof(x) */
    EXPR_ALIGNOF,       /* _Alignof(x) */
    EXPR_NOEXCEPT,      /* noexcept(x) */
    EXPR_CXX_TYPEID,    /* typeid(T) / typeid(non-polymorphic-expression) */
    EXPR_CAST,          /* (type)x */

    /* Binary */
    EXPR_ADD,           /* x + y */
    EXPR_SUB,           /* x - y */
    EXPR_MUL,           /* x * y */
    EXPR_DIV,           /* x / y */
    EXPR_MOD,           /* x % y */
    EXPR_BITAND,        /* x & y */
    EXPR_BITOR,         /* x | y */
    EXPR_BITXOR,        /* x ^ y */
    EXPR_LSHIFT,        /* x << y */
    EXPR_RSHIFT,        /* x >> y */
    EXPR_EQ,            /* x == y */
    EXPR_NE,            /* x != y */
    EXPR_LT,            /* x < y */
    EXPR_GT,            /* x > y */
    EXPR_LE,            /* x <= y */
    EXPR_GE,            /* x >= y */
    EXPR_SPACESHIP,     /* x <=> y (C++20 user-defined comparison) */
    EXPR_AND,           /* x && y */
    EXPR_OR,            /* x || y */

    /* Assignment */
    EXPR_ASSIGN,        /* x = y */
    EXPR_ADD_ASSIGN,    /* x += y */
    EXPR_SUB_ASSIGN,    /* x -= y */
    EXPR_MUL_ASSIGN,    /* x *= y */
    EXPR_DIV_ASSIGN,    /* x /= y */
    EXPR_MOD_ASSIGN,    /* x %= y */
    EXPR_AND_ASSIGN,    /* x &= y */
    EXPR_OR_ASSIGN,     /* x |= y */
    EXPR_XOR_ASSIGN,    /* x ^= y */
    EXPR_LSHIFT_ASSIGN, /* x <<= y */
    EXPR_RSHIFT_ASSIGN, /* x >>= y */

    /* Other */
    EXPR_COND,          /* x ? y : z */
    EXPR_COMMA,         /* x, y */
    EXPR_CALL,          /* f(args) */
    EXPR_INDEX,         /* a[i] */
    EXPR_MEMBER,        /* s.m */
    EXPR_PTR_MEMBER,    /* p->m */
    EXPR_CXX_MEMBER_PTR_DOT,   /* object .* member-pointer */
    EXPR_CXX_MEMBER_PTR_ARROW, /* pointer ->* member-pointer */

    /* Compound literal */
    EXPR_COMPOUND,      /* (type){...} */
    EXPR_GENERIC,       /* _Generic(control, type: expression, ...) */
    EXPR_CXX_FOLD,      /* C++ unary fold over a type function-parameter pack */
    EXPR_CXX_REQUIRES,  /* bounded C++20 requires-expression */
    EXPR_VA_START,      /* __builtin_va_start(list, last) */
    EXPR_VA_END,        /* __builtin_va_end(list) */
    EXPR_VA_COPY,       /* __builtin_va_copy(destination, source) */
    EXPR_VA_ARG,        /* __builtin_va_arg(list, type) */
} ExprKind;

typedef enum {
    CXX_CAST_NONE,
    CXX_CAST_STATIC,
    CXX_CAST_REINTERPRET,
    CXX_CAST_CONST,
    CXX_CAST_DYNAMIC,
} CxxCastKind;

typedef enum {
    INIT_DESIGNATOR_NONE,
    INIT_DESIGNATOR_INDEX,
    INIT_DESIGNATOR_FIELD,
} InitDesignatorKind;

typedef struct ExprList {
    Expr* expr;
    InitDesignatorKind designator_kind;
    int64_t designator_index;
    const char* designator_field;
    /* A class prvalue bound to a reference parameter lives through the
     * complete containing full-expression.  The synthetic owner holds its
     * caller-frame address so conditional paths can skip inactive cleanups. */
    Decl* cxx_temporary_owner;
    CxxCleanupPlan* cxx_temporary_cleanups;
    struct ExprList* cxx_temporary_next;
    struct ExprList* next;
} ExprList;

typedef enum {
    CXX_CLEANUP_EXPRESSION,
    CXX_CLEANUP_ARRAY_LOOP,
} CxxCleanupPlanKind;

/* Validated automatic-object cleanup actions.  Array nodes execute their
 * body once per element with index_decl holding the current element index. */
struct CxxCleanupPlan {
    CxxCleanupPlanKind kind;
    Expr* expression;
    Decl* index_decl;
    int element_count;
    CxxCleanupPlan* body;
    CxxCleanupPlan* next;
};

typedef struct TypeList {
    Type* type;
    SourceLoc loc;
    struct TypeList* next;
} TypeList;

struct CxxCompoundRequirement {
    Expr* expr;
    bool is_noexcept;
    Type* return_type;
    bool return_type_convertible;
    SourceLoc loc;
    CxxCompoundRequirement* next;
};

struct GenericAssociation {
    Type* type;                 /* NULL for default */
    Expr* expr;
    SourceLoc loc;
    struct GenericAssociation* next;
};

typedef struct CxxMoveAssignment {
    Expr* source;
    Expr* cleanup;
    Expr* release;
} CxxMoveAssignment;

typedef struct CxxCloseCall {
    Expr* object;
    Expr* handle;
    Expr* cleanup;
} CxxCloseCall;

struct Expr {
    ExprKind kind;
    Type* type;
    SourceLoc loc;
    /* Marks the literal spelling of C++ nullptr so it remains excluded from
     * integer constant expressions.  Its semantic type is TYPE_NULLPTR. */
    bool is_cxx_nullptr;
    /* Parentheses change decltype(auto) lvalue deduction, even though they
     * do not change ordinary expression lowering. */
    bool cxx_parenthesized;
    /* C++ conditional expressions preserve glvalue category only when both
     * operands have the same category and the same cv-qualified type. */
    bool cxx_conditional_lvalue;
    bool cxx_conditional_xvalue;
    /* A non-reference data member selected through a C++ dot-expression on
     * an xvalue object is itself an xvalue. */
    bool cxx_member_xvalue;
    /* C++20 u8 character/string literal mapped to the RinOS byte ABI. */
    bool is_cxx_utf8_literal;
    /* EXPR_STRING_LIT payload length, excluding its required terminator. */
    size_t str_length;
    /* C++ public non-virtual derived-to-base pointer conversion.  The
     * semantic pass records the fixed subobject displacement on the source
     * expression so both initializer and call lowering use the adjusted
     * pointer value. */
    bool cxx_pointer_adjustment_valid;
    int32_t cxx_pointer_adjustment;
    /* Converting a data-member pointer from a base owner to a derived owner
     * adds the base-subobject offset to the stored member displacement. */
    bool cxx_member_pointer_adjustment_valid;
    int32_t cxx_member_pointer_adjustment;
    /* The parser records the declaring/access classes for a bounded
     * `&Class::member` expression so sema can apply private/protected access
     * rules after the enclosing function context is known. */
    bool cxx_member_pointer_form;
    unsigned char cxx_member_pointer_form_access;
    struct CxxClass* cxx_member_pointer_form_declaring_class;
    struct CxxClass* cxx_member_pointer_form_designating_class;
    /* A bounded dynamic_cast downcast carries the expected complete-object
     * vtable identity.  Code generation returns a null pointer when the
     * source subobject does not contain that exact table. */
    bool cxx_dynamic_cast_checked;
    const char* cxx_dynamic_cast_vtable_symbol;
    /* Runtime RTTI search for public downcast/cross-cast relationships. */
    bool cxx_dynamic_cast_runtime;
    const char* cxx_dynamic_cast_typeinfo_symbol;
    /* A static/implicit conversion through a virtual base reads the
     * most-derived offset from the source subobject's hidden vbptr. */
    bool cxx_virtual_base_adjustment;
    /* The virtual-base adjustment is part of forming a data-member address,
     * so it must run before applying the field's in-base displacement. */
    bool cxx_virtual_base_member_access;
    int cxx_virtual_base_index;
    int32_t cxx_virtual_base_nested_adjustment;
    int32_t cxx_virtual_base_pointer_offset;
    struct CxxClass* cxx_virtual_base_source_class;
    /* Non-NULL only for a semantically validated C++ ownership transfer. */
    CxxMoveAssignment* cxx_move_assignment;
    /* Non-NULL only for the structurally validated SDK close operation. */
    CxxCloseCall* cxx_close_call;
    /* Semantic value of the C++ noexcept operator.  The value is kept
     * outside the expression union so a constexpr call folded during sema
     * cannot erase the call's exception specification before the enclosing
     * noexcept expression is evaluated. */
    bool cxx_noexcept_value_valid;
    bool cxx_noexcept_value;
    /* `typeid(expr)` on a glvalue of polymorphic class type resolves through
     * the most-derived vtable metadata at runtime. */
    bool cxx_typeid_dynamic;
    /* Bounded `type_info::hash_code()` call lowered from a typeid result. */
    bool cxx_typeinfo_hash_code;
    /* Bounded `type_info::name()` call lowered from a typeid result. */
    bool cxx_typeinfo_name;
    /* Bounded `type_info::before()` call lowered from two typeid identities. */
    bool cxx_typeinfo_before;
    /* Set after direct call resolution; false also covers function pointers
     * and unresolved/external calls whose exception specification is unknown. */
    bool cxx_call_is_noexcept;
    /* A class prvalue used as a member-call receiver is destroyed at the end
     * of the containing full-expression.  The source keeps the caller-frame
     * slot used by the cleanup plan. */
    Expr* cxx_temporary_source;
    Decl* cxx_temporary_owner;
    CxxCleanupPlan* cxx_temporary_cleanups;
    /* Captures for a C++ lambda that are spliced into an immediate call. */
    ExprList* cxx_lambda_captures;
    /* A generic lambda keeps its dependent call operator until the call site
     * supplies concrete argument types.  The semantic pass replaces the
     * expression with the cached, fully substituted function instance. */
    struct CxxTemplate* cxx_lambda_template;
    /* A bounded named C++20 concept call is evaluated only while checking a
     * template constraint; it is never lowered as a runtime function call. */
    struct CxxTemplate* cxx_concept_template;
    /* C++ `sizeof...(Pack)` is retained until a function-template
     * specialization supplies the pack length. */
    const char* sizeof_pack_name;
    const char* cxx_fold_pack_name;
    ExprKind cxx_fold_operator;
    bool cxx_fold_left;
    Expr* cxx_fold_init;
    /* The pack operand before `...` for a fold pattern such as
     * `((args + 1) + ...)`; NULL retains the identifier-only form. */
    Expr* cxx_fold_pattern;
    /* `args...` in a call is expanded while cloning a function-template
     * specialization.  The marker prevents the parser from treating it as a
     * single scalar argument. */
    bool cxx_pack_expansion;
    const char* cxx_pack_expansion_name;
    /* The complete expression before `...` for a call-argument pack
     * expansion such as `(args + 1)...`.  A NULL pattern means the legacy
     * named-pack form `args...`. */
    Expr* cxx_pack_expansion_pattern;
    /* Automatic storage used to materialize an aggregate rvalue.  A zero
     * value means that codegen has not assigned a slot; negative values are
     * frame-relative displacements, matching the other expression spills. */
    int aggregate_offset;
    /* Synthetic constructor this argument.  The offset is relative to the
     * active call's temporary/argument area and is set only by codegen. */
    int cxx_this_stack_offset;
    /* Address-of generated for the implicit object parameter of a C++ member
     * call.  It may materialize a class prvalue receiver in its aggregate
     * result slot; source-written unary & remains restricted to lvalues. */
    bool cxx_implicit_object_address;

    union {
        /* EXPR_INT_LIT */
        int64_t int_val;

        /* EXPR_FLOAT_LIT */
        double float_val;

        /* EXPR_CHAR_LIT */
        char char_val;

        /* EXPR_STRING_LIT */
        const char* str_val;

        /* EXPR_IDENT */
        struct {
            const char* ident_name;
            Decl* ident_decl;       /* Resolved during sema */
        };

        /* Unary expressions */
        struct {
            Expr* unary_operand;
            Type* sizeof_type;      /* For EXPR_SIZEOF/EXPR_ALIGNOF type */
        };

        /* Binary expressions */
        struct {
            Expr* binary_lhs;
            Expr* binary_rhs;
        };

        /* EXPR_COND */
        struct {
            Expr* cond_test;
            Expr* cond_then;
            Expr* cond_else;
        };

        /* EXPR_CALL */
        struct {
            Expr* call_func;
            ExprList* call_args;
            int call_result_offset;  /* Aggregate return spill/sret slot. */
            TypeMethod* call_method; /* Validated inline C++ accessor. */
            bool call_is_virtual;
            int call_virtual_index;
            Expr* call_virtual_object;
            /* C++ new-expression metadata.  The ordinary call fields retain
             * the allocation call so existing ABI lowering can be reused;
             * these fields describe the post-allocation initialization. */
            bool call_is_new;
            bool call_new_value_init;
            bool call_new_is_array;
            bool call_new_brace_init;
            bool call_new_array_cookie;
            bool call_new_default_member_initializers;
            bool call_new_copy_init;
            Type* call_new_type;
            Expr* call_new_count;
            ExprList* call_new_args;
            struct CxxConstructorInfo* call_new_constructor;
            /* C++ delete-expression metadata.  The ordinary call fields
             * retain the RinOS free call; these fields describe a validated
             * scalar destructor cleanup that must run before freeing. */
            bool call_is_delete;
            bool call_delete_is_array;
            Decl* call_delete_cleanup;
            Decl* call_delete_destructor;
            Decl* call_delete_array_cleanup;
            Decl* call_delete_array_destructor;
            TypeField* call_delete_cleanup_field;
            TypeField* call_delete_array_cleanup_field;
            int64_t call_delete_cleanup_invalid;
            int64_t call_delete_array_cleanup_invalid;
            /* Complete object type for recursively destroying member
             * subobjects when no single top-level destructor is available. */
            Type* call_delete_object_type;
        };

        /* EXPR_INDEX */
        struct {
            Expr* index_base;
            Expr* index_expr;
        };

        /* EXPR_MEMBER, EXPR_PTR_MEMBER */
        struct {
            Expr* member_base;
            const char* member_name;
            TypeField* member_field; /* Resolved during sema */
        };

        /* EXPR_CAST */
        struct {
            Expr* cast_expr;
            Type* cast_type;
            CxxCastKind cxx_cast_kind;
        };

        /* EXPR_CXX_TYPEID.  The expression is an lvalue-like reference to
         * the compiler's stable RinOS RTTI identity object.  Polymorphic
         * glvalues use the most-derived identity from their vtable metadata;
         * unsupported polymorphic prvalues remain a semantic diagnostic. */
        struct {
            Type* cxx_typeid_operand_type;
            Expr* cxx_typeid_operand;
            const char* cxx_typeid_symbol;
            bool cxx_typeid_is_type;
        };

        /* EXPR_COMPOUND */
        struct {
            Type* compound_type;
            ExprList* compound_init;
            int compound_offset;     /* Assigned automatic-storage slot. */
            bool compound_value_init; /* Spelled as an empty C++ {} list. */
            bool compound_copy_init;  /* C++ copy-initialization (`T t = x`). */
            bool compound_paren_init; /* C++20 aggregate `T t(a, b)`. */
            /* True only for a list synthesized for remaining levels of a
             * C aggregate designator. */
            bool compound_designator_wrapper;
            bool compound_cxx_default_member_normalized;
            /* True when C++ default member initializers rewrote this list
             * into compiler-generated designated clauses. */
            const char* compound_static_symbol;
            struct CxxConstructorInfo* compound_constructor;
        };

        /* EXPR_GENERIC */
        struct {
            Expr* generic_control;
            GenericAssociation* generic_associations;
        };

        /* EXPR_CXX_REQUIRES */
        struct {
            ExprList* cxx_requires_items;
            DeclList* cxx_requires_params;
            TypeList* cxx_requires_types;
            ExprList* cxx_requires_nested;
            CxxCompoundRequirement* cxx_requires_compound;
        };

        /* EXPR_VA_START/END/COPY/ARG */
        struct {
            Expr* va_list_operand;
            Expr* va_second_operand;
            Type* va_arg_type;
            int va_arg_result_offset; /* Aggregate va_arg spill slot. */
        };
    };
};

/* Expression constructors */
Expr* expr_int(int64_t val, SourceLoc loc);
Expr* expr_integer_literal(uint64_t val, unsigned base,
                           bool unsigned_suffix, unsigned long_suffix,
                           SourceLoc loc);
Expr* expr_float(double val, SourceLoc loc);
Expr* expr_char(char val, SourceLoc loc);
Expr* expr_string(const char* val, SourceLoc loc);
Expr* expr_string_n(const char* val, size_t length, SourceLoc loc);
Expr* expr_ident(const char* name, SourceLoc loc);
Expr* expr_cxx_this(SourceLoc loc);
Expr* expr_unary(ExprKind kind, Expr* operand, SourceLoc loc);
Expr* expr_binary(ExprKind kind, Expr* lhs, Expr* rhs, SourceLoc loc);
Expr* expr_cond(Expr* test, Expr* then_expr, Expr* else_expr, SourceLoc loc);
Expr* expr_call(Expr* func, ExprList* args, SourceLoc loc);
Expr* expr_index(Expr* base, Expr* index, SourceLoc loc);
Expr* expr_member(Expr* base, const char* name, SourceLoc loc);
Expr* expr_cast(Type* type, Expr* expr, SourceLoc loc);
Expr* expr_cxx_typeid(Type* operand_type, Expr* operand,
                      bool operand_is_type, SourceLoc loc);
Expr* expr_sizeof_expr(Expr* expr, SourceLoc loc);
Expr* expr_sizeof_type(Type* type, SourceLoc loc);
Expr* expr_sizeof_pack(const char* name, SourceLoc loc);
Expr* expr_cxx_fold(const char* pack_name, ExprKind operator_kind,
                    bool left_fold, SourceLoc loc);
Expr* expr_alignof_type(Type* type, SourceLoc loc);
Expr* expr_initializer_list(ExprList* items, SourceLoc loc);
Expr* expr_generic(Expr* control, GenericAssociation* associations,
                   SourceLoc loc);
Expr* expr_cxx_requires(ExprList* requirements, SourceLoc loc);
Expr* expr_vararg(ExprKind kind, Expr* list, Expr* second, Type* type,
                  SourceLoc loc);
void generic_association_append(GenericAssociation** list, Type* type,
                                Expr* expr, SourceLoc loc);
bool expr_eval_integer_constant(Expr* expr, int64_t* value);

/* ═══════════════════════════════════════
 * Statements
 * ═══════════════════════════════════════ */

/* A C++ catch handler.  Scalar and pointer payloads are materialized by the
 * current target-width exception ABI; aggregate/object payloads remain
 * explicit so the backend never treats an unsupported handler as empty. */
struct CxxCatch {
    Type* type;                 /* NULL for `catch (...)` */
    const char* name;           /* NULL for an unnamed handler */
    Decl* parameter;            /* Synthetic catch parameter, if named */
    Stmt* body;
    bool is_ellipsis;
    /* Additional class tags known in this translation unit that publicly
     * derive from the handler type.  Lowering ORs these exact tags into the
     * dispatch condition; no handler is silently widened.  Each offset is
     * the base-subobject displacement within the thrown derived object. */
    uint64_t* compatible_tags;
    int32_t* compatible_tag_offsets;
    size_t compatible_tag_count;
    CxxCatch* next;
};

typedef enum {
    STMT_EXPR,          /* expr; */
    STMT_BLOCK,         /* { ... } */
    STMT_IF,            /* if (cond) then [else] */
    STMT_WHILE,         /* while (cond) body */
    STMT_DO,            /* do body while (cond) */
    STMT_FOR,           /* for (init; cond; inc) body */
    STMT_SWITCH,        /* switch (expr) body */
    STMT_CASE,          /* case val: */
    STMT_DEFAULT,       /* default: */
    STMT_BREAK,         /* break; */
    STMT_CONTINUE,      /* continue; */
    STMT_RETURN,        /* return [expr]; */
    STMT_GOTO,          /* goto label; */
    STMT_LABEL,         /* label: stmt */
    STMT_DECL,          /* declaration */
    STMT_NULL,          /* ; (empty) */
    STMT_ASM,           /* asm("...") */
    STMT_TRY,           /* try { ... } catch (...) { ... } */
    STMT_THROW,         /* throw expression; */
} StmtKind;

/* Inline assembly operand */
typedef struct AsmOperand {
    const char* constraint;     /* e.g., "=a", "r", "m" */
    Expr* expr;                 /* The C expression */
    struct AsmOperand* next;
} AsmOperand;

/* Inline assembly clobber */
typedef struct AsmClobber {
    const char* reg;            /* e.g., "memory", "eax" */
    struct AsmClobber* next;
} AsmClobber;

typedef struct StmtList {
    Stmt* stmt;
    struct StmtList* next;
} StmtList;

typedef struct StmtDebugRange {
    uint32_t start;
    uint32_t end;
    struct StmtDebugRange* next;
} StmtDebugRange;

struct Stmt {
    StmtKind kind;
    SourceLoc loc;
    /* Fixed-frame slot used to restore RSP when a VLA-owning scope ends. */
    int vla_stack_offset;
    /* Codegen-populated half-open statement range.  Lexical blocks use it
     * for their scope DIE; local declarations also use the end PC as the
     * point where initialization has completed.  Offsets are module .text
     * positions consumed only by the relocatable DWARF emitter. */
    uint32_t debug_code_start;
    uint32_t debug_code_end;
    /* Legacy code generators record the first emitted text offset for each
     * statement so DWARF line tables can map non-block statements as well. */
    uint32_t debug_line_offset;
    bool debug_line_valid;
    /* Exact encoded instruction envelopes for this statement.  The legacy
     * start/end pair remains the enclosing range used by lexical blocks;
     * this list preserves holes introduced by control flow for line tables. */
    StmtDebugRange* debug_code_ranges;
    /* C++ `if constexpr` is selected after semantic constant evaluation, so
     * the discarded branch is never analyzed or lowered. */
    bool if_is_constexpr;
    /* Structured bindings are represented as a parser-generated statement
     * list, but their names belong to the surrounding scope. */
    bool block_no_scope;
    /* A reference-return statement has pointer ABI lowering.  A scalar
     * prvalue operand needs temporary storage to form that pointer, even
     * though C++ does not extend the temporary beyond the return full-expression. */
    bool return_reference_result;
    int return_reference_temporary_offset;

    union {
        /* STMT_EXPR */
        Expr* expr;

        /* STMT_BLOCK */
        StmtList* block_stmts;

        /* STMT_IF */
        struct {
            Expr* if_cond;
            Stmt* if_then;
            Stmt* if_else;
        };

        /* STMT_WHILE, STMT_DO */
        struct {
            Expr* while_cond;
            Stmt* while_body;
        };

        /* STMT_FOR */
        struct {
            Stmt* for_init;         /* Can be expr or decl */
            Expr* for_cond;
            Expr* for_inc;
            Stmt* for_body;
        };

        /* STMT_SWITCH */
        struct {
            Expr* switch_expr;
            Stmt* switch_body;
        };

        /* STMT_CASE */
        struct {
            Expr* case_val;
            Stmt* case_stmt;
        };

        /* STMT_DEFAULT */
        Stmt* default_stmt;

        /* STMT_RETURN */
        Expr* return_val;

        /* STMT_GOTO */
        struct {
            const char* goto_label;
            unsigned goto_cleanup_count;
            unsigned goto_vla_count;
        };

        /* STMT_LABEL */
        struct {
            const char* label_name;
            Stmt* label_stmt;
        };

        /* STMT_DECL */
        Decl* decl;

        /* STMT_ASM */
        struct {
            const char* asm_template;   /* Assembly template string */
            AsmOperand* asm_outputs;    /* Output operands */
            AsmOperand* asm_inputs;     /* Input operands */
            AsmClobber* asm_clobbers;   /* Clobbered registers */
            bool asm_volatile;          /* __volatile__ flag */
        };

        /* STMT_TRY */
        struct {
            Stmt* try_body;
            CxxCatch* try_catches;
            int try_frame_offset;
            int try_frame_size;
        };

        /* STMT_THROW */
        Expr* throw_expr;
    };
};

/* Statement constructors */
Stmt* stmt_expr(Expr* expr, SourceLoc loc);
Stmt* stmt_block(StmtList* stmts, SourceLoc loc);
Stmt* stmt_if(Expr* cond, Stmt* then_stmt, Stmt* else_stmt, SourceLoc loc);
Stmt* stmt_while(Expr* cond, Stmt* body, SourceLoc loc);
Stmt* stmt_do(Stmt* body, Expr* cond, SourceLoc loc);
Stmt* stmt_for(Stmt* init, Expr* cond, Expr* inc, Stmt* body, SourceLoc loc);
Stmt* stmt_switch(Expr* expr, Stmt* body, SourceLoc loc);
Stmt* stmt_case(Expr* val, Stmt* stmt, SourceLoc loc);
Stmt* stmt_default(Stmt* stmt, SourceLoc loc);
Stmt* stmt_break(SourceLoc loc);
Stmt* stmt_continue(SourceLoc loc);
Stmt* stmt_return(Expr* val, SourceLoc loc);
Stmt* stmt_goto(const char* label, SourceLoc loc);
Stmt* stmt_label(const char* name, Stmt* stmt, SourceLoc loc);
Stmt* stmt_decl(Decl* decl, SourceLoc loc);
Stmt* stmt_null(SourceLoc loc);
Stmt* stmt_asm(const char* templ, AsmOperand* outputs, AsmOperand* inputs,
               AsmClobber* clobbers, bool is_volatile, SourceLoc loc);
Stmt* stmt_try(Stmt* body, CxxCatch* catches, SourceLoc loc);
Stmt* stmt_throw(Expr* expression, SourceLoc loc);

/* Asm operand/clobber constructors */
AsmOperand* asm_operand_new(const char* constraint, Expr* expr);
AsmClobber* asm_clobber_new(const char* reg);

/* ═══════════════════════════════════════
 * Declarations
 * ═══════════════════════════════════════ */

typedef enum {
    DECL_VAR,           /* Variable */
    DECL_FUNC,          /* Function */
    DECL_PARAM,         /* Function parameter */
    DECL_TYPEDEF,       /* Typedef */
    DECL_STRUCT,        /* Struct definition */
    DECL_UNION,         /* Union definition */
    DECL_ENUM,          /* Enum definition */
    DECL_ENUM_CONST,    /* Enum constant */
    DECL_STATIC_ASSERT, /* Translation-unit static assertion */
} DeclKind;

typedef enum {
    STORAGE_NONE,
    STORAGE_EXTERN,
    STORAGE_STATIC,
    STORAGE_REGISTER,
    STORAGE_AUTO,
} StorageClass;

typedef struct DeclList {
    Decl* decl;
    struct DeclList* next;
} DeclList;

struct Decl {
    DeclKind kind;
    const char* name;
    /* Source-level lookup name and ABI symbol spelling are deliberately
     * separate.  They are identical for C declarations; C++ namespaces and
     * overloads retain a readable qualified name while code generation uses
     * the Itanium ABI spelling. */
    const char* link_name;
    Type* type;
    SourceLoc loc;
    StorageClass storage;
    /* GNU weak declarations retain external linkage but may be replaced by
     * a strong definition at link time.  Keep this source property on the
     * declaration so both native code generators and the verified backend
     * emit the same .ro binding. */
    bool is_weak;
    /* C++ default arguments belong to parameter declarations rather than
     * function types.  This remains outside the declaration union because
     * parameters reuse variable-layout fields for stack code generation. */
    Expr* param_default;
    /* Original array declarator before C parameter adjustment. */
    Type* param_array_type;
    /* C++ function parameter pack marker.  Supported type packs are expanded
     * during template instantiation before semantic analysis/codegen. */
    bool param_is_pack;

    union {
        /* DECL_VAR */
        struct {
            Expr* var_init;
            int var_offset;         /* Stack offset (set during codegen) */
            int var_reference_temporary_offset; /* Lifetime-extended object */
            Decl* var_reference_temporary_owner; /* Complete-object cleanup */
            Expr* var_reference_temporary_source; /* Static subobject source */
            Decl* var_reference_temporary_guard; /* Static/TLS init guard */
            bool var_dynamic_initializer; /* Initialize on first use */
            int var_vla_size_offset; /* Saved runtime VLA byte size */
            int var_vla_extent_offset; /* First saved VLA dimension extent */
            int var_vla_extent_count;  /* Number of saved VLA dimensions */
            int var_vla_scope_offset; /* Owning scope's saved stack slot */
            bool var_is_global;
            bool var_is_static_local;
            bool var_is_block_extern;
            bool var_is_thread_local;
            bool var_is_vla;
            bool var_is_auto;       /* C++ placeholder type, deduced in sema. */
            bool var_is_decltype_auto; /* C++14 decltype(auto) variable. */
            bool var_is_auto_reference;
            bool var_is_auto_rvalue_reference;
            bool var_is_auto_pointer;
            bool var_is_auto_const;
            /* A captureless generic lambda stored in an `auto` variable keeps
             * its dependent call operator until each call site supplies the
             * concrete arguments.  The bounded ABI still materializes the
             * selected function instance as the initializer value. */
            struct CxxTemplate* var_cxx_lambda_template;
            bool var_cxx_lambda_specialized;
            bool var_is_constexpr;  /* C++ constexpr variable declaration. */
            bool var_is_constinit;  /* C++20 constant-initialized variable. */
            bool var_is_inline;     /* C++17 inline variable definition. */
            bool var_is_deprecated; /* C++17 deprecated variable declaration. */
            const char* var_deprecated_message;
            Expr* var_cleanup;       /* Validated C++ scope-exit expression. */
            CxxCleanupPlan* var_cleanups; /* Validated object cleanup plan. */
        };

        /* DECL_FUNC */
        struct {
            DeclList* func_params;
            Stmt* func_body;        /* NULL for declaration only */
            Decl* func_this_param;  /* Implicit object parameter for C++ methods. */
            Type* func_method_owner; /* Owning class type for C++ methods. */
            bool func_is_inline;
            bool func_is_defined;
            bool func_is_template_instance;
            bool func_has_cxx_linkage;
            bool func_is_cxx_method;
            bool func_is_cxx_constructor;
            bool func_is_cxx_destructor;
            bool func_is_constexpr;
            bool func_is_consteval;
            bool func_is_noreturn;
            bool func_is_nodiscard;
            bool func_is_deprecated; /* C++14 deprecated function declaration. */
            const char* func_deprecated_message;
            bool func_is_noexcept;
            Expr* func_noexcept_expr;
            bool func_is_auto_return;
    bool func_is_decltype_auto_return;
            /* Defining namespace retained for deferred template-body
             * semantic analysis.  Ordinary C++ declarations already encode
             * this in their qualified name; instantiated function templates
             * keep the source name for ABI mangling and need this sideband. */
            const char* func_cxx_namespace;
            struct CxxNamespace* func_cxx_namespace_scope;
            Decl* func_overload_next;
        };

        /* DECL_PARAM */
        struct {
            int param_index;
        };

        /* DECL_TYPEDEF */
        struct {
            Type* typedef_type;
        };

        /* DECL_STRUCT, DECL_UNION */
        struct {
            DeclList* struct_fields;
        };

        /* DECL_ENUM */
        struct {
            DeclList* enum_consts;
        };

        /* DECL_ENUM_CONST */
        struct {
            int64_t enum_val;
        };

        /* DECL_STATIC_ASSERT */
        struct {
            Expr* static_assert_expr;
            const char* static_assert_message;
        };
    };
};

/* Declaration constructors */
Decl* decl_var(const char* name, Type* type, Expr* init, SourceLoc loc);
Decl* decl_func(const char* name, Type* type, DeclList* params, Stmt* body, SourceLoc loc);
Decl* decl_param(const char* name, Type* type, int index, SourceLoc loc);
Decl* decl_typedef(const char* name, Type* type, SourceLoc loc);
Decl* decl_struct(const char* name, DeclList* fields, SourceLoc loc);
Decl* decl_union(const char* name, DeclList* fields, SourceLoc loc);
Decl* decl_enum(const char* name, DeclList* consts, SourceLoc loc);
Decl* decl_enum_const(const char* name, int64_t val, SourceLoc loc);
Decl* decl_static_assert(Expr* expression, const char* message,
                         SourceLoc loc);
const char* decl_link_name(const Decl* decl);

/* ═══════════════════════════════════════
 * AST (Translation Unit)
 * ═══════════════════════════════════════ */

typedef struct AST {
    DeclList* decls;        /* Top-level declarations */
} AST;

AST* ast_new(void);
void ast_add_decl(AST* ast, Decl* decl);

/* ═══════════════════════════════════════
 * List utilities
 * ═══════════════════════════════════════ */

ExprList* exprlist_new(Expr* expr);
void exprlist_append(ExprList** list, Expr* expr);
void exprlist_append_designated(ExprList** list, Expr* expr,
                                InitDesignatorKind kind, int64_t index,
                                const char* field);
int exprlist_len(ExprList* list);

StmtList* stmtlist_new(Stmt* stmt);
void stmtlist_append(StmtList** list, Stmt* stmt);

DeclList* decllist_new(Decl* decl);
void decllist_append(DeclList** list, Decl* decl);

#endif /* AST_H */
