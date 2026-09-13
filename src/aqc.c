// SPDX-License-Identifier: Apache-2.0
#include "../include/aqc.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define AQC_MAX_SYMBOLS 384u
#define AQC_MAX_NESTING 32u
#define AQC_MAX_EXPRESSION_DEPTH 64u

typedef enum TokenKind {
    TOKEN_EOF = 0,
    TOKEN_IDENTIFIER,
    TOKEN_INTEGER,
    TOKEN_FLOAT,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_SEMICOLON,
    TOKEN_COMMA,
    TOKEN_AT,
    TOKEN_ASSIGN,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_BANG,
    TOKEN_TILDE,
    TOKEN_AMP,
    TOKEN_PIPE,
    TOKEN_CARET,
    TOKEN_EQUAL_EQUAL,
    TOKEN_BANG_EQUAL,
    TOKEN_LESS,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,
    TOKEN_SHIFT_LEFT,
    TOKEN_SHIFT_RIGHT,
    TOKEN_AND_AND,
    TOKEN_OR_OR,
    TOKEN_INVALID
} TokenKind;

typedef struct Token {
    TokenKind kind;
    const char* start;
    size_t length;
    uint32_t line;
    uint32_t column;
    uint32_t integer;
    float floating;
} Token;

typedef enum AqcType {
    AQC_TYPE_INVALID = 0,
    AQC_TYPE_I32,
    AQC_TYPE_F32,
    AQC_TYPE_BOOL
} AqcType;

typedef enum SymbolKind {
    SYMBOL_INPUT = 1,
    SYMBOL_OUTPUT,
    SYMBOL_RESOURCE,
    SYMBOL_LOCAL
} SymbolKind;

typedef struct Symbol {
    char name[AQC_MAX_IDENTIFIER + 1u];
    SymbolKind kind;
    AqcType type;
    uint16_t reg;
    uint16_t resource_kind;
    uint32_t index;
} Symbol;

typedef struct Value {
    AqcType type;
    uint16_t reg;
} Value;

typedef struct Compiler {
    const char* source;
    size_t source_size;
    size_t offset;
    uint32_t line;
    uint32_t column;
    Token token;
    uint8_t* output;
    size_t output_capacity;
    uint32_t instruction_count;
    uint32_t next_register;
    uint32_t stage;
    uint32_t workgroup_x;
    uint32_t workgroup_y;
    uint32_t workgroup_z;
    uint32_t input_count;
    uint32_t output_count;
    uint32_t resource_count;
    uint64_t input_mask;
    uint64_t output_mask;
    uint64_t resource_mask;
    uint64_t resource_used_mask;
    uint64_t output_assigned_mask;
    Symbol symbols[AQC_MAX_SYMBOLS];
    uint32_t symbol_count;
    uint32_t block_depth;
    uint32_t expression_depth;
    int saw_return;
    int failed;
    int failure_code;
    AqcDiagnostic* diagnostic;
} Compiler;

static int aqc_fail_at(Compiler* compiler, int code, uint32_t line,
                       uint32_t column, const char* format, ...) {
    va_list arguments;
    if (!compiler->failed) {
        compiler->failed = 1;
        compiler->failure_code = code;
        if (compiler->diagnostic) {
            compiler->diagnostic->line = line;
            compiler->diagnostic->column = column;
            va_start(arguments, format);
            (void)vsnprintf(compiler->diagnostic->message,
                            sizeof(compiler->diagnostic->message), format,
                            arguments);
            va_end(arguments);
        }
    }
    return 0;
}

static int aqc_fail(Compiler* compiler, int code, const char* format, ...) {
    va_list arguments;
    if (!compiler->failed) {
        compiler->failed = 1;
        compiler->failure_code = code;
        if (compiler->diagnostic) {
            compiler->diagnostic->line = compiler->token.line;
            compiler->diagnostic->column = compiler->token.column;
            va_start(arguments, format);
            (void)vsnprintf(compiler->diagnostic->message,
                            sizeof(compiler->diagnostic->message), format,
                            arguments);
            va_end(arguments);
        }
    }
    return 0;
}

static int ascii_alpha(char value) {
    return (value >= 'a' && value <= 'z') ||
           (value >= 'A' && value <= 'Z') || value == '_';
}

static int ascii_digit(char value) {
    return value >= '0' && value <= '9';
}

static int ascii_hex(char value, uint32_t* digit) {
    if (value >= '0' && value <= '9') {
        *digit = (uint32_t)(value - '0');
        return 1;
    }
    if (value >= 'a' && value <= 'f') {
        *digit = (uint32_t)(value - 'a') + 10u;
        return 1;
    }
    if (value >= 'A' && value <= 'F') {
        *digit = (uint32_t)(value - 'A') + 10u;
        return 1;
    }
    return 0;
}

static char compiler_peek(const Compiler* compiler, size_t ahead) {
    size_t position = compiler->offset + ahead;
    return position < compiler->source_size ? compiler->source[position] : '\0';
}

static char compiler_take(Compiler* compiler) {
    char value = compiler_peek(compiler, 0u);
    if (value == '\0') return value;
    compiler->offset++;
    if (value == '\n') {
        compiler->line++;
        compiler->column = 1u;
    } else {
        compiler->column++;
    }
    return value;
}

static int skip_trivia(Compiler* compiler) {
    for (;;) {
        char value = compiler_peek(compiler, 0u);
        if (value == ' ' || value == '\t' || value == '\r' || value == '\n') {
            (void)compiler_take(compiler);
            continue;
        }
        if (value == '/' && compiler_peek(compiler, 1u) == '/') {
            while (compiler_peek(compiler, 0u) != '\0' &&
                   compiler_peek(compiler, 0u) != '\n') {
                (void)compiler_take(compiler);
            }
            continue;
        }
        if (value == '/' && compiler_peek(compiler, 1u) == '*') {
            uint32_t line = compiler->line;
            uint32_t column = compiler->column;
            (void)compiler_take(compiler);
            (void)compiler_take(compiler);
            while (compiler_peek(compiler, 0u) != '\0' &&
                   !(compiler_peek(compiler, 0u) == '*' &&
                     compiler_peek(compiler, 1u) == '/')) {
                (void)compiler_take(compiler);
            }
            if (compiler_peek(compiler, 0u) == '\0') {
                return aqc_fail_at(compiler, AQC_ERROR_LEXICAL, line, column,
                                   "unterminated block comment");
            }
            (void)compiler_take(compiler);
            (void)compiler_take(compiler);
            continue;
        }
        return 1;
    }
}

static int scan_number(Compiler* compiler, Token* token) {
    size_t start = compiler->offset;
    uint32_t line = compiler->line;
    uint32_t column = compiler->column;
    int is_float = 0;
    uint64_t integer = 0u;

    if (compiler_peek(compiler, 0u) == '0' &&
        (compiler_peek(compiler, 1u) == 'x' ||
         compiler_peek(compiler, 1u) == 'X')) {
        uint32_t digits = 0u;
        (void)compiler_take(compiler);
        (void)compiler_take(compiler);
        for (;;) {
            uint32_t digit;
            if (!ascii_hex(compiler_peek(compiler, 0u), &digit)) break;
            if (integer > (UINT32_MAX - digit) / 16u) {
                return aqc_fail_at(compiler, AQC_ERROR_LEXICAL, line, column,
                                   "integer literal exceeds 32 bits");
            }
            integer = integer * 16u + digit;
            digits++;
            (void)compiler_take(compiler);
        }
        if (digits == 0u || ascii_alpha(compiler_peek(compiler, 0u))) {
            return aqc_fail_at(compiler, AQC_ERROR_LEXICAL, line, column,
                               "malformed hexadecimal integer literal");
        }
        token->kind = TOKEN_INTEGER;
        token->integer = (uint32_t)integer;
    } else {
        double value = 0.0;
        double fraction_scale = 0.1;
        int exponent = 0;
        int exponent_negative = 0;
        uint32_t exponent_digits = 0u;
        while (ascii_digit(compiler_peek(compiler, 0u))) {
            uint32_t digit = (uint32_t)(compiler_take(compiler) - '0');
            if (!is_float) {
                if (integer > (UINT32_MAX - digit) / 10u) {
                    return aqc_fail_at(compiler, AQC_ERROR_LEXICAL, line,
                                       column,
                                       "integer literal exceeds 32 bits");
                }
                integer = integer * 10u + digit;
            }
            value = value * 10.0 + (double)digit;
        }
        if (compiler_peek(compiler, 0u) == '.') {
            is_float = 1;
            (void)compiler_take(compiler);
            while (ascii_digit(compiler_peek(compiler, 0u))) {
                value += (double)(compiler_take(compiler) - '0') *
                         fraction_scale;
                fraction_scale *= 0.1;
            }
        }
        if (compiler_peek(compiler, 0u) == 'e' ||
            compiler_peek(compiler, 0u) == 'E') {
            is_float = 1;
            (void)compiler_take(compiler);
            if (compiler_peek(compiler, 0u) == '+' ||
                compiler_peek(compiler, 0u) == '-') {
                exponent_negative = compiler_take(compiler) == '-';
            }
            while (ascii_digit(compiler_peek(compiler, 0u))) {
                if (exponent > 400) {
                    return aqc_fail_at(compiler, AQC_ERROR_LEXICAL, line,
                                       column, "floating literal exponent is too large");
                }
                exponent = exponent * 10 +
                           (int)(compiler_take(compiler) - '0');
                exponent_digits++;
            }
            if (exponent_digits == 0u) {
                return aqc_fail_at(compiler, AQC_ERROR_LEXICAL, line, column,
                                   "malformed floating literal exponent");
            }
            while (exponent-- > 0) {
                value = exponent_negative ? value / 10.0 : value * 10.0;
            }
        }
        if (ascii_alpha(compiler_peek(compiler, 0u))) {
            return aqc_fail_at(compiler, AQC_ERROR_LEXICAL, line, column,
                               "invalid suffix on numeric literal");
        }
        if (is_float) {
            float converted = (float)value;
            if (converted != converted || converted > 3.402823466e+38F) {
                return aqc_fail_at(compiler, AQC_ERROR_LEXICAL, line, column,
                                   "floating literal is not finite Float32");
            }
            token->kind = TOKEN_FLOAT;
            token->floating = converted;
        } else {
            token->kind = TOKEN_INTEGER;
            token->integer = (uint32_t)integer;
        }
    }
    token->start = compiler->source + start;
    token->length = compiler->offset - start;
    token->line = line;
    token->column = column;
    return 1;
}

