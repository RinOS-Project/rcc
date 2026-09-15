/*
 * RCC - RinOS C Compiler
 * Lexical analyzer (Lexer)
 */

#include "rcc.h"
#include "token.h"
#include <ctype.h>
#include <errno.h>

/* Lexer state */
typedef struct {
    const char* filename;
    const char* src;
    const char* pos;
    int line;
    int column;
    int line_start;
} Lexer;

/* Keyword table */
static struct {
    const char* name;
    TokenType type;
} keywords[] = {
    {"auto", TOK_AUTO},
    {"break", TOK_BREAK},
    {"case", TOK_CASE},
    {"char", TOK_CHAR},
    {"const", TOK_CONST},
    {"continue", TOK_CONTINUE},
    {"default", TOK_DEFAULT},
    {"do", TOK_DO},
    {"double", TOK_DOUBLE},
    {"else", TOK_ELSE},
    {"enum", TOK_ENUM},
    {"extern", TOK_EXTERN},
    {"float", TOK_FLOAT},
    {"for", TOK_FOR},
    {"goto", TOK_GOTO},
    {"if", TOK_IF},
    {"inline", TOK_INLINE},
    {"int", TOK_INT},
    {"long", TOK_LONG},
    {"register", TOK_REGISTER},
    {"restrict", TOK_RESTRICT},
    {"return", TOK_RETURN},
    {"short", TOK_SHORT},
    {"signed", TOK_SIGNED},
    {"sizeof", TOK_SIZEOF},
    {"static", TOK_STATIC},
    {"struct", TOK_STRUCT},
    {"switch", TOK_SWITCH},
    {"typedef", TOK_TYPEDEF},
    {"union", TOK_UNION},
    {"unsigned", TOK_UNSIGNED},
    {"void", TOK_VOID},
    {"volatile", TOK_VOLATILE},
    {"while", TOK_WHILE},
    {"_Bool", TOK__BOOL},
    {"_Complex", TOK__COMPLEX},
    {"_Imaginary", TOK__IMAGINARY},
    {"_Atomic", TOK__ATOMIC},
    {"_Noreturn", TOK__NORETURN},
    {"_Alignof", TOK__ALIGNOF},
    {"_Alignas", TOK__ALIGNAS},
    {"_Static_assert", TOK_STATIC_ASSERT},
    {"static_assert", TOK_STATIC_ASSERT},
    {"_Generic", TOK_GENERIC},
    {"_Thread_local", TOK_THREAD_LOCAL},
    /* GNU Extensions */
    {"asm", TOK_ASM},
    {"__asm", TOK_ASM},
    {"__asm__", TOK_ASM},
    {"__volatile__", TOK___VOLATILE__},
    {"__inline__", TOK___INLINE__},
    {"__inline", TOK___INLINE__},
    {"__attribute__", TOK___ATTRIBUTE__},
    {"__extension__", TOK___EXTENSION__},
    {"__builtin_va_list", TOK___BUILTIN_VA_LIST},
    /* C++ keywords */
    {"class", TOK_CLASS},
    {"namespace", TOK_NAMESPACE},
    {"template", TOK_TEMPLATE},
    {"typename", TOK_TYPENAME},
    {"public", TOK_PUBLIC},
    {"private", TOK_PRIVATE},
    {"protected", TOK_PROTECTED},
    {"virtual", TOK_VIRTUAL},
    {"override", TOK_OVERRIDE},
    {"final", TOK_FINAL},
    {"new", TOK_NEW},
    {"delete", TOK_DELETE},
    {"this", TOK_THIS},
    {"nullptr", TOK_NULLPTR},
    {"true", TOK_TRUE},
    {"false", TOK_FALSE},
    {"bool", TOK_BOOL},
    {"throw", TOK_THROW},
    {"try", TOK_TRY},
    {"catch", TOK_CATCH},
    {"using", TOK_USING},
    {"operator", TOK_OPERATOR},
    {"friend", TOK_FRIEND},
    {"explicit", TOK_EXPLICIT},
    {"mutable", TOK_MUTABLE},
    {"constexpr", TOK_CONSTEXPR},
    {"consteval", TOK_CONSTEVAL},
    {"requires", TOK_REQUIRES},
    {"thread_local", TOK_THREAD_LOCAL},
    {"noexcept", TOK_NOEXCEPT},
    {"static_cast", TOK_STATIC_CAST},
    {"dynamic_cast", TOK_DYNAMIC_CAST},
    {"reinterpret_cast", TOK_REINTERPRET_CAST},
    {"const_cast", TOK_CONST_CAST},
    {NULL, TOK_EOF}
};

