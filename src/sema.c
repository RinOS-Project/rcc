/*
 * RCC - RinOS C Compiler
 * Semantic Analysis
 */

#include "rcc.h"
#include "ast.h"
#include "symtab.h"
#include <limits.h>

/* Current function return type */
static Type* current_func_ret = NULL;

/* Forward declarations */
static void sema_stmt(Stmt* stmt);
static Type* sema_expr(Expr* expr);
static void sema_decl(Decl* decl);

/* ═══════════════════════════════════════
 * Type Checking Helpers
 * ═══════════════════════════════════════ */

static bool is_lvalue(Expr* e) {
    switch (e->kind) {
        case EXPR_IDENT:
        case EXPR_DEREF:
        case EXPR_INDEX:
        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            return true;
        default:
            return false;
    }
}

static Type* get_pointer_base(Type* t) {
    if (t->kind == TYPE_PTR) return t->base;
    if (t->kind == TYPE_ARRAY) return t->base;
    return NULL;
}

static bool is_pointer_arithmetic_type(Type* type) {
    Type* base = get_pointer_base(type);
    return base && base->kind != TYPE_VOID && base->kind != TYPE_FUNC &&
           base->size > 0;
}

static Type* implicit_cast(Expr* e, Type* target) {
    if (!e->type || !target) return NULL;

    /* Same type */
    if (e->type == target) return target;

    /* Integer promotions */
    if (type_is_integer(e->type) && type_is_integer(target)) {
        return target;
    }

    /* Pointer to/from integer */
    if (type_is_pointer(e->type) && type_is_integer(target)) {
        return target;
    }
    if (type_is_integer(e->type) && type_is_pointer(target)) {
        return target;
    }

    /* Array to pointer decay */
    if (type_is_array(e->type) && type_is_pointer(target)) {
        if (type_is_compatible(e->type->base, target->base)) {
            return target;
        }
    }

    /* void* conversions */
    if (type_is_pointer(e->type) && type_is_pointer(target)) {
        if (e->type->base == type_void || target->base == type_void) {
            return target;
        }
        if (type_is_compatible(e->type->base, target->base)) {
            return target;
        }
    }

    return NULL;
}

/* ═══════════════════════════════════════
 * Expression Semantic Analysis
 * ═══════════════════════════════════════ */

