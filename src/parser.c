/*
 * RCC - RinOS C Compiler
 * Parser (Recursive Descent)
 */

#include "rcc.h"
#include "token.h"
#include "ast.h"
#include <limits.h>

/* Parser state - exported for parser_cxx.c */
typedef struct {
    Token* cur;
    Token* prev;
} Parser;

Parser parser;  /* Non-static for C++ parser access */

/* The C frontend and C++ frontend share this parser translation unit.  Keep
 * the C-only executable independent of parser_cxx.c without providing fake
 * language implementations: an absent C++ hook is a missing extension and
 * the caller's ordinary diagnostic path remains responsible for the token. */
#if defined(__GNUC__)
#define RCC_OPTIONAL_CXX __attribute__((weak))
#else
#define RCC_OPTIONAL_CXX
#endif
extern Type* rcc_parse_cxx_direct_list_type(void) RCC_OPTIONAL_CXX;
extern Type* rcc_parse_cxx_type_name(void) RCC_OPTIONAL_CXX;
extern bool rcc_parse_cxx_type_start(void) RCC_OPTIONAL_CXX;
extern Expr* rcc_parse_cxx_template_call(void) RCC_OPTIONAL_CXX;
extern Stmt* rcc_parse_cxx_auto_local_declaration(void) RCC_OPTIONAL_CXX;
extern Stmt* rcc_parse_cxx_class_local_declaration(
    Type* base_type, int storage, bool is_thread_local,
    SourceLoc loc) RCC_OPTIONAL_CXX;
extern Stmt* rcc_parse_cxx_operator_declaration(
    Type* return_type, SourceLoc loc) RCC_OPTIONAL_CXX;
extern Expr* rcc_parser_cxx_capture_expression(
    const char* name, SourceLoc loc) RCC_OPTIONAL_CXX;
extern Expr* rcc_parse_cxx_special_expression(void) RCC_OPTIONAL_CXX;
extern Expr* rcc_parse_cxx_lambda(void) RCC_OPTIONAL_CXX;
extern Stmt* rcc_parse_cxx_statement(void) RCC_OPTIONAL_CXX;
extern Stmt* rcc_parse_cxx_range_for_statement(void) RCC_OPTIONAL_CXX;
extern void rcc_parser_cxx_begin_function_parameters(DeclList* parameters)
    RCC_OPTIONAL_CXX;
extern void rcc_parser_cxx_end_function_parameters(void) RCC_OPTIONAL_CXX;
extern void rcc_parser_cxx_add_value_binding(const char* name, Type* type)
    RCC_OPTIONAL_CXX;

typedef struct ParserTypeName {
    const char* name;
    Type* type;
    uint32_t cxx_constructor_arity_mask;
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
    Type* type;
    struct ParserEnumConstant* next;
} ParserEnumConstant;

static ParserTypeName* parser_type_names;
static ParserTagName* parser_tag_names;
static ParserEnumConstant* parser_enum_constants;
static Type* parser_builtin_va_list_type;
static int parser_pack_alignment;
static int parser_pack_stack[32];
static int parser_pack_depth;
static bool parser_cxx_mode;
static bool parser_cxx_template_default_mode;

void rcc_parser_set_cxx_mode(bool enabled) {
    parser_cxx_mode = enabled;
}

bool rcc_parser_is_cxx_mode(void) {
    return parser_cxx_mode;
}

void rcc_parser_set_cxx_template_default_mode(bool enabled) {
    parser_cxx_template_default_mode = enabled;
}

static bool parser_pack_value_valid(int alignment) {
    return alignment == 0 || alignment == 1 || alignment == 2 ||
           alignment == 4 || alignment == 8 || alignment == 16;
}

static void parser_apply_pack(Token* directive) {
    int value = (int)directive->value.int_val;
    if (value == -1) {
        if (parser_pack_depth == 0) {
            rcc_warning(directive->loc, "#pragma pack(pop) without push");
        } else {
            parser_pack_alignment = parser_pack_stack[--parser_pack_depth];
        }
        return;
    }
    if (value >= 256) {
        int alignment = value - 256;
        if (!parser_pack_value_valid(alignment)) {
            rcc_error(directive->loc,
                      "unsupported #pragma pack alignment %d", alignment);
            return;
        }
        if (parser_pack_depth >=
            (int)(sizeof(parser_pack_stack) / sizeof(parser_pack_stack[0]))) {
            rcc_error(directive->loc, "#pragma pack stack overflow");
            return;
        }
        parser_pack_stack[parser_pack_depth++] = parser_pack_alignment;
        if (alignment != 0) parser_pack_alignment = alignment;
        return;
    }
    if (!parser_pack_value_valid(value)) {
        rcc_error(directive->loc,
                  "unsupported #pragma pack alignment %d", value);
        return;
    }
    parser_pack_alignment = value;
}

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
    entry->cxx_constructor_arity_mask = 0u;
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

static void parser_define_enum_constant(const char* name, int64_t value,
                                        Type* type) {
    ParserEnumConstant* entry = ast_arena_alloc(sizeof(*entry));
    entry->name = name;
    entry->value = value;
    entry->type = type;
    entry->next = parser_enum_constants;
    parser_enum_constants = entry;
}

