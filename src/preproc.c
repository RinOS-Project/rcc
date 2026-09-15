/*
 * RCC - RinOS C Compiler
 * Preprocessor Implementation
 */

#include "preproc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Output buffer for preprocessed source */
typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} PPBuffer;

static void buf_init(PPBuffer* buf) {
    buf->capacity = 4096;
    buf->data = rcc_alloc(buf->capacity);
    buf->size = 0;
}

static void buf_append(PPBuffer* buf, const char* str, size_t len) {
    if (buf->size + len + 1 > buf->capacity) {
        while (buf->size + len + 1 > buf->capacity) {
            buf->capacity *= 2;
        }
        buf->data = rcc_realloc(buf->data, buf->capacity);
    }
    memcpy(buf->data + buf->size, str, len);
    buf->size += len;
    buf->data[buf->size] = '\0';
}

static void buf_append_char(PPBuffer* buf, char c) {
    buf_append(buf, &c, 1);
}

static void buf_append_str(PPBuffer* buf, const char* str) {
    buf_append(buf, str, strlen(str));
}

static const char* buf_append_quoted_token(PPBuffer* buffer,
                                           const char* input) {
    char quote = *input;
    buf_append_char(buffer, *input++);
    while (*input) {
        char current = *input++;
        buf_append_char(buffer, current);
        if (current == '\\' && *input) {
            buf_append_char(buffer, *input++);
        } else if (current == quote) {
            break;
        }
    }
    return input;
}

/* Create new preprocessor */
Preprocessor* pp_new(void) {
    Preprocessor* pp = rcc_alloc(sizeof(Preprocessor));
    pp->macros = NULL;
    pp->include_paths = NULL;
    pp->include_path_count = 0;
    pp->include_depth = 0;
    pp->cond_depth = 0;
    pp->expansion_depth = 0;
    pp->dependencies = NULL;
    pp->dependency_count = 0;
    pp->dependency_capacity = 0;

    /* Define built-in macros */
    pp_define(pp, "__RCC__", "1");
    pp_define(pp, "__RINOS__", "1");
    pp_define(pp, "__STDC__", "1");
    pp_define(pp, "__STDC_VERSION__", "201710L");
    pp_define(pp, "__ATOMIC_RELAXED", "0");
    pp_define(pp, "__ATOMIC_CONSUME", "1");
    pp_define(pp, "__ATOMIC_ACQUIRE", "2");
    pp_define(pp, "__ATOMIC_RELEASE", "3");
    pp_define(pp, "__ATOMIC_ACQ_REL", "4");
    pp_define(pp, "__ATOMIC_SEQ_CST", "5");

    /* Architecture */
    if (g_opts.target_arch == ARCH_X86) {
        pp_define(pp, "__i386__", "1");
        pp_define(pp, "__i386", "1");
    } else {
        pp_define(pp, "__x86_64__", "1");
        pp_define(pp, "__amd64__", "1");
    }

    return pp;
}

static void macro_release_storage(Macro* macro, bool release_name) {
    int parameter_count;
    if (!macro) return;
    parameter_count = macro->param_count > 0 ? macro->param_count : 0;
    if (release_name) rcc_free((void*)macro->name);
    rcc_free((void*)macro->value);
    for (int index = 0; index < parameter_count; ++index) {
        rcc_free((void*)macro->params[index]);
    }
    rcc_free(macro->params);
    rcc_free((void*)macro->body);
    macro->value = NULL;
    macro->params = NULL;
    macro->body = NULL;
}

void pp_free(Preprocessor* pp) {
    if (!pp) return;
    /* Free macros */
    Macro* m = pp->macros;
    while (m) {
        Macro* next = m->next;
        macro_release_storage(m, true);
        rcc_free(m);
        m = next;
    }

    /* Free include paths */
    if (pp->include_paths) {
        for (int i = 0; i < pp->include_path_count; i++) {
            rcc_free((void*)pp->include_paths[i]);
        }
        rcc_free(pp->include_paths);
    }
    for (int i = 0; i < pp->dependency_count; i++) {
        rcc_free((void*)pp->dependencies[i]);
    }
    rcc_free(pp->dependencies);

    rcc_free(pp);
}

void pp_add_include_path(Preprocessor* pp, const char* path) {
    pp->include_paths = rcc_realloc(pp->include_paths,
        sizeof(const char*) * (pp->include_path_count + 1));
    pp->include_paths[pp->include_path_count++] = rcc_strdup(path);
}

void pp_define(Preprocessor* pp, const char* name, const char* value) {
    /* Check if already defined */
    Macro* existing = pp_get_macro(pp, name);
    if (existing) {
        /* Redefine */
        macro_release_storage(existing, false);
        existing->value = rcc_strdup(value ? value : "");
        existing->param_count = -1;
        return;
    }

    Macro* m = rcc_alloc(sizeof(Macro));
    m->name = rcc_strdup(name);
    m->value = rcc_strdup(value ? value : "");
    m->params = NULL;
    m->param_count = -1;  /* Object-like macro */
    m->body = NULL;
    m->is_builtin = false;
    m->next = pp->macros;
    pp->macros = m;
}

void pp_define_func(Preprocessor* pp, const char* name, const char** params,
                    int param_count, const char* body) {
    Macro* m = pp_get_macro(pp, name);
    if (m) {
        macro_release_storage(m, false);
    } else {
        m = rcc_alloc(sizeof(Macro));
        m->name = rcc_strdup(name);
        m->next = pp->macros;
        pp->macros = m;
    }
    m->value = NULL;
    m->params = param_count > 0
        ? rcc_alloc(sizeof(const char*) * (size_t)param_count) : NULL;
    for (int i = 0; i < param_count; i++) {
        m->params[i] = rcc_strdup(params[i]);
    }
    m->param_count = param_count;
    m->body = rcc_strdup(body);
    m->is_builtin = false;
}

void pp_undef(Preprocessor* pp, const char* name) {
    Macro** mp = &pp->macros;
    while (*mp) {
        if (strcmp((*mp)->name, name) == 0) {
            Macro* m = *mp;
            *mp = m->next;
            macro_release_storage(m, true);
            rcc_free(m);
            return;
        }
        mp = &(*mp)->next;
    }
}

bool pp_is_defined(Preprocessor* pp, const char* name) {
    return pp_get_macro(pp, name) != NULL;
}

Macro* pp_get_macro(Preprocessor* pp, const char* name) {
    for (Macro* m = pp->macros; m; m = m->next) {
        if (strcmp(m->name, name) == 0) {
            return m;
        }
    }
    return NULL;
}

