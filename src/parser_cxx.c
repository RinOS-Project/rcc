/*
 * RCC++ - RinOS C++ Compiler
 * C++ specific parser extensions
 */

#include "rcc.h"
#include "token.h"
#include "ast.h"
#include "ast_cxx.h"
#include <limits.h>
#include <stdio.h>

/* External parser state (from parser.c) */
typedef struct {
    Token* cur;
    Token* prev;
} Parser;

extern Parser parser;
extern void rcc_parser_set_cxx_template_default_mode(bool enabled);

/* The current template is only needed while parsing dependent declarations;
 * instantiated types are resolved by the later template semantic phase. */
static CxxTemplate* active_template;
static CxxNamespace* active_namespace;
static CxxClass* active_class;
static AST* active_ast;

static const char* cxx_method_source_name(CxxMethod* method);

typedef struct CxxParserValueBinding {
    const char* name;
    Type* type;
    struct CxxParserValueBinding* next;
} CxxParserValueBinding;

typedef struct CxxReferenceCapture {
    const char* name;
    struct CxxReferenceCapture* next;
} CxxReferenceCapture;

typedef struct CxxLambdaCaptureSpec {
    const char* name;
    SourceLoc loc;
    bool reference;
    struct CxxLambdaCaptureSpec* next;
} CxxLambdaCaptureSpec;

static CxxParserValueBinding* active_value_bindings;
static CxxParserValueBinding* saved_value_bindings[32];
static int saved_value_binding_depth;
static CxxReferenceCapture* active_reference_captures;
static CxxReferenceCapture* saved_reference_captures[32];
static int saved_reference_capture_depth;
static unsigned cxx_lambda_counter;
static unsigned cxx_range_for_counter;

static int cxx_class_pack_index(CxxTemplate* tmpl);

static void cxx_parser_expr_loc(SourceLoc* location, const Expr* expression,
                                const SourceLoc* fallback) {
    if (!location) return;
    location->filename = NULL;
    location->line = 0;
    location->column = 0;
    if (expression) {
        location->filename = expression->loc.filename;
        location->line = expression->loc.line;
        location->column = expression->loc.column;
    } else if (fallback) {
        location->filename = fallback->filename;
        location->line = fallback->line;
        location->column = fallback->column;
    }
}

void rcc_parser_cxx_begin_function_parameters(DeclList* parameters) {
    if (saved_value_binding_depth >=
        (int)(sizeof(saved_value_bindings) / sizeof(saved_value_bindings[0]))) {
        rcc_fatal("C++ parser function nesting is too deep");
    }
    saved_value_bindings[saved_value_binding_depth++] = active_value_bindings;
    active_value_bindings = NULL;
    for (DeclList* item = parameters; item; item = item->next) {
        if (item->decl && item->decl->name) {
            CxxParserValueBinding* binding = ast_arena_alloc(sizeof(*binding));
            binding->name = item->decl->name;
            binding->type = item->decl->type;
            binding->next = active_value_bindings;
            active_value_bindings = binding;
        }
    }
}

void rcc_parser_cxx_end_function_parameters(void) {
    if (saved_value_binding_depth <= 0) {
        rcc_fatal("C++ parser function binding stack underflow");
    }
    active_value_bindings =
        saved_value_bindings[--saved_value_binding_depth];
}

void rcc_parser_cxx_add_value_binding(const char* name, Type* type) {
    CxxParserValueBinding* binding;
    if (!name || !*name || !type) return;
    for (binding = active_value_bindings; binding; binding = binding->next) {
        if (binding->name && strcmp(binding->name, name) == 0) return;
    }
    binding = ast_arena_alloc(sizeof(*binding));
    binding->name = name;
    binding->type = type;
    binding->next = active_value_bindings;
    active_value_bindings = binding;
}

static Type* cxx_parser_value_type(const char* name) {
    for (CxxParserValueBinding* binding = active_value_bindings;
         binding; binding = binding->next) {
        if (binding->name && name && strcmp(binding->name, name) == 0) {
            return binding->type;
        }
    }
    return NULL;
}

/* Function-template deduction happens while the source is still being
 * parsed, before the normal semantic pass has assigned types to every
 * expression.  Recover the type of the expression forms whose result is
 * already determined by the parsed AST.  Returning NULL is intentional: it
 * keeps genuinely dependent or unresolved expressions on the diagnostic
 * path instead of inventing a recovery type. */
static Type* cxx_parser_expression_type(Expr* expression) {
    Type* left;
    Type* right;
    Type* function_type;
    if (!expression) return NULL;
    if (expression->type) return expression->type;

    switch (expression->kind) {
        case EXPR_IDENT:
            if (expression->ident_decl && expression->ident_decl->type) {
                return expression->ident_decl->type;
            }
            left = cxx_parser_value_type(expression->ident_name);
            if (left) return left;
            for (DeclList* item = active_ast ? active_ast->decls : NULL;
                 item; item = item->next) {
                Decl* declaration = item->decl;
                const char* name = declaration ? declaration->name : NULL;
                const char* tail = name ? strrchr(name, ':') : NULL;
                tail = tail && tail > name && tail[-1] == ':'
                    ? tail + 1 : name;
                if (declaration && declaration->type &&
                    ((name && expression->ident_name &&
                      strcmp(name, expression->ident_name) == 0) ||
                     (tail && expression->ident_name &&
                      strcmp(tail, expression->ident_name) == 0))) {
                    return declaration->type;
                }
            }
            return NULL;

        case EXPR_CAST:
            return expression->cast_type;

        case EXPR_CALL:
            function_type = cxx_parser_expression_type(
                expression->call_func);
            if (function_type && function_type->kind == TYPE_PTR) {
                function_type = function_type->base;
            }
            return function_type && function_type->kind == TYPE_FUNC
                ? function_type->ret_type : NULL;

        case EXPR_ADDR:
            left = cxx_parser_expression_type(expression->unary_operand);
            return left ? type_ptr(left) : NULL;

        case EXPR_DEREF:
            left = cxx_parser_expression_type(expression->unary_operand);
            return left && left->kind == TYPE_PTR ? left->base : NULL;

        case EXPR_INDEX:
            left = cxx_parser_expression_type(expression->index_base);
            if (left && left->kind == TYPE_ARRAY) return left->base;
            return left && left->kind == TYPE_PTR ? left->base : NULL;

        case EXPR_MEMBER:
        case EXPR_PTR_MEMBER:
            left = cxx_parser_expression_type(expression->member_base);
            if (expression->kind == EXPR_PTR_MEMBER && left &&
                left->kind == TYPE_PTR) {
                left = left->base;
            }
            for (TypeField* field = left ? left->fields : NULL;
                 field; field = field->next) {
                if (field->name && expression->member_name &&
                    strcmp(field->name, expression->member_name) == 0) {
                    return field->type;
                }
            }
            return NULL;

        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
        case EXPR_PREINC:
        case EXPR_PREDEC:
        case EXPR_POSTINC:
        case EXPR_POSTDEC:
            return cxx_parser_expression_type(expression->unary_operand);

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
        case EXPR_ASSIGN:
        case EXPR_ADD_ASSIGN:
        case EXPR_SUB_ASSIGN:
        case EXPR_MUL_ASSIGN:
        case EXPR_DIV_ASSIGN:
        case EXPR_MOD_ASSIGN:
        case EXPR_AND_ASSIGN:
        case EXPR_OR_ASSIGN:
        case EXPR_XOR_ASSIGN:
        case EXPR_LSHIFT_ASSIGN:
        case EXPR_RSHIFT_ASSIGN:
            left = cxx_parser_expression_type(expression->binary_lhs);
            right = cxx_parser_expression_type(expression->binary_rhs);
            if ((expression->kind == EXPR_ADD ||
                 expression->kind == EXPR_SUB) && left && right) {
                if (left->kind == TYPE_PTR) return left;
                if (right->kind == TYPE_PTR && expression->kind == EXPR_ADD) {
                    return right;
                }
            }
            if (left && right && type_is_arithmetic(left) &&
                type_is_arithmetic(right)) {
                return type_common(left, right);
            }
            return left;

        case EXPR_EQ:
        case EXPR_NE:
        case EXPR_LT:
        case EXPR_GT:
        case EXPR_LE:
        case EXPR_GE:
        case EXPR_AND:
        case EXPR_OR:
            return type_int;

        case EXPR_COND:
            left = cxx_parser_expression_type(expression->cond_then);
            right = cxx_parser_expression_type(expression->cond_else);
            if (left && right && type_is_arithmetic(left) &&
                type_is_arithmetic(right)) {
                return type_common(left, right);
            }
            return left && right && type_is_compatible(left, right)
                ? left : NULL;

        case EXPR_COMMA:
            return cxx_parser_expression_type(expression->binary_rhs);

        case EXPR_COMPOUND:
            return expression->compound_type;

        default:
            return NULL;
    }
}

/* Reference captures are represented by pointer parameters in the lowered
 * immediate-call ABI.  Rewrite an occurrence in the lambda body to a real
 * dereference so reads and writes observe the original object. */
Expr* rcc_parser_cxx_capture_expression(const char* name, SourceLoc loc) {
    for (CxxReferenceCapture* capture = active_reference_captures;
         capture; capture = capture->next) {
        if (capture->name && name && strcmp(capture->name, name) == 0) {
            return expr_unary(EXPR_DEREF, expr_ident(name, loc), loc);
        }
    }
    return NULL;
}

/* Parser utilities from parser.c */
static Token* peek(void) { return parser.cur; }
static Token* previous(void) { return parser.prev; }
static bool check(TokenType type) { return peek()->type == type; }
static bool at_end(void) { return check(TOK_EOF); }

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

static ExprKind cxx_fold_operator_kind(TokenType token) {
    switch (token) {
        case TOK_PLUS: return EXPR_ADD;
        case TOK_MINUS: return EXPR_SUB;
        case TOK_STAR: return EXPR_MUL;
        case TOK_SLASH: return EXPR_DIV;
        case TOK_PERCENT: return EXPR_MOD;
        case TOK_AMP: return EXPR_BITAND;
        case TOK_PIPE: return EXPR_BITOR;
        case TOK_CARET: return EXPR_BITXOR;
        case TOK_LSHIFT: return EXPR_LSHIFT;
        case TOK_RSHIFT: return EXPR_RSHIFT;
        case TOK_EQ: return EXPR_EQ;
        case TOK_NE: return EXPR_NE;
        case TOK_LT: return EXPR_LT;
        case TOK_GT: return EXPR_GT;
        case TOK_LE: return EXPR_LE;
        case TOK_GE: return EXPR_GE;
        case TOK_AND: return EXPR_AND;
        case TOK_OR: return EXPR_OR;
        case TOK_COMMA: return EXPR_COMMA;
        default: return EXPR_INT_LIT;
    }
}

extern Expr* rcc_parse_cxx_fold_operand(void);

/* Claim only unary and binary fold spellings.  Other parenthesized expressions remain on
 * the common precedence parser; an unsupported fold operator is diagnosed
 * here instead of being reinterpreted as a scalar. */
Expr* rcc_parse_cxx_fold_expression(void) {
    SourceLoc loc;
    ExprKind operator_kind;
    const char* pack_name;
    Token* saved_cur;
    Token* saved_prev;

    if (!check(TOK_LPAREN) || !parser.cur->next) return NULL;
    /* A unary fold may use an expression pattern, for example
     * `((args + 1) + ...)`.  Try the parenthesized pattern first, then restore
     * the token cursor so ordinary parenthesized expressions keep the common
     * parser path. */
    if (parser.cur->next->type == TOK_LPAREN) {
        Expr* pattern;
        Expr* fold;
        saved_cur = parser.cur;
        saved_prev = parser.prev;
        loc = peek()->loc;
        advance(); /* outer ( */
        pattern = rcc_parse_cxx_fold_operand();
        operator_kind = cxx_fold_operator_kind(peek()->type);
        if (pattern && operator_kind != EXPR_INT_LIT) {
            advance();
            if (match(TOK_ELLIPSIS) && match(TOK_RPAREN)) {
                fold = expr_cxx_fold(NULL, operator_kind, false, loc);
                fold->cxx_fold_pattern = pattern;
                return fold;
            }
        }
        parser.cur = saved_cur;
        parser.prev = saved_prev;
    }
    if (parser.cur->next->type == TOK_ELLIPSIS) {
        loc = peek()->loc;
        advance(); /* ( */
        advance(); /* ... */
        operator_kind = cxx_fold_operator_kind(peek()->type);
        if (operator_kind == EXPR_INT_LIT) {
            rcc_error(peek()->loc,
                      "unsupported C++ fold operator; expected a binary operator");
            if (!at_end()) advance();
        } else {
            advance();
        }
        if (!check(TOK_IDENT)) {
            rcc_error(peek()->loc,
                      "C++ fold expression requires a parameter pack name");
            while (!check(TOK_RPAREN) && !at_end()) advance();
            expect(TOK_RPAREN, ")");
            return expr_cxx_fold(NULL, EXPR_ADD, true, loc);
        }
        pack_name = advance()->value.str_val;
        expect(TOK_RPAREN, ")");
        return expr_cxx_fold(pack_name, operator_kind, true, loc);
    }

    /* A binary left fold has the spelling `(init op ... op pack)`. */
    if (parser.cur->next->next && parser.cur->next->next->next &&
        parser.cur->next->next->next->type == TOK_ELLIPSIS &&
        parser.cur->next->next->next->next &&
        parser.cur->next->next->next->next->next &&
        parser.cur->next->next->next->next->next->type == TOK_IDENT) {
        Expr* initializer;
        loc = peek()->loc;
        advance(); /* ( */
        initializer = rcc_parse_cxx_fold_operand();
        operator_kind = cxx_fold_operator_kind(peek()->type);
        if (operator_kind == EXPR_INT_LIT) {
            rcc_error(peek()->loc,
                      "unsupported C++ fold operator; expected a binary operator");
            if (!at_end()) advance();
        } else {
            advance();
        }
        expect(TOK_ELLIPSIS, "...");
        {
            ExprKind second_operator_kind = cxx_fold_operator_kind(peek()->type);
            if (second_operator_kind == EXPR_INT_LIT) {
                rcc_error(peek()->loc,
                          "unsupported C++ fold operator; expected a binary operator");
                if (!at_end()) advance();
            } else {
                advance();
            }
            if (second_operator_kind != operator_kind) {
                rcc_error(peek()->loc,
                          "C++ binary fold requires the same operator on both sides of ...");
            }
        }
        if (!check(TOK_IDENT)) {
            rcc_error(peek()->loc,
                      "C++ fold expression requires a parameter pack name");
            while (!check(TOK_RPAREN) && !at_end()) advance();
            expect(TOK_RPAREN, ")");
            return expr_cxx_fold(NULL, EXPR_ADD, true, loc);
        }
        pack_name = advance()->value.str_val;
        expect(TOK_RPAREN, ")");
        {
            Expr* fold = expr_cxx_fold(pack_name, operator_kind, true, loc);
            fold->cxx_fold_init = initializer;
            return fold;
        }
    }

    /* A unary right fold has the spelling `(pack op ...)`.  Require the
     * ellipsis immediately before the closing parenthesis so ordinary
     * parenthesized expressions are left to the normal parser. */
    if (parser.cur->next->type != TOK_IDENT ||
        !parser.cur->next->next || !parser.cur->next->next->next ||
        parser.cur->next->next->next->type != TOK_ELLIPSIS) {
        return NULL;
    }

    /* A binary right fold has the spelling `(pack op ... op init)`. */
    if (parser.cur->next->next->next->next &&
        parser.cur->next->next->next->next->type != TOK_RPAREN) {
        Expr* initializer;
        ExprKind second_operator_kind;
        loc = peek()->loc;
        advance(); /* ( */
        pack_name = advance()->value.str_val;
        operator_kind = cxx_fold_operator_kind(peek()->type);
        if (operator_kind == EXPR_INT_LIT) {
            rcc_error(peek()->loc,
                      "unsupported C++ fold operator; expected a binary operator");
            if (!at_end()) advance();
        } else {
            advance();
        }
        expect(TOK_ELLIPSIS, "...");
        second_operator_kind = cxx_fold_operator_kind(peek()->type);
        if (second_operator_kind == EXPR_INT_LIT) {
            rcc_error(peek()->loc,
                      "unsupported C++ fold operator; expected a binary operator");
            if (!at_end()) advance();
        } else {
            advance();
        }
        if (second_operator_kind != operator_kind) {
            rcc_error(peek()->loc,
                      "C++ binary fold requires the same operator on both sides of ...");
        }
        initializer = rcc_parse_cxx_fold_operand();
        expect(TOK_RPAREN, ")");
        {
            Expr* fold = expr_cxx_fold(pack_name, operator_kind, false, loc);
            fold->cxx_fold_init = initializer;
            return fold;
        }
    }

    loc = peek()->loc;
    advance(); /* ( */
    pack_name = advance()->value.str_val;
    operator_kind = cxx_fold_operator_kind(peek()->type);
    if (operator_kind == EXPR_INT_LIT) {
        rcc_error(peek()->loc,
                  "unsupported C++ fold operator; expected a binary operator");
        if (!at_end()) advance();
    } else {
        advance();
    }
    expect(TOK_ELLIPSIS, "...");
    expect(TOK_RPAREN, ")");
    return expr_cxx_fold(pack_name, operator_kind, false, loc);
}

/* Forward declarations */
static Expr* parse_cxx_expression(void);
extern Expr* parse_expression(void);
extern Expr* parse_assignment_expression(void);
extern Expr* rcc_parser_parse_initializer(void);
extern Type* rcc_parser_parse_cxx_declarator(Type* base_type,
                                              const char** name,
                                              DeclList** parameters);
static Stmt* parse_cxx_statement(void);
static DeclList* parse_cxx_parameter_declarations(void);
static Type* parse_cxx_type_spec(void);
static void resolve_class_bases(CxxClass* cls, SourceLoc loc);
static CxxClass* find_class(const char* qualified_name);
static CxxTemplate* find_class_template(const char* qualified_name);
static bool is_active_template_type(const char* name);
static int active_template_template_parameter_index(const char* name);
static bool eval_template_integer_expression(Expr* expression,
                                              CxxTemplate* tmpl,
                                              const int64_t* values,
                                              const bool* value_present,
                                              int64_t* result);
static Decl* parse_cxx_function_declaration(bool parse_body,
                                            bool* is_constexpr,
                                            bool* is_noexcept,
                                            bool* is_consteval);
CxxTemplate* parse_cxx_template(void);
static void add_cxx_declaration(AST* ast, Stmt* statement,
                                bool c_language_linkage);

/* `constexpr`/`consteval` can introduce either a function or a variable.  The dedicated
 * function parser is needed for C++ parameter/body handling, while ordinary
 * declaration parsing owns the variable initializer grammar.  Stop at the
 * first declaration-level initializer boundary so a call in a variable
 * initializer is not mistaken for a function declarator. */
static bool cxx_constexpr_starts_function(void) {
    Token* token = parser.cur;
    int parentheses = 0;
    int brackets = 0;

    if (!token || (token->type != TOK_CONSTEXPR &&
                   token->type != TOK_CONSTEVAL)) return false;
    token = token->next;
    for (; token; token = token->next) {
        if (parentheses == 0 && brackets == 0) {
            if (token->type == TOK_ASSIGN || token->type == TOK_LBRACE ||
                token->type == TOK_SEMICOLON) {
                return false;
            }
            if (token->type == TOK_LPAREN) return true;
        }
        if (token->type == TOK_LPAREN) ++parentheses;
        else if (token->type == TOK_RPAREN && parentheses > 0) --parentheses;
        else if (token->type == TOK_LBRACKET) ++brackets;
        else if (token->type == TOK_RBRACKET && brackets > 0) --brackets;
        if (token->type == TOK_EOF) break;
    }
    return false;
}

static bool cxx_decltype_auto_starts_function(void) {
    Token* token = parser.cur;
    if (!token || token->type != TOK_DECLTYPE) return false;
    token = token->next;
    if (!token || token->type != TOK_LPAREN) return false;
    token = token->next;
    if (!token || token->type != TOK_AUTO) return false;
    token = token->next;
    if (!token || token->type != TOK_RPAREN) return false;
    token = token->next;
    if (!token || token->type != TOK_IDENT) return false;
    token = token->next;
    return token && token->type == TOK_LPAREN;
}

/* ═══════════════════════════════════════
 * C++ Scope Resolution
 * ═══════════════════════════════════════ */

/* Parse qualified name: ns::ns::name */
static const char* parse_qualified_name(void) {
    char buffer[512] = "";

    /* Global scope? */
    if (match(TOK_SCOPE)) {
        strcat(buffer, "::");
    }

    if (!check(TOK_IDENT)) {
        rcc_error(peek()->loc, "expected identifier");
        return rcc_intern("");
    }

    Token* name = advance();
    strcat(buffer, name->value.str_val);

    while (match(TOK_SCOPE)) {
        strcat(buffer, "::");
        if (!check(TOK_IDENT)) {
            rcc_error(peek()->loc, "expected identifier after ::");
            break;
        }
        name = advance();
        strcat(buffer, name->value.str_val);
    }

    return rcc_intern(buffer);
}

/* ═══════════════════════════════════════
 * C++ Class Parsing
 * ═══════════════════════════════════════ */

/* Parse access specifier */
static AccessSpec parse_access_spec(void) {
    if (match(TOK_PUBLIC)) {
        expect(TOK_COLON, ":");
        return ACCESS_PUBLIC;
    }
    if (match(TOK_PRIVATE)) {
        expect(TOK_COLON, ":");
        return ACCESS_PRIVATE;
    }
    if (match(TOK_PROTECTED)) {
        expect(TOK_COLON, ":");
        return ACCESS_PROTECTED;
    }
    return (AccessSpec)-1;
}

static bool check_next(TokenType type) {
    return parser.cur->next && parser.cur->next->type == type;
}

/* C++ attributes are metadata at this stage.  Consume complete [[...]]
 * groups so they cannot be mistaken for array declarators. */
static void skip_cxx_attributes(void) {
    while (check(TOK_LBRACKET) && check_next(TOK_LBRACKET)) {
        SourceLoc loc = peek()->loc;
        int depth = 1;
        advance();
        advance();
        while (depth > 0 && !at_end()) {
            if (check(TOK_LBRACKET) && check_next(TOK_LBRACKET)) {
                advance();
                advance();
                depth++;
            } else if (check(TOK_RBRACKET) && check_next(TOK_RBRACKET)) {
                advance();
                advance();
                depth--;
            } else {
                advance();
            }
        }
        if (depth != 0) {
            rcc_error(loc, "unterminated C++ attribute specifier");
            return;
        }
    }
}

static void skip_balanced(TokenType open, TokenType close) {
    int depth = 0;
    if (!match(open)) return;
    depth = 1;
    while (depth > 0 && !at_end()) {
        if (match(open)) depth++;
        else if (match(close)) depth--;
        else advance();
    }
}

static void skip_cxx_template_arguments(void) {
    SourceLoc loc = peek()->loc;
    int depth = 0;
    if (!match(TOK_LT)) return;
    depth = 1;
    while (depth > 0 && !at_end()) {
        if (match(TOK_LT)) {
            depth++;
        } else if (match(TOK_GT)) {
            depth--;
        } else if (match(TOK_RSHIFT)) {
            depth = depth > 1 ? depth - 2 : 0;
        } else {
            advance();
        }
    }
    if (depth != 0) {
        rcc_error(loc, "unterminated template argument list");
    }
}

static const char* parse_operator_name(void) {
    TokenType operation;
    if (match(TOK_LPAREN)) {
        expect(TOK_RPAREN, ")");
        return rcc_intern("operator()");
    }
    if (match(TOK_LBRACKET)) {
        expect(TOK_RBRACKET, "]");
        return rcc_intern("operator[]");
    }
    operation = peek()->type;
    switch (operation) {
        case TOK_ASSIGN:
            advance();
            return rcc_intern("operator=");
        case TOK_PLUS_ASSIGN:
            advance();
            return rcc_intern("operator+=");
        case TOK_MINUS_ASSIGN:
            advance();
            return rcc_intern("operator-=");
        case TOK_STAR_ASSIGN:
            advance();
            return rcc_intern("operator*=");
        case TOK_SLASH_ASSIGN:
            advance();
            return rcc_intern("operator/=");
        case TOK_PERCENT_ASSIGN:
            advance();
            return rcc_intern("operator%=");
        case TOK_AMP_ASSIGN:
            advance();
            return rcc_intern("operator&=");
        case TOK_PIPE_ASSIGN:
            advance();
            return rcc_intern("operator|=");
        case TOK_CARET_ASSIGN:
            advance();
            return rcc_intern("operator^=");
        case TOK_LSHIFT_ASSIGN:
            advance();
            return rcc_intern("operator<<=");
        case TOK_RSHIFT_ASSIGN:
            advance();
            return rcc_intern("operator>>=");
        case TOK_PLUS:
            advance();
            return rcc_intern("operator+");
        case TOK_MINUS:
            advance();
            return rcc_intern("operator-");
        case TOK_STAR:
            advance();
            return rcc_intern("operator*");
        case TOK_SLASH:
            advance();
            return rcc_intern("operator/");
        case TOK_PERCENT:
            advance();
            return rcc_intern("operator%");
        case TOK_INC:
            advance();
            return rcc_intern("operator++");
        case TOK_DEC:
            advance();
            return rcc_intern("operator--");
        case TOK_EQ:
            advance();
            return rcc_intern("operator==");
        case TOK_NE:
            advance();
            return rcc_intern("operator!=");
        case TOK_LT:
            advance();
            return rcc_intern("operator<");
        case TOK_LE:
            advance();
            return rcc_intern("operator<=");
        case TOK_GT:
            advance();
            return rcc_intern("operator>");
        case TOK_GE:
            advance();
            return rcc_intern("operator>=");
        case TOK_AMP:
            advance();
            return rcc_intern("operator&");
        case TOK_PIPE:
            advance();
            return rcc_intern("operator|");
        case TOK_CARET:
            advance();
            return rcc_intern("operator^");
        case TOK_TILDE:
            advance();
            return rcc_intern("operator~");
        case TOK_NOT:
            advance();
            return rcc_intern("operator!");
        case TOK_AND:
            advance();
            return rcc_intern("operator&&");
        case TOK_OR:
            advance();
            return rcc_intern("operator||");
        case TOK_LSHIFT:
            advance();
            return rcc_intern("operator<<");
        case TOK_RSHIFT:
            advance();
            return rcc_intern("operator>>");
        case TOK_COMMA:
            advance();
            return rcc_intern("operator,");
        case TOK_ARROW:
            advance();
            return rcc_intern("operator->");
        case TOK_DOT_STAR:
            advance();
            return rcc_intern("operator.*");
        case TOK_ARROW_STAR:
            advance();
            return rcc_intern("operator->*");
        default:
            rcc_error(peek()->loc, "expected overloaded operator");
            return NULL;
    }
}

static Type* cxx_function_type_from_parameters(Type* return_type,
                                               DeclList* params) {
    TypeParam* type_params = NULL;
    TypeParam** tail = &type_params;
    for (DeclList* item = params; item; item = item->next) {
        TypeParam* parameter = ast_arena_alloc(sizeof(*parameter));
        parameter->name = item->decl ? item->decl->name : NULL;
        parameter->type = item->decl ? item->decl->type : NULL;
        parameter->is_bitfield = false;
        parameter->bit_width = 0u;
        parameter->is_static = false;
        parameter->initializer = item->decl ? item->decl->param_default : NULL;
        parameter->cxx_access = ACCESS_PUBLIC;
        parameter->next = NULL;
        *tail = parameter;
        tail = &parameter->next;
    }
    return type_func(return_type, type_params, false);
}

/* Parse a namespace-scope overloaded operator after the common parser has
 * consumed its return type.  The resulting declaration is an ordinary C++
 * function, so namespace lookup/ADL and the existing overload resolver remain
 * the single source of truth for calls. */
Stmt* rcc_parse_cxx_operator_declaration(Type* return_type, SourceLoc loc) {
    const char* name;
    DeclList* params;
    StmtList* statements = NULL;
    Stmt* body = NULL;
    Decl* declaration;

    if (!return_type || !match(TOK_OPERATOR)) return NULL;
    name = parse_operator_name();
    expect(TOK_LPAREN, "(");
    params = parse_cxx_parameter_declarations();
    expect(TOK_RPAREN, ")");
    if (match(TOK_NOEXCEPT) && check(TOK_LPAREN)) {
        skip_balanced(TOK_LPAREN, TOK_RPAREN);
    }
    if (match(TOK_LBRACE)) {
        rcc_parser_cxx_begin_function_parameters(params);
        while (!check(TOK_RBRACE) && !at_end()) {
            Token* start = parser.cur;
            Stmt* statement = parse_cxx_statement();
            if (statement) stmtlist_append(&statements, statement);
            if (parser.cur == start && !at_end()) advance();
        }
        expect(TOK_RBRACE, "}");
        rcc_parser_cxx_end_function_parameters();
        body = stmt_block(statements, loc);
    } else {
        expect(TOK_SEMICOLON, ";");
    }
    declaration = decl_func(name, cxx_function_type_from_parameters(
        return_type, params), params, body, loc);
    declaration->func_has_cxx_linkage = true;
    return stmt_decl(declaration, loc);
}

typedef struct ParsedConstructorInitializer {
    CxxConstructorInitializer* items;
    int count;
    bool is_supported;
} ParsedConstructorInitializer;

/* Convert the small, ABI-transparent constructor-body form into the same
 * field initializer representation used by a mem-initializer list.  The
 * conversion is deliberately structural: every statement must assign the
 * next data field, and each right-hand side must be either the corresponding
 * constructor parameter or an integer constant for a default constructor.
 * No arbitrary constructor statement is ever interpreted by codegen. */
static bool lowerable_constructor_body(CxxClass* cls,
                                       CxxConstructorInfo* constructor) {
    StmtList* statement_list;
    TypeParam* field;
    TypeParam* parameter;
    CxxConstructorInitializer* items = NULL;
    CxxConstructorInitializer** tail = &items;
    int count = 0;

    if (!cls || !constructor || constructor->initializers ||
        constructor->initializer_count != 0 ||
        !constructor->method || !constructor->method->decl ||
        !constructor->method->decl->func_body ||
        constructor->method->decl->func_body->kind != STMT_BLOCK) {
        return false;
    }
    statement_list = constructor->method->decl->func_body->block_stmts;
    field = cls->fields;
    parameter = constructor->parameters;
    while (statement_list && field) {
        Stmt* statement = statement_list->stmt;
        Expr* assignment;
        const char* field_name = NULL;
        Expr* value;
        CxxConstructorInitializer* item;

        if (!statement || statement->kind != STMT_EXPR ||
            !statement->expr || statement->expr->kind != EXPR_ASSIGN) {
            return false;
        }
        assignment = statement->expr;
        if (assignment->binary_lhs &&
            assignment->binary_lhs->kind == EXPR_IDENT) {
            field_name = assignment->binary_lhs->ident_name;
        } else if (assignment->binary_lhs &&
                   assignment->binary_lhs->kind == EXPR_PTR_MEMBER &&
                   assignment->binary_lhs->member_base &&
                   assignment->binary_lhs->member_base->kind == EXPR_IDENT &&
                   assignment->binary_lhs->member_base->ident_name &&
                   strcmp(assignment->binary_lhs->member_base->ident_name,
                          "this") == 0) {
            field_name = assignment->binary_lhs->member_name;
        }
        if (!field_name || !field->name ||
            strcmp(field_name, field->name) != 0) {
            return false;
        }
        value = assignment->binary_rhs;
        if (!value) return false;
        if (constructor->parameter_count == 0) {
            int64_t constant_value;
            if (!expr_eval_integer_constant(value, &constant_value)) {
                return false;
            }
        } else {
            if (!parameter || !parameter->name ||
                value->kind != EXPR_IDENT ||
                strcmp(value->ident_name, parameter->name) != 0) {
                return false;
            }
            parameter = parameter->next;
        }
        item = ast_arena_alloc(sizeof(*item));
        item->field = field->name;
        item->value = value;
        item->arguments = NULL;
        item->constructor = NULL;
        item->is_base_initializer = false;
        item->is_virtual_base_initializer = false;
        item->is_delegating_constructor = false;
        item->is_default_member_initializer = false;
        item->next = NULL;
        *tail = item;
        tail = &item->next;
        ++count;
        field = field->next;
        statement_list = statement_list->next;
    }
    if (statement_list || field ||
        (constructor->parameter_count != 0 && parameter) || count == 0) {
        return false;
    }
    constructor->initializers = items;
    constructor->initializer_count = count;
    constructor->initializers_are_supported = true;
    constructor->body_is_empty = true;
    return true;
}

static TypeParam* cxx_constructor_field_parameter(CxxClass* cls,
                                                  const char* name) {
    for (TypeParam* field = cls ? cls->fields : NULL; field;
         field = field->next) {
        if (!field->is_static && field->name && name &&
            strcmp(field->name, name) == 0) {
            return field;
        }
    }
    return NULL;
}

static const char* cxx_unqualified_name(const char* name) {
    const char* separator;
    if (!name) return NULL;
    separator = strrchr(name, ':');
    return separator && separator > name && separator[-1] == ':'
        ? separator + 1 : name;
}

static bool cxx_constructor_base_name_matches(CxxClass* cls, int index,
                                              const char* name) {
    CxxClass* base;
    const char* declared_name;
    const char* declared_tail;
    const char* name_tail;
    if (!cls || index < 0 || index >= cls->base_count || !name) return false;
    base = cls->bases[index].base;
    declared_name = cls->bases[index].base_name;
    declared_tail = cxx_unqualified_name(declared_name);
    name_tail = cxx_unqualified_name(name);
    if (declared_name && strcmp(declared_name, name) == 0) return true;
    if (base && base->name && strcmp(base->name, name) == 0) return true;
    return (declared_tail && name_tail &&
            strcmp(declared_tail, name_tail) == 0) ||
           (base && base->name && name_tail &&
            strcmp(cxx_unqualified_name(base->name), name_tail) == 0);
}

static int cxx_constructor_base_index(CxxClass* cls, const char* name) {
    if (!cls || !name) return -1;
    for (int index = 0; index < cls->base_count; ++index) {
        if (cxx_constructor_base_name_matches(cls, index, name)) return index;
    }
    return -1;
}

static int cxx_constructor_virtual_base_index(CxxClass* cls,
                                              const char* name) {
    const char* name_tail = cxx_unqualified_name(name);
    if (!cls || !name || !name_tail) return -1;
    for (int index = 0; index < cls->virtual_base_count; ++index) {
        CxxClass* base = cls->virtual_bases[index].base;
        const char* base_tail = base ? cxx_unqualified_name(base->name) : NULL;
        if ((base && base->name && strcmp(base->name, name) == 0) ||
            (base_tail && strcmp(base_tail, name_tail) == 0)) {
            return index;
        }
    }
    return -1;
}

static bool cxx_constructor_base_layout_supported(CxxClass* cls, int index) {
    CxxClass* base;
    if (!cls || index < 0 || index >= cls->base_count ||
        !cls->base_offsets || cls->base_offsets[index] < 0 ||
        cls->bases[index].access != ACCESS_PUBLIC) {
        return false;
    }
    base = cls->bases[index].base;
    if (!base || !base->type || !type_is_complete(base->type) ||
        base->vtable_size != 0) {
        return false;
    }
    /* A class without a user constructor is only safe to zero here when it
     * is genuinely empty.  Zeroing a POD with fields would change the
     * semantics of default-initialization, while a non-trivial member needs
     * its own constructor path. */
    if (!base->constructors &&
        (base->fields || base->base_count || base->has_field_initializer ||
         base->type->cxx_nontrivial)) {
        return false;
    }
    return true;
}

static int cxx_constructor_argument_count(ExprList* arguments) {
    int count = 0;
    for (; arguments; arguments = arguments->next) ++count;
    return count;
}

static bool cxx_constructor_scalar_constant(Expr* expression);

/* Constructor default arguments are stored on both the declaration
 * parameters and the function type parameters.  Keep the parser-side
 * arity checks independent from semantic analysis so direct initialization
 * can be recognized before the AST is walked. */
static unsigned cxx_constructor_required_parameter_count(
    CxxConstructorInfo* constructor) {
    unsigned required = 0u;
    TypeParam* parameter = constructor ? constructor->parameters : NULL;
    DeclList* declaration = constructor && constructor->method &&
        constructor->method->decl ? constructor->method->decl->func_params : NULL;
    for (; parameter; parameter = parameter->next) {
        if (!declaration || !declaration->decl ||
            !declaration->decl->param_default) {
            ++required;
        } else {
            break;
        }
        declaration = declaration->next;
    }
    return required;
}

static bool cxx_constructor_arity_has_defaults(
    CxxConstructorInfo* constructor, int supplied_count) {
    TypeParam* parameter;
    DeclList* declaration;
    int index;
    if (!constructor || supplied_count < 0 ||
        supplied_count > constructor->parameter_count) return false;
    parameter = constructor->parameters;
    declaration = constructor->method && constructor->method->decl
        ? constructor->method->decl->func_params : NULL;
    for (index = 0; index < supplied_count; ++index) {
        if (!parameter || !declaration) return false;
        parameter = parameter->next;
        declaration = declaration->next;
    }
    while (parameter && declaration) {
        if (!declaration->decl || !declaration->decl->param_default) {
            return false;
        }
        parameter = parameter->next;
        declaration = declaration->next;
    }
    return !parameter && !declaration;
}

static bool cxx_append_constructor_default_arguments(
    ExprList** arguments, CxxConstructorInfo* constructor,
    int supplied_count) {
    TypeParam* parameter;
    DeclList* declaration;
    int index;
    if (!arguments || !constructor ||
        !cxx_constructor_arity_has_defaults(constructor, supplied_count)) {
        return false;
    }
    parameter = constructor->parameters;
    declaration = constructor->method && constructor->method->decl
        ? constructor->method->decl->func_params : NULL;
    for (index = 0; index < supplied_count; ++index) {
        parameter = parameter->next;
        declaration = declaration->next;
    }
    while (parameter && declaration) {
        exprlist_append(arguments, declaration->decl->param_default);
        parameter = parameter->next;
        declaration = declaration->next;
    }
    return true;
}

