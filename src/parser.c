/*
 * RCC - RinOS C Compiler
 * Parser (Recursive Descent)
 */

#include "rcc.h"
#include "token.h"
#include "ast.h"

/* Parser state - exported for parser_cxx.c */
typedef struct {
    Token* cur;
    Token* prev;
} Parser;

Parser parser;  /* Non-static for C++ parser access */

typedef struct ParserTypeName {
    const char* name;
    Type* type;
    struct ParserTypeName* next;
} ParserTypeName;

typedef struct ParserTagName {
    const char* name;
    TypeKind kind;
    Type* type;
    struct ParserTagName* next;
} ParserTagName;

typedef struct ParserEnumConstant {
    const char* name;
    int64_t value;
    struct ParserEnumConstant* next;
} ParserEnumConstant;

static ParserTypeName* parser_type_names;
static ParserTagName* parser_tag_names;
static ParserEnumConstant* parser_enum_constants;

static Type* parser_lookup_type(const char* name) {
    ParserTypeName* entry;
    for (entry = parser_type_names; entry; entry = entry->next) {
        if (strcmp(entry->name, name) == 0) return entry->type;
    }
    return NULL;
}

static void parser_define_type(const char* name, Type* type) {
    ParserTypeName* entry = ast_arena_alloc(sizeof(*entry));
    entry->name = name;
    entry->type = type;
    entry->next = parser_type_names;
    parser_type_names = entry;
}

static Type* parser_tag_type(TypeKind kind, const char* name) {
    ParserTagName* entry;
    if (name) {
        for (entry = parser_tag_names; entry; entry = entry->next) {
            if (entry->kind == kind && strcmp(entry->name, name) == 0) {
                return entry->type;
            }
        }
    }
    Type* type = kind == TYPE_STRUCT ? type_struct(name) :
                 kind == TYPE_UNION ? type_union(name) : type_enum(name);
    if (name) {
        entry = ast_arena_alloc(sizeof(*entry));
        entry->name = name;
        entry->kind = kind;
        entry->type = type;
        entry->next = parser_tag_names;
        parser_tag_names = entry;
    }
    return type;
}

static void parser_define_enum_constant(const char* name, int64_t value) {
    ParserEnumConstant* entry = ast_arena_alloc(sizeof(*entry));
    entry->name = name;
    entry->value = value;
    entry->next = parser_enum_constants;
    parser_enum_constants = entry;
}

static bool parser_lookup_enum_constant(const char* name, int64_t* value) {
    ParserEnumConstant* entry;
    for (entry = parser_enum_constants; entry; entry = entry->next) {
        if (strcmp(entry->name, name) == 0) {
            if (value) *value = entry->value;
            return true;
        }
    }
    return false;
}

/* ═══════════════════════════════════════
 * Parser Utilities
 * ═══════════════════════════════════════ */

static Token* peek(void) {
    return parser.cur;
}

static Token* previous(void) {
    return parser.prev;
}

static bool check(TokenType type) {
    return peek()->type == type;
}

static bool at_end(void) {
    return check(TOK_EOF);
}

static Token* advance(void) {
    if (!at_end()) {
        parser.prev = parser.cur;
        parser.cur = parser.cur->next;
    }
    return previous();
}

static bool match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

static Token* expect(TokenType type, const char* msg) {
    if (check(type)) {
        return advance();
    }
    rcc_error(peek()->loc, "expected %s, got '%s'", msg, token_type_str(peek()->type));
    return NULL;
}

static void synchronize(void) {
    advance();
    while (!at_end()) {
        if (previous()->type == TOK_SEMICOLON) return;
        switch (peek()->type) {
            case TOK_IF:
            case TOK_WHILE:
            case TOK_FOR:
            case TOK_RETURN:
            case TOK_INT:
            case TOK_VOID:
            case TOK_CHAR:
                return;
            default:
                advance();
        }
    }
}

/* Forward declarations */
Expr* parse_expression(void);  /* Exported for C++ parser */
static Expr* parse_assignment(void);
static Expr* parse_initializer(void);
static Expr* parse_unary(void);
static Stmt* parse_statement(void);
Stmt* parse_declaration(void);  /* Exported for C++ parser */
static Type* parse_type_spec(void);
static bool is_type_start(void);
static Type* parse_declarator(Type* base_type, const char** name,
                              DeclList** parameters);

static Type* generic_selection_type(Type* type) {
    if (!type) return NULL;
    if (type->kind == TYPE_ARRAY) return type_ptr(type->base);
    if (type->kind == TYPE_FUNC) return type_ptr(type);
    return type;
}

/* Evaluate the integer-constant-expression subset required by C17
 * _Static_assert.  Values retain their type so unsigned comparisons and
 * width-limited wrap follow the same target model as semantic analysis. */
typedef struct {
    uint64_t bits;
    Type* type;
} IntegerConstantValue;

static uint64_t integer_type_mask(const Type* type) {
    unsigned bits = type && type->size > 0 ? (unsigned)type->size * 8u : 0u;
    if (bits == 0u) return 0u;
    return bits >= 64u ? UINT64_MAX : (UINT64_C(1) << bits) - 1u;
}

static int64_t integer_constant_signed(IntegerConstantValue value) {
    unsigned bits = value.type && value.type->size > 0
        ? (unsigned)value.type->size * 8u : 64u;
    uint64_t masked = value.bits & integer_type_mask(value.type);
    if (bits < 64u && (masked & (UINT64_C(1) << (bits - 1u))) != 0u) {
        masked |= ~integer_type_mask(value.type);
    }
    return (int64_t)masked;
}

static IntegerConstantValue integer_constant_convert(
    IntegerConstantValue value, Type* target) {
    uint64_t bits = value.bits & integer_type_mask(value.type);
    if (value.type && !value.type->is_unsigned &&
        integer_constant_signed(value) < 0) {
        bits = (uint64_t)integer_constant_signed(value);
    }
    value.bits = bits & integer_type_mask(target);
    value.type = target;
    return value;
}