static Type* sema_expr(Expr* expr) {
    if (!expr) return NULL;

    switch (expr->kind) {
        case EXPR_INT_LIT:
            expr->type = type_int;
            break;

        case EXPR_FLOAT_LIT:
            expr->type = type_double;
            break;

        case EXPR_CHAR_LIT:
            expr->type = type_int;
            break;

        case EXPR_STRING_LIT:
            expr->type = type_ptr(type_char);
            break;

        case EXPR_IDENT: {
            Symbol* sym = symtab_lookup(g_symtab, expr->ident_name);
            if (!sym) {
                rcc_error(expr->loc, "undefined identifier '%s'", expr->ident_name);
                expr->type = type_int;
            } else {
                expr->ident_decl = sym->decl;
                expr->type = sym->type;
            }
            break;
        }

        case EXPR_NEG:
        case EXPR_BITNOT:
        case EXPR_NOT: {
            Type* t = sema_expr(expr->unary_operand);
            if (!type_is_arithmetic(t) && expr->kind != EXPR_NOT) {
                rcc_error(expr->loc, "invalid operand type for unary operator");
            }
            expr->type = (expr->kind == EXPR_NOT) ? type_int : t;
            break;
        }

        case EXPR_ADDR: {
            Type* t = sema_expr(expr->unary_operand);
            if (!is_lvalue(expr->unary_operand)) {
                rcc_error(expr->loc, "cannot take address of rvalue");
            }
            expr->type = type_ptr(t);
            break;
        }

        case EXPR_DEREF: {
            Type* t = sema_expr(expr->unary_operand);
            Type* base = get_pointer_base(t);
            if (!base) {
                rcc_error(expr->loc, "cannot dereference non-pointer");
                expr->type = type_int;
            } else {
                expr->type = base;
            }
            break;
        }

        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC: {
            Type* t = sema_expr(expr->unary_operand);
            if (!is_lvalue(expr->unary_operand)) {
                rcc_error(expr->loc, "increment/decrement requires lvalue");
            }
            if (!type_is_arithmetic(t) &&
                !(type_is_pointer(t) && is_pointer_arithmetic_type(t))) {
                rcc_error(expr->loc,
                          "increment/decrement requires arithmetic or object pointer type");
            }
            expr->type = t;
            break;
        }

        case EXPR_SIZEOF: {
            if (expr->sizeof_type) {
                expr->type = type_uint;
            } else {
                sema_expr(expr->unary_operand);
                expr->type = type_uint;
            }
            break;
        }

        case EXPR_CAST: {
            sema_expr(expr->cast_expr);
            expr->type = expr->cast_type;
            break;
        }

        case EXPR_ADD:
        case EXPR_SUB: {
            Type* lt = sema_expr(expr->binary_lhs);
            Type* rt = sema_expr(expr->binary_rhs);
            bool left_pointer = type_is_pointer(lt) || type_is_array(lt);
            bool right_pointer = type_is_pointer(rt) || type_is_array(rt);
            Type* left_result = type_is_array(lt) ? type_ptr(lt->base) : lt;
            Type* right_result = type_is_array(rt) ? type_ptr(rt->base) : rt;

            /* Pointer arithmetic */
            if (left_pointer && type_is_integer(rt)) {
                if (!is_pointer_arithmetic_type(lt)) {
                    rcc_error(expr->loc,
                              "pointer arithmetic requires a complete object type");
                }
                expr->type = left_result;
            } else if (type_is_integer(lt) && right_pointer &&
                       expr->kind == EXPR_ADD) {
                if (!is_pointer_arithmetic_type(rt)) {
                    rcc_error(expr->loc,
                              "pointer arithmetic requires a complete object type");
                }
                expr->type = right_result;
            } else if (left_pointer && right_pointer &&
                       expr->kind == EXPR_SUB) {
                Type* left_base = get_pointer_base(lt);
                Type* right_base = get_pointer_base(rt);
                if (!is_pointer_arithmetic_type(lt) ||
                    !is_pointer_arithmetic_type(rt) ||
                    !type_is_compatible(left_base, right_base)) {
                    rcc_error(expr->loc,
                              "pointer subtraction requires compatible complete object types");
                }
                expr->type = type_long;  /* ptrdiff_t */
            } else if (type_is_arithmetic(lt) && type_is_arithmetic(rt)) {
                expr->type = type_common(lt, rt);
            } else {
                rcc_error(expr->loc, "invalid operands to binary +/-");
                expr->type = type_int;
            }
            break;
        }

        case EXPR_MUL:
        case EXPR_DIV:
        case EXPR_MOD: {
            Type* lt = sema_expr(expr->binary_lhs);
            Type* rt = sema_expr(expr->binary_rhs);
            if (!type_is_arithmetic(lt) || !type_is_arithmetic(rt)) {
                rcc_error(expr->loc, "invalid operands to binary operator");
            }
            expr->type = type_common(lt, rt);
            break;
        }

        case EXPR_BITAND:
        case EXPR_BITOR:
        case EXPR_BITXOR:
        case EXPR_LSHIFT:
        case EXPR_RSHIFT: {
            Type* lt = sema_expr(expr->binary_lhs);
            Type* rt = sema_expr(expr->binary_rhs);
            if (!type_is_integer(lt) || !type_is_integer(rt)) {
                rcc_error(expr->loc, "bitwise operator requires integer operands");
            }
            expr->type = type_common(lt, rt);
            break;
        }

        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_GT:
        case EXPR_LE:
        case EXPR_GE: {
            sema_expr(expr->binary_lhs);
            sema_expr(expr->binary_rhs);
            expr->type = type_int;
            break;
        }

        case EXPR_AND:
        case EXPR_OR: {
            sema_expr(expr->binary_lhs);
            sema_expr(expr->binary_rhs);
            expr->type = type_int;
            break;
        }

        case EXPR_ADD_ASSIGN:
        case EXPR_SUB_ASSIGN: {
            Type* lt = sema_expr(expr->binary_lhs);
            Type* rt = sema_expr(expr->binary_rhs);
            if (!is_lvalue(expr->binary_lhs)) {
                rcc_error(expr->loc, "assignment requires lvalue");
            }
            if (!((type_is_pointer(lt) && is_pointer_arithmetic_type(lt) &&
                   type_is_integer(rt)) ||
                  (type_is_arithmetic(lt) && type_is_arithmetic(rt)))) {
                rcc_error(expr->loc,
                          "invalid operands to compound pointer arithmetic");
            }
            expr->type = lt;
            break;
        }

        case EXPR_ASSIGN:
        case EXPR_MUL_ASSIGN:
        case EXPR_DIV_ASSIGN:
        case EXPR_MOD_ASSIGN:
        case EXPR_AND_ASSIGN:
        case EXPR_OR_ASSIGN:
        case EXPR_XOR_ASSIGN:
        case EXPR_LSHIFT_ASSIGN:
        case EXPR_RSHIFT_ASSIGN: {
            Type* lt = sema_expr(expr->binary_lhs);
            sema_expr(expr->binary_rhs);
            if (!is_lvalue(expr->binary_lhs)) {
                rcc_error(expr->loc, "assignment requires lvalue");
            }
            expr->type = lt;
            break;
        }

        case EXPR_COND: {
            sema_expr(expr->cond_test);
            Type* tt = sema_expr(expr->cond_then);
            Type* et = sema_expr(expr->cond_else);
            expr->type = type_common(tt, et);
            break;
        }

        case EXPR_COMMA: {
            sema_expr(expr->binary_lhs);
            expr->type = sema_expr(expr->binary_rhs);
            break;
        }

        case EXPR_CALL: {
            Type* ft = sema_expr(expr->call_func);
            if (!ft || ft->kind != TYPE_FUNC) {
                /* Could be pointer to function */
                if (ft && ft->kind == TYPE_PTR && ft->base && ft->base->kind == TYPE_FUNC) {
                    ft = ft->base;
                } else {
                    rcc_error(expr->loc, "called object is not a function");
                    expr->type = type_int;
                    break;
                }
            }

            /* Check arguments */
            for (ExprList* arg = expr->call_args; arg; arg = arg->next) {
                sema_expr(arg->expr);
            }

            expr->type = ft->ret_type;
            break;
        }

        case EXPR_INDEX: {
            Type* bt = sema_expr(expr->index_base);
            Type* it = sema_expr(expr->index_expr);

            Type* base = get_pointer_base(bt);
            if (!base) {
                rcc_error(expr->loc, "subscript requires array or pointer");
                expr->type = type_int;
            } else {
                if (!type_is_integer(it)) {
                    rcc_error(expr->loc, "array subscript must be integer");
                }
                expr->type = base;
            }
            break;
        }

        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER: {
            Type* bt = sema_expr(expr->member_base);

            if (expr->kind == EXPR_PTR_MEMBER) {
                bt = get_pointer_base(bt);
                if (!bt) {
                    rcc_error(expr->loc, "-> requires pointer to struct/union");
                    expr->type = type_int;
                    break;
                }
            }

            if (!bt || (bt->kind != TYPE_STRUCT && bt->kind != TYPE_UNION)) {
                rcc_error(expr->loc, "member access requires struct/union");
                expr->type = type_int;
                break;
            }

            /* Find member */
            TypeField* field = bt->fields;
            while (field) {
                if (strcmp(field->name, expr->member_name) == 0) {
                    expr->member_field = field;
                    expr->type = field->type;
                    break;
                }
                field = field->next;
            }

            if (!field) {
                rcc_error(expr->loc, "no member named '%s'", expr->member_name);
                expr->type = type_int;
            }
            break;
        }

        default:
            expr->type = type_int;
            break;
    }

    return expr->type;
}