TokenType keyword_lookup(const char* str) {
    for (int i = 0; keywords[i].name; i++) {
        if (strcmp(keywords[i].name, str) == 0) {
            return keywords[i].type;
        }
    }
    return TOK_IDENT;
}

const char* token_type_str(TokenType type) {
    switch (type) {
        case TOK_EOF: return "EOF";
        case TOK_IDENT: return "identifier";
        case TOK_INT_LIT: return "integer";
        case TOK_FLOAT_LIT: return "float";
        case TOK_CHAR_LIT: return "char";
        case TOK_STRING_LIT: return "string";
        case TOK_LPAREN: return "(";
        case TOK_RPAREN: return ")";
        case TOK_LBRACKET: return "[";
        case TOK_RBRACKET: return "]";
        case TOK_LBRACE: return "{";
        case TOK_RBRACE: return "}";
        case TOK_SEMICOLON: return ";";
        case TOK_COMMA: return ",";
        case TOK_DOT: return ".";
        case TOK_ARROW: return "->";
        case TOK_PLUS: return "+";
        case TOK_MINUS: return "-";
        case TOK_STAR: return "*";
        case TOK_SLASH: return "/";
        case TOK_PERCENT: return "%";
        case TOK_AMP: return "&";
        case TOK_PIPE: return "|";
        case TOK_CARET: return "^";
        case TOK_TILDE: return "~";
        case TOK_NOT: return "!";
        case TOK_ASSIGN: return "=";
        case TOK_EQ: return "==";
        case TOK_NE: return "!=";
        case TOK_LT: return "<";
        case TOK_GT: return ">";
        case TOK_LE: return "<=";
        case TOK_GE: return ">=";
        case TOK_AND: return "&&";
        case TOK_OR: return "||";
        case TOK_INC: return "++";
        case TOK_DEC: return "--";
        case TOK_LSHIFT: return "<<";
        case TOK_RSHIFT: return ">>";
        case TOK_QUESTION: return "?";
        case TOK_COLON: return ":";
        case TOK_ELLIPSIS: return "...";
        case TOK_PRAGMA_PACK: return "#pragma pack";
        default:
            for (int i = 0; keywords[i].name; i++) {
                if (keywords[i].type == type) {
                    return keywords[i].name;
                }
            }
            return "unknown";
    }
}

/* Token allocation */
Token* token_new(TokenType type, SourceLoc loc) {
    Token* tok = rcc_alloc(sizeof(Token));
    tok->type = type;
    tok->loc = loc;
    tok->next = NULL;
    memset(&tok->value, 0, sizeof(tok->value));
    tok->int_base = 0u;
    tok->int_long_suffix = 0u;
    tok->int_unsigned_suffix = false;
    tok->int_overflow = false;
    return tok;
}

void token_free(Token* tok) {
    rcc_free(tok);
}

TokenList* tokenlist_new(void) {
    TokenList* list = rcc_alloc(sizeof(TokenList));
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
    return list;
}

void tokenlist_free(TokenList* list) {
    Token* tok = list->head;
    while (tok) {
        Token* next = tok->next;
        token_free(tok);
        tok = next;
    }
    rcc_free(list);
}

void tokenlist_append(TokenList* list, Token* tok) {
    if (!list->head) {
        list->head = tok;
        list->tail = tok;
    } else {
        list->tail->next = tok;
        list->tail = tok;
    }
    list->count++;
}

/* Lexer helpers */
static char peek(Lexer* lex) {
    return *lex->pos;
}

static char peek_next(Lexer* lex) {
    if (*lex->pos == '\0') return '\0';
    return lex->pos[1];
}

static char advance(Lexer* lex) {
    char c = *lex->pos++;
    if (c == '\n') {
        lex->line++;
        lex->column = 1;
        lex->line_start = lex->pos - lex->src;
    } else {
        lex->column++;
    }
    return c;
}

static bool match(Lexer* lex, char expected) {
    if (*lex->pos == expected) {
        advance(lex);
        return true;
    }
    return false;
}

static SourceLoc make_loc(Lexer* lex) {
    SourceLoc loc = {
        .filename = lex->filename,
        .line = lex->line,
        .column = lex->column
    };
    return loc;
}