static int compiler_next(Compiler* compiler) {
    Token token;
    char value;
    if (compiler->failed || !skip_trivia(compiler)) return 0;
    memset(&token, 0, sizeof(token));
    token.start = compiler->source + compiler->offset;
    token.line = compiler->line;
    token.column = compiler->column;
    value = compiler_peek(compiler, 0u);
    if (value == '\0') {
        if (compiler->offset != compiler->source_size) {
            return aqc_fail_at(compiler, AQC_ERROR_LEXICAL, token.line,
                               token.column, "embedded NUL byte in source");
        }
        token.kind = TOKEN_EOF;
    } else if (ascii_alpha(value)) {
        (void)compiler_take(compiler);
        while (ascii_alpha(compiler_peek(compiler, 0u)) ||
               ascii_digit(compiler_peek(compiler, 0u))) {
            (void)compiler_take(compiler);
        }
        token.kind = TOKEN_IDENTIFIER;
        token.length = compiler->offset - (size_t)(token.start - compiler->source);
        if (token.length > AQC_MAX_IDENTIFIER) {
            return aqc_fail_at(compiler, AQC_ERROR_LEXICAL, token.line,
                               token.column, "identifier exceeds %u bytes",
                               AQC_MAX_IDENTIFIER);
        }
    } else if (ascii_digit(value)) {
        if (!scan_number(compiler, &token)) return 0;
    } else {
        (void)compiler_take(compiler);
        token.length = 1u;
        switch (value) {
            case '{': token.kind = TOKEN_LBRACE; break;
            case '}': token.kind = TOKEN_RBRACE; break;
            case '(': token.kind = TOKEN_LPAREN; break;
            case ')': token.kind = TOKEN_RPAREN; break;
            case ';': token.kind = TOKEN_SEMICOLON; break;
            case ',': token.kind = TOKEN_COMMA; break;
            case '@': token.kind = TOKEN_AT; break;
            case '+': token.kind = TOKEN_PLUS; break;
            case '-': token.kind = TOKEN_MINUS; break;
            case '*': token.kind = TOKEN_STAR; break;
            case '/': token.kind = TOKEN_SLASH; break;
            case '%': token.kind = TOKEN_PERCENT; break;
            case '~': token.kind = TOKEN_TILDE; break;
            case '=':
                if (compiler_peek(compiler, 0u) == '=') {
                    (void)compiler_take(compiler);
                    token.kind = TOKEN_EQUAL_EQUAL;
                    token.length = 2u;
                } else token.kind = TOKEN_ASSIGN;
                break;
            case '!':
                if (compiler_peek(compiler, 0u) == '=') {
                    (void)compiler_take(compiler);
                    token.kind = TOKEN_BANG_EQUAL;
                    token.length = 2u;
                } else token.kind = TOKEN_BANG;
                break;
            case '<':
                if (compiler_peek(compiler, 0u) == '=') {
                    (void)compiler_take(compiler);
                    token.kind = TOKEN_LESS_EQUAL;
                    token.length = 2u;
                } else if (compiler_peek(compiler, 0u) == '<') {
                    (void)compiler_take(compiler);
                    token.kind = TOKEN_SHIFT_LEFT;
                    token.length = 2u;
                } else token.kind = TOKEN_LESS;
                break;
            case '>':
                if (compiler_peek(compiler, 0u) == '=') {
                    (void)compiler_take(compiler);
                    token.kind = TOKEN_GREATER_EQUAL;
                    token.length = 2u;
                } else if (compiler_peek(compiler, 0u) == '>') {
                    (void)compiler_take(compiler);
                    token.kind = TOKEN_SHIFT_RIGHT;
                    token.length = 2u;
                } else token.kind = TOKEN_GREATER;
                break;
            case '&':
                if (compiler_peek(compiler, 0u) == '&') {
                    (void)compiler_take(compiler);
                    token.kind = TOKEN_AND_AND;
                    token.length = 2u;
                } else token.kind = TOKEN_AMP;
                break;
            case '|':
                if (compiler_peek(compiler, 0u) == '|') {
                    (void)compiler_take(compiler);
                    token.kind = TOKEN_OR_OR;
                    token.length = 2u;
                } else token.kind = TOKEN_PIPE;
                break;
            case '^': token.kind = TOKEN_CARET; break;
            default:
                if ((unsigned char)value >= 0x80u) {
                    return aqc_fail_at(compiler, AQC_ERROR_LEXICAL,
                                       token.line, token.column,
                                       "non-ASCII byte outside a comment");
                }
                return aqc_fail_at(compiler, AQC_ERROR_LEXICAL, token.line,
                                   token.column, "unexpected character '%c'",
                                   value);
        }
    }
    compiler->token = token;
    return 1;
}

static int token_is(const Token* token, const char* text) {
    size_t length = strlen(text);
    return token->kind == TOKEN_IDENTIFIER && token->length == length &&
           memcmp(token->start, text, length) == 0;
}

static int accept_kind(Compiler* compiler, TokenKind kind) {
    if (compiler->token.kind != kind) return 0;
    return compiler_next(compiler);
}

static int expect_kind(Compiler* compiler, TokenKind kind,
                       const char* description) {
    if (compiler->token.kind != kind) {
        return aqc_fail(compiler, AQC_ERROR_SYNTAX, "expected %s", description);
    }
    return compiler_next(compiler);
}

static int expect_word(Compiler* compiler, const char* word) {
    if (!token_is(&compiler->token, word)) {
        return aqc_fail(compiler, AQC_ERROR_SYNTAX, "expected '%s'", word);
    }
    return compiler_next(compiler);
}

static int copy_identifier(Compiler* compiler, char* destination) {
    if (compiler->token.kind != TOKEN_IDENTIFIER) {
        return aqc_fail(compiler, AQC_ERROR_SYNTAX, "expected identifier");
    }
    memcpy(destination, compiler->token.start, compiler->token.length);
    destination[compiler->token.length] = '\0';
    return compiler_next(compiler);
}

static Symbol* find_symbol(Compiler* compiler, const char* name) {
    for (uint32_t index = compiler->symbol_count; index > 0u; index--) {
        if (strcmp(compiler->symbols[index - 1u].name, name) == 0) {
            return &compiler->symbols[index - 1u];
        }
    }
    return NULL;
}

static int add_symbol(Compiler* compiler, const char* name, SymbolKind kind,
                      AqcType type, uint32_t index, uint16_t resource_kind,
                      uint16_t reg) {
    Symbol* symbol;
    if (find_symbol(compiler, name)) {
        return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                        "duplicate symbol '%s'", name);
    }
    if (compiler->symbol_count >= AQC_MAX_SYMBOLS) {
        return aqc_fail(compiler, AQC_ERROR_LIMIT,
                        "too many declarations");
    }
    symbol = &compiler->symbols[compiler->symbol_count++];
    memset(symbol, 0, sizeof(*symbol));
    (void)snprintf(symbol->name, sizeof(symbol->name), "%s", name);
    symbol->kind = kind;
    symbol->type = type;
    symbol->index = index;
    symbol->resource_kind = resource_kind;
    symbol->reg = reg;
    return 1;
}