static bool cxx_constructor_name_matches(CxxClass* cls, const char* name) {
    const char* class_name = cxx_unqualified_name(cls ? cls->name : NULL);
    const char* initializer_name = cxx_unqualified_name(name);
    return class_name && initializer_name &&
           strcmp(class_name, initializer_name) == 0;
}

static CxxConstructorInfo* cxx_find_delegating_constructor(
    CxxClass* cls, CxxConstructorInfo* current, int argument_count) {
    if (!cls || argument_count < 0) return NULL;
    for (CxxConstructorInfo* candidate = cls->constructors;
         candidate; candidate = candidate->next) {
        bool callable_body = candidate->method && candidate->method->decl &&
            candidate->method->decl->func_body;
        if (candidate == current || candidate->access != ACCESS_PUBLIC ||
            candidate->is_deleted || candidate->is_defaulted ||
            candidate->parameter_count < argument_count ||
            (candidate->parameter_count != argument_count &&
             !cxx_constructor_arity_has_defaults(candidate, argument_count)) ||
            !candidate->initializers_are_supported ||
            (!candidate->body_is_empty && !callable_body)) {
            continue;
        }
        return candidate;
    }
    return NULL;
}

static CxxConstructorInfo* cxx_find_base_constructor(CxxClass* base,
                                                      int argument_count) {
    if (!base) return NULL;
    for (CxxConstructorInfo* constructor = base->constructors;
         constructor; constructor = constructor->next) {
        bool callable_body = constructor->method && constructor->method->decl &&
            constructor->method->decl->func_body;
        if (constructor->access != ACCESS_PUBLIC || constructor->is_deleted ||
            constructor->is_defaulted ||
            constructor->parameter_count < argument_count ||
            (constructor->parameter_count != argument_count &&
             !cxx_constructor_arity_has_defaults(constructor, argument_count)) ||
            !constructor->initializers_are_supported ||
            (!constructor->body_is_empty && !callable_body)) {
            continue;
        }
        return constructor;
    }
    return NULL;
}

static CxxConstructorInitializer* cxx_find_base_initializer(
    CxxConstructorInfo* constructor, CxxClass* cls, int base_index,
    bool* duplicate) {
    CxxConstructorInitializer* result = NULL;
    if (duplicate) *duplicate = false;
    for (CxxConstructorInitializer* item = constructor
             ? constructor->initializers : NULL;
         item; item = item->next) {
        if (cxx_constructor_base_index(cls, item->field) != base_index) {
            continue;
        }
        if (result && duplicate) *duplicate = true;
        if (!result) result = item;
    }
    return result;
}

static CxxConstructorInitializer* cxx_find_virtual_base_initializer(
    CxxConstructorInfo* constructor, CxxClass* cls, int virtual_base_index,
    bool* duplicate) {
    CxxConstructorInitializer* result = NULL;
    if (duplicate) *duplicate = false;
    for (CxxConstructorInitializer* item = constructor
             ? constructor->initializers : NULL;
         item; item = item->next) {
        if (cxx_constructor_virtual_base_index(cls, item->field) !=
            virtual_base_index) {
            continue;
        }
        if (result && duplicate) *duplicate = true;
        if (!result) result = item;
    }
    return result;
}

static CxxConstructorInitializer* cxx_copy_constructor_initializer(
    CxxConstructorInitializer* source, const char* field, bool is_base,
    CxxConstructorInfo* base_constructor, bool is_default_member) {
    CxxConstructorInitializer* copy = ast_arena_alloc(sizeof(*copy));
    if (source) {
        *copy = *source;
    } else {
        copy->field = field;
        copy->value = NULL;
        copy->arguments = NULL;
        copy->constructor = NULL;
        copy->is_delegating_constructor = false;
        copy->is_virtual_base_initializer = false;
    }
    copy->constructor = base_constructor;
    copy->is_base_initializer = is_base;
    copy->is_default_member_initializer = is_default_member;
    copy->next = NULL;
    return copy;
}

static CxxConstructorInitializer* cxx_find_constructor_initializer(
    CxxConstructorInfo* constructor, const char* field, bool* duplicate) {
    CxxConstructorInitializer* result = NULL;
    if (duplicate) *duplicate = false;
    for (CxxConstructorInitializer* item = constructor
             ? constructor->initializers : NULL;
         item; item = item->next) {
        if (!item->field || !field || strcmp(item->field, field) != 0) {
            continue;
        }
        if (result && duplicate) *duplicate = true;
        if (!result) result = item;
    }
    return result;
}

/* Complete a constructor's effective base/member-initializer sequence in
 * declaration order.  Base initialization is limited to public,
 * non-polymorphic bases whose constructor overload and concrete subobject
 * offset are known.  Virtual bases use the most-derived fixed layout offset;
 * their pointer conversions use the runtime vbtable after construction. */
static void complete_cxx_default_member_initializers(CxxClass* cls) {
    if (!cls || (!cls->has_field_initializer && cls->base_count == 0 &&
                 !cls->constructors)) return;
    for (CxxConstructorInfo* constructor = cls->constructors; constructor;
         constructor = constructor->next) {
        CxxConstructorInitializer* ordered = NULL;
        CxxConstructorInitializer** tail = &ordered;
        CxxConstructorInitializer* delegation = NULL;
        bool valid = constructor->initializers_are_supported;
        int count = 0;

        if (!valid && constructor->initializer_count != 0) continue;
        for (CxxConstructorInitializer* item = constructor->initializers;
             item; item = item->next) {
            bool duplicate = false;
            if (cxx_constructor_name_matches(cls, item->field)) {
                int argument_count = cxx_constructor_argument_count(
                    item->arguments);
                CxxConstructorInfo* target =
                    cxx_find_delegating_constructor(
                        cls, constructor, argument_count);
                if (delegation || constructor->initializer_count != 1 ||
                    !target || target == constructor) {
                    valid = false;
                    continue;
                }
                if (target->parameter_count != argument_count &&
                    !cxx_append_constructor_default_arguments(
                        &item->arguments, target, argument_count)) {
                    valid = false;
                    continue;
                }
                item->value = item->arguments ? item->arguments->expr : NULL;
                item->constructor = target;
                item->is_base_initializer = false;
                item->is_delegating_constructor = true;
                item->is_default_member_initializer = false;
                delegation = item;
                continue;
            }
            int base_index = cxx_constructor_base_index(cls, item->field);
            if (base_index >= 0) {
                CxxClass* base = cls->bases[base_index].base;
                int argument_count = cxx_constructor_argument_count(
                    item->arguments);
                CxxConstructorInfo* base_constructor;
                if (cxx_find_base_initializer(constructor, cls, base_index,
                                              &duplicate) != item || duplicate ||
                    !cxx_constructor_base_layout_supported(cls, base_index)) {
                    valid = false;
                    continue;
                }
                base_constructor = cxx_find_base_constructor(
                    base, argument_count);
                if ((!base_constructor && argument_count != 0) ||
                    (!base_constructor && base && base->constructors)) {
                    valid = false;
                    continue;
                }
                if (base_constructor &&
                    argument_count != base_constructor->parameter_count &&
                    !cxx_append_constructor_default_arguments(
                        &item->arguments, base_constructor, argument_count)) {
                    valid = false;
                    continue;
                }
                item->value = item->arguments ? item->arguments->expr : NULL;
                item->constructor = base_constructor;
                item->is_base_initializer = true;
                item->is_virtual_base_initializer = cls->bases[base_index].is_virtual;
                item->is_default_member_initializer = false;
            } else {
                int virtual_base_index =
                    cxx_constructor_virtual_base_index(cls, item->field);
                if (virtual_base_index >= 0) {
                    CxxClass* virtual_base =
                        cls->virtual_bases[virtual_base_index].base;
                    int argument_count = cxx_constructor_argument_count(
                        item->arguments);
                    CxxConstructorInfo* virtual_constructor;
                    if (cxx_find_virtual_base_initializer(
                            constructor, cls, virtual_base_index,
                            &duplicate) != item || duplicate ||
                        !cls->virtual_bases[virtual_base_index].public_path) {
                        valid = false;
                        continue;
                    }
                    virtual_constructor = cxx_find_base_constructor(
                        virtual_base, argument_count);
                    if ((!virtual_constructor && argument_count != 0) ||
                        (!virtual_constructor && virtual_base &&
                         virtual_base->constructors)) {
                        valid = false;
                        continue;
                    }
                    if (virtual_constructor &&
                        argument_count != virtual_constructor->parameter_count &&
                        !cxx_append_constructor_default_arguments(
                            &item->arguments, virtual_constructor,
                            argument_count)) {
                        valid = false;
                        continue;
                    }
                    item->value = item->arguments ? item->arguments->expr : NULL;
                    item->constructor = virtual_constructor;
                    item->is_base_initializer = true;
                    item->is_virtual_base_initializer = true;
                    item->is_default_member_initializer = false;
                } else if (!cxx_constructor_field_parameter(cls, item->field) ||
                           cxx_find_constructor_initializer(
                               constructor, item->field, &duplicate) != item ||
                           duplicate) {
                    valid = false;
                } else {
                    item->is_base_initializer = false;
                }
            }
        }
        if (!valid) {
            constructor->initializers_are_supported = false;
            continue;
        }
        if (delegation) {
            constructor->initializers = cxx_copy_constructor_initializer(
                delegation, delegation->field, false, delegation->constructor,
                false);
            constructor->initializers->is_delegating_constructor = true;
            constructor->initializer_count = 1;
            constructor->initializers_are_supported = true;
            continue;
        }

        for (int base_index = 0; base_index < cls->base_count; ++base_index) {
            CxxClass* base = cls->bases[base_index].base;
            CxxConstructorInitializer* item = cxx_find_base_initializer(
                constructor, cls, base_index, NULL);
            CxxConstructorInfo* base_constructor;
            if (!cxx_constructor_base_layout_supported(cls, base_index)) {
                valid = false;
                break;
            }
            if (!item) {
                base_constructor = cxx_find_base_constructor(base, 0);
                if ((!base_constructor && base && base->constructors) ||
                    (!base_constructor && base &&
                     (base->fields || base->base_count ||
                      base->has_field_initializer ||
                      base->type->cxx_nontrivial))) {
                    valid = false;
                    break;
                }
                item = cxx_copy_constructor_initializer(
                    NULL, base && base->name ? base->name : NULL, true,
                    base_constructor, false);
                item->is_virtual_base_initializer =
                    cls->bases[base_index].is_virtual;
                if (base_constructor &&
                    !cxx_append_constructor_default_arguments(
                        &item->arguments, base_constructor, 0)) {
                    valid = false;
                    break;
                }
                item->value = item->arguments ? item->arguments->expr : NULL;
            } else {
                item = cxx_copy_constructor_initializer(
                    item, item->field, true, item->constructor, false);
            }
            *tail = item;
            tail = &item->next;
            ++count;
        }
        if (!valid) {
            constructor->initializers_are_supported = false;
            continue;
        }
        for (int virtual_base_index = 0;
             virtual_base_index < cls->virtual_base_count;
             ++virtual_base_index) {
            CxxClass* virtual_base =
                cls->virtual_bases[virtual_base_index].base;
            CxxConstructorInitializer* item = cxx_find_virtual_base_initializer(
                constructor, cls, virtual_base_index, NULL);
            CxxConstructorInfo* virtual_constructor;
            bool direct_virtual = false;
            for (int base_index = 0; base_index < cls->base_count;
                 ++base_index) {
                if (cls->bases[base_index].is_virtual &&
                    cls->bases[base_index].base == virtual_base) {
                    direct_virtual = true;
                    break;
                }
            }
            if (direct_virtual) continue;
            if (item) {
                item = cxx_copy_constructor_initializer(
                    item, item->field, true, item->constructor, false);
                item->is_virtual_base_initializer = true;
                *tail = item;
                tail = &item->next;
                ++count;
                continue;
            }
            if (!cls->virtual_bases[virtual_base_index].public_path) {
                valid = false;
                break;
            }
            virtual_constructor = cxx_find_base_constructor(virtual_base, 0);
            if ((!virtual_constructor && virtual_base &&
                 virtual_base->constructors) ||
                (!virtual_constructor && virtual_base &&
                 (virtual_base->fields || virtual_base->base_count ||
                  virtual_base->has_field_initializer ||
                  virtual_base->type->cxx_nontrivial))) {
                valid = false;
                break;
            }
            item = cxx_copy_constructor_initializer(
                NULL, virtual_base && virtual_base->name
                    ? virtual_base->name : NULL,
                true, virtual_constructor, false);
            item->is_virtual_base_initializer = true;
            if (virtual_constructor &&
                !cxx_append_constructor_default_arguments(
                    &item->arguments, virtual_constructor, 0)) {
                valid = false;
                break;
            }
            item->value = item->arguments ? item->arguments->expr : NULL;
            *tail = item;
            tail = &item->next;
            ++count;
        }
        if (!valid) {
            constructor->initializers_are_supported = false;
            continue;
        }
        for (TypeParam* field = cls->fields; field; field = field->next) {
            CxxConstructorInitializer* item;
            if (field->is_static || !field->name) continue;
            item = cxx_find_constructor_initializer(
                constructor, field->name, NULL);
            if (!item && field->initializer) {
                Type* type = field->type;
                if (!type || type->size <= 0 ||
                    !(type_is_integer(type) || type->kind == TYPE_ENUM ||
                      type->kind == TYPE_PTR ||
                      type->kind == TYPE_NULLPTR ||
                      type->kind == TYPE_FLOAT ||
                      type->kind == TYPE_DOUBLE) ||
                    !cxx_constructor_scalar_constant(field->initializer) ||
                    (g_opts.target_arch == ARCH_X86 && type->size > 4) ||
                    (g_opts.target_arch == ARCH_X64 && type->size > 8)) {
                    valid = false;
                    break;
                }
                item = ast_arena_alloc(sizeof(*item));
                item->field = field->name;
                item->value = field->initializer;
                item->arguments = NULL;
                item->constructor = NULL;
                item->is_base_initializer = false;
                item->is_virtual_base_initializer = false;
                item->is_delegating_constructor = false;
                item->is_default_member_initializer = true;
                item->next = NULL;
            }
            if (item) {
                item = cxx_copy_constructor_initializer(
                    item, item->field, false, item->constructor,
                    item->is_default_member_initializer);
                *tail = item;
                tail = &item->next;
                ++count;
            }
        }
        if (!valid) {
            constructor->initializers_are_supported = false;
            continue;
        }
        constructor->initializers = ordered;
        constructor->initializer_count = count;
        constructor->initializers_are_supported = true;
    }
}

/* Retain a parenthesized mem-initializer list.  It is lowered only when the
 * later verifier proves a one-to-one, declaration-order mapping from fields
 * to constructor parameters (or integer zeroes for a default constructor). */
static ParsedConstructorInitializer parse_ctor_initializer(void) {
    ParsedConstructorInitializer result = {0};
    bool supported = true;
    CxxConstructorInitializer** tail = &result.items;
    if (!match(TOK_COLON)) return result;
    do {
        const char* field = NULL;
        Expr* value = NULL;
        ExprList* arguments = NULL;
        bool current_supported = true;
        CxxConstructorInitializer* item;
        if (check(TOK_IDENT) || check(TOK_SCOPE)) {
            field = parse_qualified_name();
        } else {
            rcc_error(peek()->loc, "expected constructor initializer name");
            return result;
        }
        if (match(TOK_LPAREN)) {
            if (!check(TOK_RPAREN)) {
                do {
                    exprlist_append(&arguments,
                                    parse_assignment_expression());
                } while (match(TOK_COMMA));
                value = arguments ? arguments->expr : NULL;
            }
            expect(TOK_RPAREN, ")");
        } else if (check(TOK_LBRACE)) {
            skip_balanced(TOK_LBRACE, TOK_RBRACE);
            current_supported = false;
        } else {
            rcc_error(peek()->loc, "expected constructor initializer");
            return result;
        }
        item = ast_arena_alloc(sizeof(*item));
        item->field = field;
        item->value = value;
        item->arguments = arguments;
        item->constructor = NULL;
        item->is_base_initializer = false;
        item->is_virtual_base_initializer = false;
        item->is_delegating_constructor = false;
        item->is_default_member_initializer = false;
        item->next = NULL;
        *tail = item;
        tail = &item->next;
        ++result.count;
        if (!current_supported) supported = false;
    } while (match(TOK_COMMA));
    result.is_supported = result.count != 0 && supported;
    return result;
}

static bool class_has_virtual_member(CxxClass* cls) {
    struct CxxMember* member;
    for (member = cls->members; member; member = member->next) {
        if (member->is_virtual) return true;
    }
    return false;
}

static bool class_has_destructor(CxxClass* cls) {
    struct CxxMember* member;
    for (member = cls ? cls->members : NULL; member; member = member->next) {
        if (member->method && member->method->is_destructor) return true;
    }
    return false;
}

static int cxx_constructor_parameter_index(CxxConstructorInfo* constructor,
                                           const char* name) {
    int index = 0;
    if (!constructor || !name) return -1;
    for (TypeParam* parameter = constructor->parameters; parameter;
         parameter = parameter->next, ++index) {
        if (parameter->name && strcmp(parameter->name, name) == 0) {
            return index;
        }
    }
    return -1;
}

static Type* cxx_constructor_value_type(Type* type) {
    if (type && type->kind == TYPE_PTR && type->is_reference) {
        return type->base;
    }
    return type;
}

static bool cxx_constructor_scalar_type(Type* type) {
    return type && (type_is_integer(type) || type->kind == TYPE_ENUM ||
                    type->kind == TYPE_PTR || type->kind == TYPE_NULLPTR ||
                    type->kind == TYPE_FLOAT || type->kind == TYPE_DOUBLE);
}

static bool cxx_constructor_scalar_constant(Expr* expression) {
    int64_t integer_value;
    if (!expression) return false;
    if (expr_eval_integer_constant(expression, &integer_value)) return true;
    switch (expression->kind) {
        case EXPR_FLOAT_LIT:
            return true;
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
            return cxx_constructor_scalar_constant(
                expression->unary_operand);
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
        case EXPR_AND:
        case EXPR_OR:
            return cxx_constructor_scalar_constant(expression->binary_lhs) &&
                   cxx_constructor_scalar_constant(expression->binary_rhs);
        case EXPR_COND:
            return cxx_constructor_scalar_constant(expression->cond_test) &&
                   cxx_constructor_scalar_constant(expression->cond_then) &&
                   cxx_constructor_scalar_constant(expression->cond_else);
        case EXPR_CAST:
            return cxx_constructor_scalar_constant(expression->cast_expr);
        default:
            return false;
    }
}

/* A pointer member may be initialized from the address of an object with
 * static storage.  Keep this separate from integer constant evaluation: the
 * address must remain a relocatable symbol reference until the native object
 * or image emitter lays it out. */
static bool cxx_constructor_static_address(CxxConstructorInfo* constructor,
                                            Expr* expression) {
    Expr* operand;
    if (!expression) return false;
    if (expression->kind == EXPR_CAST) {
        return cxx_constructor_static_address(constructor,
                                              expression->cast_expr);
    }
    if (expression->kind != EXPR_ADDR) return false;
    operand = expression->unary_operand;
    return operand && operand->kind == EXPR_IDENT && operand->ident_name &&
        cxx_constructor_parameter_index(constructor, operand->ident_name) < 0;
}

static bool cxx_constructor_expression_is_lowerable(
    CxxConstructorInfo* constructor, Expr* expression, Type* target_type,
    bool* parameter_used, unsigned parameter_count) {
    if (!expression || (target_type &&
                        !cxx_constructor_scalar_type(target_type))) {
        return false;
    }
    if (expression->kind == EXPR_IDENT) {
        int index = cxx_constructor_parameter_index(
            constructor, expression->ident_name);
        TypeParam* parameter = constructor ? constructor->parameters : NULL;
        for (int step = 0; parameter && step < index; ++step) {
            parameter = parameter->next;
        }
        if (index < 0 || !parameter ||
            !cxx_constructor_scalar_type(parameter->type) ||
            (target_type &&
             !type_is_compatible(cxx_constructor_value_type(parameter->type),
                                 cxx_constructor_value_type(target_type)))) {
            return false;
        }
        if (parameter_used && index < (int)parameter_count) {
            parameter_used[index] = true;
        }
        return true;
    }
    switch (expression->kind) {
        case EXPR_INT_LIT:
        case EXPR_CHAR_LIT:
        case EXPR_FLOAT_LIT:
            return !target_type || cxx_constructor_scalar_type(target_type);
        case EXPR_NEG:
        case EXPR_NOT:
        case EXPR_BITNOT:
            return cxx_constructor_expression_is_lowerable(
                constructor, expression->unary_operand, NULL,
                parameter_used, parameter_count);
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
        case EXPR_AND:
        case EXPR_OR:
            return cxx_constructor_expression_is_lowerable(
                       constructor, expression->binary_lhs, NULL,
                       parameter_used, parameter_count) &&
                   cxx_constructor_expression_is_lowerable(
                       constructor, expression->binary_rhs, NULL,
                       parameter_used, parameter_count);
        case EXPR_COND:
            return cxx_constructor_expression_is_lowerable(
                       constructor, expression->cond_test, NULL,
                       parameter_used, parameter_count) &&
                   cxx_constructor_expression_is_lowerable(
                       constructor, expression->cond_then, NULL,
                       parameter_used, parameter_count) &&
                   cxx_constructor_expression_is_lowerable(
                       constructor, expression->cond_else, NULL,
                       parameter_used, parameter_count);
        case EXPR_CAST:
            return cxx_constructor_scalar_type(expression->cast_type) &&
                   cxx_constructor_expression_is_lowerable(
                       constructor, expression->cast_expr, NULL,
                       parameter_used, parameter_count);
        case EXPR_ADDR:
            return target_type && target_type->kind == TYPE_PTR &&
                   cxx_constructor_static_address(constructor, expression);
        default:
            return false;
    }
}

static bool cxx_base_initializer_is_lowerable(
    CxxConstructorInfo* derived_constructor,
    CxxConstructorInitializer* initializer, bool* parameter_used,
    unsigned parameter_count) {
    CxxConstructorInfo* base_constructor = initializer
        ? initializer->constructor : NULL;
    ExprList* argument = initializer ? initializer->arguments : NULL;
    TypeParam* parameter = base_constructor ? base_constructor->parameters : NULL;
    int argument_count = cxx_constructor_argument_count(argument);
    if (!base_constructor) return argument_count == 0;
    if (argument_count != base_constructor->parameter_count) return false;
    while (argument && parameter) {
        if (!cxx_constructor_expression_is_lowerable(
                derived_constructor, argument->expr, parameter->type,
                parameter_used, parameter_count)) {
            return false;
        }
        argument = argument->next;
        parameter = parameter->next;
    }
    return !argument && !parameter;
}

/* Recognize constructors whose observable object representation is exactly
 * declaration-order initialization of their data fields.  This covers the
 * SDK status/outcome wrappers without executing arbitrary constructor code. */
static uint32_t lowerable_constructor_arity_mask(CxxClass* cls) {
    CxxConstructorInfo* constructor;
    uint32_t mask = cls && cls->type && cls->type->move_constructor_method
        ? UINT32_C(1) << 1 : 0u;
    if (!cls || !cls->type->is_complete ||
        cls->has_static_field ||
        class_has_virtual_member(cls) ||
        (class_has_destructor(cls) && !cls->type->cleanup_function &&
         (!cls->destructor_method || !cls->destructor_method->decl ||
         !cls->destructor_method->decl->func_body))) {
        return 0u;
    }
    for (int base_index = 0; base_index < cls->base_count; ++base_index) {
        if (!cxx_constructor_base_layout_supported(cls, base_index)) {
            return 0u;
        }
    }
    if (!cls->constructors) return 0u;
    for (constructor = cls->constructors; constructor;
         constructor = constructor->next) {
        unsigned arity;
        unsigned minimum_arity;
        bool supported = true;
        bool parameter_used[32] = {false};
        TypeParam* field = cls->fields;
        CxxConstructorInitializer* initializer = constructor->initializers;
        if (constructor->access != ACCESS_PUBLIC ||
            constructor->is_deleted || constructor->is_defaulted) {
            continue;
        }
        arity = (unsigned)constructor->parameter_count;
        minimum_arity = cxx_constructor_required_parameter_count(constructor);
        if (arity >= 32u || minimum_arity > arity || minimum_arity >= 32u) {
            continue;
        }
        if (!constructor->body_is_empty) {
            if ((cls->base_count == 0 ||
                 constructor->initializers_are_supported) &&
                constructor->method && constructor->method->decl &&
                constructor->method->decl->func_is_cxx_method &&
                constructor->method->decl->func_body) {
                for (unsigned invocation_arity = minimum_arity;
                     invocation_arity <= arity; ++invocation_arity) {
                    mask |= UINT32_C(1) << invocation_arity;
                }
            }
            continue;
        }
        if (!constructor->initializers_are_supported) continue;
        if (initializer && initializer->is_delegating_constructor) {
            CxxConstructorInfo* target = initializer->constructor;
            TypeParam* parameter = target ? target->parameters : NULL;
            ExprList* argument = initializer->arguments;
            if (!target || target == constructor || initializer->next) {
                continue;
            }
            while (parameter && argument) {
                if (!cxx_constructor_expression_is_lowerable(
                        constructor, argument->expr, parameter->type,
                        parameter_used, arity)) {
                    supported = false;
                    break;
                }
                parameter = parameter->next;
                argument = argument->next;
            }
            if (!supported || parameter || argument) continue;
            if (arity != 0u) {
                bool all_parameters_used = true;
                for (unsigned index = 0; index < arity; ++index) {
                    if (!parameter_used[index]) {
                        all_parameters_used = false;
                        break;
                    }
                }
                if (!all_parameters_used) continue;
            }
            for (unsigned invocation_arity = minimum_arity;
                 invocation_arity <= arity; ++invocation_arity) {
                mask |= UINT32_C(1) << invocation_arity;
            }
            continue;
        }
        while (initializer && initializer->is_base_initializer) {
            if (!cxx_base_initializer_is_lowerable(
                    constructor, initializer, parameter_used, arity)) {
                supported = false;
                break;
            }
            initializer = initializer->next;
        }
        if (!supported) continue;
        while (field && initializer) {
            if (!initializer->field ||
                strcmp(initializer->field, field->name) != 0 ||
                (!initializer->value && !initializer->arguments &&
                 !(field->type && field->type->cxx_class))) {
                supported = false;
                break;
            }
            /* A class-valued member initializer is resolved by semantic
             * overload selection after all class declarations are in the
             * symbol table.  The parser can nevertheless preserve the
             * one-to-one parameter shape needed by this storage lowering. */
            if (field->type && field->type->cxx_class) {
                if (arity != 0u) {
                    ExprList* argument = initializer->arguments;
                    int used_index = argument && argument->expr &&
                        argument->expr->kind == EXPR_IDENT
                        ? cxx_constructor_parameter_index(
                            constructor, argument->expr->ident_name) : -1;
                    if (used_index < 0 || !argument || argument->next ||
                        !argument->expr ||
                        used_index >= (int)arity) {
                        supported = false;
                        break;
                    }
                    parameter_used[used_index] = true;
                }
                field = field->next;
                initializer = initializer->next;
                continue;
            }
            if (initializer->is_default_member_initializer) {
                if (!cxx_constructor_scalar_constant(initializer->value) ||
                    !field->type ||
                    !(type_is_integer(field->type) ||
                      field->type->kind == TYPE_ENUM ||
                      field->type->kind == TYPE_PTR ||
                      field->type->kind == TYPE_NULLPTR ||
                      field->type->kind == TYPE_FLOAT ||
                      field->type->kind == TYPE_DOUBLE) ||
                    field->type->size <= 0 ||
                    (g_opts.target_arch == ARCH_X86 &&
                     field->type->size > 4) ||
                    (g_opts.target_arch == ARCH_X64 &&
                     field->type->size > 8)) {
                    supported = false;
                    break;
                }
                field = field->next;
                initializer = initializer->next;
                continue;
            }
            if (arity == 0u) {
                if ((!cxx_constructor_scalar_constant(initializer->value) &&
                     !cxx_constructor_static_address(
                         constructor, initializer->value)) ||
                    !field->type ||
                    !(type_is_integer(field->type) ||
                      field->type->kind == TYPE_ENUM ||
                      field->type->kind == TYPE_PTR ||
                      field->type->kind == TYPE_NULLPTR ||
                      field->type->kind == TYPE_FLOAT ||
                      field->type->kind == TYPE_DOUBLE) ||
                    field->type->size <= 0 ||
                    (g_opts.target_arch == ARCH_X86 && field->type->size > 4) ||
                    (g_opts.target_arch == ARCH_X64 && field->type->size > 8)) {
                    supported = false;
                    break;
                }
            } else {
                if (!cxx_constructor_expression_is_lowerable(
                        constructor, initializer->value, field->type,
                        parameter_used, arity)) {
                    supported = false;
                    break;
                }
            }
            field = field->next;
            initializer = initializer->next;
        }
        if (!supported || field || initializer) {
            continue;
        }
        if (arity != 0u) {
            bool all_parameters_used = true;
            for (unsigned index = 0; index < arity; ++index) {
                if (!parameter_used[index]) {
                    all_parameters_used = false;
                    break;
                }
            }
            if (!all_parameters_used) continue;
        }
        for (unsigned invocation_arity = minimum_arity;
             invocation_arity <= arity; ++invocation_arity) {
            mask |= UINT32_C(1) << invocation_arity;
        }
    }
    return mask;
}

static TypeField* class_layout_field(CxxClass* cls, const char* name) {
    TypeField* field;
    for (field = cls && cls->type ? cls->type->fields : NULL;
         field; field = field->next) {
        if (field->name && name && strcmp(field->name, name) == 0) {
            return field;
        }
    }
    return NULL;
}

static void register_inline_class_accessors(CxxClass* cls) {
    struct CxxMember* member;
    TypeMethod** tail;
    if (!cls || !cls->type || !cls->type->is_complete) return;
    tail = &cls->type->methods;
    while (*tail) tail = &(*tail)->next;
    for (member = cls->members; member; member = member->next) {
        CxxMethod* method = member->method;
        StmtList* statements;
        Expr* returned;
        Expr* field_expr = NULL;
        int64_t constant = 0;
        bool has_constant = false;
        TypeField* field;
        TypeMethodKind kind;
        TypeMethod* lowered;
        if (!method || method->is_static || method->is_virtual ||
            method->is_pure_virtual || method->is_deleted ||
            method->is_defaulted || method->is_constructor ||
            method->is_destructor || !method->is_const ||
            method->decl->func_params || !method->decl->func_body ||
            method->decl->func_body->kind != STMT_BLOCK) {
            continue;
        }
        statements = method->decl->func_body->block_stmts;
        if (!statements || statements->next ||
            !statements->stmt || statements->stmt->kind != STMT_RETURN ||
            !statements->stmt->return_val) {
            continue;
        }
        returned = statements->stmt->return_val;
        if (returned->kind == EXPR_IDENT) {
            field_expr = returned;
            kind = TYPE_METHOD_FIELD;
        } else if (returned->kind == EXPR_EQ || returned->kind == EXPR_NE) {
            if (returned->binary_lhs &&
                returned->binary_lhs->kind == EXPR_IDENT &&
                returned->binary_rhs &&
                expr_eval_integer_constant(returned->binary_rhs,
                                           &constant)) {
                field_expr = returned->binary_lhs;
                has_constant = true;
            } else if (returned->binary_rhs &&
                       returned->binary_rhs->kind == EXPR_IDENT &&
                       returned->binary_lhs &&
                       expr_eval_integer_constant(returned->binary_lhs,
                                                  &constant)) {
                field_expr = returned->binary_rhs;
                has_constant = true;
            } else {
                continue;
            }
            kind = returned->kind == EXPR_EQ
                ? TYPE_METHOD_FIELD_EQ_CONSTANT
                : TYPE_METHOD_FIELD_NE_CONSTANT;
        } else {
            continue;
        }
        field = class_layout_field(cls, field_expr->ident_name);
        if (!field || !field->type || field->type->size <= 0) {
            continue;
        }
        if (kind == TYPE_METHOD_FIELD) {
            Type* return_type = method->decl->type->ret_type;
            Type* return_value_type = return_type &&
                return_type->is_reference ? return_type->base : return_type;
            if (!type_is_compatible(return_value_type, field->type)) {
                continue;
            }
            if (return_type && return_type->is_reference &&
                !(type_is_integer(field->type) ||
                  field->type->kind == TYPE_ENUM ||
                  field->type->kind == TYPE_PTR ||
                  field->type->kind == TYPE_ARRAY ||
                  field->type->kind == TYPE_STRUCT ||
                  field->type->kind == TYPE_UNION)) {
                continue;
            }
            if ((!return_type || !return_type->is_reference) &&
                (field->type->size > 8 ||
                 !(type_is_integer(field->type) ||
                   field->type->kind == TYPE_ENUM ||
                   field->type->kind == TYPE_PTR))) {
                continue;
            }
        } else {
            if (field->type->size > 8 ||
                !(type_is_integer(field->type) ||
                  field->type->kind == TYPE_ENUM ||
                  field->type->kind == TYPE_PTR) ||
                !(type_is_integer(method->decl->type->ret_type) ||
                  method->decl->type->ret_type->kind == TYPE_ENUM)) {
                continue;
            }
        }
        lowered = ast_arena_alloc(sizeof(*lowered));
        lowered->name = method->source_name
            ? method->source_name : method->decl->name;
        lowered->return_type = method->decl->type->ret_type;
        lowered->field = field;
        lowered->function_decl = NULL;
        lowered->source_decl = method->decl;
        lowered->kind = kind;
        lowered->constant = has_constant ? constant : 0;
        lowered->cxx_access = (unsigned char)member->access;
        lowered->is_noexcept = method->is_noexcept;
        lowered->this_owner = NULL;
        lowered->this_adjustment = 0;
        lowered->next = NULL;
        *tail = lowered;
        tail = &lowered->next;
    }
}

static TypeMethod* inline_bool_delegate_target(Type* aggregate,
                                               const char* name) {
    TypeMethod* method;
    for (method = aggregate ? aggregate->methods : NULL;
         method; method = method->next) {
        if (method->name && name && strcmp(method->name, name) == 0 &&
            method->return_type && method->return_type->kind == TYPE_BOOL &&
            method->field &&
            (method->kind == TYPE_METHOD_FIELD ||
             method->kind == TYPE_METHOD_FIELD_EQ_CONSTANT ||
             method->kind == TYPE_METHOD_FIELD_NE_CONSTANT)) {
            return method;
        }
    }
    return NULL;
}

/* Accept an operator-bool wrapper only when its complete body is:
 *
 *   return validated_zero_argument_bool_accessor();
 *
 * The target accessor has already been reduced to a field operation above,
 * so copying that operation cannot execute an arbitrary member body.  This
 * covers the SDK status/outcome wrappers while retaining explicit validation
 * for helper calls that have no executable lowering. */
static void register_inline_class_bool_delegates(CxxClass* cls) {
    struct CxxMember* member;
    TypeMethod** tail;
    if (!cls || !cls->type || !cls->type->is_complete) return;
    tail = &cls->type->methods;
    while (*tail) tail = &(*tail)->next;
    for (member = cls->members; member; member = member->next) {
        CxxMethod* method = member->method;
        StmtList* statements;
        Expr* returned;
        Expr* callee;
        TypeMethod* target;
        TypeMethod* lowered;
        if (!method || method->is_static || method->is_virtual ||
            method->is_pure_virtual || method->is_deleted ||
            method->is_defaulted || method->is_constructor ||
            method->is_destructor || !method->is_const ||
            !method->decl || !method->decl->type ||
            !(method->source_name ? method->source_name : method->decl->name) ||
            strcmp(method->source_name ? method->source_name : method->decl->name,
                   "operator conversion") != 0 ||
            !method->decl->type->ret_type ||
            method->decl->type->ret_type->kind != TYPE_BOOL ||
            method->decl->func_params || !method->decl->func_body ||
            method->decl->func_body->kind != STMT_BLOCK) {
            continue;
        }
        statements = method->decl->func_body->block_stmts;
        if (!statements || statements->next || !statements->stmt ||
            statements->stmt->kind != STMT_RETURN ||
            !statements->stmt->return_val) {
            continue;
        }
        returned = statements->stmt->return_val;
        if (returned->kind != EXPR_CALL || returned->call_args) continue;
        callee = returned->call_func;
        if (!callee || callee->kind != EXPR_IDENT || !callee->ident_name ||
            strcmp(callee->ident_name, "operator conversion") == 0) {
            continue;
        }
        target = inline_bool_delegate_target(cls->type,
                                             callee->ident_name);
        if (!target) continue;
        lowered = ast_arena_alloc(sizeof(*lowered));
        *lowered = *target;
        lowered->name = method->source_name
            ? method->source_name : method->decl->name;
        lowered->return_type = method->decl->type->ret_type;
        lowered->cxx_access = (unsigned char)member->access;
        lowered->source_decl = method->decl;
        lowered->this_owner = target->this_owner;
        lowered->this_adjustment = target->this_adjustment;
        lowered->next = NULL;
        *tail = lowered;
        tail = &lowered->next;
    }
}

/* Accept the ownership-transfer primitive only in this exact form:
 *
 *   FieldType value = field;
 *   field = integer-invalid;
 *   return value;
 *
 * The backend can then return the old scalar and invalidate the object as one
 * validated operation.  No arbitrary method body is interpreted. */
