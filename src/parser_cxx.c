/*
 * RCC++ - RinOS C++ Compiler
 * C++ specific parser extensions
 */

#include "rcc.h"
#include "token.h"
#include "ast.h"
#include "ast_cxx.h"

/* External parser state (from parser.c) */
typedef struct {
    Token* cur;
    Token* prev;
} Parser;

extern Parser parser;

/* The current template is only needed while parsing dependent declarations;
 * instantiated types are resolved by the later template semantic phase. */
static CxxTemplate* active_template;

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
static Stmt* parse_cxx_statement(void);
static Type* parse_cxx_type_spec(void);
static Decl* parse_cxx_function_declaration(bool parse_body,
                                            bool* is_constexpr,
                                            bool* is_noexcept);
CxxTemplate* parse_cxx_template(void);

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
        case TOK_PLUS: case TOK_MINUS: case TOK_STAR: case TOK_SLASH:
        case TOK_PERCENT: case TOK_INC: case TOK_DEC:
        case TOK_EQ: case TOK_NE: case TOK_LT: case TOK_LE:
        case TOK_GT: case TOK_GE: case TOK_AMP: case TOK_PIPE:
        case TOK_CARET: case TOK_TILDE: case TOK_NOT:
        case TOK_AND: case TOK_OR: case TOK_LSHIFT: case TOK_RSHIFT:
        case TOK_COMMA: case TOK_ARROW:
            advance();
            return rcc_intern("operator");
        default:
            rcc_error(peek()->loc, "expected overloaded operator");
            return NULL;
    }
}

/* Consume a constructor's mem-initializer-list without consuming its body. */
static void skip_ctor_initializers(void) {
    if (!match(TOK_COLON)) return;
    do {
        if (check(TOK_IDENT) || check(TOK_SCOPE)) {
            (void)parse_qualified_name();
        } else {
            rcc_error(peek()->loc, "expected constructor initializer name");
            return;
        }
        if (check(TOK_LPAREN)) {
            skip_balanced(TOK_LPAREN, TOK_RPAREN);
        } else if (check(TOK_LBRACE)) {
            skip_balanced(TOK_LBRACE, TOK_RBRACE);
        } else {
            rcc_error(peek()->loc, "expected constructor initializer");
            return;
        }
    } while (match(TOK_COMMA));
}

typedef struct ParsedConstructorInitializer {
    const char* field;
    Expr* value;
    bool is_single;
} ParsedConstructorInitializer;

/* Retain exactly one parenthesized mem-initializer.  More general lists keep
 * parsing correctly but are intentionally not candidates for aggregate
 * lowering: executing them requires the full C++ constructor pipeline. */
static ParsedConstructorInitializer parse_ctor_initializer(void) {
    ParsedConstructorInitializer result = {0};
    int count = 0;
    bool supported = true;
    if (!match(TOK_COLON)) return result;
    do {
        const char* field = NULL;
        Expr* value = NULL;
        bool current_supported = true;
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
        ++count;
        if (count == 1) {
            result.field = field;
            result.value = value;
        } else {
            supported = false;
        }
        if (!current_supported) supported = false;
    } while (match(TOK_COMMA));
    result.is_single = count == 1 && supported;
    return result;
}

static bool class_has_virtual_member(CxxClass* cls) {
    struct CxxMember* member;
    for (member = cls->members; member; member = member->next) {
        if (member->is_virtual) return true;
    }
    return false;
}

/* Recognize constructors whose observable object representation is exactly
 * a zero/value initialization of one field.  This is sufficient for the SDK
 * status wrapper without pretending to execute arbitrary constructor code. */
static uint32_t lowerable_constructor_arity_mask(CxxClass* cls) {
    CxxConstructorInfo* constructor;
    TypeParam* field;
    uint32_t mask = 0u;
    if (!cls || !cls->type->is_complete || cls->base_count != 0 ||
        cls->has_static_field || cls->has_field_initializer ||
        class_has_virtual_member(cls)) {
        return 0u;
    }
    field = cls->fields;
    if (!field || field->next || !cls->constructors) return 0u;
    for (constructor = cls->constructors; constructor;
         constructor = constructor->next) {
        unsigned arity;
        if (constructor->is_deleted || constructor->is_defaulted ||
            !constructor->initializer_is_single ||
            !constructor->body_is_empty ||
            !constructor->initializer_field ||
            strcmp(constructor->initializer_field, field->name) != 0) {
            return 0u;
        }
        arity = (unsigned)constructor->parameter_count;
        if (arity == 0u) {
            if (!constructor->initializer_value ||
                constructor->initializer_value->kind != EXPR_INT_LIT ||
                constructor->initializer_value->int_val != 0) {
                return 0u;
            }
        } else if (arity == 1u) {
            if (!constructor->parameter_name ||
                !constructor->initializer_value ||
                constructor->initializer_value->kind != EXPR_IDENT ||
                strcmp(constructor->initializer_value->ident_name,
                       constructor->parameter_name) != 0 ||
                !type_is_compatible(field->type,
                                    constructor->parameter_type)) {
                return 0u;
            }
        } else {
            return 0u;
        }
        mask |= UINT32_C(1) << arity;
    }
    return mask;
}