/* Skip whitespace (not newlines) */
static const char* skip_ws(const char* p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/* Skip to end of line */
static const char* skip_to_eol(const char* p) {
    while (*p && *p != '\n') p++;
    return p;
}

/* Read identifier */
static const char* read_ident(const char* p, char* buf, size_t buf_size) {
    size_t i = 0;
    while (isalnum((unsigned char)*p) || *p == '_') {
        if (i + 1u >= buf_size) {
            rcc_fatal("preprocessor identifier is too long");
        }
        buf[i++] = *p++;
    }
    buf[i] = '\0';
    return p;
}

/* Read string literal (for #include) */
static const char* read_string(const char* p, char* buf, size_t buf_size,
                               char delim, bool* closed) {
    size_t i = 0;
    if (closed) *closed = false;
    p++; /* Skip opening delimiter */
    while (*p && *p != delim && *p != '\n') {
        if (i + 1u >= buf_size) {
            rcc_fatal("preprocessor string literal is too long");
        }
        buf[i++] = *p++;
    }
    buf[i] = '\0';
    if (*p == delim) {
        if (closed) *closed = true;
        p++;
    }
    return p;
}

/* Read entire file into memory */
static char* read_file(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        rcc_fatal("cannot seek include file '%s'", filename);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        rcc_fatal("cannot determine include file size '%s'", filename);
        return NULL;
    }

    char* buf = rcc_alloc((size_t)size + 1u);
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        rcc_free(buf);
        fclose(f);
        rcc_fatal("cannot read include file '%s'", filename);
        return NULL;
    }
    buf[size] = '\0';
    fclose(f);
    return buf;
}

/* Find include file */
static char* find_include(Preprocessor* pp, const char* name, const char* current_file,
                          bool is_system, char* resolved, size_t resolved_size) {
    char path[RCC_MAX_PATH];

    if (!resolved || resolved_size == 0u) {
        rcc_fatal("preprocessor include path has no output buffer");
    }

#define WRITE_RESOLVED_PATH(value)                                                \
    do {                                                                           \
        int path_written__ = snprintf(resolved, resolved_size, "%s", (value));   \
        if (path_written__ < 0 || (size_t)path_written__ >= resolved_size) {      \
            rcc_fatal("resolved include path is too long");                       \
        }                                                                          \
    } while (0)

    /* For quoted includes, first search relative to current file */
    if (!is_system && current_file) {
        const char* last_sep = strrchr(current_file, '/');
        if (!last_sep) last_sep = strrchr(current_file, '\\');
        if (last_sep) {
            size_t dir_len = last_sep - current_file + 1;
            size_t name_len = strlen(name);
            if (dir_len >= sizeof(path) ||
                name_len > sizeof(path) - dir_len - 1u) {
                rcc_fatal("relative include path is too long");
            }
            memcpy(path, current_file, dir_len);
            memcpy(path + dir_len, name, name_len + 1u);
            char* content = read_file(path);
            if (content) {
                WRITE_RESOLVED_PATH(path);
                return content;
            }
        }
    }

    /* Search include paths */
    for (int i = 0; i < pp->include_path_count; i++) {
        size_t dir_len = strlen(pp->include_paths[i]);
        size_t name_len = strlen(name);
        if (dir_len >= sizeof(path) - 1u ||
            name_len > sizeof(path) - dir_len - 2u) {
            rcc_fatal("include search path is too long");
        }
        memcpy(path, pp->include_paths[i], dir_len);
        path[dir_len] = '/';
        memcpy(path + dir_len + 1u, name, name_len + 1u);
        char* content = read_file(path);
        if (content) {
            WRITE_RESOLVED_PATH(path);
            return content;
        }
    }

    /* Try current directory */
    if (strlen(name) >= sizeof(path)) {
        rcc_fatal("include path is too long");
    }
    char* content = read_file(name);
    if (content) {
        WRITE_RESOLVED_PATH(name);
        return content;
    }

#undef WRITE_RESOLVED_PATH
    return NULL;
}

static void pp_add_dependency(Preprocessor* pp, const char* path) {
    for (int i = 0; i < pp->dependency_count; i++) {
        if (strcmp(pp->dependencies[i], path) == 0) return;
    }
    if (pp->dependency_count == pp->dependency_capacity) {
        int capacity = pp->dependency_capacity ? pp->dependency_capacity * 2 : 16;
        pp->dependencies = rcc_realloc(
            pp->dependencies, sizeof(const char*) * (size_t)capacity);
        pp->dependency_capacity = capacity;
    }
    pp->dependencies[pp->dependency_count++] = rcc_strdup(path);
}