static void skip_whitespace(Lexer* lex) {
    while (1) {
        char c = peek(lex);
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                advance(lex);
                break;
            case '/':
                if (peek_next(lex) == '/') {
                    /* Single-line comment */
                    while (peek(lex) && peek(lex) != '\n') {
                        advance(lex);
                    }
                } else if (peek_next(lex) == '*') {
                    /* Multi-line comment */
                    advance(lex);
                    advance(lex);
                    while (peek(lex)) {
                        if (peek(lex) == '*' && peek_next(lex) == '/') {
                            advance(lex);
                            advance(lex);
                            break;
                        }
                        advance(lex);
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static Token* lex_identifier(Lexer* lex) {
    SourceLoc loc = make_loc(lex);
    const char* start = lex->pos;

    while (isalnum(peek(lex)) || peek(lex) == '_') {
        advance(lex);
    }

    size_t len = lex->pos - start;
    char* str = rcc_alloc(len + 1);
    memcpy(str, start, len);
    str[len] = '\0';

    TokenType type = keyword_lookup(str);
    Token* tok = token_new(type, loc);

    if (type == TOK_IDENT) {
        tok->value.str_val = rcc_intern(str);
    }
    rcc_free(str);

    return tok;
}

static Token* lex_number(Lexer* lex) {
    SourceLoc loc = make_loc(lex);
    const char* start = lex->pos;
    bool is_float = false;
    int base = 10;
    const char* suffix_start;

    /* Check for hex/octal/binary prefix */
    if (peek(lex) == '0') {
        advance(lex);
        if (peek(lex) == 'x' || peek(lex) == 'X') {
            base = 16;
            advance(lex);
        } else if (peek(lex) == 'b' || peek(lex) == 'B') {
            base = 2;
            advance(lex);
        } else if (isdigit(peek(lex))) {
            base = 8;
        }
    }

    /* Read digits */
    while (1) {
        char c = peek(lex);
        if (base == 16 && isxdigit(c)) {
            advance(lex);
        } else if (base == 10 && isdigit(c)) {
            advance(lex);
        } else if (base == 8 && c >= '0' && c <= '7') {
            advance(lex);
        } else if (base == 2 && (c == '0' || c == '1')) {
            advance(lex);
        } else {
            break;
        }
    }

    /* Check for decimal point */
    if (base == 10 && peek(lex) == '.' && isdigit(peek_next(lex))) {
        is_float = true;
        advance(lex);
        while (isdigit(peek(lex))) {
            advance(lex);
        }
    }

    /* Check for exponent */
    if (base == 10 && (peek(lex) == 'e' || peek(lex) == 'E')) {
        is_float = true;
        advance(lex);
        if (peek(lex) == '+' || peek(lex) == '-') {
            advance(lex);
        }
        while (isdigit(peek(lex))) {
            advance(lex);
        }
    }

    suffix_start = lex->pos;
    if (is_float) {
        if (peek(lex) == 'f' || peek(lex) == 'F' ||
            peek(lex) == 'l' || peek(lex) == 'L') {
            advance(lex);
        }
    } else {
        while (peek(lex) == 'u' || peek(lex) == 'U' ||
               peek(lex) == 'l' || peek(lex) == 'L') {
            advance(lex);
        }
    }

    size_t len = lex->pos - start;
    char* str = rcc_alloc(len + 1);
    memcpy(str, start, len);
    str[len] = '\0';

    Token* tok;
    if (is_float) {
        if (suffix_start < lex->pos &&
            (*suffix_start == 'l' || *suffix_start == 'L')) {
            rcc_error(loc,
                      "long double literals are not supported by the RinOS floating-point ABI");
        }
        tok = token_new(TOK_FLOAT_LIT, loc);
        tok->value.float_val = strtod(str, NULL);
        tok->float_suffix = suffix_start < lex->pos &&
                            (*suffix_start == 'f' ||
                             *suffix_start == 'F');
    } else {
        const char* suffix = suffix_start;
        bool suffix_valid = true;
        bool saw_unsigned = false;
        unsigned long_suffix = 0u;
        uint64_t value;

        while (suffix < lex->pos) {
            if (*suffix == 'u' || *suffix == 'U') {
                if (saw_unsigned) {
                    suffix_valid = false;
                    break;
                }
                saw_unsigned = true;
                ++suffix;
            } else if (*suffix == 'l' || *suffix == 'L') {
                char long_case = *suffix++;
                if (long_suffix != 0u) {
                    suffix_valid = false;
                    break;
                }
                long_suffix = 1u;
                if (suffix < lex->pos && *suffix == long_case) {
                    long_suffix = 2u;
                    ++suffix;
                }
            } else {
                suffix_valid = false;
                break;
            }
        }
        if (!suffix_valid) {
            rcc_error(loc, "invalid integer literal suffix");
        }
        errno = 0;
        value = strtoull(str, NULL, base);
        tok = token_new(TOK_INT_LIT, loc);
        tok->value.int_val = (int64_t)value;
        tok->int_base = (uint8_t)base;
        tok->int_long_suffix = (uint8_t)long_suffix;
        tok->int_unsigned_suffix = saw_unsigned;
        tok->int_overflow = errno == ERANGE;
        if (tok->int_overflow) {
            rcc_error(loc, "integer literal is too large for 64-bit C types");
        }
    }

    rcc_free(str);
    return tok;
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static char lex_escape(Lexer* lex) {
    char c = advance(lex);
    switch (c) {
        case 'a': return '\a';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'v': return '\v';
        case '\\': return '\\';
        case '\'': return '\'';
        case '"': return '"';
        case '?': return '?';
        case '0': return '\0';
        case 'x': {
            int val = 0;
            for (int i = 0; i < 2 && isxdigit(peek(lex)); i++) {
                val = val * 16 + hex_digit(advance(lex));
            }
            return (char)val;
        }
        default:
            return c;
    }
}

static Token* lex_char(Lexer* lex) {
    SourceLoc loc = make_loc(lex);
    advance(lex); /* skip ' */

    char c;
    if (peek(lex) == '\\') {
        advance(lex);
        c = lex_escape(lex);
    } else {
        c = advance(lex);
    }

    if (peek(lex) != '\'') {
        rcc_error(loc, "unterminated character literal");
    }
    advance(lex);

    Token* tok = token_new(TOK_CHAR_LIT, loc);
    tok->value.char_val = c;
    return tok;
}

static Token* lex_string(Lexer* lex) {
    SourceLoc loc = make_loc(lex);
    advance(lex); /* skip " */

    char buf[RCC_MAX_STRING];
    int len = 0;

    while (peek(lex) && peek(lex) != '"') {
        if (len >= RCC_MAX_STRING - 1) {
            rcc_error(loc, "string too long");
            break;
        }
        if (peek(lex) == '\\') {
            advance(lex);
            buf[len++] = lex_escape(lex);
        } else if (peek(lex) == '\n') {
            rcc_error(loc, "newline in string literal");
            break;
        } else {
            buf[len++] = advance(lex);
        }
    }

    if (peek(lex) != '"') {
        rcc_error(loc, "unterminated string literal");
    }
    advance(lex);

    buf[len] = '\0';
    Token* tok = token_new(TOK_STRING_LIT, loc);
    tok->value.str_val = rcc_intern(buf);
    return tok;
}

static Token* lex_token(Lexer* lex) {
    skip_whitespace(lex);

    if (peek(lex) == '\0') {
        return token_new(TOK_EOF, make_loc(lex));
    }

    SourceLoc loc = make_loc(lex);
    char c = peek(lex);

    /* Identifier or keyword */
    if (isalpha(c) || c == '_') {
        return lex_identifier(lex);
    }

    /* Number */
    if (isdigit(c)) {
        return lex_number(lex);
    }

    /* Character literal */
    if (c == '\'') {
        return lex_char(lex);
    }

    /* String literal */
    if (c == '"') {
        return lex_string(lex);
    }

    /* Operators and punctuation */
    advance(lex);
    switch (c) {
        case '(': return token_new(TOK_LPAREN, loc);
        case ')': return token_new(TOK_RPAREN, loc);
        case '[': return token_new(TOK_LBRACKET, loc);
        case ']': return token_new(TOK_RBRACKET, loc);
        case '{': return token_new(TOK_LBRACE, loc);
        case '}': return token_new(TOK_RBRACE, loc);
        case ';': return token_new(TOK_SEMICOLON, loc);
        case ',': return token_new(TOK_COMMA, loc);
        case '~': return token_new(TOK_TILDE, loc);
        case '?': return token_new(TOK_QUESTION, loc);
        case ':':
            if (match(lex, ':')) return token_new(TOK_SCOPE, loc);  /* :: */
            return token_new(TOK_COLON, loc);

        case '.':
            if (peek(lex) == '.' && peek_next(lex) == '.') {
                advance(lex);
                advance(lex);
                return token_new(TOK_ELLIPSIS, loc);
            }
            if (match(lex, '*')) return token_new(TOK_DOT_STAR, loc);  /* .* */
            return token_new(TOK_DOT, loc);

        case '+':
            if (match(lex, '+')) return token_new(TOK_INC, loc);
            if (match(lex, '=')) return token_new(TOK_PLUS_ASSIGN, loc);
            return token_new(TOK_PLUS, loc);

        case '-':
            if (match(lex, '-')) return token_new(TOK_DEC, loc);
            if (match(lex, '=')) return token_new(TOK_MINUS_ASSIGN, loc);
            if (match(lex, '>')) {
                if (match(lex, '*')) return token_new(TOK_ARROW_STAR, loc);  /* ->* */
                return token_new(TOK_ARROW, loc);
            }
            return token_new(TOK_MINUS, loc);

        case '*':
            if (match(lex, '=')) return token_new(TOK_STAR_ASSIGN, loc);
            return token_new(TOK_STAR, loc);

        case '/':
            if (match(lex, '=')) return token_new(TOK_SLASH_ASSIGN, loc);
            return token_new(TOK_SLASH, loc);

        case '%':
            if (match(lex, '=')) return token_new(TOK_PERCENT_ASSIGN, loc);
            return token_new(TOK_PERCENT, loc);

        case '&':
            if (match(lex, '&')) return token_new(TOK_AND, loc);
            if (match(lex, '=')) return token_new(TOK_AMP_ASSIGN, loc);
            return token_new(TOK_AMP, loc);

        case '|':
            if (match(lex, '|')) return token_new(TOK_OR, loc);
            if (match(lex, '=')) return token_new(TOK_PIPE_ASSIGN, loc);
            return token_new(TOK_PIPE, loc);

        case '^':
            if (match(lex, '=')) return token_new(TOK_CARET_ASSIGN, loc);
            return token_new(TOK_CARET, loc);

        case '!':
            if (match(lex, '=')) return token_new(TOK_NE, loc);
            return token_new(TOK_NOT, loc);

        case '=':
            if (match(lex, '=')) return token_new(TOK_EQ, loc);
            return token_new(TOK_ASSIGN, loc);

        case '<':
            if (match(lex, '<')) {
                if (match(lex, '=')) return token_new(TOK_LSHIFT_ASSIGN, loc);
                return token_new(TOK_LSHIFT, loc);
            }
            if (match(lex, '=')) return token_new(TOK_LE, loc);
            return token_new(TOK_LT, loc);

        case '>':
            if (match(lex, '>')) {
                if (match(lex, '=')) return token_new(TOK_RSHIFT_ASSIGN, loc);
                return token_new(TOK_RSHIFT, loc);
            }
            if (match(lex, '=')) return token_new(TOK_GE, loc);
            return token_new(TOK_GT, loc);

        case '#':
            if (match(lex, '#')) return token_new(TOK_HASHHASH, loc);
            return token_new(TOK_HASH, loc);

        default:
            rcc_error(loc, "unexpected character '%c'", c);
            return lex_token(lex);
    }
}

/* Handle #line directive from preprocessor */
static void handle_line_directive(Lexer* lex) {
    /* Skip "#line" */
    while (*lex->pos && !isspace(*lex->pos)) lex->pos++;
    while (*lex->pos == ' ' || *lex->pos == '\t') lex->pos++;

    /* Read line number */
    int line = 0;
    while (isdigit(*lex->pos)) {
        line = line * 10 + (*lex->pos - '0');
        lex->pos++;
    }
    if (line > 0) {
        lex->line = line;
    }

    /* Skip whitespace */
    while (*lex->pos == ' ' || *lex->pos == '\t') lex->pos++;

    /* Read filename if present */
    if (*lex->pos == '"') {
        lex->pos++;
        const char* name_start = lex->pos;
        while (*lex->pos && *lex->pos != '"' && *lex->pos != '\n') {
            lex->pos++;
        }
        if (lex->pos > name_start) {
            size_t len = lex->pos - name_start;
            char* name = rcc_alloc(len + 1);
            memcpy(name, name_start, len);
            name[len] = '\0';
            lex->filename = rcc_intern(name);
            rcc_free(name);
        }
        if (*lex->pos == '"') lex->pos++;
    }

    /* Skip to end of line */
    while (*lex->pos && *lex->pos != '\n') lex->pos++;
    if (*lex->pos == '\n') {
        lex->pos++;
        lex->column = 1;
    }
}

/* Preserve the ordering of #pragma pack directives in the token stream.
 * Token values use -1 for pop, 0 for reset, 1..16 for set, and 256+n for
 * push (n == 0 keeps the current alignment). */
static Token* handle_pragma_directive(Lexer* lex) {
    SourceLoc loc = make_loc(lex);
    const char* p = lex->pos + 7; /* strlen("#pragma") */
    int value = 0;
    bool recognized = false;

    while (*p == ' ' || *p == '\t') p++;
    if (strncmp(p, "pack", 4) == 0 &&
        !(isalnum((unsigned char)p[4]) || p[4] == '_')) {
        p += 4;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '(') {
            p++;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == ')') {
                recognized = true; /* reset */
            } else if (strncmp(p, "pop", 3) == 0 &&
                       !(isalnum((unsigned char)p[3]) || p[3] == '_')) {
                value = -1;
                recognized = true;
            } else if (strncmp(p, "push", 4) == 0 &&
                       !(isalnum((unsigned char)p[4]) || p[4] == '_')) {
                int alignment = 0;
                p += 4;
                while (*p == ' ' || *p == '\t') p++;
                if (*p == ',') {
                    p++;
                    while (*p == ' ' || *p == '\t') p++;
                    while (isdigit((unsigned char)*p)) {
                        alignment = alignment * 10 + (*p - '0');
                        p++;
                    }
                }
                value = 256 + alignment;
                recognized = true;
            } else if (isdigit((unsigned char)*p)) {
                while (isdigit((unsigned char)*p)) {
                    value = value * 10 + (*p - '0');
                    p++;
                }
                recognized = true;
            }
        }
    }

    while (*lex->pos && *lex->pos != '\n') advance(lex);
    if (*lex->pos == '\n') advance(lex);
    if (!recognized) return NULL;

    Token* token = token_new(TOK_PRAGMA_PACK, loc);
    token->value.int_val = value;
    return token;
}

/* Lex from string */
TokenList* rcc_lex_string(const char* src, const char* filename) {
    /* Initialize lexer */
    Lexer lex = {
        .filename = filename,
        .src = src,
        .pos = src,
        .line = 1,
        .column = 1,
        .line_start = 0
    };

    /* Tokenize */
    TokenList* list = tokenlist_new();
    while (1) {
        /* Check for #line directive from preprocessor */
        skip_whitespace(&lex);
        if (*lex.pos == '#' && strncmp(lex.pos, "#line", 5) == 0) {
            handle_line_directive(&lex);
            continue;
        }
        if (*lex.pos == '#' && strncmp(lex.pos, "#pragma", 7) == 0) {
            Token* pragma = handle_pragma_directive(&lex);
            if (pragma) tokenlist_append(list, pragma);
            continue;
        }

        Token* tok = lex_token(&lex);
        if (tok->type == TOK_STRING_LIT && list->tail &&
            list->tail->type == TOK_STRING_LIT) {
            size_t left_length = strlen(list->tail->value.str_val);
            size_t right_length = strlen(tok->value.str_val);
            char* joined = rcc_alloc(left_length + right_length + 1u);
            memcpy(joined, list->tail->value.str_val, left_length);
            memcpy(joined + left_length, tok->value.str_val,
                   right_length + 1u);
            list->tail->value.str_val = rcc_intern(joined);
            rcc_free(joined);
            token_free(tok);
            continue;
        }
        tokenlist_append(list, tok);
        if (tok->type == TOK_EOF) break;
    }

    return list;
}

/* Main lexer function */
TokenList* rcc_lex(const char* filename) {
    /* Read file */
    FILE* f = fopen(filename, "rb");
    if (!f) {
        rcc_fatal("cannot open file '%s'", filename);
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        rcc_fatal("cannot seek source file '%s'", filename);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        rcc_fatal("cannot determine source file size '%s'", filename);
        return NULL;
    }

    char* src = rcc_alloc((size_t)size + 1u);
    if (fread(src, 1, (size_t)size, f) != (size_t)size) {
        rcc_free(src);
        fclose(f);
        rcc_fatal("cannot read source file '%s'", filename);
        return NULL;
    }
    src[size] = '\0';
    fclose(f);

    TokenList* list = rcc_lex_string(src, filename);
    rcc_free(src);
    return list;
}