static void register_inline_class_releases(CxxClass* cls) {
    struct CxxMember* member;
    TypeMethod** tail;
    if (!cls || !cls->type || !cls->type->is_complete) return;
    tail = &cls->type->methods;
    while (*tail) tail = &(*tail)->next;
    for (member = cls->members; member; member = member->next) {
        CxxMethod* method = member->method;
        StmtList* statements;
        Stmt* declaration_statement;
        Stmt* assignment_statement;
        Stmt* return_statement;
        Decl* local;
        Expr* assignment;
        TypeField* field;
        Type* return_type;
        int64_t invalid;
        TypeMethod* lowered;
        if (!method || method->is_static || method->is_virtual ||
            method->is_pure_virtual || method->is_deleted ||
            method->is_defaulted || method->is_constructor ||
            method->is_destructor || method->is_const ||
            method->decl->func_params || !method->decl->func_body ||
            method->decl->func_body->kind != STMT_BLOCK) {
            continue;
        }
        statements = method->decl->func_body->block_stmts;
        if (!statements || !statements->next ||
            !statements->next->next || statements->next->next->next) {
            continue;
        }
        declaration_statement = statements->stmt;
        assignment_statement = statements->next->stmt;
        return_statement = statements->next->next->stmt;
        if (!declaration_statement ||
            declaration_statement->kind != STMT_DECL ||
            !declaration_statement->decl ||
            declaration_statement->decl->kind != DECL_VAR ||
            !declaration_statement->decl->var_init ||
            declaration_statement->decl->var_init->kind != EXPR_IDENT ||
            !assignment_statement ||
            assignment_statement->kind != STMT_EXPR ||
            !assignment_statement->expr ||
            assignment_statement->expr->kind != EXPR_ASSIGN ||
            !return_statement || return_statement->kind != STMT_RETURN ||
            !return_statement->return_val ||
            return_statement->return_val->kind != EXPR_IDENT) {
            continue;
        }
        local = declaration_statement->decl;
        assignment = assignment_statement->expr;
        if (!local->name ||
            strcmp(return_statement->return_val->ident_name,
                   local->name) != 0 ||
            !assignment->binary_lhs ||
            assignment->binary_lhs->kind != EXPR_IDENT ||
            !expr_eval_integer_constant(assignment->binary_rhs, &invalid)) {
            continue;
        }
        field = class_layout_field(
            cls, declaration_statement->decl->var_init->ident_name);
        if (!field || !field->name ||
            strcmp(assignment->binary_lhs->ident_name, field->name) != 0 ||
            !field->type || field->type->size <= 0 ||
            field->type->size > 8 ||
            !(type_is_integer(field->type) ||
              field->type->kind == TYPE_ENUM ||
              field->type->kind == TYPE_PTR)) {
            continue;
        }
        if (class_has_destructor(cls) && !cls->destructor_method &&
            (!cls->type->cleanup_function ||
             cls->type->cleanup_field != field ||
             cls->type->cleanup_invalid != invalid)) {
            continue;
        }
        return_type = method->decl->type->ret_type;
        if (!type_is_compatible(return_type, field->type)) continue;
        lowered = ast_arena_alloc(sizeof(*lowered));
        lowered->name = method->source_name
            ? method->source_name : method->decl->name;
        lowered->return_type = return_type;
        lowered->field = field;
        lowered->function_decl = NULL;
        lowered->source_decl = method->decl;
        lowered->kind = TYPE_METHOD_FIELD_RELEASE;
        lowered->constant = invalid;
        lowered->cxx_access = (unsigned char)member->access;
        lowered->is_noexcept = method->is_noexcept;
        lowered->this_owner = NULL;
        lowered->this_adjustment = 0;
        lowered->next = NULL;
        *tail = lowered;
        tail = &lowered->next;
    }
}

static bool class_reference_is_self(CxxClass* cls, Type* reference,
                                    bool require_rvalue) {
    Type* referred;
    const char* template_name;
    if (!cls || !reference || reference->kind != TYPE_PTR ||
        !reference->is_reference ||
        reference->is_rvalue_reference != require_rvalue ||
        !reference->base) {
        return false;
    }
    referred = reference->base;
    if (type_is_compatible(referred, cls->type)) return true;
    template_name = cls->templ && cls->templ->templated_class
        ? cls->templ->templated_class->name : NULL;
    return template_name && referred->kind == TYPE_STRUCT && referred->tag &&
           strcmp(referred->tag, template_name) == 0;
}

static bool move_parameter_is_self(CxxClass* cls, Type* parameter) {
    return class_reference_is_self(cls, parameter, true);
}

static TypeMethod* class_release_method(CxxClass* cls, const char* name,
                                        TypeField* field) {
    TypeMethod* method;
    for (method = cls && cls->type ? cls->type->methods : NULL;
         method; method = method->next) {
        if (method->kind == TYPE_METHOD_FIELD_RELEASE &&
            method->cxx_access == ACCESS_PUBLIC && method->name && name &&
            strcmp(method->name, name) == 0 && method->field == field &&
            method->return_type &&
            type_is_compatible(method->return_type, field->type)) {
            return method;
        }
    }
    return NULL;
}

/* Accept only the single-field ownership move used by the SDK:
 *
 *   Class(Class&& other) : field(other.release()) {}
 *
 * The release member must itself have passed the structural verifier above.
 * Constructor arguments can then be rewritten to that field operation without
 * interpreting arbitrary constructor code. */
static void register_inline_class_move_constructor(CxxClass* cls) {
    CxxConstructorInfo* constructor;
    TypeMethod* candidate = NULL;
    TypeField* only_field;
    if (!cls || !cls->type || !cls->type->is_complete ||
        cls->base_count != 0 || cls->has_static_field ||
        cls->has_field_initializer || class_has_virtual_member(cls)) {
        return;
    }
    only_field = cls->type->fields;
    if (!only_field || only_field->next) return;
    for (constructor = cls->constructors; constructor;
         constructor = constructor->next) {
        TypeParam* parameter = constructor->parameters;
        CxxConstructorInitializer* initializer = constructor->initializers;
        Expr* value;
        Expr* callee;
        TypeMethod* release;
        if (constructor->access != ACCESS_PUBLIC ||
            constructor->parameter_count != 1 || !parameter ||
            parameter->next || !move_parameter_is_self(cls, parameter->type) ||
            constructor->is_deleted || constructor->is_defaulted ||
            !constructor->initializers_are_supported ||
            constructor->initializer_count != 1 || !initializer ||
            initializer->next || !initializer->field ||
            strcmp(initializer->field, only_field->name) != 0 ||
            !constructor->body_is_empty || !initializer->value) {
            continue;
        }
        value = initializer->value;
        if (value->kind != EXPR_CALL || value->call_args ||
            !value->call_func || value->call_func->kind != EXPR_MEMBER) {
            continue;
        }
        callee = value->call_func;
        if (!callee->member_name || !callee->member_base ||
            callee->member_base->kind != EXPR_IDENT ||
            !parameter->name ||
            strcmp(callee->member_base->ident_name, parameter->name) != 0) {
            continue;
        }
        release = class_release_method(cls, callee->member_name, only_field);
        if (!release) continue;
        if (candidate) {
            cls->type->move_constructor_method = NULL;
            return;
        }
        candidate = release;
    }
    cls->type->move_constructor_method = candidate;
}

static Expr* cleanup_unwrap_void_cast(Expr* expression) {
    if (expression && expression->kind == EXPR_CAST &&
        expression->cast_type == type_void) {
        return expression->cast_expr;
    }
    return expression;
}

static Stmt* cleanup_single_statement(Stmt* statement) {
    if (statement && statement->kind == STMT_BLOCK) {
        StmtList* items = statement->block_stmts;
        if (!items || items->next) return NULL;
        return items->stmt;
    }
    return statement;
}

/* Accept a destructor only when its complete observable behavior is:
 *
 *   if (field != integer-invalid)
 *       (void)cleanup_function((optional-cast)field);
 *
 * This is sufficient for SDK opaque-handle RAII without interpreting an
 * arbitrary C++ member-function body. */
static void register_inline_class_cleanup(CxxClass* cls) {
    struct CxxMember* member;
    if (!cls || !cls->type || !cls->type->is_complete ||
        cls->base_count != 0 || class_has_virtual_member(cls)) {
        return;
    }
    for (member = cls->members; member; member = member->next) {
        CxxMethod* method = member->method;
        StmtList* body;
        Stmt* conditional;
        Stmt* action;
        Expr* condition;
        Expr* call;
        Expr* argument;
        ExprList* arguments;
        TypeField* field;
        int64_t invalid;
        if (!method || !method->is_destructor || method->is_deleted ||
            method->is_defaulted || method->decl->func_params ||
            !method->decl->func_body ||
            method->decl->func_body->kind != STMT_BLOCK) {
            continue;
        }
        body = method->decl->func_body->block_stmts;
        if (!body || body->next) continue;
        conditional = body->stmt;
        if (!conditional || conditional->kind != STMT_IF ||
            conditional->if_else) {
            continue;
        }
        condition = conditional->if_cond;
        if (!condition || condition->kind != EXPR_NE ||
            !condition->binary_lhs ||
            condition->binary_lhs->kind != EXPR_IDENT ||
            !expr_eval_integer_constant(condition->binary_rhs, &invalid)) {
            continue;
        }
        field = class_layout_field(
            cls, condition->binary_lhs->ident_name);
        if (!field || !field->type || field->type->size <= 0 ||
            field->type->size > 8 ||
            !(type_is_integer(field->type) ||
              field->type->kind == TYPE_ENUM ||
              field->type->kind == TYPE_PTR)) {
            continue;
        }
        action = cleanup_single_statement(conditional->if_then);
        if (!action || action->kind != STMT_EXPR) continue;
        call = cleanup_unwrap_void_cast(action->expr);
        if (!call || call->kind != EXPR_CALL || !call->call_func ||
            call->call_func->kind != EXPR_IDENT ||
            !call->call_func->ident_name) {
            continue;
        }
        arguments = call->call_args;
        if (!arguments || arguments->next || !arguments->expr) continue;
        argument = arguments->expr;
        if (argument->kind == EXPR_CAST) argument = argument->cast_expr;
        if (!argument || argument->kind != EXPR_IDENT ||
            strcmp(argument->ident_name, field->name) != 0) {
            continue;
        }
        if (cls->type->cleanup_function) {
            cls->type->cleanup_function = NULL;
            cls->type->cleanup_field = NULL;
            return;
        }
        cls->type->cleanup_function = call->call_func->ident_name;
        cls->type->cleanup_field = field;
        cls->type->cleanup_invalid = invalid;
    }
}

static bool move_expression_is_identifier(Expr* expression,
                                          const char* name) {
    return expression && expression->kind == EXPR_IDENT &&
           expression->ident_name && name &&
           strcmp(expression->ident_name, name) == 0;
}

static bool expression_is_field_constant(Expr* expression, ExprKind kind,
                                         const char* field_name,
                                         int64_t expected) {
    int64_t value;
    if (!expression || expression->kind != kind) return false;
    if (move_expression_is_identifier(expression->binary_lhs, field_name) &&
        expr_eval_integer_constant(expression->binary_rhs, &value)) {
        return value == expected;
    }
    if (move_expression_is_identifier(expression->binary_rhs, field_name) &&
        expr_eval_integer_constant(expression->binary_lhs, &value)) {
        return value == expected;
    }
    return false;
}

static bool expression_is_empty_compound(Expr* expression, Type* type) {
    ExprList* item;
    int64_t value;
    if (!expression || expression->kind != EXPR_COMPOUND ||
        !expression->compound_value_init || !expression->compound_type ||
        !type_is_compatible(expression->compound_type, type)) {
        return false;
    }
    for (item = expression->compound_init; item; item = item->next) {
        if (item->designator_kind != INIT_DESIGNATOR_NONE || !item->expr ||
            !expr_eval_integer_constant(item->expr, &value) || value != 0) {
            return false;
        }
    }
    return true;
}

static bool expression_is_single_identifier_compound(Expr* expression,
                                                      const char* name,
                                                      Type* type) {
    ExprList* item;
    if (!expression || expression->kind != EXPR_COMPOUND ||
        !expression->compound_type ||
        !type_is_compatible(expression->compound_type, type)) {
        return false;
    }
    item = expression->compound_init;
    return item && !item->next &&
           item->designator_kind == INIT_DESIGNATOR_NONE &&
           move_expression_is_identifier(item->expr, name);
}

/* Prove the zero-argument close helper called by move assignment.  Its exact
 * observable form is the SDK sequence:
 *
 *   if (field == invalid) return Result{};
 *   Code result = cleanup_function((optional-cast)field);
 *   if (result == success) field = invalid;
 *   return Result{result};
 *
 * The result value is ignored by operator=, but constraining both returns and
 * the success branch prevents a seemingly harmless helper name from hiding
 * arbitrary side effects. */
static void register_inline_class_closes(CxxClass* cls) {
    struct CxxMember* member;
    TypeMethod** tail;
    TypeField* field;
    if (!cls || !cls->type || !cls->type->is_complete ||
        !cls->type->cleanup_function || !cls->type->cleanup_field) {
        return;
    }
    field = cls->type->cleanup_field;
    tail = &cls->type->methods;
    while (*tail) tail = &(*tail)->next;
    for (member = cls->members; member; member = member->next) {
        CxxMethod* method = member->method;
        StmtList* statements;
        Stmt* empty_guard;
        Stmt* call_declaration;
        Stmt* success_guard;
        Stmt* final_return;
        Stmt* action;
        Decl* result;
        Expr* call;
        Expr* argument;
        ExprList* arguments;
        Type* return_type;
        TypeField* result_field;
        int64_t success;
        int64_t assigned;
        TypeMethod* lowered;
        if (!method || !cxx_method_source_name(method) ||
            member->access != ACCESS_PUBLIC || method->is_static ||
            method->is_virtual || method->is_pure_virtual ||
            method->is_deleted || method->is_defaulted ||
            method->is_constructor || method->is_destructor ||
            method->is_const || !method->decl->type ||
            method->decl->type->kind != TYPE_FUNC ||
            method->decl->func_params ||
            !method->decl->func_body ||
            method->decl->func_body->kind != STMT_BLOCK) {
            continue;
        }
        return_type = method->decl->type->ret_type;
        if (!return_type || return_type->is_reference ||
            (return_type->kind != TYPE_STRUCT &&
             return_type->kind != TYPE_UNION) ||
            return_type->cleanup_function) {
            continue;
        }
        result_field = return_type->fields;
        if (!result_field || result_field->next || !result_field->type ||
            result_field->type->size <= 0 || result_field->type->size > 4 ||
            !(type_is_integer(result_field->type) ||
              result_field->type->kind == TYPE_ENUM)) {
            continue;
        }
        statements = method->decl->func_body->block_stmts;
        if (!statements || !statements->next ||
            !statements->next->next || !statements->next->next->next ||
            statements->next->next->next->next) {
            continue;
        }
        empty_guard = statements->stmt;
        call_declaration = statements->next->stmt;
        success_guard = statements->next->next->stmt;
        final_return = statements->next->next->next->stmt;
        if (!empty_guard || empty_guard->kind != STMT_IF ||
            empty_guard->if_else ||
            !expression_is_field_constant(empty_guard->if_cond, EXPR_EQ,
                                          field->name,
                                          cls->type->cleanup_invalid)) {
            continue;
        }
        action = cleanup_single_statement(empty_guard->if_then);
        if (!action || action->kind != STMT_RETURN ||
            !expression_is_empty_compound(action->return_val, return_type)) {
            continue;
        }
        if (!call_declaration || call_declaration->kind != STMT_DECL ||
            !call_declaration->decl ||
            call_declaration->decl->kind != DECL_VAR ||
            !call_declaration->decl->name ||
            !call_declaration->decl->var_init) {
            continue;
        }
        result = call_declaration->decl;
        if (!result->type ||
            !type_is_compatible(result->type, result_field->type)) {
            continue;
        }
        call = result->var_init;
        if (call->kind != EXPR_CALL || !call->call_func ||
            call->call_func->kind != EXPR_IDENT ||
            !call->call_func->ident_name ||
            strcmp(call->call_func->ident_name,
                   cls->type->cleanup_function) != 0) {
            continue;
        }
        arguments = call->call_args;
        if (!arguments || arguments->next || !arguments->expr) continue;
        argument = arguments->expr;
        if (argument->kind == EXPR_CAST) argument = argument->cast_expr;
        if (!move_expression_is_identifier(argument, field->name)) continue;
        if (!success_guard || success_guard->kind != STMT_IF ||
            success_guard->if_else || !success_guard->if_cond ||
            success_guard->if_cond->kind != EXPR_EQ) {
            continue;
        }
        if (move_expression_is_identifier(
                success_guard->if_cond->binary_lhs, result->name) &&
            expr_eval_integer_constant(
                success_guard->if_cond->binary_rhs, &success)) {
            /* matched */
        } else if (move_expression_is_identifier(
                       success_guard->if_cond->binary_rhs, result->name) &&
                   expr_eval_integer_constant(
                       success_guard->if_cond->binary_lhs, &success)) {
            /* matched */
        } else {
            continue;
        }
        action = cleanup_single_statement(success_guard->if_then);
        if (!action || action->kind != STMT_EXPR || !action->expr ||
            action->expr->kind != EXPR_ASSIGN ||
            !move_expression_is_identifier(action->expr->binary_lhs,
                                           field->name) ||
            !expr_eval_integer_constant(action->expr->binary_rhs, &assigned) ||
            assigned != cls->type->cleanup_invalid) {
            continue;
        }
        if (!final_return || final_return->kind != STMT_RETURN ||
            !expression_is_single_identifier_compound(final_return->return_val,
                                                      result->name,
                                                      return_type)) {
            continue;
        }
        lowered = ast_arena_alloc(sizeof(*lowered));
        lowered->name = cxx_method_source_name(method);
        lowered->return_type = return_type;
        lowered->field = field;
        lowered->function_decl = NULL;
        lowered->source_decl = method->decl;
        lowered->kind = TYPE_METHOD_FIELD_CLOSE;
        lowered->constant = cls->type->cleanup_invalid;
        lowered->cleanup_function = cls->type->cleanup_function;
        lowered->result_field = result_field;
        lowered->success_constant = success;
        lowered->cxx_access = (unsigned char)member->access;
        lowered->this_owner = NULL;
        lowered->this_adjustment = 0;
        lowered->next = NULL;
        *tail = lowered;
        tail = &lowered->next;
    }
}

static TypeMethod* class_close_method(CxxClass* cls, const char* name,
                                      TypeField* field) {
    TypeMethod* method;
    for (method = cls && cls->type ? cls->type->methods : NULL;
         method; method = method->next) {
        if (method->kind == TYPE_METHOD_FIELD_CLOSE &&
            method->cxx_access == ACCESS_PUBLIC && method->name && name &&
            strcmp(method->name, name) == 0 && method->field == field &&
            method->cleanup_function && method->result_field) {
            return method;
        }
    }
    return NULL;
}

/* Accept a public close alias only when its complete body is exactly:
 *
 *   return validated_zero_argument_close();
 *
 * The target close has already proved the handle, cleanup function, result
 * layout, success value, and retry semantics.  Cloning that metadata gives
 * SDK wrappers a conventional reset() spelling without interpreting an
 * arbitrary member body or allowing hidden side effects. */
static void register_inline_class_close_delegates(CxxClass* cls) {
    struct CxxMember* member;
    TypeMethod** tail;
    TypeField* field;
    if (!cls || !cls->type || !cls->type->is_complete ||
        !cls->type->cleanup_field) {
        return;
    }
    field = cls->type->cleanup_field;
    tail = &cls->type->methods;
    while (*tail) tail = &(*tail)->next;
    for (member = cls->members; member; member = member->next) {
        CxxMethod* method = member->method;
        StmtList* statements;
        Expr* returned;
        Expr* callee;
        TypeMethod* target;
        TypeMethod* lowered;
        if (!method || !cxx_method_source_name(method) ||
            member->access != ACCESS_PUBLIC || method->is_static ||
            method->is_virtual || method->is_pure_virtual ||
            method->is_deleted || method->is_defaulted ||
            method->is_constructor || method->is_destructor ||
            method->is_const || !method->decl->type ||
            method->decl->type->kind != TYPE_FUNC ||
            method->decl->func_params || !method->decl->func_body ||
            method->decl->func_body->kind != STMT_BLOCK) {
            continue;
        }
        statements = method->decl->func_body->block_stmts;
        if (!statements || statements->next || !statements->stmt ||
            statements->stmt->kind != STMT_RETURN ||
            !statements->stmt->return_val) {
            continue;
        }
        returned = statements->stmt->return_val;
        if (returned->kind != EXPR_CALL || returned->call_args ||
            !returned->call_func ||
            returned->call_func->kind != EXPR_IDENT) {
            continue;
        }
        callee = returned->call_func;
        if (!callee->ident_name ||
            strcmp(callee->ident_name, cxx_method_source_name(method)) == 0) {
            continue;
        }
        target = class_close_method(cls, callee->ident_name, field);
        if (!target || !target->return_type ||
            !type_is_compatible(method->decl->type->ret_type,
                                target->return_type)) {
            continue;
        }
        lowered = ast_arena_alloc(sizeof(*lowered));
        *lowered = *target;
        lowered->name = cxx_method_source_name(method);
        lowered->return_type = method->decl->type->ret_type;
        lowered->cxx_access = (unsigned char)member->access;
        lowered->source_decl = method->decl;
        lowered->this_owner = target->this_owner;
        lowered->this_adjustment = target->this_adjustment;
        lowered->next = NULL;
        *tail = lowered;
        tail = &lowered->next;
    }
}

/* Accept only the SDK ownership assignment:
 *
 *   Class& operator=(Class&& other) {
 *     if (this != &other) { (void)close(); field = other.release(); }
 *     return *this;
 *   }
 *
 * close(), release(), the destructor cleanup, and the move constructor have
 * all independently passed structural verification before this metadata is
 * published to semantic analysis. */
static void register_inline_class_move_assignment(CxxClass* cls) {
    struct CxxMember* member;
    TypeMethod* candidate = NULL;
    TypeField* field;
    if (!cls || !cls->type || !cls->type->is_complete ||
        cls->base_count != 0 || cls->has_static_field ||
        cls->has_field_initializer || class_has_virtual_member(cls) ||
        !cls->type->cleanup_function || !cls->type->cleanup_field ||
        !cls->type->move_constructor_method) {
        return;
    }
    field = cls->type->fields;
    if (!field || field->next || cls->type->cleanup_field != field ||
        cls->type->move_constructor_method->field != field) {
        return;
    }
    for (member = cls->members; member; member = member->next) {
        CxxMethod* method = member->method;
        TypeParam* parameter;
        StmtList* statements;
        Stmt* guarded;
        Stmt* returned;
        StmtList* actions;
        Expr* condition;
        Expr* close_call;
        Expr* assignment;
        Expr* release_call;
        Expr* release_callee;
        TypeMethod* close;
        TypeMethod* release;
        const char* close_name;
        if (!method || !cxx_method_source_name(method) ||
            strcmp(cxx_method_source_name(method), "operator=") != 0 ||
            member->access != ACCESS_PUBLIC || method->is_static ||
            method->is_virtual || method->is_pure_virtual ||
            method->is_deleted || method->is_defaulted ||
            method->is_constructor || method->is_destructor ||
            method->is_const || !method->decl->type ||
            !class_reference_is_self(cls, method->decl->type->ret_type,
                                     false) ||
            !method->decl->func_params ||
            method->decl->func_params->next ||
            !method->decl->func_params->decl ||
            !move_parameter_is_self(
                cls, method->decl->func_params->decl->type) ||
            !method->decl->func_body ||
            method->decl->func_body->kind != STMT_BLOCK) {
            continue;
        }
        parameter = method->decl->type->params;
        if (!parameter || parameter->next || !parameter->name) continue;
        statements = method->decl->func_body->block_stmts;
        if (!statements || !statements->next || statements->next->next) {
            continue;
        }
        guarded = statements->stmt;
        returned = statements->next->stmt;
        if (!guarded || guarded->kind != STMT_IF || guarded->if_else ||
            !returned || returned->kind != STMT_RETURN ||
            !returned->return_val ||
            returned->return_val->kind != EXPR_DEREF ||
            !move_expression_is_identifier(
                returned->return_val->unary_operand, "this")) {
            continue;
        }
        condition = guarded->if_cond;
        if (!condition || condition->kind != EXPR_NE ||
            !move_expression_is_identifier(condition->binary_lhs, "this") ||
            !condition->binary_rhs ||
            condition->binary_rhs->kind != EXPR_ADDR ||
            !move_expression_is_identifier(
                condition->binary_rhs->unary_operand, parameter->name)) {
            continue;
        }
        if (!guarded->if_then || guarded->if_then->kind != STMT_BLOCK) {
            continue;
        }
        actions = guarded->if_then->block_stmts;
        if (!actions || !actions->next || actions->next->next ||
            !actions->stmt || actions->stmt->kind != STMT_EXPR ||
            !actions->next->stmt ||
            actions->next->stmt->kind != STMT_EXPR) {
            continue;
        }
        close_call = cleanup_unwrap_void_cast(actions->stmt->expr);
        if (!close_call || close_call->kind != EXPR_CALL ||
            close_call->call_args || !close_call->call_func ||
            close_call->call_func->kind != EXPR_IDENT ||
            !close_call->call_func->ident_name) {
            continue;
        }
        close_name = close_call->call_func->ident_name;
        close = class_close_method(cls, close_name, field);
        if (!close) continue;
        assignment = actions->next->stmt->expr;
        if (!assignment || assignment->kind != EXPR_ASSIGN ||
            !move_expression_is_identifier(assignment->binary_lhs,
                                           field->name)) {
            continue;
        }
        release_call = assignment->binary_rhs;
        if (!release_call || release_call->kind != EXPR_CALL ||
            release_call->call_args || !release_call->call_func ||
            release_call->call_func->kind != EXPR_MEMBER) {
            continue;
        }
        release_callee = release_call->call_func;
        if (!release_callee->member_name || !release_callee->member_base ||
            !move_expression_is_identifier(release_callee->member_base,
                                           parameter->name)) {
            continue;
        }
        release = class_release_method(cls, release_callee->member_name,
                                       field);
        if (!release || release != cls->type->move_constructor_method) {
            continue;
        }
        if (candidate) {
            cls->type->move_assignment_method = NULL;
            return;
        }
        candidate = release;
    }
    cls->type->move_assignment_method = candidate;
}

/* Build the source lookup spelling for a class member.  Qualified expressions
 * are retained as one identifier, so a static member needs a symbol-table key
 * such as api::Counter::add while its link name remains the Itanium spelling
 * produced by cxx_mangle_function(). */
static const char* cxx_class_method_source_name(CxxClass* cls,
                                                 const char* method_name,
                                                 SourceLoc loc) {
    CxxNamespace* stack[32];
    int count = 0;
    size_t length = 0u;
    char buffer[512] = "";

    if (!cls || !cls->name || !method_name) return rcc_intern("");
    for (CxxNamespace* ns = active_namespace;
         ns && ns->name;
         ns = ns->parent) {
        if (count == (int)(sizeof(stack) / sizeof(stack[0]))) {
            rcc_error(loc, "namespace nesting exceeds compiler limit");
            break;
        }
        stack[count++] = ns;
    }
    for (int index = count - 1; index >= 0; --index) {
        size_t part_length = strlen(stack[index]->name);
        if (part_length > sizeof(buffer) - 1u - length) {
            rcc_error(loc, "qualified member name exceeds compiler limit");
            return rcc_intern(buffer);
        }
        memcpy(buffer + length, stack[index]->name, part_length);
        length += part_length;
        if (length > sizeof(buffer) - 3u) {
            rcc_error(loc, "qualified member name exceeds compiler limit");
            return rcc_intern(buffer);
        }
        memcpy(buffer + length, "::", 2u);
        length += 2u;
    }
    if (strlen(cls->name) > sizeof(buffer) - 3u - length ||
        strlen(method_name) > sizeof(buffer) - 1u - length -
            strlen(cls->name) - 2u) {
        rcc_error(loc, "qualified member name exceeds compiler limit");
        return rcc_intern(buffer);
    }
    memcpy(buffer + length, cls->name, strlen(cls->name));
    length += strlen(cls->name);
    memcpy(buffer + length, "::", 2u);
    length += 2u;
    strcpy(buffer + length, method_name);
    return rcc_intern(buffer);
}

static const char* cxx_method_source_name(CxxMethod* method) {
    return method && method->source_name
        ? method->source_name
        : (method && method->decl ? method->decl->name : NULL);
}

static bool class_declares_method_name(CxxClass* cls, const char* name) {
    struct CxxMember* member;
    if (!cls || !name) return false;
    for (member = cls->members; member; member = member->next) {
        if (member->method && cxx_method_source_name(member->method) &&
            strcmp(cxx_method_source_name(member->method), name) == 0) {
            return true;
        }
    }
    return false;
}

static bool class_uses_base_member(CxxClass* cls, CxxClass* base,
                                   const char* name) {
    if (!cls || !base || !name) return false;
    for (int index = 0; index < cls->using_base_member_count; ++index) {
        const char* base_name = cls->using_base_members[index].base_name;
        const char* member_name = cls->using_base_members[index].member_name;
        const char* suffix;
        if (!base_name || !member_name || strcmp(member_name, name) != 0) {
            continue;
        }
        if (strcmp(base_name, base->name) == 0) return true;
        suffix = strstr(base_name, "::");
        while (suffix) {
            suffix += 2;
            if (strcmp(suffix, base->name) == 0) return true;
            suffix = strstr(suffix, "::");
        }
    }
    return false;
}

static bool class_has_method_declaration(CxxClass* cls, Decl* declaration) {
    if (!cls || !declaration) return false;
    for (TypeMethod* method = cls->type ? cls->type->methods : NULL;
         method; method = method->next) {
        if (method->function_decl == declaration) return true;
    }
    return false;
}

static bool class_method_declares_shared_virtual_base(
    CxxClass* cls, Decl* declaration) {
    CxxClass* owner;
    if (!cls || !declaration || !declaration->func_method_owner) return false;
    owner = declaration->func_method_owner->cxx_class;
    if (!owner) return false;
    for (int index = 0; index < cls->virtual_base_count; ++index) {
        if (cls->virtual_bases[index].base == owner) return true;
    }
    return false;
}

/* Publish methods of accessible bases on the derived class.  The layout pass
 * records the concrete offset used by this backend, so the alias can carry
 * the real base declaration and an explicit byte adjustment. */
static void register_inherited_class_methods(CxxClass* cls,
                                             TypeMethod*** tail) {
    if (!cls || !tail || !*tail || !cls->base_offsets) {
        return;
    }
    for (int base_index = 0; base_index < cls->base_count; ++base_index) {
        CxxClass* base = cls->bases[base_index].base;
        TypeMethod* method;
        if (cls->bases[base_index].access == ACCESS_PRIVATE ||
            !base || !base->type || !base->type->is_complete ||
            cls->base_offsets[base_index] < 0) {
            continue;
        }
        for (method = base->type->methods; method; method = method->next) {
            TypeMethod* inherited;
            if (method->kind != TYPE_METHOD_FUNCTION || !method->name ||
                !method->function_decl ||
                (class_has_method_declaration(cls, method->function_decl) &&
                 class_method_declares_shared_virtual_base(
                     cls, method->function_decl)) ||
                (class_declares_method_name(cls, method->name) &&
                 !class_uses_base_member(cls, base, method->name))) {
                continue;
            }
            inherited = ast_arena_alloc(sizeof(*inherited));
            *inherited = *method;
            if (cls->bases[base_index].access == ACCESS_PROTECTED &&
                inherited->cxx_access == ACCESS_PUBLIC) {
                inherited->cxx_access = ACCESS_PROTECTED;
            }
            if (method->this_owner) {
                int this_adjustment = cls->base_offsets[base_index];
                CxxClass* owner_class = method->this_owner->cxx_class;
                if (owner_class && cxx_class_virtual_base_offset(
                        cls, owner_class, &this_adjustment)) {
                    /* A method inherited through a virtual-base path must
                     * use the one shared subobject in the complete object;
                     * the intermediate branch offset is not sufficient. */
                } else {
                    this_adjustment = cls->base_offsets[base_index];
                    this_adjustment += method->this_adjustment;
                }
                inherited->this_owner = method->this_owner;
                inherited->this_adjustment = this_adjustment;
            } else if (method->function_decl->func_this_param) {
                int this_adjustment = cls->base_offsets[base_index];
                if (cls->bases[base_index].is_virtual) {
                    cxx_class_virtual_base_offset(cls, base, &this_adjustment);
                }
                inherited->this_owner = base->type;
                inherited->this_adjustment = this_adjustment;
            }
            inherited->next = NULL;
            **tail = inherited;
            *tail = &inherited->next;
        }
    }
}

static CxxConstructorInfo* constructor_info_for_method(CxxClass* cls,
                                                        CxxMethod* method) {
    for (CxxConstructorInfo* constructor = cls ? cls->constructors : NULL;
         constructor; constructor = constructor->next) {
        if (constructor->method == method) return constructor;
    }
    return NULL;
}

/* Publish ordinary non-virtual member definitions and static member
 * definitions as real functions.  Ordinary members receive the implicit
 * object parameter; static members deliberately do not, and use the normal C
 * call ABI after their qualified source lookup is resolved. */
static void register_ordinary_class_methods(CxxClass* cls) {
    struct CxxMember* member;
    TypeMethod** tail;
    if (!cls || !cls->type || !cls->type->is_complete || !active_ast ||
        active_template) return;

    tail = &cls->type->methods;
    while (*tail) tail = &(*tail)->next;
    register_inherited_class_methods(cls, &tail);
    for (member = cls->members; member; member = member->next) {
        CxxMethod* method = member->method;
        CxxConstructorInfo* constructor;
        Decl* declaration;
        TypeParam* this_type_parameter;
        Type* this_type;
        Type* const_owner;
        Decl* this_parameter;
        TypeMethod* lowered;
        const char* source_name;
        const char* link_name;

        constructor = method && method->is_constructor
            ? constructor_info_for_method(cls, method) : NULL;
        if (!method || !method->decl || !method->decl->func_body ||
            method->is_pure_virtual || method->is_deleted ||
            method->is_defaulted ||
            (method->is_constructor &&
             (!constructor || constructor->body_is_empty))) {
            continue;
        }

        declaration = method->decl;
        source_name = cxx_method_source_name(method);
        if (!method->is_destructor && source_name &&
            strcmp(source_name, "operator=") == 0 &&
            cls->type->move_assignment_method) {
            /* Validated field/ownership lowerings are complete executable
             * implementations.  Do not also publish their unvalidated body;
             * duplicate registration would make template instances route
             * through a call with no ABI function type. */
            continue;
        }
        link_name = rcc_intern(cxx_mangle_function(
            declaration, active_namespace, cls));

        /* Ordinary member names are not source-visible global symbols, so the
         * ABI spelling is also their semantic key.  Static members are
         * source-visible through Class::name and use that qualified key. */
        if (method->is_static) {
            declaration->name = cxx_class_method_source_name(
                cls, source_name, declaration->loc);
        } else {
            declaration->name = link_name;
        }
        declaration->link_name = link_name;
        declaration->func_has_cxx_linkage = true;
        declaration->func_is_cxx_method = true;
        declaration->func_method_owner = cls->type;

        this_type = NULL;
        if (!method->is_static && method->is_const) {
            const_owner = ast_arena_alloc(sizeof(*const_owner));
            *const_owner = *cls->type;
            const_owner->is_const = true;
            this_type = type_ptr(const_owner);
        } else if (!method->is_static) {
            this_type = type_ptr(cls->type);
        }
        if (!method->is_static) {
            this_parameter = decl_param("this", this_type, -1,
                                        declaration->loc);
            declaration->func_this_param = this_parameter;

            this_type_parameter = ast_arena_alloc(sizeof(*this_type_parameter));
            this_type_parameter->name = "this";
            this_type_parameter->type = this_type;
            this_type_parameter->is_bitfield = false;
            this_type_parameter->bit_width = 0u;
            this_type_parameter->is_static = false;
            this_type_parameter->cxx_access = ACCESS_PUBLIC;
            this_type_parameter->next = declaration->type->params;
            declaration->type->params = this_type_parameter;
        }

        lowered = ast_arena_alloc(sizeof(*lowered));
        lowered->name = source_name;
        lowered->return_type = declaration->type->ret_type;
        lowered->field = NULL;
        lowered->function_decl = declaration;
        lowered->kind = TYPE_METHOD_FUNCTION;
        lowered->constant = 0;
        lowered->cleanup_function = NULL;
        lowered->result_field = NULL;
        lowered->success_constant = 0;
        lowered->cxx_access = (unsigned char)member->access;
        lowered->is_explicit = method->is_explicit;
        lowered->this_owner = method->is_static ? NULL : cls->type;
        lowered->this_adjustment = 0;
        /* A method can override a secondary base slot without occupying a
         * slot in the class's primary table.  Calls through the complete
         * derived type can use the real body directly; calls through the
         * secondary base use that base's emitted adjusting thunk. */
        lowered->is_virtual = method->is_virtual &&
                              method->vtable_index >= 0 &&
                              cls->type->cxx_vtable_symbol != NULL;
        lowered->vtable_index = method->vtable_index;
        lowered->vtable_symbol = lowered->is_virtual
            ? cls->type->cxx_vtable_symbol : NULL;
        lowered->next = NULL;
        *tail = lowered;
        tail = &lowered->next;

        ast_add_decl(active_ast, declaration);
    }
}

static void diagnose_unlowered_destructors(CxxClass* cls) {
    struct CxxMember* member;
    if (!cls || !cls->type || cls->type->cleanup_function ||
        cls->destructor_method || active_template) {
        return;
    }
    for (member = cls->members; member; member = member->next) {
        CxxMethod* method = member->method;
        if (method && method->is_destructor && method->decl &&
            method->decl->func_body &&
            method->decl->func_body->block_stmts) {
            rcc_error(method->decl->loc,
                      "non-trivial C++ destructor body cannot be lowered "
                      "without object-lifetime support");
        }
    }
}

/* Publish in-class static data members as real global declarations.  They
 * retain a qualified source lookup name while their link name follows the
 * Itanium data-symbol spelling.  Static data members do not use the C
 * STORAGE_STATIC linkage rule: the `static` keyword belongs to the class
 * member, not to translation-unit visibility. */