static uint16_t allocate_register(Compiler* compiler) {
    if (compiler->next_register >= RIN_SHADER_MAX_REGISTERS) {
        (void)aqc_fail(compiler, AQC_ERROR_LIMIT,
                       "shader needs more than %u registers",
                       RIN_SHADER_MAX_REGISTERS);
        return RIN_SHADER_UNUSED;
    }
    return (uint16_t)compiler->next_register++;
}

static uint32_t emit_instruction(Compiler* compiler, uint16_t opcode,
                                 uint16_t destination, uint16_t source0,
                                 uint16_t source1, uint16_t resource,
                                 uint32_t immediate) {
    RinShaderInstructionV1 instruction;
    size_t offset;
    uint32_t index = compiler->instruction_count;
    if (compiler->failed) return UINT32_MAX;
    if (index >= RIN_SHADER_MAX_INSTRUCTIONS) {
        (void)aqc_fail(compiler, AQC_ERROR_LIMIT,
                       "shader needs more than %u instructions",
                       RIN_SHADER_MAX_INSTRUCTIONS);
        return UINT32_MAX;
    }
    offset = sizeof(RinShaderHeaderV1) +
             (size_t)index * sizeof(RinShaderInstructionV1);
    if (offset > compiler->output_capacity ||
        compiler->output_capacity - offset < sizeof(instruction)) {
        (void)aqc_fail(compiler, AQC_ERROR_OUTPUT_TOO_SMALL,
                       "output buffer is too small");
        return UINT32_MAX;
    }
    memset(&instruction, 0, sizeof(instruction));
    instruction.opcode = opcode;
    instruction.destination = destination;
    instruction.source0 = source0;
    instruction.source1 = source1;
    instruction.resource = resource;
    instruction.immediate = immediate;
    memcpy(compiler->output + offset, &instruction, sizeof(instruction));
    compiler->instruction_count++;
    return index;
}

static int patch_target(Compiler* compiler, uint32_t instruction_index,
                        uint32_t target) {
    RinShaderInstructionV1 instruction;
    size_t offset;
    if (instruction_index >= compiler->instruction_count ||
        target > RIN_SHADER_MAX_INSTRUCTIONS) {
        return aqc_fail(compiler, AQC_ERROR_INTERNAL,
                        "invalid control-flow patch");
    }
    offset = sizeof(RinShaderHeaderV1) +
             (size_t)instruction_index * sizeof(instruction);
    memcpy(&instruction, compiler->output + offset, sizeof(instruction));
    instruction.immediate = target;
    memcpy(compiler->output + offset, &instruction, sizeof(instruction));
    return 1;
}

static int emit_constant_i32(Compiler* compiler, uint32_t bits, Value* value) {
    uint16_t reg = allocate_register(compiler);
    if (reg == RIN_SHADER_UNUSED) return 0;
    if (emit_instruction(compiler, RIN_SHADER_OP_CONST_I32, reg,
                         RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                         RIN_SHADER_UNUSED, bits) == UINT32_MAX) return 0;
    value->type = AQC_TYPE_I32;
    value->reg = reg;
    return 1;
}

static int emit_constant_f32(Compiler* compiler, float number, Value* value) {
    uint32_t bits;
    uint16_t reg = allocate_register(compiler);
    if (reg == RIN_SHADER_UNUSED) return 0;
    memcpy(&bits, &number, sizeof(bits));
    if (emit_instruction(compiler, RIN_SHADER_OP_CONST_F32, reg,
                         RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                         RIN_SHADER_UNUSED, bits) == UINT32_MAX) return 0;
    value->type = AQC_TYPE_F32;
    value->reg = reg;
    return 1;
}

static int emit_unary(Compiler* compiler, uint16_t opcode, AqcType type,
                      Value operand, Value* value) {
    uint16_t reg = allocate_register(compiler);
    if (reg == RIN_SHADER_UNUSED) return 0;
    if (emit_instruction(compiler, opcode, reg, operand.reg,
                         RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, 0u) ==
        UINT32_MAX) return 0;
    value->type = type;
    value->reg = reg;
    return 1;
}

static int emit_binary(Compiler* compiler, uint16_t opcode, AqcType type,
                       Value left, Value right, Value* value) {
    uint16_t reg = allocate_register(compiler);
    if (reg == RIN_SHADER_UNUSED) return 0;
    if (emit_instruction(compiler, opcode, reg, left.reg, right.reg,
                         RIN_SHADER_UNUSED, 0u) == UINT32_MAX) return 0;
    value->type = type;
    value->reg = reg;
    return 1;
}

static int parse_expression(Compiler* compiler, Value* value);

static AqcType parse_type(Compiler* compiler, int allow_bool) {
    AqcType type = AQC_TYPE_INVALID;
    if (token_is(&compiler->token, "i32")) type = AQC_TYPE_I32;
    else if (token_is(&compiler->token, "f32")) type = AQC_TYPE_F32;
    else if (allow_bool && token_is(&compiler->token, "bool"))
        type = AQC_TYPE_BOOL;
    else {
        (void)aqc_fail(compiler, AQC_ERROR_SYNTAX,
                       allow_bool ? "expected type i32, f32, or bool"
                                  : "expected type i32 or f32");
        return AQC_TYPE_INVALID;
    }
    if (!compiler_next(compiler)) return AQC_TYPE_INVALID;
    return type;
}

static int parse_resource_argument(Compiler* compiler, Symbol** resource) {
    char name[AQC_MAX_IDENTIFIER + 1u];
    Symbol* symbol;
    if (!copy_identifier(compiler, name)) return 0;
    symbol = find_symbol(compiler, name);
    if (!symbol || symbol->kind != SYMBOL_RESOURCE) {
        return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                        "'%s' is not a resource", name);
    }
    *resource = symbol;
    return 1;
}

static int builtin_spec(const char* name, uint32_t* builtin, AqcType* type,
                        uint32_t* stage) {
    struct BuiltinSpec { const char* name; uint32_t value; AqcType type;
                         uint32_t stage; };
    static const struct BuiltinSpec specs[] = {
        {"vertex_index", RIN_SHADER_BUILTIN_VERTEX_INDEX, AQC_TYPE_I32,
         RIN_SHADER_STAGE_VERTEX},
        {"instance_index", RIN_SHADER_BUILTIN_INSTANCE_INDEX, AQC_TYPE_I32,
         RIN_SHADER_STAGE_VERTEX},
        {"global_invocation_x", RIN_SHADER_BUILTIN_GLOBAL_INVOCATION_X,
         AQC_TYPE_I32, RIN_SHADER_STAGE_COMPUTE},
        {"global_invocation_y", RIN_SHADER_BUILTIN_GLOBAL_INVOCATION_Y,
         AQC_TYPE_I32, RIN_SHADER_STAGE_COMPUTE},
        {"global_invocation_z", RIN_SHADER_BUILTIN_GLOBAL_INVOCATION_Z,
         AQC_TYPE_I32, RIN_SHADER_STAGE_COMPUTE},
        {"local_invocation_x", RIN_SHADER_BUILTIN_LOCAL_INVOCATION_X,
         AQC_TYPE_I32, RIN_SHADER_STAGE_COMPUTE},
        {"local_invocation_y", RIN_SHADER_BUILTIN_LOCAL_INVOCATION_Y,
         AQC_TYPE_I32, RIN_SHADER_STAGE_COMPUTE},
        {"local_invocation_z", RIN_SHADER_BUILTIN_LOCAL_INVOCATION_Z,
         AQC_TYPE_I32, RIN_SHADER_STAGE_COMPUTE},
        {"workgroup_x", RIN_SHADER_BUILTIN_WORKGROUP_X, AQC_TYPE_I32,
         RIN_SHADER_STAGE_COMPUTE},
        {"workgroup_y", RIN_SHADER_BUILTIN_WORKGROUP_Y, AQC_TYPE_I32,
         RIN_SHADER_STAGE_COMPUTE},
        {"workgroup_z", RIN_SHADER_BUILTIN_WORKGROUP_Z, AQC_TYPE_I32,
         RIN_SHADER_STAGE_COMPUTE},
        {"frag_coord_x", RIN_SHADER_BUILTIN_FRAG_COORD_X, AQC_TYPE_F32,
         RIN_SHADER_STAGE_FRAGMENT},
        {"frag_coord_y", RIN_SHADER_BUILTIN_FRAG_COORD_Y, AQC_TYPE_F32,
         RIN_SHADER_STAGE_FRAGMENT},
        {"frag_coord_z", RIN_SHADER_BUILTIN_FRAG_COORD_Z, AQC_TYPE_F32,
         RIN_SHADER_STAGE_FRAGMENT},
        {"frag_coord_w", RIN_SHADER_BUILTIN_FRAG_COORD_W, AQC_TYPE_F32,
         RIN_SHADER_STAGE_FRAGMENT},
        {"front_facing", RIN_SHADER_BUILTIN_FRONT_FACING, AQC_TYPE_BOOL,
         RIN_SHADER_STAGE_FRAGMENT}
    };
    for (size_t index = 0u; index < sizeof(specs) / sizeof(specs[0]); index++) {
        if (strcmp(name, specs[index].name) == 0) {
            *builtin = specs[index].value;
            *type = specs[index].type;
            *stage = specs[index].stage;
            return 1;
        }
    }
    return 0;
}