/* ═══════════════════════════════════════
 * Statement Semantic Analysis
 * ═══════════════════════════════════════ */

static void sema_stmt(Stmt* stmt) {
    if (!stmt) return;

    switch (stmt->kind) {
        case STMT_EXPR:
            if (stmt->expr) {
                sema_expr(stmt->expr);
            }
            break;

        case STMT_BLOCK:
            symtab_enter_scope(g_symtab);
            for (StmtList* s = stmt->block_stmts; s; s = s->next) {
                sema_stmt(s->stmt);
            }
            symtab_leave_scope(g_symtab);
            break;

        case STMT_IF:
            sema_expr(stmt->if_cond);
            sema_stmt(stmt->if_then);
            if (stmt->if_else) {
                sema_stmt(stmt->if_else);
            }
            break;

        case STMT_WHILE:
            sema_expr(stmt->while_cond);
            sema_stmt(stmt->while_body);
            break;

        case STMT_DO:
            sema_stmt(stmt->while_body);
            sema_expr(stmt->while_cond);
            break;

        case STMT_FOR:
            symtab_enter_scope(g_symtab);
            if (stmt->for_init) {
                sema_stmt(stmt->for_init);
            }
            if (stmt->for_cond) {
                sema_expr(stmt->for_cond);
            }
            if (stmt->for_inc) {
                sema_expr(stmt->for_inc);
            }
            sema_stmt(stmt->for_body);
            symtab_leave_scope(g_symtab);
            break;

        case STMT_SWITCH:
            sema_expr(stmt->switch_expr);
            sema_stmt(stmt->switch_body);
            break;

        case STMT_CASE:
            sema_expr(stmt->case_val);
            sema_stmt(stmt->case_stmt);
            break;

        case STMT_DEFAULT:
            sema_stmt(stmt->default_stmt);
            break;

        case STMT_RETURN:
            if (stmt->return_val) {
                Type* t = sema_expr(stmt->return_val);
                if (current_func_ret && current_func_ret != type_void) {
                    if (!implicit_cast(stmt->return_val, current_func_ret)) {
                        rcc_warning(stmt->loc, "incompatible return type");
                    }
                }
            }
            break;

        case STMT_GOTO: {
            Symbol* label = symtab_lookup_label(g_symtab, stmt->goto_label);
            if (!label) {
                /* Forward reference - create placeholder */
                label = rcc_alloc(sizeof(Symbol));
                label->name = stmt->goto_label;
                label->kind = SYM_LABEL;
                label->is_defined = false;
                label->next = g_symtab->labels;
                g_symtab->labels = label;
            }
            break;
        }

        case STMT_LABEL:
            symtab_define_label(g_symtab, stmt->label_name, stmt->loc);
            sema_stmt(stmt->label_stmt);
            break;

        case STMT_DECL:
            sema_decl(stmt->decl);
            break;

        case STMT_BREAK:
        case STMT_CONTINUE:
        case STMT_NULL:
            /* Nothing to check */
            break;

        case STMT_ASM:
            /* Analyze input/output expressions */
            for (AsmOperand* op = stmt->asm_outputs; op; op = op->next) {
                if (op->expr) {
                    sema_expr(op->expr);
                }
            }
            for (AsmOperand* op = stmt->asm_inputs; op; op = op->next) {
                if (op->expr) {
                    sema_expr(op->expr);
                }
            }
            break;
    }
}