static void register_class_static_fields(CxxClass* cls) {
    struct CxxMember* member;

    if (!cls || !active_ast || active_template) return;
    for (member = cls->members; member; member = member->next) {
        Decl* declaration = member->decl;
        const char* source_name;

        if (!member->is_static || member->method || !declaration ||
            declaration->kind != DECL_VAR || !declaration->name) {
            continue;
        }
        source_name = declaration->name;
        declaration->link_name = rcc_intern(cxx_mangle_name(
            source_name, active_namespace, cls));
        declaration->name = cxx_class_method_source_name(
            cls, source_name, declaration->loc);
        declaration->storage = STORAGE_NONE;
        ast_add_decl(active_ast, declaration);
    }
}

static const char* cxx_instance_static_source_name(
    CxxClass* cls, const char* field_name, SourceLoc loc) {
    char buffer[512];
    int written;
    if (!cls || !cls->name || !field_name) return rcc_intern("");
    written = snprintf(buffer, sizeof(buffer), "%s::%s",
                       cls->name, field_name);
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        rcc_error(loc, "instantiated static member name exceeds compiler limit");
        return rcc_intern("");
    }
    return rcc_intern(buffer);
}

static void cxx_set_template_identity(
    CxxClass* cls, CxxTemplate* tmpl, Type** arguments,
    const int64_t* value_args, const bool* value_present, int argument_count) {
    bool expanded_pack = tmpl && tmpl->param_count == 1 &&
        tmpl->params[0].is_pack;
    if (!cls || !tmpl ||
        ((!expanded_pack && argument_count != tmpl->param_count) ||
         (argument_count > 0 && !arguments))) {
        return;
    }
    cls->template_identity_tmpl = tmpl;
    cls->template_identity_arg_count = argument_count;
    if (argument_count == 0) return;
    cls->template_identity_args = ast_arena_alloc(
        sizeof(Type*) * (size_t)argument_count);
    memcpy(cls->template_identity_args, arguments,
           sizeof(Type*) * (size_t)argument_count);
    if (value_args && value_present) {
        cls->template_identity_value_args = ast_arena_alloc(
            sizeof(int64_t) * (size_t)argument_count);
        cls->template_identity_value_present = ast_arena_alloc(
            sizeof(bool) * (size_t)argument_count);
        memcpy(cls->template_identity_value_args, value_args,
               sizeof(int64_t) * (size_t)argument_count);
        memcpy(cls->template_identity_value_present, value_present,
               sizeof(bool) * (size_t)argument_count);
    }
}

/* Static data members are declarations owned by the class template
 * definition, not methods, so the ordinary class publication pass skips
 * them while the template is still dependent.  Materialize one substituted
 * declaration for each cached class instance before its member bodies are
 * re-analyzed. */
static void register_instantiated_class_static_fields(
    CxxClass* instance, CxxClass* definition) {
    if (!instance || !definition || !active_ast) return;
    for (TypeParam* field = instance->fields; field; field = field->next) {
        Decl* source = NULL;
        Decl* declaration;
        if (!field->is_static || !field->name || !field->type) continue;
        for (struct CxxMember* member = definition->members;
             member; member = member->next) {
            const char* name;
            if (!member->is_static || member->method || !member->decl) continue;
            name = member->decl->name;
            name = name ? strrchr(name, ':') : NULL;
            name = name ? name + 1 : member->decl->name;
            if (name && strcmp(name, field->name) == 0) {
                source = member->decl;
                break;
            }
        }
        declaration = decl_var(
            cxx_instance_static_source_name(instance, field->name,
                                            source ? source->loc : (SourceLoc){"<template>", 0, 0}),
            field->type, field->initializer,
            source ? source->loc : (SourceLoc){"<template>", 0, 0});
        declaration->link_name = rcc_intern(cxx_mangle_name(
            field->name, instance->ns, instance));
        declaration->var_is_thread_local = source &&
                                           source->var_is_thread_local;
        declaration->var_is_inline = source && source->var_is_inline;
        declaration->var_is_constexpr = source && source->var_is_constexpr;
        declaration->storage = STORAGE_NONE;
        cxx_class_add_member(instance, declaration,
                             (AccessSpec)field->cxx_access, true);
        ast_add_decl(active_ast, declaration);
    }
}

/* Parse class member (field or method) */
static void parse_class_member(CxxClass* cls, AccessSpec current_access) {
    SourceLoc loc = peek()->loc;

    skip_cxx_attributes();

    bool is_virtual = false;
    bool is_static = false;
    bool is_inline = false;
    bool is_constexpr = false;
    bool is_consteval = false;
    bool is_explicit = false;
    bool is_thread_local = false;

    /* C++ declaration specifiers can be combined in either order. */
    for (;;) {
        if (match(TOK_VIRTUAL)) is_virtual = true;
        else if (match(TOK_STATIC)) is_static = true;
        else if (match(TOK_THREAD_LOCAL)) is_thread_local = true;
        else if (match(TOK_CONSTEXPR)) is_constexpr = true;
        else if (match(TOK_CONSTEVAL)) {
            is_constexpr = true;
            is_consteval = true;
        }
        else if (match(TOK_EXPLICIT)) is_explicit = true;
        else if (match(TOK_INLINE) || match(TOK___INLINE__)) is_inline = true;
        else if (match(TOK_FRIEND) || match(TOK_MUTABLE)) { }
        else break;
    }

    /* Check for destructor */
    bool is_destructor = match(TOK_TILDE);
    bool is_constructor = false;

    /* Parse type (or constructor) */
    Type* type = NULL;
    const char* name = NULL;

    if (is_destructor) {
        /* Destructor: ~ClassName() */
        expect(TOK_IDENT, "class name");
        name = rcc_intern("~dtor");
        type = type_void;
    } else if (check(TOK_IDENT) && check_next(TOK_LPAREN) &&
               strcmp(peek()->value.str_val, cls->name) == 0) {
        /* Constructor */
        name = advance()->value.str_val;
        type = type_void;  /* Constructors have no return type */
        is_constructor = true;
    } else if (match(TOK_OPERATOR)) {
        /* Conversion function: operator bool(), operator T*(), ... */
        type = parse_cxx_type_spec();
        name = rcc_intern("operator conversion");
    } else {
        type = parse_cxx_type_spec();
        if (check(TOK_IDENT)) {
            name = advance()->value.str_val;
        } else if (match(TOK_OPERATOR)) {
            name = parse_operator_name();
        }
    }

    if (!name && !check(TOK_COLON)) {
        rcc_error(loc, "expected member name");
        return;
    }

    /* Is this a method or a field? */
    if (match(TOK_LPAREN)) {
        /* Method */
        DeclList* params = NULL;
        int param_idx = 0;
        bool saw_default = false;
        ParsedConstructorInitializer constructor_initializer = {0};
        Expr* noexcept_expr = NULL;

        if (!check(TOK_RPAREN)) {
            if (check(TOK_VOID) && parser.cur->next && parser.cur->next->type == TOK_RPAREN) {
                advance();
            } else {
                do {
                    const char* pname = NULL;
                    Type* ptype = parse_cxx_type_spec();
                    ptype = rcc_parser_parse_cxx_declarator(ptype, &pname,
                                                            NULL);
                    Expr* default_argument = NULL;
                    if (match(TOK_ASSIGN)) {
                        default_argument = parse_assignment_expression();
                        saw_default = true;
                    } else if (saw_default) {
                        rcc_error(peek()->loc,
                                  "parameter without a default follows a default argument");
                    }
                    Decl* p = decl_param(pname, ptype, param_idx++, peek()->loc);
                    p->param_default = default_argument;
                    decllist_append(&params, p);
                } while (match(TOK_COMMA));
            }
        }
        expect(TOK_RPAREN, ")");

        bool is_const = false;
        bool is_override = false;
        bool is_final = false;
        bool is_noexcept = false;
        for (;;) {
            if (match(TOK_CONST)) is_const = true;
            else if (match(TOK_VOLATILE)) { }
            else if (match(TOK_AMP) || match(TOK_AND)) { }
            else if (match(TOK_OVERRIDE)) is_override = true;
            else if (match(TOK_FINAL)) is_final = true;
            else if (match(TOK_NOEXCEPT)) {
                if (check(TOK_LPAREN)) {
                    advance();
                    noexcept_expr = parse_expression();
                    expect(TOK_RPAREN, ")");
                } else {
                    is_noexcept = true;
                }
            } else break;
        }

        /* Pure virtual and explicitly defaulted/deleted functions. */
        bool is_pure = false;
        bool is_deleted = false;
        bool is_defaulted = false;
        if (match(TOK_ASSIGN)) {
            if (check(TOK_INT_LIT) && peek()->value.int_val == 0) {
                advance();
                is_pure = true;
            } else if (match(TOK_DELETE)) {
                is_deleted = true;
            } else if (match(TOK_DEFAULT)) {
                is_defaulted = true;
            } else {
                rcc_error(peek()->loc,
                          "expected 0, delete, or default after '='");
            }
        }

        if (is_constructor) {
            constructor_initializer = parse_ctor_initializer();
        }

        /* Method body or declaration */
        Stmt* body = NULL;
        if (match(TOK_LBRACE)) {
            /* Parse method body */
            StmtList* stmts = NULL;
            rcc_parser_cxx_begin_function_parameters(params);
            if (!is_static) {
                Type* this_type = type_ptr(cls->type);
                if (is_const) {
                    Type* const_owner = ast_arena_alloc(sizeof(*const_owner));
                    *const_owner = *cls->type;
                    const_owner->is_const = true;
                    this_type = type_ptr(const_owner);
                }
                rcc_parser_cxx_add_value_binding("this", this_type);
            }
            while (!check(TOK_RBRACE) && !at_end()) {
                Token* statement_start = parser.cur;
                int errors_before = g_error_count;
                Stmt* s = parse_cxx_statement();
                if (s) stmtlist_append(&stmts, s);
                if (g_error_count > errors_before) {
                    while (!at_end() && !check(TOK_SEMICOLON) &&
                           !check(TOK_RBRACE)) {
                        advance();
                    }
                    if (check(TOK_SEMICOLON)) advance();
                } else if (parser.cur == statement_start && !at_end()) {
                    advance();
                }
            }
            expect(TOK_RBRACE, "}");
            rcc_parser_cxx_end_function_parameters();
            body = stmt_block(stmts, loc);
        } else {
            expect(TOK_SEMICOLON, ";");
        }

        /* Create method */
        CxxMethod* method = cxx_method_new(name, type, params, body, loc);
        method->access = current_access;
        method->is_virtual = is_virtual;
        method->is_static = is_static;
        method->is_constexpr = is_constexpr;
        method->decl->func_is_constexpr = is_constexpr;
        method->decl->func_is_consteval = is_consteval;
        method->is_explicit = is_explicit;
        method->is_const = is_const;
        method->is_override = is_override;
        method->is_final = is_final;
        method->is_noexcept = is_noexcept;
        method->decl->func_is_noexcept = is_noexcept;
        method->decl->func_noexcept_expr = noexcept_expr;
        method->is_pure_virtual = is_pure;
        method->is_deleted = is_deleted;
        method->is_defaulted = is_defaulted;
        method->is_constructor = is_constructor;
        method->is_destructor = is_destructor;
        method->decl->func_is_cxx_constructor = is_constructor;
        method->decl->func_is_cxx_destructor = is_destructor;
        /* A function defined inside a class definition is implicitly inline
         * in C++, even without the `inline` keyword.  Preserve that linkage
         * property so identical in-class definitions from separate
         * translation units are weak/ODR definitions rather than strong
         * duplicate symbols. */
        method->decl->func_is_inline = body != NULL;
        method->owner = cls;

        if (is_constructor) cls->has_user_constructor = true;

        if (is_constructor) {
            CxxConstructorInfo* info = ast_arena_alloc(sizeof(*info));
            CxxConstructorInfo** tail = &cls->constructors;
            info->method = method;
            info->parameter_count = param_idx;
            info->parameters = method->decl->type->params;
            info->initializers = constructor_initializer.items;
            info->initializer_count = constructor_initializer.count;
            info->initializers_are_supported =
                constructor_initializer.count == 0 ||
                constructor_initializer.is_supported;
            info->body_is_empty = body && body->kind == STMT_BLOCK &&
                                  body->block_stmts == NULL;
            info->is_deleted = is_deleted;
            info->is_defaulted = is_defaulted;
            info->access = current_access;
            info->next = NULL;
            if (!info->body_is_empty) {
                (void)lowerable_constructor_body(cls, info);
            }
            while (*tail) tail = &(*tail)->next;
            *tail = info;
        }

        cxx_class_add_method(cls, method);
    } else {
        /* Field */
        bool is_bitfield = false;
        unsigned bit_width = 0u;
        /* Array suffix? */
        if (match(TOK_LBRACKET)) {
            int len = -1;
            Expr* bound_expression = NULL;
            if (check(TOK_INT_LIT)) {
                len = (int)advance()->value.int_val;
            } else if (!check(TOK_RBRACKET)) {
                int64_t constant;
                bound_expression = parse_assignment_expression();
                if (expr_eval_integer_constant(bound_expression, &constant)) {
                    if (constant <= 0 || constant > INT_MAX) {
                        rcc_error(bound_expression->loc,
                                  "class member array bound must be a positive "
                                  "representable integer constant");
                    } else {
                        len = (int)constant;
                    }
                    bound_expression = NULL;
                } else if (!bound_expression) {
                    rcc_error(loc, "class member array bound is invalid");
                } else {
                    len = -2;
                }
            }
            expect(TOK_RBRACKET, "]");
            type = type_array(type, len);
            type->array_bound = bound_expression;
        }

        if (match(TOK_COLON)) {
            Expr* width_expression = parse_assignment_expression();
            int64_t width_value = 0;
            is_bitfield = true;
            if (!type || (!type_is_integer(type) &&
                          type->kind != TYPE_ENUM)) {
                rcc_error(loc,
                          "C++ bit-field type must be an integer or enum type");
            } else if (type->size <= 0 || type->size > 4) {
                rcc_error(loc,
                          "C++ bit-field type width of %d bytes is not supported",
                          type->size);
            }
            if (!width_expression ||
                !expr_eval_integer_constant(width_expression, &width_value) ||
                width_value < 0 ||
                (type && type->size > 0 &&
                 (uint64_t)width_value > (uint64_t)type->size * 8u)) {
                rcc_error(width_expression ? width_expression->loc : loc,
                          "C++ bit-field width is not a valid storage-unit constant");
            } else {
                bit_width = (unsigned)width_value;
                if (bit_width == 0u && name) {
                    rcc_error(loc, "named C++ bit-field cannot have zero width");
                }
            }
        }

        /* Initializer? */
        Expr* init = NULL;
        if (match(TOK_ASSIGN)) {
            init = parse_cxx_expression();
        } else if (check(TOK_LBRACE)) {
            init = rcc_parser_parse_initializer();
        }
        if (init && !is_static) cls->has_field_initializer = true;
        if (is_thread_local && !is_static) {
            rcc_error(loc,
                      "thread-local storage is only valid on static C++ data members");
        }
        if (current_access != ACCESS_PUBLIC) {
            cls->has_nonpublic_field = true;
        }
        if (is_static) cls->has_static_field = true;

        expect(TOK_SEMICOLON, ";");

        /* Add field to class */
        cxx_class_add_field_initializer(cls, name, type, current_access, init,
                                         is_bitfield, bit_width, is_static);
        if (is_static && name) {
            Decl* declaration = decl_var(name, type, init, loc);
            declaration->var_is_thread_local = is_thread_local;
            declaration->var_is_inline = is_inline;
            cxx_class_add_member(cls, declaration, current_access, true);
        }
    }
}

/* Parse the body and ABI metadata of a class after its source name has
 * already been consumed.  Explicit template specializations use this same
 * path so their class body cannot be mistaken for a primary-template body. */
static CxxClass* parse_cxx_class_named(SourceLoc loc, bool is_struct,
                                       const char* class_name) {
    bool has_definition = false;

    /* A final class has the same object layout as an otherwise identical
     * class; the semantic restriction is enforced when bases are resolved. */
    match(TOK_FINAL);

    CxxClass* cls = cxx_class_new(class_name, loc);
    cls->is_struct = is_struct;

    /* Inheritance */
    if (match(TOK_COLON)) {
        do {
            AccessSpec inherit_access = ACCESS_PRIVATE;
            bool is_virtual = false;
            if (match(TOK_VIRTUAL)) is_virtual = true;
            if (match(TOK_PUBLIC)) inherit_access = ACCESS_PUBLIC;
            else if (match(TOK_PROTECTED)) inherit_access = ACCESS_PROTECTED;
            else if (match(TOK_PRIVATE)) inherit_access = ACCESS_PRIVATE;
            if (match(TOK_VIRTUAL)) is_virtual = true;

            const char* base_name = parse_qualified_name();
            cxx_class_add_base(cls, base_name, inherit_access);
            if (is_virtual) {
                cls->bases[cls->base_count - 1].is_virtual = true;
            }
        } while (match(TOK_COMMA));
    }

    /* Class body */
    if (match(TOK_LBRACE)) {
        CxxClass* enclosing_class = active_class;
        active_class = cls;
        has_definition = true;
        AccessSpec current_access = is_struct
            ? ACCESS_PUBLIC : ACCESS_PRIVATE;

        while (!check(TOK_RBRACE) && !at_end()) {
            /* Check for access specifier */
            AccessSpec new_access = parse_access_spec();
            if (new_access != (AccessSpec)-1) {
                current_access = new_access;
                continue;
            }

            if (match(TOK_USING)) {
                SourceLoc using_loc = previous()->loc;
                const char* qualified = parse_qualified_name();
                const char* separator = qualified
                    ? strrchr(qualified, ':') : NULL;
                if (!separator || separator == qualified ||
                    separator[-1] != ':') {
                    rcc_error(using_loc,
                              "class using-declaration must name a base member");
                } else {
                    size_t base_length = (size_t)(separator - qualified - 1);
                    char base_name[512];
                    if (base_length == 0 || base_length >= sizeof(base_name)) {
                        rcc_error(using_loc,
                                  "class using-declaration base name is too long");
                    } else {
                        memcpy(base_name, qualified, base_length);
                        base_name[base_length] = '\0';
                        cxx_class_add_using_base_member(
                            cls, rcc_intern(base_name),
                            rcc_intern(separator + 1));
                    }
                }
                expect(TOK_SEMICOLON, ";");
                continue;
            }

            /* Parse member */
            Token* member_start = parser.cur;
            parse_class_member(cls, current_access);
            if (parser.cur == member_start && !at_end()) advance();
        }

        expect(TOK_RBRACE, "}");
        active_class = enclosing_class;
    }

    /* Optional semicolon */
    match(TOK_SEMICOLON);

    /* A forward declaration has no layout yet. */
    if (!has_definition) return cls;

    /* The namespace owner is normally published immediately after this
     * routine returns, but vtable names/layout metadata are built here. */
    cls->ns = active_namespace ? active_namespace : g_global_namespace;
    resolve_class_bases(cls, loc);
    cxx_class_compute_layout(cls);
    complete_cxx_default_member_initializers(cls);
    cxx_class_build_vtable(cls);
    register_inline_class_accessors(cls);
    register_inline_class_bool_delegates(cls);
    register_inline_class_cleanup(cls);
    register_inline_class_releases(cls);
    register_inline_class_closes(cls);
    register_inline_class_close_delegates(cls);
    register_inline_class_move_constructor(cls);
    register_inline_class_move_assignment(cls);
    diagnose_unlowered_destructors(cls);
    register_class_static_fields(cls);
    register_ordinary_class_methods(cls);

    /* Aggregate classes and the validated one-field constructor subset can
     * reuse the common initializer/codegen backend.  Every complete class
     * still has to enter the parser's type-name table: later declarations
     * may use a non-aggregate class through a pointer or reference even when
     * its constructors, private members, or virtual members are not lowered
     * by the common backend. */
    if (!active_template && cls->type->is_complete) {
        uint32_t constructor_mask = lowerable_constructor_arity_mask(cls);
        if (constructor_mask != 0u) {
            rcc_parser_define_cxx_constructor_type(
                cls->name, cls->type, constructor_mask);
        } else {
            rcc_parser_define_type(cls->name, cls->type);
        }
    }

    return cls;
}

/* Parse class definition. */
CxxClass* parse_cxx_class(void) {
    SourceLoc loc = previous()->loc;
    bool is_struct = previous()->type == TOK_STRUCT;
    Token* name_tok = expect(TOK_IDENT, "class name");
    const char* class_name = name_tok ? name_tok->value.str_val : "anonymous";
    return parse_cxx_class_named(loc, is_struct, class_name);
}

/* ═══════════════════════════════════════
 * C++ Namespace Parsing
 * ═══════════════════════════════════════ */

static const char* namespace_qualified_decl_name(CxxNamespace* ns,
                                                 const char* name,
                                                 SourceLoc loc) {
    CxxNamespace* stack[32];
    int count = 0;
    char buffer[512] = "";
    size_t length = 0u;

    for (CxxNamespace* current = ns;
         current && current->name;
         current = current->parent) {
        if (count == (int)(sizeof(stack) / sizeof(stack[0]))) {
            rcc_error(loc, "namespace nesting exceeds compiler limit");
            break;
        }
        stack[count++] = current;
    }
    for (int index = count - 1; index >= 0; --index) {
        size_t part_length = strlen(stack[index]->name);
        if (part_length > sizeof(buffer) - 1u - length) {
            rcc_error(loc, "qualified declaration name exceeds compiler limit");
            return rcc_intern(buffer);
        }
        memcpy(buffer + length, stack[index]->name, part_length);
        length += part_length;
        if (length > sizeof(buffer) - 3u) {
            rcc_error(loc, "qualified declaration name exceeds compiler limit");
            return rcc_intern(buffer);
        }
        memcpy(buffer + length, "::", 2u);
        length += 2u;
    }
    if (strlen(name) > sizeof(buffer) - 1u - length) {
        rcc_error(loc, "qualified declaration name exceeds compiler limit");
        return rcc_intern(buffer);
    }
    strcpy(buffer + length, name);
    return rcc_intern(buffer);
}

static void set_cxx_link_name(Decl* declaration, CxxNamespace* ns,
                              bool c_language_linkage) {
    if (!declaration) return;
    if (declaration->kind == DECL_FUNC) {
        declaration->func_has_cxx_linkage = !c_language_linkage;
    }
    if (c_language_linkage) return;
    if (!ns && declaration->name &&
        strcmp(declaration->name, "main") == 0) {
        /* The hosted entry point is never mangled. */
        return;
    }
    if (declaration->kind == DECL_FUNC) {
        declaration->link_name = rcc_intern(
            cxx_mangle_function(declaration, ns, NULL));
    } else if (declaration->kind == DECL_VAR) {
        declaration->link_name = rcc_intern(
            cxx_mangle_name(declaration->name, ns, NULL));
    }
}

static void add_namespace_declaration(AST* ast, CxxNamespace* ns,
                                      Decl* declaration) {
    const char* qualified_name;

    if (!declaration) return;
    set_cxx_link_name(declaration, ns, false);
    if (declaration->kind == DECL_FUNC && ns) {
        declaration->func_cxx_namespace = cxx_namespace_qualified_name(ns);
    }
    qualified_name = namespace_qualified_decl_name(
        ns, declaration->name, declaration->loc);
    declaration->name = qualified_name;
    cxx_namespace_add_decl(ns, declaration);
    ast_add_decl(ast, declaration);
}

static void add_namespace_statement(AST* ast, CxxNamespace* ns,
                                     Stmt* statement) {
    if (!statement || statement->kind != STMT_DECL || !statement->decl) {
        return;
    }
    if (statement->decl->kind == DECL_STATIC_ASSERT) {
        /* Assertions have no namespace-owned symbol.  Keep them in the
         * translation-unit stream so sema evaluates them at their source
         * position without attempting to qualify a NULL declaration name. */
        ast_add_decl(ast, statement->decl);
        return;
    }
    add_namespace_declaration(ast, ns, statement->decl);
}

static const char* cxx_using_qualified_name(const char* name,
                                            SourceLoc loc) {
    char buffer[512];
    const char* namespace_name;
    size_t namespace_length;
    CxxNamespace* ns;

    if (!name) return rcc_intern("");
    while (name[0] == ':' && name[1] == ':') name += 2;
    if (active_namespace && active_namespace->name) {
        namespace_name = cxx_namespace_qualified_name(active_namespace);
        namespace_length = namespace_name ? strlen(namespace_name) : 0u;
        if (namespace_length != 0u &&
            namespace_length + 2u + strlen(name) < sizeof(buffer)) {
            memcpy(buffer, namespace_name, namespace_length);
            memcpy(buffer + namespace_length, "::", 2u);
            strcpy(buffer + namespace_length + 2u, name);
            /* Prefer the innermost namespace spelling when it names a real
             * declaration owner; this gives `using detail::f` inside `api`
             * the expected `api::detail::f` target. */
            {
                char* separator = strrchr(buffer, ':');
                if (separator && separator > buffer && separator[-1] == ':') {
                    separator[-1] = '\0';
                    ns = cxx_namespace_find(g_global_namespace, buffer);
                    separator[-1] = ':';
                    if (ns) return rcc_intern(buffer);
                }
            }
        }
    }
    (void)loc;
    return rcc_intern(name);
}

static void parse_cxx_using(CxxNamespace* ns) {
    SourceLoc loc = previous()->loc;
    const char* name;
    Token* local_name;

    if (match(TOK_NAMESPACE)) {
        name = parse_qualified_name();
        CxxNamespace* target = cxx_namespace_find(g_global_namespace, name);
        if (!target) {
            rcc_error(loc, "unknown namespace in using-directive '%s'", name);
        } else {
            cxx_namespace_add_using_namespace(ns, target);
        }
        expect(TOK_SEMICOLON, ";");
        return;
    }

    local_name = expect(TOK_IDENT, "name in using-declaration");
    if (!local_name) {
        while (!at_end() && !match(TOK_SEMICOLON)) advance();
        return;
    }
    if (match(TOK_ASSIGN)) {
        Type* alias_type = parse_cxx_type_spec();
        if (!alias_type) {
            rcc_error(loc, "using-alias requires a type");
        } else {
            rcc_parser_define_type(local_name->value.str_val, alias_type);
        }
    } else {
        char target[512];
        const char* suffix = NULL;
        /* The first identifier was consumed as the local spelling.  A using
         * declaration has no separate local identifier, so it is the first
         * component of the target and the remaining `::` chain follows. */
        if (strlen(local_name->value.str_val) >= sizeof(target)) {
            rcc_error(loc, "using-declaration name is too long");
            target[0] = '\0';
        } else {
            strcpy(target, local_name->value.str_val);
        }
        while (match(TOK_SCOPE)) {
            if (!check(TOK_IDENT)) {
                rcc_error(peek()->loc, "expected identifier after ::");
                break;
            }
            if (strlen(target) + 2u +
                    strlen(peek()->value.str_val) >= sizeof(target)) {
                rcc_error(loc, "using-declaration name is too long");
                break;
            }
            strcat(target, "::");
            suffix = advance()->value.str_val;
            strcat(target, suffix);
        }
        cxx_namespace_add_using_decl(
            ns, cxx_using_qualified_name(target, loc));
    }
    expect(TOK_SEMICOLON, ";");
}

static CxxNamespace* parse_cxx_namespace(AST* ast, CxxNamespace* parent) {
    SourceLoc loc = previous()->loc;
    CxxNamespace* outer_namespace = active_namespace;

    /* Namespace name (can be anonymous) */
    const char* ns_name = NULL;
    if (check(TOK_IDENT)) {
        ns_name = advance()->value.str_val;
    }

    CxxNamespace* ns = cxx_namespace_new(ns_name, loc);
    if (parent) cxx_namespace_add_namespace(parent, ns);
    active_namespace = ns;

    expect(TOK_LBRACE, "{");

    /* Parse namespace contents */
    while (!check(TOK_RBRACE) && !at_end()) {
        Token* declaration_start = parser.cur;
        int errors_before = g_error_count;
        skip_cxx_attributes();
        if (match(TOK_CLASS) || match(TOK_STRUCT)) {
            CxxClass* cls = parse_cxx_class();
            cxx_namespace_add_class(ns, cls);
        } else if (match(TOK_TEMPLATE)) {
            CxxTemplate* tmpl = parse_cxx_template();
            cxx_namespace_add_template(ns, tmpl);
        } else if (match(TOK_NAMESPACE)) {
            (void)parse_cxx_namespace(ast, ns);
        } else if (match(TOK_USING)) {
            parse_cxx_using(ns);
        } else if ((check(TOK_CONSTEXPR) || check(TOK_CONSTEVAL)) &&
                   !cxx_constexpr_starts_function()) {
            Stmt* statement = parse_cxx_statement();
            add_namespace_statement(ast, ns, statement);
        } else if ((check(TOK_AUTO) && parser.cur->next &&
                   parser.cur->next->type == TOK_IDENT &&
                   parser.cur->next->next &&
                   parser.cur->next->next->type == TOK_LPAREN) ||
                   cxx_decltype_auto_starts_function()) {
            bool is_constexpr = false;
            bool is_noexcept = false;
            bool is_consteval = false;
            Decl* declaration = parse_cxx_function_declaration(
                true, &is_constexpr, &is_noexcept, &is_consteval);
            if (declaration) {
                add_namespace_declaration(ast, ns, declaration);
            }
        } else if (check(TOK_CONSTEXPR) || check(TOK_CONSTEVAL) ||
                   check(TOK_INLINE) ||
                   check(TOK___INLINE__)) {
            bool is_constexpr = false;
            bool is_noexcept = false;
            bool is_consteval = false;
            Decl* declaration = parse_cxx_function_declaration(
                true, &is_constexpr, &is_noexcept, &is_consteval);
            add_namespace_declaration(ast, ns, declaration);
        } else {
            Stmt* statement = parse_cxx_statement();
            add_namespace_statement(ast, ns, statement);
        }
        if (g_error_count > errors_before) {
            while (!at_end() && !check(TOK_SEMICOLON) &&
                   !check(TOK_RBRACE)) {
                advance();
            }
            if (check(TOK_SEMICOLON)) advance();
        } else if (parser.cur == declaration_start && !at_end()) {
            advance();
        }
    }

    expect(TOK_RBRACE, "}");
    active_namespace = outer_namespace;

    return ns;
}

static DeclList* parse_cxx_parameter_declarations(void) {
    DeclList* params = NULL;
    int param_idx = 0;

    if (check(TOK_VOID) && check_next(TOK_RPAREN)) {
        advance();
        return NULL;
    }
    while (!check(TOK_RPAREN) && !at_end()) {
        const char* name = NULL;
        Type* type = parse_cxx_type_spec();
        bool parameter_pack = match(TOK_ELLIPSIS);
        Expr* default_argument = NULL;
        Decl* parameter;
        type = rcc_parser_parse_cxx_declarator(type, &name, NULL);
        if (match(TOK_ASSIGN)) {
            default_argument = parse_assignment_expression();
        }
        parameter = decl_param(name, type, param_idx++, peek()->loc);
        parameter->param_is_pack = parameter_pack;
        if (parameter_pack && (!active_template ||
                               active_template->kind != TMPL_FUNCTION)) {
            rcc_error(parameter->loc,
                      "function parameter packs require a function template");
        }
        parameter->param_default = default_argument;
        decllist_append(&params, parameter);
        if (!match(TOK_COMMA)) break;
    }
    return params;
}

static Type* cxx_lambda_function_type(Type* return_type, DeclList* params) {
    TypeParam* type_params = NULL;
    TypeParam** tail = &type_params;
    for (DeclList* item = params; item; item = item->next) {
        TypeParam* parameter = ast_arena_alloc(sizeof(*parameter));
        parameter->name = item->decl ? item->decl->name : NULL;
        parameter->type = item->decl ? item->decl->type : NULL;
        parameter->is_bitfield = false;
        parameter->bit_width = 0u;
        parameter->is_static = false;
        parameter->initializer = item->decl ? item->decl->param_default : NULL;
        parameter->next = NULL;
        *tail = parameter;
        tail = &parameter->next;
    }
    return type_func(return_type, type_params, false);
}

/* Generic lambda parameters are function-template type parameters in the
 * closure's call operator.  Keep the placeholder inside the parameter type
 * (including pointer/reference layers) so the ordinary template substitution
 * and target ABI lowering paths can be reused after an invocation supplies a
 * concrete argument. */
static Type* parse_cxx_lambda_auto_type(CxxTemplate* tmpl, int parameter_index,
                                        bool is_const, bool is_pointer,
                                        bool is_reference,
                                        bool is_rvalue_reference,
                                        SourceLoc loc) {
    char name[64];
    int written;
    const char* parameter_name;
    Type* placeholder;

    if (!tmpl) {
        rcc_error(loc, "generic lambda parameter has no template context");
        return type_int;
    }
    written = snprintf(name, sizeof(name), "__rcc_lambda_T%d",
                       parameter_index);
    if (written < 0 || (size_t)written >= sizeof(name)) {
        rcc_error(loc, "generic lambda type parameter name is too long");
        return type_int;
    }
    parameter_name = rcc_intern(name);
    cxx_template_add_type_param(tmpl, parameter_name);
    placeholder = type_struct(parameter_name);
    placeholder->cxx_dependent = true;
    placeholder->is_const = is_const;
    if (is_pointer) placeholder = type_ptr(placeholder);
    if (is_reference) {
        placeholder = type_ptr(placeholder);
        placeholder->is_reference = true;
        placeholder->is_rvalue_reference = is_rvalue_reference;
    }
    return placeholder;
}

static DeclList* parse_cxx_lambda_parameters(CxxTemplate* tmpl) {
    DeclList* params = NULL;
    int param_idx = 0;

    if (check(TOK_VOID) && check_next(TOK_RPAREN)) {
        advance();
        return NULL;
    }
    while (!check(TOK_RPAREN) && !at_end()) {
        const char* name = NULL;
        Type* type;
        Expr* default_argument = NULL;
        bool is_auto = check(TOK_AUTO) ||
            (check(TOK_CONST) && check_next(TOK_AUTO));
        bool parameter_pack = false;

        if (is_auto) {
            bool is_const = match(TOK_CONST);
            bool is_pointer = false;
            bool is_reference = false;
            bool is_rvalue_reference = false;
            SourceLoc loc = peek()->loc;
            expect(TOK_AUTO, "auto lambda parameter");
            if (match(TOK_STAR)) {
                is_pointer = true;
            } else if (match(TOK_AMP)) {
                is_reference = true;
            } else if (match(TOK_AND)) {
                is_reference = true;
                is_rvalue_reference = true;
            }
            parameter_pack = match(TOK_ELLIPSIS);
            name = expect(TOK_IDENT, "lambda parameter name")
                ? parser.prev->value.str_val : NULL;
            type = parse_cxx_lambda_auto_type(
                tmpl, tmpl ? tmpl->param_count : 0, is_const,
                is_pointer, is_reference, is_rvalue_reference, loc);
        } else {
            type = parse_cxx_type_spec();
            type = rcc_parser_parse_cxx_declarator(type, &name, NULL);
            parameter_pack = match(TOK_ELLIPSIS);
            if (parameter_pack) {
                rcc_error(peek()->loc,
                          "typed generic lambda parameter packs are not supported");
            }
        }
        if (match(TOK_ASSIGN)) default_argument = parse_assignment_expression();
        {
            Decl* parameter = decl_param(name, type, param_idx++, peek()->loc);
            parameter->param_is_pack = parameter_pack;
            parameter->param_default = default_argument;
            if (parameter_pack && tmpl && tmpl->param_count > 0) {
                tmpl->params[tmpl->param_count - 1].is_pack = true;
            }
            decllist_append(&params, parameter);
        }
        if (!match(TOK_COMMA)) break;
    }
    return params;
}

/* Lower an immediately-invoked lambda to a real internal function
 * declaration. Captures are explicit leading parameters, so this lowering
 * never drops captured state. */
