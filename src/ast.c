/*
 * RCC - RinOS C Compiler
 * AST construction utilities
 */

#include "rcc.h"
#include "ast.h"

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
      .is_unsigned = (unsigned_type), .is_const = false, \
      .is_volatile = false, .cxx_is_class = false, \
      .cxx_nontrivial = false, .cxx_class = NULL, \
      .cxx_namespace = NULL, .cxx_vtable_size = 0, \
      .cxx_vtable_symbol = NULL }

static Type builtin_void   = BUILTIN_TYPE(TYPE_VOID,   0, 1, false);
static Type builtin_bool   = BUILTIN_TYPE(TYPE_BOOL,   1, 1, true);
static Type builtin_char   = BUILTIN_TYPE(TYPE_CHAR,   1, 1, false);
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

void type_configure_target(TargetArch architecture) {
    int long_size = architecture == ARCH_X64 ? 8 : 4;
    builtin_long.size = long_size;
    builtin_long.align = long_size;
    builtin_ulong.size = long_size;
    builtin_ulong.align = long_size;
    builtin_nullptr.size = long_size;
    builtin_nullptr.align = long_size;
}

/* ═══════════════════════════════════════
 * Type Constructors
 * ═══════════════════════════════════════ */

Type* type_ptr(Type* base) {
    Type* t = rcc_alloc(sizeof(Type));
    t->kind = TYPE_PTR;
    t->size = g_opts.target_arch == ARCH_X64 ? 8 : 4;
    t->align = t->size;
    t->base = base;
    t->array_bound = NULL;
    t->array_unspecified_bound = false;
    t->array_parameter_static = false;
    t->array_parameter_const = false;
    t->array_parameter_volatile = false;
    t->array_parameter_restrict = false;
    t->cxx_is_class = false;
    t->cxx_nontrivial = false;
    t->cxx_class = NULL;
    t->cxx_namespace = NULL;
    t->cxx_vtable_size = 0;
    t->cxx_vtable_symbol = NULL;
    return t;
}

Type* type_array(Type* base, int len) {
    Type* t = rcc_alloc(sizeof(Type));
    t->kind = TYPE_ARRAY;
    t->size = len > 0 ? base->size * len : 0;
    t->align = base->align;
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
    t->cxx_class = NULL;
    t->cxx_namespace = NULL;
    t->cxx_vtable_size = 0;
    t->cxx_vtable_symbol = NULL;
    return t;
}

Type* type_func(Type* ret, TypeParam* params, bool variadic) {
    Type* t = rcc_alloc(sizeof(Type));
    t->kind = TYPE_FUNC;
    t->size = 0;
    t->align = 1;
    t->ret_type = ret;
    t->params = params;
    t->variadic = variadic;
    t->has_prototype = true;
    t->cxx_is_class = false;
    t->cxx_nontrivial = false;
    t->cxx_class = NULL;
    t->cxx_namespace = NULL;
    t->cxx_vtable_size = 0;
    t->cxx_vtable_symbol = NULL;
    return t;
}

Type* type_struct(const char* tag) {
    Type* t = rcc_alloc(sizeof(Type));
    t->kind = TYPE_STRUCT;
    t->size = 0;
    t->align = 1;
    t->tag = tag;
    t->fields = NULL;
    t->is_complete = false;
    t->cxx_is_class = false;
    t->cxx_nontrivial = false;
    t->cxx_class = NULL;
    t->cxx_namespace = NULL;
    t->cxx_vtable_size = 0;
    t->cxx_vtable_symbol = NULL;
    return t;
}

Type* type_union(const char* tag) {
    Type* t = rcc_alloc(sizeof(Type));
    t->kind = TYPE_UNION;
    t->size = 0;
    t->align = 1;
    t->tag = tag;
    t->fields = NULL;
    t->is_complete = false;
    t->cxx_is_class = false;
    t->cxx_nontrivial = false;
    t->cxx_class = NULL;
    t->cxx_namespace = NULL;
    t->cxx_vtable_size = 0;
    t->cxx_vtable_symbol = NULL;
    return t;
}

Type* type_enum(const char* tag) {
    Type* t = rcc_alloc(sizeof(Type));
    t->kind = TYPE_ENUM;
    t->size = 4;
    t->align = 4;
    t->enum_tag = tag;
    t->cxx_is_class = false;
    t->cxx_nontrivial = false;
    t->cxx_class = NULL;
    t->cxx_namespace = NULL;
    t->cxx_vtable_size = 0;
    t->cxx_vtable_symbol = NULL;
    return t;
}

/* ═══════════════════════════════════════
 * Type Utilities
 * ═══════════════════════════════════════ */

bool type_is_integer(Type* t) {
    return t->kind >= TYPE_BOOL && t->kind <= TYPE_LLONG;
}

bool type_is_floating(Type* t) {
    return t->kind == TYPE_FLOAT || t->kind == TYPE_DOUBLE;
}

bool type_is_arithmetic(Type* t) {
    return type_is_integer(t) || type_is_floating(t);
}