static const char* pp_expr_skip(const char* p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

/* Expand macros in a string (declared here because #if needs the same
 * replacement-list machinery as ordinary source lines). */
static char* expand_macros(Preprocessor* pp, const char* input);

typedef struct {
    uint64_t bits;
    bool is_unsigned;
} PPExprValue;

typedef struct {
    const char* input;
    bool invalid;
} PPExprParser;

static PPExprValue pp_expr_signed(int64_t value) {
    PPExprValue result = {(uint64_t)value, false};
    return result;
}

static PPExprValue pp_expr_unsigned(uint64_t value) {
    PPExprValue result = {value, true};
    return result;
}

static PPExprValue pp_expr_bool(bool value) {
    return pp_expr_signed(value ? 1 : 0);
}

static bool pp_expr_truth(PPExprValue value) {
    return value.bits != 0u;
}

static int64_t pp_expr_as_signed(PPExprValue value) {
    return (int64_t)value.bits;
}

static PPExprValue pp_expr_arithmetic(PPExprValue left, PPExprValue right,
                                      char operation, PPExprParser* parser) {
    uint64_t result;
    if (operation == '/' || operation == '%') {
        if (right.bits == 0u) {
            parser->invalid = true;
            return pp_expr_signed(0);
        }
        if (!left.is_unsigned && !right.is_unsigned &&
            pp_expr_as_signed(left) == INT64_MIN &&
            pp_expr_as_signed(right) == -1) {
            return pp_expr_signed(INT64_MIN);
        }
    }

    if (left.is_unsigned || right.is_unsigned) {
        switch (operation) {
        case '+': result = left.bits + right.bits; break;
        case '-': result = left.bits - right.bits; break;
        case '*': result = left.bits * right.bits; break;
        case '/': result = left.bits / right.bits; break;
        default: result = left.bits % right.bits; break;
        }
        return pp_expr_unsigned(result);
    }

    int64_t signed_left = pp_expr_as_signed(left);
    int64_t signed_right = pp_expr_as_signed(right);
    switch (operation) {
    case '+': result = left.bits + right.bits; break;
    case '-': result = left.bits - right.bits; break;
    case '*': result = left.bits * right.bits; break;
    case '/': return pp_expr_signed(signed_left / signed_right);
    default: return pp_expr_signed(signed_left % signed_right);
    }
    return pp_expr_signed((int64_t)result);
}

static bool pp_expr_match(PPExprParser* parser, const char* spelling) {
    const char* p = pp_expr_skip(parser->input);
    size_t length = strlen(spelling);
    if (strncmp(p, spelling, length) != 0) return false;
    parser->input = p + length;
    return true;
}

static bool pp_expr_starts_with(const PPExprParser* parser,
                                const char* spelling) {
    const char* p = pp_expr_skip(parser->input);
    return strncmp(p, spelling, strlen(spelling)) == 0;
}

static PPExprValue pp_expr_conditional(PPExprParser* parser);

static uint64_t pp_expr_escape(const char** input, PPExprParser* parser) {
    const char* p = *input;
    unsigned int value = 0;
    if (*p == 'a') return (*input = p + 1), '\a';
    if (*p == 'b') return (*input = p + 1), '\b';
    if (*p == 'f') return (*input = p + 1), '\f';
    if (*p == 'n') return (*input = p + 1), '\n';
    if (*p == 'r') return (*input = p + 1), '\r';
    if (*p == 't') return (*input = p + 1), '\t';
    if (*p == 'v') return (*input = p + 1), '\v';
    if (*p == 'x') {
        p++;
        while (isxdigit((unsigned char)*p)) {
            value = value * 16u + (unsigned int)(isdigit((unsigned char)*p)
                ? *p - '0' : tolower((unsigned char)*p) - 'a' + 10);
            p++;
        }
        *input = p;
        return value;
    }
    if (*p >= '0' && *p <= '7') {
        int count = 0;
        while (count < 3 && *p >= '0' && *p <= '7') {
            value = value * 8u + (unsigned int)(*p++ - '0');
            count++;
        }
        *input = p;
        return value;
    }
    if (*p == '\0') {
        parser->invalid = true;
        return 0;
    }
    *input = p + 1;
    return (unsigned char)*p;
}

static PPExprValue pp_expr_primary(PPExprParser* parser) {
    const char* p = pp_expr_skip(parser->input);
    if (*p == '(') {
        parser->input = p + 1;
        PPExprValue value = pp_expr_conditional(parser);
        p = pp_expr_skip(parser->input);
        if (*p != ')') {
            parser->invalid = true;
            return value;
        }
        parser->input = p + 1;
        return value;
    }
    if (*p == '\'') {
        uint64_t value = 0;
        p++;
        while (*p && *p != '\'') {
            uint64_t character;
            if (*p == '\\') {
                p++;
                character = pp_expr_escape(&p, parser);
            } else {
                character = (uint64_t)(unsigned char)*p++;
            }
            value = (value << 8) | (character & 0xffu);
        }
        if (*p != '\'') {
            parser->invalid = true;
        } else {
            p++;
        }
        parser->input = p;
        return pp_expr_signed((int64_t)value);
    }
    if (isdigit((unsigned char)*p)) {
        const char* start = p;
        char* end = NULL;
        uint64_t value;
        bool is_unsigned = false;
        if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B')) {
            p += 2;
            if (*p != '0' && *p != '1') {
                parser->invalid = true;
                return pp_expr_signed(0);
            }
            value = 0u;
            while (*p == '0' || *p == '1') value = value * 2u + (uint64_t)(*p++ - '0');
            end = (char*)p;
        } else {
            value = strtoull(start, &end, 0);
            if (end == start) {
                parser->invalid = true;
                return pp_expr_signed(0);
            }
            p = end;
        }
        while (*p == 'u' || *p == 'U' || *p == 'l' || *p == 'L') {
            if (*p == 'u' || *p == 'U') is_unsigned = true;
            p++;
        }
        parser->input = p;
        if (is_unsigned || value > (uint64_t)INT64_MAX) return pp_expr_unsigned(value);
        return pp_expr_signed((int64_t)value);
    }
    if (isalpha((unsigned char)*p) || *p == '_') {
        char name[256];
        p = read_ident(p, name, sizeof(name));
        parser->input = p;
        /* Macro expansion turns unresolved identifiers into zero per C17. */
        return pp_expr_signed(0);
    }
    parser->invalid = true;
    parser->input = *p ? p + 1 : p;
    return pp_expr_signed(0);
}

static PPExprValue pp_expr_unary(PPExprParser* parser) {
    if (pp_expr_match(parser, "!")) return pp_expr_bool(!pp_expr_truth(pp_expr_unary(parser)));
    if (pp_expr_match(parser, "~")) {
        PPExprValue value = pp_expr_unary(parser);
        value.bits = ~value.bits;
        return value;
    }
    if (pp_expr_match(parser, "+")) return pp_expr_unary(parser);
    if (pp_expr_match(parser, "-")) {
        PPExprValue value = pp_expr_unary(parser);
        value.bits = 0u - value.bits;
        return value;
    }
    return pp_expr_primary(parser);
}

static PPExprValue pp_expr_multiplicative(PPExprParser* parser) {
    PPExprValue value = pp_expr_unary(parser);
    for (;;) {
        char operation = 0;
        if (pp_expr_match(parser, "*")) operation = '*';
        else if (pp_expr_match(parser, "/")) operation = '/';
        else if (pp_expr_match(parser, "%")) operation = '%';
        else break;
        value = pp_expr_arithmetic(value, pp_expr_unary(parser), operation, parser);
    }
    return value;
}

static PPExprValue pp_expr_additive(PPExprParser* parser) {
    PPExprValue value = pp_expr_multiplicative(parser);
    for (;;) {
        char operation = 0;
        if (pp_expr_match(parser, "+")) operation = '+';
        else if (pp_expr_match(parser, "-")) operation = '-';
        else break;
        value = pp_expr_arithmetic(value, pp_expr_multiplicative(parser), operation, parser);
    }
    return value;
}

static PPExprValue pp_expr_shift(PPExprParser* parser) {
    PPExprValue value = pp_expr_additive(parser);
    for (;;) {
        bool left = pp_expr_match(parser, "<<");
        bool right = !left && pp_expr_match(parser, ">>");
        if (!left && !right) break;
        PPExprValue amount = pp_expr_additive(parser);
        if (amount.bits >= 64u) {
            parser->invalid = true;
            value = pp_expr_signed(0);
        } else if (left) {
            value.bits <<= amount.bits;
        } else if (value.is_unsigned || (value.bits & (1ull << 63)) == 0u) {
            value.bits >>= amount.bits;
        } else if (amount.bits != 0u) {
            value.bits = (value.bits >> amount.bits) | (~0ull << (64u - amount.bits));
        }
    }
    return value;
}

static PPExprValue pp_expr_relational(PPExprParser* parser) {
    PPExprValue value = pp_expr_shift(parser);
    for (;;) {
        int operation = 0;
        if (pp_expr_match(parser, "<=")) operation = 1;
        else if (pp_expr_match(parser, ">=")) operation = 2;
        else if (pp_expr_match(parser, "<")) operation = 3;
        else if (pp_expr_match(parser, ">")) operation = 4;
        else break;
        PPExprValue right = pp_expr_shift(parser);
        bool result;
        if (value.is_unsigned || right.is_unsigned) {
            result = operation == 1 ? value.bits <= right.bits
                : operation == 2 ? value.bits >= right.bits
                : operation == 3 ? value.bits < right.bits : value.bits > right.bits;
        } else {
            int64_t left_signed = pp_expr_as_signed(value);
            int64_t right_signed = pp_expr_as_signed(right);
            result = operation == 1 ? left_signed <= right_signed
                : operation == 2 ? left_signed >= right_signed
                : operation == 3 ? left_signed < right_signed : left_signed > right_signed;
        }
        value = pp_expr_bool(result);
    }
    return value;
}