static bool eval_integer_constant_typed(Expr* expr,
                                        IntegerConstantValue* value) {
    IntegerConstantValue left;
    IntegerConstantValue right;
    Type* common;
    uint64_t count;
    unsigned width;

    if (!expr || !value) return false;
    switch (expr->kind) {
        case EXPR_INT_LIT:
            value->bits = (uint64_t)expr->int_val &
                          integer_type_mask(expr->type);
            value->type = expr->type ? expr->type : type_int;
            return true;
        case EXPR_CHAR_LIT:
            value->bits = (unsigned char)expr->char_val;
            value->type = type_int;
            return true;
        case EXPR_NEG:
        case EXPR_BITNOT:
            if (!eval_integer_constant_typed(expr->unary_operand, &left)) {
                return false;
            }
            common = left.type->kind < TYPE_INT ? type_int : left.type;
            left = integer_constant_convert(left, common);
            value->bits = expr->kind == EXPR_NEG ? 0u - left.bits
                                                 : ~left.bits;
            value->bits &= integer_type_mask(common);
            value->type = common;
            return true;
        case EXPR_NOT:
            if (!eval_integer_constant_typed(expr->unary_operand, &left)) {
                return false;
            }
            value->bits = (left.bits & integer_type_mask(left.type)) == 0u;
            value->type = type_int;
            return true;
        case EXPR_SIZEOF: {
            Type* measured = expr->sizeof_type;
            if (!measured && expr->unary_operand) {
                measured = expr->unary_operand->type;
            }
            if (!measured || measured->size <= 0) return false;
            value->bits = (uint64_t)measured->size;
            value->type = type_uint;
            return true;
        }
        case EXPR_ALIGNOF:
            if (!expr->sizeof_type || expr->sizeof_type->align <= 0) {
                return false;
            }
            value->bits = (uint64_t)expr->sizeof_type->align;
            value->type = type_uint;
            return true;
        case EXPR_CAST:
            if (!expr->cast_type || !type_is_integer(expr->cast_type) ||
                !eval_integer_constant_typed(expr->cast_expr, &left)) {
                return false;
            }
            *value = integer_constant_convert(left, expr->cast_type);
            return true;
        case EXPR_GENERIC: {
            Type* control = expr->generic_control
                ? generic_selection_type(expr->generic_control->type) : NULL;
            GenericAssociation* selected = NULL;
            GenericAssociation* fallback = NULL;
            if (!control) return false;
            for (GenericAssociation* association =
                     expr->generic_associations;
                 association; association = association->next) {
                if (!association->type) fallback = association;
                else if (type_is_compatible(control, association->type)) {
                    selected = association;
                }
            }
            selected = selected ? selected : fallback;
            return selected &&
                   eval_integer_constant_typed(selected->expr, value);
        }
        case EXPR_AND:
            if (!eval_integer_constant_typed(expr->binary_lhs, &left)) {
                return false;
            }
            if ((left.bits & integer_type_mask(left.type)) == 0u) {
                value->bits = 0u;
            } else {
                if (!eval_integer_constant_typed(expr->binary_rhs, &right)) {
                    return false;
                }
                value->bits =
                    (right.bits & integer_type_mask(right.type)) != 0u;
            }
            value->type = type_int;
            return true;
        case EXPR_OR:
            if (!eval_integer_constant_typed(expr->binary_lhs, &left)) {
                return false;
            }
            if ((left.bits & integer_type_mask(left.type)) != 0u) {
                value->bits = 1u;
            } else {
                if (!eval_integer_constant_typed(expr->binary_rhs, &right)) {
                    return false;
                }
                value->bits =
                    (right.bits & integer_type_mask(right.type)) != 0u;
            }
            value->type = type_int;
            return true;
        case EXPR_COND:
            if (!eval_integer_constant_typed(expr->cond_test, &left)) {
                return false;
            }
            return eval_integer_constant_typed(
                (left.bits & integer_type_mask(left.type)) != 0u
                    ? expr->cond_then : expr->cond_else,
                value);
        default:
            break;
    }

    switch (expr->kind) {
        case EXPR_ADD:
        case EXPR_SUB:
        case EXPR_MUL:
        case EXPR_DIV:
        case EXPR_MOD:
        case EXPR_BITAND:
        case EXPR_BITOR:
        case EXPR_BITXOR:
        case EXPR_LSHIFT:
        case EXPR_RSHIFT:
        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_GT:
        case EXPR_LE:
        case EXPR_GE:
        case EXPR_COMMA:
            break;
        default:
            return false;
    }
    if (!eval_integer_constant_typed(expr->binary_lhs, &left) ||
        !eval_integer_constant_typed(expr->binary_rhs, &right)) {
        return false;
    }
    if (expr->kind == EXPR_COMMA) {
        *value = right;
        return true;
    }
    if (expr->kind == EXPR_LSHIFT || expr->kind == EXPR_RSHIFT) {
        common = left.type->kind < TYPE_INT ? type_int : left.type;
        left = integer_constant_convert(left, common);
        if (right.type->is_unsigned) {
            count = right.bits & integer_type_mask(right.type);
        } else {
            int64_t signed_count = integer_constant_signed(right);
            if (signed_count < 0) return false;
            count = (uint64_t)signed_count;
        }
        width = (unsigned)common->size * 8u;
        if (count >= width) return false;
        if (expr->kind == EXPR_LSHIFT) {
            value->bits = (left.bits << (unsigned)count) &
                          integer_type_mask(common);
        } else if (common->is_unsigned) {
            value->bits = left.bits >> (unsigned)count;
        } else {
            value->bits = (uint64_t)(integer_constant_signed(left) >>
                                     (unsigned)count) &
                          integer_type_mask(common);
        }
        value->type = common;
        return true;
    }

    common = type_common(left.type, right.type);
    left = integer_constant_convert(left, common);
    right = integer_constant_convert(right, common);
    if (expr->kind >= EXPR_EQ && expr->kind <= EXPR_GE) {
        bool result;
        if (expr->kind == EXPR_EQ) result = left.bits == right.bits;
        else if (expr->kind == EXPR_NE) result = left.bits != right.bits;
        else if (common->is_unsigned) {
            if (expr->kind == EXPR_LT) result = left.bits < right.bits;
            else if (expr->kind == EXPR_GT) result = left.bits > right.bits;
            else if (expr->kind == EXPR_LE) result = left.bits <= right.bits;
            else result = left.bits >= right.bits;
        } else {
            int64_t signed_left = integer_constant_signed(left);
            int64_t signed_right = integer_constant_signed(right);
            if (expr->kind == EXPR_LT) result = signed_left < signed_right;
            else if (expr->kind == EXPR_GT) result = signed_left > signed_right;
            else if (expr->kind == EXPR_LE) result = signed_left <= signed_right;
            else result = signed_left >= signed_right;
        }
        value->bits = result;
        value->type = type_int;
        return true;
    }

    value->type = common;
    switch (expr->kind) {
        case EXPR_ADD: value->bits = left.bits + right.bits; break;
        case EXPR_SUB: value->bits = left.bits - right.bits; break;
        case EXPR_MUL: value->bits = left.bits * right.bits; break;
        case EXPR_BITAND: value->bits = left.bits & right.bits; break;
        case EXPR_BITOR: value->bits = left.bits | right.bits; break;
        case EXPR_BITXOR: value->bits = left.bits ^ right.bits; break;
        case EXPR_DIV:
        case EXPR_MOD:
            if (right.bits == 0u) return false;
            if (common->is_unsigned) {
                value->bits = expr->kind == EXPR_DIV
                    ? left.bits / right.bits : left.bits % right.bits;
            } else {
                int64_t signed_left = integer_constant_signed(left);
                int64_t signed_right = integer_constant_signed(right);
                int64_t minimum = common->size == 8
                    ? INT64_MIN
                    : -(INT64_C(1) << ((unsigned)common->size * 8u - 1u));
                if (signed_right == 0 ||
                    (signed_left == minimum && signed_right == -1)) {
                    return false;
                }
                value->bits = (uint64_t)(expr->kind == EXPR_DIV
                    ? signed_left / signed_right : signed_left % signed_right);
            }
            break;
        default:
            return false;
    }
    value->bits &= integer_type_mask(common);
    return true;
}

