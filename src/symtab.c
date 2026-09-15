/*
 * RCC - RinOS C Compiler
 * Symbol Table Implementation
 */

#include "rcc.h"
#include "symtab.h"

/* Global symbol table */
SymTab* g_symtab = NULL;

static bool symtab_is_vla(Type* type) {
    return type && type->kind == TYPE_ARRAY &&
           (type->array_bound != NULL || symtab_is_vla(type->base));
}

static int symtab_vla_dimension_count(Type* type) {
    if (!type || type->kind != TYPE_ARRAY) return 0;
    return 1 + symtab_vla_dimension_count(type->base);
}

/* Hash function for symbol lookup */
static unsigned int hash_name(const char* name) {
    unsigned int h = 0;
    while (*name) {
        h = h * 31 + (unsigned char)*name++;
    }
    return h;
}

/* Create new symbol table */
SymTab* symtab_new(void) {
    SymTab* st = rcc_alloc(sizeof(SymTab));

    /* Create global scope */
    st->global = rcc_alloc(sizeof(Scope));
    st->global->symbols = NULL;
    st->global->parent = NULL;
    st->global->local_offset = 0;
    st->global->is_function = false;

    st->current = st->global;
    st->labels = NULL;

    return st;
}

/* Free symbol table */
void symtab_free(SymTab* st) {
    /* Free all scopes and symbols */
    while (st->current != st->global) {
        symtab_leave_scope(st);
    }

    /* Free global symbols */
    Symbol* sym = st->global->symbols;
    while (sym) {
        Symbol* next = sym->next;
        rcc_free(sym);
        sym = next;
    }

    /* Free labels */
    sym = st->labels;
    while (sym) {
        Symbol* next = sym->next;
        rcc_free(sym);
        sym = next;
    }

    rcc_free(st->global);
    rcc_free(st);
}

/* Enter new scope */
void symtab_enter_scope(SymTab* st) {
    Scope* scope = rcc_alloc(sizeof(Scope));
    scope->symbols = NULL;
    scope->parent = st->current;
    scope->local_offset = st->current->local_offset;
    scope->is_function = false;
    st->current = scope;
}

/* Leave current scope */
void symtab_leave_scope(SymTab* st) {
    if (st->current == st->global) {
        return;
    }

    Scope* old = st->current;
    st->current = old->parent;

    /* Free symbols in old scope */
    Symbol* sym = old->symbols;
    while (sym) {
        Symbol* next = sym->next;
        rcc_free(sym);
        sym = next;
    }

    rcc_free(old);
}

/* Enter function scope */
void symtab_enter_function(SymTab* st) {
    symtab_enter_scope(st);
    st->current->is_function = true;
    st->current->local_offset = 0;
    st->labels = NULL;
}

/* Leave function scope */
void symtab_leave_function(SymTab* st) {
    /* Free labels */
    Symbol* sym = st->labels;
    while (sym) {
        Symbol* next = sym->next;
        rcc_free(sym);
        sym = next;
    }
    st->labels = NULL;

    symtab_leave_scope(st);
}

/* Define a new symbol */
Symbol* symtab_define(SymTab* st, const char* name, SymKind kind, Type* type, SourceLoc loc) {
    /* Check for redefinition in current scope */
    Symbol* existing = symtab_lookup_local(st, name);
    if (existing) {
        /* C permits a typedef name to be redeclared to the same type in the
         * same scope.  This is required for the standard forward declaration
         * form `typedef struct Tag Tag;` followed by the completed definition
         * `typedef struct Tag { ... } Tag;`.  The parser interns tagged types
         * and builtin types, so pointer identity is the semantic identity
         * available at this symbol-table boundary. */
        if (kind == SYM_TYPE && existing->kind == SYM_TYPE &&
            existing->type == type) {
            return existing;
        }
        rcc_error(loc, "redefinition of '%s'", name);
        return existing;
    }

    /* Create new symbol */
    Symbol* sym = rcc_alloc(sizeof(Symbol));
    sym->name = name;
    sym->kind = kind;
    sym->type = type;
    sym->decl = NULL;
    sym->loc = loc;
    sym->offset = 0;
    sym->is_global = (st->current == st->global);
    sym->is_defined = false;
    sym->enum_val = 0;

    /* Check if shadowing */
    sym->shadowed = symtab_lookup(st, name);

    /* Add to current scope */
    sym->next = st->current->symbols;
    st->current->symbols = sym;

    /* Allocate stack space for local variables */
    if (!sym->is_global && (kind == SYM_VAR || kind == SYM_PARAM)) {
        if (type && (type->size > 0 || symtab_is_vla(type))) {
            int storage = type->size;
            if (symtab_is_vla(type)) {
                /* A VLA identifier names runtime storage, so keep a pointer
                 * and its saved byte extent plus one byte extent for every
                 * array dimension in the fixed stack frame. */
                int word_size = g_opts.target_arch == ARCH_X64 ? 8 : 4;
                storage = (2 + symtab_vla_dimension_count(type)) * word_size;
            }
            st->current->local_offset += storage;
            /* Align to 4 bytes */
            st->current->local_offset = (st->current->local_offset + 3) & ~3;
            sym->offset = -st->current->local_offset;
        }
    }

    return sym;
}

/* Lookup symbol in all scopes */
Symbol* symtab_lookup(SymTab* st, const char* name) {
    for (Scope* scope = st->current; scope; scope = scope->parent) {
        for (Symbol* sym = scope->symbols; sym; sym = sym->next) {
            if (strcmp(sym->name, name) == 0) {
                return sym;
            }
        }
    }
    return NULL;
}

/* Lookup symbol in current scope only */
Symbol* symtab_lookup_local(SymTab* st, const char* name) {
    for (Symbol* sym = st->current->symbols; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0) {
            return sym;
        }
    }
    return NULL;
}

/* Lookup struct/union/enum tag */
Symbol* symtab_lookup_tag(SymTab* st, const char* name) {
    for (Scope* scope = st->current; scope; scope = scope->parent) {
        for (Symbol* sym = scope->symbols; sym; sym = sym->next) {
            if ((sym->kind == SYM_STRUCT || sym->kind == SYM_UNION || sym->kind == SYM_ENUM) &&
                strcmp(sym->name, name) == 0) {
                return sym;
            }
        }
    }
    return NULL;
}

/* Define a label */
Symbol* symtab_define_label(SymTab* st, const char* name, SourceLoc loc) {
    /* Check for existing label */
    Symbol* existing = symtab_lookup_label(st, name);
    if (existing && existing->is_defined) {
        rcc_error(loc, "redefinition of label '%s'", name);
        return existing;
    }

    if (existing) {
        existing->is_defined = true;
        existing->loc = loc;
        return existing;
    }

    /* Create new label */
    Symbol* sym = rcc_alloc(sizeof(Symbol));
    sym->name = name;
    sym->kind = SYM_LABEL;
    sym->type = NULL;
    sym->loc = loc;
    sym->is_defined = true;

    sym->next = st->labels;
    st->labels = sym;

    return sym;
}

/* Lookup a label */
Symbol* symtab_lookup_label(SymTab* st, const char* name) {
    for (Symbol* sym = st->labels; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0) {
            return sym;
        }
    }
    return NULL;
}