static int parse_call(Compiler* compiler, const char* name, Value* value) {
    if (!expect_kind(compiler, TOKEN_LPAREN, "'('") ) return 0;
    if (strcmp(name, "i32") == 0 || strcmp(name, "f32") == 0) {
        AqcType destination = strcmp(name, "i32") == 0
            ? AQC_TYPE_I32 : AQC_TYPE_F32;
        Value operand;
        if (!parse_expression(compiler, &operand) ||
            !expect_kind(compiler, TOKEN_RPAREN, "')'")) return 0;
        if (operand.type == AQC_TYPE_BOOL && destination == AQC_TYPE_I32) {
            *value = operand;
            value->type = AQC_TYPE_I32;
            return 1;
        }
        if (operand.type == destination) {
            *value = operand;
            return 1;
        }
        if (operand.type != AQC_TYPE_I32 && operand.type != AQC_TYPE_F32) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "numeric conversion requires i32 or f32");
        }
        return emit_unary(compiler,
            destination == AQC_TYPE_F32 ? RIN_SHADER_OP_I32_TO_F32
                                        : RIN_SHADER_OP_F32_TO_I32,
            destination, operand, value);
    }
    if (strcmp(name, "min") == 0 || strcmp(name, "max") == 0) {
        Value left;
        Value right;
        uint16_t opcode;
        if (!parse_expression(compiler, &left) ||
            !expect_kind(compiler, TOKEN_COMMA, "','") ||
            !parse_expression(compiler, &right) ||
            !expect_kind(compiler, TOKEN_RPAREN, "')'")) return 0;
        if (left.type != right.type ||
            (left.type != AQC_TYPE_I32 && left.type != AQC_TYPE_F32)) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "min/max operands must have the same numeric type");
        }
        if (left.type == AQC_TYPE_I32) {
            opcode = strcmp(name, "min") == 0 ? RIN_SHADER_OP_MIN_I32
                                               : RIN_SHADER_OP_MAX_I32;
        } else {
            opcode = strcmp(name, "min") == 0 ? RIN_SHADER_OP_MIN_F32
                                               : RIN_SHADER_OP_MAX_F32;
        }
        return emit_binary(compiler, opcode, left.type, left, right, value);
    }
    if (strcmp(name, "load") == 0) {
        Symbol* resource;
        Value index;
        uint16_t reg;
        uint16_t opcode;
        if (!parse_resource_argument(compiler, &resource) ||
            !expect_kind(compiler, TOKEN_COMMA, "','") ||
            !parse_expression(compiler, &index) ||
            !expect_kind(compiler, TOKEN_RPAREN, "')'")) return 0;
        if (resource->resource_kind != RIN_SHADER_RESOURCE_STORAGE_BUFFER ||
            (resource->type != AQC_TYPE_I32 && resource->type != AQC_TYPE_F32)) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "load requires a typed storage resource");
        }
        if (index.type != AQC_TYPE_I32) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "storage index must be i32");
        }
        reg = allocate_register(compiler);
        if (reg == RIN_SHADER_UNUSED) return 0;
        opcode = resource->type == AQC_TYPE_I32
            ? RIN_SHADER_OP_LOAD_RESOURCE_I32
            : RIN_SHADER_OP_LOAD_RESOURCE_F32;
        if (emit_instruction(compiler, opcode, reg, index.reg,
                             RIN_SHADER_UNUSED, (uint16_t)resource->index,
                             0u) == UINT32_MAX) return 0;
        compiler->resource_used_mask |= UINT64_C(1) << resource->index;
        value->type = resource->type;
        value->reg = reg;
        return 1;
    }
    if (strcmp(name, "sample") == 0) {
        Symbol* image;
        Symbol* sampler;
        Value coordinate;
        uint16_t reg;
        uint16_t opcode;
        if (!parse_resource_argument(compiler, &image) ||
            !expect_kind(compiler, TOKEN_COMMA, "','") ||
            !parse_resource_argument(compiler, &sampler) ||
            !expect_kind(compiler, TOKEN_COMMA, "','") ||
            !parse_expression(compiler, &coordinate) ||
            !expect_kind(compiler, TOKEN_RPAREN, "')'")) return 0;
        if (compiler->stage != RIN_SHADER_STAGE_FRAGMENT) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "sample is available only in fragment shaders");
        }
        if (image->resource_kind != RIN_SHADER_RESOURCE_SAMPLED_IMAGE ||
            sampler->resource_kind != RIN_SHADER_RESOURCE_SAMPLER ||
            coordinate.type != image->type) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "sample requires texture, sampler, and matching coordinate type");
        }
        reg = allocate_register(compiler);
        if (reg == RIN_SHADER_UNUSED) return 0;
        opcode = image->type == AQC_TYPE_I32
            ? RIN_SHADER_OP_SAMPLE_IMAGE_I32 : RIN_SHADER_OP_SAMPLE_IMAGE_F32;
        if (emit_instruction(compiler, opcode, reg, coordinate.reg,
                             RIN_SHADER_UNUSED, (uint16_t)image->index,
                             sampler->index) == UINT32_MAX) return 0;
        compiler->resource_used_mask |= UINT64_C(1) << image->index;
        compiler->resource_used_mask |= UINT64_C(1) << sampler->index;
        value->type = image->type;
        value->reg = reg;
        return 1;
    }
    if (strcmp(name, "sample_compare") == 0) {
        Symbol* image;
        Symbol* sampler;
        Value coordinate;
        Value reference;
        uint16_t reg;
        uint16_t opcode;
        if (!parse_resource_argument(compiler, &image) ||
            !expect_kind(compiler, TOKEN_COMMA, "','") ||
            !parse_resource_argument(compiler, &sampler) ||
            !expect_kind(compiler, TOKEN_COMMA, "','") ||
            !parse_expression(compiler, &coordinate) ||
            !expect_kind(compiler, TOKEN_COMMA, "','") ||
            !parse_expression(compiler, &reference) ||
            !expect_kind(compiler, TOKEN_RPAREN, "')'")) return 0;
        if (compiler->stage != RIN_SHADER_STAGE_FRAGMENT) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "sample_compare is available only in fragment shaders");
        }
        if (image->resource_kind != RIN_SHADER_RESOURCE_SAMPLED_DEPTH_IMAGE ||
            sampler->resource_kind != RIN_SHADER_RESOURCE_COMPARISON_SAMPLER ||
            coordinate.type != image->type || reference.type != image->type) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "sample_compare arguments have incompatible resource or scalar types");
        }
        reg = allocate_register(compiler);
        if (reg == RIN_SHADER_UNUSED) return 0;
        opcode = image->type == AQC_TYPE_I32
            ? RIN_SHADER_OP_SAMPLE_COMPARE_I32
            : RIN_SHADER_OP_SAMPLE_COMPARE_F32;
        if (emit_instruction(compiler, opcode, reg, coordinate.reg,
                             reference.reg, (uint16_t)image->index,
                             sampler->index) == UINT32_MAX) return 0;
        compiler->resource_used_mask |= UINT64_C(1) << image->index;
        compiler->resource_used_mask |= UINT64_C(1) << sampler->index;
        value->type = image->type;
        value->reg = reg;
        return 1;
    }
    if (strcmp(name, "builtin") == 0) {
        char builtin_name[AQC_MAX_IDENTIFIER + 1u];
        uint32_t builtin;
        uint32_t required_stage;
        AqcType type;
        uint16_t reg;
        if (!copy_identifier(compiler, builtin_name) ||
            !expect_kind(compiler, TOKEN_RPAREN, "')'")) return 0;
        if (!builtin_spec(builtin_name, &builtin, &type, &required_stage)) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "unknown shader builtin '%s'", builtin_name);
        }
        if (compiler->stage != required_stage) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "builtin '%s' is unavailable in this stage",
                            builtin_name);
        }
        reg = allocate_register(compiler);
        if (reg == RIN_SHADER_UNUSED) return 0;
        if (emit_instruction(compiler,
                type == AQC_TYPE_F32 ? RIN_SHADER_OP_LOAD_BUILTIN_F32
                                     : RIN_SHADER_OP_LOAD_BUILTIN_I32,
                reg, RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                builtin) == UINT32_MAX) return 0;
        value->type = type;
        value->reg = reg;
        return 1;
    }
    return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                    "unknown function '%s'", name);
}