bool type_is_scalar(Type* t) {
    return type_is_arithmetic(t) || t->kind == TYPE_PTR ||
           t->kind == TYPE_NULLPTR;
}

bool type_is_pointer(Type* t) {
    return t->kind == TYPE_PTR;
}

bool type_is_array(Type* t) {
    return t->kind == TYPE_ARRAY;
}

bool type_is_function(Type* t) {
    return t->kind == TYPE_FUNC;
}

bool type_is_complete(Type* t) {
    if (t->kind == TYPE_VOID) return false;
    if (t->kind == TYPE_ARRAY && t->array_len < 0 &&
        !t->array_bound) return false;
    if ((t->kind == TYPE_STRUCT || t->kind == TYPE_UNION) && !t->is_complete) return false;
    return true;
}

bool type_is_compatible(Type* a, Type* b) {
    TypeParam* ap;
    TypeParam* bp;
    if (!a || !b) return false;
    if (a == b) return true;
    if (a->kind != b->kind) return false;
    if (a->is_reference != b->is_reference ||
        a->is_rvalue_reference != b->is_rvalue_reference) {
        return false;
    }
    if (type_is_integer(a) && a->is_unsigned != b->is_unsigned) return false;
    if (a->kind == TYPE_PTR) {
        return type_is_compatible(a->base, b->base);
    }
    if (a->kind == TYPE_ARRAY) {
        return (a->array_len < 0 || b->array_len < 0 ||
                a->array_len == b->array_len) &&
               type_is_compatible(a->base, b->base);
    }
    if (a->kind == TYPE_FUNC) {
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
            if (!type_is_compatible(ap->type, bp->type)) return false;
            ap = ap->next;
            bp = bp->next;
        }
        return ap == NULL && bp == NULL;
    }
    if (a->kind == TYPE_STRUCT || a->kind == TYPE_UNION) {
        if (a->tag || b->tag) {
            return a->tag && b->tag && strcmp(a->tag, b->tag) == 0;
        }
        /* A qualified copy of an anonymous aggregate retains the same field
         * graph even though it has no tag to compare. */
        return a->fields == b->fields;
    }
    if (a->kind == TYPE_ENUM) {
        return a->enum_tag && b->enum_tag &&
               strcmp(a->enum_tag, b->enum_tag) == 0;
    }
    return true;
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
    e->type = type_int;  /* char literals have type int in C */
    return e;
}

Expr* expr_string(const char* val, SourceLoc loc) {
    Expr* e = rcc_alloc(sizeof(Expr));
    e->kind = EXPR_STRING_LIT;
    e->loc = loc;
    e->str_val = val;
    e->type = type_array(type_char, (int)strlen(val) + 1);
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

Expr* expr_unary(ExprKind kind, Expr* operand, SourceLoc loc) {
    Expr* e = rcc_alloc(sizeof(Expr));
    e->kind = kind;
    e->loc = loc;
    e->unary_operand = operand;
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
    e->call_method = NULL;
    e->call_is_virtual = false;
    e->call_virtual_index = -1;
    e->call_virtual_object = NULL;
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
    s->block_stmts = stmts;
    return s;
}

Stmt* stmt_if(Expr* cond, Stmt* then_stmt, Stmt* else_stmt, SourceLoc loc) {
    Stmt* s = rcc_alloc(sizeof(Stmt));
    s->kind = STMT_IF;
    s->loc = loc;
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

AsmOperand* asm_operand_new(const char* constraint, Expr* expr) {
    AsmOperand* op = rcc_alloc(sizeof(AsmOperand));
    op->constraint = constraint;
    op->expr = expr;
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
    d->param_default = NULL;
    d->var_init = init;
    d->var_offset = 0;
    d->var_vla_size_offset = 0;
    d->var_vla_extent_offset = 0;
    d->var_vla_extent_count = 0;
    d->var_vla_scope_offset = 0;
    d->var_is_global = false;
    d->var_is_thread_local = false;
    d->var_is_auto = false;
    d->var_cleanup = NULL;
    return d;
}

Decl* decl_func(const char* name, Type* type, DeclList* params, Stmt* body, SourceLoc loc) {
    Decl* d = rcc_alloc(sizeof(Decl));
    d->kind = DECL_FUNC;
    d->name = name;
    d->link_name = name;
    d->type = type;
    d->loc = loc;
    d->param_default = NULL;
    d->func_params = params;
    d->func_body = body;
    d->func_this_param = NULL;
    d->func_method_owner = NULL;
    d->func_is_inline = false;
    d->func_is_defined = (body != NULL);
    d->func_is_template_instance = false;
    d->func_has_cxx_linkage = false;
    d->func_is_cxx_method = false;
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
    d->param_default = NULL;
    d->param_index = index;
    d->param_array_type = NULL;
    return d;
}

Decl* decl_typedef(const char* name, Type* type, SourceLoc loc) {
    Decl* d = rcc_alloc(sizeof(Decl));
    d->kind = DECL_TYPEDEF;
    d->name = name;
    d->link_name = name;
    d->type = type;
    d->loc = loc;
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
    d->type = type_int;
    d->param_default = NULL;
    d->enum_val = val;
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
