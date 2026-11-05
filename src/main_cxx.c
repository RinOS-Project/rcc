/*
 * RCC++ - RinOS C++ Compiler
 * Main entry point for C++ compilation
 */

#include "rcc.h"
#include "token.h"
#include "ast.h"
#include "ast_cxx.h"
#include "symtab.h"
#include "codegen.h"
#include "driver_policy.h"
#include "optimize.h"
#include "preproc.h"
#include "build_manifest.h"
#include "verified_codegen.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <getopt.h>

/* C++ specific options */
static int g_cxx_standard = 20;  /* C++20 is the RinOS v3 language contract. */

#define RCC_CXX_MODULE_MAX_ARTIFACT (16u * 1024u * 1024u)
#define RCC_CXX_MODULE_MAX_TOKENS 1048576u
#define RCC_CXX_MODULE_MAX_IMPORT_DEPTH 128u

typedef struct CxxModuleToken {
    size_t begin;
    size_t end;
    bool identifier;
} CxxModuleToken;

typedef struct CxxModuleBuffer {
    char* data;
    size_t length;
    size_t capacity;
} CxxModuleBuffer;

typedef struct CxxModuleNameList {
    char** items;
    size_t count;
    size_t capacity;
} CxxModuleNameList;

typedef struct CxxModulePendingArtifact {
    char path[RCC_MAX_PATH];
    char* bytes;
    size_t size;
} CxxModulePendingArtifact;

typedef struct CxxModuleLoadContext {
    Preprocessor* preprocessor;
    const char* source_directory;
    const char** include_paths;
    int include_count;
    TargetArch target_arch;
    int standard;
    RccCxxItaniumAbi abi;
    uint64_t macro_state_hash;
    CxxModuleNameList loaded;
    char* stack[RCC_CXX_MODULE_MAX_IMPORT_DEPTH];
    size_t stack_count;
    char error[512];
} CxxModuleLoadContext;

typedef struct CxxModuleEvent {
    size_t begin;
    size_t end;
    char* import_name;
    char* replacement;
} CxxModuleEvent;

typedef struct CxxModuleEvents {
    CxxModuleEvent* items;
    size_t count;
    size_t capacity;
} CxxModuleEvents;

static bool cxx_module_buffer_reserve(CxxModuleBuffer* buffer,
                                      size_t additional) {
    size_t needed;
    size_t capacity;
    char* replacement;
    if (additional > SIZE_MAX - buffer->length - 1u) return false;
    needed = buffer->length + additional + 1u;
    if (needed <= buffer->capacity) return true;
    capacity = buffer->capacity ? buffer->capacity : 256u;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2u) {
            capacity = needed;
            break;
        }
        capacity *= 2u;
    }
    replacement = rcc_realloc(buffer->data, capacity);
    if (!replacement) return false;
    buffer->data = replacement;
    buffer->capacity = capacity;
    return true;
}