Expr* rcc_parse_cxx_lambda(void) {
    SourceLoc loc = peek()->loc;
    DeclList* capture_params = NULL;
    DeclList* params = NULL;
    DeclList* all_params = NULL;
    ExprList* captures = NULL;
    StmtList* statements = NULL;
    Type* return_type = NULL;
    Stmt* body;
    Decl* function;
    char name[64];
    int written;
    int capture_count = 0;
    bool lambda_mutable = false;
    CxxReferenceCapture* lambda_reference_captures = NULL;
    CxxLambdaCaptureSpec* explicit_captures = NULL;
    CxxLambdaCaptureSpec* explicit_capture_tail = NULL;
    bool default_capture = false;
    bool default_reference = false;
    CxxTemplate* lambda_template = cxx_template_new(loc);

    expect(TOK_LBRACKET, "[");
    if ((check(TOK_ASSIGN) || check(TOK_AMP)) &&
        (check_next(TOK_RBRACKET) || check_next(TOK_COMMA))) {
        bool reference_default = check(TOK_AMP);
        advance();
        default_capture = true;
        default_reference = reference_default;
    }
    if (default_capture && !check(TOK_RBRACKET)) {
        expect(TOK_COMMA, "',' after lambda default capture");
    }
    if (!match(TOK_RBRACKET)) {
        do {
            Token* capture;
            bool reference_capture = match(TOK_AMP) || match(TOK_AND);
            if (check(TOK_THIS)) {
                capture = advance();
                if (reference_capture) {
                    rcc_error(capture->loc,
                              "lambda cannot capture this by reference");
                    reference_capture = false;
                }
            } else {
                capture = expect(TOK_IDENT, "lambda capture name");
            }
            if (!capture) break;
            {
                CxxLambdaCaptureSpec* spec =
                    ast_arena_alloc(sizeof(*spec));
                spec->name = capture->type == TOK_THIS
                    ? rcc_intern("this") : capture->value.str_val;
                spec->loc = capture->loc;
                spec->reference = reference_capture;
                spec->next = NULL;
                if (explicit_capture_tail) {
                    explicit_capture_tail->next = spec;
                } else {
                    explicit_captures = spec;
                }
                explicit_capture_tail = spec;
            }
        } while (match(TOK_COMMA));
        expect(TOK_RBRACKET, "]");
    }
    for (CxxLambdaCaptureSpec* spec = explicit_captures; spec;
         spec = spec->next) {
        Type* capture_type = cxx_parser_value_type(spec->name);
        if (!capture_type) {
            rcc_error(spec->loc, "lambda capture '%s' is not a local value",
                      spec->name);
            continue;
        }
        if (spec->reference) {
            CxxReferenceCapture* reference =
                ast_arena_alloc(sizeof(*reference));
            reference->name = spec->name;
            reference->next = lambda_reference_captures;
            lambda_reference_captures = reference;
            capture_type = type_ptr(capture_type);
            exprlist_append(&captures, expr_unary(
                EXPR_ADDR, expr_ident(spec->name, spec->loc), spec->loc));
        } else {
            exprlist_append(&captures,
                            expr_ident(spec->name, spec->loc));
        }
        decllist_append(&capture_params,
                        decl_param(spec->name, capture_type,
                                   capture_count++, spec->loc));
    }
    if (default_capture) {
        for (CxxParserValueBinding* binding = active_value_bindings;
             binding; binding = binding->next) {
            bool explicitly_captured = false;
            for (CxxLambdaCaptureSpec* spec = explicit_captures; spec;
                 spec = spec->next) {
                if (strcmp(spec->name, binding->name) == 0) {
                    explicitly_captured = true;
                    break;
                }
            }
            if (explicitly_captured) continue;
            Type* capture_type = binding->type;
            if (!capture_type) {
                rcc_error(loc, "lambda capture has no semantic type");
                continue;
            }
            if (default_reference && strcmp(binding->name, "this") != 0) {
                CxxReferenceCapture* reference =
                    ast_arena_alloc(sizeof(*reference));
                reference->name = binding->name;
                reference->next = lambda_reference_captures;
                lambda_reference_captures = reference;
                capture_type = type_ptr(capture_type);
                exprlist_append(&captures, expr_unary(
                    EXPR_ADDR, expr_ident(binding->name, loc), loc));
            } else {
                exprlist_append(&captures,
                                expr_ident(binding->name, loc));
            }
            decllist_append(&capture_params,
                            decl_param(binding->name, capture_type,
                                       capture_count++, loc));
        }
    }
    if (match(TOK_LPAREN)) {
        params = parse_cxx_lambda_parameters(lambda_template);
        expect(TOK_RPAREN, ")");
    }
    for (DeclList* item = capture_params; item; item = item->next) {
        decllist_append(&all_params, item->decl);
    }
    for (DeclList* item = params; item; item = item->next) {
        if (item->decl) {
            item->decl->param_index =
                capture_count + item->decl->param_index;
        }
        decllist_append(&all_params, item->decl);
    }
    lambda_mutable = match(TOK_MUTABLE);
    if (!lambda_mutable) {
        /* A non-mutable lambda has a const call operator.  Model each
         * by-value capture as a top-level const parameter so assignments to
         * the captured object are diagnosed while pointer/reference captures
         * retain their standard pointee mutability.  `this` is a pointer to
         * the original object and is likewise intentionally not qualified. */
        for (DeclList* item = capture_params; item; item = item->next) {
            Type* capture_type = item->decl ? item->decl->type : NULL;
            if (!item->decl || !capture_type ||
                (item->decl->name &&
                 strcmp(item->decl->name, "this") == 0) ||
                capture_type->is_reference || capture_type->is_const) {
                continue;
            }
            capture_type = ast_arena_alloc(sizeof(*capture_type));
            *capture_type = *item->decl->type;
            capture_type->is_const = true;
            item->decl->type = capture_type;
        }
    }
    if (match(TOK_NOEXCEPT)) {
        if (check(TOK_LPAREN)) skip_balanced(TOK_LPAREN, TOK_RPAREN);
    }
    if (match(TOK_ARROW)) {
        return_type = parse_cxx_type_spec();
        return_type = rcc_parser_parse_cxx_declarator(
            return_type, NULL, NULL);
    }
    expect(TOK_LBRACE, "{");
    if (saved_reference_capture_depth >=
        (int)(sizeof(saved_reference_captures) /
              sizeof(saved_reference_captures[0]))) {
        rcc_fatal("C++ lambda nesting is too deep");
    }
    saved_reference_captures[saved_reference_capture_depth++] =
        active_reference_captures;
    active_reference_captures = lambda_reference_captures;
    rcc_parser_cxx_begin_function_parameters(all_params);
    while (!check(TOK_RBRACE) && !at_end()) {
        Token* start = parser.cur;
        Stmt* statement = parse_cxx_statement();
        if (statement) stmtlist_append(&statements, statement);
        if (parser.cur == start && !at_end()) advance();
    }
    expect(TOK_RBRACE, "}");
    rcc_parser_cxx_end_function_parameters();
    active_reference_captures = saved_reference_captures[
        --saved_reference_capture_depth];
    body = stmt_block(statements, loc);
    written = snprintf(name, sizeof(name), "__rcc_lambda_%u",
                       ++cxx_lambda_counter);
    if (written < 0 || (size_t)written >= sizeof(name)) {
        rcc_fatal("C++ lambda symbol name exceeds compiler limits");
    }
    function = decl_func(rcc_intern(name),
                         cxx_lambda_function_type(
                             return_type ? return_type : type_int,
                             all_params),
                         all_params, body, loc);
    function->storage = STORAGE_STATIC;
    function->func_is_inline = true;
    /* An omitted lambda trailing return type follows the ordinary C++
     * placeholder-return rules.  Keep a concrete type in the pre-sema
     * function signature so parsing and call construction remain well typed,
     * then let sema deduce the exact return type from every return statement.
     * This is important for `double`, pointer, aggregate, and void lambdas;
     * treating every non-empty lambda as `int` silently changed the ABI and
     * truncated otherwise valid results. */
    function->func_is_auto_return = return_type == NULL;
    function->link_name = function->name;
    if (lambda_template->param_count > 0) {
        lambda_template->kind = TMPL_FUNCTION;
        lambda_template->name = function->name;
        lambda_template->func_def = function;
        lambda_template->ns = active_namespace;
    } else if (active_ast) {
        ast_add_decl(active_ast, function);
    }
    {
        Expr* result = expr_ident(function->name, loc);
        result->ident_decl = function;
        result->type = function->type;
        result->cxx_lambda_captures = captures;
        result->cxx_lambda_template = lambda_template->param_count > 0
            ? lambda_template : NULL;
        return result;
    }
}

/* Header-only SDK functions are emitted eagerly today.  Parse only bodies
 * made from the common C/C++ expression subset; template definitions and
 * exception/allocation constructs stay deferred instead of being assigned a
 * guessed meaning.  Local auto declarations are accepted because their
 * initializer must provide a concrete type before parsing can continue. */
static bool inline_body_is_lowerable(void) {
    Token* cursor = parser.cur;
    int depth = 0;
    if (!cursor || cursor->type != TOK_LBRACE) return false;
    do {
        switch (cursor->type) {
            case TOK_TEMPLATE:
            case TOK_TRY:
            case TOK_THROW:
            case TOK_NEW:
            case TOK_DELETE:
                return false;
            case TOK_LBRACE:
                ++depth;
                break;
            case TOK_RBRACE:
                --depth;
                break;
            default:
                break;
        }
        cursor = cursor->next;
    } while (cursor && depth > 0);
    return depth == 0;
}

static Decl* parse_cxx_function_declaration(bool parse_body,
                                            bool* is_constexpr,
                                            bool* is_noexcept,
                                            bool* is_consteval) {
    SourceLoc loc;
    Type* return_type;
    Token* name;
    DeclList* params;
    Stmt* body = NULL;
    bool is_inline = false;
    bool is_auto_return = false;
    bool is_decltype_auto_return = false;
    Expr* noexcept_expr = NULL;

    *is_constexpr = false;
    *is_noexcept = false;
    *is_consteval = false;
    skip_cxx_attributes();
    loc = peek()->loc;
    for (;;) {
        if (match(TOK_CONSTEXPR)) *is_constexpr = true;
        else if (match(TOK_CONSTEVAL)) {
            *is_constexpr = true;
            *is_consteval = true;
        }
        else if (match(TOK_INLINE) || match(TOK___INLINE__)) is_inline = true;
        else break;
    }
    if (match(TOK_AUTO)) {
        is_auto_return = true;
        /* The final return type is resolved after the body has been parsed
         * and its expressions have entered the function scope. */
        return_type = type_int;
    } else if (check(TOK_DECLTYPE) && parser.cur->next &&
               parser.cur->next->type == TOK_LPAREN &&
               parser.cur->next->next &&
               parser.cur->next->next->type == TOK_AUTO &&
               parser.cur->next->next->next &&
               parser.cur->next->next->next->type == TOK_RPAREN) {
        advance();
        advance();
        advance();
        advance();
        is_auto_return = true;
        is_decltype_auto_return = true;
        /* decltype(auto) is deduced after the body has been semantically
         * analyzed.  The placeholder is never emitted as a real type. */
        return_type = type_int;
    } else {
        return_type = parse_cxx_type_spec();
    }
    name = expect(TOK_IDENT, "function name");
    if (!name) return NULL;
    expect(TOK_LPAREN, "(");
    params = parse_cxx_parameter_declarations();
    expect(TOK_RPAREN, ")");
    if (is_auto_return && match(TOK_ARROW)) {
        return_type = parse_cxx_type_spec();
        is_auto_return = false;
        is_decltype_auto_return = false;
    }
    if (match(TOK_NOEXCEPT)) {
        if (check(TOK_LPAREN)) {
            advance();
            noexcept_expr = parse_expression();
            expect(TOK_RPAREN, ")");
        } else {
            *is_noexcept = true;
        }
    }
    /* Emit only the verified non-dependent header subset.  Incomplete class
     * and template bodies remain deferred until their object model exists. */
    if (!parse_body && is_inline && type_is_complete(return_type) &&
        check(TOK_LBRACE) && inline_body_is_lowerable()) {
        parse_body = true;
    }
    if (!parse_body && check(TOK_LBRACE)) {
        skip_balanced(TOK_LBRACE, TOK_RBRACE);
    } else if (match(TOK_LBRACE)) {
        StmtList* statements = NULL;
        rcc_parser_cxx_begin_function_parameters(params);
        while (!check(TOK_RBRACE) && !at_end()) {
            Token* start = parser.cur;
            Stmt* statement = parse_cxx_statement();
            if (statement) stmtlist_append(&statements, statement);
            if (parser.cur == start && !at_end()) advance();
        }
        expect(TOK_RBRACE, "}");
        rcc_parser_cxx_end_function_parameters();
        body = stmt_block(statements, loc);
    } else {
        expect(TOK_SEMICOLON, ";");
    }

    CxxMethod* function = cxx_method_new(name->value.str_val, return_type,
                                         params, body, loc);
    for (DeclList* parameter = params; parameter; parameter = parameter->next) {
        if (parameter->decl && parameter->decl->param_is_pack) {
            function->decl->type->variadic = true;
            break;
        }
    }
    function->decl->func_is_inline = is_inline;
    function->decl->func_is_constexpr = *is_constexpr;
    function->decl->func_is_consteval = *is_consteval;
    function->decl->func_is_noexcept = *is_noexcept;
    function->decl->func_noexcept_expr = noexcept_expr;
    function->decl->func_is_auto_return = is_auto_return;
    function->decl->func_is_decltype_auto_return = is_decltype_auto_return;
    return function->decl;
}

/* ═══════════════════════════════════════
 * C++ Template Parsing
 * ═══════════════════════════════════════ */

static bool template_type_parameter_matches(CxxTemplate* tmpl, Type* type,
                                            int parameter_index) {
    TemplateParam* parameter;
    if (!tmpl || !type || type->kind != TYPE_STRUCT || !type->tag ||
        parameter_index < 0 || parameter_index >= tmpl->param_count) {
        return false;
    }
    parameter = &tmpl->params[parameter_index];
    return parameter->kind == TPARAM_TYPE && parameter->name &&
           strcmp(parameter->name, type->tag) == 0;
}

static bool expression_is_identifier(Expr* expression, const char* name) {
    return expression && expression->kind == EXPR_IDENT && name &&
           expression->ident_name &&
           strcmp(expression->ident_name, name) == 0;
}

static bool expression_is_member_of(Expr* expression, const char* object,
                                    const char* member) {
    return expression && expression->kind == EXPR_MEMBER && member &&
           expression->member_name &&
           strcmp(expression->member_name, member) == 0 &&
           expression_is_identifier(expression->member_base, object);
}

static bool expression_is_template_sizeof(CxxTemplate* tmpl,
                                           Expr* expression,
                                           int parameter_index) {
    TemplateParam* parameter;
    if (!tmpl || !expression || expression->kind != EXPR_SIZEOF ||
        parameter_index < 0 || parameter_index >= tmpl->param_count) {
        return false;
    }
    parameter = &tmpl->params[parameter_index];
    if (expression->sizeof_type) {
        return template_type_parameter_matches(tmpl, expression->sizeof_type,
                                               parameter_index);
    }
    return parameter->name &&
           expression_is_identifier(expression->unary_operand,
                                    parameter->name);
}

/* Recognize only the ABI initializer idiom used by RinSDK.  Keeping this
 * structural, rather than interpreting arbitrary function-template bodies,
 * makes an accepted specialization equivalent to a designated value
 * initializer in the common C AST:
 *
 *   T value{};
 *   value.struct_size = sizeof(T);
 *   value.version = <integer constant>;
 *   return value;
 */
static void recognize_versioned_function_template(CxxTemplate* tmpl) {
    Decl* function;
    StmtList* statements;
    Stmt* declaration;
    Stmt* size_assignment;
    Stmt* version_assignment;
    Stmt* result;
    const char* variable;
    int64_t version;

    if (!tmpl || tmpl->kind != TMPL_FUNCTION || tmpl->param_count != 1 ||
        tmpl->params[0].kind != TPARAM_TYPE) {
        return;
    }
    function = tmpl->func_def;
    if (!function || function->kind != DECL_FUNC || !function->type ||
        function->type->kind != TYPE_FUNC || function->func_params ||
        !template_type_parameter_matches(tmpl, function->type->ret_type, 0) ||
        !function->func_body || function->func_body->kind != STMT_BLOCK) {
        return;
    }
    statements = function->func_body->block_stmts;
    if (!statements || !statements->next || !statements->next->next ||
        !statements->next->next->next ||
        statements->next->next->next->next) {
        return;
    }
    declaration = statements->stmt;
    size_assignment = statements->next->stmt;
    version_assignment = statements->next->next->stmt;
    result = statements->next->next->next->stmt;
    if (!declaration || declaration->kind != STMT_DECL ||
        !declaration->decl || declaration->decl->kind != DECL_VAR ||
        !declaration->decl->name ||
        !template_type_parameter_matches(tmpl, declaration->decl->type, 0) ||
        !declaration->decl->var_init ||
        declaration->decl->var_init->kind != EXPR_COMPOUND ||
        !declaration->decl->var_init->compound_value_init ||
        declaration->decl->var_init->compound_init) {
        return;
    }
    variable = declaration->decl->name;
    if (!size_assignment || size_assignment->kind != STMT_EXPR ||
        !size_assignment->expr || size_assignment->expr->kind != EXPR_ASSIGN ||
        !expression_is_member_of(size_assignment->expr->binary_lhs, variable,
                                 "struct_size") ||
        !expression_is_template_sizeof(tmpl,
                                       size_assignment->expr->binary_rhs, 0)) {
        return;
    }
    if (!version_assignment || version_assignment->kind != STMT_EXPR ||
        !version_assignment->expr ||
        version_assignment->expr->kind != EXPR_ASSIGN ||
        !expression_is_member_of(version_assignment->expr->binary_lhs,
                                 variable, "version") ||
        !expr_eval_integer_constant(version_assignment->expr->binary_rhs,
                                    &version) ||
        version < 0 || (uint64_t)version > UINT32_MAX) {
        return;
    }
    if (!result || result->kind != STMT_RETURN ||
        !expression_is_identifier(result->return_val, variable)) {
        return;
    }
    tmpl->function_lowering = TMPL_FUNCTION_VERSIONED_STRUCT;
    tmpl->function_constant = version;
}

CxxTemplate* parse_cxx_template(void) {
    SourceLoc loc = previous()->loc;
    CxxTemplate* parameter_outer_template = active_template;

    expect(TOK_LT, "<");

    CxxTemplate* tmpl = cxx_template_new(loc);
    active_template = tmpl;

    /* Parse template parameters */
    if (!check(TOK_GT)) {
        do {
            bool type_parameter = false;
            bool parameter_pack = false;
            bool template_parameter = false;
            if (match(TOK_TYPENAME) || match(TOK_CLASS)) {
                /* Type parameter */
                type_parameter = true;
                parameter_pack = match(TOK_ELLIPSIS);
                const char* param_name = NULL;
                if (check(TOK_IDENT)) {
                    param_name = advance()->value.str_val;
                }
                cxx_template_add_type_param(tmpl, param_name);
                tmpl->params[tmpl->param_count - 1].is_pack = parameter_pack;
            } else if (match(TOK_TEMPLATE)) {
                /* Keep the accepted template-template profile explicit.  The
                 * nested signature is retained so an argument cannot be
                 * accepted merely because it happens to name a class. */
                CxxTemplate* signature = cxx_template_new(peek()->loc);
                const char* param_name = NULL;
                expect(TOK_LT, "template parameter list");
                if (!check(TOK_GT)) {
                    do {
                        const char* nested_name = NULL;
                        if (match(TOK_TYPENAME) || match(TOK_CLASS)) {
                            if (check(TOK_IDENT)) {
                                nested_name = advance()->value.str_val;
                            }
                            cxx_template_add_type_param(signature, nested_name);
                        } else if (match(TOK_AUTO)) {
                            if (check(TOK_IDENT)) {
                                nested_name = advance()->value.str_val;
                            } else {
                                rcc_error(peek()->loc,
                                          "template-template auto parameter "
                                          "requires a name");
                            }
                            cxx_template_add_value_param(signature, nested_name,
                                                         type_int);
                        } else {
                            rcc_error(peek()->loc,
                                      "template-template parameter requires "
                                      "typename, class, or auto");
                            while (!check(TOK_COMMA) && !check(TOK_GT) &&
                                   !at_end()) {
                                advance();
                            }
                            cxx_template_add_type_param(signature, NULL);
                        }
                    } while (match(TOK_COMMA));
                }
                expect(TOK_GT, "template parameter list");
                if (!match(TOK_CLASS) && !match(TOK_TYPENAME)) {
                    rcc_error(peek()->loc,
                              "template-template parameter requires class "
                              "or typename");
                }
                if (check(TOK_IDENT)) param_name = advance()->value.str_val;
                tmpl->params = ast_arena_grow(
                    tmpl->params, sizeof(TemplateParam) * (size_t)tmpl->param_count,
                    sizeof(TemplateParam) * (size_t)(tmpl->param_count + 1));
                {
                    TemplateParam* parameter =
                        &tmpl->params[tmpl->param_count++];
                    memset(parameter, 0, sizeof(*parameter));
                    parameter->kind = TPARAM_TEMPLATE;
                    parameter->name = param_name ? rcc_strdup(param_name) : NULL;
                    parameter->template_signature = signature;
                }
                template_parameter = true;
            } else if (match(TOK_AUTO)) {
                /* C++17 `template<auto N>` is represented by the existing
                 * integral non-type path.  The bounded RinOS profile accepts
                 * only values that evaluate as target-independent integers;
                 * keeping the parameter's ABI type as int preserves the
                 * existing substitution, constraint, and mangling rules. */
                const char* param_name = NULL;
                parameter_pack = match(TOK_ELLIPSIS);
                if (check(TOK_IDENT)) {
                    param_name = advance()->value.str_val;
                } else {
                    rcc_error(peek()->loc,
                              "auto non-type template parameter requires a name");
                }
                cxx_template_add_value_param(tmpl, param_name, type_int);
                parameter_pack = parameter_pack || match(TOK_ELLIPSIS);
            } else {
                /* Non-type parameter */
                Type* param_type = parse_cxx_type_spec();
                const char* param_name = NULL;
                parameter_pack = match(TOK_ELLIPSIS);
                if (check(TOK_IDENT)) {
                    param_name = advance()->value.str_val;
                }
                cxx_template_add_value_param(tmpl, param_name, param_type);
            }

            if (parameter_pack) {
                tmpl->params[tmpl->param_count - 1].is_pack = true;
            }

            /* Default value? */
            if (match(TOK_ASSIGN)) {
                int parameter_index = tmpl->param_count - 1;
                if (parameter_index >= 0) {
                    if (template_parameter) {
                        rcc_error(peek()->loc,
                                  "template-template parameter defaults are "
                                  "not supported");
                        if (check(TOK_IDENT) || check(TOK_SCOPE)) {
                            (void)parse_qualified_name();
                        } else {
                            (void)parse_assignment_expression();
                        }
                    } else {
                        tmpl->params[parameter_index].has_default = true;
                    }
                    if (type_parameter) {
                        tmpl->params[parameter_index].default_type =
                            parse_cxx_type_spec();
                    } else if (!template_parameter) {
                        rcc_parser_set_cxx_template_default_mode(true);
                        tmpl->params[parameter_index].default_value =
                            parse_assignment_expression();
                        rcc_parser_set_cxx_template_default_mode(false);
                    }
                }
            }
        } while (match(TOK_COMMA));
    }

    expect(TOK_GT, ">");

    /* C++20 permits a requires-clause between the template parameter list
     * and the declaration.  Keep the accepted subset deliberately explicit:
     * instantiation evaluates an integral constant expression over non-type
     * parameters, so an unsupported type/concept requirement is diagnosed
     * instead of being treated as an always-true annotation. */
    if (match(TOK_REQUIRES)) {
        bool parenthesized = match(TOK_LPAREN);
        tmpl->constraint = parse_assignment_expression();
        if (parenthesized) expect(TOK_RPAREN, ")");
        if (!tmpl->constraint) {
            rcc_error(loc, "requires-clause requires a constraint expression");
        }
    }
    active_template = parameter_outer_template;

    /* Template body */
    if ((check(TOK_CLASS) || check(TOK_STRUCT)) &&
        parser.cur->next && parser.cur->next->type == TOK_IDENT &&
        parser.cur->next->next &&
        parser.cur->next->next->type == TOK_LT) {
        bool is_struct = match(TOK_STRUCT);
        Token* name_token;
        CxxTemplate* primary;
        Type* arguments[32] = { NULL };
        Expr* value_arguments[32] = { NULL };
        int argument_count = 0;
        CxxClass* specialized_class;
        CxxTemplate* outer_template = active_template;

        if (!is_struct) expect(TOK_CLASS, "class or struct");
        name_token = expect(TOK_IDENT, "specialized class name");
        primary = name_token ? find_class_template(name_token->value.str_val)
                             : NULL;
        active_template = tmpl;
        expect(TOK_LT, "<");
        if (!check(TOK_GT)) {
            do {
                if (argument_count == (int)(sizeof(arguments) /
                                            sizeof(arguments[0]))) {
                    rcc_error(loc, "class specialization argument limit exceeded");
                    while (!check(TOK_GT) && !at_end()) advance();
                    break;
                }
                if (primary && argument_count < primary->param_count &&
                    primary->params[argument_count].kind == TPARAM_NONTYPE) {
                    rcc_parser_set_cxx_template_default_mode(true);
                    value_arguments[argument_count] =
                        parse_assignment_expression();
                    rcc_parser_set_cxx_template_default_mode(false);
                } else {
                    arguments[argument_count] = parse_cxx_type_spec();
                }
                ++argument_count;
            } while (match(TOK_COMMA));
        }
        expect(TOK_GT, ">");
        if (!primary || primary->kind != TMPL_CLASS ||
            argument_count != primary->param_count) {
            rcc_error(loc, "explicit specialization has no matching class template");
        }
        tmpl->name = ast_arena_strdup(name_token ? name_token->value.str_val
                                                  : "specialization");
        tmpl->kind = TMPL_CLASS;
        tmpl->primary_template = primary;
        tmpl->templated_class = NULL;
        specialized_class = parse_cxx_class_named(
            loc, is_struct,
            name_token ? name_token->value.str_val : "specialization");
        active_template = outer_template;
        tmpl->templated_class = specialized_class;
        if (specialized_class) {
            int64_t identity_values[32] = { 0 };
            bool identity_present[32] = { false };
            if (primary && argument_count == primary->param_count) {
                for (int argument_index = 0;
                     argument_index < argument_count; ++argument_index) {
                    if (primary->params[argument_index].kind ==
                            TPARAM_NONTYPE &&
                        value_arguments[argument_index] &&
                        expr_eval_integer_constant(
                            value_arguments[argument_index],
                            &identity_values[argument_index])) {
                        identity_present[argument_index] = true;
                    }
                }
                cxx_set_template_identity(
                    specialized_class, primary, arguments,
                    identity_values, identity_present, argument_count);
            }
            if (tmpl->param_count == 0) {
                register_class_static_fields(specialized_class);
                register_ordinary_class_methods(specialized_class);
            }
            specialized_class->templ = tmpl;
            if (primary && argument_count == primary->param_count) {
                tmpl->specialization_args = ast_arena_alloc(
                    sizeof(Type*) * (size_t)argument_count);
                memcpy(tmpl->specialization_args, arguments,
                       sizeof(Type*) * (size_t)argument_count);
                tmpl->specialization_value_args = ast_arena_alloc(
                    sizeof(Expr*) * (size_t)argument_count);
                memcpy(tmpl->specialization_value_args, value_arguments,
                       sizeof(Expr*) * (size_t)argument_count);
                tmpl->specialization_arg_count = argument_count;
                primary->specializations = ast_arena_grow(
                    primary->specializations,
                    sizeof(CxxTemplate*) * (size_t)primary->specialization_count,
                    sizeof(CxxTemplate*) *
                        (size_t)(primary->specialization_count + 1));
                primary->specializations[primary->specialization_count++] = tmpl;
            }
        }
    } else if (match(TOK_CLASS) || match(TOK_STRUCT)) {
        CxxTemplate* outer_template = active_template;
        active_template = tmpl;
        tmpl->templated_class = parse_cxx_class();
        active_template = outer_template;
        tmpl->kind = TMPL_CLASS;
        tmpl->class_def = tmpl->templated_class;
        if (tmpl->templated_class) {
            tmpl->name = ast_arena_strdup(tmpl->templated_class->name);
        }
    } else {
        tmpl->kind = TMPL_FUNCTION;
        CxxTemplate* outer_template = active_template;
        bool is_consteval = false;
        active_template = tmpl;
        tmpl->func_def = parse_cxx_function_declaration(
            true, &tmpl->is_constexpr, &tmpl->is_noexcept, &is_consteval);
        active_template = outer_template;
        if (tmpl->func_def) {
            tmpl->name = ast_arena_strdup(tmpl->func_def->name);
        }
        recognize_versioned_function_template(tmpl);
    }

    for (int index = 0; index < tmpl->param_count; ++index) {
        if (tmpl->params[index].is_pack &&
            tmpl->kind != TMPL_FUNCTION &&
            cxx_class_pack_index(tmpl) < 0) {
            rcc_error(loc,
                      "only a single class-template parameter pack is supported");
        }
    }

    return tmpl;
}

/* ═══════════════════════════════════════
 * C++ Type Parsing
 * ═══════════════════════════════════════ */

static CxxTemplate* namespace_template(CxxNamespace* ns,
                                        const char* name) {
    int index;
    if (!ns || !name) return NULL;
    for (index = 0; index < ns->template_count; ++index) {
        CxxTemplate* candidate = ns->templates[index];
        if (candidate && candidate->name &&
            strcmp(candidate->name, name) == 0) {
            return candidate;
        }
    }
    return NULL;
}

static CxxTemplate* find_template(const char* qualified_name,
                                  int kind) {
    char buffer[512];
    char* component;
    char* next;
    CxxNamespace* ns;
    CxxTemplate* result;
    const char* name = qualified_name;

    if (!name) return NULL;
    while (name[0] == ':' && name[1] == ':') name += 2;
    if (!strstr(name, "::")) {
        for (ns = active_namespace; ns; ns = ns->parent) {
            result = namespace_template(ns, name);
            if (result && (int)result->kind == kind) return result;
        }
        result = namespace_template(g_global_namespace, name);
        return result && (int)result->kind == kind ? result : NULL;
    }

    if (strlen(name) >= sizeof(buffer)) return NULL;
    strcpy(buffer, name);
    ns = g_global_namespace;
    component = buffer;
    for (;;) {
        next = strstr(component, "::");
        if (!next) break;
        *next = '\0';
        ns = cxx_namespace_lookup(ns, component);
        if (!ns) return NULL;
        component = next + 2;
    }
    result = namespace_template(ns, component);
    return result && (int)result->kind == kind ? result : NULL;
}

static CxxTemplate* find_class_template(const char* qualified_name) {
    return find_template(qualified_name, TMPL_CLASS);
}

static int namespace_function_templates(CxxNamespace* ns, const char* name,
                                        CxxTemplate** results, int capacity) {
    int count = 0;
    if (!ns || !name || !results || capacity <= 0) return 0;
    for (int index = 0; index < ns->template_count; ++index) {
        CxxTemplate* candidate = ns->templates[index];
        if (!candidate || candidate->kind != TMPL_FUNCTION ||
            !candidate->name || strcmp(candidate->name, name) != 0) {
            continue;
        }
        if (count < capacity) results[count] = candidate;
        ++count;
    }
    return count;
}

/* Unlike ordinary name lookup, a function-template name denotes a candidate
 * set.  Stop at the first namespace in the lexical search that contributes a
 * match, then let the call-site deduction/partial-ordering pass select one. */
static int find_function_template_candidates(const char* qualified_name,
                                             CxxTemplate** results,
                                             int capacity) {
    char buffer[512];
    char* component;
    char* next;
    CxxNamespace* ns;
    const char* name = qualified_name;
    int count;

    if (!name || !results || capacity <= 0) return 0;
    while (name[0] == ':' && name[1] == ':') name += 2;
    if (!strstr(name, "::")) {
        for (ns = active_namespace; ns; ns = ns->parent) {
            count = namespace_function_templates(ns, name, results, capacity);
            if (count > 0) return count;
        }
        return namespace_function_templates(g_global_namespace, name,
                                            results, capacity);
    }
    if (strlen(name) >= sizeof(buffer)) return 0;
    strcpy(buffer, name);
    ns = g_global_namespace;
    component = buffer;
    for (;;) {
        next = strstr(component, "::");
        if (!next) break;
        *next = '\0';
        ns = cxx_namespace_lookup(ns, component);
        if (!ns) return 0;
        component = next + 2;
    }
    return namespace_function_templates(ns, component, results, capacity);
}

static CxxClass* namespace_class(CxxNamespace* ns, const char* name) {
    if (!ns || !name) return NULL;
    for (int index = 0; index < ns->class_count; ++index) {
        CxxClass* candidate = ns->classes[index];
        if (candidate && candidate->name &&
            strcmp(candidate->name, name) == 0) {
            return candidate;
        }
    }
    return NULL;
}

static CxxClass* find_class(const char* qualified_name) {
    char buffer[512];
    char* component;
    char* next;
    CxxNamespace* ns;
    const char* name = qualified_name;

    if (!name) return NULL;
    while (name[0] == ':' && name[1] == ':') name += 2;
    if (!strstr(name, "::")) {
        for (ns = active_namespace; ns; ns = ns->parent) {
            CxxClass* result = namespace_class(ns, name);
            if (result) return result;
        }
        return namespace_class(g_global_namespace, name);
    }
    if (strlen(name) >= sizeof(buffer)) return NULL;
    strcpy(buffer, name);
    ns = g_global_namespace;
    component = buffer;
    for (;;) {
        next = strstr(component, "::");
        if (!next) break;
        *next = '\0';
        ns = cxx_namespace_lookup(ns, component);
        if (!ns) return NULL;
        component = next + 2;
    }
    return namespace_class(ns, component);
}

/* Complete a previously declared static data member outside its class.  The
 * class parser has already published the declaration and its ABI spelling;
 * this hook only consumes the qualified definition and updates that same
 * declaration, avoiding duplicate data symbols in one translation unit. */