static PPExprValue pp_expr_equality(PPExprParser* parser) {
    PPExprValue value = pp_expr_relational(parser);
    for (;;) {
        bool equal = pp_expr_match(parser, "==");
        bool not_equal = !equal && pp_expr_match(parser, "!=");
        if (!equal && !not_equal) break;
        PPExprValue right = pp_expr_relational(parser);
        bool result = value.is_unsigned || right.is_unsigned
            ? value.bits == right.bits
            : pp_expr_as_signed(value) == pp_expr_as_signed(right);
        value = pp_expr_bool(not_equal ? !result : result);
    }
    return value;
}

static PPExprValue pp_expr_bitand(PPExprParser* parser) {
    PPExprValue value = pp_expr_equality(parser);
    while (!pp_expr_starts_with(parser, "&&") && pp_expr_match(parser, "&")) {
        PPExprValue right = pp_expr_equality(parser);
        value.bits &= right.bits;
        value.is_unsigned = value.is_unsigned || right.is_unsigned;
    }
    return value;
}

static PPExprValue pp_expr_bitxor(PPExprParser* parser) {
    PPExprValue value = pp_expr_bitand(parser);
    while (pp_expr_match(parser, "^")) {
        PPExprValue right = pp_expr_bitand(parser);
        value.bits ^= right.bits;
        value.is_unsigned = value.is_unsigned || right.is_unsigned;
    }
    return value;
}

static PPExprValue pp_expr_bitor(PPExprParser* parser) {
    PPExprValue value = pp_expr_bitxor(parser);
    while (!pp_expr_starts_with(parser, "||") && pp_expr_match(parser, "|")) {
        PPExprValue right = pp_expr_bitxor(parser);
        value.bits |= right.bits;
        value.is_unsigned = value.is_unsigned || right.is_unsigned;
    }
    return value;
}

static PPExprValue pp_expr_logical_and(PPExprParser* parser) {
    PPExprValue value = pp_expr_bitor(parser);
    while (pp_expr_match(parser, "&&")) {
        PPExprValue right = pp_expr_bitor(parser);
        value = pp_expr_bool(pp_expr_truth(value) && pp_expr_truth(right));
    }
    return value;
}

static PPExprValue pp_expr_logical_or(PPExprParser* parser) {
    PPExprValue value = pp_expr_logical_and(parser);
    while (pp_expr_match(parser, "||")) {
        PPExprValue right = pp_expr_logical_and(parser);
        value = pp_expr_bool(pp_expr_truth(value) || pp_expr_truth(right));
    }
    return value;
}

static PPExprValue pp_expr_conditional(PPExprParser* parser) {
    PPExprValue condition = pp_expr_logical_or(parser);
    if (!pp_expr_match(parser, "?")) return condition;
    PPExprValue when_true = pp_expr_conditional(parser);
    if (!pp_expr_match(parser, ":")) {
        parser->invalid = true;
        return when_true;
    }
    PPExprValue when_false = pp_expr_conditional(parser);
    return pp_expr_truth(condition) ? when_true : when_false;
}

static char* pp_prepare_if_expression(Preprocessor* pp, const char* expression,
                                      bool* valid) {
    PPBuffer protected;
    buf_init(&protected);
    const char* p = expression;
    *valid = true;
    while (*p && *p != '\n') {
        if (*p == '"' || *p == '\'') {
            p = buf_append_quoted_token(&protected, p);
            continue;
        }
        if (isalpha((unsigned char)*p) || *p == '_') {
            char ident[256];
            const char* end = read_ident(p, ident, sizeof(ident));
            if (strcmp(ident, "defined") == 0) {
                const char* operand = pp_expr_skip(end);
                bool parenthesized = *operand == '(';
                if (parenthesized) operand = pp_expr_skip(operand + 1);
                char name[256];
                const char* name_end = read_ident(operand, name, sizeof(name));
                if (name_end == operand ||
                    (parenthesized && *pp_expr_skip(name_end) != ')')) {
                    *valid = false;
                    buf_append_str(&protected, "0");
                    p = end;
                    continue;
                }
                buf_append_str(&protected, pp_is_defined(pp, name) ? "1" : "0");
                p = parenthesized ? pp_expr_skip(name_end) + 1 : name_end;
                continue;
            }
            buf_append(&protected, p, (size_t)(end - p));
            p = end;
            continue;
        }
        buf_append_char(&protected, *p++);
    }
    char* expanded = expand_macros(pp, protected.data);
    rcc_free(protected.data);
    return expanded;
}

static bool pp_eval_expression(Preprocessor* pp, const char* expression,
                               bool* valid) {
    char* expanded = pp_prepare_if_expression(pp, expression, valid);
    PPExprParser parser = {expanded, !*valid};
    PPExprValue value = pp_expr_conditional(&parser);
    parser.input = pp_expr_skip(parser.input);
    if (*parser.input && *parser.input != '\r' && *parser.input != '\n') {
        parser.invalid = true;
    }
    *valid = !parser.invalid;
    rcc_free(expanded);
    return *valid && pp_expr_truth(value);
}

/* C17 translation phase 2 removes every backslash-newline pair before
 * directives, comments, and tokens are interpreted.  Doing this once for the
 * complete file also makes continued #if expressions follow the same rules as
 * continued macro definitions and ordinary source lines. */
static char* splice_source_lines(const char* source) {
    size_t input_length = strlen(source);
    char* spliced = rcc_alloc(input_length + 1u);
    size_t input = 0;
    size_t output = 0;

    while (input < input_length) {
        if (source[input] == '\\' && source[input + 1u] == '\n') {
            input += 2u;
            continue;
        }
        if (source[input] == '\\' && source[input + 1u] == '\r' &&
            source[input + 2u] == '\n') {
            input += 3u;
            continue;
        }
        spliced[output++] = source[input++];
    }
    spliced[output] = '\0';
    return spliced;
}

/* C17 translation phase 3 replaces comments with whitespace before
 * directives and macro replacement are interpreted.  Preserve newlines and
 * columns so diagnostics and directive boundaries remain stable. */
static char* strip_source_comments(const char* source) {
    size_t length = strlen(source);
    char* stripped = rcc_alloc(length + 1u);
    size_t input = 0;
    size_t output = 0;
    char quoted = '\0';

    while (input < length) {
        char current = source[input];
        if (quoted != '\0') {
            stripped[output++] = current;
            input++;
            if (current == '\\' && input < length) {
                stripped[output++] = source[input++];
            } else if (current == quoted) {
                quoted = '\0';
            }
            continue;
        }
        if (current == '"' || current == '\'') {
            quoted = current;
            stripped[output++] = current;
            input++;
            continue;
        }
        if (current == '/' && input + 1u < length &&
            source[input + 1u] == '/') {
            stripped[output++] = ' ';
            stripped[output++] = ' ';
            input += 2u;
            while (input < length && source[input] != '\n') {
                stripped[output++] = ' ';
                input++;
            }
            continue;
        }
        if (current == '/' && input + 1u < length &&
            source[input + 1u] == '*') {
            stripped[output++] = ' ';
            stripped[output++] = ' ';
            input += 2u;
            while (input < length) {
                if (source[input] == '*' && input + 1u < length &&
                    source[input + 1u] == '/') {
                    stripped[output++] = ' ';
                    stripped[output++] = ' ';
                    input += 2u;
                    break;
                }
                stripped[output++] = source[input] == '\n' ? '\n' : ' ';
                input++;
            }
            continue;
        }
        stripped[output++] = current;
        input++;
    }
    stripped[output] = '\0';
    return stripped;
}