static bool eval_integer_constant(Expr* expr, int64_t* value) {
    IntegerConstantValue evaluated;
    if (!value || !eval_integer_constant_typed(expr, &evaluated)) return false;
    *value = evaluated.type && evaluated.type->is_unsigned
        ? (int64_t)(evaluated.bits & integer_type_mask(evaluated.type))
        : integer_constant_signed(evaluated);
    return true;
}

bool expr_eval_integer_constant(Expr* expr, int64_t* value) {
    return eval_integer_constant(expr, value);
}

static void skip_attributes(void) {
    while (match(TOK___ATTRIBUTE__)) {
        if (match(TOK_LPAREN)) {
            int depth = 1;
            while (!at_end() && depth > 0) {
                if (match(TOK_LPAREN)) depth++;
                else if (match(TOK_RPAREN)) depth--;
                else advance();
            }
        }
    }
}

/* ═══════════════════════════════════════
 * Expression Parsing
 * ═══════════════════════════════════════ */

static bool generic_association_type_valid(Type* type) {
    return type && type->kind != TYPE_VOID && type->kind != TYPE_FUNC &&
           type_is_complete(type);
}

static Expr* parse_generic_selection(SourceLoc loc) {
    Expr* control;
    GenericAssociation* associations = NULL;
    bool have_default = false;

    expect(TOK_LPAREN, "(");
    control = parse_assignment();
    expect(TOK_COMMA, ",");
    if (check(TOK_RPAREN)) {
        rcc_error(peek()->loc,
                  "generic selection requires at least one association");
    }
    while (!check(TOK_RPAREN) && !at_end()) {
        SourceLoc association_loc = peek()->loc;
        Type* association_type = NULL;
        Expr* association_expression;

        if (match(TOK_DEFAULT)) {
            if (have_default) {
                rcc_error(association_loc,
                          "generic selection has more than one default association");
            }
            have_default = true;
        } else if (is_type_start()) {
            association_type = parse_type_spec();
            association_type = parse_declarator(
                association_type, NULL, NULL);
            if (!generic_association_type_valid(association_type)) {
                rcc_error(association_loc,
                          "generic association requires a complete object type");
            }
            for (GenericAssociation* previous = associations; previous;
                 previous = previous->next) {
                if (previous->type &&
                    type_is_compatible(previous->type, association_type)) {
                    rcc_error(association_loc,
                              "generic selection has compatible duplicate types");
                    break;
                }
            }
        } else {
            rcc_error(association_loc,
                      "expected type name or default in generic association");
            if (!check(TOK_COLON)) advance();
        }
        expect(TOK_COLON, ":");
        association_expression = parse_assignment();
        generic_association_append(&associations, association_type,
                                   association_expression,
                                   association_loc);
        if (!match(TOK_COMMA)) break;
    }
    expect(TOK_RPAREN, ")");
    return expr_generic(control, associations, loc);
}

/* Primary: literal, identifier, (expr) */
static Expr* parse_primary(void) {
    SourceLoc loc = peek()->loc;

    if (match(TOK_GENERIC)) {
        return parse_generic_selection(loc);
    }
    if (match(TOK_INT_LIT)) {
        Token* literal = previous();
        return expr_integer_literal((uint64_t)literal->value.int_val,
                                    literal->int_base,
                                    literal->int_unsigned_suffix,
                                    literal->int_long_suffix, loc);
    }
    if (match(TOK_FLOAT_LIT)) {
        return expr_float(previous()->value.float_val, loc);
    }
    if (match(TOK_CHAR_LIT)) {
        return expr_char(previous()->value.char_val, loc);
    }
    if (match(TOK_STRING_LIT)) {
        return expr_string(previous()->value.str_val, loc);
    }
    if (match(TOK_IDENT)) {
        int64_t enum_value;
        if (parser_lookup_enum_constant(previous()->value.str_val,
                                        &enum_value)) {
            return expr_int(enum_value, loc);
        }
        return expr_ident(previous()->value.str_val, loc);
    }
    if (match(TOK_LPAREN)) {
        Expr* e = parse_expression();
        expect(TOK_RPAREN, ")");
        return e;
    }
    if (match(TOK__ALIGNOF)) {
        Type* type;
        expect(TOK_LPAREN, "(");
        if (!is_type_start()) {
            rcc_error(peek()->loc, "_Alignof requires a type name");
            type = type_int;
        } else {
            type = parse_type_spec();
            type = parse_declarator(type, NULL, NULL);
        }
        expect(TOK_RPAREN, ")");
        if (!type_is_complete(type) || type->kind == TYPE_FUNC) {
            rcc_error(loc, "_Alignof requires a complete object type");
        }
        return expr_alignof_type(type, loc);
    }
    if (match(TOK_SIZEOF)) {
        if (match(TOK_LPAREN)) {
            if (is_type_start()) {
                Type* type = parse_type_spec();
                type = parse_declarator(type, NULL, NULL);
                expect(TOK_RPAREN, ")");
                return expr_sizeof_type(type, loc);
            } else {
                Expr* e = parse_expression();
                expect(TOK_RPAREN, ")");
                return expr_sizeof_expr(e, loc);
            }
        } else {
            Expr* e = parse_unary();
            return expr_sizeof_expr(e, loc);
        }
    }

    rcc_error(loc, "expected expression");
    return expr_int(0, loc);
}

/* Postfix: a[i], a.m, a->m, a++, a--, f(args) */
static Expr* parse_postfix(void) {
    Expr* e = parse_primary();

    while (1) {
        SourceLoc loc = peek()->loc;

        if (match(TOK_LBRACKET)) {
            Expr* index = parse_expression();
            expect(TOK_RBRACKET, "]");
            e = expr_index(e, index, loc);
        } else if (match(TOK_DOT)) {
            Token* name = expect(TOK_IDENT, "member name");
            if (name) {
                e = expr_member(e, name->value.str_val, loc);
            }
        } else if (match(TOK_ARROW)) {
            Token* name = expect(TOK_IDENT, "member name");
            if (name) {
                Expr* m = expr_member(e, name->value.str_val, loc);
                m->kind = EXPR_PTR_MEMBER;
                e = m;
            }
        } else if (match(TOK_INC)) {
            e = expr_unary(EXPR_POSTINC, e, loc);
        } else if (match(TOK_DEC)) {
            e = expr_unary(EXPR_POSTDEC, e, loc);
        } else if (match(TOK_LPAREN)) {
            /* Function call */
            ExprList* args = NULL;
            if (!check(TOK_RPAREN)) {
                do {
                    Expr* arg = parse_assignment();
                    exprlist_append(&args, arg);
                } while (match(TOK_COMMA));
            }
            expect(TOK_RPAREN, ")");
            e = expr_call(e, args, loc);
        } else {
            break;
        }
    }

    return e;
}

