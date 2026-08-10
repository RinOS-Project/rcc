/*
 * RCC - RinOS C Compiler
 * AST construction utilities
 */

#include "rcc.h"
#include "ast.h"

/* ═══════════════════════════════════════
 * Built-in Types
 * ═══════════════════════════════════════ */

#define BUILTIN_TYPE(type_kind, type_size, type_align, unsigned_type) \
    { .kind = (type_kind), .size = (type_size), .align = (type_align), \
      .is_unsigned = (unsigned_type), .is_const = false, \
      .is_volatile = false }

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

void type_configure_target(TargetArch architecture) {
    int long_size = architecture == ARCH_X64 ? 8 : 4;
    builtin_long.size = long_size;
    builtin_long.align = long_size;
    builtin_ulong.size = long_size;
    builtin_ulong.align = long_size;
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
    return t;
}

Type* type_array(Type* base, int len) {
    Type* t = rcc_alloc(sizeof(Type));
    t->kind = TYPE_ARRAY;
    t->size = len > 0 ? base->size * len : 0;
    t->align = base->align;
    t->base = base;
    t->array_len = len;
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
    return t;
}

Type* type_enum(const char* tag) {
    Type* t = rcc_alloc(sizeof(Type));
    t->kind = TYPE_ENUM;
    t->size = 4;
    t->align = 4;
    t->enum_tag = tag;
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
    return type_is_arithmetic(t) || t->kind == TYPE_PTR;
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
    if (t->kind == TYPE_ARRAY && t->array_len < 0) return false;
    if ((t->kind == TYPE_STRUCT || t->kind == TYPE_UNION) && !t->is_complete) return false;
    return true;
}

bool type_is_compatible(Type* a, Type* b) {
    TypeParam* ap;
    TypeParam* bp;
    if (!a || !b) return false;
    if (a == b) return true;
    if (a->kind != b->kind) return false;
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
        if (a->variadic != b->variadic ||
            !type_is_compatible(a->ret_type, b->ret_type)) return false;
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
        return a->tag && b->tag && strcmp(a->tag, b->tag) == 0;
    }
    if (a->kind == TYPE_ENUM) {
        return a->enum_tag && b->enum_tag &&
               strcmp(a->enum_tag, b->enum_tag) == 0;
    }
    return true;
}

Type* type_common(Type* a, Type* b) {
    /* Usual arithmetic conversions */
    if (a->kind == TYPE_DOUBLE || b->kind == TYPE_DOUBLE) return type_double;
    if (a->kind == TYPE_FLOAT || b->kind == TYPE_FLOAT) return type_float;

    /* Integer promotions */
    if (a->kind < TYPE_INT) a = type_int;
    if (b->kind < TYPE_INT) b = type_int;

    /* Same type */
    if (a->kind == b->kind && a->is_unsigned == b->is_unsigned) return a;

    /* Unsigned has priority if same rank */
    if (a->kind == b->kind) {
        return a->is_unsigned ? a : b;
    }

    /* Higher rank wins */
    return a->kind > b->kind ? a : b;
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
    e->type = type_ptr(type_char);
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
    d->type = type;
    d->loc = loc;
    d->var_init = init;
    d->var_offset = 0;
    d->var_is_global = false;
    return d;
}

Decl* decl_func(const char* name, Type* type, DeclList* params, Stmt* body, SourceLoc loc) {
    Decl* d = rcc_alloc(sizeof(Decl));
    d->kind = DECL_FUNC;
    d->name = name;
    d->type = type;
    d->loc = loc;
    d->func_params = params;
    d->func_body = body;
    d->func_is_inline = false;
    d->func_is_defined = (body != NULL);
    return d;
}

Decl* decl_param(const char* name, Type* type, int index, SourceLoc loc) {
    Decl* d = rcc_alloc(sizeof(Decl));
    d->kind = DECL_PARAM;
    d->name = name;
    d->type = type;
    d->loc = loc;
    d->param_index = index;
    return d;
}

Decl* decl_typedef(const char* name, Type* type, SourceLoc loc) {
    Decl* d = rcc_alloc(sizeof(Decl));
    d->kind = DECL_TYPEDEF;
    d->name = name;
    d->type = type;
    d->loc = loc;
    d->typedef_type = type;
    return d;
}

Decl* decl_struct(const char* name, DeclList* fields, SourceLoc loc) {
    Decl* d = rcc_alloc(sizeof(Decl));
    d->kind = DECL_STRUCT;
    d->name = name;
    d->loc = loc;
    d->struct_fields = fields;
    return d;
}

Decl* decl_union(const char* name, DeclList* fields, SourceLoc loc) {
    Decl* d = rcc_alloc(sizeof(Decl));
    d->kind = DECL_UNION;
    d->name = name;
    d->loc = loc;
    d->struct_fields = fields;
    return d;
}

Decl* decl_enum(const char* name, DeclList* consts, SourceLoc loc) {
    Decl* d = rcc_alloc(sizeof(Decl));
    d->kind = DECL_ENUM;
    d->name = name;
    d->loc = loc;
    d->enum_consts = consts;
    return d;
}

Decl* decl_enum_const(const char* name, int64_t val, SourceLoc loc) {
    Decl* d = rcc_alloc(sizeof(Decl));
    d->kind = DECL_ENUM_CONST;
    d->name = name;
    d->loc = loc;
    d->type = type_int;
    d->enum_val = val;
    return d;
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