static bool parser_lookup_enum_constant(const char* name, int64_t* value,
                                        Type** type) {
    ParserEnumConstant* entry;
    for (entry = parser_enum_constants; entry; entry = entry->next) {
        if (strcmp(entry->name, name) == 0) {
            if (value) *value = entry->value;
            if (type) *type = entry->type;
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
    if (at_end()) return;
    advance();
    while (!at_end()) {
        if (previous()->type == TOK_SEMICOLON) return;
        switch (peek()->type) {
            case TOK_RBRACE:
            case TOK_IF:
            case TOK_WHILE:
            case TOK_DO:
            case TOK_FOR:
            case TOK_SWITCH:
            case TOK_BREAK:
            case TOK_CONTINUE:
            case TOK_GOTO:
            case TOK_RETURN:
            case TOK_TYPEDEF:
            case TOK_INT:
            case TOK_VOID:
            case TOK_CHAR:
            case TOK_SHORT:
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

/* C++ field parsing reuses the complete shared initializer grammar. */
Expr* rcc_parser_parse_initializer(void);
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
            if (expr->is_cxx_nullptr) return false;
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

static TypeField* parser_find_field(Type* aggregate, const char* name) {
    for (TypeField* field = aggregate ? aggregate->fields : NULL;
         field; field = field->next) {
        if (field->name && strcmp(field->name, name) == 0) return field;
    }
    return NULL;
}

Type* rcc_parser_lookup_type(const char* name) {
    return parser_lookup_type(name);
}

void rcc_parser_define_type(const char* name, Type* type) {
    if (name && type) parser_define_type(name, type);
}

void rcc_parser_define_cxx_constructor_type(const char* name, Type* type,
                                            uint32_t arity_mask) {
    ParserTypeName* entry;
    if (!name || !type || arity_mask == 0u) return;
    parser_define_type(name, type);
    entry = parser_type_names;
    entry->cxx_constructor_arity_mask = arity_mask;
}

uint32_t rcc_parser_cxx_constructor_arity_mask(Type* type) {
    ParserTypeName* entry;
    for (entry = parser_type_names; entry; entry = entry->next) {
        if (entry->type == type && entry->cxx_constructor_arity_mask != 0u) {
            return entry->cxx_constructor_arity_mask;
        }
    }
    return 0u;
}

void rcc_parser_validate_cxx_constructor_initializer(Type* type,
                                                     Expr* initializer) {
    uint32_t mask;
    unsigned arity = 0u;
    ExprList* item;
    if (!parser_cxx_mode || !type || type->cxx_dependent || !initializer ||
        initializer->kind != EXPR_COMPOUND) {
        return;
    }
    if (initializer->compound_init &&
        !initializer->compound_init->next &&
        initializer->compound_init->designator_kind ==
            INIT_DESIGNATOR_NONE &&
        initializer->compound_init->expr &&
        initializer->compound_init->expr->kind == EXPR_CAST) {
        Expr* cast = initializer->compound_init->expr;
        Type* cast_type = cast->cast_type;
        if (cast_type && cast_type->kind == TYPE_PTR &&
            cast_type->is_reference && cast_type->is_rvalue_reference &&
            cast_type->base && type_is_compatible(cast_type->base, type)) {
            TypeMethod* move = type->move_constructor_method;
            Expr* source = cast->cast_expr;
            if (!move || !move->name || !source ||
                source->kind != EXPR_IDENT) {
                rcc_error(cast->loc,
                          "C++ move construction requires a validated release constructor");
                return;
            }
            initializer->compound_init->expr = expr_call(
                expr_member(source, move->name, cast->loc), NULL, cast->loc);
            return;
        }
    }
    mask = rcc_parser_cxx_constructor_arity_mask(type);
    if (mask == 0u) return;
    if (!initializer->compound_value_init) {
        for (item = initializer->compound_init; item; item = item->next) {
            if (arity < 32u) ++arity;
        }
    }
    if (arity >= 32u || (mask & (UINT32_C(1) << arity)) == 0u) {
        rcc_error(initializer->loc,
                  "no safely lowerable constructor accepts %u argument%s",
                  arity, arity == 1u ? "" : "s");
    }
}

static Expr* parse_builtin_offsetof(SourceLoc loc) {
    Type* current;
    int64_t offset = 0;

    advance(); /* __builtin_offsetof */
    expect(TOK_LPAREN, "(");
    if (!is_type_start()) {
        rcc_error(peek()->loc, "__builtin_offsetof requires a type name");
        current = type_int;
    } else {
        current = parse_type_spec();
        current = parse_declarator(current, NULL, NULL);
    }
    expect(TOK_COMMA, ",");

    if (!current || (current->kind != TYPE_STRUCT &&
                     current->kind != TYPE_UNION)) {
        rcc_error(loc, "__builtin_offsetof requires an aggregate type");
    }
    while (!check(TOK_RPAREN) && !at_end()) {
        if (match(TOK_IDENT)) {
            TypeField* field = parser_find_field(
                current, previous()->value.str_val);
            if (!field) {
                rcc_error(previous()->loc, "unknown member '%s' in offsetof",
                          previous()->value.str_val);
                current = type_int;
            } else {
                offset += field->offset;
                if (field->is_bitfield) {
                    rcc_error(previous()->loc,
                              "cannot compute offsetof for a bit-field");
                }
                current = field->type;
            }
        } else if (match(TOK_LBRACKET)) {
            Expr* index_expression = parse_assignment();
            int64_t index = 0;
            if (!current || current->kind != TYPE_ARRAY) {
                rcc_error(previous()->loc,
                          "offsetof subscript requires an array member");
            } else if (!eval_integer_constant(index_expression, &index) ||
                       index < 0) {
                rcc_error(index_expression->loc,
                          "offsetof subscript is not a non-negative integer constant");
            } else {
                offset += index * current->base->size;
                current = current->base;
            }
            expect(TOK_RBRACKET, "]");
        } else {
            rcc_error(peek()->loc, "expected member designator in offsetof");
            advance();
        }
        if (!match(TOK_DOT) && !check(TOK_LBRACKET)) break;
    }
    expect(TOK_RPAREN, ")");

    Expr* result = expr_int(offset, loc);
    result->type = g_opts.target_arch == ARCH_X64 ? type_ullong : type_uint;
    return result;
}

static bool parser_builtin_name(const char* name) {
    return check(TOK_IDENT) &&
           strcmp(peek()->value.str_val, name) == 0;
}

static Expr* parse_builtin_vararg(SourceLoc loc) {
    const char* name = advance()->value.str_val;
    ExprKind kind = strcmp(name, "__builtin_va_start") == 0
        ? EXPR_VA_START : strcmp(name, "__builtin_va_end") == 0
            ? EXPR_VA_END : strcmp(name, "__builtin_va_copy") == 0
                ? EXPR_VA_COPY : EXPR_VA_ARG;
    Expr* list;
    Expr* second = NULL;
    Type* argument_type = NULL;

    expect(TOK_LPAREN, "(");
    list = parse_assignment();
    if (kind == EXPR_VA_START || kind == EXPR_VA_COPY ||
        kind == EXPR_VA_ARG) {
        expect(TOK_COMMA, ",");
        if (kind == EXPR_VA_ARG) {
            if (!is_type_start()) {
                rcc_error(peek()->loc,
                          "__builtin_va_arg requires a type name");
                argument_type = type_int;
            } else {
                argument_type = parse_type_spec();
                argument_type = parse_declarator(argument_type, NULL, NULL);
            }
        } else {
            second = parse_assignment();
        }
    }
    expect(TOK_RPAREN, ")");
    return expr_vararg(kind, list, second, argument_type, loc);
}

static bool qualified_name_append(char* buffer, size_t capacity,
                                  size_t* length, const char* text,
                                  SourceLoc loc) {
    size_t text_length = strlen(text);
    if (*length > capacity - 1u ||
        text_length > capacity - 1u - *length) {
        rcc_error(loc, "qualified identifier exceeds compiler limit");
        return false;
    }
    memcpy(buffer + *length, text, text_length);
    *length += text_length;
    buffer[*length] = '\0';
    return true;
}

/* C++ expression parsing shares the mature C precedence parser.  Preserve a
 * qualified-id as one lookup key before the postfix/call layers consume it. */
static const char* parse_expression_qualified_name(SourceLoc loc) {
    char buffer[512] = "";
    size_t length = 0u;

    /* A leading global-scope operator does not change the canonical lookup
     * key stored in the AST. */
    (void)match(TOK_SCOPE);
    if (!check(TOK_IDENT)) {
        rcc_error(peek()->loc, "expected identifier in qualified name");
        return rcc_intern(buffer);
    }
    if (!qualified_name_append(buffer, sizeof(buffer), &length,
                               advance()->value.str_val, loc)) {
        return rcc_intern(buffer);
    }
    while (match(TOK_SCOPE)) {
        if (!qualified_name_append(buffer, sizeof(buffer), &length, "::", loc)) {
            return rcc_intern(buffer);
        }
        if (!check(TOK_IDENT)) {
            rcc_error(peek()->loc, "expected identifier after ::");
            break;
        }
        if (!qualified_name_append(buffer, sizeof(buffer), &length,
                                   advance()->value.str_val, loc)) {
            break;
        }
    }
    return rcc_intern(buffer);
}

/* Primary: literal, identifier, (expr) */
static Expr* parse_primary(void) {
    SourceLoc loc = peek()->loc;

    if (parser_cxx_mode && rcc_parse_cxx_special_expression &&
        (check(TOK_NEW) || check(TOK_DELETE))) {
        return rcc_parse_cxx_special_expression();
    }
    if (parser_cxx_mode && rcc_parse_cxx_lambda && check(TOK_LBRACKET)) {
        return rcc_parse_cxx_lambda();
    }
    if (parser_cxx_mode && match(TOK_THIS)) {
        return expr_ident("this", loc);
    }
    if (parser_cxx_mode && match(TOK_NULLPTR)) {
        Expr* null_pointer = expr_int(0, loc);
        null_pointer->type = type_nullptr;
        null_pointer->is_cxx_nullptr = true;
        return null_pointer;
    }
    if (parser_cxx_mode && match(TOK_TRUE)) {
        return expr_int(1, loc);
    }
    if (parser_cxx_mode && match(TOK_FALSE)) {
        return expr_int(0, loc);
    }
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
        Token* literal = previous();
        Expr* expression = expr_float(literal->value.float_val, loc);
        if (literal->float_suffix) expression->type = type_float;
        return expression;
    }
    if (match(TOK_CHAR_LIT)) {
        return expr_char(previous()->value.char_val, loc);
    }
    if (match(TOK_STRING_LIT)) {
        return expr_string(previous()->value.str_val, loc);
    }
    if (check(TOK_IDENT) &&
        strcmp(peek()->value.str_val, "__builtin_offsetof") == 0) {
        return parse_builtin_offsetof(loc);
    }
    if (parser_builtin_name("__builtin_va_start") ||
        parser_builtin_name("__builtin_va_end") ||
        parser_builtin_name("__builtin_va_copy") ||
        parser_builtin_name("__builtin_va_arg")) {
        return parse_builtin_vararg(loc);
    }
    /* A small, structurally validated set of C++ function templates can be
     * expanded directly to the common expression AST.  The hook restores the
     * token cursor when the current spelling is not one of those templates. */
    if (parser_cxx_mode && rcc_parse_cxx_template_call &&
        (check(TOK_IDENT) || check(TOK_SCOPE))) {
        Expr* template_call = rcc_parse_cxx_template_call();
        if (template_call) return template_call;
    }
    /* C++ aggregate direct-list initialization has the same storage and
     * initializer semantics as the compound-literal node already used by
     * the C backend.  Restrict this lowering to registered, complete C ABI
     * types; class construction remains with the C++ frontend. */
    if (parser_cxx_mode && rcc_parse_cxx_direct_list_type &&
        (check(TOK_IDENT) || check(TOK_SCOPE))) {
        Type* direct_type = rcc_parse_cxx_direct_list_type();
        if (direct_type) {
            Expr* initializer = parse_initializer();
            initializer->compound_type = direct_type;
            rcc_parser_validate_cxx_constructor_initializer(direct_type,
                                                            initializer);
            return initializer;
        }
    }
    if (parser_cxx_mode && check(TOK_IDENT) && parser.cur->next &&
        parser.cur->next->type == TOK_LBRACE) {
        Type* direct_type = parser_lookup_type(peek()->value.str_val);
        if (direct_type && type_is_complete(direct_type) &&
            direct_type->kind != TYPE_FUNC &&
            direct_type->kind != TYPE_VOID) {
            advance();
            Expr* initializer = parse_initializer();
            initializer->compound_type = direct_type;
            rcc_parser_validate_cxx_constructor_initializer(direct_type,
                                                            initializer);
            return initializer;
        }
    }
    if (parser_cxx_mode &&
        (check(TOK_SCOPE) ||
         (check(TOK_IDENT) && parser.cur->next &&
          parser.cur->next->type == TOK_SCOPE))) {
        const char* qualified_name = parse_expression_qualified_name(loc);
        int64_t enum_value;
        Type* enum_type = NULL;
        if (parser_lookup_enum_constant(qualified_name, &enum_value,
                                        &enum_type)) {
            Expr* value = expr_int(enum_value, loc);
            value->type = enum_type ? enum_type : type_int;
            return value;
        }
        return expr_ident(qualified_name, loc);
    }
    if (match(TOK_IDENT)) {
        int64_t enum_value;
        if (parser_lookup_enum_constant(previous()->value.str_val,
                                        &enum_value, NULL)) {
            return expr_int(enum_value, loc);
        }
        if (parser_cxx_mode && rcc_parser_cxx_capture_expression) {
            Expr* capture = rcc_parser_cxx_capture_expression(
                previous()->value.str_val, loc);
            if (capture) return capture;
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
static Expr* parse_postfix_tail(Expr* e) {
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

static Expr* parse_postfix(void) {
    return parse_postfix_tail(parse_primary());
}

/* Unary: ++a, --a, &a, *a, +a, -a, ~a, !a */
static Expr* parse_unary(void) {
    SourceLoc loc = peek()->loc;

    if (parser_cxx_mode && rcc_parse_cxx_type_name &&
        (check(TOK_DYNAMIC_CAST) || check(TOK_CONST_CAST))) {
        TokenType cast_token = advance()->type;
        Type* cast_type;
        Expr* operand;
        expect(TOK_LT, "<");
        cast_type = rcc_parse_cxx_type_name();
        if (!cast_type) {
            rcc_error(peek()->loc, "C++ named cast requires a type name");
            cast_type = type_int;
        }
        expect(TOK_GT, ">");
        expect(TOK_LPAREN, "(");
        operand = parse_expression();
        expect(TOK_RPAREN, ")");
        rcc_error(loc, cast_token == TOK_DYNAMIC_CAST
                      ? "dynamic_cast requires the unavailable RinOS RTTI ABI"
                      : "const_cast is not supported by the RinOS cv-qualified object ABI");
        return parse_postfix_tail(expr_cast(cast_type, operand, loc));
    }

    /* The SDK's fixed-width wrappers only need value-preserving static and
     * reinterpret casts.  Lower both named forms to the existing typed cast
     * node so the 32/64-bit semantic and code-generation paths stay shared. */
    if (parser_cxx_mode && rcc_parse_cxx_type_name &&
        (check(TOK_STATIC_CAST) || check(TOK_REINTERPRET_CAST))) {
        advance();
        expect(TOK_LT, "<");
        Type* cast_type;
        if (parser_cxx_mode) {
            cast_type = rcc_parse_cxx_type_name();
            if (!cast_type) {
                rcc_error(peek()->loc, "C++ named cast requires a type name");
                cast_type = type_int;
            }
        } else if (!is_type_start()) {
            rcc_error(peek()->loc, "C++ named cast requires a type name");
            cast_type = type_int;
        } else {
            cast_type = parse_type_spec();
            cast_type = parse_declarator(cast_type, NULL, NULL);
        }
        expect(TOK_GT, ">");
        expect(TOK_LPAREN, "(");
        Expr* operand = parse_expression();
        expect(TOK_RPAREN, ")");
        return parse_postfix_tail(expr_cast(cast_type, operand, loc));
    }

    if (check(TOK_LPAREN) && parser.cur->next) {
        Token* saved_cur = parser.cur;
        Token* saved_prev = parser.prev;
        advance();
        if (is_type_start()) {
            Type* cast_type = parse_type_spec();
            cast_type = parse_declarator(cast_type, NULL, NULL);
            expect(TOK_RPAREN, ")");
            if (check(TOK_LBRACE)) {
                Expr* literal = parse_initializer();
                literal->compound_type = cast_type;
                return parse_postfix_tail(literal);
            }
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
        /* In a non-type template default, the closing `>` terminates the
         * default expression rather than acting as a relational operator. */
        if (parser_cxx_template_default_mode && check(TOK_GT)) break;
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

/* Export the assignment-expression grammar required by C++ default
 * arguments.  Unlike parse_expression(), it deliberately leaves a top-level
 * comma for the enclosing parameter list. */
Expr* parse_assignment_expression(void) {
    return parse_assignment();
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
    if (parser_cxx_mode && rcc_parse_cxx_type_start &&
        rcc_parse_cxx_type_start()) return true;
    switch (peek()->type) {
        case TOK_DECLTYPE:
            return parser_cxx_mode;
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
        case TOK__COMPLEX:
        case TOK__IMAGINARY:
        case TOK__ATOMIC:
        case TOK__NORETURN:
        case TOK___BUILTIN_VA_LIST:
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
    bool value_init = false;
    if (!match(TOK_LBRACE)) return parse_assignment();
    loc = previous()->loc;
    if (check(TOK_RBRACE)) {
        if (parser_cxx_mode) {
            /* C++ value-initialization zeroes scalars and aggregates.  The
             * existing {0} semantic/codegen path already implements that
             * contract for both target ABIs. */
            exprlist_append_designated(
                &items, expr_int(0, loc), INIT_DESIGNATOR_NONE, 0, NULL);
            value_init = true;
        } else {
            rcc_error(loc, "empty initializer list is not valid C17");
        }
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
    Expr* initializer = expr_initializer_list(items, loc);
    initializer->compound_value_init = value_init;
    return initializer;
}

Expr* rcc_parser_parse_initializer(void) {
    return parse_initializer();
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
        if (!parser_lookup_enum_constant(previous()->value.str_val,
                                         &value, NULL)) {
            rcc_error(previous()->loc, "unknown enum constant '%s'",
                      previous()->value.str_val);
        }
    } else {
        rcc_error(peek()->loc, "expected integer enum value");
    }
    return negative ? -value : value;
}

static void parse_enum_body(Type* enum_type, bool scoped,
                            const char* enum_tag) {
    int64_t next_value = 0;
    while (!check(TOK_RBRACE) && !at_end()) {
        Token* name = expect(TOK_IDENT, "enumerator name");
        int64_t value = next_value;
        if (match(TOK_ASSIGN)) value = parse_enum_value(next_value);
        if (name) {
            char qualified[512];
            const char* spelling = name->value.str_val;
            if (scoped && enum_tag) {
                int written = snprintf(qualified, sizeof(qualified),
                                       "%s::%s", enum_tag, spelling);
                if (written < 0 || (size_t)written >= sizeof(qualified)) {
                    rcc_error(name->loc,
                              "scoped enum constant name exceeds compiler limit");
                } else {
                    spelling = rcc_intern(qualified);
                }
            }
            parser_define_enum_constant(spelling, value, enum_type);
        }
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
    if (parser_pack_alignment > 0 && alignment > parser_pack_alignment) {
        alignment = parser_pack_alignment;
    }
    field->name = name;
    field->type = type;
    field->is_bitfield = false;
    field->bit_width = 0u;
    field->bit_offset = 0u;
    field->cxx_access = 0u;
    field->next = NULL;
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

typedef struct ParserBitfieldLayout {
    bool active;
    int offset;
    int size;
    int alignment;
    unsigned used;
} ParserBitfieldLayout;

static void parser_reset_bitfield_layout(ParserBitfieldLayout* layout) {
    layout->active = false;
    layout->offset = 0;
    layout->size = 0;
    layout->alignment = 0;
    layout->used = 0u;
}

static void parser_append_bitfield(Type* aggregate,
                                   ParserBitfieldLayout* layout,
                                   const char* name,
                                   Type* type,
                                   int64_t width_value,
                                   SourceLoc loc) {
    TypeField* field;
    TypeField** tail;
    int alignment;
    int size;
    unsigned storage_bits;
    unsigned width;

    if (!type || (!type_is_integer(type) && type->kind != TYPE_ENUM)) {
        rcc_error(loc, "bit-field type must be an integer or enum type");
        parser_reset_bitfield_layout(layout);
        return;
    }
    size = type->size;
    if (size <= 0 || size > 4) {
        rcc_error(loc,
                  "bit-field type width of %d bytes is not supported",
                  size);
        parser_reset_bitfield_layout(layout);
        return;
    }
    storage_bits = (unsigned)size * 8u;
    if (width_value < 0 || (uint64_t)width_value > storage_bits) {
        rcc_error(loc,
                  "bit-field width %lld exceeds its %u-bit storage unit",
                  (long long)width_value, storage_bits);
        parser_reset_bitfield_layout(layout);
        return;
    }
    width = (unsigned)width_value;
    if (width == 0u) {
        if (name) {
            rcc_error(loc, "named bit-field cannot have zero width");
        }
        parser_reset_bitfield_layout(layout);
        if (aggregate->kind == TYPE_STRUCT) {
            alignment = type->align > 0 ? type->align : 1;
            if (parser_pack_alignment > 0 &&
                alignment > parser_pack_alignment) {
                alignment = parser_pack_alignment;
            }
            aggregate->size = parser_align_up(aggregate->size, alignment);
            if (alignment > aggregate->align) aggregate->align = alignment;
        }
        return;
    }

    alignment = type->align > 0 ? type->align : 1;
    if (parser_pack_alignment > 0 && alignment > parser_pack_alignment) {
        alignment = parser_pack_alignment;
    }
    if (aggregate->kind == TYPE_UNION) {
        parser_reset_bitfield_layout(layout);
        layout->offset = 0;
        layout->size = size;
        layout->alignment = alignment;
        layout->used = width;
        if (size > aggregate->size) aggregate->size = size;
    } else if (!layout->active || layout->size != size ||
               layout->alignment != alignment || layout->used + width > storage_bits) {
        layout->active = true;
        layout->offset = parser_align_up(aggregate->size, alignment);
        layout->size = size;
        layout->alignment = alignment;
        layout->used = 0u;
        aggregate->size = layout->offset + size;
    }
    if (alignment > aggregate->align) aggregate->align = alignment;

    /* Unnamed non-zero fields still consume storage, but cannot be selected
     * by a member expression or by an initializer designator. */
    if (!name) {
        layout->used += width;
        return;
    }
    field = ast_arena_alloc(sizeof(*field));
    field->name = name;
    field->type = type;
    field->offset = layout->offset;
    field->is_bitfield = true;
    field->bit_width = width;
    field->bit_offset = layout->used;
    field->cxx_access = 0u;
    field->next = NULL;
    tail = &aggregate->fields;
    while (*tail) tail = &(*tail)->next;
    *tail = field;
    layout->used += width;
}

/* C17 does not permit a variably modified type as a struct or union member.
 * This includes a pointer whose pointed-to type is a VLA; the member itself
 * remains a fixed-size pointer, but its declared type is still variably
 * modified and may not be stored in an aggregate definition. */
static bool parser_type_is_variably_modified(Type* type) {
    if (!type) return false;
    if (type->kind == TYPE_ARRAY) {
        return type->array_bound != NULL || type->array_unspecified_bound ||
               parser_type_is_variably_modified(type->base);
    }
    if (type->kind == TYPE_PTR) {
        return parser_type_is_variably_modified(type->base);
    }
    return false;
}

static bool parser_is_flexible_array(Type* type) {
    return type && type->kind == TYPE_ARRAY && type->array_len == -1 &&
           !type->array_bound && !type->array_unspecified_bound;
}

static void parser_validate_flexible_array_members(Type* aggregate) {
    TypeField* field;
    TypeField* last_named = NULL;
    int named_count = 0;
    if (!aggregate || (aggregate->kind != TYPE_STRUCT &&
                       aggregate->kind != TYPE_UNION)) return;
    for (field = aggregate->fields; field; field = field->next) {
        if (field->name) {
            last_named = field;
            ++named_count;
        }
    }
    for (field = aggregate->fields; field; field = field->next) {
        Type* element;
        if (!parser_is_flexible_array(field->type)) continue;
        if (aggregate->kind == TYPE_UNION) {
            rcc_error(peek()->loc,
                      "flexible array member is not allowed in a union");
        } else if (named_count == 1) {
            rcc_error(peek()->loc,
                      "flexible array member requires another named member");
        }
        if (aggregate->kind == TYPE_STRUCT && field != last_named) {
            rcc_error(peek()->loc,
                      "flexible array member must be the last member of a struct");
        }
        element = field->type->base;
        if (!element || element->size <= 0 || !type_is_complete(element)) {
            rcc_error(peek()->loc,
                      "flexible array member has an incomplete element type");
        }
    }
}

static bool parser_type_has_array_parameter_spec(Type* type) {
    if (!type) return false;
    if (type->kind == TYPE_ARRAY) {
        return type->array_parameter_static ||
               type->array_unspecified_bound ||
               type->array_parameter_const ||
               type->array_parameter_volatile ||
               type->array_parameter_restrict ||
               parser_type_has_array_parameter_spec(type->base);
    }
    if (type->kind == TYPE_PTR) {
        return parser_type_has_array_parameter_spec(type->base);
    }
    return false;
}

static void parser_append_anonymous_fields(Type* aggregate, Type* anonymous) {
    TypeField** tail = &aggregate->fields;
    int alignment = anonymous && anonymous->align > 0 ? anonymous->align : 1;
    int size = anonymous && anonymous->size > 0 ? anonymous->size : 0;
    if (parser_pack_alignment > 0 && alignment > parser_pack_alignment) {
        alignment = parser_pack_alignment;
    }
    int base_offset = aggregate->kind == TYPE_UNION
        ? 0 : parser_align_up(aggregate->size, alignment);
    while (*tail) tail = &(*tail)->next;
    for (TypeField* source = anonymous ? anonymous->fields : NULL;
         source; source = source->next) {
        TypeField* field = ast_arena_alloc(sizeof(*field));
        TypeField* existing;
        for (existing = aggregate->fields; existing;
             existing = existing->next) {
            if (existing->name && source->name &&
                strcmp(existing->name, source->name) == 0) {
                rcc_error(peek()->loc,
                          "duplicate member '%s' from anonymous aggregate",
                          source->name);
                break;
            }
        }
        field->name = source->name;
        field->type = source->type;
        field->offset = base_offset + source->offset;
        field->is_bitfield = source->is_bitfield;
        field->bit_width = source->bit_width;
        field->bit_offset = source->bit_offset;
        field->cxx_access = source->cxx_access;
        field->next = NULL;
        *tail = field;
        tail = &field->next;
    }
    if (aggregate->kind == TYPE_UNION) {
        if (size > aggregate->size) aggregate->size = size;
    } else {
        aggregate->size = base_offset + size;
    }
    if (alignment > aggregate->align) aggregate->align = alignment;
}

static void parse_aggregate_body(Type* aggregate) {
    ParserBitfieldLayout bitfield_layout;
    aggregate->size = 0;
    aggregate->align = 1;
    aggregate->fields = NULL;
    parser_reset_bitfield_layout(&bitfield_layout);
    while (!check(TOK_RBRACE) && !at_end()) {
        Type* field_base;
        if (match(TOK_PRAGMA_PACK)) {
            parser_apply_pack(previous());
            continue;
        }
        skip_attributes();
        field_base = parse_type_spec();
        if (!field_base) {
            rcc_error(peek()->loc, "expected field type specifier");
            if (check(TOK_IDENT)) advance();
            field_base = type_int;
        }
        do {
            const char* field_name = NULL;
            Type* field_type = parse_declarator(field_base, &field_name, NULL);
            if (parser_type_has_array_parameter_spec(field_type)) {
                rcc_error(previous()->loc,
                          "array parameter qualifiers are only valid in function parameter declarations");
            }
            if (parser_type_is_variably_modified(field_type)) {
                rcc_error(previous()->loc,
                          "variably modified type is not allowed for struct/union member");
            }
            if (match(TOK_COLON)) {
                Expr* width_expression = parse_assignment();
                int64_t width_value = 0;
                if (!eval_integer_constant(width_expression, &width_value)) {
                    rcc_error(width_expression ? width_expression->loc : previous()->loc,
                              "bit-field width must be an integer constant");
                }
                parser_append_bitfield(aggregate, &bitfield_layout,
                                       field_name, field_type, width_value,
                                       width_expression ? width_expression->loc
                                                        : previous()->loc);
                continue;
            }
            if (!field_name) {
                if ((field_type->kind == TYPE_STRUCT ||
                     field_type->kind == TYPE_UNION) &&
                    field_type->is_complete && check(TOK_SEMICOLON)) {
                    parser_append_anonymous_fields(aggregate, field_type);
                } else {
                    rcc_error(peek()->loc, "expected field name");
                }
                break;
            }
            parser_reset_bitfield_layout(&bitfield_layout);
            parser_append_field(aggregate, field_name, field_type);
        } while (match(TOK_COMMA));
        expect(TOK_SEMICOLON, ";");
    }
    expect(TOK_RBRACE, "}");
    parser_validate_flexible_array_members(aggregate);
    aggregate->size = parser_align_up(aggregate->size, aggregate->align);
    aggregate->is_complete = true;
}

static Type* parser_qualify_type(Type* type, bool is_const,
                                 bool is_volatile) {
    Type* qualified;
    if (!type || (!is_const && !is_volatile)) return type;
    qualified = ast_arena_alloc(sizeof(*qualified));
    *qualified = *type;
    qualified->is_const = qualified->is_const || is_const;
    qualified->is_volatile = qualified->is_volatile || is_volatile;
    return qualified;
}

static Type* parse_type_spec(void) {
    Type* t = NULL;
    bool is_unsigned = false;
    bool saw_sign = false;
    bool is_const = false;
    bool is_volatile = false;
    bool saw_complex = false;
    bool saw_imaginary = false;
    int long_count = 0;
    bool is_short = false;

    while (1) {
        if (match(TOK_CONST)) {
            is_const = true;
        } else if (match(TOK_VOLATILE)) {
            is_volatile = true;
        } else if (match(TOK_UNSIGNED)) {
            is_unsigned = true;
            saw_sign = true;
        } else if (match(TOK_SIGNED)) {
            is_unsigned = false;
            saw_sign = true;
        } else if (match(TOK_LONG)) {
            long_count++;
        } else if (match(TOK_SHORT)) {
            is_short = true;
        } else if (match(TOK__COMPLEX)) {
            rcc_error(previous()->loc,
                      "_Complex is not supported by the RinOS floating-point ABI");
            saw_complex = true;
        } else if (match(TOK__IMAGINARY)) {
            rcc_error(previous()->loc,
                      "_Imaginary is not supported by the RinOS floating-point ABI");
            saw_imaginary = true;
        } else if (match(TOK__ATOMIC)) {
            rcc_error(previous()->loc,
                      "language _Atomic is not supported; use RinOS atomic builtins");
        } else if (match(TOK__NORETURN)) {
            rcc_error(previous()->loc,
                      "_Noreturn is not supported by the RinOS function ABI");
        } else {
            break;
        }
    }

    if (saw_complex || saw_imaginary) {
        /* Consume the optional scalar component after the diagnostic so
         * parser recovery does not reinterpret it as a declarator. */
        if (match(TOK_FLOAT)) {
            t = type_float;
        } else if (match(TOK_DOUBLE)) {
            t = type_double;
        } else if (match(TOK_INT)) {
            t = type_int;
        } else {
            t = type_double;
        }
    } else if (long_count != 0 && match(TOK_DOUBLE)) {
        rcc_error(previous()->loc,
                  "long double is not supported by the RinOS floating-point ABI");
        if (is_unsigned || saw_sign || is_short) {
            rcc_error(previous()->loc,
                      "invalid integer qualifier on double type");
        }
        t = type_double;
    } else if (match(TOK_VOID)) {
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
        if (is_unsigned || saw_sign || long_count != 0 || is_short) {
            rcc_error(previous()->loc,
                      "invalid integer qualifier on float type");
        }
        t = type_float;
    } else if (match(TOK_DOUBLE)) {
        if (long_count != 0) {
            rcc_error(previous()->loc,
                      "long double is not supported by the RinOS floating-point ABI");
        }
        if (is_unsigned || saw_sign || is_short) {
            rcc_error(previous()->loc,
                      "invalid integer qualifier on double type");
        }
        t = type_double;
    } else if (match(TOK__BOOL)) {
        t = type_bool;
    } else if (match(TOK___BUILTIN_VA_LIST)) {
        t = parser_builtin_va_list_type;
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
        bool scoped = false;
        Token* tag = NULL;
        if (parser_cxx_mode && (match(TOK_CLASS) || match(TOK_STRUCT))) {
            scoped = true;
        }
        if (check(TOK_IDENT)) {
            tag = advance();
        }
        t = parser_tag_type(TYPE_ENUM, tag ? tag->value.str_val : NULL);
        t->enum_is_scoped = scoped;
        if (parser_cxx_mode && match(TOK_COLON)) {
            Type* underlying = parse_type_spec();
            if (!underlying || !type_is_integer(underlying)) {
                rcc_error(previous()->loc,
                          "C++ enum underlying type must be an integer type");
            } else {
                t->size = underlying->size;
                t->align = underlying->align;
                t->is_unsigned = underlying->is_unsigned;
            }
        }
        if (match(TOK_LBRACE)) {
            parse_enum_body(t, scoped, tag ? tag->value.str_val : NULL);
        }
        if (parser_cxx_mode && tag) {
            parser_define_type(tag->value.str_val, t);
        }
    } else if (parser_cxx_mode && rcc_parse_cxx_type_name &&
               (check(TOK_DECLTYPE) ||
                (rcc_parse_cxx_type_start && rcc_parse_cxx_type_start()))) {
        t = rcc_parse_cxx_type_name();
    } else if (check(TOK_IDENT)) {
        const char* name = peek()->value.str_val;
        t = parser_lookup_type(name);
        if (t) {
            advance();
        } else if (parser_cxx_mode) {
            rcc_error(peek()->loc, "unknown C++ type name '%s'", name);
            advance();
            t = type_int;
        }
    } else {
        /* Default to int */
        t = is_unsigned ? type_uint : type_int;
    }

    /* Also diagnose the standard spellings where a floating qualifier follows
     * the component type, such as `double _Complex`. */
    while (match(TOK__COMPLEX) || match(TOK__IMAGINARY)) {
        rcc_error(previous()->loc,
                  previous()->type == TOK__COMPLEX
                      ? "_Complex is not supported by the RinOS floating-point ABI"
                      : "_Imaginary is not supported by the RinOS floating-point ABI");
    }

    /* C17 permits signed and unsigned without an explicit int.  When the
     * next identifier is the declarator rather than a typedef name, the
     * typedef lookup branch above intentionally leaves t unset. */
    if (!t && saw_sign) {
        t = is_unsigned ? type_uint : type_int;
    }

    /* Declaration specifiers permit qualifiers on either side of the type
     * specifier (for example, both const int and int const). */
    while (match(TOK_CONST) || match(TOK_VOLATILE)) {
        if (previous()->type == TOK_CONST) is_const = true;
        else is_volatile = true;
    }
    t = parser_qualify_type(t, is_const, is_volatile);

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
        param->is_bitfield = false;
        param->bit_width = 0u;
        param->cxx_access = 0u;
        param->next = NULL;
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
        Type* parameter_array_type = NULL;
        const char* parameter_name = NULL;
        Expr* parameter_default = NULL;
        Decl* parameter;
        if (match(TOK_ELLIPSIS)) {
            if (parameter_index == 0) {
                rcc_error(previous()->loc,
                          "ellipsis requires at least one named parameter");
            }
            *variadic = true;
            break;
        }
        parameter_base = parse_type_spec();
        if (!parameter_base) {
            rcc_error(peek()->loc, "expected parameter type specifier");
            if (check(TOK_IDENT)) advance();
            parameter_base = type_int;
        }
        parameter_type = parse_declarator(parameter_base, &parameter_name, NULL);
        if (parameter_type->kind == TYPE_ARRAY) {
            parameter_array_type = parameter_type;
            {
                Type* adjusted = type_ptr(parameter_type->base);
                adjusted->is_const = parameter_type->is_const;
                adjusted->is_volatile = parameter_type->is_volatile;
                adjusted->is_const = adjusted->is_const ||
                    parameter_type->array_parameter_const;
                adjusted->is_volatile = adjusted->is_volatile ||
                    parameter_type->array_parameter_volatile;
                adjusted->is_restrict =
                    parameter_type->array_parameter_restrict;
                parameter_type = adjusted;
            }
        } else if (parameter_type->kind == TYPE_FUNC) {
            parameter_type = type_ptr(parameter_type);
        } else if (parser_type_is_variably_modified(parameter_type)) {
            /* A pointer-to-VLA parameter keeps its pointer ABI type, but the
             * original variably modified type is needed for bound analysis
             * and for multidimensional stride lowering. */
            parameter_array_type = parameter_type;
        }
        if (parser_cxx_mode && match(TOK_ASSIGN)) {
            parameter_default = parse_assignment();
        }
        parameter = decl_param(parameter_name, parameter_type,
                               parameter_index++, peek()->loc);
        parameter->param_array_type = parameter_array_type;
        parameter->param_default = parameter_default;
        decllist_append(&parameters, parameter);
        if (!match(TOK_COMMA)) break;
        if (check(TOK_RPAREN)) {
            rcc_error(peek()->loc,
                      "expected parameter declaration after ','");
            break;
        }
    }
    return parameters;
}

typedef struct ParsedPointerLevel {
    bool is_const;
    bool is_volatile;
    bool is_restrict;
    bool is_reference;
    bool is_rvalue_reference;
    struct ParsedPointerLevel* next;
} ParsedPointerLevel;

/* A parenthesized pointer declarator is also the spelling used for a
 * pointer-to-function.  Decide which grammar follows the closing parenthesis
 * before taking the function-pointer shortcut; otherwise a valid pointer to
 * a VLA such as `int (*rows)[count]` is mistaken for a function declarator. */
static bool parser_parenthesized_pointer_is_function(void) {
    Token* token = parser.cur;
    int depth = 0;
    if (!token || token->type != TOK_LPAREN) return false;
    for (; token; token = token->next) {
        if (token->type == TOK_LPAREN) {
            ++depth;
        } else if (token->type == TOK_RPAREN) {
            --depth;
            if (depth == 0) {
                return token->next && token->next->type == TOK_LPAREN;
            }
        }
    }
    return false;
}

static ParsedPointerLevel* parse_pointer_levels(void) {
    ParsedPointerLevel* levels = NULL;
    ParsedPointerLevel** tail = &levels;
    while (match(TOK_STAR) || (parser_cxx_mode && match(TOK_AMP))) {
        ParsedPointerLevel* level = ast_arena_alloc(sizeof(*level));
        level->is_reference = previous()->type == TOK_AMP;
        level->is_rvalue_reference = false;
        while (check(TOK_CONST) || check(TOK_VOLATILE) ||
               check(TOK_RESTRICT)) {
            if (match(TOK_CONST)) level->is_const = true;
            else if (match(TOK_VOLATILE)) level->is_volatile = true;
            else if (match(TOK_RESTRICT)) level->is_restrict = true;
        }
        *tail = level;
        tail = &level->next;
    }
    return levels;
}

static Type* apply_pointer_levels(Type* type,
                                  ParsedPointerLevel* levels) {
    for (ParsedPointerLevel* level = levels; level; level = level->next) {
        type = type_ptr(type);
        type->is_const = level->is_const;
        type->is_volatile = level->is_volatile;
        type->is_restrict = level->is_restrict;
        type->is_reference = level->is_reference;
        type->is_rvalue_reference = level->is_rvalue_reference;
    }
    return type;
}

static Type* parse_declarator(Type* base_type, const char** name,
                              DeclList** parameters) {
    Type* type = base_type;
    ParsedPointerLevel* leading_pointers;
    int array_lengths[32];
    Expr* array_bounds[32];
    SourceLoc array_locs[32];
    bool array_unspecified[32];
    bool array_statics[32];
    bool array_consts[32];
    bool array_volatiles[32];
    bool array_restricts[32];
    int array_count = 0;
    ParsedPointerLevel* parenthesized_pointers = NULL;
    bool parenthesized_pointer = false;
    if (name) *name = NULL;
    if (parameters) *parameters = NULL;

    leading_pointers = parse_pointer_levels();

    /* Function-pointer declarator: return_type (*name)(parameters). */
    if (check(TOK_LPAREN) && parser.cur->next &&
        (parser.cur->next->type == TOK_STAR ||
         (parser_cxx_mode && parser.cur->next->type == TOK_AMP)) &&
        parser_parenthesized_pointer_is_function()) {
        ParsedPointerLevel* nested_pointers;
        DeclList* function_parameters = NULL;
        bool variadic = false;
        bool has_prototype;
        advance();
        nested_pointers = parse_pointer_levels();
        if (check(TOK_IDENT)) {
            Token* identifier = advance();
            if (name) *name = identifier->value.str_val;
        }
        expect(TOK_RPAREN, ")");
        expect(TOK_LPAREN, "(");
        has_prototype = !check(TOK_RPAREN);
        function_parameters = parse_parameter_list(&variadic);
        expect(TOK_RPAREN, ")");
        type = type_func(apply_pointer_levels(base_type, leading_pointers),
                         parser_type_params(function_parameters, &variadic),
                         variadic);
        type->has_prototype = has_prototype;
        type = apply_pointer_levels(type, nested_pointers);
        if (parameters) *parameters = function_parameters;
        return type;
    }

    /* Handle the other parenthesized pointer declarators before parsing
     * suffixes.  Array suffixes bind to the declarator inside the group, and
     * the pointer levels inside the group are applied afterwards. */
    if (check(TOK_LPAREN) && parser.cur->next &&
        (parser.cur->next->type == TOK_STAR ||
         (parser_cxx_mode && parser.cur->next->type == TOK_AMP))) {
        advance();
        parenthesized_pointers = parse_pointer_levels();
        if (check(TOK_IDENT)) {
            Token* identifier = advance();
            if (name) *name = identifier->value.str_val;
        }
        expect(TOK_RPAREN, ")");
        parenthesized_pointer = true;
    }

    type = apply_pointer_levels(type, leading_pointers);
    if (!parenthesized_pointer && check(TOK_IDENT)) {
        Token* identifier = advance();
        if (name) *name = identifier->value.str_val;
    }

    for (;;) {
        if (match(TOK_LBRACKET)) {
            int length = -1;
            Expr* bound_expression = NULL;
            bool unspecified_bound = false;
            bool parameter_static = match(TOK_STATIC);
            bool parameter_const = false;
            bool parameter_volatile = false;
            bool parameter_restrict = false;
            while (check(TOK_CONST) || check(TOK_VOLATILE) ||
                   check(TOK_RESTRICT)) {
                if (match(TOK_CONST)) parameter_const = true;
                else if (match(TOK_VOLATILE)) parameter_volatile = true;
                else if (match(TOK_RESTRICT)) parameter_restrict = true;
            }
            if (match(TOK_STAR)) {
                length = -2;
                unspecified_bound = true;
            } else if (!check(TOK_RBRACKET)) {
                Expr* bound = parse_assignment();
                int64_t constant = 0;
                if (eval_integer_constant(bound, &constant)) {
                    if (constant <= 0 || constant > INT_MAX) {
                        rcc_error(bound->loc,
                                  "array bound is not a positive representable integer constant");
                    } else {
                        length = (int)constant;
                    }
                } else {
                    /* A non-constant bound is a C17 variable-length array
                     * dimension. Its semantic type and runtime extent are
                     * checked after parameter/local scopes are available. */
                    length = -2;
                    bound_expression = bound;
                }
            }
            expect(TOK_RBRACKET, "]");
            if (array_count >= (int)(sizeof(array_lengths) /
                                     sizeof(array_lengths[0]))) {
                rcc_error(previous()->loc, "array declarator is too deep");
            } else {
                array_lengths[array_count] = length;
                array_bounds[array_count] = bound_expression;
                array_locs[array_count] = previous()->loc;
                array_unspecified[array_count] = unspecified_bound;
                array_statics[array_count] = parameter_static;
                array_consts[array_count] = parameter_const;
                array_volatiles[array_count] = parameter_volatile;
                array_restricts[array_count] = parameter_restrict;
                ++array_count;
            }
        } else if (match(TOK_LPAREN)) {
            bool variadic = false;
            bool has_prototype = !check(TOK_RPAREN);
            DeclList* function_parameters = parse_parameter_list(&variadic);
            expect(TOK_RPAREN, ")");
            type = type_func(type,
                             parser_type_params(function_parameters, &variadic),
                             variadic);
            type->has_prototype = has_prototype;
            if (parameters) *parameters = function_parameters;
        } else {
            break;
        }
    }
    /* Array declarator suffixes bind from the identifier outward.  Applying
     * them in source order reverses `int a[2][3]` into [3][2]; retain the
     * parsed suffixes and construct the type from the inside out instead. */
    while (array_count > 0) {
        int length = array_lengths[--array_count];
        if (length > 0 && type->size > 0 &&
            length > INT_MAX / type->size) {
            rcc_error(array_locs[array_count],
                      "array bound is too large for the complete element type");
            length = -1;
        }
        type = type_array(type, length);
        type->array_bound = array_bounds[array_count];
        type->array_unspecified_bound = array_unspecified[array_count];
        type->array_parameter_static = array_statics[array_count];
        type->array_parameter_const = array_consts[array_count];
        type->array_parameter_volatile = array_volatiles[array_count];
        type->array_parameter_restrict = array_restricts[array_count];
    }
    if (parenthesized_pointers) {
        type = apply_pointer_levels(type, parenthesized_pointers);
    }
    return type;
}

Type* rcc_parser_parse_cxx_declarator(Type* base_type, const char** name,
                                      DeclList** parameters) {
    return parse_declarator(base_type, name, parameters);
}

/* ═══════════════════════════════════════
 * Statement Parsing
 * ═══════════════════════════════════════ */

static Stmt* parse_block(void) {
    SourceLoc loc = previous()->loc;
    StmtList* stmts = NULL;

    while (!check(TOK_RBRACE) && !at_end()) {
        Token* iteration_start = parser.cur;
        int errors_before = g_error_count;
        Stmt* s = parse_declaration();
        if (s) {
            stmtlist_append(&stmts, s);
        }
        if (g_error_count > errors_before && parser.cur == iteration_start) {
            synchronize();
        }
        if (parser.cur == iteration_start && !at_end() &&
            !check(TOK_RBRACE)) {
            advance();
        }
    }

    expect(TOK_RBRACE, "}");
    return stmt_block(stmts, loc);
}

static Stmt* parse_if_stmt(void) {
    SourceLoc loc = previous()->loc;
    bool is_constexpr = parser_cxx_mode && match(TOK_CONSTEXPR);
    expect(TOK_LPAREN, "(");
    Expr* cond = parse_expression();
    expect(TOK_RPAREN, ")");

    Stmt* then_stmt = parse_statement();
    Stmt* else_stmt = NULL;

    if (match(TOK_ELSE)) {
        else_stmt = parse_statement();
    }

    Stmt* statement = stmt_if(cond, then_stmt, else_stmt, loc);
    statement->if_is_constexpr = is_constexpr;
    return statement;
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
        if (is_type_start() || (parser_cxx_mode && check(TOK_AUTO))) {
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
        val = parser_cxx_mode && check(TOK_LBRACE)
            ? parse_initializer() : parse_expression();
    }
    expect(TOK_SEMICOLON, ";");

    return stmt_return(val, loc);
}

static Stmt* parse_statement(void) {
    SourceLoc loc = peek()->loc;

    /* C++ exception statements are parsed by the C++ frontend even inside
     * free-function bodies, which otherwise use this shared C statement
     * parser.  The frontend retains the construct for diagnostics instead
     * of allowing the C expression parser to reinterpret `try`/`throw`. */
    if (parser_cxx_mode && rcc_parse_cxx_statement &&
        (check(TOK_TRY) || check(TOK_THROW))) {
        return rcc_parse_cxx_statement();
    }

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
    if (parser_cxx_mode && rcc_parse_cxx_range_for_statement &&
        check(TOK_FOR)) {
        Stmt* range_for = rcc_parse_cxx_range_for_statement();
        if (range_for) return range_for;
    }
    if (match(TOK_FOR)) {
        return parse_for_stmt();
    }
    if (match(TOK_SWITCH)) {
        return parse_switch_stmt();
    }
    if (match(TOK_CASE)) {
        /* A case label requires a conditional-expression, not the wider
         * comma/assignment expression grammar. */
        Expr* val = parse_conditional();
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
    bool is_constexpr = false;
    bool is_consteval = false;
    bool is_thread_local = false;
    const char* declaration_name = NULL;
    DeclList* parameters = NULL;
    Type* base_type;
    Type* type;
    Decl* declaration;

    if (parser_cxx_mode && rcc_parse_cxx_auto_local_declaration &&
        check(TOK_AUTO)) {
        Stmt* auto_declaration = rcc_parse_cxx_auto_local_declaration();
        if (auto_declaration) return auto_declaration;
    }

    if (check(TOK_STATIC_ASSERT)) {
        SourceLoc assertion_loc = advance()->loc;
        Expr* condition;
        Token* message = NULL;

        expect(TOK_LPAREN, "(");
        condition = parse_assignment();
        if (match(TOK_COMMA)) {
            message = expect(TOK_STRING_LIT, "static assertion message");
        }
        expect(TOK_RPAREN, ")");
        expect(TOK_SEMICOLON, ";");
        return stmt_decl(decl_static_assert(
            condition, message ? message->value.str_val : NULL,
            assertion_loc), assertion_loc);
    }

    skip_attributes();
    if (parser_cxx_mode && match(TOK_CONSTEXPR)) is_constexpr = true;
    if (parser_cxx_mode && match(TOK_CONSTEVAL)) {
        is_constexpr = true;
        is_consteval = true;
    }
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

    if (parser_cxx_mode && rcc_parse_cxx_operator_declaration &&
        check(TOK_OPERATOR)) {
        Stmt* operator_declaration = rcc_parse_cxx_operator_declaration(
            base_type, loc);
        if (operator_declaration) return operator_declaration;
    }

    /* C++ direct initialization uses a different declarator grammar from C:
     * `Pair value(7, 11);` is an object declaration, not a function returning
     * Pair.  Let the C++ parser consume this form only after the declaration
     * specifiers have been collected, so storage qualifiers are preserved. */
    if (!is_typedef && parser_cxx_mode &&
        rcc_parse_cxx_class_local_declaration) {
        Stmt* class_declaration = rcc_parse_cxx_class_local_declaration(
            base_type, storage, is_thread_local, loc);
        if (class_declaration) return class_declaration;
    }

    type = parse_declarator(base_type, &declaration_name, &parameters);
    if (parser_cxx_mode && type && type->kind == TYPE_FUNC &&
        match(TOK_NOEXCEPT)) {
        if (match(TOK_LPAREN)) {
            int depth = 1;
            while (!at_end() && depth > 0) {
                if (match(TOK_LPAREN)) {
                    ++depth;
                } else if (match(TOK_RPAREN)) {
                    --depth;
                } else {
                    advance();
                }
            }
            if (depth != 0) {
                rcc_error(peek()->loc,
                          "unterminated C++ noexcept specification");
            }
        }
    }
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
            if (parser_cxx_mode && rcc_parser_cxx_begin_function_parameters) {
                rcc_parser_cxx_begin_function_parameters(parameters);
            }
            body = parse_block();
            if (parser_cxx_mode && rcc_parser_cxx_end_function_parameters) {
                rcc_parser_cxx_end_function_parameters();
            }
        } else {
            expect(TOK_SEMICOLON, ";");
        }

        declaration = decl_func(declaration_name, type, parameters, body, loc);
        declaration->storage = storage;
        declaration->func_is_inline = is_inline;
        declaration->func_is_constexpr = is_constexpr;
        declaration->func_is_consteval = is_consteval;
        return stmt_decl(declaration, loc);
    }

    /* Variable initializer */
    Expr* init = NULL;
    if (match(TOK_ASSIGN)) {
        init = parse_initializer();
    } else if (parser_cxx_mode && check(TOK_LBRACE)) {
        init = parse_initializer();
    }
    /* A declaration-level C++ braced initializer carries the declared class
     * type, just like a direct-list expression parsed in an expression or
     * local-declaration context.  Preserve that type so validated direct
     * constructors, including static-storage cleanup wrappers, are selected
     * consistently at semantic analysis time. */
    if (parser_cxx_mode && init && init->kind == EXPR_COMPOUND &&
        !init->compound_type) {
        init->compound_type = type;
    }
    rcc_parser_validate_cxx_constructor_initializer(type, init);

    expect(TOK_SEMICOLON, ";");

    declaration = decl_var(declaration_name, type, init, loc);
    declaration->storage = storage;
    declaration->var_is_thread_local = is_thread_local;
    declaration->var_is_constexpr = is_constexpr;
    if (is_consteval) {
        rcc_error(loc, "consteval declaration must declare a function");
    }
    if (parser_cxx_mode && rcc_parser_cxx_add_value_binding) {
        rcc_parser_cxx_add_value_binding(declaration_name, type);
    }
    return stmt_decl(declaration, loc);
}

/* ═══════════════════════════════════════
 * Top-level Parsing
 * ═══════════════════════════════════════ */

static Decl* parse_toplevel(void) {
    if (match(TOK_PRAGMA_PACK)) {
        parser_apply_pack(previous());
        return NULL;
    }
    Stmt* s = parse_declaration();
    if (s && s->kind == STMT_DECL) {
        return s->decl;
    }
    return NULL;
}

/* Main parser function */
AST* rcc_parse(TokenList* tokens) {
    rcc_parser_set_cxx_mode(false);
    parser.cur = tokens->head;
    parser.prev = NULL;
    parser_type_names = NULL;
    parser_tag_names = NULL;
    parser_enum_constants = NULL;
    if (g_opts.target_arch == ARCH_X64) {
        Type* record = type_struct("__rcc_sysv_va_list");
        record->size = 24;
        record->align = 8;
        record->is_complete = true;
        parser_builtin_va_list_type = type_array(record, 1);
    } else {
        parser_builtin_va_list_type = type_ptr(type_char);
    }
    parser_pack_alignment = 0;
    parser_pack_depth = 0;

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