/* Unary: ++a, --a, &a, *a, +a, -a, ~a, !a */
static Expr* parse_unary(void) {
    SourceLoc loc = peek()->loc;

    if (check(TOK_LPAREN) && parser.cur->next) {
        Token* saved_cur = parser.cur;
        Token* saved_prev = parser.prev;
        advance();
        if (is_type_start()) {
            Type* cast_type = parse_type_spec();
            cast_type = parse_declarator(cast_type, NULL, NULL);
            expect(TOK_RPAREN, ")");
            return expr_cast(cast_type, parse_unary(), loc);
        }
        parser.cur = saved_cur;
        parser.prev = saved_prev;
    }

    if (match(TOK_INC)) {
        Expr* e = parse_unary();
        return expr_unary(EXPR_PREINC, e, loc);
    }
    if (match(TOK_DEC)) {
        Expr* e = parse_unary();
        return expr_unary(EXPR_PREDEC, e, loc);
    }
    if (match(TOK_AMP)) {
        Expr* e = parse_unary();
        return expr_unary(EXPR_ADDR, e, loc);
    }
    if (match(TOK_STAR)) {
        Expr* e = parse_unary();
        return expr_unary(EXPR_DEREF, e, loc);
    }
    if (match(TOK_PLUS)) {
        return parse_unary();  /* Unary + is no-op */
    }
    if (match(TOK_MINUS)) {
        Expr* e = parse_unary();
        return expr_unary(EXPR_NEG, e, loc);
    }
    if (match(TOK_TILDE)) {
        Expr* e = parse_unary();
        return expr_unary(EXPR_BITNOT, e, loc);
    }
    if (match(TOK_NOT)) {
        Expr* e = parse_unary();
        return expr_unary(EXPR_NOT, e, loc);
    }

    return parse_postfix();
}

/* Multiplicative: a * b, a / b, a % b */
static Expr* parse_multiplicative(void) {
    Expr* e = parse_unary();

    while (1) {
        SourceLoc loc = peek()->loc;
        if (match(TOK_STAR)) {
            e = expr_binary(EXPR_MUL, e, parse_unary(), loc);
        } else if (match(TOK_SLASH)) {
            e = expr_binary(EXPR_DIV, e, parse_unary(), loc);
        } else if (match(TOK_PERCENT)) {
            e = expr_binary(EXPR_MOD, e, parse_unary(), loc);
        } else {
            break;
        }
    }

    return e;
}

/* Additive: a + b, a - b */
static Expr* parse_additive(void) {
    Expr* e = parse_multiplicative();

    while (1) {
        SourceLoc loc = peek()->loc;
        if (match(TOK_PLUS)) {
            e = expr_binary(EXPR_ADD, e, parse_multiplicative(), loc);
        } else if (match(TOK_MINUS)) {
            e = expr_binary(EXPR_SUB, e, parse_multiplicative(), loc);
        } else {
            break;
        }
    }

    return e;
}

/* Shift: a << b, a >> b */
static Expr* parse_shift(void) {
    Expr* e = parse_additive();

    while (1) {
        SourceLoc loc = peek()->loc;
        if (match(TOK_LSHIFT)) {
            e = expr_binary(EXPR_LSHIFT, e, parse_additive(), loc);
        } else if (match(TOK_RSHIFT)) {
            e = expr_binary(EXPR_RSHIFT, e, parse_additive(), loc);
        } else {
            break;
        }
    }

    return e;
}

/* Relational: a < b, a > b, a <= b, a >= b */
static Expr* parse_relational(void) {
    Expr* e = parse_shift();

    while (1) {
        SourceLoc loc = peek()->loc;
        if (match(TOK_LT)) {
            e = expr_binary(EXPR_LT, e, parse_shift(), loc);
        } else if (match(TOK_GT)) {
            e = expr_binary(EXPR_GT, e, parse_shift(), loc);
        } else if (match(TOK_LE)) {
            e = expr_binary(EXPR_LE, e, parse_shift(), loc);
        } else if (match(TOK_GE)) {
            e = expr_binary(EXPR_GE, e, parse_shift(), loc);
        } else {
            break;
        }
    }

    return e;
}

/* Equality: a == b, a != b */
static Expr* parse_equality(void) {
    Expr* e = parse_relational();

    while (1) {
        SourceLoc loc = peek()->loc;
        if (match(TOK_EQ)) {
            e = expr_binary(EXPR_EQ, e, parse_relational(), loc);
        } else if (match(TOK_NE)) {
            e = expr_binary(EXPR_NE, e, parse_relational(), loc);
        } else {
            break;
        }
    }

    return e;
}

/* Bitwise AND: a & b */
static Expr* parse_bitand(void) {
    Expr* e = parse_equality();

    while (match(TOK_AMP)) {
        SourceLoc loc = previous()->loc;
        e = expr_binary(EXPR_BITAND, e, parse_equality(), loc);
    }

    return e;
}

/* Bitwise XOR: a ^ b */
static Expr* parse_bitxor(void) {
    Expr* e = parse_bitand();

    while (match(TOK_CARET)) {
        SourceLoc loc = previous()->loc;
        e = expr_binary(EXPR_BITXOR, e, parse_bitand(), loc);
    }

    return e;
}

/* Bitwise OR: a | b */
static Expr* parse_bitor(void) {
    Expr* e = parse_bitxor();

    while (match(TOK_PIPE)) {
        SourceLoc loc = previous()->loc;
        e = expr_binary(EXPR_BITOR, e, parse_bitxor(), loc);
    }

    return e;
}

/* Logical AND: a && b */
static Expr* parse_logand(void) {
    Expr* e = parse_bitor();

    while (match(TOK_AND)) {
        SourceLoc loc = previous()->loc;
        e = expr_binary(EXPR_AND, e, parse_bitor(), loc);
    }

    return e;
}

/* Logical OR: a || b */
static Expr* parse_logor(void) {
    Expr* e = parse_logand();

    while (match(TOK_OR)) {
        SourceLoc loc = previous()->loc;
        e = expr_binary(EXPR_OR, e, parse_logand(), loc);
    }

    return e;
}

/* Conditional: a ? b : c */
static Expr* parse_conditional(void) {
    Expr* e = parse_logor();

    if (match(TOK_QUESTION)) {
        SourceLoc loc = previous()->loc;
        Expr* then_expr = parse_expression();
        expect(TOK_COLON, ":");
        Expr* else_expr = parse_conditional();
        e = expr_cond(e, then_expr, else_expr, loc);
    }

    return e;
}