static int parse_primary(Compiler* compiler, Value* value) {
    if (compiler->token.kind == TOKEN_INTEGER) {
        uint32_t bits = compiler->token.integer;
        if (!compiler_next(compiler)) return 0;
        return emit_constant_i32(compiler, bits, value);
    }
    if (compiler->token.kind == TOKEN_FLOAT) {
        float number = compiler->token.floating;
        if (!compiler_next(compiler)) return 0;
        return emit_constant_f32(compiler, number, value);
    }
    if (token_is(&compiler->token, "true") ||
        token_is(&compiler->token, "false")) {
        uint32_t bits = token_is(&compiler->token, "true") ? 1u : 0u;
        if (!compiler_next(compiler) ||
            !emit_constant_i32(compiler, bits, value)) return 0;
        value->type = AQC_TYPE_BOOL;
        return 1;
    }
    if (compiler->token.kind == TOKEN_IDENTIFIER) {
        char name[AQC_MAX_IDENTIFIER + 1u];
        Symbol* symbol;
        uint16_t reg;
        uint16_t opcode;
        if (!copy_identifier(compiler, name)) return 0;
        if (compiler->token.kind == TOKEN_LPAREN) {
            return parse_call(compiler, name, value);
        }
        symbol = find_symbol(compiler, name);
        if (!symbol) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "unknown symbol '%s'", name);
        }
        if (symbol->kind == SYMBOL_LOCAL) {
            value->type = symbol->type;
            value->reg = symbol->reg;
            return 1;
        }
        if (symbol->kind != SYMBOL_INPUT) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "'%s' is not a value", name);
        }
        reg = allocate_register(compiler);
        if (reg == RIN_SHADER_UNUSED) return 0;
        opcode = symbol->type == AQC_TYPE_I32
            ? RIN_SHADER_OP_LOAD_INPUT : RIN_SHADER_OP_LOAD_INPUT_F32;
        if (emit_instruction(compiler, opcode, reg, RIN_SHADER_UNUSED,
                             RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                             symbol->index) == UINT32_MAX) return 0;
        value->type = symbol->type;
        value->reg = reg;
        return 1;
    }
    if (accept_kind(compiler, TOKEN_LPAREN)) {
        if (!parse_expression(compiler, value)) return 0;
        return expect_kind(compiler, TOKEN_RPAREN, "')'");
    }
    return aqc_fail(compiler, AQC_ERROR_SYNTAX, "expected expression");
}

static int parse_unary(Compiler* compiler, Value* value) {
    TokenKind operator_kind = compiler->token.kind;
    if (operator_kind != TOKEN_MINUS && operator_kind != TOKEN_BANG &&
        operator_kind != TOKEN_TILDE) {
        return parse_primary(compiler, value);
    }
    if (!compiler_next(compiler) || !parse_unary(compiler, value)) return 0;
    if (operator_kind == TOKEN_MINUS) {
        Value zero;
        Value operand = *value;
        if (operand.type == AQC_TYPE_I32) {
            if (!emit_constant_i32(compiler, 0u, &zero)) return 0;
            return emit_binary(compiler, RIN_SHADER_OP_SUB_I32, AQC_TYPE_I32,
                               zero, operand, value);
        }
        if (operand.type == AQC_TYPE_F32) {
            if (!emit_constant_f32(compiler, 0.0f, &zero)) return 0;
            return emit_binary(compiler, RIN_SHADER_OP_SUB_F32, AQC_TYPE_F32,
                               zero, operand, value);
        }
        return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                        "unary '-' requires i32 or f32");
    }
    if (operator_kind == TOKEN_BANG) {
        Value zero;
        Value operand = *value;
        if (operand.type != AQC_TYPE_I32 && operand.type != AQC_TYPE_BOOL) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "unary '!' requires i32 or bool");
        }
        if (!emit_constant_i32(compiler, 0u, &zero)) return 0;
        return emit_binary(compiler, RIN_SHADER_OP_CMP_EQ_I32, AQC_TYPE_BOOL,
                           operand, zero, value);
    }
    if (value->type != AQC_TYPE_I32) {
        return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                        "unary '~' requires i32");
    }
    {
        Value all_bits;
        Value operand = *value;
        if (!emit_constant_i32(compiler, UINT32_MAX, &all_bits)) return 0;
        return emit_binary(compiler, RIN_SHADER_OP_XOR_I32, AQC_TYPE_I32,
                           operand, all_bits, value);
    }
}

static int parse_multiplicative(Compiler* compiler, Value* value) {
    if (!parse_unary(compiler, value)) return 0;
    while (compiler->token.kind == TOKEN_STAR ||
           compiler->token.kind == TOKEN_SLASH ||
           compiler->token.kind == TOKEN_PERCENT) {
        TokenKind operation = compiler->token.kind;
        Value left = *value;
        Value right;
        uint16_t opcode;
        if (!compiler_next(compiler) || !parse_unary(compiler, &right)) return 0;
        if (left.type != right.type ||
            (left.type != AQC_TYPE_I32 && left.type != AQC_TYPE_F32)) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "arithmetic operands must have the same numeric type");
        }
        if (operation == TOKEN_PERCENT && left.type != AQC_TYPE_I32) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "remainder is available only for i32");
        }
        if (left.type == AQC_TYPE_I32) {
            opcode = operation == TOKEN_STAR ? RIN_SHADER_OP_MUL_I32
                   : operation == TOKEN_SLASH ? RIN_SHADER_OP_DIV_I32
                                               : RIN_SHADER_OP_MOD_I32;
        } else {
            opcode = operation == TOKEN_STAR ? RIN_SHADER_OP_MUL_F32
                                             : RIN_SHADER_OP_DIV_F32;
        }
        if (!emit_binary(compiler, opcode, left.type, left, right, value))
            return 0;
    }
    return 1;
}

static int parse_additive(Compiler* compiler, Value* value) {
    if (!parse_multiplicative(compiler, value)) return 0;
    while (compiler->token.kind == TOKEN_PLUS ||
           compiler->token.kind == TOKEN_MINUS) {
        TokenKind operation = compiler->token.kind;
        Value left = *value;
        Value right;
        uint16_t opcode;
        if (!compiler_next(compiler) || !parse_multiplicative(compiler, &right))
            return 0;
        if (left.type != right.type ||
            (left.type != AQC_TYPE_I32 && left.type != AQC_TYPE_F32)) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "arithmetic operands must have the same numeric type");
        }
        if (left.type == AQC_TYPE_I32) {
            opcode = operation == TOKEN_PLUS ? RIN_SHADER_OP_ADD_I32
                                             : RIN_SHADER_OP_SUB_I32;
        } else {
            opcode = operation == TOKEN_PLUS ? RIN_SHADER_OP_ADD_F32
                                             : RIN_SHADER_OP_SUB_F32;
        }
        if (!emit_binary(compiler, opcode, left.type, left, right, value))
            return 0;
    }
    return 1;
}

static int parse_shift(Compiler* compiler, Value* value) {
    if (!parse_additive(compiler, value)) return 0;
    while (compiler->token.kind == TOKEN_SHIFT_LEFT ||
           compiler->token.kind == TOKEN_SHIFT_RIGHT) {
        TokenKind operation = compiler->token.kind;
        Value left = *value;
        Value right;
        if (!compiler_next(compiler) || !parse_additive(compiler, &right))
            return 0;
        if (left.type != AQC_TYPE_I32 || right.type != AQC_TYPE_I32) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "shift operands must be i32");
        }
        if (!emit_binary(compiler,
                operation == TOKEN_SHIFT_LEFT ? RIN_SHADER_OP_SHL_I32
                                              : RIN_SHADER_OP_SHR_I32,
                AQC_TYPE_I32, left, right, value)) return 0;
    }
    return 1;
}

