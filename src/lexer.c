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

static bool lexer_cxx_mode;
static int lexer_cxx_standard = 20;

void rcc_lexer_set_cxx_mode(bool enabled) {
    lexer_cxx_mode = enabled;
}

void rcc_lexer_set_cxx_standard(int standard) {
    lexer_cxx_standard = standard;
}

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
    {"char16_t", TOK_CHAR16_T},
    {"char32_t", TOK_CHAR32_T},
    {"char8_t", TOK_CHAR8_T},
    {"wchar_t", TOK_WCHAR_T},
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
    {"constinit", TOK_CONSTINIT},
    {"decltype", TOK_DECLTYPE},
    {"concept", TOK_CONCEPT},
    {"alignas", TOK__ALIGNAS},
    {"alignof", TOK__ALIGNOF},
    {"requires", TOK_REQUIRES},
    {"thread_local", TOK_THREAD_LOCAL},
    {"noexcept", TOK_NOEXCEPT},
    {"static_cast", TOK_STATIC_CAST},
    {"dynamic_cast", TOK_DYNAMIC_CAST},
    {"reinterpret_cast", TOK_REINTERPRET_CAST},
    {"const_cast", TOK_CONST_CAST},
    {"typeid", TOK_TYPEID},
    /* C++ alternative operator spellings.  These are keywords only in C++;
     * lex_identifier maps them back to ordinary identifiers for C17. */
    {"and", TOK_AND},
    {"and_eq", TOK_AMP_ASSIGN},
    {"bitand", TOK_AMP},
    {"bitor", TOK_PIPE},
    {"compl", TOK_TILDE},
    {"not", TOK_NOT},
    {"not_eq", TOK_NE},
    {"or", TOK_OR},
    {"or_eq", TOK_PIPE_ASSIGN},
    {"xor", TOK_CARET},
    {"xor_eq", TOK_CARET_ASSIGN},
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
        case TOK_SPACESHIP: return "<=>";
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
    tok->user_literal_suffix = NULL;
    tok->is_utf8_literal = false;
    tok->is_wide_literal = false;
    tok->char_element_width = 1u;
    tok->is_multichar_literal = false;
    tok->string_length = 0u;
    tok->string_unit_count = 0u;
    tok->string_numeric_units = NULL;
    tok->string_numeric_values = NULL;
    tok->string_element_width = 1u;
    tok->owns_string = false;
    return tok;
}

void token_free(Token* tok) {
    if (!tok) return;
    if (tok->owns_string) rcc_free((void*)tok->value.str_val);
    rcc_free(tok->string_numeric_units);
    rcc_free(tok->string_numeric_values);
    rcc_free(tok);
}

static void lex_set_string_unit_metadata(Token* token,
                                         const uint8_t* numeric_units,
                                         const uint32_t* numeric_values,
                                         size_t unit_count) {
    token->string_unit_count = unit_count;
    if (unit_count == 0u) return;
    token->string_numeric_units = rcc_alloc(unit_count);
    token->string_numeric_values =
        rcc_alloc(unit_count * sizeof(*token->string_numeric_values));
    if (numeric_units) {
        memcpy(token->string_numeric_units, numeric_units, unit_count);
    } else {
        memset(token->string_numeric_units, 0, unit_count);
    }
    if (numeric_values) {
        memcpy(token->string_numeric_values, numeric_values,
               unit_count * sizeof(*token->string_numeric_values));
    } else {
        memset(token->string_numeric_values, 0,
               unit_count * sizeof(*token->string_numeric_values));
    }
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

static int lex_identifier_ucn(Lexer* lex, char* output, size_t capacity);

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
    char spelling[RCC_MAX_IDENT];
    size_t len = 0u;
    bool over_limit = false;

    while (isalnum((unsigned char)peek(lex)) || peek(lex) == '_' ||
           (peek(lex) == '\\' &&
            (peek_next(lex) == 'u' || peek_next(lex) == 'U'))) {
        if (peek(lex) == '\\') {
            int written = lex_identifier_ucn(
                lex, spelling + (len < sizeof(spelling) ? len : sizeof(spelling)),
                len < sizeof(spelling) ? sizeof(spelling) - len : 0u);
            if (written > 0) {
                if (len + (size_t)written < sizeof(spelling)) {
                    len += (size_t)written;
                } else {
                    over_limit = true;
                }
            }
            continue;
        }
        if (len + 1u < sizeof(spelling)) {
            spelling[len++] = advance(lex);
        } else {
            over_limit = true;
            advance(lex);
        }
    }

    if (over_limit) {
        rcc_error(loc, "identifier exceeds compiler limits");
    }
    spelling[len] = '\0';

    TokenType type = keyword_lookup(spelling);
    if ((type == TOK_CHAR16_T || type == TOK_CHAR32_T) &&
        (!lexer_cxx_mode || lexer_cxx_standard < 11)) {
        type = TOK_IDENT;
    }
    if (type == TOK_CHAR8_T &&
        (!lexer_cxx_mode || lexer_cxx_standard < 20)) {
        type = TOK_IDENT;
    }
    /* C++ keywords are ordinary identifiers in C17.  Keeping this decision
     * in the lexer prevents a valid C implementation name such as
     * `protected` from being rejected before the C parser sees it. */
    if (!lexer_cxx_mode && type >= TOK_CLASS && type <= TOK_CONST_CAST) {
        type = TOK_IDENT;
    }
    if (!lexer_cxx_mode &&
        (type == TOK_AND || type == TOK_AMP_ASSIGN || type == TOK_AMP ||
         type == TOK_PIPE || type == TOK_TILDE || type == TOK_NOT ||
         type == TOK_NE || type == TOK_OR || type == TOK_PIPE_ASSIGN ||
         type == TOK_CARET || type == TOK_CARET_ASSIGN)) {
        type = TOK_IDENT;
    }
    Token* tok = token_new(type, loc);

    if (type == TOK_IDENT) {
        tok->value.str_val = rcc_intern(spelling);
    }

    return tok;
}

