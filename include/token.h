/*
 * RCC - RinOS C Compiler
 * Token definitions
 */

#ifndef TOKEN_H
#define TOKEN_H

#include "rcc.h"

/* Token types */
typedef enum {
    /* End of file */
    TOK_EOF = 0,

    /* Literals */
    TOK_IDENT,          /* identifier */
    TOK_INT_LIT,        /* integer literal */
    TOK_FLOAT_LIT,      /* floating point literal */
    TOK_CHAR_LIT,       /* character literal */
    TOK_STRING_LIT,     /* string literal */

    /* Keywords */
    TOK_AUTO,
    TOK_BREAK,
    TOK_CASE,
    TOK_CHAR,
    TOK_CONST,
    TOK_CONTINUE,
    TOK_DEFAULT,
    TOK_DO,
    TOK_DOUBLE,
    TOK_ELSE,
    TOK_ENUM,
    TOK_EXTERN,
    TOK_FLOAT,
    TOK_FOR,
    TOK_GOTO,
    TOK_IF,
    TOK_INLINE,
    TOK_INT,
    TOK_LONG,
    TOK_REGISTER,
    TOK_RESTRICT,
    TOK_RETURN,
    TOK_SHORT,
    TOK_SIGNED,
    TOK_SIZEOF,
    TOK_STATIC,
    TOK_STRUCT,
    TOK_SWITCH,
    TOK_TYPEDEF,
    TOK_UNION,
    TOK_UNSIGNED,
    TOK_VOID,
    TOK_VOLATILE,
    TOK_WHILE,
    TOK__BOOL,
    TOK__ALIGNOF,
    TOK__ALIGNAS,
    TOK_STATIC_ASSERT,
    TOK_GENERIC,
    TOK_THREAD_LOCAL,

    /* GNU Extensions */
    TOK_ASM,            /* asm, __asm, __asm__ */
    TOK___VOLATILE__,   /* __volatile__, volatile (in asm context) */
    TOK___INLINE__,     /* __inline, __inline__ */
    TOK___ATTRIBUTE__,  /* __attribute__ */
    TOK___EXTENSION__,  /* __extension__ */
    TOK___BUILTIN_VA_LIST, /* __builtin_va_list */

    /* C++ Keywords */
    TOK_CLASS,          /* class */
    TOK_NAMESPACE,      /* namespace */
    TOK_TEMPLATE,       /* template */
    TOK_TYPENAME,       /* typename */
    TOK_PUBLIC,         /* public */
    TOK_PRIVATE,        /* private */
    TOK_PROTECTED,      /* protected */
    TOK_VIRTUAL,        /* virtual */
    TOK_OVERRIDE,       /* override */
    TOK_FINAL,          /* final */
    TOK_NEW,            /* new */
    TOK_DELETE,         /* delete */
    TOK_THIS,           /* this */
    TOK_NULLPTR,        /* nullptr */
    TOK_TRUE,           /* true */
    TOK_FALSE,          /* false */
    TOK_BOOL,           /* bool */
    TOK_THROW,          /* throw */
    TOK_TRY,            /* try */
    TOK_CATCH,          /* catch */
    TOK_USING,          /* using */
    TOK_OPERATOR,       /* operator */
    TOK_FRIEND,         /* friend */
    TOK_EXPLICIT,       /* explicit */
    TOK_MUTABLE,        /* mutable */
    TOK_CONSTEXPR,      /* constexpr */
    TOK_NOEXCEPT,       /* noexcept */
    TOK_STATIC_CAST,    /* static_cast */
    TOK_DYNAMIC_CAST,   /* dynamic_cast */
    TOK_REINTERPRET_CAST, /* reinterpret_cast */
    TOK_CONST_CAST,     /* const_cast */

    /* Operators and punctuation */
    TOK_LPAREN,         /* ( */
    TOK_RPAREN,         /* ) */
    TOK_LBRACKET,       /* [ */
    TOK_RBRACKET,       /* ] */
    TOK_LBRACE,         /* { */
    TOK_RBRACE,         /* } */
    TOK_DOT,            /* . */
    TOK_ARROW,          /* -> */
    TOK_COMMA,          /* , */
    TOK_COLON,          /* : */
    TOK_SEMICOLON,      /* ; */
    TOK_QUESTION,       /* ? */
    TOK_ELLIPSIS,       /* ... */

    /* Arithmetic operators */
    TOK_PLUS,           /* + */
    TOK_MINUS,          /* - */
    TOK_STAR,           /* * */
    TOK_SLASH,          /* / */
    TOK_PERCENT,        /* % */
    TOK_INC,            /* ++ */
    TOK_DEC,            /* -- */

    /* Bitwise operators */
    TOK_AMP,            /* & */
    TOK_PIPE,           /* | */
    TOK_CARET,          /* ^ */
    TOK_TILDE,          /* ~ */
    TOK_LSHIFT,         /* << */
    TOK_RSHIFT,         /* >> */

    /* Logical operators */
    TOK_AND,            /* && */
    TOK_OR,             /* || */
    TOK_NOT,            /* ! */

    /* Comparison operators */
    TOK_EQ,             /* == */
    TOK_NE,             /* != */
    TOK_LT,             /* < */
    TOK_GT,             /* > */
    TOK_LE,             /* <= */
    TOK_GE,             /* >= */

    /* Assignment operators */
    TOK_ASSIGN,         /* = */
    TOK_PLUS_ASSIGN,    /* += */
    TOK_MINUS_ASSIGN,   /* -= */
    TOK_STAR_ASSIGN,    /* *= */
    TOK_SLASH_ASSIGN,   /* /= */
    TOK_PERCENT_ASSIGN, /* %= */
    TOK_AMP_ASSIGN,     /* &= */
    TOK_PIPE_ASSIGN,    /* |= */
    TOK_CARET_ASSIGN,   /* ^= */
    TOK_LSHIFT_ASSIGN,  /* <<= */
    TOK_RSHIFT_ASSIGN,  /* >>= */

    /* Preprocessor (handled specially) */
    TOK_HASH,           /* # */
    TOK_HASHHASH,       /* ## */

    /* C++ specific operators */
    TOK_SCOPE,          /* :: */
    TOK_DOT_STAR,       /* .* */
    TOK_ARROW_STAR,     /* ->* */

    TOK_COUNT
} TokenType;

/* Token value union */
typedef union {
    int64_t int_val;
    double float_val;
    char char_val;
    const char* str_val;
} TokenValue;

/* Token structure */
typedef struct Token {
    TokenType type;
    TokenValue value;
    SourceLoc loc;
    struct Token* next;
} Token;

/* Token list */
typedef struct TokenList {
    Token* head;
    Token* tail;
    int count;
} TokenList;

/* Token functions */
Token* token_new(TokenType type, SourceLoc loc);
void token_free(Token* tok);
TokenList* tokenlist_new(void);
void tokenlist_free(TokenList* list);
void tokenlist_append(TokenList* list, Token* tok);

/* Token type to string */
const char* token_type_str(TokenType type);

/* Keyword lookup */
TokenType keyword_lookup(const char* str);

#endif /* TOKEN_H */