static int parse_relational(Compiler* compiler, Value* value) {
    if (!parse_shift(compiler, value)) return 0;
    while (compiler->token.kind == TOKEN_LESS ||
           compiler->token.kind == TOKEN_LESS_EQUAL ||
           compiler->token.kind == TOKEN_GREATER ||
           compiler->token.kind == TOKEN_GREATER_EQUAL) {
        TokenKind operation = compiler->token.kind;
        Value left = *value;
        Value right;
        uint16_t opcode;
        if (!compiler_next(compiler) || !parse_shift(compiler, &right)) return 0;
        if (left.type != right.type ||
            (left.type != AQC_TYPE_I32 && left.type != AQC_TYPE_F32)) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "comparison operands must have the same numeric type");
        }
        if (left.type == AQC_TYPE_I32) {
            opcode = operation == TOKEN_LESS ? RIN_SHADER_OP_CMP_LT_I32
                   : operation == TOKEN_LESS_EQUAL ? RIN_SHADER_OP_CMP_LE_I32
                   : operation == TOKEN_GREATER ? RIN_SHADER_OP_CMP_GT_I32
                                                : RIN_SHADER_OP_CMP_GE_I32;
        } else {
            opcode = operation == TOKEN_LESS ? RIN_SHADER_OP_CMP_LT_F32
                   : operation == TOKEN_LESS_EQUAL ? RIN_SHADER_OP_CMP_LE_F32
                   : operation == TOKEN_GREATER ? RIN_SHADER_OP_CMP_GT_F32
                                                : RIN_SHADER_OP_CMP_GE_F32;
        }
        if (!emit_binary(compiler, opcode, AQC_TYPE_BOOL, left, right, value))
            return 0;
    }
    return 1;
}

static int parse_equality(Compiler* compiler, Value* value) {
    if (!parse_relational(compiler, value)) return 0;
    while (compiler->token.kind == TOKEN_EQUAL_EQUAL ||
           compiler->token.kind == TOKEN_BANG_EQUAL) {
        TokenKind operation = compiler->token.kind;
        Value left = *value;
        Value right;
        uint16_t opcode;
        if (!compiler_next(compiler) || !parse_relational(compiler, &right))
            return 0;
        if (left.type != right.type) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "equality operands must have the same type");
        }
        if (left.type == AQC_TYPE_F32) {
            opcode = operation == TOKEN_EQUAL_EQUAL ? RIN_SHADER_OP_CMP_EQ_F32
                                                    : RIN_SHADER_OP_CMP_NE_F32;
        } else if (left.type == AQC_TYPE_I32 || left.type == AQC_TYPE_BOOL) {
            opcode = operation == TOKEN_EQUAL_EQUAL ? RIN_SHADER_OP_CMP_EQ_I32
                                                    : RIN_SHADER_OP_CMP_NE_I32;
        } else {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "invalid equality operand type");
        }
        if (!emit_binary(compiler, opcode, AQC_TYPE_BOOL, left, right, value))
            return 0;
    }
    return 1;
}

static int parse_bit_and(Compiler* compiler, Value* value) {
    if (!parse_equality(compiler, value)) return 0;
    while (compiler->token.kind == TOKEN_AMP) {
        Value left = *value;
        Value right;
        if (!compiler_next(compiler) || !parse_equality(compiler, &right))
            return 0;
        if (left.type != AQC_TYPE_I32 || right.type != AQC_TYPE_I32) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "bitwise operands must be i32");
        }
        if (!emit_binary(compiler, RIN_SHADER_OP_AND_I32, AQC_TYPE_I32,
                         left, right, value)) return 0;
    }
    return 1;
}

static int parse_bit_xor(Compiler* compiler, Value* value) {
    if (!parse_bit_and(compiler, value)) return 0;
    while (compiler->token.kind == TOKEN_CARET) {
        Value left = *value;
        Value right;
        if (!compiler_next(compiler) || !parse_bit_and(compiler, &right))
            return 0;
        if (left.type != AQC_TYPE_I32 || right.type != AQC_TYPE_I32) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "bitwise operands must be i32");
        }
        if (!emit_binary(compiler, RIN_SHADER_OP_XOR_I32, AQC_TYPE_I32,
                         left, right, value)) return 0;
    }
    return 1;
}

static int parse_bit_or(Compiler* compiler, Value* value) {
    if (!parse_bit_xor(compiler, value)) return 0;
    while (compiler->token.kind == TOKEN_PIPE) {
        Value left = *value;
        Value right;
        if (!compiler_next(compiler) || !parse_bit_xor(compiler, &right))
            return 0;
        if (left.type != AQC_TYPE_I32 || right.type != AQC_TYPE_I32) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "bitwise operands must be i32");
        }
        if (!emit_binary(compiler, RIN_SHADER_OP_OR_I32, AQC_TYPE_I32,
                         left, right, value)) return 0;
    }
    return 1;
}

static int parse_logical_and(Compiler* compiler, Value* value) {
    if (!parse_bit_or(compiler, value)) return 0;
    while (compiler->token.kind == TOKEN_AND_AND) {
        Value left = *value;
        Value right;
        if (!compiler_next(compiler) || !parse_bit_or(compiler, &right))
            return 0;
        if (left.type != AQC_TYPE_BOOL || right.type != AQC_TYPE_BOOL) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "logical operands must be bool");
        }
        if (!emit_binary(compiler, RIN_SHADER_OP_AND_I32, AQC_TYPE_BOOL,
                         left, right, value)) return 0;
    }
    return 1;
}

static int parse_logical_or(Compiler* compiler, Value* value) {
    if (!parse_logical_and(compiler, value)) return 0;
    while (compiler->token.kind == TOKEN_OR_OR) {
        Value left = *value;
        Value right;
        if (!compiler_next(compiler) || !parse_logical_and(compiler, &right))
            return 0;
        if (left.type != AQC_TYPE_BOOL || right.type != AQC_TYPE_BOOL) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "logical operands must be bool");
        }
        if (!emit_binary(compiler, RIN_SHADER_OP_OR_I32, AQC_TYPE_BOOL,
                         left, right, value)) return 0;
    }
    return 1;
}

static int parse_expression(Compiler* compiler, Value* value) {
    int result;
    if (++compiler->expression_depth > AQC_MAX_EXPRESSION_DEPTH) {
        compiler->expression_depth--;
        return aqc_fail(compiler, AQC_ERROR_LIMIT,
                        "expression nesting exceeds %u",
                        AQC_MAX_EXPRESSION_DEPTH);
    }
    result = parse_logical_or(compiler, value);
    compiler->expression_depth--;
    return result;
}

static int parse_statement(Compiler* compiler);

static int parse_block(Compiler* compiler) {
    uint32_t saved_symbols = compiler->symbol_count;
    if (++compiler->block_depth > AQC_MAX_NESTING) {
        compiler->block_depth--;
        return aqc_fail(compiler, AQC_ERROR_LIMIT,
                        "block nesting exceeds %u", AQC_MAX_NESTING);
    }
    if (!expect_kind(compiler, TOKEN_LBRACE, "'{'") ) return 0;
    while (!compiler->failed && compiler->token.kind != TOKEN_RBRACE &&
           compiler->token.kind != TOKEN_EOF) {
        if (!parse_statement(compiler)) break;
    }
    if (!compiler->failed && !expect_kind(compiler, TOKEN_RBRACE, "'}'"))
        return 0;
    compiler->symbol_count = saved_symbols;
    compiler->block_depth--;
    return !compiler->failed;
}

static int parse_let(Compiler* compiler) {
    AqcType declared_type;
    char name[AQC_MAX_IDENTIFIER + 1u];
    Value value;
    if (!expect_word(compiler, "let")) return 0;
    declared_type = parse_type(compiler, 1);
    if (declared_type == AQC_TYPE_INVALID || !copy_identifier(compiler, name) ||
        !expect_kind(compiler, TOKEN_ASSIGN, "'='") ||
        !parse_expression(compiler, &value) ||
        !expect_kind(compiler, TOKEN_SEMICOLON, "';'")) return 0;
    if (declared_type != value.type) {
        return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                        "initializer type does not match local '%s'", name);
    }
    return add_symbol(compiler, name, SYMBOL_LOCAL, declared_type, 0u, 0u,
                      value.reg);
}

static int parse_store_statement(Compiler* compiler) {
    Symbol* resource;
    Value index;
    Value value;
    uint16_t opcode;
    if (!expect_word(compiler, "store") ||
        !expect_kind(compiler, TOKEN_LPAREN, "'('") ||
        !parse_resource_argument(compiler, &resource) ||
        !expect_kind(compiler, TOKEN_COMMA, "','") ||
        !parse_expression(compiler, &index) ||
        !expect_kind(compiler, TOKEN_COMMA, "','") ||
        !parse_expression(compiler, &value) ||
        !expect_kind(compiler, TOKEN_RPAREN, "')'") ||
        !expect_kind(compiler, TOKEN_SEMICOLON, "';'")) return 0;
    if (resource->resource_kind != RIN_SHADER_RESOURCE_STORAGE_BUFFER ||
        index.type != AQC_TYPE_I32 || value.type != resource->type) {
        return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                        "store requires typed storage, i32 index, and matching value");
    }
    opcode = resource->type == AQC_TYPE_I32
        ? RIN_SHADER_OP_STORE_RESOURCE_I32
        : RIN_SHADER_OP_STORE_RESOURCE_F32;
    if (emit_instruction(compiler, opcode, RIN_SHADER_UNUSED, index.reg,
                         value.reg, (uint16_t)resource->index, 0u) ==
        UINT32_MAX) return 0;
    compiler->resource_used_mask |= UINT64_C(1) << resource->index;
    return 1;
}