Stmt* rcc_parse_cxx_qualified_data_definition(
    Type* base_type, int storage, bool is_inline, bool is_constexpr,
    bool is_thread_local, SourceLoc loc) {
    Token* saved_cur = parser.cur;
    Token* saved_prev = parser.prev;
    const char* qualified;
    const char* separator;
    const char* member_name;
    size_t owner_length;
    char owner_name[512];
    CxxClass* cls;
    Decl* declaration = NULL;
    TypeParam* field;
    Expr* initializer = NULL;

    (void)is_inline;
    if (!check(TOK_IDENT) && !check(TOK_SCOPE)) return NULL;
    qualified = parse_qualified_name();
    separator = qualified ? strrchr(qualified, ':') : NULL;
    if (!separator || separator <= qualified || separator[-1] != ':') {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    owner_length = (size_t)(separator - qualified - 1);
    if (owner_length == 0u || owner_length >= sizeof(owner_name)) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    memcpy(owner_name, qualified, owner_length);
    owner_name[owner_length] = '\0';
    member_name = separator + 1;
    if (!*member_name) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    cls = find_class(owner_name);
    if (!cls) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    for (struct CxxMember* member = cls->members; member;
         member = member->next) {
        const char* final_name;
        if (!member->is_static || member->method || !member->decl ||
            member->decl->kind != DECL_VAR || !member->decl->name) {
            continue;
        }
        final_name = strrchr(member->decl->name, ':');
        final_name = final_name ? final_name + 1 : member->decl->name;
        if (strcmp(final_name, member_name) == 0) {
            declaration = member->decl;
            break;
        }
    }
    if (!declaration) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    if (is_thread_local) declaration->var_is_thread_local = true;
    if (!base_type || !declaration->type ||
        !type_is_compatible(base_type, declaration->type)) {
        rcc_error(loc, "static data member definition type does not match '%s'",
                  declaration->name);
    }
    if (match(TOK_ASSIGN)) {
        initializer = rcc_parser_parse_initializer();
    } else if (check(TOK_LBRACE)) {
        initializer = rcc_parser_parse_initializer();
    }
    rcc_parser_validate_cxx_constructor_initializer(
        declaration->type, initializer);
    expect(TOK_SEMICOLON, ";");

    if (declaration->var_init && initializer) {
        rcc_error(loc, "redefinition of static data member '%s'",
                  declaration->name);
    } else if (initializer) {
        declaration->var_init = initializer;
        declaration->var_is_constexpr = is_constexpr;
        for (field = cls->fields; field; field = field->next) {
            if (field->is_static && field->name &&
                strcmp(field->name, member_name) == 0) {
                field->initializer = initializer;
                break;
            }
        }
    }
    if (storage == STORAGE_EXTERN) declaration->storage = STORAGE_EXTERN;
    if (is_inline) declaration->var_is_inline = true;
    return stmt_null(loc);
}

static void resolve_class_bases(CxxClass* cls, SourceLoc loc) {
    if (!cls) return;
    for (int index = 0; index < cls->base_count; ++index) {
        const char* base_name = cls->bases[index].base_name;
        CxxClass* base;
        if (cls->bases[index].base || !base_name) continue;
        base = find_class(base_name);
        if (!base) {
            rcc_error(loc, "unknown base class '%s'", base_name);
            continue;
        }
        if (base == cls) {
            rcc_error(loc, "a class cannot derive from itself");
            continue;
        }
        cls->bases[index].base = base;
    }
}

/* Tell the shared C declaration parser when an identifier begins a C++ type
 * declaration.  This is deliberately a query: parsing an expression such as
 * `value < limit` must not consume tokens merely to decide whether it is a
 * declaration. */
bool rcc_parse_cxx_type_start(void) {
    Token* saved_cur = parser.cur;
    Token* saved_prev = parser.prev;
    const char* name;
    bool result = false;

    if (check(TOK_CLASS) || check(TOK_STRUCT)) {
        Token* next = parser.cur->next;
        result = next && next->type == TOK_IDENT;
        return result;
    }
    if (!check(TOK_IDENT) && !check(TOK_SCOPE)) return false;

    name = parse_qualified_name();
    result = is_active_template_type(name) || find_class(name) != NULL ||
             (strstr(name, "::") == NULL &&
              rcc_parser_lookup_type(name) != NULL);
    if (check(TOK_LT) &&
        (find_class_template(name) ||
         active_template_template_parameter_index(name) >= 0)) {
        result = true;
    }

    parser.cur = saved_cur;
    parser.prev = saved_prev;
    return result;
}

static int template_parameter_index(CxxTemplate* tmpl, Type* type) {
    int index;
    if (!tmpl || !type || type->kind != TYPE_STRUCT || !type->tag) {
        return -1;
    }
    for (index = 0; index < tmpl->param_count; ++index) {
        if (tmpl->params[index].kind == TPARAM_TYPE &&
            tmpl->params[index].name &&
            strcmp(tmpl->params[index].name, type->tag) == 0) {
            return index;
        }
    }
    return -1;
}

static Type* instantiate_class_template(CxxTemplate* tmpl, Type** arguments,
                                        const int64_t* value_args,
                                        const bool* value_present,
                                        int argument_count, SourceLoc loc);

static int active_template_template_parameter_index(const char* name) {
    if (!active_template || !name) return -1;
    for (int index = 0; index < active_template->param_count; ++index) {
        TemplateParam* parameter = &active_template->params[index];
        if (parameter->kind == TPARAM_TEMPLATE && parameter->name &&
            strcmp(parameter->name, name) == 0) {
            return index;
        }
    }
    return -1;
}

static bool template_template_signature_matches(
    const TemplateParam* parameter, const CxxTemplate* actual) {
    const CxxTemplate* signature = parameter ? parameter->template_signature : NULL;
    if (!parameter || parameter->kind != TPARAM_TEMPLATE || !signature ||
        !actual || actual->kind != TMPL_CLASS ||
        signature->param_count != actual->param_count) {
        return false;
    }
    for (int index = 0; index < signature->param_count; ++index) {
        if (signature->params[index].kind != actual->params[index].kind ||
            signature->params[index].is_pack != actual->params[index].is_pack) {
            return false;
        }
    }
    return true;
}

static Type* substitute_template_type(CxxTemplate* tmpl, Type* type,
                                      Type** arguments, int argument_count,
                                      const int64_t* value_args,
                                      const bool* value_present) {
    Type* substituted;
    Type* base;
    int array_len;
    Expr* array_bound;
    int parameter_index;
    if (!type) return NULL;
    if (type->cxx_dependent && type->cxx_template_param_index >= 0 &&
        type->cxx_template_arg_count > 0) {
        Type* template_argument;
        CxxTemplate* actual_template;
        Type* nested_arguments[32] = { NULL };
        Type* instantiated;
        if (type->cxx_template_param_index >= argument_count ||
            !arguments ||
            !(template_argument = arguments[type->cxx_template_param_index]) ||
            !(actual_template = template_argument->cxx_template) ||
            type->cxx_template_arg_count >
                (int)(sizeof(nested_arguments) / sizeof(nested_arguments[0])) ||
            !template_template_signature_matches(
                tmpl && type->cxx_template_param_index < tmpl->param_count
                    ? &tmpl->params[type->cxx_template_param_index] : NULL,
                actual_template) ||
            actual_template->param_count != type->cxx_template_arg_count) {
            rcc_error(type->cxx_class ? (SourceLoc){"<template>", 0, 0}
                                      : (SourceLoc){"<template>", 0, 0},
                      "template-template argument cannot be instantiated");
            return NULL;
        }
        for (int nested_index = 0;
             nested_index < type->cxx_template_arg_count; ++nested_index) {
            nested_arguments[nested_index] = substitute_template_type(
                tmpl, type->cxx_template_args[nested_index], arguments,
                argument_count, value_args, value_present);
            if (!nested_arguments[nested_index]) return NULL;
        }
        instantiated = instantiate_class_template(
            actual_template, nested_arguments, NULL, NULL,
            type->cxx_template_arg_count, (SourceLoc){"<template>", 0, 0});
        if (!instantiated) return NULL;
        return instantiated;
    }
    parameter_index = template_parameter_index(tmpl, type);
    if (parameter_index >= 0 && parameter_index < argument_count) {
        substituted = arguments[parameter_index];
        if (substituted != type && substituted->cxx_dependent) {
            Type* resolved = substitute_template_type(
                tmpl, substituted, arguments, argument_count, value_args,
                value_present);
            if (resolved) substituted = resolved;
        }
        if ((type->is_const && !substituted->is_const) ||
            (type->is_volatile && !substituted->is_volatile)) {
            Type* qualified = ast_arena_alloc(sizeof(*qualified));
            *qualified = *substituted;
            qualified->is_const = qualified->is_const || type->is_const;
            qualified->is_volatile = qualified->is_volatile ||
                                     type->is_volatile;
            substituted = qualified;
        }
        return substituted;
    }
    if (type->kind == TYPE_PTR || type->kind == TYPE_ARRAY) {
        base = substitute_template_type(
            tmpl, type->base, arguments, argument_count,
            value_args, value_present);
        array_len = type->array_len;
        array_bound = type->array_bound;
        if (type->kind == TYPE_ARRAY && type->array_bound) {
            int64_t value;
            if (eval_template_integer_expression(
                    type->array_bound, tmpl, value_args, value_present,
                    &value)) {
                if (value <= 0 || value > INT_MAX) {
                    rcc_error(type->array_bound->loc,
                              "template array bound is out of range");
                    return NULL;
                }
                array_len = (int)value;
                array_bound = NULL;
            }
        }
        if (base != type->base || array_len != type->array_len ||
            array_bound != type->array_bound) {
            substituted = ast_arena_alloc(sizeof(*substituted));
            *substituted = *type;
            substituted->base = base;
            if (substituted->kind == TYPE_ARRAY) {
                substituted->array_len = array_len;
                substituted->array_bound = array_bound;
                substituted->size = substituted->array_len > 0
                    ? base->size * substituted->array_len : 0;
                substituted->align = base->align;
            }
            return substituted;
        }
    } else if (type->kind == TYPE_FUNC) {
        Type* return_type = substitute_template_type(
            tmpl, type->ret_type, arguments, argument_count, value_args,
            value_present);
        TypeParam* parameters = NULL;
        TypeParam** tail = &parameters;
        bool changed = return_type != type->ret_type;
        for (TypeParam* parameter = type->params; parameter;
             parameter = parameter->next) {
            TypeParam* copy = ast_arena_alloc(sizeof(*copy));
            *copy = *parameter;
            copy->type = substitute_template_type(
                tmpl, parameter->type, arguments, argument_count, value_args,
                value_present);
            copy->next = NULL;
            if (copy->type != parameter->type) changed = true;
            *tail = copy;
            tail = &copy->next;
        }
        if (changed) {
            substituted = ast_arena_alloc(sizeof(*substituted));
            *substituted = *type;
            substituted->ret_type = return_type;
            substituted->params = parameters;
            return substituted;
        }
    }
    return type;
}

static TypeParam* substitute_template_parameters(CxxTemplate* tmpl,
                                                  TypeParam* parameters,
                                                  Type** arguments,
                                                  int argument_count,
                                                  const int64_t* value_args,
                                                  const bool* value_present) {
    TypeParam* result = NULL;
    TypeParam** tail = &result;
    for (; parameters; parameters = parameters->next) {
        TypeParam* copy = ast_arena_alloc(sizeof(*copy));
        *copy = *parameters;
        copy->type = substitute_template_type(
            tmpl, parameters->type, arguments, argument_count,
            value_args, value_present);
        copy->next = NULL;
        *tail = copy;
        tail = &copy->next;
    }
    return result;
}

static DeclList* substitute_template_decl_parameters(
    CxxTemplate* tmpl, DeclList* parameters, Type** arguments,
    int argument_count, const int64_t* value_args,
    const bool* value_present) {
    DeclList* result = NULL;
    for (; parameters; parameters = parameters->next) {
        Decl* parameter = parameters->decl;
        Decl* copy;
        copy = decl_param(parameter->name,
                          substitute_template_type(
                              tmpl, parameter->type, arguments,
                              argument_count, value_args, value_present),
                          parameter->param_index, parameter->loc);
        copy->param_default = parameter->param_default;
        decllist_append(
            &result, copy);
    }
    return result;
}

static CxxMethod* substitute_template_method(CxxTemplate* tmpl,
                                             CxxMethod* method,
                                             Type** arguments,
                                             int argument_count,
                                             const int64_t* value_args,
                                             const bool* value_present) {
    CxxMethod* copy;
    DeclList* parameters;
    Type* return_type;
    if (!method || !method->decl || !method->decl->type) return NULL;
    parameters = substitute_template_decl_parameters(
        tmpl, method->decl->func_params, arguments, argument_count,
        value_args, value_present);
    return_type = substitute_template_type(
        tmpl, method->decl->type->ret_type, arguments, argument_count,
        value_args, value_present);
    copy = cxx_method_new(cxx_method_source_name(method), return_type, parameters,
                          method->decl->func_body, method->decl->loc);
    copy->access = method->access;
    copy->is_static = method->is_static;
    copy->is_virtual = method->is_virtual;
    copy->is_pure_virtual = method->is_pure_virtual;
    copy->is_override = method->is_override;
    copy->is_final = method->is_final;
    copy->is_const = method->is_const;
    copy->is_constexpr = method->is_constexpr;
    copy->is_explicit = method->is_explicit;
    copy->is_noexcept = method->is_noexcept;
    copy->decl->func_is_noexcept = copy->is_noexcept;
    copy->is_deleted = method->is_deleted;
    copy->is_defaulted = method->is_defaulted;
    copy->is_constructor = method->is_constructor;
    copy->is_destructor = method->is_destructor;
    copy->decl->func_is_cxx_constructor = copy->is_constructor;
    copy->decl->func_is_cxx_destructor = copy->is_destructor;
    copy->decl->func_is_auto_return = method->decl->func_is_auto_return;
    copy->decl->func_is_decltype_auto_return =
        method->decl->func_is_decltype_auto_return;
    copy->decl->func_noexcept_expr = cxx_template_clone_expr_with_values(
        tmpl, method->decl->func_noexcept_expr, arguments, argument_count,
        value_args, value_present);
    copy->vtable_index = method->vtable_index;
    copy->decl->func_body = cxx_template_clone_stmt_with_values(
        tmpl, method->decl->func_body, arguments, argument_count,
        value_args, value_present);
    return copy;
}

static CxxMethod* instantiated_constructor_method(CxxClass* instance,
                                                   int ordinal) {
    for (struct CxxMember* member = instance ? instance->members : NULL;
         member; member = member->next) {
        if (!member->method || !member->method->is_constructor) continue;
        if (ordinal == 0) return member->method;
        --ordinal;
    }
    return NULL;
}

/* Expand one trailing class-template parameter pack while preserving any
 * fixed parameters declared before it.  A pack in any other position remains
 * diagnosed by the parser because its explicit argument boundary is not
 * recoverable in this bounded lowering path. */
static int cxx_class_pack_index(CxxTemplate* tmpl) {
    int pack_index = -1;
    if (!tmpl || tmpl->kind != TMPL_CLASS) return -1;
    for (int index = 0; index < tmpl->param_count; ++index) {
        if (!tmpl->params[index].is_pack) continue;
        if (pack_index >= 0 || index != tmpl->param_count - 1) return -1;
        pack_index = index;
    }
    return pack_index;
}

static Type* instantiate_class_template(CxxTemplate* tmpl, Type** arguments,
                                        const int64_t* value_args,
                                        const bool* value_present,
                                        int argument_count, SourceLoc loc) {
    CxxClass* definition;
    CxxClass* instance;
    char tag[320];
    int index;
    uint32_t constructor_mask;
    bool has_value_parameters = false;
    int pack_index = cxx_class_pack_index(tmpl);

    if (!tmpl || tmpl->kind != TMPL_CLASS || !tmpl->templated_class ||
        (pack_index < 0 && argument_count != tmpl->param_count) ||
        (pack_index >= 0 &&
         (argument_count < pack_index || argument_count > 32))) {
        rcc_error(loc, "class template argument count mismatch");
        return type_struct(tmpl && tmpl->name ? tmpl->name : "template");
    }
    for (index = 0; index < argument_count; ++index) {
        int parameter_index = pack_index >= 0 && index >= pack_index
            ? pack_index : index;
        TemplateParam* parameter = &tmpl->params[parameter_index];
        if (parameter->kind == TPARAM_NONTYPE) {
            has_value_parameters = true;
        }
        if (parameter->kind == TPARAM_TYPE &&
            (!arguments || !arguments[index])) {
            rcc_error(loc,
                      "class template type argument %d is missing", index + 1);
            return NULL;
        }
        if (parameter->kind == TPARAM_NONTYPE &&
            (!value_args || !value_present || !value_present[index] ||
             !parameter->type || !type_is_integer(parameter->type))) {
            rcc_error(loc,
                      "class template non-type argument %d requires an "
                      "integer constant", index + 1);
            return NULL;
        }
        if (parameter->kind == TPARAM_TEMPLATE) {
            if (!arguments || !arguments[index] ||
                !arguments[index]->cxx_template ||
                !template_template_signature_matches(
                    parameter, arguments[index]->cxx_template)) {
                rcc_error(loc, "class template template argument is invalid");
                return NULL;
            }
        }
    }
    for (index = 0; index < tmpl->instance_count; ++index) {
        int argument_index;
        bool matches = tmpl->instances[index].arg_count == argument_count;
        for (argument_index = 0; matches &&
             argument_index < argument_count; ++argument_index) {
        int parameter_index = pack_index >= 0 &&
                argument_index >= pack_index
            ? pack_index : argument_index;
            if (tmpl->params[parameter_index].kind == TPARAM_NONTYPE) {
                matches = tmpl->instances[index].value_present &&
                    tmpl->instances[index].value_present[argument_index] &&
                    value_present[argument_index] &&
                    tmpl->instances[index].value_args[argument_index] ==
                        value_args[argument_index];
            } else if (tmpl->params[parameter_index].kind == TPARAM_TEMPLATE) {
                matches = tmpl->instances[index].args &&
                    tmpl->instances[index].args[argument_index] &&
                    arguments && arguments[argument_index] &&
                    tmpl->instances[index].args[argument_index]->cxx_template ==
                        arguments[argument_index]->cxx_template;
            } else {
                matches = tmpl->instances[index].args && arguments &&
                    type_is_compatible(
                        tmpl->instances[index].args[argument_index],
                        arguments[argument_index]);
            }
        }
        if (matches) {
            return ((CxxClass*)tmpl->instances[index].instantiated)->type;
        }
    }

    definition = tmpl->templated_class;
    if (tmpl->specialization_arg_count > 0) {
        char specialization_suffix[256] = "";
        size_t suffix_length = 0;
        for (int argument_index = 0;
             argument_index < tmpl->specialization_arg_count;
             ++argument_index) {
            const char* mangled = tmpl->specialization_value_args &&
                    tmpl->specialization_value_args[argument_index]
                ? "v"
                : cxx_mangle_type(tmpl->specialization_args[argument_index]);
            int written = snprintf(
                specialization_suffix + suffix_length,
                sizeof(specialization_suffix) - suffix_length,
                "%s%s", argument_index == 0 ? "" : "_",
                mangled ? mangled : "?");
            if (written < 0 || (size_t)written >=
                                   sizeof(specialization_suffix) -
                                       suffix_length) {
                rcc_error(loc,
                          "class template specialization pattern is too long");
                return NULL;
            }
            suffix_length += (size_t)written;
        }
        if (snprintf(tag, sizeof(tag), "%s.__instance%d.%s",
                     tmpl->name ? tmpl->name : "template",
                     tmpl->instance_count, specialization_suffix) >=
            (int)sizeof(tag)) {
            rcc_error(loc, "class template specialization name is too long");
            return NULL;
        }
    } else if (snprintf(tag, sizeof(tag), "%s.__instance%d",
                        tmpl->name ? tmpl->name : "template",
                        tmpl->instance_count) >= (int)sizeof(tag)) {
        rcc_error(loc, "class template specialization name is too long");
        return NULL;
    }
    instance = cxx_class_new(ast_arena_strdup(tag), loc);
    instance->is_struct = definition->is_struct;
    instance->ns = definition->ns;
    instance->has_user_constructor = definition->has_user_constructor;
    instance->has_nonpublic_field = definition->has_nonpublic_field;
    instance->has_static_field = definition->has_static_field;
    instance->has_field_initializer = definition->has_field_initializer;
    instance->using_base_member_count = definition->using_base_member_count;
    if (definition->using_base_member_count != 0) {
        instance->using_base_members = ast_arena_alloc(
            sizeof(instance->using_base_members[0]) *
            (size_t)definition->using_base_member_count);
        memcpy(instance->using_base_members, definition->using_base_members,
               sizeof(instance->using_base_members[0]) *
               (size_t)definition->using_base_member_count);
    }
    instance->templ = tmpl;
    instance->template_arg_count = argument_count;
    if (argument_count > 0) {
        instance->template_args = ast_arena_alloc(
            sizeof(Type*) * (size_t)argument_count);
        memcpy(instance->template_args, arguments,
               sizeof(Type*) * (size_t)argument_count);
    }
    if (has_value_parameters) {
        if (argument_count > 0) {
            instance->template_value_args = ast_arena_alloc(
                sizeof(int64_t) * (size_t)argument_count);
            instance->template_value_present = ast_arena_alloc(
                sizeof(bool) * (size_t)argument_count);
            memcpy(instance->template_value_args, value_args,
                   sizeof(int64_t) * (size_t)argument_count);
            memcpy(instance->template_value_present, value_present,
                   sizeof(bool) * (size_t)argument_count);
        }
    }
    if (pack_index >= 0) {
        instance->template_pack_count = argument_count - pack_index;
        if (tmpl->params[pack_index].kind == TPARAM_TYPE) {
            if (argument_count > 0) {
                instance->template_args = ast_arena_alloc(
                    sizeof(Type*) * (size_t)argument_count);
                memcpy(instance->template_args, arguments,
                       sizeof(Type*) * (size_t)argument_count);
            }
        } else if (argument_count > 0) {
            instance->template_pack_values = ast_arena_alloc(
                sizeof(int64_t) * (size_t)argument_count);
            instance->template_pack_value_present = ast_arena_alloc(
                sizeof(bool) * (size_t)argument_count);
            memcpy(instance->template_pack_values, value_args,
                   sizeof(int64_t) * (size_t)argument_count);
            memcpy(instance->template_pack_value_present, value_present,
                   sizeof(bool) * (size_t)argument_count);
        }
    }
    if (tmpl->primary_template &&
        tmpl->specialization_arg_count ==
            tmpl->primary_template->param_count) {
        Type* identity_arguments[32] = { NULL };
        int64_t identity_values[32] = { 0 };
        bool identity_present[32] = { false };
        for (int identity_index = 0;
             identity_index < tmpl->primary_template->param_count;
             ++identity_index) {
            TemplateParam* parameter =
                &tmpl->primary_template->params[identity_index];
            if (parameter->kind == TPARAM_TYPE) {
                identity_arguments[identity_index] = substitute_template_type(
                    tmpl, tmpl->specialization_args[identity_index],
                    arguments, argument_count, value_args, value_present);
            } else if (parameter->kind == TPARAM_NONTYPE &&
                       tmpl->specialization_value_args &&
                       tmpl->specialization_value_args[identity_index]) {
                if (eval_template_integer_expression(
                        tmpl->specialization_value_args[identity_index],
                        tmpl,
                        value_args, value_present,
                        &identity_values[identity_index])) {
                    identity_present[identity_index] = true;
                } else {
                    rcc_error(loc,
                              "class template specialization value is not "
                              "an integer constant expression");
                }
            }
        }
        cxx_set_template_identity(
            instance, tmpl->primary_template, identity_arguments,
            identity_values, identity_present,
            tmpl->primary_template->param_count);
    } else {
        cxx_set_template_identity(instance, tmpl, arguments, value_args,
                                  value_present, argument_count);
    }

    int saved_pending_pack_count = tmpl->pending_pack_count;
    Type** saved_pending_pack_args = tmpl->pending_pack_args;
    int64_t* saved_pending_pack_values = tmpl->pending_pack_values;
    bool* saved_pending_pack_value_present = tmpl->pending_pack_value_present;
    if (pack_index >= 0) {
        tmpl->pending_pack_args = tmpl->params[pack_index].kind == TPARAM_TYPE
            ? arguments + pack_index : NULL;
        tmpl->pending_pack_values = tmpl->params[pack_index].kind == TPARAM_NONTYPE
            ? (int64_t*)value_args + pack_index : NULL;
        tmpl->pending_pack_value_present =
            tmpl->params[pack_index].kind == TPARAM_NONTYPE
                ? (bool*)value_present + pack_index : NULL;
        tmpl->pending_pack_count = argument_count - pack_index;
    }
    for (TypeParam* field = definition->fields; field; field = field->next) {
        cxx_class_add_field_initializer(
            instance, field->name,
            substitute_template_type(
                tmpl, field->type, arguments, argument_count,
                value_args, value_present),
            (AccessSpec)field->cxx_access,
            cxx_template_clone_expr_with_values(
                tmpl, field->initializer, arguments, argument_count,
                value_args, value_present),
            field->is_bitfield, field->bit_width, field->is_static);
    }
    register_instantiated_class_static_fields(instance, definition);
    for (struct CxxMember* member = definition->members; member;
         member = member->next) {
        CxxMethod* method = substitute_template_method(
            tmpl, member->method, arguments, argument_count,
            value_args, value_present);
        if (method) {
            method->owner = instance;
            cxx_class_add_method(instance, method);
        }
    }
    {
        int constructor_ordinal = 0;
        for (CxxConstructorInfo* constructor = definition->constructors;
             constructor; constructor = constructor->next) {
        CxxConstructorInfo* copy = ast_arena_alloc(sizeof(*copy));
        CxxConstructorInfo** tail = &instance->constructors;
        CxxConstructorInitializer** initializer_tail;
        *copy = *constructor;
        copy->method = instantiated_constructor_method(instance,
                                                       constructor_ordinal++);
        copy->parameters = substitute_template_parameters(
            tmpl, constructor->parameters, arguments, argument_count,
            value_args, value_present);
        copy->initializers = NULL;
        initializer_tail = &copy->initializers;
        for (CxxConstructorInitializer* initializer = constructor->initializers;
             initializer; initializer = initializer->next) {
            CxxConstructorInitializer* initializer_copy =
                ast_arena_alloc(sizeof(*initializer_copy));
            *initializer_copy = *initializer;
            initializer_copy->value = cxx_template_clone_expr_with_values(
                tmpl, initializer->value, arguments, argument_count,
                value_args, value_present);
            initializer_copy->arguments = NULL;
            for (ExprList* argument = initializer->arguments; argument;
                 argument = argument->next) {
                exprlist_append(
                    &initializer_copy->arguments,
                    cxx_template_clone_expr_with_values(
                        tmpl, argument->expr, arguments, argument_count,
                        value_args, value_present));
            }
            initializer_copy->constructor = NULL;
            initializer_copy->next = NULL;
            *initializer_tail = initializer_copy;
            initializer_tail = &initializer_copy->next;
        }
        copy->next = NULL;
        while (*tail) tail = &(*tail)->next;
        *tail = copy;
        }
    }
    tmpl->pending_pack_args = saved_pending_pack_args;
    tmpl->pending_pack_values = saved_pending_pack_values;
    tmpl->pending_pack_value_present = saved_pending_pack_value_present;
    tmpl->pending_pack_count = saved_pending_pack_count;

    cxx_class_compute_layout(instance);
    complete_cxx_default_member_initializers(instance);
    cxx_class_build_vtable(instance);
    diagnose_unlowered_destructors(instance);
    register_inline_class_accessors(instance);
    register_inline_class_bool_delegates(instance);
    register_inline_class_cleanup(instance);
    register_inline_class_releases(instance);
    register_inline_class_closes(instance);
    register_inline_class_close_delegates(instance);
    register_inline_class_move_constructor(instance);
    register_inline_class_move_assignment(instance);
    register_ordinary_class_methods(instance);
    constructor_mask = lowerable_constructor_arity_mask(instance);
    if (constructor_mask != 0u) {
        rcc_parser_define_cxx_constructor_type(instance->name,
                                               instance->type,
                                               constructor_mask);
    }

    tmpl->instances = ast_arena_grow(
        tmpl->instances,
        sizeof(tmpl->instances[0]) * (size_t)tmpl->instance_count,
        sizeof(tmpl->instances[0]) * (size_t)(tmpl->instance_count + 1));
    tmpl->instances[tmpl->instance_count].args = instance->template_args;
    tmpl->instances[tmpl->instance_count].value_args = NULL;
    tmpl->instances[tmpl->instance_count].value_present = NULL;
    tmpl->instances[tmpl->instance_count].pack_args = NULL;
    tmpl->instances[tmpl->instance_count].pack_values = NULL;
    tmpl->instances[tmpl->instance_count].pack_value_present = NULL;
    tmpl->instances[tmpl->instance_count].pack_count = 0;
    if (has_value_parameters) {
        tmpl->instances[tmpl->instance_count].value_args = ast_arena_alloc(
            sizeof(int64_t) * (size_t)argument_count);
        tmpl->instances[tmpl->instance_count].value_present = ast_arena_alloc(
            sizeof(bool) * (size_t)argument_count);
        memcpy(tmpl->instances[tmpl->instance_count].value_args, value_args,
               sizeof(int64_t) * (size_t)argument_count);
        memcpy(tmpl->instances[tmpl->instance_count].value_present,
               value_present, sizeof(bool) * (size_t)argument_count);
    }
    if (pack_index >= 0 && argument_count > 0) {
        tmpl->instances[tmpl->instance_count].pack_count = argument_count;
        if (tmpl->params[pack_index].kind == TPARAM_TYPE) {
            tmpl->instances[tmpl->instance_count].pack_args =
                ast_arena_alloc(sizeof(Type*) * (size_t)argument_count);
            memcpy(tmpl->instances[tmpl->instance_count].pack_args,
                   arguments, sizeof(Type*) * (size_t)argument_count);
        } else {
            tmpl->instances[tmpl->instance_count].pack_values = ast_arena_alloc(
                sizeof(int64_t) * (size_t)argument_count);
            tmpl->instances[tmpl->instance_count].pack_value_present =
                ast_arena_alloc(sizeof(bool) * (size_t)argument_count);
            memcpy(tmpl->instances[tmpl->instance_count].pack_values, value_args,
                   sizeof(int64_t) * (size_t)argument_count);
            memcpy(tmpl->instances[tmpl->instance_count].pack_value_present,
                   value_present, sizeof(bool) * (size_t)argument_count);
        }
    }
    tmpl->instances[tmpl->instance_count].arg_count = argument_count;
    tmpl->instances[tmpl->instance_count].instantiated = instance;
    ++tmpl->instance_count;
    return instance->type;
}

static bool deduce_class_specialization_value(
    CxxTemplate* tmpl, Expr* pattern, int64_t actual, int64_t* values,
    bool* value_present, int* specificity) {
    int parameter_index;
    int64_t constant;
    if (!tmpl || !pattern || !values || !value_present) return false;
    parameter_index = -1;
    if (pattern->kind == EXPR_IDENT && pattern->ident_name) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->kind == TPARAM_NONTYPE && parameter->name &&
                strcmp(parameter->name, pattern->ident_name) == 0) {
                parameter_index = index;
                break;
            }
        }
    }
    if (parameter_index >= 0) {
        if (value_present[parameter_index]) {
            return values[parameter_index] == actual;
        }
        values[parameter_index] = actual;
        value_present[parameter_index] = true;
        return true;
    }
    if (!expr_eval_integer_constant(pattern, &constant)) return false;
    if (specificity) *specificity += 16;
    return constant == actual;
}

static bool deduce_class_specialization_type(CxxTemplate* tmpl,
                                              Type* pattern, Type* actual,
                                              Type** arguments,
                                              int* specificity) {
    int nested_specificity = 0;
    if (!tmpl || !pattern || !actual || !arguments) return false;
    if (pattern->kind == TYPE_STRUCT && pattern->tag) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->kind == TPARAM_TYPE && parameter->name &&
                strcmp(parameter->name, pattern->tag) == 0) {
                if (!arguments[index]) {
                    arguments[index] = actual;
                    if (specificity) *specificity += 0;
                    return true;
                }
                return type_is_compatible(arguments[index], actual);
            }
        }
    }
    if (pattern->kind == TYPE_PTR && actual->kind == TYPE_PTR &&
        pattern->is_reference == actual->is_reference &&
        pattern->is_rvalue_reference == actual->is_rvalue_reference) {
        return deduce_class_specialization_type(
            tmpl, pattern->base, actual->base, arguments,
            specificity ? &nested_specificity : NULL) &&
            (!specificity || (*specificity += nested_specificity + 1, true));
    }
    if (pattern->kind == TYPE_ARRAY && actual->kind == TYPE_ARRAY &&
        (pattern->array_len < 0 ||
         pattern->array_len == actual->array_len)) {
        if (specificity && pattern->array_len >= 0) *specificity += 4;
        return deduce_class_specialization_type(
            tmpl, pattern->base, actual->base, arguments, specificity);
    }
    if (!type_is_compatible(pattern, actual)) return false;
    if (specificity) *specificity += 8;
    return true;
}

CxxClass* rcc_cxx_instantiate_class_template(CxxTemplate* tmpl,
                                              Type** arguments,
                                              const int64_t* value_args,
                                              const bool* value_present,
                                              int argument_count,
                                              SourceLoc loc) {
    Type* type = instantiate_class_template(tmpl, arguments, value_args,
                                             value_present, argument_count,
                                             loc);
    int pack_index = cxx_class_pack_index(tmpl);
    if (!type || !tmpl) return NULL;
    for (int index = 0; index < tmpl->instance_count; ++index) {
        if (tmpl->instances[index].arg_count != argument_count) continue;
        bool matches = true;
        for (int argument_index = 0; argument_index < argument_count;
             ++argument_index) {
        int parameter_index = pack_index >= 0 &&
                argument_index >= pack_index
            ? pack_index : argument_index;
            if (tmpl->params[parameter_index].kind == TPARAM_NONTYPE) {
                if (!tmpl->instances[index].value_present ||
                    !value_present ||
                    !tmpl->instances[index].value_present[argument_index] ||
                    !value_present[argument_index] ||
                    tmpl->instances[index].value_args[argument_index] !=
                        value_args[argument_index]) {
                    matches = false;
                    break;
                }
            } else if (tmpl->params[parameter_index].kind == TPARAM_TEMPLATE) {
                if (!tmpl->instances[index].args[argument_index] ||
                    !arguments[argument_index] ||
                    tmpl->instances[index].args[argument_index]->cxx_template !=
                        arguments[argument_index]->cxx_template) {
                    matches = false;
                    break;
                }
            } else if (!type_is_compatible(
                           tmpl->instances[index].args[argument_index],
                           arguments[argument_index])) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return (CxxClass*)tmpl->instances[index].instantiated;
        }
    }
    return NULL;
}

static Type* parse_class_template_specialization(CxxTemplate* tmpl,
                                                 SourceLoc loc) {
    Type* arguments[32] = { NULL };
    int64_t values[32] = { 0 };
    bool value_present[32] = { false };
    int argument_count = 0;
    int pack_index = cxx_class_pack_index(tmpl);
    expect(TOK_LT, "<");
    if (!check(TOK_GT)) {
        do {
            if (argument_count == (int)(sizeof(arguments) /
                                        sizeof(arguments[0]))) {
                rcc_error(loc, "class template argument limit exceeded");
                break;
            }
            if (argument_count >= tmpl->param_count && pack_index < 0) {
                rcc_error(peek()->loc, "too many class template arguments");
                while (!check(TOK_COMMA) && !check(TOK_GT) && !at_end()) {
                    advance();
                }
                arguments[argument_count++] = type_int;
                continue;
            }
            TemplateParam* parameter = &tmpl->params[
                pack_index >= 0 && argument_count >= pack_index
                    ? pack_index : argument_count];
            if (parameter->kind == TPARAM_TYPE) {
                arguments[argument_count++] = parse_cxx_type_spec();
            } else if (parameter->kind == TPARAM_NONTYPE) {
                Expr* value_expression;
                int64_t value;
                rcc_parser_set_cxx_template_default_mode(true);
                value_expression = parse_assignment_expression();
                rcc_parser_set_cxx_template_default_mode(false);
                if (!expr_eval_integer_constant(value_expression, &value)) {
                    SourceLoc value_loc;
                    cxx_parser_expr_loc(&value_loc, value_expression, &loc);
                    rcc_error(value_loc,
                              "class template non-type argument must be an "
                              "integer constant expression");
                    value = 0;
                }
                arguments[argument_count] = parameter->type;
                values[argument_count] = value;
                value_present[argument_count] = true;
                ++argument_count;
            } else {
                const char* argument_name = NULL;
                CxxTemplate* argument_template;
                Type* template_carrier;
                if (check(TOK_IDENT) || check(TOK_SCOPE)) {
                    argument_name = parse_qualified_name();
                } else {
                    rcc_error(peek()->loc,
                              "template-template argument requires a class "
                              "template name");
                }
                argument_template = argument_name
                    ? find_class_template(argument_name) : NULL;
                if (!argument_template ||
                    !template_template_signature_matches(
                        parameter, argument_template)) {
                    rcc_error(loc,
                              "template-template argument does not match its "
                              "parameter list");
                    template_carrier = type_int;
                } else {
                    template_carrier = type_struct(argument_name);
                    template_carrier->cxx_template = argument_template;
                }
                arguments[argument_count++] = template_carrier;
            }
        } while (match(TOK_COMMA));
    }
    while (pack_index < 0 && argument_count < tmpl->param_count &&
           tmpl->params[argument_count].has_default) {
        TemplateParam* parameter = &tmpl->params[argument_count];
        if (parameter->kind == TPARAM_TYPE && parameter->default_type) {
            arguments[argument_count] = substitute_template_type(
                tmpl, parameter->default_type, arguments, tmpl->param_count,
                values, value_present);
        } else if (parameter->kind == TPARAM_NONTYPE &&
                   parameter->default_value) {
            int64_t value;
            if (!eval_template_integer_expression(
                    parameter->default_value, tmpl, values, value_present,
                    &value)) {
                rcc_error(loc,
                          "class template non-type default must be an "
                          "integer constant expression");
                value = 0;
            }
            arguments[argument_count] = parameter->type;
            values[argument_count] = value;
            value_present[argument_count] = true;
        } else {
            break;
        }
        ++argument_count;
    }
    expect(TOK_GT, ">");
    for (int argument_index = 0; argument_index < argument_count;
         ++argument_index) {
        if (arguments[argument_index] &&
            arguments[argument_index]->cxx_dependent) {
            Type* dependent = type_struct(tmpl->name ? tmpl->name :
                                          "dependent-template");
            dependent->cxx_dependent = true;
            return dependent;
        }
    }
    CxxTemplate* selected = NULL;
    Type* selected_arguments[32] = { NULL };
    int64_t selected_values[32] = { 0 };
    bool selected_value_present[32] = { false };
    int selected_specificity = -1;
    for (int index = 0; index < tmpl->specialization_count; ++index) {
        CxxTemplate* specialization = tmpl->specializations[index];
        bool matches = specialization &&
            specialization->specialization_arg_count == argument_count;
        Type* specialization_arguments[32] = { NULL };
        int64_t specialization_values[32] = { 0 };
        bool specialization_value_present[32] = { false };
        int specificity = specialization && specialization->param_count == 0
            ? 100000 : 0;
        for (int argument_index = 0; matches &&
             argument_index < argument_count; ++argument_index) {
            if (tmpl->params[argument_index].kind == TPARAM_NONTYPE) {
                matches = specialization->specialization_value_args &&
                    specialization->specialization_value_args[argument_index] &&
                    deduce_class_specialization_value(
                        specialization,
                        specialization->specialization_value_args[
                            argument_index],
                        values[argument_index],
                        specialization_values,
                        specialization_value_present, &specificity);
            } else if (specialization->param_count == 0) {
                matches = specialization->specialization_args &&
                    type_is_compatible(
                        specialization->specialization_args[argument_index],
                        arguments[argument_index]);
            } else if (specialization->param_count <=
                       (int)(sizeof(specialization_arguments) /
                             sizeof(specialization_arguments[0]))) {
                matches = deduce_class_specialization_type(
                    specialization,
                    specialization->specialization_args[argument_index],
                    arguments[argument_index], specialization_arguments,
                    &specificity);
            } else {
                matches = false;
            }
        }
        if (matches && specialization->templated_class) {
            for (int parameter_index = 0;
                 parameter_index < specialization->param_count;
                 ++parameter_index) {
                if (specialization->params[parameter_index].kind ==
                        TPARAM_NONTYPE
                    ? !specialization_value_present[parameter_index]
                    : !specialization_arguments[parameter_index]) {
                    matches = false;
                    break;
                }
            }
            if (matches) {
                if (specificity > selected_specificity) {
                    selected = specialization;
                    selected_specificity = specificity;
                    memcpy(selected_arguments, specialization_arguments,
                           sizeof(selected_arguments));
                    memcpy(selected_values, specialization_values,
                           sizeof(selected_values));
                    memcpy(selected_value_present, specialization_value_present,
                           sizeof(selected_value_present));
                } else if (specificity == selected_specificity) {
                    rcc_error(loc,
                              "ambiguous class template partial specialization "
                              "for '%s'",
                              tmpl->name ? tmpl->name : "template");
                    return NULL;
                }
            }
        }
    }
    if (selected) {
        if (selected->param_count == 0) {
            CxxClass* specialized = selected->templated_class;
            int64_t identity_values[32] = { 0 };
            bool identity_present[32] = { false };
            for (int argument_index = 0; argument_index < argument_count;
                 ++argument_index) {
                if (tmpl->params[argument_index].kind == TPARAM_NONTYPE &&
                    selected->specialization_value_args &&
                    selected->specialization_value_args[argument_index] &&
                    expr_eval_integer_constant(
                        selected->specialization_value_args[argument_index],
                        &identity_values[argument_index])) {
                    identity_present[argument_index] = true;
                }
            }
            cxx_set_template_identity(
                specialized, tmpl, arguments, identity_values,
                identity_present, argument_count);
            return specialized->type;
        }
        Type* instance_type = instantiate_class_template(
            selected, selected_arguments, selected_values,
            selected_value_present,
            selected->param_count, loc);
        if (instance_type && instance_type->cxx_class) {
            cxx_set_template_identity(
                instance_type->cxx_class, tmpl, arguments, values,
                value_present, argument_count);
        }
        return instance_type;
    }
    return instantiate_class_template(tmpl, arguments, values, value_present,
                                      argument_count, loc);
}