/* Parse class member (field or method) */
static void parse_class_member(CxxClass* cls, AccessSpec current_access) {
    SourceLoc loc = peek()->loc;

    skip_cxx_attributes();

    bool is_virtual = false;
    bool is_static = false;
    bool is_constexpr = false;
    bool is_explicit = false;

    /* C++ declaration specifiers can be combined in either order. */
    for (;;) {
        if (match(TOK_VIRTUAL)) is_virtual = true;
        else if (match(TOK_STATIC)) is_static = true;
        else if (match(TOK_CONSTEXPR)) is_constexpr = true;
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
        ParsedConstructorInitializer constructor_initializer = {0};

        if (!check(TOK_RPAREN)) {
            if (check(TOK_VOID) && parser.cur->next && parser.cur->next->type == TOK_RPAREN) {
                advance();
            } else {
                do {
                    Type* ptype = parse_cxx_type_spec();
                    const char* pname = NULL;
                    if (check(TOK_IDENT)) {
                        pname = advance()->value.str_val;
                    }
                    /* Default value? */
                    if (match(TOK_ASSIGN)) {
                        parse_cxx_expression();  /* Ignore for now */
                    }
                    Decl* p = decl_param(pname, ptype, param_idx++, peek()->loc);
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
            if (active_template) {
                skip_ctor_initializers();
            } else {
                constructor_initializer = parse_ctor_initializer();
            }
        }

        /* Method body or declaration */
        Stmt* body = NULL;
        if (active_template && check(TOK_LBRACE)) {
            /* Template member bodies are instantiated and parsed
             * semantically only after template arguments are known. */
            skip_balanced(TOK_LBRACE, TOK_RBRACE);
        } else if (match(TOK_LBRACE)) {
            /* Parse method body */
            StmtList* stmts = NULL;
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
        method->owner = cls;

        if (is_constructor) cls->has_user_constructor = true;

        if (is_constructor && !active_template) {
            CxxConstructorInfo* info = ast_arena_alloc(sizeof(*info));
            CxxConstructorInfo** tail = &cls->constructors;
            info->parameter_count = param_idx;
            info->parameter_name = params && param_idx == 1
                ? params->decl->name : NULL;
            info->parameter_type = params && param_idx == 1
                ? params->decl->type : NULL;
            info->initializer_field = constructor_initializer.field;
            info->initializer_value = constructor_initializer.value;
            info->initializer_is_single = constructor_initializer.is_single;
            info->body_is_empty = body && body->kind == STMT_BLOCK &&
                                  body->block_stmts == NULL;
            info->is_deleted = is_deleted;
            info->is_defaulted = is_defaulted;
            info->next = NULL;
            while (*tail) tail = &(*tail)->next;
            *tail = info;
        }

        cxx_class_add_method(cls, method);
    } else {
        /* Field */
        /* Array suffix? */
        if (match(TOK_LBRACKET)) {
            int len = -1;
            if (check(TOK_INT_LIT)) {
                len = (int)advance()->value.int_val;
            }
            expect(TOK_RBRACKET, "]");
            type = type_array(type, len);
        }

        /* Initializer? */
        Expr* init = NULL;
        if (match(TOK_ASSIGN)) {
            init = parse_cxx_expression();
        }
        if (init) cls->has_field_initializer = true;
        if (current_access != ACCESS_PUBLIC) {
            cls->has_nonpublic_field = true;
        }
        if (is_static) cls->has_static_field = true;

        expect(TOK_SEMICOLON, ";");

        /* Add field to class */
        cxx_class_add_field(cls, name, type, current_access);
    }
}

/* Parse class definition */
CxxClass* parse_cxx_class(void) {
    SourceLoc loc = previous()->loc;
    bool is_struct = previous()->type == TOK_STRUCT;
    bool has_definition = false;

    /* Class name */
    Token* name_tok = expect(TOK_IDENT, "class name");
    const char* class_name = name_tok ? name_tok->value.str_val : "anonymous";
    /* A final class has the same object layout as an otherwise identical
     * class; the semantic restriction is enforced when bases are resolved. */
    match(TOK_FINAL);

    CxxClass* cls = cxx_class_new(class_name, loc);
    cls->is_struct = is_struct;

    /* Inheritance */
    if (match(TOK_COLON)) {
        do {
            AccessSpec inherit_access = ACCESS_PRIVATE;
            if (match(TOK_PUBLIC)) inherit_access = ACCESS_PUBLIC;
            else if (match(TOK_PROTECTED)) inherit_access = ACCESS_PROTECTED;
            else if (match(TOK_PRIVATE)) inherit_access = ACCESS_PRIVATE;

            const char* base_name = parse_qualified_name();
            cxx_class_add_base(cls, base_name, inherit_access);
        } while (match(TOK_COMMA));
    }

    /* Class body */
    if (match(TOK_LBRACE)) {
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
    }

    /* Optional semicolon */
    match(TOK_SEMICOLON);

    /* A forward declaration has no layout yet. */
    if (!has_definition) return cls;

    cxx_class_compute_layout(cls);

    /* Aggregate classes and the validated one-field constructor subset can
     * reuse the common initializer/codegen backend. */
    if (!active_template && cls->type->is_complete) {
        uint32_t constructor_mask = lowerable_constructor_arity_mask(cls);
        if (constructor_mask != 0u) {
            rcc_parser_define_cxx_constructor_type(
                cls->name, cls->type, constructor_mask);
        } else if (!cls->has_user_constructor &&
                   !cls->has_nonpublic_field &&
                   !cls->has_static_field &&
                   !cls->has_field_initializer &&
                   cls->base_count == 0 &&
                   !class_has_virtual_member(cls)) {
            rcc_parser_define_type(cls->name, cls->type);
        }
    }

    return cls;
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

static CxxNamespace* parse_cxx_namespace(AST* ast, CxxNamespace* parent) {
    SourceLoc loc = previous()->loc;

    /* Namespace name (can be anonymous) */
    const char* ns_name = NULL;
    if (check(TOK_IDENT)) {
        ns_name = advance()->value.str_val;
    }

    CxxNamespace* ns = cxx_namespace_new(ns_name, loc);
    if (parent) cxx_namespace_add_namespace(parent, ns);

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
        } else if (check(TOK_CONSTEXPR) || check(TOK_INLINE) ||
                   check(TOK___INLINE__)) {
            bool is_constexpr = false;
            bool is_noexcept = false;
            Decl* declaration = parse_cxx_function_declaration(
                false, &is_constexpr, &is_noexcept);
            add_namespace_declaration(ast, ns, declaration);
        } else {
            Stmt* statement = parse_cxx_statement();
            if (statement && statement->kind == STMT_DECL) {
                add_namespace_declaration(ast, ns, statement->decl);
            }
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
        Type* type = parse_cxx_type_spec();
        const char* name = NULL;
        if (check(TOK_IDENT)) name = advance()->value.str_val;
        if (match(TOK_ASSIGN)) {
            (void)parse_cxx_expression();
        }
        decllist_append(&params,
                        decl_param(name, type, param_idx++, peek()->loc));
        if (!match(TOK_COMMA)) break;
    }
    return params;
}

static bool inline_constructor_parameters_supported(Type* return_type,
                                                     DeclList* params) {
    DeclList* parameter;
    if (rcc_parser_cxx_constructor_arity_mask(return_type) == 0u) {
        return true;
    }
    /* References currently share the pointer representation in the common
     * AST.  Defer those wrappers until reference address/value semantics are
     * represented explicitly instead of compiling an incorrect body. */
    for (parameter = params; parameter; parameter = parameter->next) {
        if (parameter->decl->type->kind == TYPE_PTR) return false;
    }
    return true;
}

static Decl* parse_cxx_function_declaration(bool parse_body,
                                            bool* is_constexpr,
                                            bool* is_noexcept) {
    SourceLoc loc;
    Type* return_type;
    Token* name;
    DeclList* params;
    Stmt* body = NULL;
    bool is_inline = false;

    *is_constexpr = false;
    *is_noexcept = false;
    skip_cxx_attributes();
    loc = peek()->loc;
    for (;;) {
        if (match(TOK_CONSTEXPR)) *is_constexpr = true;
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
    /* Header-only SDK helpers returning a complete C ABI aggregate can be
     * lowered through the common C statement/initializer pipeline.  Keep
     * incomplete class and template return types deferred until their C++
     * object model is implemented. */
    if (!parse_body && is_inline && type_is_complete(return_type) &&
        (return_type->kind == TYPE_STRUCT ||
         return_type->kind == TYPE_UNION) &&
        inline_constructor_parameters_supported(return_type, params) &&
        check(TOK_LBRACE) && parser.cur->next &&
        parser.cur->next->type == TOK_RETURN) {
        parse_body = true;
    }
    if (!parse_body && check(TOK_LBRACE)) {
        skip_balanced(TOK_LBRACE, TOK_RBRACE);
    } else if (match(TOK_LBRACE)) {
        StmtList* statements = NULL;
        while (!check(TOK_RBRACE) && !at_end()) {
            Token* start = parser.cur;
            Stmt* statement = parse_cxx_statement();
            if (statement) stmtlist_append(&statements, statement);
            if (parser.cur == start && !at_end()) advance();
        }
        expect(TOK_RBRACE, "}");
        body = stmt_block(statements, loc);
    } else {
        expect(TOK_SEMICOLON, ";");
    }

    CxxMethod* function = cxx_method_new(name->value.str_val, return_type,
                                         params, body, loc);
    function->decl->func_is_inline = is_inline;
    return function->decl;
}

/* ═══════════════════════════════════════
 * C++ Template Parsing
 * ═══════════════════════════════════════ */

CxxTemplate* parse_cxx_template(void) {
    SourceLoc loc = previous()->loc;

    expect(TOK_LT, "<");

    CxxTemplate* tmpl = cxx_template_new(loc);

    /* Parse template parameters */
    if (!check(TOK_GT)) {
        do {
            if (match(TOK_TYPENAME) || match(TOK_CLASS)) {
                /* Type parameter */
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
                if (tmpl->param_count > 0) {
                    tmpl->params[tmpl->param_count - 1].has_default = true;
                }
                /* Skip default for now */
                int depth = 0;
                while (!at_end()) {
                    if (check(TOK_COMMA) && depth == 0) break;
                    if (check(TOK_GT) && depth == 0) break;
                    if (match(TOK_LT)) depth++;
                    else if (match(TOK_GT)) depth--;
                    else advance();
                }
            }
        } while (match(TOK_COMMA));
    }

    expect(TOK_GT, ">");

    /* Template body */
    if (match(TOK_CLASS) || match(TOK_STRUCT)) {
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
        active_template = tmpl;
        tmpl->func_def = parse_cxx_function_declaration(
            true, &tmpl->is_constexpr, &tmpl->is_noexcept);
        active_template = outer_template;
        if (tmpl->func_def) {
            tmpl->name = ast_arena_strdup(tmpl->func_def->name);
        }
    }

    return tmpl;
}

/* ═══════════════════════════════════════
 * C++ Type Parsing
 * ═══════════════════════════════════════ */

static Type* parse_cxx_type_spec(void) {
    Type* t = NULL;
    bool is_unsigned = false;
    bool is_const = false;
    bool is_long = false;
    bool is_short = false;

    /* Qualifiers */
    while (1) {
        if (match(TOK_CONST)) is_const = true;
        else if (match(TOK_VOLATILE)) { /* ignore */ }
        else if (match(TOK_UNSIGNED)) is_unsigned = true;
        else if (match(TOK_SIGNED)) is_unsigned = false;
        else if (match(TOK_LONG)) is_long = true;
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
    } else if (match(TOK_INT) || is_long || is_short) {
        if (is_short) {
            t = is_unsigned ? type_ushort : type_short;
        } else if (is_long) {
            t = is_unsigned ? type_ulong : type_long;
        } else {
            t = is_unsigned ? type_uint : type_int;
        }
    } else if (match(TOK_FLOAT)) {
        t = type_float;
    } else if (match(TOK_DOUBLE)) {
        t = type_double;
    } else if (check(TOK_IDENT) || check(TOK_SCOPE)) {
        /* Class or namespace qualified type */
        const char* name = parse_qualified_name();
        Type* known_type = strstr(name, "::") == NULL
            ? rcc_parser_lookup_type(name) : NULL;
        if (check(TOK_LT)) skip_cxx_template_arguments();
        t = known_type ? known_type : type_struct(name);
    } else {
        /* Default to int */
        t = is_unsigned ? type_uint : type_int;
    }

    /* Prefix cv-qualifiers apply to the base type, before pointer and
     * reference declarators are layered on top. */
    if (is_const && t) {
        Type* ct = ast_arena_alloc(sizeof(Type));
        *ct = *t;
        ct->is_const = true;
        t = ct;
    }

    /* Reference and pointer */
    while (1) {
        if (match(TOK_STAR)) {
            t = type_ptr(t);
            while (match(TOK_CONST)) t->is_const = true;
        } else if (match(TOK_AMP)) {
            /* Reference - treat as pointer internally */
            t = type_ptr(t);
            /* Mark as reference somehow? */
        } else if (match(TOK_AND)) {
            /* Rvalue reference - use the same lowered representation. */
            t = type_ptr(t);
        } else {
            break;
        }
    }

    return t;
}

/* ═══════════════════════════════════════
 * C++ Expression Parsing (simplified)
 * ═══════════════════════════════════════ */

/* Forward declaration */
extern Expr* parse_expression(void);

static Expr* parse_cxx_primary(void) {
    SourceLoc loc = peek()->loc;

    /* this */
    if (match(TOK_THIS)) {
        Expr* e = expr_ident("this", loc);
        return e;
    }

    /* nullptr */
    if (match(TOK_NULLPTR)) {
        return expr_int(0, loc);  /* Treat as null pointer */
    }

    /* true/false */
    if (match(TOK_TRUE)) {
        return expr_int(1, loc);
    }
    if (match(TOK_FALSE)) {
        return expr_int(0, loc);
    }

    /* new expression */
    if (match(TOK_NEW)) {
        Type* t = parse_cxx_type_spec();
        /* TODO: implement new properly */
        return expr_int(0, loc);  /* Placeholder */
        (void)t;
    }

    /* delete expression */
    if (match(TOK_DELETE)) {
        bool is_array = match(TOK_LBRACKET);
        if (is_array) expect(TOK_RBRACKET, "]");
        Expr* e = parse_cxx_expression();
        /* TODO: implement delete properly */
        return e;
    }

    /* Fall back to C expression parsing */
    return parse_expression();
}

static Expr* parse_cxx_expression(void) {
    return parse_cxx_primary();
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

    if (match(TOK_AUTO)) {
        /* Deduction is deferred until template instantiation. */
        type = type_int;
    } else {
        type = parse_cxx_type_spec();
    }
    name = expect(TOK_IDENT, "local variable name");
    if (!name) return NULL;
    if (match(TOK_ASSIGN)) {
        if (check(TOK_LBRACE)) {
            skip_balanced(TOK_LBRACE, TOK_RBRACE);
        } else {
            initializer = parse_cxx_expression();
        }
    } else if (check(TOK_LBRACE)) {
        skip_balanced(TOK_LBRACE, TOK_RBRACE);
    }
    expect(TOK_SEMICOLON, ";");
    return stmt_decl(decl_var(name->value.str_val, type, initializer, loc),
                     loc);
}

static Stmt* parse_cxx_statement(void) {
    if (check(TOK_AUTO) ||
        (check(TOK_IDENT) &&
         is_active_template_type(peek()->value.str_val))) {
        return parse_cxx_dependent_local_declaration();
    }

    /* try-catch */
    if (match(TOK_TRY)) {
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

        return stmt_block(stmts, previous()->loc);
    }

    /* throw */
    if (match(TOK_THROW)) {
        if (!check(TOK_SEMICOLON)) {
            parse_cxx_expression();
        }
        expect(TOK_SEMICOLON, ";");
        return stmt_null(previous()->loc);  /* Placeholder */
    }

    /* Fall back to C statement parsing */
    return parse_declaration();
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
        } else if (check(TOK_CONSTEXPR) || check(TOK_INLINE) ||
                   check(TOK___INLINE__)) {
            bool is_constexpr = false;
            bool is_noexcept = false;
            Decl* declaration = parse_cxx_function_declaration(
                false, &is_constexpr, &is_noexcept);
            if (g_global_namespace && declaration) {
                cxx_namespace_add_decl(g_global_namespace, declaration);
            }
        } else if (match(TOK_USING)) {
            /* using declaration or directive */
            if (match(TOK_NAMESPACE)) {
                /* using namespace ns; */
                parse_qualified_name();
            } else {
                /* using alias = type; or using ns::name; */
                if (check(TOK_IDENT)) {
                    advance();
                    if (match(TOK_ASSIGN)) {
                        parse_cxx_type_spec();
                    }
                }
            }
            expect(TOK_SEMICOLON, ";");
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

    return ast;
}