static bool lex_number_digit(char c, int base) {
    if (base == 16) return isxdigit((unsigned char)c) != 0;
    if (base == 10) return isdigit((unsigned char)c) != 0;
    if (base == 8) return c >= '0' && c <= '7';
    return c == '0' || c == '1';
}

static Token* lex_number(Lexer* lex, bool leading_dot) {
    SourceLoc loc = make_loc(lex);
    const char* start = lex->pos;
    bool is_float = false;
    int base = 10;
    const char* suffix_start;
    const char* user_suffix_start = NULL;
    bool saw_digit = false;
    bool saw_exponent_digit = false;
    bool saw_binary_exponent = false;

    if (leading_dot) {
        /* The caller has already established that the dot is followed by a
         * decimal digit.  C permits a leading decimal point in a floating
         * constant, so keep the dot as part of the numeric token. */
        is_float = true;
        advance(lex);
    } else if (peek(lex) == '0') {
        advance(lex);
        saw_digit = true;
        if (peek(lex) == 'x' || peek(lex) == 'X') {
            base = 16;
            saw_digit = false;
            advance(lex);
        } else if (peek(lex) == 'b' || peek(lex) == 'B') {
            base = 2;
            saw_digit = false;
            advance(lex);
        } else if (isdigit(peek(lex))) {
            base = 8;
        }
    }

    /* Read the integral part.  Hexadecimal floating constants share this
     * path with hexadecimal integers until a dot or binary exponent proves
     * that the token is floating-point. */
    while (1) {
        char c = peek(lex);
        if (lex_number_digit(c, base)) {
            advance(lex);
            saw_digit = true;
        } else if (lexer_cxx_mode && c == '\'' && saw_digit &&
                   lex_number_digit(lex->pos[1], base)) {
            advance(lex);
        } else {
            break;
        }
    }

    /* Check for a fractional part.  A decimal point does not need a digit
     * on its right (`1.` is a valid C floating constant). */
    if ((base == 10 || base == 16) && peek(lex) == '.') {
        is_float = true;
        advance(lex);
        while (lex_number_digit(peek(lex), base) ||
               (lexer_cxx_mode && peek(lex) == '\'' &&
                lex_number_digit(lex->pos[1], base))) {
            advance(lex);
            if (peek(lex) != '\'' ||
                !lex_number_digit(lex->pos[1], base)) {
                saw_digit = true;
            }
        }
    }

    /* Decimal constants use e/E; hexadecimal floating constants use the
     * mandatory binary p/P exponent. */
    if ((base == 10 && (peek(lex) == 'e' || peek(lex) == 'E')) ||
        (base == 16 && (peek(lex) == 'p' || peek(lex) == 'P'))) {
        if (base == 16) saw_binary_exponent = true;
        is_float = true;
        advance(lex);
        if (peek(lex) == '+' || peek(lex) == '-') {
            advance(lex);
        }
        while (isdigit(peek(lex))) {
            advance(lex);
            saw_exponent_digit = true;
        }
        while (lexer_cxx_mode && peek(lex) == '\'' &&
               isdigit((unsigned char)lex->pos[1]) != 0) {
            advance(lex);
            while (isdigit(peek(lex))) {
                advance(lex);
                saw_exponent_digit = true;
            }
        }
        if (!saw_exponent_digit) {
            rcc_error(loc, "floating literal exponent requires digits");
        }
    }

    if (!saw_digit) {
        rcc_error(loc, "numeric literal requires digits");
    }
    if (base == 16 && is_float && !saw_binary_exponent) {
        rcc_error(loc,
                  "hexadecimal floating literal requires a binary exponent");
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

    /* Keep a C++ user-defined numeric literal suffix on the literal token
     * instead of letting it become a separate identifier. */
    if (lexer_cxx_mode && peek(lex) == '_') {
        user_suffix_start = lex->pos;
        advance(lex);
        while (isalnum((unsigned char)peek(lex)) || peek(lex) == '_') {
            advance(lex);
        }
    }

    size_t len = lex->pos - start;
    char* str = rcc_alloc(len + 1);
    memcpy(str, start, len);
    str[len] = '\0';

    /* C++ digit separators are part of the source spelling but not of the
     * value accepted by strtod/strtoull.  Remove only separators that were
     * already validated in the numeric scanner above. */
    {
        size_t read_index = 0;
        size_t write_index = 0;
        while (read_index < len) {
            if (!lexer_cxx_mode || str[read_index] != '\'') {
                str[write_index++] = str[read_index];
            }
            ++read_index;
        }
        str[write_index] = '\0';
    }

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
        const char* builtin_suffix_end = user_suffix_start
            ? user_suffix_start : lex->pos;
        bool suffix_valid = true;
        bool saw_unsigned = false;
        unsigned long_suffix = 0u;
        uint64_t value;
        const char* integer_text = str;

        while (suffix < builtin_suffix_end) {
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
                if (suffix < builtin_suffix_end && *suffix == long_case) {
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
        /* strtoull accepts the 0x prefix for base 16, but not the C++/GNU
         * 0b prefix when an explicit base of 2 is supplied. */
        if (base == 2 && str[0] == '0' &&
            (str[1] == 'b' || str[1] == 'B')) {
            integer_text = str + 2;
        }
        value = strtoull(integer_text, NULL, base);
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

    if (user_suffix_start) {
        size_t user_suffix_length = (size_t)(lex->pos - user_suffix_start);
        char* user_suffix = rcc_alloc(user_suffix_length + 1u);
        memcpy(user_suffix, user_suffix_start, user_suffix_length);
        user_suffix[user_suffix_length] = '\0';
        tok->user_literal_suffix = rcc_intern(user_suffix);
        rcc_free(user_suffix);
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

/* Decode a universal character name occurring in an identifier.  Identifier
 * UCNs are source spelling, not byte-string data: retain them as UTF-8 in the
 * interned name while keeping the narrow string/character literal ABI
 * diagnostics in lex_escape(). */
static int lex_identifier_ucn(Lexer* lex, char* output, size_t capacity) {
    SourceLoc loc;
    char kind;
    int digits;
    uint32_t codepoint = 0u;
    int length;

    if (peek(lex) != '\\' ||
        (peek_next(lex) != 'u' && peek_next(lex) != 'U')) {
        return 0;
    }
    loc = make_loc(lex);
    advance(lex); /* backslash */
    kind = advance(lex);
    digits = kind == 'u' ? 4 : 8;
    for (int index = 0; index < digits; ++index) {
        if (!isxdigit((unsigned char)peek(lex))) {
            rcc_error(loc,
                      "universal character name requires hexadecimal digits");
            return -1;
        }
        codepoint = (codepoint << 4) |
                    (uint32_t)hex_digit(advance(lex));
    }
    /* C17/C++20 reserve the basic source character set and control range from
     * identifier UCNs.  The bounded frontend accepts the remaining Unicode
     * scalar range and stores its UTF-8 spelling in the identifier table. */
    if (codepoint < 0xa0u || codepoint > 0x10ffffu ||
        (codepoint >= 0xd800u && codepoint <= 0xdfffu)) {
        rcc_error(loc,
                  "universal character name is not valid in an identifier");
        return -1;
    }
    if (codepoint <= 0x7fu) {
        length = 1;
    } else if (codepoint <= 0x7ffu) {
        length = 2;
    } else if (codepoint <= 0xffffu) {
        length = 3;
    } else {
        length = 4;
    }
    if (!output || capacity < (size_t)length) {
        rcc_error(loc, "identifier exceeds compiler limits");
        return -1;
    }
    if (length == 1) {
        output[0] = (char)codepoint;
    } else if (length == 2) {
        output[0] = (char)(0xc0u | (codepoint >> 6));
        output[1] = (char)(0x80u | (codepoint & 0x3fu));
    } else if (length == 3) {
        output[0] = (char)(0xe0u | (codepoint >> 12));
        output[1] = (char)(0x80u | ((codepoint >> 6) & 0x3fu));
        output[2] = (char)(0x80u | (codepoint & 0x3fu));
    } else {
        output[0] = (char)(0xf0u | (codepoint >> 18));
        output[1] = (char)(0x80u | ((codepoint >> 12) & 0x3fu));
        output[2] = (char)(0x80u | ((codepoint >> 6) & 0x3fu));
        output[3] = (char)(0x80u | (codepoint & 0x3fu));
    }
    return length;
}

/* Decode the current \u/\U escape into a Unicode scalar value. */
static bool lex_read_ucn_codepoint(Lexer* lex, uint32_t* result) {
    char kind = advance(lex);
    int digits = kind == 'u' ? 4 : 8;
    uint32_t codepoint = 0u;

    for (int index = 0; index < digits; ++index) {
        if (!isxdigit((unsigned char)peek(lex))) {
            rcc_error(make_loc(lex),
                      "universal character name requires hexadecimal digits");
            return false;
        }
        codepoint = (codepoint << 4) |
                    (uint32_t)hex_digit(advance(lex));
    }
    if (codepoint > 0x10ffffu ||
        (codepoint >= 0xd800u && codepoint <= 0xdfffu)) {
        rcc_error(make_loc(lex),
                  "universal character name is not a valid Unicode code point");
        return false;
    }
    /* C17 excludes UCNs for basic source characters below U+00A0 except
     * dollar, at-sign, and grave accent. */
    if (!lexer_cxx_mode && codepoint < 0xa0u && codepoint != 0x24u &&
        codepoint != 0x40u && codepoint != 0x60u) {
        rcc_error(make_loc(lex),
                  "C17 universal character name cannot designate a basic source character");
        return false;
    }
    *result = codepoint;
    return true;
}

/* Append a Unicode scalar value in a literal's target code-unit encoding.
 * RinOS images are little-endian on both supported x86 targets. */
static bool lex_append_codepoint(Lexer* lex, char* output,
                                 size_t capacity, size_t* length,
                                 uint32_t codepoint, uint8_t width) {
    uint32_t units[2];
    size_t unit_count = 1u;
    size_t bytes_needed;
    if (width == 1u) {
        int utf8_length;
        if (codepoint <= 0x7fu) utf8_length = 1;
        else if (codepoint <= 0x7ffu) utf8_length = 2;
        else if (codepoint <= 0xffffu) utf8_length = 3;
        else utf8_length = 4;
        bytes_needed = (size_t)utf8_length;
        if (!output || *length > capacity ||
            bytes_needed > capacity - *length) {
            rcc_error(make_loc(lex), "encoded string literal exceeds capacity");
            return false;
        }
        char* bytes = output + *length;
        if (utf8_length == 1) {
            bytes[0] = (char)codepoint;
        } else if (utf8_length == 2) {
            bytes[0] = (char)(0xc0u | (codepoint >> 6));
            bytes[1] = (char)(0x80u | (codepoint & 0x3fu));
        } else if (utf8_length == 3) {
            bytes[0] = (char)(0xe0u | (codepoint >> 12));
            bytes[1] = (char)(0x80u | ((codepoint >> 6) & 0x3fu));
            bytes[2] = (char)(0x80u | (codepoint & 0x3fu));
        } else {
            bytes[0] = (char)(0xf0u | (codepoint >> 18));
            bytes[1] = (char)(0x80u | ((codepoint >> 12) & 0x3fu));
            bytes[2] = (char)(0x80u | ((codepoint >> 6) & 0x3fu));
            bytes[3] = (char)(0x80u | (codepoint & 0x3fu));
        }
        *length += bytes_needed;
        return true;
    }
    if (width == 2u) {
        if (codepoint <= 0xffffu) {
            units[0] = codepoint;
        } else {
            uint32_t supplementary = codepoint - 0x10000u;
            units[0] = 0xd800u + (supplementary >> 10);
            units[1] = 0xdc00u + (supplementary & 0x3ffu);
            unit_count = 2u;
        }
    } else if (width == 4u) {
        units[0] = codepoint;
    } else {
        rcc_fatal("invalid string literal code-unit width %u",
                  (unsigned)width);
    }
    bytes_needed = unit_count * width;
    if (!output || *length > capacity ||
        bytes_needed > capacity - *length) {
        rcc_error(make_loc(lex), "encoded string literal exceeds capacity");
        return false;
    }
    for (size_t unit = 0u; unit < unit_count; ++unit) {
        for (uint8_t byte = 0u; byte < width; ++byte) {
            output[*length + unit * width + byte] =
                (char)(units[unit] >> (byte * 8u));
        }
    }
    *length += bytes_needed;
    return true;
}

/* Read and validate one UTF-8 code point from source text. */
static bool lex_read_source_codepoint(Lexer* lex, uint32_t* result) {
    uint8_t first = (uint8_t)advance(lex);
    uint32_t codepoint;
    unsigned continuation_count;
    if (first <= 0x7fu) {
        *result = first;
        return true;
    }
    if (first >= 0xc2u && first <= 0xdfu) {
        codepoint = first & 0x1fu;
        continuation_count = 1u;
    } else if (first >= 0xe0u && first <= 0xefu) {
        codepoint = first & 0x0fu;
        continuation_count = 2u;
    } else if (first >= 0xf0u && first <= 0xf4u) {
        codepoint = first & 0x07u;
        continuation_count = 3u;
    } else {
        rcc_error(make_loc(lex), "invalid UTF-8 source character in string literal");
        return false;
    }
    for (unsigned index = 0u; index < continuation_count; ++index) {
        uint8_t next = (uint8_t)peek(lex);
        if (next < 0x80u || next > 0xbfu) {
            rcc_error(make_loc(lex), "invalid UTF-8 source character in string literal");
            return false;
        }
        advance(lex);
        codepoint = (codepoint << 6) | (uint32_t)(next & 0x3fu);
    }
    if ((continuation_count == 2u && codepoint < 0x800u) ||
        (continuation_count == 3u && codepoint < 0x10000u) ||
        codepoint > 0x10ffffu ||
        (codepoint >= 0xd800u && codepoint <= 0xdfffu)) {
        rcc_error(make_loc(lex), "invalid UTF-8 source character in string literal");
        return false;
    }
    *result = codepoint;
    return true;
}

/* Decode a UCN into the existing narrow/u8 UTF-8 representation. */
static int lex_utf8_escape(Lexer* lex, char* output, size_t capacity) {
    uint32_t codepoint = 0u;
    size_t length = 0u;
    if (!lex_read_ucn_codepoint(lex, &codepoint) ||
        !lex_append_codepoint(lex, output, capacity, &length,
                              codepoint, 1u)) {
        return 0;
    }
    return (int)length;
}

static uint32_t lex_escape_value(Lexer* lex, bool* numeric_escape) {
    uint64_t value;
    unsigned digits;
    char c = advance(lex);
    if (numeric_escape) *numeric_escape = false;
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
        case '0': case '1': case '2': case '3':
        case '4': case '5': case '6': case '7':
            if (numeric_escape) *numeric_escape = true;
            value = (uint64_t)(c - '0');
            digits = 1u;
            while (digits < 3u && peek(lex) >= '0' && peek(lex) <= '7') {
                value = value * 8u + (uint64_t)(advance(lex) - '0');
                ++digits;
            }
            return (uint32_t)value;
        case 'x':
            if (numeric_escape) *numeric_escape = true;
            if (!isxdigit((unsigned char)peek(lex))) {
                rcc_error(make_loc(lex), "hex escape requires at least one digit");
                return 0u;
            }
            value = 0u;
            while (isxdigit((unsigned char)peek(lex))) {
                unsigned digit = (unsigned)hex_digit(advance(lex));
                if (value > (UINT32_MAX - digit) / 16u) {
                    rcc_error(make_loc(lex),
                              "hex escape value exceeds 32-bit literal range");
                    value = UINT32_MAX;
                } else if (value != UINT32_MAX) {
                    value = value * 16u + digit;
                }
            }
            return (uint32_t)value;
        default:
            return (uint8_t)c;
    }
}

static char lex_escape(Lexer* lex) {
    return (char)(uint8_t)lex_escape_value(lex, NULL);
}

static Token* lex_char_encoded(Lexer* lex, SourceLoc loc,
                               uint8_t element_width, bool is_wide) {
    uint32_t encoded_value = 0u;
    unsigned code_unit_count = 0u;
    bool saw_content = false;

    advance(lex); /* skip ' */
    while (peek(lex) && peek(lex) != '\'' && peek(lex) != '\n') {
        uint32_t value = 0u;
        size_t encoded_length = 0u;
        char encoded[8];
        bool numeric_escape = false;
        saw_content = true;
        if (peek(lex) == '\\') {
            advance(lex);
            if (peek(lex) == 'u' || peek(lex) == 'U') {
                if (!lex_read_ucn_codepoint(lex, &value)) continue;
            } else {
                value = lex_escape_value(lex, &numeric_escape);
            }
        } else if (!lex_read_source_codepoint(lex, &value)) {
            continue;
        }

        if (numeric_escape) {
            uint32_t maximum = element_width == 2u ? UINT16_MAX
                                                   : UINT32_MAX;
            if (value > maximum) {
                rcc_error(loc,
                          "numeric escape value does not fit character code unit");
                value &= maximum;
            }
            for (uint8_t byte = 0u; byte < element_width; ++byte) {
                encoded[byte] = (char)(value >> (byte * 8u));
            }
            encoded_length = element_width;
        } else if (!lex_append_codepoint(
                       lex, encoded, sizeof(encoded), &encoded_length,
                       value, element_width)) {
            continue;
        }

        for (size_t offset = 0u; offset < encoded_length;
             offset += element_width) {
            uint32_t code_unit = 0u;
            for (uint8_t byte = 0u; byte < element_width; ++byte) {
                code_unit |= (uint32_t)(uint8_t)encoded[offset + byte]
                             << (byte * 8u);
            }
            if (code_unit_count == 0u || is_wide) {
                /* C permits multi-character wide constants with an
                 * implementation-defined value. RinOS chooses the last
                 * UTF-32 code unit. C++ wide character literals are scalar. */
                encoded_value = code_unit;
            }
            if (code_unit_count < UINT_MAX) ++code_unit_count;
        }
    }

    if (peek(lex) != '\'') {
        rcc_error(loc, "unterminated character literal");
    } else {
        advance(lex);
    }
    if (!saw_content) {
        rcc_error(loc, "empty character literal");
    }
    if (code_unit_count > 1u &&
        (element_width == 2u || !is_wide || lexer_cxx_mode)) {
        rcc_error(loc,
                  element_width == 2u
                      ? "UTF-16 character literal must encode exactly one code unit"
                      : is_wide
                          ? "wide character literal must encode one character in C++"
                          : "UTF-32 character literal must encode exactly one code unit");
    }

    Token* tok = token_new(TOK_CHAR_LIT, loc);
    tok->value.int_val = (int64_t)encoded_value;
    tok->char_element_width = element_width;
    tok->is_wide_literal = is_wide;
    return tok;
}

static Token* lex_char(Lexer* lex, SourceLoc loc, bool is_utf8,
                       uint8_t element_width, bool is_wide) {
    if (element_width > 1u) {
        return lex_char_encoded(lex, loc, element_width, is_wide);
    }
    advance(lex); /* skip ' */

    uint32_t encoded_value = 0u;
    unsigned code_unit_count = 0u;
    bool saw_content = false;
    while (peek(lex) && peek(lex) != '\'' && peek(lex) != '\n') {
        char encoded[4];
        int encoded_length = 1;
        saw_content = true;
        if (peek(lex) == '\\') {
            advance(lex);
            if (peek(lex) == 'u' || peek(lex) == 'U') {
                encoded_length = lex_utf8_escape(
                    lex, encoded, sizeof(encoded));
            } else {
                encoded[0] = lex_escape(lex);
            }
        } else {
            encoded[0] = advance(lex);
        }
        for (int index = 0; index < encoded_length; ++index) {
            encoded_value = (encoded_value << 8) |
                (uint32_t)(unsigned char)encoded[index];
            if (code_unit_count < 5u) ++code_unit_count;
        }
    }

    if (peek(lex) != '\'') {
        rcc_error(loc, "unterminated character literal");
    } else {
        advance(lex);
    }
    if (!saw_content) {
        rcc_error(loc, "empty character literal");
    }
    if (is_utf8 && code_unit_count > 1u) {
        rcc_error(loc,
                  "UTF-8 character literal must encode exactly one code unit");
    }

    Token* tok = token_new(TOK_CHAR_LIT, loc);
    if (!is_utf8 && code_unit_count > 1u) {
        tok->value.int_val = encoded_value <= (uint32_t)INT32_MAX
            ? (int64_t)encoded_value
            : (int64_t)encoded_value - INT64_C(0x100000000);
    } else {
        tok->value.char_val = (char)(uint8_t)encoded_value;
    }
    tok->is_utf8_literal = is_utf8;
    tok->is_multichar_literal = !is_utf8 && code_unit_count > 1u;
    tok->char_element_width = 1u;
    return tok;
}

static Token* lex_string(Lexer* lex, SourceLoc loc, bool is_utf8,
                         uint8_t element_width, bool is_wide) {
    advance(lex); /* skip " */

    char buf[RCC_MAX_STRING];
    uint8_t numeric_units[RCC_MAX_STRING];
    uint32_t numeric_values[RCC_MAX_STRING];
    size_t len = 0u;
    size_t unit_count = 0u;
    size_t capacity = sizeof(buf) - element_width;

    while (peek(lex) && peek(lex) != '"') {
        size_t previous_length = len;
        bool numeric_escape = false;
        uint32_t numeric_value = 0u;
        if (len >= capacity) {
            rcc_error(loc, "string too long");
            break;
        }
        if (peek(lex) == '\\') {
            advance(lex);
            if (peek(lex) == 'u' || peek(lex) == 'U') {
                uint32_t codepoint = 0u;
                if (lex_read_ucn_codepoint(lex, &codepoint)) {
                    (void)lex_append_codepoint(lex, buf, capacity, &len,
                                               codepoint, element_width);
                }
            } else {
                uint32_t value = lex_escape_value(lex, &numeric_escape);
                numeric_value = value;
                uint32_t maximum = element_width == 1u ? UINT8_MAX
                    : element_width == 2u ? UINT16_MAX : UINT32_MAX;
                if (element_width > 1u && value > maximum) {
                    rcc_error(loc,
                              "numeric escape value does not fit string code unit");
                    value &= maximum;
                }
                if (element_width == 1u) {
                    buf[len++] = (char)value;
                } else {
                    for (uint8_t byte = 0u; byte < element_width; ++byte) {
                        buf[len++] = (char)(value >> (byte * 8u));
                    }
                }
            }
        } else if (peek(lex) == '\n') {
            rcc_error(loc, "newline in string literal");
            break;
        } else if (element_width > 1u) {
            uint32_t codepoint = 0u;
            if (lex_read_source_codepoint(lex, &codepoint)) {
                (void)lex_append_codepoint(lex, buf, capacity, &len,
                                           codepoint, element_width);
            }
        } else {
            buf[len++] = advance(lex);
        }
        while (previous_length < len) {
            numeric_units[unit_count] = numeric_escape ? 1u : 0u;
            numeric_values[unit_count] = numeric_escape ? numeric_value : 0u;
            ++unit_count;
            previous_length += element_width;
        }
    }

    if (peek(lex) != '"') {
        rcc_error(loc, "unterminated string literal");
    }
    advance(lex);

    Token* tok = token_new(TOK_STRING_LIT, loc);
    tok->value.str_val = rcc_alloc(len + element_width);
    memcpy((void*)tok->value.str_val, buf, len);
    memset((char*)tok->value.str_val + len, 0, element_width);
    tok->string_length = len;
    tok->string_element_width = element_width;
    tok->owns_string = true;
    lex_set_string_unit_metadata(tok, numeric_units, numeric_values,
                                 unit_count);
    tok->is_utf8_literal = is_utf8;
    tok->is_wide_literal = is_wide;
    return tok;
}

/* C++11 raw strings bypass escape decoding while retaining the selected
 * literal encoding, delimiter matching, and embedded source newlines. */
static Token* lex_raw_string(Lexer* lex, SourceLoc loc, unsigned prefix_length,
                             bool is_utf8, uint8_t element_width,
                             bool is_wide) {
    char delimiter[17];
    char buf[RCC_MAX_STRING];
    size_t delimiter_length = 0;
    size_t len = 0u;

    for (unsigned index = 0; index < prefix_length; ++index) advance(lex);
    if (peek(lex) != '"') {
        rcc_error(loc, "raw string literal requires an opening quote");
        return lex_string(lex, loc, is_utf8, element_width, is_wide);
    }
    advance(lex);
    while (peek(lex) && peek(lex) != '(') {
        unsigned char c = (unsigned char)peek(lex);
        if (delimiter_length >= sizeof(delimiter) - 1u) {
            rcc_error(loc, "raw string literal delimiter exceeds 16 bytes");
            while (peek(lex) && peek(lex) != '(') advance(lex);
            break;
        }
        if (c <= 0x20u || c == ')' || c == '\\' || c == '"') {
            rcc_error(loc, "invalid raw string literal delimiter");
        }
        delimiter[delimiter_length++] = advance(lex);
    }
    if (peek(lex) != '(') {
        rcc_error(loc, "raw string literal delimiter requires an opening parenthesis");
    } else {
        advance(lex);
    }
    delimiter[delimiter_length] = '\0';
    while (peek(lex)) {
        if (peek(lex) == ')' &&
            strncmp(lex->pos + 1, delimiter, delimiter_length) == 0 &&
            lex->pos[1 + delimiter_length] == '"') {
            advance(lex);
            for (size_t index = 0; index < delimiter_length + 1u; ++index) {
                advance(lex);
            }
            {
                Token* tok = token_new(TOK_STRING_LIT, loc);
                tok->value.str_val = rcc_alloc(len + element_width);
                memcpy((void*)tok->value.str_val, buf, len);
                memset((char*)tok->value.str_val + len, 0,
                       element_width);
                tok->string_length = len;
                tok->string_element_width = element_width;
                tok->owns_string = true;
                lex_set_string_unit_metadata(
                    tok, NULL, NULL, len / element_width);
                tok->is_utf8_literal = is_utf8;
                tok->is_wide_literal = is_wide;
                return tok;
            }
        }
        if (len >= (size_t)RCC_MAX_STRING - element_width) {
            rcc_error(loc, "raw string literal is too long");
            while (peek(lex)) advance(lex);
            break;
        }
        if (element_width == 1u) {
            buf[len++] = advance(lex);
        } else {
            uint32_t codepoint = 0u;
            if (lex_read_source_codepoint(lex, &codepoint)) {
                (void)lex_append_codepoint(
                    lex, buf, sizeof(buf) - element_width, &len,
                    codepoint, element_width);
            }
        }
    }
    rcc_error(loc, "unterminated raw string literal");
    {
        Token* tok = token_new(TOK_STRING_LIT, loc);
        tok->value.str_val = rcc_alloc(len + element_width);
        memcpy((void*)tok->value.str_val, buf, len);
        memset((char*)tok->value.str_val + len, 0, element_width);
        tok->string_length = len;
        tok->string_element_width = element_width;
        tok->owns_string = true;
        lex_set_string_unit_metadata(tok, NULL, NULL, len / element_width);
        tok->is_utf8_literal = is_utf8;
        tok->is_wide_literal = is_wide;
        return tok;
    }
}

static Token* lex_token(Lexer* lex) {
    skip_whitespace(lex);

    if (peek(lex) == '\0') {
        return token_new(TOK_EOF, make_loc(lex));
    }

    SourceLoc loc = make_loc(lex);
    char c = peek(lex);

    /* Raw literal prefixes are recognized before ordinary identifier lexing. */
    if (lexer_cxx_mode) {
        unsigned prefix_length = 0;
        bool is_utf8 = false;
        uint8_t element_width = 1u;
        bool is_wide = false;
        bool cxx11_literals = lexer_cxx_standard >= 11;
        if (c == 'R' && peek_next(lex) == '"') {
            prefix_length = 1;
        } else if (cxx11_literals && c == 'u' && peek_next(lex) == '8' &&
                   lex->pos[2] == 'R' && lex->pos[3] == '"') {
            prefix_length = 3;
            is_utf8 = true;
        } else if (c == 'L' && peek_next(lex) == 'R' &&
                   lex->pos[2] == '"') {
            prefix_length = 2;
            element_width = 4u;
            is_wide = true;
        } else if (cxx11_literals && c == 'u' && peek_next(lex) == 'R' &&
                   lex->pos[2] == '"') {
            prefix_length = 2;
            element_width = 2u;
        } else if (cxx11_literals && c == 'U' && peek_next(lex) == 'R' &&
                   lex->pos[2] == '"') {
            prefix_length = 2;
            element_width = 4u;
        }
        if (prefix_length != 0) {
            return lex_raw_string(lex, loc, prefix_length, is_utf8,
                                  element_width, is_wide);
        }
    }

    /* Recognize encoded literal prefixes before ordinary identifier lexing. */
    {
        bool prefixed_literal = false;
        bool is_utf8 = false;
        bool is_wide = false;
        uint8_t element_width = 1u;
        char quote;
        unsigned prefix_length = 0u;
        if (c == 'u' && peek_next(lex) == '8' &&
            (!lexer_cxx_mode || lexer_cxx_standard >= 11) &&
            (lex->pos[2] == '"' ||
             (lexer_cxx_mode && lex->pos[2] == '\''))) {
            prefixed_literal = true;
            is_utf8 = true;
            prefix_length = 2u;
        } else if ((c == 'u' || c == 'U') &&
                   (!lexer_cxx_mode || lexer_cxx_standard >= 11) &&
                   (peek_next(lex) == '"' ||
                    (lexer_cxx_mode && peek_next(lex) == '\''))) {
            prefixed_literal = true;
            prefix_length = 1u;
            element_width = c == 'u' ? 2u : 4u;
        } else if (c == 'L' &&
                   (peek_next(lex) == '"' || peek_next(lex) == '\'')) {
            prefixed_literal = true;
            prefix_length = 1u;
            element_width = 4u;
            is_wide = true;
        }
        if (!prefixed_literal) goto no_prefixed_literal;
        for (unsigned index = 0u; index < prefix_length; ++index) {
            advance(lex);
        }
        quote = peek(lex);
        if (quote == '\'') {
            return lex_char(lex, loc, is_utf8, element_width, is_wide);
        }
        return lex_string(lex, loc, is_utf8, element_width, is_wide);
    }

no_prefixed_literal:

    /* Identifier or keyword */
    if (isalpha((unsigned char)c) || c == '_' ||
        (c == '\\' && (peek_next(lex) == 'u' || peek_next(lex) == 'U'))) {
        return lex_identifier(lex);
    }

    /* Number.  A leading dot followed by a digit starts a decimal floating
     * constant (`.5`), not the member-access operator. */
    if (isdigit(c)) {
        return lex_number(lex, false);
    }
    if (c == '.' && isdigit(peek_next(lex))) {
        return lex_number(lex, true);
    }

    /* Character literal */
    if (c == '\'') {
        return lex_char(lex, loc, false, 1u, false);
    }

    /* String literal */
    if (c == '"') {
        return lex_string(lex, loc, false, 1u, false);
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
            if (match(lex, '=')) {
                if (match(lex, '>')) return token_new(TOK_SPACESHIP, loc);
                return token_new(TOK_LE, loc);
            }
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

enum {
    LEX_STRING_ORDINARY,
    LEX_STRING_UTF8,
    LEX_STRING_UTF16,
    LEX_STRING_UTF32,
    LEX_STRING_WIDE
};

static unsigned lex_string_prefix(const Token* token) {
    if (token->is_wide_literal) return LEX_STRING_WIDE;
    if (token->is_utf8_literal) return LEX_STRING_UTF8;
    if (token->string_element_width == 2u) return LEX_STRING_UTF16;
    if (token->string_element_width == 4u) return LEX_STRING_UTF32;
    return LEX_STRING_ORDINARY;
}

static uint32_t lex_string_code_unit(const Token* token, size_t index) {
    size_t offset = index * token->string_element_width;
    uint32_t value = 0u;
    for (uint8_t byte = 0u; byte < token->string_element_width; ++byte) {
        value |= (uint32_t)(uint8_t)token->value.str_val[offset + byte]
                 << (byte * 8u);
    }
    return value;
}

static bool lex_decode_utf8_string_unit(const Token* token, size_t* index,
                                        uint32_t* codepoint) {
    const unsigned char* bytes = (const unsigned char*)token->value.str_val;
    size_t at = *index;
    unsigned char first = bytes[at];
    unsigned count;
    uint32_t value;
    if (first <= 0x7fu) {
        count = 1u;
        value = first;
    } else if (first >= 0xc2u && first <= 0xdfu) {
        count = 2u;
        value = first & 0x1fu;
    } else if (first >= 0xe0u && first <= 0xefu) {
        count = 3u;
        value = first & 0x0fu;
    } else if (first >= 0xf0u && first <= 0xf4u) {
        count = 4u;
        value = first & 0x07u;
    } else {
        rcc_error(token->loc,
                  "ordinary literal is not valid UTF-8 for prefixed concatenation");
        *codepoint = first;
        *index = at + 1u;
        return false;
    }
    if (count > token->string_unit_count - at) {
        rcc_error(token->loc,
                  "ordinary literal is not valid UTF-8 for prefixed concatenation");
        *codepoint = first;
        *index = at + 1u;
        return false;
    }
    for (unsigned offset = 1u; offset < count; ++offset) {
        unsigned char continuation = bytes[at + offset];
        if ((continuation & 0xc0u) != 0x80u ||
            (token->string_numeric_units &&
             token->string_numeric_units[at + offset] != 0u)) {
            rcc_error(token->loc,
                      "ordinary literal is not valid UTF-8 for prefixed concatenation");
            *codepoint = first;
            *index = at + 1u;
            return false;
        }
        value = (value << 6) | (continuation & 0x3fu);
    }
    if ((count == 2u && value < 0x80u) ||
        (count == 3u && value < 0x800u) ||
        (count == 4u && value < 0x10000u) || value > 0x10ffffu ||
        (value >= 0xd800u && value <= 0xdfffu)) {
        rcc_error(token->loc,
                  "ordinary literal is not valid UTF-8 for prefixed concatenation");
        *codepoint = first;
        *index = at + 1u;
        return false;
    }
    *codepoint = value;
    *index = at + count;
    return true;
}

static bool lex_decode_string_codepoint(const Token* token, unsigned prefix,
                                        size_t* index, uint32_t* codepoint,
                                        bool* is_numeric) {
    size_t at = *index;
    *is_numeric = token->string_numeric_units &&
                  token->string_numeric_units[at] != 0u;
    if (*is_numeric) {
        *codepoint = token->string_numeric_values[at];
        *index = at + 1u;
        return true;
    }
    if (prefix == LEX_STRING_ORDINARY || prefix == LEX_STRING_UTF8) {
        return lex_decode_utf8_string_unit(token, index, codepoint);
    }
    if (prefix == LEX_STRING_UTF16) {
        uint32_t first = lex_string_code_unit(token, at);
        if (first >= 0xd800u && first <= 0xdbffu &&
            at + 1u < token->string_unit_count &&
            (!token->string_numeric_units ||
             token->string_numeric_units[at + 1u] == 0u)) {
            uint32_t second = lex_string_code_unit(token, at + 1u);
            if (second >= 0xdc00u && second <= 0xdfffu) {
                *codepoint = 0x10000u + ((first - 0xd800u) << 10) +
                             (second - 0xdc00u);
                *index = at + 2u;
                return true;
            }
        }
        if (first >= 0xd800u && first <= 0xdfffu) {
            rcc_error(token->loc,
                      "UTF-16 literal contains an unpaired surrogate during concatenation");
            *codepoint = first;
            *index = at + 1u;
            return false;
        }
        *codepoint = first;
        *index = at + 1u;
        return true;
    }
    *codepoint = lex_string_code_unit(token, at);
    *index = at + 1u;
    if (*codepoint > 0x10ffffu ||
        (*codepoint >= 0xd800u && *codepoint <= 0xdfffu)) {
        rcc_error(token->loc,
                  "Unicode literal contains a non-scalar value during concatenation");
        return false;
    }
    return true;
}

static bool lex_append_concat_value(Lexer* lex, const Token* source,
                                    unsigned target_prefix,
                                    char* output, size_t capacity,
                                    size_t* length, uint32_t value,
                                    bool is_numeric) {
    uint8_t width = target_prefix == LEX_STRING_UTF16 ? 2u
        : target_prefix == LEX_STRING_UTF32 ||
          target_prefix == LEX_STRING_WIDE ? 4u : 1u;
    if (is_numeric) {
        uint32_t maximum = width == 1u ? UINT8_MAX
            : width == 2u ? UINT16_MAX : UINT32_MAX;
        if (value > maximum &&
            target_prefix != LEX_STRING_WIDE &&
            target_prefix != LEX_STRING_ORDINARY) {
            rcc_error(source->loc,
                      "numeric escape value does not fit concatenated string code unit");
            value &= maximum;
        }
        if (*length > capacity || width > capacity - *length) {
            rcc_error(source->loc,
                      "concatenated string literal exceeds capacity");
            return false;
        }
        for (uint8_t byte = 0u; byte < width; ++byte) {
            output[*length + byte] = (char)(value >> (byte * 8u));
        }
        *length += width;
        return true;
    }
    if (!lex_append_codepoint(lex, output, capacity, length, value, width)) {
        return false;
    }
    return true;
}

static void lex_append_string_token(Lexer* lex, const Token* source,
                                    unsigned target_prefix,
                                    char* output, size_t capacity,
                                    size_t* length) {
    unsigned source_prefix = lex_string_prefix(source);
    if (source_prefix == target_prefix) {
        if (source->string_length > capacity - *length) {
            rcc_error(source->loc,
                      "concatenated string literal exceeds capacity");
            return;
        }
        memcpy(output + *length, source->value.str_val,
               source->string_length);
        *length += source->string_length;
        return;
    }
    {
        size_t index = 0u;
        while (index < source->string_unit_count) {
            uint32_t codepoint = 0u;
            bool is_numeric = false;
            size_t previous = index;
            bool decoded = lex_decode_string_codepoint(
                source, source_prefix, &index, &codepoint, &is_numeric);
            if (!decoded && index == previous) ++index;
            (void)lex_append_concat_value(
                lex, source, target_prefix, output, capacity, length,
                codepoint, is_numeric);
        }
    }
}

static void lex_normalize_adjacent_strings(TokenList* list, Lexer* lex) {
    Token** link = &list->head;
    while (*link) {
        Token* first = *link;
        Token* after;
        Token* item;
        size_t token_count = 0u;
        size_t input_bytes = 0u;
        unsigned target_prefix = LEX_STRING_ORDINARY;
        bool have_prefixed_token = false;
        bool mixed_nonordinary_prefixes = false;
        if (first->type != TOK_STRING_LIT) {
            link = &first->next;
            continue;
        }
        after = first;
        while (after && after->type == TOK_STRING_LIT) {
            unsigned prefix = lex_string_prefix(after);
            ++token_count;
            input_bytes += after->string_length;
            if (prefix != LEX_STRING_ORDINARY) {
                if (!have_prefixed_token) {
                    target_prefix = prefix;
                    have_prefixed_token = true;
                } else if (prefix != target_prefix) {
                    mixed_nonordinary_prefixes = true;
                }
            }
            after = after->next;
        }
        if (token_count < 2u) {
            link = &first->next;
            continue;
        }
        if (mixed_nonordinary_prefixes && lexer_cxx_mode &&
            lexer_cxx_standard >= 20) {
            rcc_error(first->loc,
                      "adjacent string literals have different encoding prefixes");
        }
        {
            uint8_t target_width = target_prefix == LEX_STRING_UTF16 ? 2u
                : target_prefix == LEX_STRING_UTF32 ||
                  target_prefix == LEX_STRING_WIDE ? 4u : 1u;
            size_t capacity = input_bytes <=
                    (SIZE_MAX - target_width - 1u) / 4u
                ? input_bytes * 4u + target_width + 1u
                : 0u;
            size_t output_length = 0u;
            char* joined;
            if (capacity == 0u) {
                rcc_error(first->loc,
                          "adjacent string literal sequence is too large");
                capacity = target_width + 1u;
            }
            joined = rcc_alloc(capacity);
            for (item = first; item != after; item = item->next) {
                lex_append_string_token(lex, item, target_prefix, joined,
                                        capacity - target_width,
                                        &output_length);
            }
            memset(joined + output_length, 0, target_width);
            if (first->owns_string) {
                rcc_free((void*)first->value.str_val);
            }
            rcc_free(first->string_numeric_units);
            rcc_free(first->string_numeric_values);
            first->value.str_val = joined;
            first->string_length = output_length;
            first->string_element_width = target_width;
            first->string_unit_count = 0u;
            first->string_numeric_units = NULL;
            first->string_numeric_values = NULL;
            first->owns_string = true;
            first->is_utf8_literal = target_prefix == LEX_STRING_UTF8;
            first->is_wide_literal = target_prefix == LEX_STRING_WIDE;
        }
        item = first->next;
        while (item != after) {
            Token* next = item->next;
            token_free(item);
            item = next;
            --list->count;
        }
        first->next = after;
        link = &first->next;
    }
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
        tokenlist_append(list, tok);
        if (tok->type == TOK_EOF) break;
    }

    lex_normalize_adjacent_strings(list, &lex);

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