static const char* read_macro_body(const char* p, char* body, size_t body_size) {
    size_t used = 0;
    for (;;) {
        const char* eol = skip_to_eol(p);
        const char* logical_end = eol;
        if (logical_end > p && logical_end[-1] == '\r') logical_end--;
        bool continued = logical_end > p && logical_end[-1] == '\\';
        if (continued) logical_end--;
        size_t part = (size_t)(logical_end - p);
        if (used >= body_size || part > body_size - used - 1u) {
            rcc_fatal("preprocessor macro replacement list is too long");
        }
        if (part != 0u) memcpy(body + used, p, part);
        used += part;
        if (!continued || *eol == '\0') {
            body[used] = '\0';
            return eol;
        }
        if (used + 1u >= body_size) {
            rcc_fatal("preprocessor macro replacement list is too long");
        }
        body[used++] = ' ';
        p = eol + 1;
    }
}

/* Check if in active preprocessing region */
static bool pp_is_active(Preprocessor* pp) {
    if (pp->cond_depth == 0) return true;
    return pp->cond_stack[pp->cond_depth - 1].active;
}

/* Expand macros in a string */
static char* expand_macros(Preprocessor* pp, const char* input);
static char* expand_macro(Preprocessor* pp, Macro* macro,
                          const char** args, int arg_count);

static bool macro_is_expanding(const Preprocessor* pp, const Macro* macro) {
    for (int i = 0; i < pp->expansion_depth; i++) {
        if (pp->expanding[i] == macro) return true;
    }
    return false;
}

static char* expand_macro_recursive(Preprocessor* pp, Macro* macro,
                                    const char** args, int arg_count) {
    char* initial = expand_macro(pp, macro, args, arg_count);
    if (pp->expansion_depth >= (int)(sizeof(pp->expanding) / sizeof(pp->expanding[0]))) {
        return initial;
    }
    pp->expanding[pp->expansion_depth++] = macro;
    char* expanded = expand_macros(pp, initial);
    pp->expansion_depth--;
    rcc_free(initial);
    return expanded;
}