static int parse_output_assignment(Compiler* compiler) {
    char name[AQC_MAX_IDENTIFIER + 1u];
    Symbol* output;
    Value value;
    uint16_t opcode;
    if (!copy_identifier(compiler, name)) return 0;
    output = find_symbol(compiler, name);
    if (!output || output->kind != SYMBOL_OUTPUT) {
        return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                        "'%s' is not an output", name);
    }
    if (!expect_kind(compiler, TOKEN_ASSIGN, "'='") ||
        !parse_expression(compiler, &value) ||
        !expect_kind(compiler, TOKEN_SEMICOLON, "';'")) return 0;
    if (value.type != output->type) {
        return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                        "value type does not match output '%s'", name);
    }
    opcode = output->type == AQC_TYPE_I32
        ? RIN_SHADER_OP_STORE_OUTPUT : RIN_SHADER_OP_STORE_OUTPUT_F32;
    if (emit_instruction(compiler, opcode, RIN_SHADER_UNUSED, value.reg,
                         RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                         output->index) == UINT32_MAX) return 0;
    compiler->output_assigned_mask |= UINT64_C(1) << output->index;
    return 1;
}

static int parse_if_statement(Compiler* compiler) {
    Value condition;
    Value zero;
    Value inverse;
    uint32_t branch;
    uint32_t skip_else = UINT32_MAX;
    uint64_t assigned_before;
    uint64_t assigned_then;
    uint64_t assigned_else;
    if (!expect_word(compiler, "if") ||
        !expect_kind(compiler, TOKEN_LPAREN, "'('") ||
        !parse_expression(compiler, &condition) ||
        !expect_kind(compiler, TOKEN_RPAREN, "')'")) return 0;
    if (condition.type != AQC_TYPE_BOOL && condition.type != AQC_TYPE_I32) {
        return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                        "if condition must be bool or i32");
    }
    if (!emit_constant_i32(compiler, 0u, &zero) ||
        !emit_binary(compiler, RIN_SHADER_OP_CMP_EQ_I32, AQC_TYPE_BOOL,
                     condition, zero, &inverse)) return 0;
    branch = emit_instruction(compiler, RIN_SHADER_OP_JUMP_IF,
                              RIN_SHADER_UNUSED, inverse.reg,
                              RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, 0u);
    if (branch == UINT32_MAX) return 0;
    assigned_before = compiler->output_assigned_mask;
    if (!parse_block(compiler)) return 0;
    assigned_then = compiler->output_assigned_mask;
    if (token_is(&compiler->token, "else")) {
        skip_else = emit_instruction(compiler, RIN_SHADER_OP_JUMP,
                                     RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                                     RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, 0u);
        if (skip_else == UINT32_MAX ||
            !patch_target(compiler, branch, compiler->instruction_count) ||
            !compiler_next(compiler)) return 0;
        compiler->output_assigned_mask = assigned_before;
        if (!parse_block(compiler)) return 0;
        assigned_else = compiler->output_assigned_mask;
        if (!patch_target(compiler, skip_else, compiler->instruction_count))
            return 0;
        compiler->output_assigned_mask = assigned_before |
            (assigned_then & assigned_else);
    } else {
        if (!patch_target(compiler, branch, compiler->instruction_count))
            return 0;
        compiler->output_assigned_mask = assigned_before;
    }
    return 1;
}

static int parse_statement(Compiler* compiler) {
    if (token_is(&compiler->token, "let")) return parse_let(compiler);
    if (token_is(&compiler->token, "store"))
        return parse_store_statement(compiler);
    if (token_is(&compiler->token, "if")) return parse_if_statement(compiler);
    if (token_is(&compiler->token, "discard")) {
        if (compiler->stage != RIN_SHADER_STAGE_FRAGMENT) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "discard is available only in fragment shaders");
        }
        if (!compiler_next(compiler) ||
            !expect_kind(compiler, TOKEN_SEMICOLON, "';'")) return 0;
        return emit_instruction(compiler, RIN_SHADER_OP_DISCARD,
                                RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                                RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, 0u) !=
               UINT32_MAX;
    }
    if (token_is(&compiler->token, "return")) {
        if (compiler->block_depth != 1u) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "return is allowed only at the end of main");
        }
        if (!compiler_next(compiler) ||
            !expect_kind(compiler, TOKEN_SEMICOLON, "';'")) return 0;
        if (compiler->token.kind != TOKEN_RBRACE) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "return must be the final statement in main");
        }
        compiler->saw_return = 1;
        return emit_instruction(compiler, RIN_SHADER_OP_RETURN,
                                RIN_SHADER_UNUSED, RIN_SHADER_UNUSED,
                                RIN_SHADER_UNUSED, RIN_SHADER_UNUSED, 0u) !=
               UINT32_MAX;
    }
    if (compiler->token.kind == TOKEN_IDENTIFIER) {
        return parse_output_assignment(compiler);
    }
    return aqc_fail(compiler, AQC_ERROR_SYNTAX, "expected statement");
}

static int parse_location(Compiler* compiler, const char* annotation,
                          uint32_t limit, uint32_t* value) {
    if (!expect_kind(compiler, TOKEN_AT, "'@'") ||
        !expect_word(compiler, annotation) ||
        !expect_kind(compiler, TOKEN_LPAREN, "'('") ) return 0;
    if (compiler->token.kind != TOKEN_INTEGER ||
        compiler->token.integer >= limit) {
        return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                        "%s must be below %u", annotation, limit);
    }
    *value = compiler->token.integer;
    return compiler_next(compiler) &&
           expect_kind(compiler, TOKEN_RPAREN, "')'") &&
           expect_kind(compiler, TOKEN_SEMICOLON, "';'");
}

static int parse_io_declaration(Compiler* compiler, int output) {
    AqcType type;
    char name[AQC_MAX_IDENTIFIER + 1u];
    uint32_t location;
    uint64_t bit;
    if (!compiler_next(compiler)) return 0;
    type = parse_type(compiler, 0);
    if (type == AQC_TYPE_INVALID || !copy_identifier(compiler, name) ||
        !parse_location(compiler, "location", RIN_SHADER_MAX_IO, &location))
        return 0;
    bit = UINT64_C(1) << location;
    if (output ? (compiler->output_mask & bit) : (compiler->input_mask & bit)) {
        return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                        "duplicate %s location %u",
                        output ? "output" : "input", location);
    }
    if (output) {
        compiler->output_mask |= bit;
        if (compiler->output_count <= location)
            compiler->output_count = location + 1u;
    } else {
        compiler->input_mask |= bit;
        if (compiler->input_count <= location)
            compiler->input_count = location + 1u;
    }
    return add_symbol(compiler, name,
                      output ? SYMBOL_OUTPUT : SYMBOL_INPUT, type, location,
                      0u, RIN_SHADER_UNUSED);
}

static int parse_resource_type(Compiler* compiler, AqcType* type,
                               uint16_t* kind) {
    if (token_is(&compiler->token, "storage_i32")) {
        *type = AQC_TYPE_I32;
        *kind = RIN_SHADER_RESOURCE_STORAGE_BUFFER;
    } else if (token_is(&compiler->token, "storage_f32")) {
        *type = AQC_TYPE_F32;
        *kind = RIN_SHADER_RESOURCE_STORAGE_BUFFER;
    } else if (token_is(&compiler->token, "texture_i32")) {
        *type = AQC_TYPE_I32;
        *kind = RIN_SHADER_RESOURCE_SAMPLED_IMAGE;
    } else if (token_is(&compiler->token, "texture_f32")) {
        *type = AQC_TYPE_F32;
        *kind = RIN_SHADER_RESOURCE_SAMPLED_IMAGE;
    } else if (token_is(&compiler->token, "depth_i32")) {
        *type = AQC_TYPE_I32;
        *kind = RIN_SHADER_RESOURCE_SAMPLED_DEPTH_IMAGE;
    } else if (token_is(&compiler->token, "depth_f32")) {
        *type = AQC_TYPE_F32;
        *kind = RIN_SHADER_RESOURCE_SAMPLED_DEPTH_IMAGE;
    } else if (token_is(&compiler->token, "sampler")) {
        *type = AQC_TYPE_INVALID;
        *kind = RIN_SHADER_RESOURCE_SAMPLER;
    } else if (token_is(&compiler->token, "comparison_sampler")) {
        *type = AQC_TYPE_INVALID;
        *kind = RIN_SHADER_RESOURCE_COMPARISON_SAMPLER;
    } else {
        return aqc_fail(compiler, AQC_ERROR_SYNTAX,
                        "expected Aquamarine resource type");
    }
    return compiler_next(compiler);
}