static bool deduce_function_template_type(CxxTemplate* tmpl, Type* pattern,
                                          Type* actual, Type** arguments,
                                          int64_t* values,
                                          bool* value_present,
                                          int* specificity) {
    TypeParam* pattern_parameter;
    TypeParam* actual_parameter;
    if (!tmpl || !pattern || !actual || !arguments) return false;
    if (pattern->kind == TYPE_STRUCT && pattern->tag) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->kind == TPARAM_TYPE && parameter->name &&
                strcmp(parameter->name, pattern->tag) == 0) {
                if (pattern->is_const || pattern->is_volatile) {
                    Type* unqualified = ast_arena_alloc(sizeof(*unqualified));
                    *unqualified = *actual;
                    unqualified->is_const = false;
                    unqualified->is_volatile = false;
                    actual = unqualified;
                }
                if (!arguments[index]) {
                    arguments[index] = actual;
                    return true;
                }
                return type_is_compatible(arguments[index], actual);
            }
        }
    }
    if (pattern->kind == TYPE_PTR && pattern->is_reference) {
        if (actual->kind == TYPE_PTR && actual->is_reference) {
            actual = actual->base;
        }
        if (specificity) *specificity += 8;
        return deduce_function_template_type(tmpl, pattern->base, actual,
                                             arguments, values,
                                             value_present, specificity);
    }
    if (pattern->kind == TYPE_PTR && !pattern->is_reference &&
        actual->kind == TYPE_FUNC && pattern->base &&
        pattern->base->kind == TYPE_FUNC) {
        /* A function designator undergoes the standard function-to-pointer
         * conversion when it is passed to a function-pointer parameter. */
        if (specificity) *specificity += 4;
        return deduce_function_template_type(
            tmpl, pattern->base, actual, arguments, values, value_present,
            specificity);
    }
    if (pattern->kind == TYPE_PTR && actual->kind == TYPE_PTR) {
        if (specificity) *specificity += 8;
        return deduce_function_template_type(tmpl, pattern->base,
                                             actual->base, arguments, values,
                                             value_present, specificity);
    }
    if (pattern->kind == TYPE_FUNC && actual->kind == TYPE_FUNC) {
        if (pattern->variadic != actual->variadic ||
            pattern->has_prototype != actual->has_prototype) {
            return false;
        }
        if (!deduce_function_template_type(
                tmpl, pattern->ret_type, actual->ret_type, arguments, values,
                value_present, specificity)) {
            return false;
        }
        pattern_parameter = pattern->params;
        actual_parameter = actual->params;
        while (pattern_parameter && actual_parameter) {
            if (!deduce_function_template_type(
                    tmpl, pattern_parameter->type, actual_parameter->type,
                    arguments, values, value_present, specificity)) {
                return false;
            }
            pattern_parameter = pattern_parameter->next;
            actual_parameter = actual_parameter->next;
        }
        if (pattern_parameter || actual_parameter) return false;
        if (specificity) *specificity += 16;
        return true;
    }
    if (pattern->kind == TYPE_ARRAY && actual->kind == TYPE_ARRAY) {
        if (specificity) *specificity += 8;
        if (pattern->array_bound &&
            pattern->array_bound->kind == EXPR_IDENT && values &&
            value_present) {
            for (int index = 0; index < tmpl->param_count; ++index) {
                TemplateParam* parameter = &tmpl->params[index];
                if (parameter->kind != TPARAM_NONTYPE ||
                    !parameter->name ||
                    strcmp(parameter->name,
                           pattern->array_bound->ident_name) != 0) {
                    continue;
                }
                if (actual->array_len <= 0) return false;
                if (value_present[index] && values[index] != actual->array_len) {
                    return false;
                }
                values[index] = actual->array_len;
                value_present[index] = true;
                break;
            }
        } else if (pattern->array_len >= 0 &&
                   pattern->array_len != actual->array_len) {
            return false;
        }
        return deduce_function_template_type(tmpl, pattern->base,
                                             actual->base, arguments, values,
                                             value_present, specificity);
    }
    if (specificity) *specificity += 16;
    return type_is_compatible(pattern, actual);
}

/* Function-call template deduction applies the by-value parameter
 * adjustments before matching the pattern.  In particular, an array or
 * function argument decays at the call boundary, while an array bound behind
 * a reference pattern remains available for non-type deduction. */
static Type* cxx_parser_template_deduction_argument(Type* pattern,
                                                    Type* actual) {
    Type* adjusted;
    if (!pattern || !actual || pattern->is_reference ||
        pattern->is_rvalue_reference) {
        return actual;
    }
    if (actual->kind == TYPE_ARRAY) {
        return type_ptr(actual->base);
    }
    if (actual->kind == TYPE_FUNC) return type_ptr(actual);
    if (!actual->is_const && !actual->is_volatile) return actual;
    adjusted = ast_arena_alloc(sizeof(*adjusted));
    *adjusted = *actual;
    adjusted->is_const = false;
    adjusted->is_volatile = false;
    return adjusted;
}

static int cxx_parser_type_pack_index(CxxTemplate* tmpl, Type* pattern);

static bool deduce_function_template_arguments(CxxTemplate* tmpl,
                                               ExprList* call_arguments,
                                               Type** template_arguments,
                                               int64_t* template_values,
                                               bool* template_value_present,
                                               int* specificity,
                                               bool report_errors,
                                               Type** pack_arguments,
                                               int* pack_count) {
    DeclList* parameter;
    ExprList* argument;
    if (!tmpl || !tmpl->func_def || !template_arguments) return false;
    parameter = tmpl->func_def->func_params;
    argument = call_arguments;
    while (parameter && argument) {
        if (parameter->decl && parameter->decl->param_is_pack) {
            int pack_index = cxx_parser_type_pack_index(
                tmpl, parameter->decl->type);
            (void)pack_index;
            if (!pack_arguments || !pack_count || pack_index < 0) {
                if (report_errors) {
                    rcc_error(parameter->decl->loc,
                              "function parameter pack is not a type pack");
                }
                return false;
            }
            while (argument) {
                Type* actual = cxx_parser_expression_type(argument->expr);
                if (!actual) {
                    SourceLoc location;
                    cxx_parser_expr_loc(&location, argument->expr,
                                        &tmpl->func_def->loc);
                    if (report_errors) {
                        rcc_error(location,
                                  "cannot deduce function template pack type from an expression without a parser-known type");
                    }
                    return false;
                }
                if (*pack_count >= 32) {
                    if (report_errors) {
                        rcc_error(argument->expr->loc,
                                  "function template parameter pack exceeds compiler limits");
                    }
                    return false;
                }
                pack_arguments[(*pack_count)++] =
                    cxx_parser_template_deduction_argument(
                        parameter->decl->type, actual);
                argument = argument->next;
            }
            parameter = parameter->next;
            break;
        }
        Type* actual = cxx_parser_expression_type(argument->expr);
        if (!actual) {
            SourceLoc location;
            cxx_parser_expr_loc(&location, argument->expr,
                                &tmpl->func_def->loc);
            if (report_errors) {
                rcc_error(location,
                          "cannot deduce function template type from an expression "
                          "without a parser-known type");
            }
            return false;
        }
        if (!deduce_function_template_type(
                tmpl, parameter->decl->type,
                cxx_parser_template_deduction_argument(
                    parameter->decl->type, actual), template_arguments,
                template_values, template_value_present, specificity)) {
            if (report_errors) {
                rcc_error(argument->expr->loc,
                          "function template argument type does not match its "
                          "parameter pattern");
            }
            return false;
        }
        parameter = parameter->next;
        argument = argument->next;
    }
    if (argument) {
        SourceLoc location;
        cxx_parser_expr_loc(&location, argument->expr,
                            &tmpl->func_def->loc);
        if (report_errors) {
            rcc_error(location,
                      "too many arguments for function template deduction");
        }
        return false;
    }
    for (; parameter; parameter = parameter->next) {
        if (parameter->decl && parameter->decl->param_is_pack) continue;
        if (!parameter->decl->param_default) {
            if (report_errors) {
                rcc_error(tmpl->func_def->loc,
                          "too few arguments for function template deduction");
            }
            return false;
        }
    }
    return true;
}

static bool eval_template_integer_expression(Expr* expression,
                                              CxxTemplate* tmpl,
                                              const int64_t* values,
                                              const bool* value_present,
                                              int64_t* result) {
    int64_t left;
    int64_t right;
    if (!expression || !result) return false;
    if (expr_eval_integer_constant(expression, result)) return true;
    if (expression->kind == EXPR_SIZEOF && expression->sizeof_pack_name &&
        tmpl && tmpl->pending_pack_count >= 0) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->is_pack && parameter->name &&
                strcmp(parameter->name, expression->sizeof_pack_name) == 0) {
                *result = tmpl->pending_pack_count;
                return true;
            }
        }
    }
    if (expression->kind == EXPR_IDENT && tmpl && values && value_present) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->kind == TPARAM_NONTYPE && parameter->name &&
                value_present[index] &&
                strcmp(parameter->name, expression->ident_name) == 0) {
                *result = values[index];
                return true;
            }
        }
        return false;
    }
    switch (expression->kind) {
        case EXPR_NEG:
            if (!eval_template_integer_expression(
                    expression->unary_operand, tmpl, values, value_present,
                    &left)) return false;
            *result = -left;
            return true;
        case EXPR_NOT:
            if (!eval_template_integer_expression(
                    expression->unary_operand, tmpl, values, value_present,
                    &left)) return false;
            *result = !left;
            return true;
        case EXPR_BITNOT:
            if (!eval_template_integer_expression(
                    expression->unary_operand, tmpl, values, value_present,
                    &left)) return false;
            *result = ~left;
            return true;
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
        case EXPR_AND:
        case EXPR_OR:
            if (!eval_template_integer_expression(
                    expression->binary_lhs, tmpl, values, value_present,
                    &left) ||
                !eval_template_integer_expression(
                    expression->binary_rhs, tmpl, values, value_present,
                    &right)) return false;
            switch (expression->kind) {
                case EXPR_ADD: *result = left + right; break;
                case EXPR_SUB: *result = left - right; break;
                case EXPR_MUL: *result = left * right; break;
                case EXPR_DIV:
                    if (right == 0) return false;
                    *result = left / right;
                    break;
                case EXPR_MOD:
                    if (right == 0) return false;
                    *result = left % right;
                    break;
                case EXPR_BITAND: *result = left & right; break;
                case EXPR_BITOR: *result = left | right; break;
                case EXPR_BITXOR: *result = left ^ right; break;
                case EXPR_LSHIFT:
                    if (right < 0 || right >= 64) return false;
                    *result = (int64_t)((uint64_t)left << (unsigned)right);
                    break;
                case EXPR_RSHIFT:
                    if (right < 0 || right >= 64) return false;
                    *result = (int64_t)((uint64_t)left >> (unsigned)right);
                    break;
                case EXPR_EQ: *result = left == right; break;
                case EXPR_NE: *result = left != right; break;
                case EXPR_LT: *result = left < right; break;
                case EXPR_GT: *result = left > right; break;
                case EXPR_LE: *result = left <= right; break;
                case EXPR_GE: *result = left >= right; break;
                case EXPR_AND: *result = left && right; break;
                case EXPR_OR: *result = left || right; break;
                default: return false;
            }
            return true;
        case EXPR_COND:
            if (!eval_template_integer_expression(
                    expression->cond_test, tmpl, values, value_present,
                    &left)) return false;
            return eval_template_integer_expression(
                left ? expression->cond_then : expression->cond_else,
                tmpl, values, value_present, result);
        case EXPR_CAST:
            return eval_template_integer_expression(
                expression->cast_expr, tmpl, values, value_present, result);
        default:
            return false;
    }
}

static bool cxx_template_constraint_satisfied(CxxTemplate* tmpl,
                                               const int64_t* values,
                                               const bool* value_present,
                                               SourceLoc loc,
                                               bool report_errors) {
    int64_t result;
    if (!tmpl || !tmpl->constraint) return true;
    if (!eval_template_integer_expression(tmpl->constraint, tmpl, values,
                                          value_present, &result)) {
        if (report_errors) {
            rcc_error(loc,
                      "requires-clause must be an integral constant expression "
                      "over non-type template parameters");
        }
        return false;
    }
    if (!result) {
        if (report_errors) {
            rcc_error(loc, "template constraints are not satisfied");
        }
        return false;
    }
    return true;
}

typedef struct CxxParsedTemplateArgument {
    bool is_type;
    Type* type;
    int64_t value;
    bool value_valid;
} CxxParsedTemplateArgument;

typedef struct CxxFunctionTemplateMatch {
    CxxTemplate* tmpl;
    Type* arguments[32];
    Type* pack_arguments[32];
    int64_t pack_values[32];
    bool pack_value_present[32];
    int pack_count;
    int64_t values[32];
    bool value_present[32];
    int argument_count;
    int specificity;
    int conversion_total;
    int conversion_worst;
    Decl* instance;
} CxxFunctionTemplateMatch;

static int cxx_parser_type_pack_index(CxxTemplate* tmpl, Type* pattern) {
    if (!tmpl || !pattern || pattern->kind != TYPE_STRUCT ||
        !pattern->tag) return -1;
    for (int index = 0; index < tmpl->param_count; ++index) {
        TemplateParam* parameter = &tmpl->params[index];
        if (parameter->kind == TPARAM_TYPE && parameter->is_pack &&
            parameter->name && strcmp(parameter->name, pattern->tag) == 0) {
            return index;
        }
    }
    return -1;
}

static int cxx_parser_template_pack_parameter_index(CxxTemplate* tmpl) {
    if (!tmpl) return -1;
    for (int index = 0; index < tmpl->param_count; ++index) {
        if (tmpl->params[index].is_pack) return index;
    }
    return -1;
}

static bool cxx_parser_expression_is_lvalue(Expr* expression) {
    if (!expression) return false;
    switch (expression->kind) {
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

static int cxx_parser_template_conversion_rank(Expr* argument,
                                                Type* target) {
    Type* source;
    Type* source_base;
    Type* target_base;
    if (!argument || !target) return -1;
    source = cxx_parser_expression_type(argument);
    if (!source) return -1;
    if (source->is_reference) source = source->base;
    if (!source) return -1;

    if (target->is_reference) {
        bool is_lvalue = cxx_parser_expression_is_lvalue(argument);
        if ((!target->is_rvalue_reference && !is_lvalue) ||
            (target->is_rvalue_reference && is_lvalue) || !target->base) {
            return -1;
        }
        target = target->base;
    }
    if (type_is_compatible(source, target)) return 0;
    if (type_is_arithmetic(source) && type_is_arithmetic(target)) return 2;
    if (source->kind == TYPE_ARRAY && target->kind == TYPE_PTR) {
        source_base = source->base;
        target_base = target->base;
        if (source_base && target_base &&
            (type_is_compatible(source_base, target_base) ||
             target_base->kind == TYPE_VOID)) {
            return 1;
        }
    }
    if (source->kind == TYPE_FUNC && target->kind == TYPE_PTR &&
        target->base && target->base->kind == TYPE_FUNC) {
        return type_is_compatible(source, target->base) ? 1 : -1;
    }
    if (source->kind == TYPE_PTR && target->kind == TYPE_PTR) {
        source_base = source->base;
        target_base = target->base;
        if (source_base && target_base) {
            if (type_is_compatible(source_base, target_base)) return 1;
            if (source_base->kind == TYPE_VOID ||
                target_base->kind == TYPE_VOID) return 2;
        }
    }
    if (source->kind == TYPE_NULLPTR && target->kind == TYPE_PTR) return 1;
    if (source->kind == TYPE_INT && target->kind == TYPE_PTR &&
        argument->kind == EXPR_INT_LIT && argument->int_val == 0) {
        return 2;
    }
    return -1;
}

static bool cxx_function_template_instance_viable(
    Decl* instance, ExprList* call_arguments, int* total, int* worst) {
    TypeParam* parameter;
    DeclList* declaration;
    ExprList* argument;
    if (!instance || instance->kind != DECL_FUNC || !instance->type ||
        instance->type->kind != TYPE_FUNC) return false;
    parameter = instance->type->params;
    declaration = instance->func_params;
    argument = call_arguments;
    if (total) *total = 0;
    if (worst) *worst = 0;
    while (argument && parameter) {
        int rank = cxx_parser_template_conversion_rank(
            argument->expr, parameter->type);
        if (rank < 0) return false;
        if (total) *total += rank;
        if (worst && rank > *worst) *worst = rank;
        argument = argument->next;
        parameter = parameter->next;
        if (declaration) declaration = declaration->next;
    }
    if (argument) {
        if (!instance->type->variadic) return false;
        while (argument) {
            if (total) *total += 8;
            if (worst && *worst < 8) *worst = 8;
            argument = argument->next;
        }
    }
    while (parameter && declaration) {
        if (!declaration->decl || !declaration->decl->param_default) {
            return false;
        }
        parameter = parameter->next;
        declaration = declaration->next;
    }
    return parameter == NULL && declaration == NULL;
}

static bool prepare_cxx_function_template_match(
    CxxTemplate* tmpl, ExprList* call_arguments,
    const CxxParsedTemplateArgument* explicit_arguments,
    int explicit_argument_count, CxxFunctionTemplateMatch* match,
    bool* constraint_invalid) {
    int specificity = 0;
    bool has_type_pack = false;
    bool has_value_pack = false;
    if (!tmpl || !match || tmpl->kind != TMPL_FUNCTION || !tmpl->func_def ||
        tmpl->param_count < 0 || tmpl->param_count > 32) return false;
    memset(match, 0, sizeof(*match));
    match->tmpl = tmpl;
    match->argument_count = tmpl->param_count;
    for (int index = 0; index < tmpl->param_count; ++index) {
        if (tmpl->params[index].is_pack) {
            if (tmpl->params[index].kind == TPARAM_TYPE) has_type_pack = true;
            else has_value_pack = true;
        }
    }

    if (explicit_arguments) {
        int pack_parameter_index =
            cxx_parser_template_pack_parameter_index(tmpl);
        if (pack_parameter_index < 0 &&
            explicit_argument_count > tmpl->param_count) return false;
        for (int index = 0; index < explicit_argument_count; ++index) {
            int parameter_index = pack_parameter_index >= 0 &&
                                  index >= pack_parameter_index
                ? pack_parameter_index : index;
            TemplateParam* parameter;
            if (parameter_index >= tmpl->param_count) return false;
            parameter = &tmpl->params[parameter_index];
            if ((parameter->kind == TPARAM_TYPE) !=
                explicit_arguments[index].is_type) {
                return false;
            }
            if (parameter->is_pack) {
                if (match->pack_count >= 32) return false;
                if (parameter->kind == TPARAM_TYPE) {
                    if (!explicit_arguments[index].is_type ||
                        !explicit_arguments[index].type) return false;
                    match->pack_arguments[match->pack_count] =
                        explicit_arguments[index].type;
                } else {
                    if (explicit_arguments[index].is_type ||
                        !explicit_arguments[index].value_valid) return false;
                    match->pack_values[match->pack_count] =
                        explicit_arguments[index].value;
                    match->pack_value_present[match->pack_count] = true;
                }
                ++match->pack_count;
            } else if (parameter->kind == TPARAM_TYPE) {
                match->arguments[parameter_index] =
                    explicit_arguments[index].type;
                if (!match->arguments[parameter_index]) return false;
            } else {
                if (!explicit_arguments[index].value_valid) return false;
                match->arguments[parameter_index] = parameter->type;
                match->values[parameter_index] = explicit_arguments[index].value;
                match->value_present[parameter_index] = true;
            }
        }
        int fixed_explicit_count = pack_parameter_index >= 0
            ? pack_parameter_index : explicit_argument_count;
        for (int index = fixed_explicit_count;
             index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->is_pack) continue;
            if (!parameter->has_default) return false;
            if (parameter->kind == TPARAM_TYPE) {
                if (!parameter->default_type) return false;
                match->arguments[index] = substitute_template_type(
                    tmpl, parameter->default_type, match->arguments,
                    tmpl->param_count, match->values, match->value_present);
            } else {
                if (!parameter->default_value ||
                    !eval_template_integer_expression(
                        parameter->default_value, tmpl, match->values,
                        match->value_present, &match->values[index])) {
                    return false;
                }
                match->arguments[index] = parameter->type;
                match->value_present[index] = true;
            }
        }
        for (DeclList* parameter = tmpl->func_def->func_params;
             parameter; parameter = parameter->next) {
            /* Explicit template arguments still have to make each dependent
             * parameter pattern viable.  This also supplies the partial-
             * ordering score for `T` versus `T*`. */
            ExprList* argument = call_arguments;
            int index = parameter->decl ? parameter->decl->param_index : 0;
            while (argument && index-- > 0) argument = argument->next;
            if (parameter->decl && parameter->decl->param_is_pack) {
                continue;
            }
            if (argument) {
                Type* actual = cxx_parser_expression_type(argument->expr);
                if (!actual || !deduce_function_template_type(
                        tmpl, parameter->decl->type,
                        cxx_parser_template_deduction_argument(
                            parameter->decl->type, actual),
                        match->arguments, match->values,
                        match->value_present, &specificity)) {
                    return false;
                }
            }
        }
        for (int index = 0; index < tmpl->param_count; ++index) {
            if (tmpl->params[index].is_pack) {
                if (tmpl->params[index].kind == TPARAM_TYPE) {
                    match->arguments[index] = match->pack_count > 0
                        ? match->pack_arguments[0] : type_void;
                } else {
                    match->arguments[index] = tmpl->params[index].type;
                }
            }
        }
    } else {
        if (!deduce_function_template_arguments(
                tmpl, call_arguments, match->arguments, match->values,
                match->value_present, &specificity, false,
                match->pack_arguments, &match->pack_count)) {
            return false;
        }
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->is_pack) {
                if (match->pack_count > 0) {
                    match->arguments[index] = match->pack_arguments[0];
                } else {
                    /* The argument slot is only a cache key placeholder;
                     * the expanded parameter list carries the actual ABI. */
                    match->arguments[index] = type_void;
                }
            } else if (parameter->kind == TPARAM_NONTYPE) {
                if (!match->value_present[index]) return false;
                match->arguments[index] = parameter->type;
            } else if (!match->arguments[index] && parameter->has_default &&
                       parameter->default_type) {
                match->arguments[index] = substitute_template_type(
                    tmpl, parameter->default_type, match->arguments,
                    tmpl->param_count, match->values, match->value_present);
            }
            if (!match->arguments[index]) return false;
        }
    }

    if (!cxx_template_constraint_satisfied(
            tmpl, match->values, match->value_present, tmpl->func_def->loc,
            false)) {
        if (constraint_invalid) *constraint_invalid = true;
        return false;
    }
    match->specificity = specificity;
    tmpl->pending_pack_args = has_type_pack ? match->pack_arguments : NULL;
    tmpl->pending_pack_values = has_value_pack ? match->pack_values : NULL;
    tmpl->pending_pack_value_present = has_value_pack
        ? match->pack_value_present : NULL;
    tmpl->pending_pack_count = has_type_pack ? match->pack_count : -1;
    if (has_value_pack) tmpl->pending_pack_count = match->pack_count;
    match->instance = (Decl*)cxx_template_instantiate_with_values(
        tmpl, match->arguments, match->values, match->value_present,
        match->argument_count);
    tmpl->pending_pack_args = NULL;
    tmpl->pending_pack_values = NULL;
    tmpl->pending_pack_value_present = NULL;
    tmpl->pending_pack_count = -1;
    if (!cxx_function_template_instance_viable(
            match->instance, call_arguments, &match->conversion_total,
            &match->conversion_worst)) {
        return false;
    }
    return true;
}

Type* rcc_parse_cxx_direct_list_type(void) {
    Token* saved_cur = parser.cur;
    Token* saved_prev = parser.prev;
    SourceLoc loc = peek()->loc;
    const char* name;
    CxxTemplate* tmpl;
    Type* type;

    if (!check(TOK_IDENT) && !check(TOK_SCOPE)) return NULL;
    name = parse_qualified_name();
    tmpl = check(TOK_LT) ? find_class_template(name) : NULL;
    if (!tmpl) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    type = parse_class_template_specialization(tmpl, loc);
    if (!check(TOK_LBRACE) ||
        rcc_parser_cxx_constructor_arity_mask(type) == 0u) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    return type;
}

Expr* rcc_parse_cxx_functional_cast(void) {
    Token* saved_cur = parser.cur;
    Token* saved_prev = parser.prev;
    SourceLoc loc = peek()->loc;
    const char* name = NULL;
    CxxTemplate* tmpl;
    CxxClass* cls = NULL;
    Type* type;
    ExprList* arguments = NULL;
    Expr* initializer;
    bool keyword_type = false;
    bool brace_form = false;

    switch (peek()->type) {
        case TOK_VOID:
        case TOK_BOOL:
        case TOK_CHAR:
        case TOK_SHORT:
        case TOK_INT:
        case TOK_LONG:
        case TOK_SIGNED:
        case TOK_UNSIGNED:
        case TOK_FLOAT:
        case TOK_DOUBLE:
        case TOK_CONST:
        case TOK_VOLATILE:
        case TOK_DECLTYPE:
            keyword_type = true;
            break;
        default:
            break;
    }

    if (keyword_type) {
        type = parse_cxx_type_spec();
    } else {
        if (!check(TOK_IDENT) && !check(TOK_SCOPE)) return NULL;
        name = parse_qualified_name();
        tmpl = check(TOK_LT) ? find_class_template(name) : NULL;
        if (tmpl) {
            type = parse_class_template_specialization(tmpl, loc);
        } else {
            cls = find_class(name);
            type = cls ? cls->type : NULL;
        }
        if (!type) {
            /* Classes registered through the common aggregate path are also
             * visible in the parser type table.  The cxx_class guard keeps a
             * C aggregate or typedef from becoming a constructor expression. */
            type = rcc_parser_lookup_type(name);
            if (!type || ((type->kind == TYPE_STRUCT ||
                           type->kind == TYPE_UNION) && !type->cxx_class)) {
                type = NULL;
            }
        }
    }
    if (!type || (!check(TOK_LPAREN) && !check(TOK_LBRACE)) ||
        (type->cxx_class &&
         rcc_parser_cxx_constructor_arity_mask(type) == 0u)) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }

    brace_form = match(TOK_LBRACE);
    if (!brace_form) advance(); /* `(` */
    if ((!brace_form && !check(TOK_RPAREN)) ||
        (brace_form && !check(TOK_RBRACE))) {
        do {
            exprlist_append(&arguments, parse_assignment_expression());
        } while (match(TOK_COMMA));
    }
    expect(brace_form ? TOK_RBRACE : TOK_RPAREN,
           brace_form ? "}" : ")");

    if (!type->cxx_class) {
        if (!arguments) {
            arguments = exprlist_new(expr_int(0, loc));
        } else if (arguments->next) {
            rcc_error(loc,
                      "C++ functional scalar cast requires one argument");
        }
        initializer = expr_cast(type, arguments->expr, loc);
        return initializer;
    }

    initializer = expr_initializer_list(arguments, loc);
    initializer->compound_type = type;
    initializer->compound_value_init = arguments == NULL;
    rcc_parser_validate_cxx_constructor_initializer(type, initializer);
    return initializer;
}

static TypeField* versioned_public_integer_field(Type* type,
                                                const char* name) {
    TypeField* field;
    if (!type || !name) return NULL;
    for (field = type->fields; field; field = field->next) {
        if (field->name && strcmp(field->name, name) == 0) {
            if (field->cxx_access != 0 || !field->type ||
                (!type_is_integer(field->type) &&
                 field->type->kind != TYPE_ENUM)) {
                return NULL;
            }
            return field;
        }
    }
    return NULL;
}

/* A function template that constructs a dependent aggregate and then writes
 * its members needs a dedicated typed lowering.  Do not instantiate such a
 * body through the scalar function-template path: that would emit a partial
 * object while silently dropping the remaining member writes. */
static bool template_has_unsupported_versioned_shape(CxxTemplate* tmpl) {
    Decl* function;
    StmtList* statements;
    const char* variable;
    bool has_member_assignment = false;
    if (!tmpl || tmpl->kind != TMPL_FUNCTION || tmpl->param_count != 1 ||
        tmpl->params[0].kind != TPARAM_TYPE) {
        return false;
    }
    function = tmpl->func_def;
    if (!function || function->func_params || !function->type ||
        !template_type_parameter_matches(tmpl, function->type->ret_type, 0) ||
        !function->func_body || function->func_body->kind != STMT_BLOCK) {
        return false;
    }
    statements = function->func_body->block_stmts;
    if (!statements || !statements->stmt ||
        statements->stmt->kind != STMT_DECL || !statements->stmt->decl ||
        !statements->stmt->decl->name ||
        !template_type_parameter_matches(tmpl,
                                          statements->stmt->decl->type, 0) ||
        !statements->stmt->decl->var_init ||
        statements->stmt->decl->var_init->kind != EXPR_COMPOUND ||
        !statements->stmt->decl->var_init->compound_value_init) {
        return false;
    }
    variable = statements->stmt->decl->name;
    for (statements = statements->next; statements;
         statements = statements->next) {
        Stmt* statement = statements->stmt;
        if (statement && statement->kind == STMT_EXPR && statement->expr &&
            statement->expr->kind == EXPR_ASSIGN &&
            statement->expr->binary_lhs &&
            statement->expr->binary_lhs->kind == EXPR_MEMBER &&
            expression_is_identifier(
                statement->expr->binary_lhs->member_base, variable) &&
            statement->expr->binary_lhs->member_name &&
            strcmp(statement->expr->binary_lhs->member_name, "struct_size") != 0 &&
            strcmp(statement->expr->binary_lhs->member_name, "version") != 0) {
            has_member_assignment = true;
            break;
        }
    }
    return has_member_assignment;
}

Expr* rcc_parse_cxx_template_call(void) {
    Token* saved_cur = parser.cur;
    Token* saved_prev = parser.prev;
    SourceLoc loc = peek()->loc;
    const char* name;
    CxxTemplate* tmpl;
    CxxTemplate* candidate_templates[32];
    int candidate_count;
    Type* argument;
    TypeField* size_field;
    TypeField* version_field;
    ExprList* items = NULL;
    Expr* initializer;
    bool unsafe_versioned_shape;

    if (!check(TOK_IDENT) && !check(TOK_SCOPE)) return NULL;
    name = parse_qualified_name();
    candidate_count = find_function_template_candidates(
        name, candidate_templates,
        (int)(sizeof(candidate_templates) / sizeof(candidate_templates[0])));
    if (candidate_count <= 0) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    if (candidate_count > (int)(sizeof(candidate_templates) /
                                sizeof(candidate_templates[0]))) {
        rcc_error(loc, "too many function template overloads for '%s'", name);
        candidate_count = (int)(sizeof(candidate_templates) /
                                sizeof(candidate_templates[0]));
    }
    tmpl = candidate_templates[0];
    unsafe_versioned_shape = template_has_unsupported_versioned_shape(tmpl);
    if (candidate_count != 1 ||
        tmpl->function_lowering != TMPL_FUNCTION_VERSIONED_STRUCT) {
        CxxParsedTemplateArgument explicit_arguments[32];
        int explicit_argument_count = 0;
        ExprList* call_arguments = NULL;
        Expr* function;
        bool explicit_template_arguments = check(TOK_LT);
        int pack_parameter_index =
            cxx_parser_template_pack_parameter_index(tmpl);

        if (explicit_template_arguments) {
            expect(TOK_LT, "<");
            if (!check(TOK_GT)) {
                do {
                    int parameter_index = pack_parameter_index >= 0 &&
                                          explicit_argument_count >=
                                              pack_parameter_index
                        ? pack_parameter_index : explicit_argument_count;
                    TemplateParam* parameter = parameter_index <
                        tmpl->param_count ? &tmpl->params[parameter_index] : NULL;
                    CxxParsedTemplateArgument* parsed;
                    if (explicit_argument_count >=
                        (int)(sizeof(explicit_arguments) /
                              sizeof(explicit_arguments[0]))) {
                        rcc_error(loc, "function template argument limit exceeded");
                        while (!check(TOK_GT) && !at_end()) advance();
                        break;
                    }
                    parsed = &explicit_arguments[explicit_argument_count++];
                    memset(parsed, 0, sizeof(*parsed));
                    if (parameter && parameter->kind == TPARAM_TYPE) {
                        parsed->is_type = true;
                        parsed->type = parse_cxx_type_spec();
                    } else if (parameter && parameter->kind == TPARAM_NONTYPE) {
                        Expr* value_expression;
                        int64_t value;
                        rcc_parser_set_cxx_template_default_mode(true);
                        value_expression = parse_assignment_expression();
                        rcc_parser_set_cxx_template_default_mode(false);
                        if (!expr_eval_integer_constant(value_expression, &value)) {
                            SourceLoc error_location;
                            cxx_parser_expr_loc(&error_location,
                                                value_expression, &loc);
                            rcc_error(error_location,
                                      "function template non-type argument must be "
                                      "an integer constant expression");
                        } else {
                            parsed->value = value;
                            parsed->value_valid = true;
                        }
                    } else {
                        rcc_error(peek()->loc,
                                  "too many function template arguments");
                        while (!check(TOK_COMMA) && !check(TOK_GT) &&
                               !at_end()) {
                            advance();
                        }
                    }
                } while (match(TOK_COMMA));
            }
        } else if (!check(TOK_LPAREN)) {
            parser.cur = saved_cur;
            parser.prev = saved_prev;
            return NULL;
        }
        if (explicit_template_arguments) expect(TOK_GT, ">");
        if (!match(TOK_LPAREN)) {
            rcc_error(loc, "function template specialization must be called");
            return expr_int(0, loc);
        }
        if (!check(TOK_RPAREN)) {
            do {
                exprlist_append(&call_arguments, parse_assignment_expression());
            } while (match(TOK_COMMA));
        }
        expect(TOK_RPAREN, ")");

        if (unsafe_versioned_shape && candidate_count == 1) {
            rcc_error(loc, "function template '%s' is not safely lowerable",
                      name);
            return expr_int(0, loc);
        }

        CxxFunctionTemplateMatch matches[32];
        int match_count = 0;
        bool constraint_invalid = false;
        for (int index = 0; index < candidate_count; ++index) {
            if (template_has_unsupported_versioned_shape(
                    candidate_templates[index])) {
                continue;
            }
            if (prepare_cxx_function_template_match(
                    candidate_templates[index], call_arguments,
                    explicit_template_arguments ? explicit_arguments : NULL,
                    explicit_argument_count, &matches[match_count],
                    &constraint_invalid)) {
                ++match_count;
            }
        }
        if (match_count == 0) {
            if (constraint_invalid) {
                rcc_error(loc, "template constraints are not satisfied");
            } else if (unsafe_versioned_shape && candidate_count == 1) {
                rcc_error(loc, "function template '%s' is not safely lowerable",
                          name);
            } else if (!explicit_template_arguments) {
                /* Preserve the useful single-template diagnostics for the
                 * common failure cases, while overload sets receive one
                 * consolidated error after every candidate was examined. */
                Type* diagnostic_arguments[32] = { NULL };
                int64_t diagnostic_values[32] = { 0 };
                bool diagnostic_present[32] = { false };
                Type* diagnostic_pack[32] = { NULL };
                int diagnostic_pack_count = 0;
                (void)deduce_function_template_arguments(
                    tmpl, call_arguments, diagnostic_arguments,
                    diagnostic_values, diagnostic_present, NULL, true,
                    diagnostic_pack, &diagnostic_pack_count);
                rcc_error(loc, "no matching function template overload for '%s'",
                          name);
            } else {
                rcc_error(loc, "no matching function template overload for '%s'",
                          name);
            }
            return expr_int(0, loc);
        }
        int selected = 0;
        bool ambiguous = false;
        for (int index = 1; index < match_count; ++index) {
            CxxFunctionTemplateMatch* best = &matches[selected];
            CxxFunctionTemplateMatch* candidate = &matches[index];
            if (candidate->specificity > best->specificity ||
                (candidate->specificity == best->specificity &&
                 (candidate->conversion_worst < best->conversion_worst ||
                  (candidate->conversion_worst == best->conversion_worst &&
                   candidate->conversion_total < best->conversion_total)))) {
                selected = index;
                ambiguous = false;
            } else if (candidate->specificity == best->specificity &&
                       candidate->conversion_worst == best->conversion_worst &&
                       candidate->conversion_total == best->conversion_total) {
                ambiguous = true;
            }
        }
        if (ambiguous) {
            rcc_error(loc, "ambiguous function template overload for '%s'",
                      name);
            return expr_int(0, loc);
        }
        Decl* instance = matches[selected].instance;
        if (!instance || instance->kind != DECL_FUNC) {
            rcc_error(loc, "could not instantiate function template '%s'", name);
            return expr_int(0, loc);
        }
        if (active_ast) {
            bool present = false;
            for (DeclList* item = active_ast->decls; item; item = item->next) {
                if (item->decl == instance) {
                    present = true;
                    break;
                }
            }
            if (!present) ast_add_decl(active_ast, instance);
        }
        function = expr_ident(instance->name, loc);
        function->ident_decl = instance;
        function->type = instance->type;
        return expr_call(function, call_arguments, loc);
    }

    expect(TOK_LT, "<");
    argument = parse_cxx_type_spec();
    if (match(TOK_COMMA)) {
        rcc_error(loc, "versioned structure template requires one type argument");
        while (!check(TOK_GT) && !at_end()) advance();
    }
    expect(TOK_GT, ">");
    if (!match(TOK_LPAREN)) {
        rcc_error(loc, "versioned structure specialization must be called");
        return expr_int(0, loc);
    }
    if (!check(TOK_RPAREN)) {
        rcc_error(loc, "versioned structure template takes no arguments");
        while (!check(TOK_RPAREN) && !at_end()) advance();
    }
    expect(TOK_RPAREN, ")");

    if (!argument || argument->kind != TYPE_STRUCT ||
        !type_is_complete(argument) || argument->size <= 0) {
        rcc_error(loc,
                  "versioned structure template requires a complete public "
                  "struct with integer struct_size and version fields");
        return expr_int(0, loc);
    }
    size_field = versioned_public_integer_field(argument, "struct_size");
    version_field = versioned_public_integer_field(argument, "version");
    if (!size_field || !version_field || size_field == version_field) {
        rcc_error(loc,
                  "versioned structure template requires a complete public "
                  "struct with integer struct_size and version fields");
        return expr_int(0, loc);
    }

    exprlist_append_designated(&items, expr_int(argument->size, loc),
                               INIT_DESIGNATOR_FIELD, 0, "struct_size");
    exprlist_append_designated(&items,
                               expr_int(tmpl->function_constant, loc),
                               INIT_DESIGNATOR_FIELD, 0, "version");
    initializer = expr_initializer_list(items, loc);
    initializer->compound_type = argument;
    initializer->type = argument;
    return initializer;
}