/* Assignment: a = b, a += b, etc. */
static Expr* parse_assignment(void) {
    Expr* e = parse_conditional();

    SourceLoc loc = peek()->loc;
    ExprKind kind = -1;

    if (match(TOK_ASSIGN)) kind = EXPR_ASSIGN;
    else if (match(TOK_PLUS_ASSIGN)) kind = EXPR_ADD_ASSIGN;
    else if (match(TOK_MINUS_ASSIGN)) kind = EXPR_SUB_ASSIGN;
    else if (match(TOK_STAR_ASSIGN)) kind = EXPR_MUL_ASSIGN;
    else if (match(TOK_SLASH_ASSIGN)) kind = EXPR_DIV_ASSIGN;
    else if (match(TOK_PERCENT_ASSIGN)) kind = EXPR_MOD_ASSIGN;
    else if (match(TOK_AMP_ASSIGN)) kind = EXPR_AND_ASSIGN;
    else if (match(TOK_PIPE_ASSIGN)) kind = EXPR_OR_ASSIGN;
    else if (match(TOK_CARET_ASSIGN)) kind = EXPR_XOR_ASSIGN;
    else if (match(TOK_LSHIFT_ASSIGN)) kind = EXPR_LSHIFT_ASSIGN;
    else if (match(TOK_RSHIFT_ASSIGN)) kind = EXPR_RSHIFT_ASSIGN;

    if (kind != (ExprKind)-1) {
        Expr* rhs = parse_assignment();
        e = expr_binary(kind, e, rhs, loc);
    }

    return e;
}

/* Comma expression: a, b */
Expr* parse_expression(void) {
    Expr* e = parse_assignment();

    while (match(TOK_COMMA)) {
        SourceLoc loc = previous()->loc;
        e = expr_binary(EXPR_COMMA, e, parse_assignment(), loc);
    }

    return e;
}

/* ═══════════════════════════════════════
 * Type Parsing
 * ═══════════════════════════════════════ */

static bool is_type_start(void) {
    switch (peek()->type) {
        case TOK_TYPEDEF:
        case TOK_VOID:
        case TOK_CHAR:
        case TOK_SHORT:
        case TOK_INT:
        case TOK_LONG:
        case TOK_FLOAT:
        case TOK_DOUBLE:
        case TOK_SIGNED:
        case TOK_UNSIGNED:
        case TOK_STRUCT:
        case TOK_UNION:
        case TOK_ENUM:
        case TOK_CONST:
        case TOK_VOLATILE:
        case TOK_STATIC:
        case TOK_EXTERN:
        case TOK_THREAD_LOCAL:
        case TOK__BOOL:
        case TOK___ATTRIBUTE__:
        case TOK___INLINE__:
        case TOK_INLINE:
            return true;
        default:
            return check(TOK_IDENT) &&
                   parser_lookup_type(peek()->value.str_val) != NULL;
    }
}

typedef struct ParsedInitializerDesignator {
    InitDesignatorKind kind;
    int64_t index;
    const char* field;
    SourceLoc loc;
    struct ParsedInitializerDesignator* next;
} ParsedInitializerDesignator;

static Expr* parse_initializer(void) {
    ExprList* items = NULL;
    SourceLoc loc;
    if (!match(TOK_LBRACE)) return parse_assignment();
    loc = previous()->loc;
    if (check(TOK_RBRACE)) {
        rcc_error(loc, "empty initializer list is not valid C17");
    } else {
        for (;;) {
            ParsedInitializerDesignator* designators = NULL;
            ParsedInitializerDesignator** designator_tail = &designators;
            while (check(TOK_DOT) || check(TOK_LBRACKET)) {
                ParsedInitializerDesignator* designator =
                    rcc_alloc(sizeof(*designator));
                if (match(TOK_DOT)) {
                    Token* name;
                    designator->kind = INIT_DESIGNATOR_FIELD;
                    designator->loc = previous()->loc;
                    name = expect(TOK_IDENT, "field designator");
                    if (name) designator->field = name->value.str_val;
                } else {
                    Expr* index_expression;
                    match(TOK_LBRACKET);
                    designator->kind = INIT_DESIGNATOR_INDEX;
                    designator->loc = previous()->loc;
                    index_expression = parse_assignment();
                    if (!eval_integer_constant(index_expression,
                                               &designator->index) ||
                        designator->index < 0) {
                        rcc_error(
                            index_expression->loc,
                            "array designator must be a non-negative integer constant");
                        designator->index = 0;
                    }
                    expect(TOK_RBRACKET, "]");
                }
                *designator_tail = designator;
                designator_tail = &designator->next;
            }
            if (designators) {
                expect(TOK_ASSIGN, "=");
            }
            {
                Expr* value = parse_initializer();
                if (designators) {
                    ParsedInitializerDesignator* reversed = NULL;
                    ParsedInitializerDesignator* item = designators->next;
                    while (item) {
                        ParsedInitializerDesignator* next = item->next;
                        item->next = reversed;
                        reversed = item;
                        item = next;
                    }
                    while (reversed) {
                        ParsedInitializerDesignator* next = reversed->next;
                        ExprList* nested = NULL;
                        exprlist_append_designated(
                            &nested, value, reversed->kind,
                            reversed->index, reversed->field);
                        value = expr_initializer_list(nested, reversed->loc);
                        rcc_free(reversed);
                        reversed = next;
                    }
                    exprlist_append_designated(
                        &items, value, designators->kind,
                        designators->index, designators->field);
                    rcc_free(designators);
                } else {
                    exprlist_append_designated(
                        &items, value, INIT_DESIGNATOR_NONE, 0, NULL);
                }
            }
            if (!match(TOK_COMMA) || check(TOK_RBRACE)) break;
        }
    }
    expect(TOK_RBRACE, "}");
    return expr_initializer_list(items, loc);
}

static int parser_align_up(int value, int alignment) {
    if (alignment <= 1) return value;
    return (value + alignment - 1) & ~(alignment - 1);
}

static int64_t parse_enum_value(int64_t fallback) {
    bool negative = match(TOK_MINUS);
    int64_t value = fallback;
    if (match(TOK_INT_LIT)) {
        value = previous()->value.int_val;
    } else if (match(TOK_IDENT)) {
        if (!parser_lookup_enum_constant(previous()->value.str_val, &value)) {
            rcc_error(previous()->loc, "unknown enum constant '%s'",
                      previous()->value.str_val);
        }
    } else {
        rcc_error(peek()->loc, "expected integer enum value");
    }
    return negative ? -value : value;
}

static void parse_enum_body(void) {
    int64_t next_value = 0;
    while (!check(TOK_RBRACE) && !at_end()) {
        Token* name = expect(TOK_IDENT, "enumerator name");
        int64_t value = next_value;
        if (match(TOK_ASSIGN)) value = parse_enum_value(next_value);
        if (name) parser_define_enum_constant(name->value.str_val, value);
        next_value = value + 1;
        if (!match(TOK_COMMA)) break;
    }
    expect(TOK_RBRACE, "}");
}

