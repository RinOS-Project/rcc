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

typedef struct CxxParserValueBinding {
    const char* name;
    Type* type;
    struct CxxParserValueBinding* next;
} CxxParserValueBinding;

typedef struct CxxReferenceCapture {
    const char* name;
    struct CxxReferenceCapture* next;
} CxxReferenceCapture;

static CxxParserValueBinding* active_value_bindings;
static CxxParserValueBinding* saved_value_bindings[32];
static int saved_value_binding_depth;
static CxxReferenceCapture* active_reference_captures;
static CxxReferenceCapture* saved_reference_captures[32];
static int saved_reference_capture_depth;
static unsigned cxx_lambda_counter;

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

static Type* cxx_parser_value_type(const char* name) {
    for (CxxParserValueBinding* binding = active_value_bindings;
         binding; binding = binding->next) {
        if (binding->name && name && strcmp(binding->name, name) == 0) {
            return binding->type;
        }
    }
    return NULL;
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
        bool current_supported = true;
        CxxConstructorInitializer* item;
        if (check(TOK_IDENT) || check(TOK_SCOPE)) {
            field = parse_qualified_name();
        } else {
            rcc_error(peek()->loc, "expected constructor initializer name");
            return result;
        }
        if (match(TOK_LPAREN)) {
            if (!check(TOK_RPAREN)) value = parse_cxx_expression();
            expect(TOK_RPAREN, ")");
            if (!value || value->kind == EXPR_COMMA) {
                current_supported = false;
            }
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

/* Recognize constructors whose observable object representation is exactly
 * declaration-order initialization of their data fields.  This covers the
 * SDK status/outcome wrappers without executing arbitrary constructor code. */
static uint32_t lowerable_constructor_arity_mask(CxxClass* cls) {
    CxxConstructorInfo* constructor;
    uint32_t mask = cls && cls->type && cls->type->move_constructor_method
        ? UINT32_C(1) << 1 : 0u;
    if (!cls || !cls->type->is_complete || cls->base_count != 0 ||
        cls->has_static_field || cls->has_field_initializer ||
        class_has_virtual_member(cls) ||
        (class_has_destructor(cls) && !cls->type->cleanup_function)) {
        return 0u;
    }
    if (!cls->constructors) return 0u;
    for (constructor = cls->constructors; constructor;
         constructor = constructor->next) {
        unsigned arity;
        bool supported = true;
        TypeParam* field = cls->fields;
        TypeParam* parameter = constructor->parameters;
        CxxConstructorInitializer* initializer = constructor->initializers;
        if (constructor->access != ACCESS_PUBLIC ||
            constructor->is_deleted || constructor->is_defaulted) {
            continue;
        }
        arity = (unsigned)constructor->parameter_count;
        if (arity >= 32u) continue;
        if (!constructor->body_is_empty) {
            if (constructor->initializer_count == 0 &&
                constructor->method && constructor->method->decl &&
                constructor->method->decl->func_is_cxx_method &&
                constructor->method->decl->func_body) {
                mask |= UINT32_C(1) << arity;
            }
            continue;
        }
        if (!constructor->initializers_are_supported) continue;
        while (field && initializer) {
            Type* parameter_value_type;
            if (!initializer->field ||
                strcmp(initializer->field, field->name) != 0 ||
                !initializer->value) {
                supported = false;
                break;
            }
            if (arity == 0u) {
                int64_t constant_value = 0;
                if (!expr_eval_integer_constant(
                        initializer->value, &constant_value) ||
                    !field->type ||
                    !(type_is_integer(field->type) ||
                      field->type->kind == TYPE_ENUM ||
                      field->type->kind == TYPE_PTR ||
                      field->type->kind == TYPE_NULLPTR) ||
                    field->type->size <= 0 ||
                    (g_opts.target_arch == ARCH_X86 && field->type->size > 4) ||
                    (g_opts.target_arch == ARCH_X64 && field->type->size > 8)) {
                    supported = false;
                    break;
                }
            } else {
                if (!parameter || !parameter->name ||
                    initializer->value->kind != EXPR_IDENT ||
                    strcmp(initializer->value->ident_name,
                           parameter->name) != 0) {
                    supported = false;
                    break;
                }
                parameter_value_type = parameter->type;
                if (parameter_value_type &&
                    parameter_value_type->kind == TYPE_PTR &&
                    parameter_value_type->is_reference) {
                    parameter_value_type = parameter_value_type->base;
                }
                if (!type_is_compatible(field->type,
                                        parameter_value_type)) {
                    supported = false;
                    break;
                }
                parameter = parameter->next;
            }
            field = field->next;
            initializer = initializer->next;
        }
        if (!supported || field || initializer || parameter ||
            (arity != 0u &&
             constructor->initializer_count != (int)arity)) {
            continue;
        }
        mask |= UINT32_C(1) << arity;
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
        lowered->name = method->decl->name;
        lowered->return_type = method->decl->type->ret_type;
        lowered->field = field;
        lowered->function_decl = NULL;
        lowered->kind = kind;
        lowered->constant = has_constant ? constant : 0;
        lowered->cxx_access = (unsigned char)member->access;
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
 * covers the SDK status/outcome wrappers while retaining fail-closed parsing
 * for unvalidated helper calls. */
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
            !method->decl->name ||
            strcmp(method->decl->name, "operator conversion") != 0 ||
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
        lowered->name = method->decl->name;
        lowered->return_type = method->decl->type->ret_type;
        lowered->cxx_access = (unsigned char)member->access;
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
        if (class_has_destructor(cls) &&
            (!cls->type->cleanup_function ||
             cls->type->cleanup_field != field ||
             cls->type->cleanup_invalid != invalid)) {
            continue;
        }
        return_type = method->decl->type->ret_type;
        if (!type_is_compatible(return_type, field->type)) continue;
        lowered = ast_arena_alloc(sizeof(*lowered));
        lowered->name = method->decl->name;
        lowered->return_type = return_type;
        lowered->field = field;
        lowered->function_decl = NULL;
        lowered->kind = TYPE_METHOD_FIELD_RELEASE;
        lowered->constant = invalid;
        lowered->cxx_access = (unsigned char)member->access;
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
        if (!method || !method->decl || !method->decl->name ||
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
        lowered->name = method->decl->name;
        lowered->return_type = return_type;
        lowered->field = field;
        lowered->function_decl = NULL;
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
        if (!method || !method->decl || !method->decl->name ||
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
            strcmp(callee->ident_name, method->decl->name) == 0) {
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
        lowered->name = method->decl->name;
        lowered->return_type = method->decl->type->ret_type;
        lowered->cxx_access = (unsigned char)member->access;
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
        if (!method || !method->decl || !method->decl->name ||
            strcmp(method->decl->name, "operator=") != 0 ||
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

static bool class_declares_method_name(CxxClass* cls, const char* name) {
    struct CxxMember* member;
    if (!cls || !name) return false;
    for (member = cls->members; member; member = member->next) {
        if (member->method && member->method->decl &&
            member->method->decl->name &&
            strcmp(member->method->decl->name, name) == 0) {
            return true;
        }
    }
    return false;
}

/* Publish non-virtual methods of accessible non-virtual bases on the derived
 * class.  The layout pass records the exact base-subobject offset, so the
 * alias can carry the real base method declaration and an explicit byte
 * adjustment without inventing a guessed thunk.  Virtual bases remain out of
 * scope because their address is not a fixed compile-time offset. */
static void register_inherited_class_methods(CxxClass* cls,
                                             TypeMethod*** tail) {
    if (!cls || !tail || !*tail || !cls->base_offsets) {
        return;
    }
    for (int base_index = 0; base_index < cls->base_count; ++base_index) {
        CxxClass* base = cls->bases[base_index].base;
        TypeMethod* method;
        if (cls->bases[base_index].is_virtual ||
            cls->bases[base_index].access == ACCESS_PRIVATE ||
            !base || !base->type || !base->type->is_complete ||
            cls->base_offsets[base_index] < 0) {
            continue;
        }
        for (method = base->type->methods; method; method = method->next) {
            TypeMethod* inherited;
            if (method->kind != TYPE_METHOD_FUNCTION || !method->name ||
                !method->function_decl ||
                class_declares_method_name(cls, method->name)) {
                continue;
            }
            inherited = ast_arena_alloc(sizeof(*inherited));
            *inherited = *method;
            if (cls->bases[base_index].access == ACCESS_PROTECTED &&
                inherited->cxx_access == ACCESS_PUBLIC) {
                inherited->cxx_access = ACCESS_PROTECTED;
            }
            if (method->this_owner) {
                inherited->this_owner = method->this_owner;
                inherited->this_adjustment =
                    cls->base_offsets[base_index] +
                    method->this_adjustment;
            } else if (method->function_decl->func_this_param) {
                inherited->this_owner = base->type;
                inherited->this_adjustment = cls->base_offsets[base_index];
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
            method->is_defaulted || method->is_destructor ||
            (method->is_constructor &&
             (!constructor || constructor->body_is_empty))) {
            continue;
        }

        declaration = method->decl;
        source_name = declaration->name;
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
        lowered->is_virtual = method->is_virtual;
        lowered->vtable_index = method->vtable_index;
        lowered->vtable_symbol = method->is_virtual
            ? cls->type->cxx_vtable_symbol : NULL;
        lowered->next = NULL;
        *tail = lowered;
        tail = &lowered->next;

        ast_add_decl(active_ast, declaration);
    }
}

static void diagnose_unlowered_destructors(CxxClass* cls) {
    struct CxxMember* member;
    if (!cls || !cls->type || cls->type->cleanup_function || active_template) {
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

/* Parse class member (field or method) */
static void parse_class_member(CxxClass* cls, AccessSpec current_access) {
    SourceLoc loc = peek()->loc;

    skip_cxx_attributes();

    bool is_virtual = false;
    bool is_static = false;
    bool is_constexpr = false;
    bool is_consteval = false;
    bool is_explicit = false;

    /* C++ declaration specifiers can be combined in either order. */
    for (;;) {
        if (match(TOK_VIRTUAL)) is_virtual = true;
        else if (match(TOK_STATIC)) is_static = true;
        else if (match(TOK_CONSTEXPR)) is_constexpr = true;
        else if (match(TOK_CONSTEVAL)) {
            is_constexpr = true;
            is_consteval = true;
        }
        else if (match(TOK_EXPLICIT)) is_explicit = true;
        else if (match(TOK_INLINE) || match(TOK___INLINE__)) { }
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

    if (!name) {
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
                is_noexcept = true;
                if (check(TOK_LPAREN)) {
                    skip_balanced(TOK_LPAREN, TOK_RPAREN);
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
        method->is_pure_virtual = is_pure;
        method->is_deleted = is_deleted;
        method->is_defaulted = is_defaulted;
        method->is_constructor = is_constructor;
        method->is_destructor = is_destructor;
        method->decl->func_is_cxx_constructor = is_constructor;
        method->decl->func_is_cxx_destructor = is_destructor;
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

        /* Initializer? */
        Expr* init = NULL;
        if (match(TOK_ASSIGN)) {
            init = parse_cxx_expression();
        } else if (check(TOK_LBRACE)) {
            init = rcc_parser_parse_initializer();
        }
        if (init) cls->has_field_initializer = true;
        if (current_access != ACCESS_PUBLIC) {
            cls->has_nonpublic_field = true;
        }
        if (is_static) cls->has_static_field = true;

        expect(TOK_SEMICOLON, ";");

        /* Add field to class */
        cxx_class_add_field_initializer(cls, name, type, current_access, init);
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
                rcc_error(loc,
                          "C++ virtual base '%s' requires virtual-base layout "
                          "and ABI support",
                          base_name ? base_name : "<anonymous>");
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
    cxx_class_build_vtable(cls);
    cxx_class_compute_layout(cls);
    register_inline_class_accessors(cls);
    register_inline_class_bool_delegates(cls);
    register_inline_class_cleanup(cls);
    register_inline_class_releases(cls);
    register_inline_class_closes(cls);
    register_inline_class_close_delegates(cls);
    register_inline_class_move_constructor(cls);
    register_inline_class_move_assignment(cls);
    diagnose_unlowered_destructors(cls);
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
        Expr* default_argument = NULL;
        Decl* parameter;
        type = rcc_parser_parse_cxx_declarator(type, &name, NULL);
        if (match(TOK_ASSIGN)) {
            default_argument = parse_assignment_expression();
        }
        parameter = decl_param(name, type, param_idx++, peek()->loc);
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
        parameter->initializer = item->decl ? item->decl->param_default : NULL;
        parameter->next = NULL;
        *tail = parameter;
        tail = &parameter->next;
    }
    return type_func(return_type, type_params, false);
}

static bool cxx_lambda_has_return(Stmt* statement) {
    if (!statement) return false;
    switch (statement->kind) {
        case STMT_RETURN:
            return statement->return_val != NULL;
        case STMT_BLOCK:
            for (StmtList* item = statement->block_stmts; item;
                 item = item->next) {
                if (cxx_lambda_has_return(item->stmt)) return true;
            }
            return false;
        case STMT_IF:
            return cxx_lambda_has_return(statement->if_then) ||
                   cxx_lambda_has_return(statement->if_else);
        case STMT_WHILE:
        case STMT_DO:
            return cxx_lambda_has_return(statement->while_body);
        case STMT_FOR:
            return cxx_lambda_has_return(statement->for_body);
        default:
            return false;
    }
}

/* Lower a non-capturing lambda to a real internal function declaration.  A
 * lambda with captures needs a closure object and an environment ABI, so it
 * is diagnosed at the grammar boundary rather than being converted to a
 * function with silently missing state. */
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
    CxxReferenceCapture* lambda_reference_captures = NULL;

    expect(TOK_LBRACKET, "[");
    if (!match(TOK_RBRACKET)) {
        do {
            Token* capture;
            Type* capture_type;
            bool reference_capture = match(TOK_AMP) || match(TOK_AND);
            capture = expect(TOK_IDENT, "lambda capture name");
            if (!capture) break;
            capture_type = cxx_parser_value_type(capture->value.str_val);
            if (!capture_type) capture_type = type_int;
            if (reference_capture) {
                CxxReferenceCapture* reference =
                    ast_arena_alloc(sizeof(*reference));
                reference->name = capture->value.str_val;
                reference->next = lambda_reference_captures;
                lambda_reference_captures = reference;
                capture_type = type_ptr(capture_type);
                exprlist_append(&captures, expr_unary(
                    EXPR_ADDR, expr_ident(capture->value.str_val,
                                          capture->loc), capture->loc));
            } else {
                exprlist_append(&captures,
                                expr_ident(capture->value.str_val,
                                           capture->loc));
            }
            decllist_append(&capture_params,
                            decl_param(capture->value.str_val, capture_type,
                                       capture_count++, capture->loc));
        } while (match(TOK_COMMA));
        expect(TOK_RBRACKET, "]");
    }
    if (match(TOK_LPAREN)) {
        params = parse_cxx_parameter_declarations();
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
    (void)match(TOK_MUTABLE);
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
    if (!return_type) return_type = cxx_lambda_has_return(body)
        ? type_int : type_void;
    written = snprintf(name, sizeof(name), "__rcc_lambda_%u",
                       ++cxx_lambda_counter);
    if (written < 0 || (size_t)written >= sizeof(name)) {
        rcc_fatal("C++ lambda symbol name exceeds compiler limits");
    }
    function = decl_func(rcc_intern(name),
                         cxx_lambda_function_type(return_type, all_params),
                         all_params, body, loc);
    function->storage = STORAGE_STATIC;
    function->func_is_inline = true;
    function->link_name = function->name;
    if (active_ast) ast_add_decl(active_ast, function);
    {
        Expr* result = expr_ident(function->name, loc);
        result->ident_decl = function;
        result->type = function->type;
        result->cxx_lambda_captures = captures;
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
    return_type = parse_cxx_type_spec();
    name = expect(TOK_IDENT, "function name");
    if (!name) return NULL;
    expect(TOK_LPAREN, "(");
    params = parse_cxx_parameter_declarations();
    expect(TOK_RPAREN, ")");
    if (match(TOK_NOEXCEPT)) {
        *is_noexcept = true;
        if (check(TOK_LPAREN)) {
            skip_balanced(TOK_LPAREN, TOK_RPAREN);
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
    function->decl->func_is_inline = is_inline;
    function->decl->func_is_constexpr = *is_constexpr;
    function->decl->func_is_consteval = *is_consteval;
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

    expect(TOK_LT, "<");

    CxxTemplate* tmpl = cxx_template_new(loc);

    /* Parse template parameters */
    if (!check(TOK_GT)) {
        do {
            bool type_parameter = false;
            if (match(TOK_TYPENAME) || match(TOK_CLASS)) {
                /* Type parameter */
                type_parameter = true;
                const char* param_name = NULL;
                if (check(TOK_IDENT)) {
                    param_name = advance()->value.str_val;
                }
                cxx_template_add_type_param(tmpl, param_name);
            } else {
                /* Non-type parameter */
                Type* param_type = parse_cxx_type_spec();
                const char* param_name = NULL;
                if (check(TOK_IDENT)) {
                    param_name = advance()->value.str_val;
                }
                cxx_template_add_value_param(tmpl, param_name, param_type);
            }

            /* Default value? */
            if (match(TOK_ASSIGN)) {
                int parameter_index = tmpl->param_count - 1;
                if (parameter_index >= 0) {
                    tmpl->params[parameter_index].has_default = true;
                    if (type_parameter) {
                        tmpl->params[parameter_index].default_type =
                            parse_cxx_type_spec();
                    } else {
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

    /* Template body */
    if ((check(TOK_CLASS) || check(TOK_STRUCT)) &&
        parser.cur->next && parser.cur->next->type == TOK_IDENT &&
        parser.cur->next->next &&
        parser.cur->next->next->type == TOK_LT) {
        bool is_struct = match(TOK_STRUCT);
        Token* name_token;
        CxxTemplate* primary;
        Type* arguments[32];
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
                arguments[argument_count++] = parse_cxx_type_spec();
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
        tmpl->templated_class = NULL;
        specialized_class = parse_cxx_class_named(
            loc, is_struct,
            name_token ? name_token->value.str_val : "specialization");
        active_template = outer_template;
        tmpl->templated_class = specialized_class;
        if (specialized_class) {
            if (tmpl->param_count == 0) {
                register_ordinary_class_methods(specialized_class);
            }
            specialized_class->templ = tmpl;
            if (primary && argument_count == primary->param_count) {
                tmpl->specialization_args = ast_arena_alloc(
                    sizeof(Type*) * (size_t)argument_count);
                memcpy(tmpl->specialization_args, arguments,
                       sizeof(Type*) * (size_t)argument_count);
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

static CxxTemplate* find_function_template(const char* qualified_name) {
    return find_template(qualified_name, TMPL_FUNCTION);
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
    if (check(TOK_LT) && find_class_template(name)) result = true;

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

static int template_value_parameter_index(CxxTemplate* tmpl,
                                           Expr* bound_expression) {
    if (!tmpl || !bound_expression || bound_expression->kind != EXPR_IDENT ||
        !bound_expression->ident_name) {
        return -1;
    }
    for (int index = 0; index < tmpl->param_count; ++index) {
        if (tmpl->params[index].kind == TPARAM_NONTYPE &&
            tmpl->params[index].name &&
            strcmp(tmpl->params[index].name,
                   bound_expression->ident_name) == 0) {
            return index;
        }
    }
    return -1;
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
    parameter_index = template_parameter_index(tmpl, type);
    if (parameter_index >= 0 && parameter_index < argument_count) {
        substituted = arguments[parameter_index];
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
        if (type->kind == TYPE_ARRAY && value_args && value_present &&
            type->array_bound) {
            int value_index = template_value_parameter_index(
                tmpl, type->array_bound);
            if (value_index >= 0 && value_present[value_index]) {
                int64_t value = value_args[value_index];
                if (value <= 0 || value > INT_MAX) {
                    rcc_error(type->array_bound->loc,
                              "non-type template array bound is out of range");
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
    copy = cxx_method_new(method->decl->name, return_type, parameters,
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
    copy->is_deleted = method->is_deleted;
    copy->is_defaulted = method->is_defaulted;
    copy->is_constructor = method->is_constructor;
    copy->is_destructor = method->is_destructor;
    copy->decl->func_is_cxx_constructor = copy->is_constructor;
    copy->decl->func_is_cxx_destructor = copy->is_destructor;
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

    if (!tmpl || tmpl->kind != TMPL_CLASS || !tmpl->templated_class ||
        argument_count != tmpl->param_count) {
        rcc_error(loc, "class template argument count mismatch");
        return type_struct(tmpl && tmpl->name ? tmpl->name : "template");
    }
    for (index = 0; index < argument_count; ++index) {
        if (tmpl->params[index].kind == TPARAM_NONTYPE) {
            has_value_parameters = true;
        }
        if (tmpl->params[index].kind == TPARAM_TYPE &&
            (!arguments || !arguments[index])) {
            rcc_error(loc,
                      "class template type argument %d is missing", index + 1);
            return NULL;
        }
        if (tmpl->params[index].kind == TPARAM_NONTYPE &&
            (!value_args || !value_present || !value_present[index] ||
             !tmpl->params[index].type ||
             !type_is_integer(tmpl->params[index].type))) {
            rcc_error(loc,
                      "class template non-type argument %d requires an "
                      "integer constant", index + 1);
            return NULL;
        }
        if (tmpl->params[index].kind == TPARAM_TEMPLATE) {
            rcc_error(loc,
                      "class template template parameters are not supported");
            return NULL;
        }
    }
    for (index = 0; index < tmpl->instance_count; ++index) {
        int argument_index;
        bool matches = tmpl->instances[index].arg_count == argument_count;
        for (argument_index = 0; matches &&
             argument_index < argument_count; ++argument_index) {
            if (tmpl->params[argument_index].kind == TPARAM_NONTYPE) {
                matches = tmpl->instances[index].value_present &&
                    tmpl->instances[index].value_present[argument_index] &&
                    value_present[argument_index] &&
                    tmpl->instances[index].value_args[argument_index] ==
                        value_args[argument_index];
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
            const char* mangled = cxx_mangle_type(
                tmpl->specialization_args[argument_index]);
            int written = snprintf(
                specialization_suffix + suffix_length,
                sizeof(specialization_suffix) - suffix_length,
                "%s%s", argument_index == 0 ? "" : ",",
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
    instance->templ = tmpl;
    instance->template_arg_count = argument_count;
            instance->template_args = ast_arena_alloc(
        sizeof(Type*) * (size_t)argument_count);
    memcpy(instance->template_args, arguments,
           sizeof(Type*) * (size_t)argument_count);

    for (TypeParam* field = definition->fields; field; field = field->next) {
        cxx_class_add_field_initializer(
            instance, field->name,
            substitute_template_type(
                tmpl, field->type, arguments, argument_count,
                value_args, value_present),
            (AccessSpec)field->cxx_access,
            cxx_template_clone_expr_with_values(
                tmpl, field->initializer, arguments, argument_count,
                value_args, value_present));
    }
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
            initializer_copy->next = NULL;
            *initializer_tail = initializer_copy;
            initializer_tail = &initializer_copy->next;
        }
        copy->next = NULL;
        while (*tail) tail = &(*tail)->next;
        *tail = copy;
        }
    }

    cxx_class_build_vtable(instance);
    cxx_class_compute_layout(instance);
    diagnose_unlowered_destructors(instance);
    register_ordinary_class_methods(instance);
    register_inline_class_accessors(instance);
    register_inline_class_bool_delegates(instance);
    register_inline_class_cleanup(instance);
    register_inline_class_releases(instance);
    register_inline_class_closes(instance);
    register_inline_class_close_delegates(instance);
    register_inline_class_move_constructor(instance);
    register_inline_class_move_assignment(instance);
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
    tmpl->instances[tmpl->instance_count].arg_count = argument_count;
    tmpl->instances[tmpl->instance_count].instantiated = instance;
    ++tmpl->instance_count;
    return instance->type;
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
    if (!type || !tmpl) return NULL;
    for (int index = 0; index < tmpl->instance_count; ++index) {
        if (tmpl->instances[index].arg_count != argument_count) continue;
        bool matches = true;
        for (int argument_index = 0; argument_index < argument_count;
             ++argument_index) {
            if (tmpl->params[argument_index].kind == TPARAM_NONTYPE) {
                if (!tmpl->instances[index].value_present ||
                    !value_present ||
                    !tmpl->instances[index].value_present[argument_index] ||
                    !value_present[argument_index] ||
                    tmpl->instances[index].value_args[argument_index] !=
                        value_args[argument_index]) {
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
    Type* arguments[32];
    int64_t values[32] = { 0 };
    bool value_present[32] = { false };
    int argument_count = 0;
    expect(TOK_LT, "<");
    if (!check(TOK_GT)) {
        do {
            if (argument_count == (int)(sizeof(arguments) /
                                        sizeof(arguments[0]))) {
                rcc_error(loc, "class template argument limit exceeded");
                break;
            }
            if (argument_count >= tmpl->param_count) {
                rcc_error(peek()->loc, "too many class template arguments");
                while (!check(TOK_COMMA) && !check(TOK_GT) && !at_end()) {
                    advance();
                }
                arguments[argument_count++] = type_int;
                continue;
            }
            TemplateParam* parameter = &tmpl->params[argument_count];
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
                rcc_error(peek()->loc,
                          "class template template parameters are not supported");
                arguments[argument_count++] = type_int;
            }
        } while (match(TOK_COMMA));
    }
    while (argument_count < tmpl->param_count &&
           tmpl->params[argument_count].has_default) {
        TemplateParam* parameter = &tmpl->params[argument_count];
        if (parameter->kind == TPARAM_TYPE && parameter->default_type) {
            arguments[argument_count] = parameter->default_type;
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
    CxxTemplate* selected = NULL;
    Type* selected_arguments[32] = { NULL };
    int selected_specificity = -1;
    for (int index = 0; index < tmpl->specialization_count; ++index) {
        CxxTemplate* specialization = tmpl->specializations[index];
        bool matches = specialization &&
            specialization->specialization_arg_count == argument_count;
        Type* specialization_arguments[32] = { NULL };
        int specificity = specialization && specialization->param_count == 0
            ? 100000 : 0;
        for (int argument_index = 0; matches &&
             argument_index < argument_count; ++argument_index) {
            if (specialization->param_count == 0) {
                matches = type_is_compatible(
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
                if (!specialization_arguments[parameter_index]) {
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
        if (selected->param_count == 0) return selected->templated_class->type;
        return instantiate_class_template(
            selected, selected_arguments, NULL, NULL,
            selected->param_count, loc);
    }
    return instantiate_class_template(tmpl, arguments, values, value_present,
                                      argument_count, loc);
}

static bool deduce_function_template_type(CxxTemplate* tmpl, Type* pattern,
                                          Type* actual, Type** arguments,
                                          int64_t* values,
                                          bool* value_present) {
    if (!tmpl || !pattern || !actual || !arguments) return false;
    if (pattern->kind == TYPE_STRUCT && pattern->tag) {
        for (int index = 0; index < tmpl->param_count; ++index) {
            TemplateParam* parameter = &tmpl->params[index];
            if (parameter->kind == TPARAM_TYPE && parameter->name &&
                strcmp(parameter->name, pattern->tag) == 0) {
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
        return deduce_function_template_type(tmpl, pattern->base, actual,
                                             arguments, values,
                                             value_present);
    }
    if (pattern->kind == TYPE_PTR && actual->kind == TYPE_PTR) {
        return deduce_function_template_type(tmpl, pattern->base,
                                             actual->base, arguments, values,
                                             value_present);
    }
    if (pattern->kind == TYPE_ARRAY && actual->kind == TYPE_ARRAY) {
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
                                             value_present);
    }
    return type_is_compatible(pattern, actual);
}

static bool deduce_function_template_arguments(CxxTemplate* tmpl,
                                               ExprList* call_arguments,
                                               Type** template_arguments,
                                               int64_t* template_values,
                                               bool* template_value_present) {
    DeclList* parameter;
    ExprList* argument;
    if (!tmpl || !tmpl->func_def || !template_arguments) return false;
    parameter = tmpl->func_def->func_params;
    argument = call_arguments;
    while (parameter && argument) {
        Type* actual = argument->expr ? argument->expr->type : NULL;
        if (!actual && argument->expr &&
            argument->expr->kind == EXPR_IDENT) {
            actual = cxx_parser_value_type(argument->expr->ident_name);
        }
        if (!actual) {
            SourceLoc location;
            cxx_parser_expr_loc(&location, argument->expr,
                                &tmpl->func_def->loc);
            rcc_error(location,
                      "cannot deduce function template type from an expression "
                      "without a parser-known type");
            return false;
        }
        if (!deduce_function_template_type(
                tmpl, parameter->decl->type, actual, template_arguments,
                template_values, template_value_present)) {
            rcc_error(argument->expr->loc,
                      "function template argument type does not match its "
                      "parameter pattern");
            return false;
        }
        parameter = parameter->next;
        argument = argument->next;
    }
    if (argument) {
        SourceLoc location;
        cxx_parser_expr_loc(&location, argument->expr,
                            &tmpl->func_def->loc);
        rcc_error(location, "too many arguments for function template deduction");
        return false;
    }
    for (; parameter; parameter = parameter->next) {
        if (!parameter->decl->param_default) {
            rcc_error(tmpl->func_def->loc,
                      "too few arguments for function template deduction");
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
                                               SourceLoc loc) {
    int64_t result;
    if (!tmpl || !tmpl->constraint) return true;
    if (!eval_template_integer_expression(tmpl->constraint, tmpl, values,
                                          value_present, &result)) {
        rcc_error(loc,
                  "requires-clause must be an integral constant expression "
                  "over non-type template parameters");
        return false;
    }
    if (!result) {
        rcc_error(loc, "template constraints are not satisfied");
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
    Type* argument;
    TypeField* size_field;
    TypeField* version_field;
    ExprList* items = NULL;
    Expr* initializer;
    bool unsafe_versioned_shape;

    if (!check(TOK_IDENT) && !check(TOK_SCOPE)) return NULL;
    name = parse_qualified_name();
    tmpl = find_function_template(name);
    if (!tmpl) {
        parser.cur = saved_cur;
        parser.prev = saved_prev;
        return NULL;
    }
    unsafe_versioned_shape = template_has_unsupported_versioned_shape(tmpl);
    if (tmpl->function_lowering != TMPL_FUNCTION_VERSIONED_STRUCT) {
        Type* template_arguments[32];
        int64_t template_values[32] = { 0 };
        bool template_value_present[32] = { false };
        int argument_count = 0;
        Decl* instance;
        ExprList* call_arguments = NULL;
        Expr* function;
        bool explicit_template_arguments = check(TOK_LT);

        if (explicit_template_arguments) {
            expect(TOK_LT, "<");
            if (!check(TOK_GT)) {
                do {
                    if (argument_count >=
                        (int)(sizeof(template_arguments) /
                              sizeof(template_arguments[0]))) {
                        rcc_error(loc, "function template argument limit exceeded");
                        while (!check(TOK_GT) && !at_end()) advance();
                        break;
                    }
                    if (tmpl->param_count > argument_count &&
                        tmpl->params[argument_count].kind == TPARAM_TYPE) {
                        template_arguments[argument_count++] =
                            parse_cxx_type_spec();
                    } else {
                        TemplateParam* parameter = argument_count <
                            tmpl->param_count
                            ? &tmpl->params[argument_count] : NULL;
                        Expr* value_expression;
                        int64_t value;

                        if (!parameter || parameter->kind != TPARAM_NONTYPE) {
                            rcc_error(peek()->loc,
                                      "too many function template arguments");
                            while (!check(TOK_COMMA) && !check(TOK_GT) &&
                                   !at_end()) {
                                advance();
                            }
                            template_arguments[argument_count++] = type_int;
                            continue;
                        }
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
                            value = 0;
                        }
                        template_arguments[argument_count] = parameter->type;
                        template_values[argument_count] = value;
                        template_value_present[argument_count] = true;
                        ++argument_count;
                    }
                } while (match(TOK_COMMA));
            }
        } else if (!check(TOK_LPAREN)) {
            parser.cur = saved_cur;
            parser.prev = saved_prev;
            return NULL;
        }
        while (argument_count < tmpl->param_count &&
               tmpl->params[argument_count].has_default) {
            TemplateParam* parameter = &tmpl->params[argument_count];
            if (parameter->kind == TPARAM_TYPE && parameter->default_type) {
                template_arguments[argument_count] = parameter->default_type;
            } else if (parameter->kind == TPARAM_NONTYPE &&
                       parameter->default_value) {
                int64_t value;
                if (!eval_template_integer_expression(
                        parameter->default_value, tmpl, template_values,
                        template_value_present, &value)) {
                    rcc_error(loc,
                              "function template non-type default must be "
                              "an integer constant expression");
                    value = 0;
                }
                template_arguments[argument_count] = parameter->type;
                template_values[argument_count] = value;
                template_value_present[argument_count] = true;
            } else {
                break;
            }
            ++argument_count;
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

        if (unsafe_versioned_shape) {
            rcc_error(loc, "function template '%s' is not safely lowerable",
                      name);
            return expr_int(0, loc);
        }

        if (!explicit_template_arguments) {
            memset(template_arguments, 0, sizeof(template_arguments));
            argument_count = 0;
            if (!deduce_function_template_arguments(tmpl, call_arguments,
                                                     template_arguments,
                                                     template_values,
                                                     template_value_present)) {
                return expr_int(0, loc);
            }
            for (int index = 0; index < tmpl->param_count; ++index) {
                TemplateParam* parameter = &tmpl->params[index];
                if (parameter->kind == TPARAM_NONTYPE) {
                    if (!template_value_present[index]) {
                        rcc_error(loc,
                                  "could not deduce function template non-type "
                                  "argument %d", index + 1);
                        return expr_int(0, loc);
                    }
                    template_arguments[index] = parameter->type;
                    continue;
                }
                if (!template_arguments[index] && parameter->has_default &&
                    parameter->kind == TPARAM_TYPE &&
                    parameter->default_type) {
                    template_arguments[index] = parameter->default_type;
                }
                if (!template_arguments[index]) {
                    rcc_error(loc,
                              "could not deduce function template type "
                              "argument %d",
                              index + 1);
                    return expr_int(0, loc);
                }
            }
            argument_count = tmpl->param_count;
        }

        if (argument_count != tmpl->param_count) {
            rcc_error(loc, "function template '%s' requires %d template arguments",
                      name, tmpl->param_count);
            return expr_int(0, loc);
        }
        if (!cxx_template_constraint_satisfied(tmpl, template_values,
                                               template_value_present, loc)) {
            return expr_int(0, loc);
        }
        instance = (Decl*)cxx_template_instantiate_with_values(
            tmpl, template_arguments, template_values,
            template_value_present, argument_count);
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
    if (match(TOK_VOID)) {
        t = type_void;
    } else if (match(TOK_BOOL)) {
        t = type_bool;
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
        CxxClass* known_class = find_class(name);
        if (!known_class && active_class && active_class->name &&
            strcmp(name, active_class->name) == 0) {
            known_class = active_class;
        }
        Type* known_type = strstr(name, "::") == NULL
            ? rcc_parser_lookup_type(name) : NULL;
        if (tmpl) {
            t = parse_class_template_specialization(tmpl, loc);
        } else if (known_class) {
            t = known_class->type;
        } else if (known_type) {
            t = known_type;
        } else if (is_active_template_type(name)) {
            /* A dependent type remains an incomplete placeholder until
             * template substitution. */
            t = type_struct(name);
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

static Stmt* parse_cxx_dependent_local_declaration(void) {
    SourceLoc loc = peek()->loc;
    Type* type;
    Token* name;
    Expr* initializer = NULL;
    bool is_auto = match(TOK_AUTO);

    if (is_auto) {
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
    return stmt_decl(declaration, loc);
}

Stmt* rcc_parse_cxx_auto_local_declaration(void) {
    if (!check(TOK_AUTO)) return NULL;
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
        /* Parse try block */
        expect(TOK_LBRACE, "{");
        StmtList* stmts = NULL;
        while (!check(TOK_RBRACE) && !at_end()) {
            Stmt* s = parse_cxx_statement();
            if (s) stmtlist_append(&stmts, s);
        }
        expect(TOK_RBRACE, "}");

        /* Parse catch blocks */
        while (match(TOK_CATCH)) {
            expect(TOK_LPAREN, "(");
            if (!check(TOK_ELLIPSIS)) {
                parse_cxx_type_spec();
                if (check(TOK_IDENT)) advance();
            } else {
                advance();  /* ... */
            }
            expect(TOK_RPAREN, ")");

            expect(TOK_LBRACE, "{");
            while (!check(TOK_RBRACE) && !at_end()) {
                parse_cxx_statement();
            }
            expect(TOK_RBRACE, "}");
        }

        /* There is no exception runtime/ABI in the current RinOS image
         * contract.  Keep parsing the complete construct for recovery, but
         * never turn it into a successful try block with silently discarded
         * handlers. */
        rcc_error(loc,
                  "C++ try/catch requires exception tables and runtime ABI");
        return NULL;
    }

    /* throw */
    if (match(TOK_THROW)) {
        SourceLoc loc = previous()->loc;
        if (!check(TOK_SEMICOLON)) {
            parse_cxx_expression();
        }
        expect(TOK_SEMICOLON, ";");
        rcc_error(loc,
                  "C++ throw requires exception tables and runtime ABI");
        return NULL;
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
            add_cxx_declaration(ast, parse_cxx_statement(),
                                c_language_linkage);
            if (parser.cur == start && !at_end()) advance();
        }
        expect(TOK_RBRACE, "}");
        return;
    }
    add_cxx_declaration(ast, parse_cxx_statement(), c_language_linkage);
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