static Type* cxx_decltype_member_type(Type* object_type,
                                      const char* member_name,
                                      SourceLoc loc) {
    TypeField* field;
    if (!object_type || !member_name) return NULL;
    if (object_type->kind == TYPE_PTR && object_type->is_reference) {
        object_type = object_type->base;
    }
    if (object_type->kind == TYPE_PTR) object_type = object_type->base;
    if (!object_type || (object_type->kind != TYPE_STRUCT &&
                         object_type->kind != TYPE_UNION)) {
        rcc_error(loc, "decltype member expression requires an aggregate object");
        return NULL;
    }
    for (field = object_type->fields; field; field = field->next) {
        if (field->name && strcmp(field->name, member_name) == 0) {
            if (field->cxx_access != ACCESS_PUBLIC) {
                rcc_error(loc, "decltype cannot name a non-public member '%s'",
                          member_name);
                return NULL;
            }
            return field->type;
        }
    }
    rcc_error(loc, "unknown member '%s' in decltype expression", member_name);
    return NULL;
}

static void cxx_skip_decltype_expression(void) {
    int depth = 0;
    while (!at_end()) {
        if (check(TOK_LPAREN)) {
            ++depth;
            advance();
        } else if (check(TOK_RPAREN)) {
            if (depth == 0) {
                advance();
                return;
            }
            --depth;
            advance();
        } else {
            advance();
        }
    }
}

static void cxx_skip_decltype_call(void) {
    int depth = 0;
    if (!match(TOK_LPAREN)) return;
    while (!at_end()) {
        if (check(TOK_LPAREN)) {
            ++depth;
        } else if (check(TOK_RPAREN)) {
            if (depth == 0) {
                advance();
                return;
            }
            --depth;
        }
        advance();
    }
}

static Type* cxx_decltype_function_return(const char* name, SourceLoc loc) {
    Type* result = NULL;
    const char* suffix = name ? strrchr(name, ':') : NULL;
    suffix = suffix && suffix > name && suffix[-1] == ':' ? suffix + 1 : name;
    for (DeclList* item = active_ast ? active_ast->decls : NULL;
         item; item = item->next) {
        Decl* declaration = item->decl;
        bool matches;
        if (!declaration || declaration->kind != DECL_FUNC ||
            !declaration->type || declaration->type->kind != TYPE_FUNC) {
            continue;
        }
        matches = name && declaration->name &&
            (strcmp(declaration->name, name) == 0 ||
             strcmp(declaration->name, suffix) == 0);
        if (!matches) continue;
        if (result && !type_is_compatible(result,
                                          declaration->type->ret_type)) {
            rcc_error(loc,
                      "decltype call names overloaded functions with different return types");
            return NULL;
        }
        result = declaration->type->ret_type;
    }
    return result;
}

/* Parse the expression forms for which the parser already has an exact
 * source-level type.  `decltype` is intentionally not an integer fallback:
 * an unsupported dependent or side-effecting expression is diagnosed at its
 * grammar boundary rather than being assigned a guessed type. */
static Type* parse_cxx_decltype_type(SourceLoc loc) {
    Type* result = NULL;
    bool extra_parentheses = false;
    bool valid = true;
    bool dereference = false;
    bool address = false;
    bool expression_is_lvalue = false;
    bool needs_lvalue_reference = false;

    expect(TOK_DECLTYPE, "decltype");
    expect(TOK_LPAREN, "(");
    if (match(TOK_LPAREN)) extra_parentheses = true;
    if (match(TOK_STAR)) {
        dereference = true;
    } else if (match(TOK_AMP)) {
        address = true;
    }

    if (check(TOK_IDENT)) {
        const char* name = advance()->value.str_val;
        result = cxx_parser_value_type(name);
        if (!result && check(TOK_LPAREN)) {
            result = cxx_decltype_function_return(name, loc);
        }
        if (!result) {
            Type* named_type = rcc_parser_lookup_type(name);
            if (named_type) {
                rcc_error(loc,
                          "decltype requires an expression, not a type name");
            } else {
                rcc_error(loc, "unknown identifier '%s' in decltype expression",
                          name);
            }
            valid = false;
        }
        expression_is_lvalue = result != NULL;
        if (result && check(TOK_LPAREN)) {
            Type* function_type = result;
            if (function_type->kind == TYPE_PTR && function_type->base) {
                function_type = function_type->base;
            }
            if (function_type->kind != TYPE_FUNC) {
                function_type = cxx_decltype_function_return(name, loc);
            } else {
                function_type = function_type->ret_type;
            }
            cxx_skip_decltype_call();
            result = function_type;
            expression_is_lvalue = false;
            if (!result) {
                rcc_error(loc, "unknown function '%s' in decltype expression",
                          name);
                valid = false;
            }
        }
        if (result && (check(TOK_DOT) || check(TOK_ARROW))) {
            bool through_pointer = match(TOK_ARROW);
            if (!through_pointer) expect(TOK_DOT, ".");
            if (!check(TOK_IDENT)) {
                rcc_error(peek()->loc, "expected member name in decltype expression");
                valid = false;
            } else {
                const char* member_name = advance()->value.str_val;
                if (through_pointer && result->kind != TYPE_PTR) {
                    rcc_error(loc,
                              "decltype '->' expression requires a pointer object");
                    valid = false;
                } else {
                    result = cxx_decltype_member_type(result, member_name, loc);
                    if (!result) valid = false;
                    expression_is_lvalue = result != NULL;
                    needs_lvalue_reference = expression_is_lvalue;
                }
            }
        }
    } else if (match(TOK_INT_LIT) || match(TOK_CHAR_LIT)) {
        result = type_int;
    } else if (match(TOK_FLOAT_LIT)) {
        result = previous()->float_suffix ? type_float : type_double;
    } else if (match(TOK_TRUE) || match(TOK_FALSE)) {
        result = type_bool;
    } else if (match(TOK_NULLPTR)) {
        result = type_nullptr;
    } else {
        rcc_error(loc,
                  "unsupported expression in decltype; expected a simple value expression");
        valid = false;
    }

    if (extra_parentheses) {
        if (!check(TOK_RPAREN)) {
            rcc_error(peek()->loc, "expected ')' in decltype expression");
            valid = false;
            cxx_skip_decltype_expression();
        } else {
            advance();
        }
    } else if (!check(TOK_RPAREN)) {
        rcc_error(peek()->loc,
                  "unsupported operator in decltype expression");
        valid = false;
        cxx_skip_decltype_expression();
    }
    if (extra_parentheses && !check(TOK_RPAREN)) {
        cxx_skip_decltype_expression();
    } else {
        expect(TOK_RPAREN, ")");
    }

    if (!valid || !result) return type_int;
    if (address) return type_ptr(result);
    if (dereference) {
        if (result->kind != TYPE_PTR || !result->base) {
            rcc_error(loc, "decltype dereference requires a pointer expression");
            return type_int;
        }
        result = result->base;
        expression_is_lvalue = true;
        needs_lvalue_reference = true;
    }
    if ((extra_parentheses || needs_lvalue_reference) &&
        expression_is_lvalue) {
        Type* reference;
        if (result->kind == TYPE_PTR && result->is_reference) {
            result = result->base;
        }
        reference = type_ptr(result);
        reference->is_reference = true;
        return reference;
    }
    return result;
}

static Type* parse_cxx_type_spec(void) {
    SourceLoc loc = peek()->loc;
    Type* t = NULL;
    bool is_unsigned = false;
    bool is_const = false;
    bool is_volatile = false;
    bool saw_sign = false;
    int long_count = 0;
    bool is_short = false;

    /* Qualifiers */
    while (1) {
        if (match(TOK_CONST)) is_const = true;
        else if (match(TOK_VOLATILE)) is_volatile = true;
        else if (match(TOK_UNSIGNED)) {
            is_unsigned = true;
            saw_sign = true;
        } else if (match(TOK_SIGNED)) {
            is_unsigned = false;
            saw_sign = true;
        } else if (match(TOK_LONG)) {
            ++long_count;
        }
        else if (match(TOK_SHORT)) is_short = true;
        else break;
    }

    /* Base type */
    if (check(TOK_DECLTYPE)) {
        t = parse_cxx_decltype_type(loc);
    } else if (match(TOK_VOID)) {
        t = type_void;
    } else if (match(TOK_BOOL)) {
        t = type_bool;
    } else if (match(TOK_CHAR)) {
        t = is_unsigned ? type_uchar : type_char;
    } else if (match(TOK_INT) || long_count > 0 || is_short || saw_sign) {
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
    } else if (match(TOK_CLASS) || match(TOK_STRUCT)) {
        const char* name = parse_qualified_name();
        CxxClass* known_class = find_class(name);
        if (!known_class && active_class && active_class->name &&
            strcmp(name, active_class->name) == 0) {
            known_class = active_class;
        }
        t = known_class ? known_class->type : NULL;
        if (!t) {
            rcc_error(loc, "unknown C++ class type '%s'", name);
            t = type_int;
        }
    } else if (check(TOK_IDENT) || check(TOK_SCOPE)) {
        /* Class or namespace qualified type */
        const char* name = parse_qualified_name();
        CxxTemplate* tmpl = check(TOK_LT)
            ? find_class_template(name) : NULL;
        int template_template_index =
            active_template_template_parameter_index(name);
        CxxClass* known_class = find_class(name);
        if (!known_class && active_class && active_class->name &&
            strcmp(name, active_class->name) == 0) {
            known_class = active_class;
        }
        Type* known_type = strstr(name, "::") == NULL
            ? rcc_parser_lookup_type(name) : NULL;
        if (template_template_index >= 0 && check(TOK_LT)) {
            TemplateParam* parameter = &active_template->params[
                template_template_index];
            Type* dependent = type_struct(name);
            int nested_count = 0;
            dependent->cxx_dependent = true;
            dependent->cxx_template_param_index = template_template_index;
            expect(TOK_LT, "template-template argument list");
            if (!check(TOK_GT)) {
                do {
                    if (nested_count >= 32 || !parameter->template_signature ||
                        nested_count >=
                            parameter->template_signature->param_count) {
                        rcc_error(peek()->loc,
                                  "template-template argument count exceeded");
                        while (!check(TOK_COMMA) && !check(TOK_GT) &&
                               !at_end()) {
                            advance();
                        }
                        if (check(TOK_COMMA)) advance();
                        continue;
                    }
                    if (parameter->template_signature->params[nested_count].kind !=
                        TPARAM_TYPE) {
                        rcc_error(peek()->loc,
                                  "non-type template-template arguments are "
                                  "not supported in dependent class types");
                        (void)parse_assignment_expression();
                        dependent->cxx_template_args = ast_arena_grow(
                            dependent->cxx_template_args,
                            sizeof(Type*) * (size_t)nested_count,
                            sizeof(Type*) * (size_t)(nested_count + 1));
                        dependent->cxx_template_args[nested_count++] = type_int;
                    } else {
                        dependent->cxx_template_args = ast_arena_grow(
                            dependent->cxx_template_args,
                            sizeof(Type*) * (size_t)nested_count,
                            sizeof(Type*) * (size_t)(nested_count + 1));
                        dependent->cxx_template_args[nested_count++] =
                            parse_cxx_type_spec();
                    }
                } while (match(TOK_COMMA));
            }
            expect(TOK_GT, ">");
            dependent->cxx_template_arg_count = nested_count;
            if (!parameter->template_signature ||
                nested_count != parameter->template_signature->param_count) {
                rcc_error(loc,
                          "template-template argument count does not match "
                          "its parameter list");
            }
            t = dependent;
        } else if (tmpl) {
            t = parse_class_template_specialization(tmpl, loc);
        } else if (known_class && active_template &&
                   active_class == known_class) {
            /* A self-reference in a class template is dependent on the
             * specialization even when the primary class is already visible
             * in the parser's class table. */
            t = type_struct(name);
            t->cxx_dependent = true;
        } else if (known_class) {
            t = known_class->type;
        } else if (known_type) {
            t = known_type;
        } else if (is_active_template_type(name)) {
            /* A dependent type remains an incomplete placeholder until
             * template substitution. */
            t = type_struct(name);
            t->cxx_dependent = true;
        } else {
            if (check(TOK_LT)) {
                rcc_error(loc, "unknown C++ class template '%s'", name);
                skip_cxx_template_arguments();
            } else {
                rcc_error(loc, "unknown C++ type name '%s'", name);
            }
            t = type_int;
        }
    } else {
        rcc_error(loc, "expected C++ type specifier, got '%s'",
                  token_type_str(peek()->type));
        t = is_unsigned ? type_uint : type_int;
    }

    if (!t && (saw_sign || long_count > 0 || is_short)) {
        t = is_unsigned ? type_uint : type_int;
    }

    /* Prefix cv-qualifiers apply to the base type, before pointer and
     * reference declarators are layered on top. */
    if ((is_const || is_volatile) && t) {
        Type* ct = ast_arena_alloc(sizeof(Type));
        *ct = *t;
        ct->is_const = ct->is_const || is_const;
        ct->is_volatile = ct->is_volatile || is_volatile;
        t = ct;
    }

    /* Reference and pointer */
    while (1) {
        if (match(TOK_STAR)) {
            t = type_ptr(t);
            while (match(TOK_CONST)) t->is_const = true;
        } else if (match(TOK_AMP)) {
            t = type_ptr(t);
            t->is_reference = true;
        } else if (match(TOK_AND)) {
            t = type_ptr(t);
            t->is_reference = true;
            t->is_rvalue_reference = true;
        } else {
            break;
        }
    }

    return t;
}

Expr* rcc_parse_cxx_special_expression(void) {
    SourceLoc loc = peek()->loc;

    if (match(TOK_NEW)) {
        Type* object_type = parse_cxx_type_spec();
        Expr* count = NULL;
        Expr* bytes;
        Expr* allocation;
        ExprList* new_args = NULL;
        ExprList* allocation_args = NULL;
        bool is_array = false;
        bool value_init = false;
        bool brace_init = false;

        if (match(TOK_LBRACKET)) {
            is_array = true;
            if (check(TOK_RBRACKET)) {
                rcc_error(loc, "array new requires an element count");
            } else {
                count = parse_expression();
            }
            expect(TOK_RBRACKET, "]");
        }
        if (match(TOK_LPAREN)) {
            value_init = check(TOK_RPAREN);
            while (!check(TOK_RPAREN) && !at_end()) {
                exprlist_append(&new_args, parse_assignment_expression());
                if (!match(TOK_COMMA)) break;
            }
            expect(TOK_RPAREN, ")");
        } else if (match(TOK_LBRACE)) {
            brace_init = true;
            value_init = check(TOK_RBRACE);
            while (!check(TOK_RBRACE) && !at_end()) {
                exprlist_append(&new_args, parse_assignment_expression());
                if (!match(TOK_COMMA)) break;
            }
            expect(TOK_RBRACE, "}");
        }
        if (!object_type || object_type == type_void ||
            object_type->kind == TYPE_FUNC ||
            !type_is_complete(object_type)) {
            rcc_error(loc, "new requires a complete object type");
        }
        bytes = expr_sizeof_type(object_type, loc);
        if (is_array && count) {
            bytes = expr_binary(EXPR_MUL, bytes, count, loc);
        }
        exprlist_append(&allocation_args, bytes);
        allocation = expr_call(expr_ident("rin_malloc", loc),
                               allocation_args, loc);
        allocation->call_is_new = true;
        allocation->call_new_value_init = value_init;
        allocation->call_new_is_array = is_array;
        allocation->call_new_brace_init = brace_init;
        allocation->call_new_type = object_type;
        allocation->call_new_count = count;
        /* The allocation size is the ordinary call argument.  Keep the
         * constructor arguments separate so they are evaluated exactly once
         * after the allocator returns the object address. */
        allocation->call_new_args = new_args;
        return allocation;
    }

    if (match(TOK_DELETE)) {
        Expr* operand;
        ExprList* arguments = NULL;
        bool is_array = match(TOK_LBRACKET);
        if (is_array) expect(TOK_RBRACKET, "]");
        operand = parse_expression();
        if (!operand) return expr_call(
            expr_ident("rin_free", loc), NULL, loc);
        exprlist_append(&arguments, operand);
        /* Both scalar and array allocation use the RinOS allocator.  Semantic
         * analysis attaches a destructor only after proving the complete
         * cleanup shape; unsupported object lifetimes remain diagnostics
         * instead of silently becoming free-only calls. */
        Expr* result = expr_call(expr_ident("rin_free", loc), arguments, loc);
        result->call_is_delete = true;
        result->call_delete_is_array = is_array;
        return result;
    }

    rcc_error(loc, "internal C++ special-expression parser entry");
    return expr_int(0, loc);
}

/* Parse a static member selected through a class-template specialization,
 * such as `Counter<int>::value`.  The common qualified-name parser cannot
 * consume the angle-bracket portion, so keep this narrow hook transactional:
 * ordinary comparisons, namespace names, and unsupported members are left
 * for the normal expression parser without inventing a fallback value. */
Expr* rcc_parse_cxx_qualified_template_member(void) {
    Token* saved_cur = parser.cur;
    Token* saved_prev = parser.prev;
    SourceLoc loc = peek()->loc;
    const char* name;
    CxxTemplate* tmpl;
    Type* type;
    const char* member_name;
    CxxClass* cls;
    Expr* result = NULL;

    if (!check(TOK_IDENT) && !check(TOK_SCOPE)) return NULL;
    name = parse_qualified_name();
    tmpl = check(TOK_LT) ? find_class_template(name) : NULL;
    if (!tmpl) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    type = parse_class_template_specialization(tmpl, loc);
    if (!type || !type->cxx_class || !match(TOK_SCOPE) ||
        !check(TOK_IDENT)) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    member_name = advance()->value.str_val;
    cls = type->cxx_class;
    for (struct CxxMember* member = cls->members; member;
         member = member->next) {
        const char* final_name;
        Decl* declaration = member->decl;
        if (!member->is_static || !declaration ||
            (declaration->kind != DECL_VAR && declaration->kind != DECL_FUNC) ||
            !declaration->name) {
            continue;
        }
        final_name = strrchr(declaration->name, ':');
        final_name = final_name ? final_name + 1 : declaration->name;
        if (strcmp(final_name, member_name) != 0) continue;
        result = expr_ident(declaration->name, loc);
        result->ident_decl = declaration;
        result->type = declaration->type;
        break;
    }
    if (!result) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
    }
    return result;
}

Type* rcc_parse_cxx_type_name(void) {
    return parse_cxx_type_spec();
}

static Expr* parse_cxx_expression(void) {
    /* The common parser provides the complete precedence grammar.  Its C++
     * primary hook handles this/nullptr/new/delete and its C++ mode branch
     * handles true/false, so expressions are not truncated at one token. */
    return parse_expression();
}

/* ═══════════════════════════════════════
 * C++ Statement Parsing
 * ═══════════════════════════════════════ */

extern Stmt* parse_declaration(void);

static bool is_active_template_type(const char* name) {
    int index;
    if (!active_template || !name) return false;
    for (index = 0; index < active_template->param_count; ++index) {
        TemplateParam* parameter = &active_template->params[index];
        if (parameter->kind == TPARAM_TYPE && parameter->name &&
            strcmp(parameter->name, name) == 0) {
            return true;
        }
    }
    return false;
}

/* Return whether a `for` header contains the range separator at its outer
 * parameter-list depth.  Nested conditional expressions may contain `:`;
 * only a separator directly inside the header belongs to range-for. */
static bool cxx_range_for_header(void) {
    Token* token;
    int depth = 0;
    int conditional_depth = 0;
    if (!parser.cur || parser.cur->type != TOK_FOR || !parser.cur->next ||
        parser.cur->next->type != TOK_LPAREN) return false;
    for (token = parser.cur->next; token; token = token->next) {
        if (token->type == TOK_LPAREN || token->type == TOK_LBRACKET ||
            token->type == TOK_LBRACE) {
            ++depth;
        } else if (token->type == TOK_RPAREN ||
                   token->type == TOK_RBRACKET ||
                   token->type == TOK_RBRACE) {
            if (token->type == TOK_RPAREN && depth == 1) break;
            if (depth > 0) --depth;
        } else if (depth == 1 && token->type == TOK_QUESTION) {
            ++conditional_depth;
        } else if (token->type == TOK_COLON && depth == 1) {
            if (conditional_depth > 0) {
                --conditional_depth;
            } else {
                return true;
            }
        }
    }
    return false;
}

/* Lower the array form of a C++ range-for into the existing indexed-loop
 * representation.  Restricting the range operand to an identifier makes
 * the C++ single-evaluation rule explicit without inventing a hidden object
 * temporary.  Iterator/class ranges are diagnosed instead of being silently
 * reinterpreted as a different loop. */
Stmt* rcc_parse_cxx_range_for_statement(void) {
    SourceLoc loc;
    Type* item_type = NULL;
    bool is_auto = false;
    bool auto_const = false;
    bool auto_reference = false;
    bool auto_rvalue_reference = false;
    const char* item_name = NULL;
    Expr* range;
    Type* range_type = NULL;
    Expr* index_expression;
    Expr* element_expression;
    Expr* count_expression;
    Expr* condition;
    Expr* increment;
    Decl* index_decl;
    Decl* item_decl;
    Stmt* original_body;
    StmtList* body_statements = NULL;
    char index_name[64];
    int written;

    if (!cxx_range_for_header()) return NULL;
    loc = parser.cur->loc;
    advance(); /* for */
    expect(TOK_LPAREN, "(");
    if (check(TOK_AUTO) || (check(TOK_CONST) && check_next(TOK_AUTO))) {
        auto_const = match(TOK_CONST);
        match(TOK_AUTO);
        is_auto = true;
        if (match(TOK_AMP)) {
            auto_reference = true;
        } else if (match(TOK_AND)) {
            /* An array identifier is an lvalue range.  `auto&&` therefore
             * deduces an lvalue reference to its element, while retaining
             * the spelling here lets us reject the cv-qualified form that
             * cannot bind to this range. */
            auto_reference = true;
            auto_rvalue_reference = true;
        }
        {
            Token* name = expect(TOK_IDENT, "range variable name");
            if (name) item_name = name->value.str_val;
        }
    } else {
        item_type = parse_cxx_type_spec();
        item_type = rcc_parser_parse_cxx_declarator(
            item_type, &item_name, NULL);
        if (!item_name) {
            rcc_error(peek()->loc, "range-for requires an element declaration");
        }
    }
    expect(TOK_COLON, ":");
    range = parse_cxx_expression();
    expect(TOK_RPAREN, ")");
    if (!range || range->kind != EXPR_IDENT) {
        rcc_error(loc,
                  "RinOS range-for currently requires an array identifier range");
    } else {
        range_type = cxx_parser_value_type(range->ident_name);
        if (!range_type || range_type->kind != TYPE_ARRAY ||
            range_type->array_len < 0 || !range_type->base) {
            rcc_error(range->loc,
                      "RinOS range-for requires a complete array identifier range");
        }
        if (auto_reference && range_type && range_type->base) {
            if (auto_rvalue_reference && auto_const) {
                rcc_error(loc,
                          "const auto&& range variable cannot bind to an array lvalue");
            }
            Type* referred_type = range_type->base;
            if (auto_const) {
                Type* qualified = ast_arena_alloc(sizeof(*qualified));
                *qualified = *referred_type;
                qualified->is_const = true;
                referred_type = qualified;
            }
            item_type = type_ptr(referred_type);
            item_type->is_reference = true;
        }
    }

    written = snprintf(index_name, sizeof(index_name),
                       "__rcc_range_index_%u", ++cxx_range_for_counter);
    if (written < 0 || (size_t)written >= sizeof(index_name)) {
        rcc_error(loc, "range-for index name exceeds compiler limits");
        index_name[0] = '\0';
    }
    index_decl = decl_var(rcc_intern(index_name), type_int,
                          expr_int(0, loc), loc);
    index_expression = expr_ident(index_decl->name, loc);
    element_expression = expr_index(range, index_expression, loc);
    item_decl = decl_var(item_name ? item_name : rcc_intern("__rcc_range_item"),
                         item_type, element_expression, loc);
    item_decl->var_is_auto = is_auto && !auto_reference;
    rcc_parser_cxx_add_value_binding(item_decl->name,
                                     item_type ? item_type : type_int);

    count_expression = expr_binary(
        EXPR_DIV, expr_sizeof_expr(range, loc),
        expr_sizeof_expr(expr_index(range, expr_int(0, loc), loc), loc), loc);
    condition = expr_binary(EXPR_LT, expr_ident(index_decl->name, loc),
                            count_expression, loc);
    increment = expr_unary(EXPR_PREINC,
                           expr_ident(index_decl->name, loc), loc);

    original_body = parse_cxx_statement();
    stmtlist_append(&body_statements, stmt_decl(item_decl, loc));
    if (original_body) stmtlist_append(&body_statements, original_body);
    return stmt_for(stmt_decl(index_decl, loc), condition, increment,
                    stmt_block(body_statements, loc), loc);
}

static Stmt* parse_cxx_dependent_local_declaration(void) {
    SourceLoc loc = peek()->loc;
    Type* type;
    Token* name;
    Expr* initializer = NULL;
    bool is_auto_const = match(TOK_CONST);
    bool is_auto = match(TOK_AUTO);
    bool is_auto_reference = false;
    bool is_auto_rvalue_reference = false;
    bool is_auto_pointer = false;

    if (is_auto) {
        if (match(TOK_STAR)) {
            is_auto_pointer = true;
        } else if (match(TOK_AMP)) {
            is_auto_reference = true;
        } else if (match(TOK_AND)) {
            is_auto_reference = true;
            is_auto_rvalue_reference = true;
        }
        type = NULL;
    } else {
        type = parse_cxx_type_spec();
    }
    name = expect(TOK_IDENT, "local variable name");
    if (!name) return NULL;
    if (match(TOK_ASSIGN)) {
        if (check(TOK_LBRACE)) {
            if (check_next(TOK_RBRACE)) {
                SourceLoc initializer_loc = peek()->loc;
                advance();
                advance();
                initializer = expr_initializer_list(
                    exprlist_new(expr_int(0, initializer_loc)),
                    initializer_loc);
                initializer->compound_type = type;
                initializer->type = type;
                initializer->compound_value_init = true;
            } else {
                skip_balanced(TOK_LBRACE, TOK_RBRACE);
            }
        } else {
            initializer = parse_cxx_expression();
        }
    } else if (check(TOK_LBRACE)) {
        if (check_next(TOK_RBRACE)) {
            SourceLoc initializer_loc = peek()->loc;
            advance();
            advance();
            initializer = expr_initializer_list(
                exprlist_new(expr_int(0, initializer_loc)),
                initializer_loc);
            initializer->compound_type = type;
            initializer->type = type;
            initializer->compound_value_init = true;
        } else {
            skip_balanced(TOK_LBRACE, TOK_RBRACE);
        }
    }
    if (is_auto && initializer) {
        type = initializer->type;
        if (!type && initializer->kind == EXPR_COMPOUND) {
            type = initializer->compound_type;
        }
    }
    expect(TOK_SEMICOLON, ";");
    Decl* declaration = decl_var(name->value.str_val, type, initializer, loc);
    declaration->var_is_auto = is_auto;
    declaration->var_is_auto_reference = is_auto_reference;
    declaration->var_is_auto_rvalue_reference = is_auto_rvalue_reference;
    declaration->var_is_auto_pointer = is_auto_pointer;
    declaration->var_is_auto_const = is_auto_const;
    rcc_parser_cxx_add_value_binding(declaration->name, declaration->type);
    return stmt_decl(declaration, loc);
}

Stmt* rcc_parse_cxx_auto_local_declaration(void) {
    if (!check(TOK_AUTO) && !(check(TOK_CONST) && parser.cur->next &&
                              parser.cur->next->type == TOK_AUTO)) {
        return NULL;
    }
    return parse_cxx_dependent_local_declaration();
}

Stmt* rcc_parse_cxx_class_local_declaration(Type* base_type,
                                            int storage,
                                            bool is_thread_local,
                                            SourceLoc loc) {
    Token* name;
    ExprList* arguments = NULL;
    Expr* initializer;
    Decl* declaration;

    /* Only consume the spelling that the common C parser would misinterpret
     * as a function declarator.  Constructor arity was registered only after
     * the C++ class verifier proved its storage representation is ABI-safe. */
    if (!base_type || (base_type->kind != TYPE_STRUCT &&
                       base_type->kind != TYPE_UNION) ||
        !type_is_complete(base_type) ||
        rcc_parser_cxx_constructor_arity_mask(base_type) == 0u ||
        !check(TOK_IDENT) || !check_next(TOK_LPAREN)) {
        return NULL;
    }

    name = advance();
    advance(); /* `(` */
    if (!check(TOK_RPAREN)) {
        do {
            exprlist_append(&arguments, parse_assignment_expression());
        } while (match(TOK_COMMA));
    }
    expect(TOK_RPAREN, ")");

    initializer = expr_initializer_list(arguments, loc);
    initializer->compound_type = base_type;
    rcc_parser_validate_cxx_constructor_initializer(base_type, initializer);
    expect(TOK_SEMICOLON, ";");

    declaration = decl_var(name->value.str_val, base_type, initializer, loc);
    declaration->storage = (StorageClass)storage;
    declaration->var_is_thread_local = is_thread_local;
    rcc_parser_cxx_add_value_binding(declaration->name, declaration->type);
    return stmt_decl(declaration, loc);
}

static Stmt* parse_cxx_statement(void) {
    if (check(TOK_CONSTEXPR)) return parse_declaration();

    if (check(TOK_AUTO) ||
        (check(TOK_IDENT) &&
         is_active_template_type(peek()->value.str_val))) {
        return parse_cxx_dependent_local_declaration();
    }

    /* try-catch */
    if (match(TOK_TRY)) {
        SourceLoc loc = previous()->loc;
        Stmt* try_body;
        CxxCatch* catches = NULL;
        CxxCatch** catch_tail = &catches;

        /* Parse try block */
        expect(TOK_LBRACE, "{");
        StmtList* stmts = NULL;
        while (!check(TOK_RBRACE) && !at_end()) {
            Stmt* s = parse_cxx_statement();
            if (s) stmtlist_append(&stmts, s);
        }
        expect(TOK_RBRACE, "}");
        try_body = stmt_block(stmts, loc);

        /* Parse catch blocks */
        while (match(TOK_CATCH)) {
            CxxCatch* handler = ast_arena_alloc(sizeof(*handler));
            StmtList* handler_stmts = NULL;
            SourceLoc handler_loc = previous()->loc;
            Type* handler_type = NULL;
            const char* handler_name = NULL;
            bool is_ellipsis = false;

            memset(handler, 0, sizeof(*handler));
            expect(TOK_LPAREN, "(");
            if (!check(TOK_ELLIPSIS)) {
                handler_type = parse_cxx_type_spec();
                if (check(TOK_IDENT)) handler_name = advance()->value.str_val;
            } else {
                advance();  /* ... */
                is_ellipsis = true;
            }
            expect(TOK_RPAREN, ")");

            expect(TOK_LBRACE, "{");
            while (!check(TOK_RBRACE) && !at_end()) {
                Stmt* s = parse_cxx_statement();
                if (s) stmtlist_append(&handler_stmts, s);
            }
            expect(TOK_RBRACE, "}");

            /* Make the catch parameter a normal block declaration so lookup,
             * stack layout, and template cloning all use the existing paths. */
            if (handler_name && handler_type) {
                handler->parameter = decl_var(handler_name, handler_type,
                                               NULL, handler_loc);
                {
                    StmtList* parameter = ast_arena_alloc(sizeof(*parameter));
                    parameter->stmt = stmt_decl(handler->parameter,
                                                handler_loc);
                    parameter->next = handler_stmts;
                    handler_stmts = parameter;
                }
            }
            handler->type = handler_type;
            handler->name = handler_name;
            handler->is_ellipsis = is_ellipsis;
            handler->body = stmt_block(handler_stmts, handler_loc);
            handler->next = NULL;
            *catch_tail = handler;
            catch_tail = &handler->next;
        }

        return stmt_try(try_body, catches, loc);
    }

    /* throw */
    if (match(TOK_THROW)) {
        SourceLoc loc = previous()->loc;
        Expr* expression = check(TOK_SEMICOLON) ? NULL
                                               : parse_cxx_expression();
        expect(TOK_SEMICOLON, ";");
        return stmt_throw(expression, loc);
    }

    /* Fall back to C statement parsing */
    return parse_declaration();
}

Stmt* rcc_parse_cxx_statement(void) {
    return parse_cxx_statement();
}

static void add_cxx_declaration(AST* ast, Stmt* statement,
                                bool c_language_linkage) {
    if (statement && statement->kind == STMT_DECL) {
        set_cxx_link_name(statement->decl, NULL, c_language_linkage);
        ast_add_decl(ast, statement->decl);
    }
}

/* Preserve C ABI symbol spelling inside extern "C" while extern "C++" and
 * ordinary declarations use Itanium ABI link names. */
static void parse_cxx_language_linkage(AST* ast) {
    SourceLoc loc = peek()->loc;
    Token* language;
    bool c_language_linkage;
    advance(); /* extern */
    language = expect(TOK_STRING_LIT, "language linkage string");
    if (!language) return;
    if (strcmp(language->value.str_val, "C") != 0 &&
        strcmp(language->value.str_val, "C++") != 0) {
        rcc_error(loc, "unsupported language linkage '%s'",
                  language->value.str_val);
    }
    c_language_linkage = strcmp(language->value.str_val, "C") == 0;
    if (match(TOK_LBRACE)) {
        while (!check(TOK_RBRACE) && !at_end()) {
            Token* start = parser.cur;
            if ((check(TOK_AUTO) && parser.cur->next &&
                 parser.cur->next->type == TOK_IDENT &&
                 parser.cur->next->next &&
                 parser.cur->next->next->type == TOK_LPAREN) ||
                cxx_decltype_auto_starts_function()) {
                bool is_constexpr = false;
                bool is_noexcept = false;
                bool is_consteval = false;
                Decl* declaration = parse_cxx_function_declaration(
                    true, &is_constexpr, &is_noexcept, &is_consteval);
                if (declaration) {
                    set_cxx_link_name(declaration, NULL, c_language_linkage);
                    ast_add_decl(ast, declaration);
                }
            } else {
                add_cxx_declaration(ast, parse_cxx_statement(),
                                    c_language_linkage);
            }
            if (parser.cur == start && !at_end()) advance();
        }
        expect(TOK_RBRACE, "}");
        return;
    }
    if ((check(TOK_AUTO) && parser.cur->next &&
         parser.cur->next->type == TOK_IDENT && parser.cur->next->next &&
         parser.cur->next->next->type == TOK_LPAREN) ||
        cxx_decltype_auto_starts_function()) {
        bool is_constexpr = false;
        bool is_noexcept = false;
        bool is_consteval = false;
        Decl* declaration = parse_cxx_function_declaration(
            true, &is_constexpr, &is_noexcept, &is_consteval);
        if (declaration) {
            set_cxx_link_name(declaration, NULL, c_language_linkage);
            ast_add_decl(ast, declaration);
        }
    } else {
        add_cxx_declaration(ast, parse_cxx_statement(), c_language_linkage);
    }
}

/* ═══════════════════════════════════════
 * C++ Top-level Parsing
 * ═══════════════════════════════════════ */

/* Parse C++ translation unit */
AST* rcc_parse_cxx(TokenList* tokens) {
    rcc_parser_set_cxx_mode(true);
    parser.cur = tokens->head;
    parser.prev = NULL;

    AST* ast = ast_new();
    active_ast = ast;

    while (!at_end()) {
        Token* declaration_start = parser.cur;
        SourceLoc loc = peek()->loc;
        skip_cxx_attributes();

        if (check(TOK_EXTERN) && parser.cur->next &&
            parser.cur->next->type == TOK_STRING_LIT) {
            parse_cxx_language_linkage(ast);
        } else if (match(TOK_NAMESPACE)) {
            (void)parse_cxx_namespace(ast, g_global_namespace);
        } else if (match(TOK_TEMPLATE)) {
            CxxTemplate* tmpl = parse_cxx_template();
            if (g_global_namespace) {
                cxx_namespace_add_template(g_global_namespace, tmpl);
            }
        } else if (match(TOK_CLASS) || match(TOK_STRUCT)) {
            CxxClass* cls = parse_cxx_class();
            /* Class is stored in global namespace */
            if (g_global_namespace) {
                cxx_namespace_add_class(g_global_namespace, cls);
            }
            (void)loc;
        } else if ((check(TOK_CONSTEXPR) || check(TOK_CONSTEVAL)) &&
                   !cxx_constexpr_starts_function()) {
            Stmt* statement = parse_cxx_statement();
            if (statement && statement->kind == STMT_DECL) {
                add_cxx_declaration(ast, statement, false);
            }
        } else if ((check(TOK_AUTO) && parser.cur->next &&
                   parser.cur->next->type == TOK_IDENT &&
                   parser.cur->next->next &&
                   parser.cur->next->next->type == TOK_LPAREN) ||
                   cxx_decltype_auto_starts_function()) {
            bool is_constexpr = false;
            bool is_noexcept = false;
            bool is_consteval = false;
            Decl* declaration = parse_cxx_function_declaration(
                true, &is_constexpr, &is_noexcept, &is_consteval);
            if (g_global_namespace && declaration) {
                add_namespace_declaration(ast, g_global_namespace,
                                          declaration);
            }
        } else if (check(TOK_CONSTEXPR) || check(TOK_CONSTEVAL) ||
                   check(TOK_INLINE) ||
                   check(TOK___INLINE__)) {
            bool is_constexpr = false;
            bool is_noexcept = false;
            bool is_consteval = false;
            Decl* declaration = parse_cxx_function_declaration(
                true, &is_constexpr, &is_noexcept, &is_consteval);
            if (g_global_namespace && declaration) {
                add_namespace_declaration(ast, g_global_namespace,
                                           declaration);
            }
        } else if (match(TOK_USING)) {
            parse_cxx_using(g_global_namespace);
        } else {
            /* Regular C declaration */
            Stmt* s = parse_cxx_statement();
            add_cxx_declaration(ast, s, false);
        }

        /* Individual declaration and scope parsers synchronize at their own
         * grammar boundary.  Only force one-token progress here; using the
         * cumulative error count would otherwise discard every declaration
         * following the first recovered error. */
        if (parser.cur == declaration_start && !at_end()) advance();
    }

    active_ast = NULL;
    return ast;
}