static void parser_append_field(Type* aggregate, const char* name, Type* type) {
    TypeField* field = ast_arena_alloc(sizeof(*field));
    TypeField** tail = &aggregate->fields;
    int alignment = type && type->align > 0 ? type->align : 1;
    int size = type && type->size > 0 ? type->size : 0;
    field->name = name;
    field->type = type;
    while (*tail) tail = &(*tail)->next;
    if (aggregate->kind == TYPE_UNION) {
        field->offset = 0;
        if (size > aggregate->size) aggregate->size = size;
    } else {
        field->offset = parser_align_up(aggregate->size, alignment);
        aggregate->size = field->offset + size;
    }
    if (alignment > aggregate->align) aggregate->align = alignment;
    *tail = field;
}

static void parse_aggregate_body(Type* aggregate) {
    aggregate->size = 0;
    aggregate->align = 1;
    aggregate->fields = NULL;
    while (!check(TOK_RBRACE) && !at_end()) {
        Type* field_base;
        skip_attributes();
        field_base = parse_type_spec();
        do {
            const char* field_name = NULL;
            Type* field_type = parse_declarator(field_base, &field_name, NULL);
            if (!field_name) {
                rcc_error(peek()->loc, "expected field name");
                break;
            }
            parser_append_field(aggregate, field_name, field_type);
        } while (match(TOK_COMMA));
        expect(TOK_SEMICOLON, ";");
    }
    expect(TOK_RBRACE, "}");
    aggregate->size = parser_align_up(aggregate->size, aggregate->align);
    aggregate->is_complete = true;
}

static Type* parse_type_spec(void) {
    Type* t = NULL;
    bool is_unsigned = false;
    bool is_const = false;
    int long_count = 0;
    bool is_short = false;

    while (1) {
        if (match(TOK_CONST)) {
            is_const = true;
        } else if (match(TOK_VOLATILE)) {
            /* ignore for now */
        } else if (match(TOK_UNSIGNED)) {
            is_unsigned = true;
        } else if (match(TOK_SIGNED)) {
            is_unsigned = false;
        } else if (match(TOK_LONG)) {
            long_count++;
        } else if (match(TOK_SHORT)) {
            is_short = true;
        } else {
            break;
        }
    }

    if (match(TOK_VOID)) {
        t = type_void;
    } else if (match(TOK_CHAR)) {
        t = is_unsigned ? type_uchar : type_char;
    } else if (match(TOK_INT) || long_count > 0 || is_short) {
        if (is_short) {
            t = is_unsigned ? type_ushort : type_short;
        } else if (long_count > 1) {
            t = is_unsigned ? type_ullong : type_llong;
        } else if (long_count == 1) {
            t = is_unsigned ? type_ulong : type_long;
        } else {
            t = is_unsigned ? type_uint : type_int;
        }
    } else if (match(TOK_FLOAT)) {
        t = type_float;
    } else if (match(TOK_DOUBLE)) {
        t = type_double;
    } else if (match(TOK__BOOL)) {
        t = type_bool;
    } else if (match(TOK_STRUCT)) {
        Token* tag = NULL;
        if (check(TOK_IDENT)) {
            tag = advance();
        }
        t = parser_tag_type(TYPE_STRUCT, tag ? tag->value.str_val : NULL);
        if (match(TOK_LBRACE)) parse_aggregate_body(t);
    } else if (match(TOK_UNION)) {
        Token* tag = NULL;
        if (check(TOK_IDENT)) {
            tag = advance();
        }
        t = parser_tag_type(TYPE_UNION, tag ? tag->value.str_val : NULL);
        if (match(TOK_LBRACE)) parse_aggregate_body(t);
    } else if (match(TOK_ENUM)) {
        Token* tag = NULL;
        if (check(TOK_IDENT)) {
            tag = advance();
        }
        t = parser_tag_type(TYPE_ENUM, tag ? tag->value.str_val : NULL);
        if (match(TOK_LBRACE)) parse_enum_body();
    } else if (check(TOK_IDENT)) {
        t = parser_lookup_type(peek()->value.str_val);
        if (t) advance();
    } else {
        /* Default to int */
        t = is_unsigned ? type_uint : type_int;
    }

    if (is_const && t) {
        /* Make a copy with const flag */
        Type* ct = ast_arena_alloc(sizeof(Type));
        *ct = *t;
        ct->is_const = true;
        t = ct;
    }

    return t;
}

static TypeParam* parser_type_params(DeclList* parameters, bool* variadic) {
    TypeParam* head = NULL;
    TypeParam** tail = &head;
    DeclList* item;
    (void)variadic;
    for (item = parameters; item; item = item->next) {
        TypeParam* param = ast_arena_alloc(sizeof(*param));
        param->name = item->decl->name;
        param->type = item->decl->type;
        *tail = param;
        tail = &param->next;
    }
    return head;
}

static DeclList* parse_parameter_list(bool* variadic) {
    DeclList* parameters = NULL;
    int parameter_index = 0;
    *variadic = false;
    if (check(TOK_VOID) && parser.cur->next &&
        parser.cur->next->type == TOK_RPAREN) {
        advance();
        return NULL;
    }
    while (!check(TOK_RPAREN) && !at_end()) {
        Type* parameter_base;
        Type* parameter_type;
        const char* parameter_name = NULL;
        if (match(TOK_ELLIPSIS)) {
            *variadic = true;
            break;
        }
        parameter_base = parse_type_spec();
        parameter_type = parse_declarator(parameter_base, &parameter_name, NULL);
        if (parameter_type->kind == TYPE_ARRAY) {
            parameter_type = type_ptr(parameter_type->base);
        }
        decllist_append(&parameters,
                        decl_param(parameter_name, parameter_type,
                                   parameter_index++, peek()->loc));
        if (!match(TOK_COMMA)) break;
    }
    return parameters;
}