static const char* skip_macro_space(const char* p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static int macro_param_index(const Macro* macro, const char* name) {
    for (int i = 0; i < macro->param_count; i++) {
        if (strcmp(name, macro->params[i]) == 0) return i;
    }
    return -1;
}

static void buf_trim_macro_space(PPBuffer* buffer) {
    while (buffer->size != 0u &&
           isspace((unsigned char)buffer->data[buffer->size - 1u])) {
        buffer->data[--buffer->size] = '\0';
    }
}

static void buf_append_trimmed_macro_argument(PPBuffer* result,
                                              const char* argument) {
    const char* start = argument;
    while (*start && isspace((unsigned char)*start)) start++;
    const char* end = start + strlen(start);
    while (end != start && isspace((unsigned char)end[-1])) end--;
    buf_append(result, start, (size_t)(end - start));
}

static void buf_append_macro_argument(PPBuffer* result, Preprocessor* pp,
                                      const Macro* macro, const char** args,
                                      int arg_count, int parameter,
                                      bool expand) {
    if (parameter >= arg_count) return;

    int last = parameter;
    bool variadic = strcmp(macro->params[parameter], "__VA_ARGS__") == 0;
    if (variadic) last = arg_count - 1;

    for (int argument = parameter; argument <= last; argument++) {
        if (argument != parameter) buf_append_char(result, ',');
        if (expand) {
            char* expanded = expand_macros(pp, args[argument]);
            buf_append_str(result, expanded);
            rcc_free(expanded);
        } else {
            buf_append_trimmed_macro_argument(result, args[argument]);
        }
    }
}

static void pp_copy_macro_argument(char* destination, size_t capacity,
                                   const char* start, size_t length) {
    if (length >= capacity) {
        rcc_fatal("preprocessor macro argument is too long");
    }
    memcpy(destination, start, length);
    destination[length] = '\0';
}

static void buf_append_macro_raw_spelling(PPBuffer* result, const Macro* macro,
                                          const char** args, int arg_count,
                                          int parameter) {
    if (parameter >= arg_count) return;
    int last = strcmp(macro->params[parameter], "__VA_ARGS__") == 0
        ? arg_count - 1 : parameter;
    for (int argument = parameter; argument <= last; argument++) {
        if (argument != parameter) buf_append_char(result, ',');
        buf_append_str(result, args[argument]);
    }
}

static void buf_append_stringized_argument(PPBuffer* result,
                                           const char* argument) {
    bool pending_space = false;
    bool emitted = false;
    buf_append_char(result, '"');

    for (const char* p = argument; *p; p++) {
        unsigned char current = (unsigned char)*p;
        if (isspace(current)) {
            if (emitted) pending_space = true;
            continue;
        }
        if (pending_space) {
            buf_append_char(result, ' ');
            pending_space = false;
        }
        if (*p == '\\' || *p == '"') buf_append_char(result, '\\');
        buf_append_char(result, *p);
        emitted = true;
    }
    buf_append_char(result, '"');
}

static const char* macro_item_end(const char* p) {
    if (*p == '"' || *p == '\'') {
        char quote = *p++;
        while (*p) {
            if (*p == '\\' && p[1]) {
                p += 2;
                continue;
            }
            if (*p++ == quote) break;
        }
        return p;
    }
    if (isalpha((unsigned char)*p) || *p == '_') {
        char ident[256];
        return read_ident(p, ident, sizeof(ident));
    }
    return p + (*p ? 1 : 0);
}

/* Expand a single macro invocation */
static char* expand_macro(Preprocessor* pp, Macro* macro, const char** args, int arg_count) {
    if (macro->param_count < 0) {
        /* Object-like macro */
        return rcc_strdup(macro->value);
    }

    /*
     * Function-like macro substitution.  The old implementation treated the
     * replacement list as a plain string, which made the two C preprocessing
     * replacement operators impossible to implement correctly.  Keep the
     * source spelling here so # sees the raw argument and ## can paste raw
     * tokens, while ordinary parameters receive their required prescan.
     */
    PPBuffer result;
    buf_init(&result);

    const char* p = macro->body;
    bool paste_pending = false;
    while (*p) {
        if (isspace((unsigned char)*p)) {
            const char* end = skip_macro_space(p);
            if (!paste_pending) buf_append(&result, p, (size_t)(end - p));
            p = end;
            continue;
        }

        if (*p == '#' && p[1] == '#') {
            buf_trim_macro_space(&result);
            paste_pending = true;
            p += 2;
            continue;
        }

        if (*p == '#') {
            const char* parameter_start = skip_macro_space(p + 1);
            if (isalpha((unsigned char)*parameter_start) ||
                *parameter_start == '_') {
                char ident[256];
                const char* end = read_ident(parameter_start, ident,
                                             sizeof(ident));
                int parameter = macro_param_index(macro, ident);
                if (parameter >= 0) {
                    if (strcmp(macro->params[parameter], "__VA_ARGS__") == 0) {
                        PPBuffer raw_arguments;
                        buf_init(&raw_arguments);
                        buf_append_macro_raw_spelling(&raw_arguments, macro,
                                                       args, arg_count,
                                                       parameter);
                        buf_append_stringized_argument(&result,
                                                       raw_arguments.data);
                        rcc_free(raw_arguments.data);
                    } else {
                        buf_append_stringized_argument(&result,
                                                       parameter < arg_count
                                                           ? args[parameter]
                                                           : "");
                    }
                    p = end;
                    paste_pending = false;
                    continue;
                }
            }
            /* Keep malformed/non-parameter '#' text visible for the parser. */
            buf_append_char(&result, *p++);
            paste_pending = false;
            continue;
        }

        const char* end = p;
        bool paste_after = false;
        if (*p == '"' || *p == '\'') {
            end = macro_item_end(p);
            buf_append(&result, p, (size_t)(end - p));
            p = end;
        } else if (isalpha((unsigned char)*p) || *p == '_') {
            char ident[256];
            end = read_ident(p, ident, sizeof(ident));

            if (strcmp(ident, "__VA_OPT__") == 0) {
                const char* open = skip_macro_space(end);
                const char* content_start;
                const char* content_end;
                int depth = 0;
                char quote = '\0';
                int variadic = macro_param_index(macro, "__VA_ARGS__");
                bool present = false;
                if (*open != '(') {
                    buf_append(&result, p, (size_t)(end - p));
                    p = end;
                    paste_pending = false;
                    continue;
                }
                content_start = open + 1;
                content_end = content_start;
                for (const char* cursor = content_start; *cursor; cursor++) {
                    if (quote != '\0') {
                        if (*cursor == '\\' && cursor[1]) {
                            cursor++;
                        } else if (*cursor == quote) {
                            quote = '\0';
                        }
                        continue;
                    }
                    if (*cursor == '"' || *cursor == '\'') {
                        quote = *cursor;
                    } else if (*cursor == '(') {
                        depth++;
                    } else if (*cursor == ')') {
                        if (depth == 0) {
                            content_end = cursor;
                            break;
                        }
                        depth--;
                    }
                }
                if (*content_end != ')') {
                    /* Leave malformed C++20 syntax for the parser to reject. */
                    buf_append(&result, p, (size_t)(end - p));
                    p = end;
                    paste_pending = false;
                    continue;
                }
                if (variadic >= 0 && variadic < arg_count) {
                    const char* argument = args[variadic];
                    while (*argument && isspace((unsigned char)*argument)) {
                        argument++;
                    }
                    present = *argument != '\0';
                }
                if (present) {
                    size_t length = (size_t)(content_end - content_start);
                    char* body = rcc_alloc(length + 1u);
                    Macro nested = *macro;
                    memcpy(body, content_start, length);
                    body[length] = '\0';
                    nested.body = body;
                    char* expanded = expand_macro(pp, &nested, args,
                                                  arg_count);
                    buf_append_str(&result, expanded);
                    rcc_free(expanded);
                    rcc_free(body);
                }
                p = content_end + 1;
                paste_pending = false;
                continue;
            }

            /* Check if it's a parameter */
            int parameter = macro_param_index(macro, ident);
            const char* paste = skip_macro_space(end);
            paste_after = paste[0] == '#' && paste[1] == '#';
            if (parameter >= 0) {
                /* A parameter next to # or ## is substituted without prescan. */
                buf_append_macro_argument(&result, pp, macro, args, arg_count,
                                          parameter, !paste_pending &&
                                                         !paste_after);
            } else {
                buf_append(&result, p, end - p);
            }
            p = end;
        } else {
            buf_append_char(&result, *p++);
            end = p;
        }

        if (!paste_after) {
            const char* paste = skip_macro_space(end);
            paste_after = paste[0] == '#' && paste[1] == '#';
        }
        if (paste_after) {
            buf_trim_macro_space(&result);
            p = skip_macro_space(end) + 2;
            paste_pending = true;
        } else {
            paste_pending = false;
        }
    }

    return result.data;
}

/* Expand macros in input string */
static char* expand_macros(Preprocessor* pp, const char* input) {
    PPBuffer result;
    buf_init(&result);

    const char* p = input;
    while (*p) {
        if (*p == '"' || *p == '\'') {
            p = buf_append_quoted_token(&result, p);
        } else if (isalpha(*p) || *p == '_') {
            char ident[256];
            const char* end = read_ident(p, ident, sizeof(ident));

            Macro* macro = pp_get_macro(pp, ident);
            if (macro && !macro_is_expanding(pp, macro)) {
                if (macro->param_count >= 0) {
                    /* Function-like macro - need arguments */
                    const char* args_start = skip_ws(end);
                    if (*args_start == '(') {
                        args_start++;
                        const char* args[PP_MAX_PARAMS];
                        char arg_bufs[PP_MAX_PARAMS][1024];
                        int arg_count = 0;
                        int paren_depth = 1;
                        char quoted = '\0';

                        const char* arg_start = args_start;
                        while (*args_start && paren_depth > 0) {
                            if (quoted != '\0') {
                                if (*args_start == '\\' && args_start[1]) {
                                    args_start += 2;
                                    continue;
                                }
                                if (*args_start == quoted) quoted = '\0';
                            } else if (*args_start == '"' ||
                                       *args_start == '\'') {
                                quoted = *args_start;
                            } else if (*args_start == '(') paren_depth++;
                            else if (*args_start == ')') {
                                paren_depth--;
                                if (paren_depth == 0) {
                                    /* End of args */
                                    if (arg_count < PP_MAX_PARAMS) {
                                        size_t len = args_start - arg_start;
                                        pp_copy_macro_argument(
                                            arg_bufs[arg_count],
                                            sizeof(arg_bufs[arg_count]),
                                            arg_start, len);
                                        args[arg_count] = arg_bufs[arg_count];
                                        arg_count++;
                                    }
                                }
                            } else if (*args_start == ',' && paren_depth == 1) {
                                /* Argument separator */
                                if (arg_count < PP_MAX_PARAMS) {
                                    size_t len = args_start - arg_start;
                                    pp_copy_macro_argument(
                                        arg_bufs[arg_count],
                                        sizeof(arg_bufs[arg_count]),
                                        arg_start, len);
                                    args[arg_count] = arg_bufs[arg_count];
                                    arg_count++;
                                }
                                arg_start = args_start + 1;
                            }
                            args_start++;
                        }

                        char* expanded = expand_macro_recursive(pp, macro, args, arg_count);
                        buf_append_str(&result, expanded);
                        rcc_free(expanded);
                        p = args_start;
                        continue;
                    }
                } else {
                    /* Object-like macro */
                    char* expanded = expand_macro_recursive(pp, macro, NULL, 0);
                    buf_append_str(&result, expanded);
                    rcc_free(expanded);
                    p = end;
                    continue;
                }
            }

            /* Not a macro, copy as-is */
            buf_append(&result, p, end - p);
            p = end;
        } else {
            buf_append_char(&result, *p++);
        }
    }

    return result.data;
}

/* Process a single directive */
static const char* process_directive(Preprocessor* pp, const char* p,
                                     const char* filename, int source_line,
                                     PPBuffer* output) {
    p = skip_ws(p + 1); /* Skip '#' and whitespace */

    char directive[64];
    p = read_ident(p, directive, sizeof(directive));
    p = skip_ws(p);

    if (strcmp(directive, "include") == 0) {
        if (!pp_is_active(pp)) {
            return skip_to_eol(p);
        }

        if (pp->include_depth >= PP_MAX_INCLUDE_DEPTH) {
            rcc_error((SourceLoc){filename, 0, 0}, "#include nested too deeply");
            return skip_to_eol(p);
        }

        bool is_system = (*p == '<');
        char inc_name[256];
        char delim = (*p == '<') ? '>' : '"';
        bool include_closed = false;
        p = read_string(p, inc_name, sizeof(inc_name), delim,
                        &include_closed);
        if (!include_closed) {
            rcc_error((SourceLoc){filename, source_line, 0},
                      "unterminated #include path");
            return skip_to_eol(p);
        }

        char resolved[RCC_MAX_PATH];
        char* content = find_include(pp, inc_name, filename, is_system,
                                     resolved, sizeof(resolved));
        if (!content) {
            rcc_error((SourceLoc){filename, 0, 0}, "cannot find include file: %s", inc_name);
            return skip_to_eol(p);
        }
        pp_add_dependency(pp, resolved);

        /* Process included file */
        pp->include_depth++;
        char* processed = pp_process_string(pp, content, resolved);
        pp->include_depth--;

        buf_append_str(output, processed);
        buf_append_char(output, '\n');
        {
            char return_line[256];
            int written = snprintf(return_line, sizeof(return_line),
                                   "#line %d \"%s\"\n",
                                   source_line + 1, filename);
            if (written < 0 || (size_t)written >= sizeof(return_line)) {
                rcc_fatal("#line include filename is too long");
            }
            buf_append_str(output, return_line);
        }

        rcc_free(content);
        rcc_free(processed);

        return skip_to_eol(p);
    }

    if (strcmp(directive, "define") == 0) {
        if (!pp_is_active(pp)) {
            return skip_to_eol(p);
        }

        char macro_name[256];
        p = read_ident(p, macro_name, sizeof(macro_name));

        if (*p == '(') {
            /* Function-like macro */
            p++; /* Skip '(' */
            const char* params[PP_MAX_PARAMS];
            char param_bufs[PP_MAX_PARAMS][64];
            int param_count = 0;

            while (*p && *p != ')') {
                p = skip_ws(p);
                if (*p == ')') break;

                if (p[0] == '.' && p[1] == '.' && p[2] == '.') {
                    if (param_count < PP_MAX_PARAMS) {
                        strcpy(param_bufs[param_count], "__VA_ARGS__");
                        params[param_count] = param_bufs[param_count];
                        param_count++;
                    }
                    p += 3;
                    p = skip_ws(p);
                    break;
                }

                char param[64];
                const char* before = p;
                p = read_ident(p, param, sizeof(param));
                if (param[0] && param_count < PP_MAX_PARAMS) {
                    strcpy(param_bufs[param_count], param);
                    params[param_count] = param_bufs[param_count];
                    param_count++;
                }

                p = skip_ws(p);
                if (*p == ',') p++;
                else if (p == before) {
                    rcc_error((SourceLoc){filename, 0, 0},
                              "invalid token in macro parameter list");
                    p++;
                }
            }
            if (*p == ')') p++;

            p = skip_ws(p);
            char body[4096];
            p = read_macro_body(p, body, sizeof(body));

            pp_define_func(pp, macro_name, params, param_count, body);
        } else {
            /* Object-like macro */
            p = skip_ws(p);
            char value[4096];
            p = read_macro_body(p, value, sizeof(value));
            size_t value_len = strlen(value);

            /* Trim trailing whitespace */
            while (value_len > 0 && (value[value_len-1] == ' ' || value[value_len-1] == '\t')) {
                value[--value_len] = '\0';
            }

            pp_define(pp, macro_name, value);
        }

        return p;
    }

    if (strcmp(directive, "undef") == 0) {
        if (!pp_is_active(pp)) {
            return skip_to_eol(p);
        }

        char name[256];
        p = read_ident(p, name, sizeof(name));
        pp_undef(pp, name);
        return skip_to_eol(p);
    }

    if (strcmp(directive, "ifdef") == 0) {
        char name[256];
        p = read_ident(p, name, sizeof(name));

        bool parent_active = pp_is_active(pp);
        bool cond = pp_is_defined(pp, name);

        if (pp->cond_depth >= (int)(sizeof(pp->cond_stack) / sizeof(pp->cond_stack[0]))) {
            rcc_error((SourceLoc){filename, 0, 0}, "conditional nesting too deep");
            return skip_to_eol(p);
        }
        pp->cond_stack[pp->cond_depth].active = parent_active && cond;
        pp->cond_stack[pp->cond_depth].had_true = cond;
        pp->cond_stack[pp->cond_depth].in_else = false;
        pp->cond_depth++;

        return skip_to_eol(p);
    }

    if (strcmp(directive, "ifndef") == 0) {
        char name[256];
        p = read_ident(p, name, sizeof(name));

        bool parent_active = pp_is_active(pp);
        bool cond = !pp_is_defined(pp, name);

        if (pp->cond_depth >= (int)(sizeof(pp->cond_stack) / sizeof(pp->cond_stack[0]))) {
            rcc_error((SourceLoc){filename, 0, 0}, "conditional nesting too deep");
            return skip_to_eol(p);
        }
        pp->cond_stack[pp->cond_depth].active = parent_active && cond;
        pp->cond_stack[pp->cond_depth].had_true = cond;
        pp->cond_stack[pp->cond_depth].in_else = false;
        pp->cond_depth++;

        return skip_to_eol(p);
    }

    if (strcmp(directive, "if") == 0) {
        bool parent_active = pp_is_active(pp);
        bool expression_valid = true;
        bool cond = pp_eval_expression(pp, p, &expression_valid);
        if (!expression_valid) {
            rcc_error((SourceLoc){filename, source_line, 0},
                      "invalid #if expression");
        }
        if (pp->cond_depth >= (int)(sizeof(pp->cond_stack) / sizeof(pp->cond_stack[0]))) {
            rcc_error((SourceLoc){filename, 0, 0}, "conditional nesting too deep");
            return skip_to_eol(p);
        }

        pp->cond_stack[pp->cond_depth].active = parent_active && cond;
        pp->cond_stack[pp->cond_depth].had_true = cond;
        pp->cond_stack[pp->cond_depth].in_else = false;
        pp->cond_depth++;

        return skip_to_eol(p);
    }

    if (strcmp(directive, "elif") == 0) {
        if (pp->cond_depth == 0) {
            rcc_error((SourceLoc){filename, 0, 0}, "#elif without #if");
            return skip_to_eol(p);
        }

        if (pp->cond_stack[pp->cond_depth - 1].in_else) {
            rcc_error((SourceLoc){filename, 0, 0}, "#elif after #else");
            return skip_to_eol(p);
        }

        bool parent_active = (pp->cond_depth > 1) ? pp->cond_stack[pp->cond_depth - 2].active : true;
        bool had_true = pp->cond_stack[pp->cond_depth - 1].had_true;

        bool expression_valid = true;
        bool cond = pp_eval_expression(pp, p, &expression_valid);
        if (!expression_valid) {
            rcc_error((SourceLoc){filename, source_line, 0},
                      "invalid #elif expression");
        }

        pp->cond_stack[pp->cond_depth - 1].active = parent_active && !had_true && cond;
        if (cond) {
            pp->cond_stack[pp->cond_depth - 1].had_true = true;
        }

        return skip_to_eol(p);
    }

    if (strcmp(directive, "else") == 0) {
        if (pp->cond_depth == 0) {
            rcc_error((SourceLoc){filename, 0, 0}, "#else without #if");
            return skip_to_eol(p);
        }

        if (pp->cond_stack[pp->cond_depth - 1].in_else) {
            rcc_error((SourceLoc){filename, 0, 0}, "duplicate #else");
            return skip_to_eol(p);
        }

        bool parent_active = (pp->cond_depth > 1) ? pp->cond_stack[pp->cond_depth - 2].active : true;
        bool had_true = pp->cond_stack[pp->cond_depth - 1].had_true;

        pp->cond_stack[pp->cond_depth - 1].active = parent_active && !had_true;
        pp->cond_stack[pp->cond_depth - 1].in_else = true;

        return skip_to_eol(p);
    }

    if (strcmp(directive, "endif") == 0) {
        if (pp->cond_depth == 0) {
            rcc_error((SourceLoc){filename, 0, 0}, "#endif without #if");
            return skip_to_eol(p);
        }
        pp->cond_depth--;
        return skip_to_eol(p);
    }

    if (strcmp(directive, "error") == 0) {
        if (pp_is_active(pp)) {
            const char* msg_start = p;
            p = skip_to_eol(p);
            size_t len = p - msg_start;
            char* msg = rcc_alloc(len + 1u);
            memcpy(msg, msg_start, len);
            msg[len] = '\0';
            rcc_error((SourceLoc){filename, 0, 0}, "#error %s", msg);
            rcc_free(msg);
        }
        return skip_to_eol(p);
    }

    if (strcmp(directive, "warning") == 0) {
        if (pp_is_active(pp)) {
            const char* msg_start = p;
            p = skip_to_eol(p);
            size_t len = p - msg_start;
            char* msg = rcc_alloc(len + 1u);
            memcpy(msg, msg_start, len);
            msg[len] = '\0';
            rcc_warning((SourceLoc){filename, 0, 0}, "#warning %s", msg);
            rcc_free(msg);
        }
        return skip_to_eol(p);
    }

    if (strcmp(directive, "pragma") == 0) {
        const char* end = skip_to_eol(p);
        if (pp_is_active(pp)) {
            /* Packing directives affect the layout of declarations that
             * follow them, so they must survive preprocessing.  The lexer
             * recognizes #pragma pack and continues to ignore other pragmas.
             */
            buf_append_str(output, "#pragma ");
            buf_append(output, p, (size_t)(end - p));
        }
        return end;
    }

    if (strcmp(directive, "line") == 0) {
        /* Ignore #line for now */
        return skip_to_eol(p);
    }

    /* Unknown directive */
    if (pp_is_active(pp)) {
        rcc_warning((SourceLoc){filename, 0, 0}, "unknown preprocessor directive: #%s", directive);
    }
    return skip_to_eol(p);
}

/* Process source string */
char* pp_process_string(Preprocessor* pp, const char* source, const char* filename) {
    PPBuffer output;
    buf_init(&output);

    char* spliced_source = splice_source_lines(source);
    char* comment_free_source = strip_source_comments(spliced_source);
    const char* p = comment_free_source;
    int line = 1;
    int initial_cond_depth = pp->cond_depth;

    /* Emit #line directive for debugging */
    char line_dir[256];
    int line_written = snprintf(line_dir, sizeof(line_dir),
                                "#line 1 \"%s\"\n", filename);
    if (line_written < 0 || (size_t)line_written >= sizeof(line_dir)) {
        rcc_fatal("#line filename is too long");
    }
    buf_append_str(&output, line_dir);

    while (*p) {
        /* Skip leading whitespace on line */
        const char* line_start = p;
        while (*p == ' ' || *p == '\t') p++;

        if (*p == '#') {
            /* Preprocessor directive */
            p = process_directive(pp, p, filename, line, &output);
            if (*p == '\n') {
                buf_append_char(&output, '\n');
                p++;
                line++;
            }
            continue;
        }

        if (!pp_is_active(pp)) {
            /* Skip inactive code */
            p = skip_to_eol(p);
            if (*p == '\n') {
                buf_append_char(&output, '\n');
                p++;
                line++;
            }
            continue;
        }

        /* Process normal line - expand macros */
        const char* eol = skip_to_eol(p);
        size_t line_len = eol - line_start;
        char* line_buf = rcc_alloc(line_len + 1);
        memcpy(line_buf, line_start, line_len);
        line_buf[line_len] = '\0';

        /* Expand macros */
        char* expanded = expand_macros(pp, line_buf);
        buf_append_str(&output, expanded);
        rcc_free(line_buf);
        rcc_free(expanded);

        p = eol;
        if (*p == '\n') {
            buf_append_char(&output, '\n');
            p++;
            line++;
        }
    }

    /* Check for unterminated conditionals */
    if (pp->cond_depth != initial_cond_depth) {
        rcc_error((SourceLoc){filename, line, 0}, "unterminated #if");
        pp->cond_depth = initial_cond_depth;
    }

    rcc_free(comment_free_source);
    rcc_free(spliced_source);
    return output.data;
}

/* Process file */
char* pp_process_file(Preprocessor* pp, const char* filename) {
    char* source = read_file(filename);
    if (!source) {
        rcc_error((SourceLoc){filename, 0, 0}, "cannot open file");
        return NULL;
    }

    char* result = pp_process_string(pp, source, filename);
    rcc_free(source);
    return result;
}

static void write_make_path(FILE* output, const char* path) {
    for (const char* p = path; *p; p++) {
        if (*p == ' ' || *p == '#' || *p == '$') fputc('\\', output);
        fputc(*p == '\\' ? '/' : *p, output);
    }
}

bool pp_write_dependencies(Preprocessor* pp, const char* target,
                           const char* source, const char* dependency_file) {
    if (!pp || !target || !source || !dependency_file) return false;
    FILE* output = fopen(dependency_file, "wb");
    if (!output) return false;
    write_make_path(output, target);
    fputs(": ", output);
    write_make_path(output, source);
    for (int i = 0; i < pp->dependency_count; i++) {
        fputs(" \\\n  ", output);
        write_make_path(output, pp->dependencies[i]);
    }
    fputc('\n', output);
    return fclose(output) == 0;
}
