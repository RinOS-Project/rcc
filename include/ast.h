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
typedef struct GenericAssociation GenericAssociation;

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
    TYPE_FUNC,
    TYPE_STRUCT,
    TYPE_UNION,
    TYPE_ENUM,
} TypeKind;

typedef struct TypeField {
    const char* name;
    Type* type;
    int offset;
    struct TypeField* next;
} TypeField;

typedef struct TypeParam {
    const char* name;
    Type* type;
    struct TypeParam* next;
} TypeParam;

struct Type {
    TypeKind kind;
    int size;           /* Size in bytes */
    int align;          /* Alignment */
    bool is_unsigned;
    bool is_const;
    bool is_volatile;

    union {
        /* TYPE_PTR, TYPE_ARRAY */
        struct {
            Type* base;
            int array_len;      /* -1 for flexible array */
        };
        /* TYPE_FUNC */
        struct {
            Type* ret_type;
            TypeParam* params;
            bool variadic;
        };
        /* TYPE_STRUCT, TYPE_UNION */
        struct {
            const char* tag;
            TypeField* fields;
            bool is_complete;
        };
        /* TYPE_ENUM */
        struct {
            const char* enum_tag;
            /* Enum values stored in symbol table */
        };
    };
};

/* Built-in types */
extern Type* type_void;
extern Type* type_bool;
extern Type* type_char;
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

/* Configure target-dependent fundamental widths after option parsing and
 * before lexing/parsing a translation unit. */
void type_configure_target(TargetArch architecture);

/* Type constructors */
Type* type_ptr(Type* base);
Type* type_array(Type* base, int len);
Type* type_func(Type* ret, TypeParam* params, bool variadic);
Type* type_struct(const char* tag);
Type* type_union(const char* tag);
Type* type_enum(const char* tag);

/* Type utilities */
bool type_is_integer(Type* t);
bool type_is_floating(Type* t);
bool type_is_arithmetic(Type* t);
bool type_is_scalar(Type* t);
bool type_is_pointer(Type* t);
bool type_is_array(Type* t);
bool type_is_function(Type* t);
bool type_is_complete(Type* t);
bool type_is_compatible(Type* a, Type* b);
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

    /* Compound literal */
    EXPR_COMPOUND,      /* (type){...} */
    EXPR_GENERIC,       /* _Generic(control, type: expression, ...) */
} ExprKind;

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
    struct ExprList* next;
} ExprList;

struct GenericAssociation {
    Type* type;                 /* NULL for default */
    Expr* expr;
    SourceLoc loc;
    struct GenericAssociation* next;
};

struct Expr {
    ExprKind kind;
    Type* type;
    SourceLoc loc;

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
        };

        /* EXPR_COMPOUND */
        struct {
            Type* compound_type;
            ExprList* compound_init;
        };

        /* EXPR_GENERIC */
        struct {
            Expr* generic_control;
            GenericAssociation* generic_associations;
        };
    };
};

/* Expression constructors */
Expr* expr_int(int64_t val, SourceLoc loc);
Expr* expr_float(double val, SourceLoc loc);
Expr* expr_char(char val, SourceLoc loc);
Expr* expr_string(const char* val, SourceLoc loc);
Expr* expr_ident(const char* name, SourceLoc loc);
Expr* expr_unary(ExprKind kind, Expr* operand, SourceLoc loc);
Expr* expr_binary(ExprKind kind, Expr* lhs, Expr* rhs, SourceLoc loc);
Expr* expr_cond(Expr* test, Expr* then_expr, Expr* else_expr, SourceLoc loc);
Expr* expr_call(Expr* func, ExprList* args, SourceLoc loc);
Expr* expr_index(Expr* base, Expr* index, SourceLoc loc);
Expr* expr_member(Expr* base, const char* name, SourceLoc loc);
Expr* expr_cast(Type* type, Expr* expr, SourceLoc loc);
Expr* expr_sizeof_expr(Expr* expr, SourceLoc loc);
Expr* expr_sizeof_type(Type* type, SourceLoc loc);
Expr* expr_alignof_type(Type* type, SourceLoc loc);
Expr* expr_initializer_list(ExprList* items, SourceLoc loc);
Expr* expr_generic(Expr* control, GenericAssociation* associations,
                   SourceLoc loc);
void generic_association_append(GenericAssociation** list, Type* type,
                                Expr* expr, SourceLoc loc);

/* ═══════════════════════════════════════
 * Statements
 * ═══════════════════════════════════════ */

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

struct Stmt {
    StmtKind kind;
    SourceLoc loc;

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
        const char* goto_label;

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
    Type* type;
    SourceLoc loc;
    StorageClass storage;

    union {
        /* DECL_VAR */
        struct {
            Expr* var_init;
            int var_offset;         /* Stack offset (set during codegen) */
            bool var_is_global;
        };

        /* DECL_FUNC */
        struct {
            DeclList* func_params;
            Stmt* func_body;        /* NULL for declaration only */
            bool func_is_inline;
            bool func_is_defined;
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