static Type* parse_declarator(Type* base_type, const char** name,
                              DeclList** parameters) {
    Type* type = base_type;
    int pointer_count = 0;
    if (name) *name = NULL;
    if (parameters) *parameters = NULL;

    while (match(TOK_STAR)) {
        pointer_count++;
        while (match(TOK_CONST) || match(TOK_VOLATILE) || match(TOK_RESTRICT)) {}
    }

    /* Function-pointer declarator: return_type (*name)(parameters). */
    if (check(TOK_LPAREN) && parser.cur->next &&
        parser.cur->next->type == TOK_STAR) {
        int nested_pointers = 0;
        DeclList* function_parameters = NULL;
        bool variadic = false;
        advance();
        while (match(TOK_STAR)) nested_pointers++;
        if (check(TOK_IDENT)) {
            Token* identifier = advance();
            if (name) *name = identifier->value.str_val;
        }
        expect(TOK_RPAREN, ")");
        expect(TOK_LPAREN, "(");
        function_parameters = parse_parameter_list(&variadic);
        expect(TOK_RPAREN, ")");
        type = type_func(base_type,
                         parser_type_params(function_parameters, &variadic),
                         variadic);
        while (nested_pointers-- > 0) type = type_ptr(type);
        while (pointer_count-- > 0) type = type_ptr(type);
        if (parameters) *parameters = function_parameters;
        return type;
    }

    while (pointer_count-- > 0) type = type_ptr(type);
    if (check(TOK_IDENT)) {
        Token* identifier = advance();
        if (name) *name = identifier->value.str_val;
    }

    for (;;) {
        if (match(TOK_LBRACKET)) {
            int length = -1;
            if (match(TOK_INT_LIT)) length = (int)previous()->value.int_val;
            expect(TOK_RBRACKET, "]");
            type = type_array(type, length);
        } else if (match(TOK_LPAREN)) {
            bool variadic = false;
            DeclList* function_parameters = parse_parameter_list(&variadic);
            expect(TOK_RPAREN, ")");
            type = type_func(type,
                             parser_type_params(function_parameters, &variadic),
                             variadic);
            if (parameters) *parameters = function_parameters;
        } else {
            break;
        }
    }
    return type;
}

/* ═══════════════════════════════════════
 * Statement Parsing
 * ═══════════════════════════════════════ */

static Stmt* parse_block(void) {
    SourceLoc loc = previous()->loc;
    StmtList* stmts = NULL;

    while (!check(TOK_RBRACE) && !at_end()) {
        Stmt* s = parse_declaration();
        if (s) {
            stmtlist_append(&stmts, s);
        }
    }

    expect(TOK_RBRACE, "}");
    return stmt_block(stmts, loc);
}

static Stmt* parse_if_stmt(void) {
    SourceLoc loc = previous()->loc;
    expect(TOK_LPAREN, "(");
    Expr* cond = parse_expression();
    expect(TOK_RPAREN, ")");

    Stmt* then_stmt = parse_statement();
    Stmt* else_stmt = NULL;

    if (match(TOK_ELSE)) {
        else_stmt = parse_statement();
    }

    return stmt_if(cond, then_stmt, else_stmt, loc);
}

static Stmt* parse_while_stmt(void) {
    SourceLoc loc = previous()->loc;
    expect(TOK_LPAREN, "(");
    Expr* cond = parse_expression();
    expect(TOK_RPAREN, ")");
    Stmt* body = parse_statement();
    return stmt_while(cond, body, loc);
}

static Stmt* parse_do_stmt(void) {
    SourceLoc loc = previous()->loc;
    Stmt* body = parse_statement();
    expect(TOK_WHILE, "while");
    expect(TOK_LPAREN, "(");
    Expr* cond = parse_expression();
    expect(TOK_RPAREN, ")");
    expect(TOK_SEMICOLON, ";");
    return stmt_do(body, cond, loc);
}

static Stmt* parse_for_stmt(void) {
    SourceLoc loc = previous()->loc;
    expect(TOK_LPAREN, "(");

    Stmt* init = NULL;
    if (!check(TOK_SEMICOLON)) {
        if (is_type_start()) {
            init = parse_declaration();
        } else {
            Expr* e = parse_expression();
            expect(TOK_SEMICOLON, ";");
            init = stmt_expr(e, e->loc);
        }
    } else {
        advance();  /* consume ; */
    }

    Expr* cond = NULL;
    if (!check(TOK_SEMICOLON)) {
        cond = parse_expression();
    }
    expect(TOK_SEMICOLON, ";");

    Expr* inc = NULL;
    if (!check(TOK_RPAREN)) {
        inc = parse_expression();
    }
    expect(TOK_RPAREN, ")");

    Stmt* body = parse_statement();
    return stmt_for(init, cond, inc, body, loc);
}

static Stmt* parse_switch_stmt(void) {
    SourceLoc loc = previous()->loc;
    expect(TOK_LPAREN, "(");
    Expr* expr = parse_expression();
    expect(TOK_RPAREN, ")");
    Stmt* body = parse_statement();
    return stmt_switch(expr, body, loc);
}

static Stmt* parse_return_stmt(void) {
    SourceLoc loc = previous()->loc;
    Expr* val = NULL;

    if (!check(TOK_SEMICOLON)) {
        val = parse_expression();
    }
    expect(TOK_SEMICOLON, ";");

    return stmt_return(val, loc);
}

static Stmt* parse_statement(void) {
    SourceLoc loc = peek()->loc;

    if (match(TOK_LBRACE)) {
        return parse_block();
    }
    if (match(TOK_IF)) {
        return parse_if_stmt();
    }
    if (match(TOK_WHILE)) {
        return parse_while_stmt();
    }
    if (match(TOK_DO)) {
        return parse_do_stmt();
    }
    if (match(TOK_FOR)) {
        return parse_for_stmt();
    }
    if (match(TOK_SWITCH)) {
        return parse_switch_stmt();
    }
    if (match(TOK_CASE)) {
        Expr* val = parse_expression();
        expect(TOK_COLON, ":");
        Stmt* s = parse_statement();
        return stmt_case(val, s, loc);
    }
    if (match(TOK_DEFAULT)) {
        expect(TOK_COLON, ":");
        Stmt* s = parse_statement();
        return stmt_default(s, loc);
    }
    if (match(TOK_BREAK)) {
        expect(TOK_SEMICOLON, ";");
        return stmt_break(loc);
    }
    if (match(TOK_CONTINUE)) {
        expect(TOK_SEMICOLON, ";");
        return stmt_continue(loc);
    }
    if (match(TOK_RETURN)) {
        return parse_return_stmt();
    }
    if (match(TOK_GOTO)) {
        Token* label = expect(TOK_IDENT, "label");
        expect(TOK_SEMICOLON, ";");
        return stmt_goto(label ? label->value.str_val : "", loc);
    }
    if (match(TOK_SEMICOLON)) {
        return stmt_null(loc);
    }

    /* Inline assembly: asm [volatile] ( "template" [: outputs [: inputs [: clobbers]]] ) ; */
    if (match(TOK_ASM)) {
        bool is_volatile = false;
        if (match(TOK___VOLATILE__) || match(TOK_VOLATILE)) {
            is_volatile = true;
        }

        expect(TOK_LPAREN, "(");

        /* Parse template string */
        Token* templ_tok = expect(TOK_STRING_LIT, "assembly template");
        const char* templ = templ_tok ? templ_tok->value.str_val : "";

        AsmOperand* outputs = NULL;
        AsmOperand* inputs = NULL;
        AsmClobber* clobbers = NULL;

        /* Parse output operands */
        if (match(TOK_COLON)) {
            AsmOperand** out_tail = &outputs;
            while (check(TOK_STRING_LIT)) {
                Token* constraint = advance();
                expect(TOK_LPAREN, "(");
                Expr* expr = parse_expression();
                expect(TOK_RPAREN, ")");

                AsmOperand* op = asm_operand_new(constraint->value.str_val, expr);
                *out_tail = op;
                out_tail = &op->next;

                if (!match(TOK_COMMA)) break;
            }

            /* Parse input operands */
            if (match(TOK_COLON)) {
                AsmOperand** in_tail = &inputs;
                while (check(TOK_STRING_LIT)) {
                    Token* constraint = advance();
                    expect(TOK_LPAREN, "(");
                    Expr* expr = parse_expression();
                    expect(TOK_RPAREN, ")");

                    AsmOperand* op = asm_operand_new(constraint->value.str_val, expr);
                    *in_tail = op;
                    in_tail = &op->next;

                    if (!match(TOK_COMMA)) break;
                }

                /* Parse clobbers */
                if (match(TOK_COLON)) {
                    AsmClobber** clob_tail = &clobbers;
                    while (check(TOK_STRING_LIT)) {
                        Token* reg = advance();
                        AsmClobber* cl = asm_clobber_new(reg->value.str_val);
                        *clob_tail = cl;
                        clob_tail = &cl->next;

                        if (!match(TOK_COMMA)) break;
                    }
                }
            }
        }

        expect(TOK_RPAREN, ")");
        expect(TOK_SEMICOLON, ";");
        return stmt_asm(templ, outputs, inputs, clobbers, is_volatile, loc);
    }

    /* __extension__ - just skip it */
    if (match(TOK___EXTENSION__)) {
        return parse_statement();
    }

    /* Label? */
    if (check(TOK_IDENT) && parser.cur->next && parser.cur->next->type == TOK_COLON) {
        Token* name = advance();
        advance();  /* consume : */
        Stmt* s = parse_statement();
        return stmt_label(name->value.str_val, s, loc);
    }

    /* Expression statement */
    Expr* e = parse_expression();
    expect(TOK_SEMICOLON, ";");
    return stmt_expr(e, loc);
}