static bool cxx_module_buffer_append(CxxModuleBuffer* buffer,
                                     const char* bytes, size_t length) {
    if ((!bytes && length) || !cxx_module_buffer_reserve(buffer, length)) {
        return false;
    }
    if (length) memcpy(buffer->data + buffer->length, bytes, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return true;
}

static bool cxx_module_buffer_text(CxxModuleBuffer* buffer,
                                   const char* text) {
    return text && cxx_module_buffer_append(buffer, text, strlen(text));
}

static bool cxx_module_name_valid(const char* name) {
    const unsigned char* cursor = (const unsigned char*)name;
    bool need_identifier = true;
    if (!name || !*name) return false;
    for (; *cursor; ++cursor) {
        if (*cursor == '.' || *cursor == ':') {
            if (need_identifier) return false;
            need_identifier = true;
        } else if (isalnum(*cursor) || *cursor == '_') {
            need_identifier = false;
        } else {
            return false;
        }
    }
    return !need_identifier;
}

static bool cxx_module_names_add(CxxModuleNameList* list,
                                 const char* name) {
    char** replacement;
    size_t capacity;
    if (!list || !name || !*name) return false;
    for (size_t index = 0; index < list->count; ++index) {
        if (strcmp(list->items[index], name) == 0) return true;
    }
    if (list->count == list->capacity) {
        capacity = list->capacity ? list->capacity * 2u : 8u;
        if (capacity < list->capacity ||
            capacity > SIZE_MAX / sizeof(list->items[0])) return false;
        replacement = rcc_realloc(list->items,
                                  capacity * sizeof(list->items[0]));
        if (!replacement) return false;
        list->items = replacement;
        list->capacity = capacity;
    }
    list->items[list->count] = rcc_strdup(name);
    if (!list->items[list->count]) return false;
    ++list->count;
    return true;
}

static bool cxx_module_name_list_contains(const CxxModuleNameList* list,
                                          const char* name) {
    if (!list || !name) return false;
    for (size_t index = 0; index < list->count; ++index) {
        if (strcmp(list->items[index], name) == 0) return true;
    }
    return false;
}

static void cxx_module_names_clear(CxxModuleNameList* list) {
    if (!list) return;
    for (size_t index = 0; index < list->count; ++index) {
        rcc_free(list->items[index]);
    }
    rcc_free(list->items);
    memset(list, 0, sizeof(*list));
}

static bool cxx_module_events_add(CxxModuleEvents* events, size_t begin,
                                  size_t end, char* import_name,
                                  char* replacement_text) {
    CxxModuleEvent* replacement;
    size_t capacity;
    if (!events || end < begin) return false;
    if (events->count == events->capacity) {
        capacity = events->capacity ? events->capacity * 2u : 8u;
        if (capacity < events->capacity ||
            capacity > SIZE_MAX / sizeof(events->items[0])) return false;
        replacement = rcc_realloc(events->items,
                                  capacity * sizeof(events->items[0]));
        if (!replacement) return false;
        events->items = replacement;
        events->capacity = capacity;
    }
    events->items[events->count].begin = begin;
    events->items[events->count].end = end;
    events->items[events->count].import_name = import_name;
    events->items[events->count].replacement = replacement_text;
    ++events->count;
    return true;
}

static void cxx_module_events_clear(CxxModuleEvents* events) {
    if (!events) return;
    for (size_t index = 0; index < events->count; ++index) {
        rcc_free(events->items[index].import_name);
        rcc_free(events->items[index].replacement);
    }
    rcc_free(events->items);
    memset(events, 0, sizeof(*events));
}

static bool cxx_module_identifier_start(unsigned char value) {
    return isalpha(value) || value == '_';
}

static bool cxx_module_identifier_part(unsigned char value) {
    return isalnum(value) || value == '_';
}

static bool cxx_module_tokens_add(CxxModuleToken** tokens, size_t* count,
                                  size_t* capacity, size_t begin,
                                  size_t end, bool identifier) {
    CxxModuleToken* replacement;
    size_t new_capacity;
    if (*count >= RCC_CXX_MODULE_MAX_TOKENS) return false;
    if (*count == *capacity) {
        new_capacity = *capacity ? *capacity * 2u : 256u;
        if (new_capacity < *capacity ||
            new_capacity > SIZE_MAX / sizeof((*tokens)[0])) return false;
        replacement = rcc_realloc(*tokens,
                                  new_capacity * sizeof((*tokens)[0]));
        if (!replacement) return false;
        *tokens = replacement;
        *capacity = new_capacity;
    }
    (*tokens)[*count].begin = begin;
    (*tokens)[*count].end = end;
    (*tokens)[*count].identifier = identifier;
    ++*count;
    return true;
}

static size_t cxx_module_skip_quoted(const char* source, size_t length,
                                    size_t quote) {
    char delimiter = source[quote];
    size_t cursor = quote + 1u;
    while (cursor < length) {
        if (source[cursor] == '\\') {
            cursor += cursor + 1u < length ? 2u : 1u;
        } else if (source[cursor++] == delimiter) {
            break;
        }
    }
    return cursor;
}

static size_t cxx_module_skip_raw_string(const char* source, size_t length,
                                         size_t quote) {
    size_t open = quote + 1u;
    size_t delimiter_length;
    size_t cursor;
    while (open < length && source[open] != '(' && source[open] != '\n' &&
           source[open] != '\\' && source[open] != ')') {
        ++open;
    }
    if (open == length || source[open] != '(') return quote + 1u;
    delimiter_length = open - quote - 1u;
    cursor = open + 1u;
    while (cursor < length) {
        if (source[cursor] == ')' &&
            delimiter_length <= length - cursor - 1u &&
            memcmp(source + cursor + 1u, source + quote + 1u,
                   delimiter_length) == 0 &&
            cursor + delimiter_length + 1u < length &&
            source[cursor + delimiter_length + 1u] == '"') {
            return cursor + delimiter_length + 2u;
        }
        ++cursor;
    }
    return length;
}

static bool cxx_module_lex(const char* source, CxxModuleToken** tokens_out,
                           size_t* count_out) {
    size_t length = strlen(source);
    size_t cursor = 0u;
    size_t count = 0u;
    size_t capacity = 0u;
    CxxModuleToken* tokens = NULL;
    while (cursor < length) {
        size_t begin;
        unsigned char value = (unsigned char)source[cursor];
        if (isspace(value)) {
            ++cursor;
            continue;
        }
        if (source[cursor] == '#') {
            size_t line_begin = cursor;
            while (line_begin > 0u && source[line_begin - 1u] != '\n') {
                --line_begin;
            }
            while (line_begin < cursor &&
                   (source[line_begin] == ' ' || source[line_begin] == '\t' ||
                    source[line_begin] == '\r')) ++line_begin;
            if (line_begin == cursor &&
                cursor + 5u <= length &&
                memcmp(source + cursor, "#line", 5u) == 0) {
                while (cursor < length && source[cursor] != '\n') ++cursor;
                continue;
            }
        }
        if (source[cursor] == '/' && cursor + 1u < length &&
            source[cursor + 1u] == '/') {
            cursor += 2u;
            while (cursor < length && source[cursor] != '\n') ++cursor;
            continue;
        }
        if (source[cursor] == '/' && cursor + 1u < length &&
            source[cursor + 1u] == '*') {
            cursor += 2u;
            while (cursor + 1u < length &&
                   !(source[cursor] == '*' && source[cursor + 1u] == '/')) {
                ++cursor;
            }
            if (cursor + 1u < length) cursor += 2u;
            continue;
        }
        if (source[cursor] == 'R' && cursor + 1u < length &&
            source[cursor + 1u] == '"') {
            begin = cursor;
            cursor = cxx_module_skip_raw_string(source, length, cursor + 1u);
            if (cursor <= begin + 1u ||
                !cxx_module_tokens_add(&tokens, &count, &capacity, begin,
                                       cursor, false)) goto fail;
            continue;
        }
        if (source[cursor] == '"' || source[cursor] == '\'') {
            begin = cursor;
            cursor = cxx_module_skip_quoted(source, length, cursor);
            if (!cxx_module_tokens_add(&tokens, &count, &capacity, begin,
                                       cursor, false)) goto fail;
            continue;
        }
        if (cxx_module_identifier_start(value)) {
            begin = cursor++;
            while (cursor < length &&
                   cxx_module_identifier_part(
                       (unsigned char)source[cursor])) ++cursor;
            if (cursor < length && source[cursor] == '"' &&
                ((cursor - begin == 1u && source[begin] == 'R') ||
                 (cursor - begin == 3u &&
                  memcmp(source + begin, "u8R", 3u) == 0) ||
                 (cursor - begin == 2u &&
                  (source[begin] == 'u' || source[begin] == 'U' ||
                   source[begin] == 'L') && source[begin + 1u] == 'R'))) {
                cursor = cxx_module_skip_raw_string(source, length, cursor);
                if (!cxx_module_tokens_add(&tokens, &count, &capacity, begin,
                                           cursor, false)) goto fail;
                continue;
            }
            if (!cxx_module_tokens_add(&tokens, &count, &capacity, begin,
                                       cursor, true)) goto fail;
            continue;
        }
        begin = cursor++;
        if (!cxx_module_tokens_add(&tokens, &count, &capacity, begin,
                                   cursor, false)) goto fail;
    }
    *tokens_out = tokens;
    *count_out = count;
    return true;
fail:
    rcc_free(tokens);
    return false;
}

static bool cxx_module_token_is(const char* source,
                                const CxxModuleToken* token,
                                const char* spelling) {
    size_t length;
    if (!source || !token || !spelling || !token->identifier) return false;
    length = strlen(spelling);
    return token->end - token->begin == length &&
           memcmp(source + token->begin, spelling, length) == 0;
}

static bool cxx_module_append_without_export(const char* source,
                                             size_t begin, size_t end,
                                             CxxModuleBuffer* output) {
    CxxModuleToken* tokens = NULL;
    size_t count = 0u;
    size_t cursor = begin;
    if (end < begin) return false;
    if (!cxx_module_lex(source + begin, &tokens, &count)) return false;
    for (size_t index = 0; index < count; ++index) {
        CxxModuleToken* token = &tokens[index];
        size_t absolute_begin = begin + token->begin;
        size_t absolute_end = begin + token->end;
        if (cxx_module_token_is(source + begin, token, "export")) {
            if (!cxx_module_buffer_append(output, source + cursor,
                                          absolute_begin - cursor)) {
                rcc_free(tokens);
                return false;
            }
            cursor = absolute_end;
        }
    }
    if (!cxx_module_buffer_append(output, source + cursor, end - cursor)) {
        rcc_free(tokens);
        return false;
    }
    rcc_free(tokens);
    return true;
}

static char* cxx_module_copy_span(const char* source, size_t begin,
                                  size_t end) {
    char* copy;
    if (!source || end < begin || end - begin == SIZE_MAX) return NULL;
    copy = rcc_alloc(end - begin + 1u);
    if (!copy) return NULL;
    memcpy(copy, source + begin, end - begin);
    copy[end - begin] = '\0';
    return copy;
}

static bool cxx_module_parse_name(const char* source,
                                 CxxModuleToken* tokens,
                                 size_t first, size_t semicolon,
                                 char** name_out, bool* private_fragment) {
    CxxModuleBuffer name = {0};
    if (!source || !tokens || first > semicolon || !name_out) return false;
    *name_out = NULL;
    if (private_fragment) *private_fragment = false;
    if (first == semicolon) return true; /* global module fragment */
    if (first + 1u < semicolon &&
        source[tokens[first].begin] == ':' &&
        cxx_module_token_is(source, &tokens[first + 1u], "private")) {
        if (private_fragment) *private_fragment = true;
        return true;
    }
    for (size_t index = first; index < semicolon; ++index) {
        if (!tokens[index].identifier &&
            !(tokens[index].end - tokens[index].begin == 1u &&
              (source[tokens[index].begin] == '.' ||
               source[tokens[index].begin] == ':'))) {
            rcc_free(name.data);
            return false;
        }
        if (!cxx_module_buffer_append(&name,
                                      source + tokens[index].begin,
                                      tokens[index].end - tokens[index].begin)) {
            rcc_free(name.data);
            return false;
        }
    }
    if (!name.data || !cxx_module_name_valid(name.data)) {
        rcc_free(name.data);
        return false;
    }
    *name_out = name.data;
    return true;
}

static bool cxx_module_punct(const char* source,
                             const CxxModuleToken* token, char punct) {
    return source && token && !token->identifier &&
           token->end - token->begin == 1u &&
           source[token->begin] == punct;
}

static size_t cxx_module_find_next_semicolon(const char* source,
                                             CxxModuleToken* tokens,
                                             size_t count, size_t first) {
    for (size_t index = first; index < count; ++index) {
        if (cxx_module_punct(source, &tokens[index], ';')) return index;
    }
    return count;
}

static size_t cxx_module_find_matching_brace(const char* source,
                                             CxxModuleToken* tokens,
                                             size_t count, size_t open) {
    size_t depth = 0u;
    for (size_t index = open; index < count; ++index) {
        if (cxx_module_punct(source, &tokens[index], '{')) {
            ++depth;
        } else if (cxx_module_punct(source, &tokens[index], '}')) {
            if (depth == 0u) return count;
            if (--depth == 0u) return index;
        }
    }
    return count;
}

static bool cxx_module_prefix_has(CxxModuleToken* tokens, size_t first,
                                  size_t last, const char* source,
                                  const char* word) {
    for (size_t index = first; index < last; ++index) {
        if (cxx_module_token_is(source, &tokens[index], word)) return true;
    }
    return false;
}

static size_t cxx_module_find_export_declaration_end(
    const char* source, CxxModuleToken* tokens, size_t count,
    size_t first, size_t* body_open_out, size_t* body_close_out) {
    int paren = 0;
    int bracket = 0;
    int brace = 0;
    bool saw_paren = false;
    bool saw_assignment = false;
    bool type_or_namespace = false;
    if (body_open_out) *body_open_out = count;
    if (body_close_out) *body_close_out = count;
    for (size_t index = first; index < count; ++index) {
        CxxModuleToken* token = &tokens[index];
        if (paren == 0 && bracket == 0 && brace == 0) {
            type_or_namespace = type_or_namespace ||
                cxx_module_token_is(source, token, "class") ||
                cxx_module_token_is(source, token, "struct") ||
                cxx_module_token_is(source, token, "union") ||
                cxx_module_token_is(source, token, "enum") ||
                cxx_module_token_is(source, token, "namespace");
            if (cxx_module_punct(source, token, '=')) {
                saw_assignment = true;
            }
            if (cxx_module_punct(source, token, ';')) return index + 1u;
        }
        if (cxx_module_punct(source, token, '(')) {
            if (paren == 0 && brace == 0) saw_paren = true;
            ++paren;
        } else if (cxx_module_punct(source, token, ')') && paren > 0) {
            --paren;
        } else if (cxx_module_punct(source, token, '[')) {
            ++bracket;
        } else if (cxx_module_punct(source, token, ']') && bracket > 0) {
            --bracket;
        } else if (cxx_module_punct(source, token, '{')) {
            if (paren == 0 && bracket == 0 && brace == 0 &&
                saw_paren && !saw_assignment && !type_or_namespace) {
                size_t close = cxx_module_find_matching_brace(
                    source, tokens, count, index);
                if (close == count) return count;
                if (body_open_out) *body_open_out = index;
                if (body_close_out) *body_close_out = close;
                return close + 1u;
            }
            ++brace;
        } else if (cxx_module_punct(source, token, '}') && brace > 0) {
            --brace;
            if (brace == 0 && paren == 0 && bracket == 0 &&
                type_or_namespace) {
                return index + 1u < count &&
                       cxx_module_punct(source, &tokens[index + 1u], ';')
                    ? index + 2u : index + 1u;
            }
        }
    }
    return count;
}

static bool cxx_module_export_span(const char* source,
                                   CxxModuleToken* tokens,
                                   size_t first, size_t end,
                                   size_t body_open, size_t body_close,
                                   CxxModuleBuffer* output) {
    size_t begin_byte;
    size_t end_byte;
    bool preserve_body;
    bool is_type_or_alias;
    bool is_function;
    bool is_inline;
    size_t initializer = end;
    if (first >= end || end > SIZE_MAX) return false;
    begin_byte = tokens[first].begin;
    end_byte = tokens[end - 1u].end;
    if (body_open < end && body_close < end && body_open < body_close) {
        preserve_body = cxx_module_prefix_has(
            tokens, first, body_open, source, "template") ||
            cxx_module_prefix_has(tokens, first, body_open, source, "inline") ||
            cxx_module_prefix_has(tokens, first, body_open, source, "constexpr") ||
            cxx_module_prefix_has(tokens, first, body_open, source, "consteval");
        if (!preserve_body) {
            if (!cxx_module_buffer_append(
                    output, source + begin_byte,
                    tokens[body_open].begin - begin_byte) ||
                !cxx_module_buffer_text(output, ";\n")) return false;
            return true;
        }
    }
    if (body_open >= end) {
        is_type_or_alias = cxx_module_prefix_has(tokens, first, end, source,
                                                  "template") ||
            cxx_module_prefix_has(tokens, first, end, source, "class") ||
            cxx_module_prefix_has(tokens, first, end, source, "struct") ||
            cxx_module_prefix_has(tokens, first, end, source, "union") ||
            cxx_module_prefix_has(tokens, first, end, source, "enum") ||
            cxx_module_prefix_has(tokens, first, end, source, "namespace") ||
            cxx_module_prefix_has(tokens, first, end, source, "using") ||
            cxx_module_prefix_has(tokens, first, end, source, "typedef") ||
            cxx_module_prefix_has(tokens, first, end, source, "concept") ||
            cxx_module_prefix_has(tokens, first, end, source, "static_assert");
        is_function = false;
        for (size_t index = first; index < end; ++index) {
            if ((cxx_module_punct(source, &tokens[index], '=') ||
                 cxx_module_punct(source, &tokens[index], '{')) &&
                !is_function) {
                break;
            }
            if (cxx_module_punct(source, &tokens[index], '(')) {
                is_function = true;
                break;
            }
        }
        is_inline = cxx_module_prefix_has(tokens, first, end, source, "inline");
        if (!is_type_or_alias && !is_function) {
            for (size_t index = first; index < end; ++index) {
                if (cxx_module_punct(source, &tokens[index], '=') ||
                    cxx_module_punct(source, &tokens[index], '{')) {
                    initializer = index;
                    break;
                }
            }
            if (cxx_module_prefix_has(tokens, first, end, source,
                                      "constexpr") || is_inline) {
                if (!is_inline &&
                    !cxx_module_buffer_text(output, "inline ")) return false;
                if (!cxx_module_buffer_append(output, source + begin_byte,
                                              end_byte - begin_byte)) return false;
                return cxx_module_buffer_text(output, "\n");
            }
            if (!cxx_module_prefix_has(tokens, first, end, source, "extern") &&
                !cxx_module_buffer_text(output, "extern ")) return false;
            if (initializer < end) {
                if (!cxx_module_buffer_append(
                        output, source + begin_byte,
                        tokens[initializer].begin - begin_byte)) return false;
                return cxx_module_buffer_text(output, ";\n");
            }
        }
    }
    if (!cxx_module_buffer_append(output, source + begin_byte,
                                  end_byte - begin_byte)) return false;
    return cxx_module_buffer_text(output, "\n");
}

static bool cxx_module_collect_export_block(const char* source,
                                            CxxModuleToken* tokens,
                                            size_t first, size_t close,
                                            CxxModuleBuffer* exports) {
    size_t cursor = first;
    while (cursor < close) {
        size_t end;
        size_t body_open;
        size_t body_close;
        if (cxx_module_punct(source, &tokens[cursor], ';')) {
            ++cursor;
            continue;
        }
        if (cxx_module_token_is(source, &tokens[cursor], "export")) {
            ++cursor;
            if (cursor >= close) return false;
        }
        if (cxx_module_punct(source, &tokens[cursor], '{')) return false;
        end = cxx_module_find_export_declaration_end(
            source, tokens, close, cursor, &body_open, &body_close);
        if (end <= cursor || end > close ||
            !cxx_module_export_span(source, tokens, cursor, end,
                                    body_open, body_close, exports)) {
            return false;
        }
        cursor = end;
    }
    return true;
}

static bool cxx_module_is_header_import(const char* source,
                                        CxxModuleToken* tokens,
                                        size_t first, size_t semicolon) {
    if (first >= semicolon) return false;
    return !tokens[first].identifier ||
        cxx_module_punct(source, &tokens[first], '<');
}

static char* cxx_module_resolve_relative_import(const char* source,
                                                CxxModuleToken* tokens,
                                                size_t first, size_t end,
                                                const char* current_module) {
    CxxModuleBuffer name_buffer = {0};
    char* name;
    size_t length;
    const char* colon;
    size_t prefix_length;
    for (size_t index = first; index < end; ++index) {
        if (!tokens[index].identifier &&
            !(tokens[index].end - tokens[index].begin == 1u &&
              (source[tokens[index].begin] == '.' ||
               source[tokens[index].begin] == ':'))) {
            rcc_free(name_buffer.data);
            return NULL;
        }
        if (!cxx_module_buffer_append(&name_buffer,
                                      source + tokens[index].begin,
                                      tokens[index].end - tokens[index].begin)) {
            rcc_free(name_buffer.data);
            return NULL;
        }
    }
    name = name_buffer.data;
    length = name_buffer.length;
    if (!name || length >= RCC_MAX_PATH) {
        rcc_free(name);
        return NULL;
    }
    if (name[0] == ':') {
        colon = current_module ? strchr(current_module, ':') : NULL;
        prefix_length = colon ? (size_t)(colon - current_module)
                              : (current_module ? strlen(current_module) : 0u);
        if (prefix_length == 0u || prefix_length + length >= RCC_MAX_PATH) {
            rcc_free(name);
            return NULL;
        }
        {
            char* full = rcc_alloc(prefix_length + length + 1u);
            if (!full) {
                rcc_free(name);
                return NULL;
            }
            memcpy(full, current_module, prefix_length);
            memcpy(full + prefix_length, name, length + 1u);
            rcc_free(name);
            name = full;
        }
    }
    return name;
}

static bool cxx_module_directory_of(const char* path, char* output,
                                    size_t capacity) {
    const char* slash;
    const char* backslash;
    const char* separator;
    size_t length;
    if (!path || !output || capacity == 0u) return false;
    slash = strrchr(path, '/');
    backslash = strrchr(path, '\\');
    separator = slash;
    if (backslash && (!separator || backslash > separator)) {
        separator = backslash;
    }
    if (!separator) return rcc_copy_path(output, capacity, ".");
    length = (size_t)(separator - path);
    if (length == 0u) length = 1u;
    if (length + 1u > capacity) return false;
    memcpy(output, path, length);
    output[length] = '\0';
    return true;
}

static uint64_t cxx_module_hash_option(uint64_t hash, unsigned char tag,
                                      const char* value) {
    hash ^= tag;
    hash *= UINT64_C(1099511628211);
    for (const unsigned char* cursor = (const unsigned char*)value;
         cursor && *cursor; ++cursor) {
        hash ^= *cursor;
        hash *= UINT64_C(1099511628211);
    }
    hash ^= 0u;
    hash *= UINT64_C(1099511628211);
    return hash;
}

static uint64_t cxx_module_configuration_hash(void) {
    uint64_t hash = UINT64_C(14695981039346656037);
    char option[32];
    for (int index = 0; index < g_opts.include_count; ++index) {
        hash = cxx_module_hash_option(hash, 'I', g_opts.include_paths[index]);
    }
    for (int index = 0; index < g_opts.define_count; ++index) {
        hash = cxx_module_hash_option(hash, 'D', g_opts.defines[index]);
    }
    for (int index = 0; index < g_opts.undef_count; ++index) {
        hash = cxx_module_hash_option(hash, 'U', g_opts.undefines[index]);
    }
    hash = cxx_module_hash_option(hash, 'F',
                                  g_opts.freestanding ? "1" : "0");
    hash = cxx_module_hash_option(hash, 'N', g_opts.nostdinc ? "1" : "0");
    hash = cxx_module_hash_option(hash, 'S', g_opts.sse_enabled ? "1" : "0");
    hash = cxx_module_hash_option(hash, '2', g_opts.sse2_enabled ? "1" : "0");
    snprintf(option, sizeof(option), "%u", (unsigned)g_opts.cxx_itanium_abi);
    return cxx_module_hash_option(hash, 'A', option);
}

static bool cxx_module_artifact_path(const char* directory,
                                     const char* module_name,
                                     TargetArch target_arch, int standard,
                                     RccCxxItaniumAbi abi,
                                     uint64_t macro_state_hash,
                                     char* output, size_t capacity) {
    size_t directory_length;
    size_t name_length;
    size_t slug_length = 0u;
    char suffix[64];
    int suffix_length;
    size_t needed;
    size_t cursor;
    if (!directory || !module_name || !output || capacity == 0u ||
        !cxx_module_name_valid(module_name)) return false;
    directory_length = strlen(directory);
    name_length = strlen(module_name);
    for (size_t index = 0; index < name_length; ++index) {
        size_t addition = module_name[index] == ':' ? 3u : 1u;
        if (slug_length > SIZE_MAX - addition) return false;
        slug_length += addition;
    }
    suffix_length = snprintf(suffix, sizeof(suffix),
                             ".%s.cxx%d.a%u.m%016llx.rmi",
                             target_arch == ARCH_X64 ? "x64" : "x86",
                             standard, (unsigned)abi,
                             (unsigned long long)macro_state_hash);
    if (suffix_length < 0 || (size_t)suffix_length >= sizeof(suffix) ||
        directory_length > SIZE_MAX - slug_length -
                           (size_t)suffix_length - 2u) return false;
    needed = directory_length + slug_length + (size_t)suffix_length + 2u;
    if (needed >= capacity) return false;
    memcpy(output, directory, directory_length);
    cursor = directory_length;
    if (cursor && output[cursor - 1u] != '/' && output[cursor - 1u] != '\\') {
        output[cursor++] = '/';
    }
    for (size_t index = 0; index < name_length; ++index) {
        if (module_name[index] == ':') {
            memcpy(output + cursor, "%3A", 3u);
            cursor += 3u;
        } else {
            output[cursor++] = module_name[index];
        }
    }
    memcpy(output + cursor, suffix, (size_t)suffix_length + 1u);
    return true;
}

static uint64_t cxx_module_hash(const void* bytes, size_t length) {
    const unsigned char* cursor = bytes;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t index = 0; index < length; ++index) {
        hash ^= cursor[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t cxx_module_hash_prefixed(const char* prefix,
                                         const void* bytes,
                                         size_t length) {
    const unsigned char* cursor = bytes;
    uint64_t hash = cxx_module_hash(prefix, strlen(prefix));
    for (size_t index = 0; index < length; ++index) {
        hash ^= cursor[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static bool cxx_module_build_payload(const char* module_name,
                                     const CxxModuleNameList* dependencies,
                                     const char* exports, size_t exports_size,
                                     CxxModuleBuffer* payload) {
    if (!cxx_module_buffer_text(payload, module_name) ||
        !cxx_module_buffer_text(payload, "\n")) return false;
    for (size_t index = 0; index < dependencies->count; ++index) {
        if (!cxx_module_buffer_text(payload, dependencies->items[index]) ||
            !cxx_module_buffer_text(payload, "\n")) return false;
    }
    return cxx_module_buffer_append(payload, exports, exports_size);
}

static bool cxx_module_artifact_bytes(const char* module_name,
                                      const CxxModuleNameList* dependencies,
                                      const char* exports, size_t exports_size,
                                      const char* preprocessed_source,
                                      TargetArch target_arch, int standard,
                                      RccCxxItaniumAbi abi,
                                      uint64_t macro_state_hash,
                                      CxxModuleBuffer* artifact) {
    CxxModuleBuffer payload = {0};
    char prefix[128];
    char header[128];
    int prefix_size;
    int header_size;
    unsigned target_bits = target_arch == ARCH_X64 ? 64u : 32u;
    uint64_t source_state_hash = cxx_module_hash(
        preprocessed_source, strlen(preprocessed_source));
    uint64_t artifact_checksum;
    bool ok = false;
    if (!cxx_module_build_payload(module_name, dependencies, exports,
                                  exports_size, &payload)) goto done;
    prefix_size = snprintf(prefix, sizeof(prefix),
                           "RCCBMI2 %u %d %u %016llx %lu %lu %016llx ",
                           target_bits, standard, (unsigned)abi,
                           (unsigned long long)macro_state_hash,
                           (unsigned long)dependencies->count,
                           (unsigned long)payload.length,
                           (unsigned long long)source_state_hash);
    if (prefix_size < 0 || (size_t)prefix_size >= sizeof(prefix)) goto done;
    artifact_checksum = cxx_module_hash_prefixed(
        prefix, payload.data, payload.length);
    header_size = snprintf(header, sizeof(header), "%s%016llx\n", prefix,
                           (unsigned long long)artifact_checksum);
    if (header_size < 0 || (size_t)header_size >= sizeof(header) ||
        !cxx_module_buffer_append(artifact, header, (size_t)header_size) ||
        !cxx_module_buffer_append(artifact, payload.data, payload.length)) {
        goto done;
    }
    ok = true;
done:
    rcc_free(payload.data);
    return ok;
}

static bool cxx_module_read_all(const char* path, char** bytes_out,
                                size_t* size_out) {
    FILE* file;
    long end;
    char* bytes;
    if (!path || !bytes_out || !size_out) return false;
    *bytes_out = NULL;
    *size_out = 0u;
    file = fopen(path, "rb");
    if (!file) return false;
    if (fseek(file, 0, SEEK_END) != 0 || (end = ftell(file)) < 0 ||
        (unsigned long)end > RCC_CXX_MODULE_MAX_ARTIFACT ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    bytes = rcc_alloc((size_t)end + 1u);
    if (!bytes) {
        fclose(file);
        return false;
    }
    {
        bool read_ok = fread(bytes, 1u, (size_t)end, file) == (size_t)end;
        bool close_ok = fclose(file) == 0;
        if (!read_ok || !close_ok) {
            rcc_free(bytes);
            return false;
        }
    }
    bytes[end] = '\0';
    *bytes_out = bytes;
    *size_out = (size_t)end;
    return true;
}

static bool cxx_module_find_artifact_path(CxxModuleLoadContext* context,
                                          const char* module_name,
                                          char* path, size_t capacity) {
    char candidate[RCC_MAX_PATH];
    if (cxx_module_artifact_path(context->source_directory, module_name,
                                 context->target_arch, context->standard,
                                 context->abi, context->macro_state_hash,
                                 candidate, sizeof(candidate))) {
        FILE* file = fopen(candidate, "rb");
        if (file) {
            fclose(file);
            return rcc_copy_path(path, capacity, candidate);
        }
    }
    for (int index = 0; index < context->include_count; ++index) {
        if (!cxx_module_artifact_path(context->include_paths[index],
                                      module_name, context->target_arch,
                                      context->standard, context->abi,
                                      context->macro_state_hash, candidate,
                                      sizeof(candidate))) continue;
        FILE* file = fopen(candidate, "rb");
        if (file) {
            fclose(file);
            return rcc_copy_path(path, capacity, candidate);
        }
    }
    if (cxx_module_artifact_path(".", module_name, context->target_arch,
                                 context->standard, context->abi,
                                 context->macro_state_hash, candidate,
                                 sizeof(candidate))) {
        FILE* file = fopen(candidate, "rb");
        if (file) {
            fclose(file);
            return rcc_copy_path(path, capacity, candidate);
        }
    }
    return false;
}

static bool cxx_module_load_artifact(CxxModuleLoadContext* context,
                                     const char* requested_name,
                                     CxxModuleBuffer* imports) {
    char artifact_path[RCC_MAX_PATH];
    char* bytes = NULL;
    size_t size = 0u;
    unsigned target_bits;
    unsigned abi;
    int standard;
    unsigned long dependency_count_value;
    unsigned long payload_size_value;
    size_t dependency_count;
    size_t payload_size;
    unsigned long long expected_hash;
    unsigned long long source_state_hash;
    unsigned long long macro_state_hash;
    int header_bytes = 0;
    char checksum_prefix[128];
    int checksum_prefix_size;
    char* payload;
    char* cursor;
    char* payload_end;
    char* name_end;
    char* module_name = NULL;
    CxxModuleNameList dependencies = {0};
    bool ok = false;
    if (cxx_module_name_list_contains(&context->loaded, requested_name)) {
        return true;
    }
    for (size_t index = 0; index < context->stack_count; ++index) {
        if (strcmp(context->stack[index], requested_name) == 0) {
            snprintf(context->error, sizeof(context->error),
                     "cyclic C++ module dependency involving '%s'",
                     requested_name);
            return false;
        }
    }
    if (context->stack_count >= RCC_CXX_MODULE_MAX_IMPORT_DEPTH) {
        snprintf(context->error, sizeof(context->error),
                 "C++ module dependency depth exceeds %u",
                 (unsigned)RCC_CXX_MODULE_MAX_IMPORT_DEPTH);
        return false;
    }
    if (!cxx_module_find_artifact_path(context, requested_name,
                                       artifact_path,
                                       sizeof(artifact_path)) ||
        !cxx_module_read_all(artifact_path, &bytes, &size)) {
        snprintf(context->error, sizeof(context->error),
                 "module interface '%s' was not found (expected %s.rmi)",
                 requested_name, requested_name);
        return false;
    }
    pp_record_dependency(context->preprocessor, artifact_path);
    if (sscanf(bytes,
               "RCCBMI2 %u %d %u %llx %lu %lu %llx %llx%n", &target_bits,
               &standard, &abi, &macro_state_hash, &dependency_count_value,
               &payload_size_value, &source_state_hash, &expected_hash,
               &header_bytes) != 8 ||
        header_bytes <= 0 ||
        payload_size_value > RCC_CXX_MODULE_MAX_ARTIFACT ||
        (size_t)header_bytes >= size || bytes[header_bytes] != '\n' ||
        (size_t)payload_size_value != size - (size_t)header_bytes - 1u) {
        snprintf(context->error, sizeof(context->error),
                 "module interface '%s' is corrupt or has an invalid header",
                 requested_name);
        goto done;
    }
    checksum_prefix_size = snprintf(
        checksum_prefix, sizeof(checksum_prefix),
        "RCCBMI2 %u %d %u %016llx %lu %lu %016llx ", target_bits,
        standard, abi, macro_state_hash,
        dependency_count_value, payload_size_value, source_state_hash);
    if (checksum_prefix_size < 0 ||
        (size_t)checksum_prefix_size >= sizeof(checksum_prefix) ||
        cxx_module_hash_prefixed(checksum_prefix, bytes + header_bytes + 1,
                                 (size_t)payload_size_value) !=
            (uint64_t)expected_hash) {
        snprintf(context->error, sizeof(context->error),
                 "module interface '%s' is corrupt or has an invalid checksum",
                 requested_name);
        goto done;
    }
    dependency_count = (size_t)dependency_count_value;
    payload_size = (size_t)payload_size_value;
    if (target_bits != (context->target_arch == ARCH_X64 ? 64u : 32u) ||
        standard != context->standard || abi != (unsigned)context->abi) {
        snprintf(context->error, sizeof(context->error),
                 "module interface '%s' was built for a different target, C++ mode, or ABI",
                 requested_name);
        goto done;
    }
    if (macro_state_hash != (unsigned long long)context->macro_state_hash) {
        snprintf(context->error, sizeof(context->error),
                 "module interface '%s' was built with different macro or include-path options",
                 requested_name);
        goto done;
    }
    payload = bytes + header_bytes + 1;
    payload_end = payload + payload_size;
    if (memchr(payload, '\0', payload_size)) {
        snprintf(context->error, sizeof(context->error),
                 "module interface '%s' contains a truncated source payload",
                 requested_name);
        goto done;
    }
    name_end = memchr(payload, '\n', (size_t)(payload_end - payload));
    if (!name_end || name_end == payload ||
        (size_t)(name_end - payload) >= RCC_MAX_PATH) {
        snprintf(context->error, sizeof(context->error),
                 "module interface '%s' has an invalid module identity",
                 requested_name);
        goto done;
    }
    module_name = cxx_module_copy_span(payload, 0u,
                                       (size_t)(name_end - payload));
    if (!module_name || strcmp(module_name, requested_name) != 0) {
        snprintf(context->error, sizeof(context->error),
                 "module interface identity does not match import '%s'",
                 requested_name);
        goto done;
    }
    cursor = name_end + 1;
    for (size_t dependency_index = 0;
         dependency_index < dependency_count; ++dependency_index) {
        char* line_end = memchr(cursor, '\n', (size_t)(payload_end - cursor));
        char* dependency;
        if (!line_end || line_end == cursor ||
            (size_t)(line_end - cursor) >= RCC_MAX_PATH) {
            snprintf(context->error, sizeof(context->error),
                     "module interface '%s' has an invalid dependency list",
                     requested_name);
            goto done;
        }
        dependency = cxx_module_copy_span(
            cursor, 0u, (size_t)(line_end - cursor));
        if (!dependency || !cxx_module_name_valid(dependency) ||
            !cxx_module_names_add(&dependencies, dependency)) {
            rcc_free(dependency);
            snprintf(context->error, sizeof(context->error),
                     "module interface '%s' has an invalid dependency name",
                     requested_name);
            goto done;
        }
        rcc_free(dependency);
        cursor = line_end + 1;
    }
    if (cursor > payload_end) {
        snprintf(context->error, sizeof(context->error),
                 "module interface '%s' is missing its export payload",
                 requested_name);
        goto done;
    }
    context->stack[context->stack_count++] = module_name;
    module_name = NULL;
    for (size_t index = 0; index < dependencies.count; ++index) {
        if (!cxx_module_load_artifact(context, dependencies.items[index],
                                      imports)) goto pop;
    }
    if (!cxx_module_name_list_contains(&context->loaded, requested_name)) {
        if (!cxx_module_buffer_append(imports, cursor,
                                      (size_t)(payload_end - cursor)) ||
            !cxx_module_buffer_text(imports, "\n") ||
            !cxx_module_names_add(&context->loaded, requested_name)) {
            snprintf(context->error, sizeof(context->error),
                     "out of memory importing module '%s'", requested_name);
            goto pop;
        }
    }
    ok = true;
pop:
    rcc_free(context->stack[--context->stack_count]);
done:
    rcc_free(module_name);
    cxx_module_names_clear(&dependencies);
    rcc_free(bytes);
    return ok;
}

static bool cxx_module_resolve_imports(const char* source,
                                       CxxModuleToken* tokens,
                                       size_t token_count,
                                       const char* current_module,
                                       bool module_interface,
                                       CxxModuleEvents* events,
                                       CxxModuleNameList* dependencies,
                                       CxxModuleBuffer* exports,
                                       char* error, size_t error_capacity) {
    int brace_depth = 0;
    bool private_fragment = false;
    char* declared_module = NULL;
    for (size_t index = 0; index < token_count; ++index) {
        bool exported = cxx_module_token_is(source, &tokens[index], "export");
        size_t keyword_index = exported ? index + 1u : index;
        bool declaration_start = index == 0u ||
            cxx_module_punct(source, &tokens[index - 1u], ';') ||
            cxx_module_punct(source, &tokens[index - 1u], '}');
        bool is_module = keyword_index < token_count &&
            cxx_module_token_is(source, &tokens[keyword_index], "module");
        bool is_import = keyword_index < token_count &&
            cxx_module_token_is(source, &tokens[keyword_index], "import");
        if (brace_depth == 0 && declaration_start && !private_fragment &&
            (is_module || is_import)) {
            size_t semicolon = cxx_module_find_next_semicolon(
                source, tokens, token_count, keyword_index + 1u);
            size_t event_begin = tokens[index].begin;
            size_t event_end;
            char* name = NULL;
            if (semicolon == token_count) {
                snprintf(error, error_capacity,
                         "unterminated C++ module or import declaration");
                goto fail;
            }
            event_end = tokens[semicolon].end;
            if (is_module) {
                bool private_marker = false;
                if (!cxx_module_parse_name(source, tokens,
                                           keyword_index + 1u, semicolon,
                                           &name, &private_marker)) {
                    snprintf(error, error_capacity,
                             "invalid C++ module declaration");
                    goto fail;
                }
                if (private_marker) {
                    private_fragment = true;
                } else if (name) {
                    if (declared_module) {
                        snprintf(error, error_capacity,
                                 "a translation unit may declare only one named module");
                        rcc_free(name);
                        goto fail;
                    }
                    declared_module = name;
                    name = NULL;
                }
                if (!exported && declared_module &&
                    strcmp(declared_module, current_module ? current_module : "") == 0) {
                    /* Module implementation units see the primary interface. */
                    name = rcc_strdup(declared_module);
                    if (!name) goto allocation_fail;
                }
            } else {
                size_t import_first = keyword_index + 1u;
                if (exported && (!module_interface || !declared_module)) {
                    snprintf(error, error_capacity,
                             "export import requires a C++ module interface unit");
                    goto fail;
                }
                if (cxx_module_is_header_import(source, tokens,
                                                import_first, semicolon)) {
                    snprintf(error, error_capacity,
                             "C++ header-unit imports are not supported yet");
                    goto fail;
                }
                if (import_first >= semicolon) {
                    snprintf(error, error_capacity,
                             "empty C++ module import declaration");
                    goto fail;
                }
                name = cxx_module_resolve_relative_import(
                    source, tokens, import_first, semicolon,
                    declared_module ? declared_module : current_module);
                if (!name || !cxx_module_name_valid(name)) {
                    rcc_free(name);
                    snprintf(error, error_capacity,
                             "invalid C++ module import name");
                    goto fail;
                }
                if (!cxx_module_names_add(dependencies, name)) {
                    rcc_free(name);
                    goto allocation_fail;
                }
            }
            if (!cxx_module_events_add(events, event_begin, event_end,
                                       name, NULL)) {
                rcc_free(name);
                goto allocation_fail;
            }
            index = semicolon;
            continue;
        }
        if (brace_depth == 0 && declaration_start && !private_fragment && exported &&
            keyword_index < token_count &&
            cxx_module_punct(source, &tokens[keyword_index], '{')) {
            size_t close = cxx_module_find_matching_brace(
                source, tokens, token_count, keyword_index);
            CxxModuleBuffer replacement = {0};
            if (!module_interface || !declared_module) {
                snprintf(error, error_capacity,
                         "export declaration requires a C++ module interface unit");
                goto fail;
            }
            if (close == token_count) {
                snprintf(error, error_capacity,
                         "unterminated C++ export block");
                goto fail;
            }
            if (!cxx_module_collect_export_block(
                    source, tokens, keyword_index + 1u, close, exports) ||
                !cxx_module_append_without_export(
                    source, tokens[keyword_index].end,
                    tokens[close].begin, &replacement) ||
                !cxx_module_buffer_text(exports, "\n") ||
                !cxx_module_events_add(events, tokens[index].begin,
                                       tokens[close].end, NULL,
                                       replacement.data)) {
                rcc_free(replacement.data);
                goto allocation_fail;
            }
            replacement.data = NULL;
            index = close;
            continue;
        }
        if (brace_depth == 0 && declaration_start && !private_fragment &&
            exported) {
            size_t body_open;
            size_t body_close;
            if (!module_interface || !declared_module) {
                snprintf(error, error_capacity,
                         "export declaration requires a C++ module interface unit");
                goto fail;
            }
            size_t end = cxx_module_find_export_declaration_end(
                source, tokens, token_count, keyword_index + 1u,
                &body_open, &body_close);
            if (end == token_count) {
                snprintf(error, error_capacity,
                         "unterminated C++ export declaration");
                goto fail;
            }
            if (!cxx_module_export_span(source, tokens, keyword_index + 1u,
                                        end, body_open, body_close,
                                        exports) ||
                !cxx_module_events_add(events, tokens[index].begin,
                                       tokens[index].end, NULL, NULL)) {
                goto allocation_fail;
            }
            index = end - 1u;
            continue;
        }
        if (cxx_module_punct(source, &tokens[index], '{')) {
            ++brace_depth;
        } else if (cxx_module_punct(source, &tokens[index], '}') &&
                   brace_depth > 0) {
            --brace_depth;
        }
    }
    rcc_free(declared_module);
    return true;
allocation_fail:
    snprintf(error, error_capacity, "out of memory processing C++ modules");
fail:
    rcc_free(declared_module);
    return false;
}

static bool cxx_module_rewrite_source(const char* source,
                                      CxxModuleEvents* events,
                                      CxxModuleLoadContext* load_context,
                                      char** rewritten_out) {
    CxxModuleBuffer rewritten = {0};
    CxxModuleBuffer imports = {0};
    size_t cursor = 0u;
    if (!source || !events || !rewritten_out) return false;
    *rewritten_out = NULL;
    for (size_t index = 0; index < events->count; ++index) {
        CxxModuleEvent* event = &events->items[index];
        if (event->begin < cursor || event->end > strlen(source)) goto fail;
        if (!cxx_module_buffer_append(&rewritten, source + cursor,
                                      event->begin - cursor)) goto fail;
        if (event->import_name) {
            imports.length = 0u;
            if (imports.data) imports.data[0] = '\0';
            if (!cxx_module_load_artifact(load_context, event->import_name,
                                          &imports) ||
                !cxx_module_buffer_append(&rewritten, imports.data,
                                          imports.length)) goto fail;
        }
        if (event->replacement &&
            !cxx_module_buffer_text(&rewritten, event->replacement)) goto fail;
        cursor = event->end;
    }
    if (!cxx_module_buffer_append(&rewritten, source + cursor,
                                  strlen(source) - cursor)) goto fail;
    *rewritten_out = rewritten.data;
    rcc_free(imports.data);
    return true;
fail:
    rcc_free(imports.data);
    rcc_free(rewritten.data);
    return false;
}

static bool cxx_module_prepare(const char* source, Preprocessor* preprocessor,
                               const char* input_path,
                               char** rewritten_out,
                               CxxModulePendingArtifact* pending,
                               CxxModuleLoadContext* load_context) {
    CxxModuleToken* tokens = NULL;
    size_t token_count = 0u;
    CxxModuleEvents events = {0};
    CxxModuleNameList dependencies = {0};
    CxxModuleBuffer exports = {0};
    CxxModuleBuffer artifact = {0};
    char source_directory[RCC_MAX_PATH];
    char module_name[RCC_MAX_PATH] = {0};
    char error[512] = {0};
    bool has_interface = false;
    bool ok = false;
    if (!source || !input_path || !rewritten_out || !pending ||
        !load_context) return false;
    memset(pending, 0, sizeof(*pending));
    *rewritten_out = NULL;
    if (!cxx_module_directory_of(input_path, source_directory,
                                 sizeof(source_directory)) ||
        !cxx_module_lex(source, &tokens, &token_count)) {
        snprintf(error, sizeof(error), "could not scan C++ module source");
        goto done;
    }
    /* Find a named interface declaration first so relative partition imports
     * can resolve even when they precede its export block. */
    for (size_t index = 0; index + 1u < token_count; ++index) {
        bool exported = cxx_module_token_is(source, &tokens[index], "export");
        size_t keyword = exported ? index + 1u : index;
        bool declaration_start = index == 0u ||
            cxx_module_punct(source, &tokens[index - 1u], ';') ||
            cxx_module_punct(source, &tokens[index - 1u], '}');
        if (!declaration_start || keyword >= token_count ||
            !cxx_module_token_is(source, &tokens[keyword], "module")) continue;
        size_t semicolon = cxx_module_find_next_semicolon(
            source, tokens, token_count, keyword + 1u);
        char* parsed_name = NULL;
        bool private_marker = false;
        if (semicolon == token_count ||
            !cxx_module_parse_name(source, tokens, keyword + 1u,
                                   semicolon, &parsed_name,
                                   &private_marker)) {
            snprintf(error, sizeof(error), "invalid C++ module declaration");
            goto done;
        }
        if (parsed_name) {
            if (strlen(parsed_name) >= sizeof(module_name)) {
                rcc_free(parsed_name);
                snprintf(error, sizeof(error), "C++ module name is too long");
                goto done;
            }
            memcpy(module_name, parsed_name, strlen(parsed_name) + 1u);
            has_interface = exported;
            rcc_free(parsed_name);
            break;
        }
    }
    load_context->source_directory = source_directory;
    load_context->preprocessor = preprocessor;
    load_context->include_paths = g_opts.include_paths;
    load_context->include_count = g_opts.include_count;
    load_context->target_arch = g_opts.target_arch;
    load_context->standard = g_cxx_standard;
    load_context->abi = g_opts.cxx_itanium_abi;
    load_context->macro_state_hash = cxx_module_configuration_hash();
    if (!cxx_module_resolve_imports(source, tokens, token_count,
                                    module_name[0] ? module_name : NULL,
                                    has_interface,
                                    &events, &dependencies, &exports,
                                    error, sizeof(error))) {
        goto done;
    }
    if (has_interface) {
        if (load_context->stack_count >= RCC_CXX_MODULE_MAX_IMPORT_DEPTH) {
            snprintf(error, sizeof(error),
                     "C++ module dependency depth exceeds %u",
                     (unsigned)RCC_CXX_MODULE_MAX_IMPORT_DEPTH);
            goto done;
        }
        load_context->stack[load_context->stack_count] =
            rcc_strdup(module_name);
        if (!load_context->stack[load_context->stack_count]) {
            snprintf(error, sizeof(error),
                     "out of memory recording C++ module identity");
            goto done;
        }
        ++load_context->stack_count;
    }
    {
        bool rewrite_ok = cxx_module_rewrite_source(
            source, &events, load_context, rewritten_out);
        if (has_interface) {
            rcc_free(load_context->stack[--load_context->stack_count]);
        }
        if (!rewrite_ok) {
            if (!error[0]) {
                snprintf(error, sizeof(error), "%s",
                         load_context->error[0] ? load_context->error
                                                : "could not rewrite C++ module source");
            }
            goto done;
        }
    }
    if (has_interface) {
        if (!cxx_module_artifact_path(source_directory, module_name,
                                      g_opts.target_arch, g_cxx_standard,
                                      g_opts.cxx_itanium_abi,
                                      load_context->macro_state_hash,
                                      pending->path,
                                      sizeof(pending->path)) ||
            !cxx_module_artifact_bytes(module_name, &dependencies,
                                       exports.data ? exports.data : "",
                                       exports.length, source,
                                       g_opts.target_arch,
                                       g_cxx_standard,
                                       g_opts.cxx_itanium_abi,
                                       load_context->macro_state_hash,
                                       &artifact)) {
            snprintf(error, sizeof(error),
                     "could not create the C++ module interface artifact");
            goto done;
        }
        pending->bytes = artifact.data;
        pending->size = artifact.length;
        artifact.data = NULL;
    }
    ok = true;
done:
    if (!ok) {
        fprintf(stderr, "rcc++: error: %s\n", error[0] ? error :
                "C++ module processing failed");
        rcc_free(*rewritten_out);
        *rewritten_out = NULL;
    }
    rcc_free(tokens);
    cxx_module_events_clear(&events);
    cxx_module_names_clear(&dependencies);
    rcc_free(exports.data);
    rcc_free(artifact.data);
    cxx_module_names_clear(&load_context->loaded);
    return ok;
}

static bool cxx_module_publish(CxxModulePendingArtifact* pending) {
    char staged_path[RCC_MAX_PATH];
    FILE* file;
    bool ok;
    if (!pending || !pending->bytes) return true;
    if (!rcc_create_signing_temp(pending->path, "rcc-module", staged_path,
                                 sizeof(staged_path))) return false;
    file = fopen(staged_path, "wb");
    if (!file) {
        (void)remove(staged_path);
        return false;
    }
    ok = fwrite(pending->bytes, 1u, pending->size, file) == pending->size;
    if (fclose(file) != 0) ok = false;
    if (ok) {
#if defined(_WIN32)
        if (remove(pending->path) != 0 && errno != ENOENT) {
            (void)remove(staged_path);
            return false;
        }
#endif
        if (rename(staged_path, pending->path) != 0) {
            (void)remove(staged_path);
            return false;
        }
    }
    if (!ok) (void)remove(staged_path);
    return ok;
}

static void cxx_module_pending_clear(CxxModulePendingArtifact* pending) {
    if (!pending) return;
    rcc_free(pending->bytes);
    memset(pending, 0, sizeof(*pending));
}

static void define_cxx_feature_test_macros(Preprocessor* pp) {
    int standard = g_cxx_standard;

    /* These values describe syntax and semantic paths that RCC++ actually
     * lowers.  They intentionally do not claim a complete hosted standard
     * library or an ABI outside the bounded RinOS profile. */
    pp_define(pp, "__cpp_attributes", "200809L");
    pp_define(pp, "__cpp_decltype", "200707L");
    pp_define(pp, "__cpp_lambdas", "200907L");
    pp_define(pp, "__cpp_static_assert", "200410L");
    pp_define(pp, "__cpp_constexpr",
              standard >= 20 ? "201907L" :
              standard >= 17 ? "201603L" :
              standard >= 14 ? "201304L" : "200704L");
    if (standard >= 14) {
        pp_define(pp, "__cpp_deprecated", "201309L");
        pp_define(pp, "__cpp_init_captures", "201304L");
        pp_define(pp, "__cpp_generic_lambdas",
                  standard >= 20 ? "201707L" : "201304L");
        pp_define(pp, "__cpp_variable_templates", "201304L");
    }
    if (standard >= 17) {
        pp_define(pp, "__cpp_deduction_guides", "201611L");
        pp_define(pp, "__cpp_fold_expressions", "201603L");
        pp_define(pp, "__cpp_nested_namespace_definitions", "201411L");
        pp_define(pp, "__cpp_nontype_template_args", "201411L");
        pp_define(pp, "__cpp_if_constexpr", "201606L");
        pp_define(pp, "__cpp_structured_bindings", "201606L");
        pp_define(pp, "__cpp_inline_variables", "201606L");
        pp_define(pp, "__cpp_nodiscard", "201603L");
        pp_define(pp, "__cpp_range_based_for", "201603L");
    }
    if (standard >= 20) {
        pp_define(pp, "__cpp_aggregate_paren_init", "201902L");
        pp_define(pp, "__cpp_consteval", "201811L");
        pp_define(pp, "__cpp_constinit", "201907L");
        pp_define(pp, "__cpp_concepts", "201907L");
        pp_define(pp, "__cpp_char8_t", "201811L");
        pp_define(pp, "__cpp_using_enum", "201907L");
        pp_define(pp, "__cpp_designated_initializers", "201707L");
        pp_define(pp, "__cpp_conditional_explicit", "201806L");
        pp_define(pp, "__cpp_nodiscard", "201907L");
    }
}

/* Print usage for rcc++ */
static void print_usage_cxx(void) {
    printf("RCC++ - RinOS C++ Compiler v%d.%d.%d\n",
           RCC_VERSION_MAJOR, RCC_VERSION_MINOR, RCC_VERSION_PATCH);
    printf("\n");
    printf("Usage: rcc++ [options] <input.cpp> [more-input.cpp ...]\n");
    printf("\n");
    printf("Options:\n");
    printf("  -o <file>       Output file name\n");
    printf("  -c              Compile to .ro v2 object\n");
    printf("  -E              Preprocess only\n");
    printf("  -shared         Create shared library (.rll)\n");
    printf("  -driver         Create driver (.drv)\n");
    printf("  -m32            Generate 32-bit code (default)\n");
    printf("  -m64            Generate 64-bit code\n");
    printf("  -msse/-mno-sse  Enable/disable SSE intrinsics (default: enabled)\n");
    printf("  -msse2/-mno-sse2  Enable/disable SSE2 intrinsics (default: enabled)\n");
    printf("  --target <triple>  i686-unknown-rinos or x86_64-unknown-rinos\n");
    printf("  --manifest <file>  RIN-BUILD-MANIFEST 1 build contract\n");
    printf("  --rinsign/--sign-key/--public-key  Required final v3 signing inputs\n");
    printf("  --sign-profile <p>  Build profile: debug or release\n");
    printf("  -O<level>       Optimization level (0-3)\n");
    printf("  -std=c++<ver>   C++ standard (11, 14, 17, 20)\n");
    printf("  --itanium-abi=gnu|clang18  Select GNU or Clang 18 NTTP mangling\n");
    printf("  -g              Generate debug info\n");
    printf("  -S              Output assembly\n");
    printf("  -I/-D/-U        Include path and macro controls\n");
    printf("  -nostdinc       Don't search standard include paths\n");
    printf("  -MMD/-MF <file> Emit user-header dependencies\n");
    printf("  -Wall/-Werror   Warning controls\n");
    printf("  -ffreestanding  Freestanding environment\n");
    printf("  -fPIC/-fpic     Position-independent code and data references\n");
    printf("  -fPIE/-fpie     Position-independent executable code and data references\n");
    printf("  -frounding-math  Honor dynamic floating-point rounding modes\n");
    printf("  --dynamic-tls/-fdynamic-tls  Use RinOS DTV/__tls_get_addr TLS model\n");
    printf("  -fverified-backend  Use typed-SSA x86 backend when supported\n");
    printf("  -v              Verbose output\n");
    printf("  -h, --help      Show this help\n");
    printf("  --version       Show version\n");
    printf("\n");
    printf("Output formats:\n");
    printf("  .rin            RinOS executable (default)\n");
    printf("  .rll            RinOS shared library\n");
    printf("  .drv            RinOS driver\n");
}

/* Parse C++ specific options */
static int parse_cxx_args(int argc, char** argv) {
    /* Default options */
    g_opts.output_format = OUTPUT_RIN;
    g_opts.target_arch = ARCH_X86;
    g_opts.opt_level = 0;
    g_opts.sse_enabled = true;
    g_opts.sse2_enabled = true;
    g_opts.cxx_itanium_abi = RCC_CXX_ITANIUM_ABI_GNU;
    g_opts.input_count = 0;

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},
        {"shared", no_argument, 0, 1},
        {"driver", no_argument, 0, 2},
        {"target", required_argument, 0, 3},
        {"std", required_argument, 0, 4},
        {"rinsign", required_argument, 0, 5},
        {"sign-key", required_argument, 0, 6},
        {"public-key", required_argument, 0, 7},
        {"python", required_argument, 0, 8},
        {"emit-unsigned-v3", no_argument, 0, 9},
        {"MMD", no_argument, 0, 10},
        {"MF", required_argument, 0, 11},
        {"nostdinc", no_argument, 0, 12},
        {"ffreestanding", no_argument, 0, 13},
        {"pedantic", no_argument, 0, 14},
        {"manifest", required_argument, 0, 15},
        {"sign-profile", required_argument, 0, 16},
        {"verified-backend", no_argument, 0, 17},
        {"fverified-backend", no_argument, 0, 17},
        {"fPIC", no_argument, 0, 18},
        {"fpic", no_argument, 0, 18},
        {"fPIE", no_argument, 0, 19},
        {"fpie", no_argument, 0, 19},
        {"fno-PIC", no_argument, 0, 20},
        {"fno-pic", no_argument, 0, 20},
        {"fno-PIE", no_argument, 0, 21},
        {"fno-pie", no_argument, 0, 21},
        {"dynamic-tls", no_argument, 0, 22},
        {"fdynamic-tls", no_argument, 0, 22},
        {"frounding-math", no_argument, 0, 23},
        {"fno-rounding-math", no_argument, 0, 24},
        {"itanium-abi", required_argument, 0, 25},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long_only(argc, argv, "co:O:m:gSEvhwW:I:D:U:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                g_opts.output_format = OUTPUT_OBJ;
                g_opts.output_format_explicit = true;
                break;
            case 'o':
                if (!rcc_copy_path(g_opts.output_file,
                                   sizeof(g_opts.output_file), optarg)) {
                    fprintf(stderr, "rcc++: error: output path is too long (maximum %u bytes)\n",
                            (unsigned)(sizeof(g_opts.output_file) - 1u));
                    return -1;
                }
                break;
            case 'O':
                if (!rcc_parse_optimization_level(
                        optarg, &g_opts.opt_level)) {
                    fprintf(stderr,
                            "rcc++: error: invalid optimization level "
                            "'-O%s'; expected -O0 through -O3\n", optarg);
                    return -1;
                }
                break;
            case 'm':
                if (strcmp(optarg, "32") == 0) {
                    if (g_opts.target_explicit && g_opts.target_arch != ARCH_X86) {
                        fprintf(stderr, "rcc++: error: conflicting target selections\n");
                        return -1;
                    }
                    g_opts.target_arch = ARCH_X86;
                    g_opts.target_explicit = true;
                } else if (strcmp(optarg, "64") == 0) {
                    if (g_opts.target_explicit && g_opts.target_arch != ARCH_X64) {
                        fprintf(stderr, "rcc++: error: conflicting target selections\n");
                        return -1;
                    }
                    g_opts.target_arch = ARCH_X64;
                    g_opts.target_explicit = true;
                } else if (strcmp(optarg, "sse") == 0) {
                    g_opts.sse_enabled = true;
                } else if (strcmp(optarg, "no-sse") == 0) {
                    g_opts.sse_enabled = false;
                } else if (strcmp(optarg, "sse2") == 0) {
                    g_opts.sse2_enabled = true;
                } else if (strcmp(optarg, "no-sse2") == 0) {
                    g_opts.sse2_enabled = false;
                } else {
                    rcc_fatal("unknown architecture: -m%s", optarg);
                }
                break;
            case 'g':
                g_opts.debug_info = true;
                break;
            case 'S':
                g_opts.output_format = OUTPUT_ASM;
                g_opts.output_format_explicit = true;
                break;
            case 'E':
                g_opts.preprocess_only = true;
                break;
            case 'v':
                g_opts.verbose = true;
                break;
            case 'W':
                if (strcmp(optarg, "error") == 0) {
                    g_opts.warnings_as_errors = true;
                } else if (strcmp(optarg, "all") == 0) {
                    g_opts.wall = true;
                } else {
                    fprintf(stderr, "rcc++: error: unsupported warning option: -W%s\n",
                            optarg);
                    return -1;
                }
                break;
            case 'w':
                g_opts.wall = false;
                break;
            case 'I':
                if (g_opts.include_count >= RCC_MAX_INCLUDES) {
                    fprintf(stderr, "rcc++: error: too many include paths\n");
                    return -1;
                }
                g_opts.include_paths[g_opts.include_count++] = optarg;
                break;
            case 'D':
                if (g_opts.define_count >= RCC_MAX_DEFINES) {
                    fprintf(stderr, "rcc++: error: too many macro definitions\n");
                    return -1;
                }
                g_opts.defines[g_opts.define_count++] = optarg;
                break;
            case 'U':
                if (g_opts.undef_count >= RCC_MAX_DEFINES) {
                    fprintf(stderr, "rcc++: error: too many macro undefinitions\n");
                    return -1;
                }
                g_opts.undefines[g_opts.undef_count++] = optarg;
                break;
            case 'h':
                print_usage_cxx();
                exit(0);
            case 'V':
                printf("rcc++ %d.%d.%d\n",
                       RCC_VERSION_MAJOR, RCC_VERSION_MINOR, RCC_VERSION_PATCH);
                exit(0);
            case 1:  /* --shared */
                g_opts.output_format = OUTPUT_RLL;
                g_opts.output_format_explicit = true;
                break;
            case 2:  /* --driver */
                g_opts.output_format = OUTPUT_DRV;
                g_opts.output_format_explicit = true;
                break;
            case 3: { /* --target */
                TargetArch target_arch;
                if (!rcc_parse_target_triple(optarg, &target_arch)) {
                    fprintf(stderr, "rcc++: error: unsupported target triple: %s\n", optarg);
                    return -1;
                }
                if (g_opts.target_explicit && g_opts.target_arch != target_arch) {
                    fprintf(stderr, "rcc++: error: conflicting target selections\n");
                    return -1;
                }
                g_opts.target_arch = target_arch;
                g_opts.target_explicit = true;
                break;
            }
            case 4: /* --std=c++XX, also accepts the conventional -std form. */
                if (strncmp(optarg, "c++", 3) != 0) {
                    fprintf(stderr, "rcc++: error: unsupported language standard: %s\n", optarg);
                    return -1;
                }
                g_cxx_standard = atoi(optarg + 3);
                if (g_cxx_standard != 11 && g_cxx_standard != 14 &&
                    g_cxx_standard != 17 && g_cxx_standard != 20) {
                    fprintf(stderr, "rcc++: error: unsupported C++ standard: %s\n", optarg);
                    return -1;
                }
                break;
            case 5: g_opts.rinsign_path = optarg; break;
            case 6: g_opts.sign_key = optarg; break;
            case 7: g_opts.public_key = optarg; break;
            case 8: g_opts.python_path = optarg; break;
            case 9: g_opts.emit_unsigned_v3 = true; break;
            case 10: g_opts.emit_dependencies = true; break;
            case 11:
                if (!rcc_copy_path(g_opts.dependency_file,
                                   sizeof(g_opts.dependency_file), optarg)) {
                    fprintf(stderr, "rcc++: error: dependency path is too long (maximum %u bytes)\n",
                            (unsigned)(sizeof(g_opts.dependency_file) - 1u));
                    return -1;
                }
                break;
            case 12: g_opts.nostdinc = true; break;
            case 13: g_opts.freestanding = true; break;
            case 14: g_opts.pedantic = true; break;
            case 15: g_opts.manifest_path = optarg; break;
            case 16:
                if (!rcc_parse_signing_profile(optarg,
                                               &g_opts.signing_profile)) {
                    fprintf(stderr, "rcc++: error: signing profile must be debug or release\n");
                    return -1;
                }
                g_opts.signing_profile_explicit = true;
                break;
            case 17:
                g_opts.verified_backend = true;
                break;
            case 18:
                g_opts.pic = true;
                break;
            case 19:
                g_opts.pie = true;
                break;
            case 20:
                g_opts.pic = false;
                break;
            case 21:
                g_opts.pie = false;
                break;
            case 22:
                g_opts.dynamic_tls = true;
                break;
            case 23:
                g_opts.frounding_math = true;
                break;
            case 24:
                g_opts.frounding_math = false;
                break;
            case 25:
                if (strcmp(optarg, "gnu") == 0) {
                    g_opts.cxx_itanium_abi = RCC_CXX_ITANIUM_ABI_GNU;
                } else if (strcmp(optarg, "clang18") == 0) {
                    g_opts.cxx_itanium_abi = RCC_CXX_ITANIUM_ABI_CLANG18;
                } else {
                    fprintf(stderr,
                            "rcc++: error: unsupported Itanium ABI mode '%s'; "
                            "expected gnu or clang18\n", optarg);
                    return -1;
                }
                break;
            default:
                return -1;
        }
    }

    /* Get input files.  Multiple inputs are independent compile jobs for
     * -c/-S/-E; final image linking remains the responsibility of rld. */
    if (optind >= argc) {
        fprintf(stderr, "rcc++: error: no input file\n");
        return -1;
    }
    for (int index = optind; index < argc; ++index) {
        if (g_opts.input_count >= RCC_MAX_INPUTS) {
            fprintf(stderr,
                    "rcc++: error: too many input files (maximum %d)\n",
                    RCC_MAX_INPUTS);
            return -1;
        }
        g_opts.input_files[g_opts.input_count++] = argv[index];
        if (g_opts.input_count == 1 &&
            !rcc_copy_path(g_opts.input_file, sizeof(g_opts.input_file),
                           argv[index])) {
            fprintf(stderr,
                    "rcc++: error: input path is too long (maximum %u bytes)\n",
                    (unsigned)(sizeof(g_opts.input_file) - 1u));
            return -1;
        }
    }

    if (g_opts.manifest_path) {
        RccBuildManifest manifest;
        char error[RCC_BUILD_MANIFEST_ERROR_MAX];
        if (!rcc_manifest_load(g_opts.manifest_path, &manifest,
                               error, sizeof(error)) ||
            !rcc_manifest_apply_compiler(&manifest, &g_opts,
                                         error, sizeof(error))) {
            fprintf(stderr, "rcc++: error: %s\n", error);
            return -1;
        }
    }
    if (g_opts.input_count > 1) {
        if (g_opts.output_file[0] != '\0') {
            fprintf(stderr,
                    "rcc++: error: -o cannot name one output for multiple input files\n");
            return -1;
        }
        if (!g_opts.preprocess_only &&
            g_opts.output_format != OUTPUT_OBJ &&
            g_opts.output_format != OUTPUT_ASM) {
            fprintf(stderr,
                    "rcc++: error: multiple input files require -c, -S, or -E\n");
            return -1;
        }
        if (g_opts.emit_dependencies && g_opts.dependency_file[0] != '\0') {
            fprintf(stderr,
                    "rcc++: error: -MF cannot name one dependency file for multiple input files\n");
            return -1;
        }
    }

    if (!rcc_validate_signing_options(
            "rcc++", !g_opts.preprocess_only &&
                     (g_opts.output_format == OUTPUT_RIN ||
                      g_opts.output_format == OUTPUT_RLL ||
                      g_opts.output_format == OUTPUT_DRV))) {
        return -1;
    }
    if (g_opts.verified_backend && !g_opts.preprocess_only &&
        g_opts.output_format != OUTPUT_OBJ) {
        fprintf(stderr,
                "rcc++: error: -fverified-backend currently requires -c\n");
        return -1;
    }
    /* Default output file */
    if (g_opts.input_count == 1 && g_opts.output_file[0] == '\0') {
        const char* ext;
        switch (g_opts.output_format) {
            case OUTPUT_RIN: ext = ".rin"; break;
            case OUTPUT_RLL: ext = ".rll"; break;
            case OUTPUT_DRV: ext = ".drv"; break;
            case OUTPUT_ASM: ext = ".s"; break;
            case OUTPUT_OBJ: ext = ".ro"; break;
            default: ext = ".out"; break;
        }

        if (!rcc_derive_output_path(g_opts.input_file, ext,
                                    g_opts.output_file,
                                    sizeof(g_opts.output_file))) {
            fprintf(stderr, "rcc++: error: derived output path is too long (maximum %u bytes)\n",
                    (unsigned)(sizeof(g_opts.output_file) - 1u));
            return -1;
        }
    }

    /* Preserve the selected source standard for relocatable debug metadata.
     * The parser keeps this in g_cxx_standard; the emitter only sees the
     * common compiler options, so copy it after all command-line/manifest
     * validation and before any translation unit is compiled. */
    g_opts.cxx_standard = g_cxx_standard;
    return 0;
}

/* C++ main function */
static int compile_current_input(char** argv) {
    CxxModulePendingArtifact module_pending = {0};
    type_configure_target(g_opts.target_arch);

    /* Initialize C++ subsystem */
    cxx_init();

    if (g_opts.verbose) {
        printf("Input:  %s\n", g_opts.input_file);
        printf("Output: %s\n", g_opts.output_file);
        printf("Format: %s\n",
               g_opts.output_format == OUTPUT_RIN ? ".rin" :
               g_opts.output_format == OUTPUT_RLL ? ".rll" :
               g_opts.output_format == OUTPUT_DRV ? ".drv" : ".s");
        printf("Target: %s\n", rcc_target_triple(g_opts.target_arch));
        printf("C++:    C++%d\n", g_cxx_standard);
    }

    /* Phase 0: Preprocessing */
    if (g_opts.verbose) {
        printf("Preprocessing...\n");
    }
    Preprocessor* pp = pp_new();
    pp->cxx_mode = true;
    pp->cxx_standard = g_cxx_standard;
    /* Keep the operator discoverable to #if defined(...) without letting the
     * ordinary macro expander decide its argument semantics. */
    pp_define(pp, "__has_cpp_attribute", "1");

    /* Add standard include paths (unless -nostdinc). */
    /* User include directories take precedence over compiler/system headers,
       matching the documented -I search order. */
    for (int i = 0; i < g_opts.include_count; ++i) {
        pp_add_include_path(pp, g_opts.include_paths[i]);
    }

    if (!g_opts.nostdinc) {
        char tool_include[RCC_MAX_PATH];
        char tool_bootstrap_include[RCC_MAX_PATH];
        char installed_include[RCC_MAX_PATH];
        pp_add_include_path(pp, ".");
        pp_add_include_path(pp, "include");
        pp_add_include_path(pp, "include/rcc");
        if (rcc_tool_relative_path(argv[0], "include/rcc", tool_include,
                                   sizeof(tool_include))) {
            pp_add_include_path(pp, tool_include);
        }
        if (rcc_tool_relative_path(argv[0], "bootstrap/include",
                                   tool_bootstrap_include,
                                   sizeof(tool_bootstrap_include))) {
            pp_add_include_path(pp, tool_bootstrap_include);
        }
        if (rcc_tool_relative_path(argv[0], "../include/rcc",
                                   installed_include,
                                   sizeof(installed_include))) {
            pp_add_include_path(pp, installed_include);
        }
        pp_add_include_path(pp, "/rinos/include");
    }
    /* C++ predefined macros */
    pp_define(pp, "__cplusplus",
              g_cxx_standard >= 20 ? "202002L" :
              g_cxx_standard >= 17 ? "201703L" :
              g_cxx_standard >= 14 ? "201402L" : "201103L");
    pp_define(pp, "__RCC__", "1");
    pp_define(pp, "__RCXX__", "1");
    pp_define(pp, "__STDC_HOSTED__", g_opts.freestanding ? "0" : "1");
    define_cxx_feature_test_macros(pp);
    for (int i = 0; i < g_opts.define_count; ++i) {
        const char* definition = g_opts.defines[i];
        const char* equals = strchr(definition, '=');
        if (equals) {
            char name[256];
            size_t length = (size_t)(equals - definition);
            if (length >= sizeof(name)) length = sizeof(name) - 1u;
            memcpy(name, definition, length);
            name[length] = '\0';
            pp_define(pp, name, equals + 1);
        } else {
            pp_define(pp, definition, "1");
        }
    }
    for (int i = 0; i < g_opts.undef_count; ++i) {
        pp_undef(pp, g_opts.undefines[i]);
    }
    if (g_opts.freestanding) {
        pp_define(pp, "__FREESTANDING__", "1");
    }

    char* pp_source = pp_process_file(pp, g_opts.input_file);
    if (!pp_source || g_error_count > 0) {
        fprintf(stderr, "rcc++: %d error(s) in preprocessing\n", g_error_count);
        rcc_free(pp_source);
        pp_free(pp);
        return 1;
    }
    if (g_opts.emit_dependencies) {
        if (g_opts.dependency_file[0] == '\0') {
            if (!rcc_derive_output_path(g_opts.output_file, ".d",
                                        g_opts.dependency_file,
                                        sizeof(g_opts.dependency_file))) {
                fprintf(stderr, "rcc++: error: derived dependency path is too long (maximum %u bytes)\n",
                        (unsigned)(sizeof(g_opts.dependency_file) - 1u));
                rcc_free(pp_source);
                pp_free(pp);
                return 1;
            }
        }
        if (!pp_write_dependencies(pp, g_opts.output_file, g_opts.input_file,
                                   g_opts.dependency_file)) {
            fprintf(stderr, "rcc++: error: cannot write dependency file: %s\n",
                    g_opts.dependency_file);
            rcc_free(pp_source);
            pp_free(pp);
            return 1;
        }
    }
    if (g_opts.preprocess_only) {
        printf("%s", pp_source);
        rcc_free(pp_source);
        pp_free(pp);
        return 0;
    }

    if (g_cxx_standard >= 20) {
        CxxModuleLoadContext module_context = {0};
        char* module_source = NULL;
        if (!cxx_module_prepare(pp_source, pp, g_opts.input_file,
                                &module_source, &module_pending,
                                &module_context)) {
            rcc_free(pp_source);
            pp_free(pp);
            cxx_module_pending_clear(&module_pending);
            return 1;
        }
        rcc_free(pp_source);
        pp_source = module_source;
        if (g_opts.emit_dependencies &&
            !pp_write_dependencies(pp, g_opts.output_file, g_opts.input_file,
                                   g_opts.dependency_file)) {
            fprintf(stderr, "rcc++: error: cannot update module dependencies: %s\n",
                    g_opts.dependency_file);
            rcc_free(pp_source);
            pp_free(pp);
            cxx_module_pending_clear(&module_pending);
            return 1;
        }
    }

    /* Phase 1: Lexical analysis */
    rcc_lexer_set_cxx_mode(true);
    rcc_lexer_set_cxx_standard(g_cxx_standard);
    if (g_opts.verbose) {
        printf("Lexing...\n");
    }
    TokenList* tokens = rcc_lex_string(pp_source, g_opts.input_file);
    if (g_error_count > 0) {
        fprintf(stderr, "rcc++: %d error(s) in lexical analysis\n", g_error_count);
        tokenlist_free(tokens);
        rcc_free(pp_source);
        pp_free(pp);
        cxx_module_pending_clear(&module_pending);
        return 1;
    }

    if (g_opts.verbose) {
        printf("Tokens: %d\n", tokens->count);
    }

    /* Phase 2: Parsing */
    if (g_opts.verbose) {
        printf("Parsing (C++ mode)...\n");
    }
    rcc_parser_set_cxx_standard(g_cxx_standard);
    AST* ast = rcc_parse_cxx(tokens);
    if (g_error_count > 0) {
        fprintf(stderr, "rcc++: %d error(s) in parsing\n", g_error_count);
        tokenlist_free(tokens);
        rcc_free(pp_source);
        pp_free(pp);
        cxx_module_pending_clear(&module_pending);
        return 1;
    }

    /* Count declarations */
    int decl_count = 0;
    for (DeclList* d = ast->decls; d; d = d->next) {
        decl_count++;
    }

    if (g_opts.verbose) {
        printf("AST: %d top-level declaration(s)\n", decl_count);
    }

    /* Phase 3: Semantic analysis */
    if (g_opts.verbose) {
        printf("Semantic analysis...\n");
    }
    if (!rcc_sema(ast)) {
        fprintf(stderr, "rcc++: %d error(s) in semantic analysis\n", g_error_count);
        tokenlist_free(tokens);
        rcc_free(pp_source);
        pp_free(pp);
        cxx_module_pending_clear(&module_pending);
        return 1;
    }
    if (g_opts.output_format == OUTPUT_DRV &&
        !rcc_validate_driver_policy(ast)) {
        fprintf(stderr, "rcc++: driver policy validation failed\n");
        tokenlist_free(tokens);
        rcc_free(pp_source);
        pp_free(pp);
        cxx_module_pending_clear(&module_pending);
        return 1;
    }
    if (g_opts.opt_level > 0) {
        if (g_opts.verbose) printf("Optimization (-O%d)...\n", g_opts.opt_level);
        rcc_optimize(ast);
    }

    /* Phase 4: Code generation */
    if (g_opts.verbose) {
        printf("Code generation (%s)...\n",
               g_opts.target_arch == ARCH_X64 ? "x86-64" : "x86-32");
    }
    Module* mod;
    if (g_opts.target_arch == ARCH_X64) {
        mod = rcc_codegen64(ast);
    } else {
        mod = rcc_codegen(ast);
    }
    if (!mod || g_error_count > 0) {
        fprintf(stderr, "rcc++: code generation failed with %d error(s)\n",
                g_error_count);
        if (mod) codegen_free(mod);
        tokenlist_free(tokens);
        rcc_free(pp_source);
        pp_free(pp);
        cxx_module_pending_clear(&module_pending);
        return 1;
    }

    if (g_opts.verbose) {
        printf("Generated: %lu bytes code, %lu bytes data\n",
               (unsigned long)mod->code.size, (unsigned long)mod->data.size);
    }

    /* Phase 5: Output */
    if (g_opts.verbose) {
        printf("Writing output...\n");
    }

    bool emit_ok = false;
    bool final_artifact = g_opts.output_format == OUTPUT_RIN ||
                          g_opts.output_format == OUTPUT_RLL ||
                          g_opts.output_format == OUTPUT_DRV;
    char unsigned_path[RCC_MAX_PATH + 64];
    const char* emit_path = g_opts.output_file;
    if (final_artifact && !g_opts.emit_unsigned_v3) {
        if (!rcc_create_signing_temp(g_opts.output_file, "rcc-unsigned",
                                     unsigned_path, sizeof(unsigned_path))) {
            perror("rcc++: cannot create unsigned staging file");
            codegen_free(mod);
            tokenlist_free(tokens);
            rcc_free(pp_source);
            pp_free(pp);
            cxx_module_pending_clear(&module_pending);
            return 1;
        }
        emit_path = unsigned_path;
    }
    switch (g_opts.output_format) {
        case OUTPUT_RIN:
            emit_ok = rcc_emit(mod, emit_path);
            break;
        case OUTPUT_RLL:
            emit_ok = rcc_emit_rll(mod, ast, emit_path);
            break;
        case OUTPUT_DRV:
            emit_ok = rcc_emit_drv(mod, ast, emit_path);
            break;
        case OUTPUT_OBJ:
            if (g_opts.verified_backend) {
                char reason[256];
                size_t verified_functions = 0u;
                RccVerifiedObjectStatus status = rcc_emit_verified_object(
                    ast, g_opts.input_file, emit_path,
                    &verified_functions, reason, sizeof(reason));
                if (status == RCC_VERIFIED_OBJECT_EMITTED) {
                    emit_ok = true;
                    if (g_opts.verbose) {
                        printf("Verified backend: %lu function(s) emitted\n",
                               (unsigned long)verified_functions);
                    }
                } else if (status == RCC_VERIFIED_OBJECT_FALLBACK) {
                    if (g_opts.verbose) {
                        printf("Verified backend fallback: %s\n", reason);
                    }
                    emit_ok = rcc_emit_obj(mod, emit_path);
                } else {
                    fprintf(stderr,
                            "rcc++: verified backend failed: %s\n", reason);
                }
            } else {
                emit_ok = rcc_emit_obj(mod, emit_path);
            }
            break;
        case OUTPUT_ASM:
            emit_ok = rcc_emit_asm(mod, emit_path);
            break;
        default:
            fprintf(stderr, "rcc++: unsupported output format\n");
            break;
    }
    if (final_artifact && !g_opts.emit_unsigned_v3) {
        if (emit_ok) emit_ok = rcc_run_rinsign(emit_path, g_opts.output_file);
        remove(emit_path);
    }

    if (emit_ok && !cxx_module_publish(&module_pending)) {
        fprintf(stderr, "rcc++: failed to publish C++ module interface '%s'\n",
                module_pending.path);
        emit_ok = false;
    }

    if (!emit_ok) {
        fprintf(stderr, "rcc++: failed to write output\n");
        codegen_free(mod);
        tokenlist_free(tokens);
        rcc_free(pp_source);
        pp_free(pp);
        cxx_module_pending_clear(&module_pending);
        return 1;
    }

    const char* fmt_name =
        g_opts.output_format == OUTPUT_RIN ? ".rin" :
        g_opts.output_format == OUTPUT_RLL ? ".rll" :
        g_opts.output_format == OUTPUT_DRV ? ".drv" :
        g_opts.output_format == OUTPUT_OBJ ? ".ro" :
        g_opts.output_format == OUTPUT_ASM ? ".s" : "?";

    printf("rcc++: compiled '%s' -> '%s' (%s, C++%d)\n",
           g_opts.input_file, g_opts.output_file, fmt_name, g_cxx_standard);
    printf("       %d tokens, %d declarations, %lu bytes\n",
           tokens->count, decl_count, (unsigned long)(mod->code.size + mod->data.size + 64));

    codegen_free(mod);
    tokenlist_free(tokens);
    rcc_free(pp_source);
    pp_free(pp);
    cxx_module_pending_clear(&module_pending);
    return 0;
}

int main(int argc, char** argv) {
    const char* extension;
    g_opts.cxx_mode = true;
    if (argc < 2) {
        print_usage_cxx();
        return 1;
    }
    if (parse_cxx_args(argc, argv) < 0) return 1;
    if (g_opts.input_count == 1) return compile_current_input(argv);

    for (int index = 0; index < g_opts.input_count; ++index) {
        if (!rcc_copy_path(g_opts.input_file, sizeof(g_opts.input_file),
                           g_opts.input_files[index])) {
            fprintf(stderr,
                    "rcc++: error: input path is too long (maximum %u bytes)\n",
                    (unsigned)(sizeof(g_opts.input_file) - 1u));
            return 1;
        }
        g_opts.output_file[0] = '\0';
        if (g_opts.output_format == OUTPUT_OBJ) extension = ".ro";
        else if (g_opts.output_format == OUTPUT_ASM) extension = ".s";
        else extension = ".out";
        if (!g_opts.preprocess_only &&
            !rcc_derive_output_path(g_opts.input_file, extension,
                                    g_opts.output_file,
                                    sizeof(g_opts.output_file))) {
            fprintf(stderr,
                    "rcc++: error: derived output path is too long (maximum %u bytes)\n",
                    (unsigned)(sizeof(g_opts.output_file) - 1u));
            return 1;
        }
        g_opts.dependency_file[0] = '\0';
        if (compile_current_input(argv) != 0) return 1;
    }
    return 0;
}