static int parse_resource_declaration(Compiler* compiler) {
    AqcType type = AQC_TYPE_INVALID;
    uint16_t kind = RIN_SHADER_RESOURCE_NONE;
    char name[AQC_MAX_IDENTIFIER + 1u];
    uint32_t binding;
    uint64_t bit;
    if (!compiler_next(compiler) ||
        !parse_resource_type(compiler, &type, &kind) ||
        !copy_identifier(compiler, name) ||
        !parse_location(compiler, "binding", RIN_SHADER_MAX_RESOURCES,
                        &binding)) return 0;
    bit = UINT64_C(1) << binding;
    if (compiler->resource_mask & bit) {
        return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                        "duplicate resource binding %u", binding);
    }
    compiler->resource_mask |= bit;
    if (compiler->resource_count <= binding)
        compiler->resource_count = binding + 1u;
    return add_symbol(compiler, name, SYMBOL_RESOURCE, type, binding, kind,
                      RIN_SHADER_UNUSED);
}

static int complete_mask(uint64_t mask, uint32_t count) {
    uint64_t expected;
    if (count == 0u) return mask == 0u;
    expected = count == 64u ? UINT64_MAX : (UINT64_C(1) << count) - 1u;
    return mask == expected;
}

static int parse_workgroup(Compiler* compiler) {
    uint32_t values[3];
    if (!expect_kind(compiler, TOKEN_AT, "'@'") ||
        !expect_word(compiler, "workgroup") ||
        !expect_kind(compiler, TOKEN_LPAREN, "'('") ) return 0;
    for (uint32_t index = 0u; index < 3u; index++) {
        if (compiler->token.kind != TOKEN_INTEGER ||
            compiler->token.integer == 0u) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "workgroup dimensions must be positive integers");
        }
        values[index] = compiler->token.integer;
        if (!compiler_next(compiler)) return 0;
        if (index != 2u &&
            !expect_kind(compiler, TOKEN_COMMA, "','")) return 0;
    }
    if (!expect_kind(compiler, TOKEN_RPAREN, "')'")) return 0;
    if (values[0] > 1024u || values[1] > 1024u || values[2] > 64u ||
        (uint64_t)values[0] * values[1] * values[2] > 1024u) {
        return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                        "workgroup exceeds RinShader limits");
    }
    compiler->workgroup_x = values[0];
    compiler->workgroup_y = values[1];
    compiler->workgroup_z = values[2];
    return 1;
}

static int parse_shader(Compiler* compiler) {
    if (!expect_word(compiler, "shader")) return 0;
    if (token_is(&compiler->token, "vertex"))
        compiler->stage = RIN_SHADER_STAGE_VERTEX;
    else if (token_is(&compiler->token, "fragment"))
        compiler->stage = RIN_SHADER_STAGE_FRAGMENT;
    else if (token_is(&compiler->token, "compute"))
        compiler->stage = RIN_SHADER_STAGE_COMPUTE;
    else
        return aqc_fail(compiler, AQC_ERROR_SYNTAX,
                        "expected shader stage vertex, fragment, or compute");
    if (!compiler_next(compiler)) return 0;
    if (compiler->token.kind == TOKEN_AT) {
        if (compiler->stage != RIN_SHADER_STAGE_COMPUTE) {
            return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                            "workgroup is valid only for compute shaders");
        }
        if (!parse_workgroup(compiler)) return 0;
    } else if (compiler->stage == RIN_SHADER_STAGE_COMPUTE) {
        return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                        "compute shader requires @workgroup(x, y, z)");
    }
    if (!expect_kind(compiler, TOKEN_LBRACE, "'{'") ) return 0;
    while (token_is(&compiler->token, "input") ||
           token_is(&compiler->token, "output") ||
           token_is(&compiler->token, "resource")) {
        if (token_is(&compiler->token, "input")) {
            if (!parse_io_declaration(compiler, 0)) return 0;
        } else if (token_is(&compiler->token, "output")) {
            if (!parse_io_declaration(compiler, 1)) return 0;
        } else if (!parse_resource_declaration(compiler)) return 0;
    }
    if (!complete_mask(compiler->input_mask, compiler->input_count) ||
        !complete_mask(compiler->output_mask, compiler->output_count) ||
        !complete_mask(compiler->resource_mask, compiler->resource_count)) {
        return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                        "locations and bindings must be contiguous from zero");
    }
    if (!expect_word(compiler, "fn") || !expect_word(compiler, "main") ||
        !expect_kind(compiler, TOKEN_LPAREN, "'('") ||
        !expect_kind(compiler, TOKEN_RPAREN, "')'") ||
        !parse_block(compiler)) return 0;
    if (!compiler->saw_return) {
        return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                        "main must end with return;");
    }
    if (compiler->output_assigned_mask != compiler->output_mask) {
        return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                        "every declared output must be assigned on all paths");
    }
    if (compiler->resource_used_mask != compiler->resource_mask) {
        return aqc_fail(compiler, AQC_ERROR_SEMANTIC,
                        "every declared resource must be used");
    }
    if (!expect_kind(compiler, TOKEN_RBRACE, "'}'")) return 0;
    if (compiler->token.kind != TOKEN_EOF) {
        return aqc_fail(compiler, AQC_ERROR_SYNTAX,
                        "unexpected tokens after shader declaration");
    }
    return 1;
}

int aqc_compile(const char* source, size_t source_size, void* output,
                size_t output_capacity, size_t* output_size,
                AqcDiagnostic* diagnostic) {
    Compiler compiler;
    RinShaderHeaderV1 header;
    RinShaderInfoV1 info;
    size_t total_size;
    int validation;

    if (output_size) *output_size = 0u;
    if (diagnostic) memset(diagnostic, 0, sizeof(*diagnostic));
    if (!source || !output || !output_size || source_size == 0u) {
        if (diagnostic) {
            diagnostic->line = 1u;
            diagnostic->column = 1u;
            (void)snprintf(diagnostic->message, sizeof(diagnostic->message),
                           "source, output, and output_size are required");
        }
        return AQC_ERROR_INVALID_ARGUMENT;
    }
    if (source_size > AQC_MAX_SOURCE_SIZE) {
        if (diagnostic) {
            diagnostic->line = 1u;
            diagnostic->column = 1u;
            (void)snprintf(diagnostic->message, sizeof(diagnostic->message),
                           "source exceeds %u bytes", AQC_MAX_SOURCE_SIZE);
        }
        return AQC_ERROR_SOURCE_TOO_LARGE;
    }
    if (output_capacity < sizeof(RinShaderHeaderV1) +
                              sizeof(RinShaderInstructionV1)) {
        if (diagnostic) {
            diagnostic->line = 1u;
            diagnostic->column = 1u;
            (void)snprintf(diagnostic->message, sizeof(diagnostic->message),
                           "output buffer is too small");
        }
        return AQC_ERROR_OUTPUT_TOO_SMALL;
    }
    memset(&compiler, 0, sizeof(compiler));
    compiler.source = source;
    compiler.source_size = source_size;
    compiler.line = 1u;
    compiler.column = 1u;
    compiler.output = (uint8_t*)output;
    compiler.output_capacity = output_capacity;
    compiler.diagnostic = diagnostic;
    if (!compiler_next(&compiler) || !parse_shader(&compiler)) {
        return compiler.failure_code ? compiler.failure_code
                                     : AQC_ERROR_INTERNAL;
    }

    memset(&header, 0, sizeof(header));
    header.magic = RIN_SHADER_MAGIC;
    header.version = RIN_SHADER_IR_VERSION;
    header.header_size = sizeof(header);
    header.stage = compiler.stage;
    header.instruction_count = compiler.instruction_count;
    header.register_count = compiler.next_register == 0u
        ? 1u : compiler.next_register;
    header.input_count = compiler.input_count;
    header.output_count = compiler.output_count;
    header.resource_count = compiler.resource_count;
    header.workgroup_x = compiler.workgroup_x;
    header.workgroup_y = compiler.workgroup_y;
    header.workgroup_z = compiler.workgroup_z;
    total_size = sizeof(header) +
        (size_t)header.instruction_count * sizeof(RinShaderInstructionV1);
    header.total_size = (uint32_t)total_size;
    memcpy(output, &header, sizeof(header));
    validation = ringpu_shader_validate(output, total_size, &info);
    if (validation != RIN_SHADER_OK) {
        if (diagnostic) {
            diagnostic->line = 1u;
            diagnostic->column = 1u;
            (void)snprintf(diagnostic->message, sizeof(diagnostic->message),
                           "internal RSH1 validation failed (%d)", validation);
        }
        return AQC_ERROR_INTERNAL;
    }
    *output_size = total_size;
    return AQC_OK;
}