/* ═══════════════════════════════════════
 * Declaration Semantic Analysis
 * ═══════════════════════════════════════ */

static void sema_decl(Decl* decl) {
    if (!decl) return;

    switch (decl->kind) {
        case DECL_VAR: {
            bool is_global = g_symtab->current == g_symtab->global;
            bool string_array_initializer = decl->type &&
                decl->type->kind == TYPE_ARRAY && decl->type->base &&
                decl->type->base->kind == TYPE_CHAR && decl->var_init &&
                decl->var_init->kind == EXPR_STRING_LIT;
            if (string_array_initializer) {
                size_t characters = strlen(decl->var_init->str_val);
                size_t storage = characters + 1u;
                if (decl->type->array_len < 0) {
                    if (storage > INT_MAX ||
                        decl->type->base->size <= 0 ||
                        storage > (size_t)INT_MAX /
                                  (size_t)decl->type->base->size) {
                        rcc_error(decl->loc,
                                  "character array initializer is too large");
                    } else {
                        decl->type->array_len = (int)storage;
                        decl->type->size = (int)storage *
                                           decl->type->base->size;
                    }
                } else if ((size_t)decl->type->array_len < characters) {
                    rcc_error(decl->loc,
                              "initializer string is too long for character array");
                }
            } else if (decl->type && decl->type->kind == TYPE_ARRAY &&
                       decl->var_init) {
                rcc_error(decl->loc,
                          "unsupported array initializer for '%s'",
                          decl->name);
            } else if (decl->type && decl->type->kind == TYPE_ARRAY &&
                       decl->type->array_len < 0 &&
                       !(decl->storage == STORAGE_EXTERN && !decl->var_init)) {
                rcc_error(decl->loc,
                          "incomplete array requires an initializer with known size");
            }
            Symbol* sym = is_global
                ? symtab_lookup_local(g_symtab, decl->name) : NULL;
            if (sym) {
                if (sym->kind != SYM_VAR ||
                    !type_is_compatible(sym->type, decl->type)) {
                    rcc_error(decl->loc,
                              "conflicting declaration of variable '%s'",
                              decl->name);
                } else if (decl->var_init && sym->is_defined) {
                    rcc_error(decl->loc, "redefinition of variable '%s'",
                              decl->name);
                }
            } else {
                sym = symtab_define(g_symtab, decl->name, SYM_VAR,
                                    decl->type, decl->loc);
            }
            sym->decl = decl;
            if (!is_global || decl->var_init) sym->is_defined = true;
            decl->var_offset = sym->offset;
            decl->var_is_global = sym->is_global;

            if (decl->var_init) {
                sema_expr(decl->var_init);
                if (!string_array_initializer &&
                    decl->type->kind != TYPE_ARRAY &&
                    !implicit_cast(decl->var_init, decl->type)) {
                    rcc_warning(decl->loc, "incompatible types in initialization");
                }
            }
            break;
        }

        case DECL_FUNC: {
            Symbol* sym = symtab_lookup(g_symtab, decl->name);
            if (sym && sym->kind == SYM_FUNC) {
                /* Check for redefinition */
                if (sym->is_defined && decl->func_body) {
                    rcc_error(decl->loc, "redefinition of function '%s'", decl->name);
                }
            } else {
                sym = symtab_define(g_symtab, decl->name, SYM_FUNC, decl->type, decl->loc);
            }
            sym->decl = decl;

            if (decl->func_body) {
                sym->is_defined = true;

                /* Enter function scope */
                symtab_enter_function(g_symtab);
                current_func_ret = decl->type->ret_type;

                /* Add parameters */
                int param_offset = 8;  /* After saved EBP and return address */
                for (DeclList* p = decl->func_params; p; p = p->next) {
                    Symbol* psym = symtab_define(g_symtab, p->decl->name, SYM_PARAM,
                                                  p->decl->type, p->decl->loc);
                    psym->decl = p->decl;
                    psym->offset = param_offset;
                    p->decl->var_offset = param_offset;
                    {
                        int parameter_size = p->decl->type &&
                            p->decl->type->size > 4
                            ? p->decl->type->size : 4;
                        param_offset += (parameter_size + 3) & ~3;
                    }
                }

                /* Analyze body */
                sema_stmt(decl->func_body);

                /* Check for undefined labels */
                for (Symbol* label = g_symtab->labels; label; label = label->next) {
                    if (!label->is_defined) {
                        rcc_error(decl->loc, "undefined label '%s'", label->name);
                    }
                }

                symtab_leave_function(g_symtab);
                current_func_ret = NULL;
            }
            break;
        }

        case DECL_PARAM:
            /* Handled in DECL_FUNC */
            break;

        case DECL_TYPEDEF: {
            symtab_define(g_symtab, decl->name, SYM_TYPE, decl->typedef_type, decl->loc);
            break;
        }

        case DECL_STRUCT:
        case DECL_UNION: {
            SymKind k = (decl->kind == DECL_STRUCT) ? SYM_STRUCT : SYM_UNION;
            symtab_define(g_symtab, decl->name, k, decl->type, decl->loc);
            break;
        }

        case DECL_ENUM: {
            symtab_define(g_symtab, decl->name, SYM_ENUM, decl->type, decl->loc);
            /* Define enum constants */
            for (DeclList* c = decl->enum_consts; c; c = c->next) {
                Symbol* sym = symtab_define(g_symtab, c->decl->name, SYM_ENUM_CONST,
                                             type_int, c->decl->loc);
                sym->enum_val = c->decl->enum_val;
            }
            break;
        }

        case DECL_ENUM_CONST:
            /* Handled in DECL_ENUM */
            break;
    }
}

/* ═══════════════════════════════════════
 * Main Semantic Analysis
 * ═══════════════════════════════════════ */

bool rcc_sema(AST* ast) {
    /* Create symbol table */
    g_symtab = symtab_new();

    /* Process all top-level declarations */
    for (DeclList* d = ast->decls; d; d = d->next) {
        sema_decl(d->decl);
    }

    return g_error_count == 0;
}
