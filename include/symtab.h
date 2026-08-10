/*
 * RCC - RinOS C Compiler
 * Symbol Table
 */

#ifndef SYMTAB_H
#define SYMTAB_H

#include "rcc.h"
#include "ast.h"

/* Symbol kinds */
typedef enum {
    SYM_VAR,        /* Variable */
    SYM_FUNC,       /* Function */
    SYM_PARAM,      /* Function parameter */
    SYM_TYPE,       /* Typedef */
    SYM_STRUCT,     /* Struct tag */
    SYM_UNION,      /* Union tag */
    SYM_ENUM,       /* Enum tag */
    SYM_ENUM_CONST, /* Enum constant */
    SYM_LABEL,      /* Goto label */
} SymKind;

/* Symbol entry */
typedef struct Symbol {
    const char* name;
    SymKind kind;
    Type* type;
    Decl* decl;
    SourceLoc loc;

    /* For variables */
    int offset;         /* Stack offset (local) or address (global) */
    bool is_global;
    bool is_defined;

    /* For enum constants */
    int64_t enum_val;

    /* Scope chain */
    struct Symbol* next;        /* Next in same scope */
    struct Symbol* shadowed;    /* Symbol shadowed by this one */
} Symbol;

/* Scope */
typedef struct Scope {
    Symbol* symbols;            /* Symbol list */
    struct Scope* parent;       /* Parent scope */
    int local_offset;           /* Next local variable offset */
    bool is_function;           /* Is function scope */
} Scope;

/* Symbol table */
typedef struct SymTab {
    Scope* current;             /* Current scope */
    Scope* global;              /* Global scope */
    Symbol* labels;             /* Label list (function-wide) */
} SymTab;

/* Symbol table functions */
SymTab* symtab_new(void);
void symtab_free(SymTab* st);

/* Scope management */
void symtab_enter_scope(SymTab* st);
void symtab_leave_scope(SymTab* st);
void symtab_enter_function(SymTab* st);
void symtab_leave_function(SymTab* st);

/* Symbol operations */
Symbol* symtab_define(SymTab* st, const char* name, SymKind kind, Type* type, SourceLoc loc);
Symbol* symtab_lookup(SymTab* st, const char* name);
Symbol* symtab_lookup_local(SymTab* st, const char* name);
Symbol* symtab_lookup_tag(SymTab* st, const char* name);

/* Label operations */
Symbol* symtab_define_label(SymTab* st, const char* name, SourceLoc loc);
Symbol* symtab_lookup_label(SymTab* st, const char* name);

/* Global symbol table */
extern SymTab* g_symtab;

#endif /* SYMTAB_H */