/* ═══════════════════════════════════════
 * Declaration Parsing
 * ═══════════════════════════════════════ */

Stmt* parse_declaration(void) {
    bool is_typedef = false;
    bool is_inline = false;
    bool is_thread_local = false;
    const char* declaration_name = NULL;
    DeclList* parameters = NULL;
    Type* base_type;
    Type* type;
    Decl* declaration;

    if (check(TOK_STATIC_ASSERT)) {
        SourceLoc assertion_loc = advance()->loc;
        Expr* condition;
        Token* message = NULL;
        int64_t condition_value = 0;

        expect(TOK_LPAREN, "(");
        condition = parse_assignment();
        if (match(TOK_COMMA)) {
            message = expect(TOK_STRING_LIT, "static assertion message");
        }
        expect(TOK_RPAREN, ")");
        expect(TOK_SEMICOLON, ";");
        if (!eval_integer_constant(condition, &condition_value)) {
            rcc_error(assertion_loc,
                      "static assertion is not an integer constant expression");
        } else if (condition_value == 0) {
            rcc_error(assertion_loc, "static assertion failed%s%s",
                      message ? ": " : "",
                      message ? message->value.str_val : "");
        }
        return stmt_null(assertion_loc);
    }

    skip_attributes();
    if (!is_type_start()) {
        return parse_statement();
    }

    SourceLoc loc = peek()->loc;
    StorageClass storage = STORAGE_NONE;

    /* Declaration specifiers may legally appear in either order. */
    for (;;) {
        if (match(TOK_TYPEDEF)) is_typedef = true;
        else if (match(TOK_STATIC)) storage = STORAGE_STATIC;
        else if (match(TOK_EXTERN)) storage = STORAGE_EXTERN;
        else if (match(TOK_REGISTER)) storage = STORAGE_REGISTER;
        else if (match(TOK_AUTO)) storage = STORAGE_AUTO;
        else if (match(TOK_THREAD_LOCAL)) is_thread_local = true;
        else if (match(TOK_INLINE) || match(TOK___INLINE__)) is_inline = true;
        else break;
    }

    base_type = parse_type_spec();
    if (!base_type) {
        rcc_error(loc, "expected type specifier");
        synchronize();
        return NULL;
    }

    /* A standalone aggregate declaration has no declarator. Enum constants
     * were registered while parsing its body. */
    if (match(TOK_SEMICOLON)) return stmt_null(loc);

    type = parse_declarator(base_type, &declaration_name, &parameters);
    skip_attributes();
    if (!declaration_name) {
        rcc_error(loc, "expected identifier");
        synchronize();
        return NULL;
    }

    if (is_typedef) {
        if (is_thread_local) {
            rcc_error(loc, "thread-local storage is not valid on a typedef");
        }
        expect(TOK_SEMICOLON, ";");
        parser_define_type(declaration_name, type);
        return stmt_decl(decl_typedef(declaration_name, type, loc), loc);
    }

    /* Function declaration? */
    if (type->kind == TYPE_FUNC) {
        Stmt* body = NULL;
        if (is_thread_local) {
            rcc_error(loc, "thread-local storage is not valid on a function");
        }
        if (match(TOK_LBRACE)) {
            body = parse_block();
        } else {
            expect(TOK_SEMICOLON, ";");
        }

        declaration = decl_func(declaration_name, type, parameters, body, loc);
        declaration->storage = storage;
        declaration->func_is_inline = is_inline;
        return stmt_decl(declaration, loc);
    }

    /* Variable initializer */
    Expr* init = NULL;
    if (match(TOK_ASSIGN)) {
        init = parse_initializer();
    }

    expect(TOK_SEMICOLON, ";");

    declaration = decl_var(declaration_name, type, init, loc);
    declaration->storage = storage;
    declaration->var_is_thread_local = is_thread_local;
    return stmt_decl(declaration, loc);
}

/* ═══════════════════════════════════════
 * Top-level Parsing
 * ═══════════════════════════════════════ */

static Decl* parse_toplevel(void) {
    Stmt* s = parse_declaration();
    if (s && s->kind == STMT_DECL) {
        return s->decl;
    }
    return NULL;
}

/* Main parser function */
AST* rcc_parse(TokenList* tokens) {
    parser.cur = tokens->head;
    parser.prev = NULL;
    parser_type_names = NULL;
    parser_tag_names = NULL;
    parser_enum_constants = NULL;

    AST* ast = ast_new();

    while (!at_end()) {
        Token* iteration_start = parser.cur;
        int errors_before = g_error_count;
        Decl* d = parse_toplevel();
        if (d) {
            ast_add_decl(ast, d);
        }
        if (g_error_count > errors_before && parser.cur == iteration_start) {
            synchronize();
        }
        if (parser.cur == iteration_start && !at_end()) advance();
    }

    return ast;
}
