/*
 * RCC - RinOS C Compiler
 * Preprocessor Implementation
 */

#include "preproc.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* Output buffer for preprocessed source */
typedef struct {
    char* data;
    size_t size;
    size_t capacity;
} PPBuffer;

#define PP_MAX_FILE_BYTES ((size_t)-1 - 1u)

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

static void pp_report_builtin_error(const char* message, const char* value) {
    rcc_error((SourceLoc){"<preprocessor>", 0, 0}, message, value);
}

static bool pp_get_build_timestamp(time_t* timestamp) {
    const char* source_date_epoch = getenv("SOURCE_DATE_EPOCH");

    if (!source_date_epoch || !source_date_epoch[0]) {
        time_t now = time(NULL);
        if (now == (time_t)-1) {
            rcc_error((SourceLoc){"<preprocessor>", 0, 0},
                      "cannot determine the current time for __DATE__/__TIME__");
            return false;
        }
        *timestamp = now;
        return true;
    }

    char* end = NULL;
    errno = 0;
    long long seconds = strtoll(source_date_epoch, &end, 10);
    if (errno == ERANGE || end == source_date_epoch || *end != '\0' ||
        seconds < 0) {
        pp_report_builtin_error("invalid SOURCE_DATE_EPOCH value '%s'", source_date_epoch);
        return false;
    }

    time_t converted = (time_t)seconds;
    if ((long long)converted != seconds) {
        pp_report_builtin_error("SOURCE_DATE_EPOCH value '%s' is outside the host time_t range",
                                source_date_epoch);
        return false;
    }
    *timestamp = converted;
    return true;
}

static bool pp_build_date_time_literals(char date_literal[12],
                                        char time_literal[9]) {
    static const char* months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    const char* source_date_epoch = getenv("SOURCE_DATE_EPOCH");
    time_t timestamp;
    struct tm* broken_down;
    int year;
    int date_length;
    int time_length;

    if (!pp_get_build_timestamp(&timestamp)) return false;
    broken_down = (source_date_epoch && source_date_epoch[0])
        ? gmtime(&timestamp) : localtime(&timestamp);
    if (!broken_down) {
        rcc_error((SourceLoc){"<preprocessor>", 0, 0},
                  "cannot convert the build timestamp for __DATE__/__TIME__");
        return false;
    }
    if (broken_down->tm_mon < 0 || broken_down->tm_mon >= 12 ||
        broken_down->tm_mday < 1 || broken_down->tm_mday > 31 ||
        broken_down->tm_hour < 0 || broken_down->tm_hour > 23 ||
        broken_down->tm_min < 0 || broken_down->tm_min > 59 ||
        broken_down->tm_sec < 0 || broken_down->tm_sec > 60 ||
        broken_down->tm_year < -1900 || broken_down->tm_year > 8099) {
        rcc_error((SourceLoc){"<preprocessor>", 0, 0},
                  "host returned an invalid build timestamp for __DATE__/__TIME__");
        return false;
    }

    year = broken_down->tm_year + 1900;
    date_length = snprintf(date_literal, 12, "%.3s %2d %04d",
                           months[broken_down->tm_mon],
                           broken_down->tm_mday, year);
    time_length = snprintf(time_literal, 9, "%02d:%02d:%02d",
                           broken_down->tm_hour, broken_down->tm_min,
                           broken_down->tm_sec);
    if (date_length != 11 || time_length != 8) {
        rcc_error((SourceLoc){"<preprocessor>", 0, 0},
                  "cannot format the build timestamp for __DATE__/__TIME__");
        return false;
    }
    return true;
}

/* Create new preprocessor */
Preprocessor* pp_new(void) {
    Preprocessor* pp = rcc_alloc(sizeof(Preprocessor));
    pp->macros = NULL;
    pp->include_paths = NULL;
    pp->include_path_count = 0;
    pp->include_depth = 0;
    pp->current_file = NULL;
    pp->cxx_mode = false;
    pp->cxx_standard = 0;
    pp->cond_depth = 0;
    pp->expansion_depth = 0;
    pp->dependencies = NULL;
    pp->dependency_count = 0;
    pp->dependency_capacity = 0;
    pp->once_files = NULL;
    pp->once_file_count = 0;
    pp->once_file_capacity = 0;

    /* Define built-in macros */
    pp_define(pp, "__RCC__", "1");
    pp_define(pp, "__RINOS__", "1");
    /* RinOS lowers the distinct long double type with binary64 size and
     * calling convention.  Let libc expose the conversion and printf/scanf
     * APIs that use that ABI instead of withholding them as unsupported. */
    pp_define(pp, "RINOS_ABI_HAS_LONG_DOUBLE", "1");
    pp_define(pp, "__STDC__", "1");
    pp_define(pp, "__STDC_VERSION__",
              g_opts.c_standard >= 23 ? "202311L" : "201710L");
    if (g_opts.c_standard >= 23) {
        pp_define(pp, "__BITINT_MAXWIDTH__", "64");
        pp_define(pp, "__STDC_EMBED_NOT_FOUND__", "0");
        pp_define(pp, "__STDC_EMBED_FOUND__", "1");
        pp_define(pp, "__STDC_EMBED_EMPTY__", "2");
    }
    char date_literal[12];
    char time_literal[9];
    if (pp_build_date_time_literals(date_literal, time_literal)) {
        char date_macro[14];
        char time_macro[11];
        snprintf(date_macro, sizeof(date_macro), "\"%s\"", date_literal);
        snprintf(time_macro, sizeof(time_macro), "\"%s\"", time_literal);
        pp_define(pp, "__DATE__", date_macro);
        pp_define(pp, "__TIME__", time_macro);
    }
    pp_define(pp, "__ATOMIC_RELAXED", "0");
    pp_define(pp, "__ATOMIC_CONSUME", "1");
    pp_define(pp, "__ATOMIC_ACQUIRE", "2");
    pp_define(pp, "__ATOMIC_RELEASE", "3");
    pp_define(pp, "__ATOMIC_ACQ_REL", "4");
    pp_define(pp, "__ATOMIC_SEQ_CST", "5");
    /* Match the RinOS data model used by libc/stdint.h and libcxx headers. */
    pp_define(pp, "__INT8_TYPE__", "signed char");
    pp_define(pp, "__UINT8_TYPE__", "unsigned char");
    pp_define(pp, "__INT16_TYPE__", "short");
    pp_define(pp, "__UINT16_TYPE__", "unsigned short");
    pp_define(pp, "__INT32_TYPE__", "int");
    pp_define(pp, "__UINT32_TYPE__", "unsigned int");
    pp_define(pp, "__INT64_TYPE__", "long long");
    pp_define(pp, "__UINT64_TYPE__", "unsigned long long");
    pp_define(pp, "__INT_FAST8_TYPE__", "signed char");
    pp_define(pp, "__UINT_FAST8_TYPE__", "unsigned char");
    pp_define(pp, "__INT_FAST16_TYPE__", "int");
    pp_define(pp, "__UINT_FAST16_TYPE__", "unsigned int");
    pp_define(pp, "__INT_FAST32_TYPE__", "int");
    pp_define(pp, "__UINT_FAST32_TYPE__", "unsigned int");
    pp_define(pp, "__INT_FAST64_TYPE__", "long long");
    pp_define(pp, "__UINT_FAST64_TYPE__", "unsigned long long");
    pp_define(pp, "__INTMAX_TYPE__", "long long");
    pp_define(pp, "__UINTMAX_TYPE__", "unsigned long long");
    pp_define(pp, "__CHAR16_TYPE__", "unsigned short");
    pp_define(pp, "__CHAR32_TYPE__", "unsigned int");
    pp_define(pp, "__WCHAR_TYPE__", "int");
    pp_define(pp, "__CHAR_BIT__", "8");
    pp_define(pp, "__SCHAR_MAX__", "127");
    pp_define(pp, "__SHRT_MAX__", "32767");
    pp_define(pp, "__INT_MAX__", "2147483647");
    pp_define(pp, "__LONG_LONG_MAX__", "9223372036854775807LL");
    pp_define(pp, "__DBL_MIN__",
              "((double)2.22507385850720138309023271733240406e-308L)");
    pp_define(pp, "__INT8_MAX__", "127");
    pp_define(pp, "__UINT8_MAX__", "255");
    pp_define(pp, "__INT16_MAX__", "32767");
    pp_define(pp, "__UINT16_MAX__", "65535");
    pp_define(pp, "__INT32_MAX__", "2147483647");
    pp_define(pp, "__UINT32_MAX__", "4294967295U");
    pp_define(pp, "__INT64_MAX__", "9223372036854775807LL");
    pp_define(pp, "__UINT64_MAX__", "18446744073709551615ULL");
    pp_define(pp, "__INT_FAST8_MAX__", "127");
    pp_define(pp, "__UINT_FAST8_MAX__", "255");
    pp_define(pp, "__INT_FAST16_MAX__", "2147483647");
    pp_define(pp, "__UINT_FAST16_MAX__", "4294967295U");
    pp_define(pp, "__INT_FAST32_MAX__", "2147483647");
    pp_define(pp, "__UINT_FAST32_MAX__", "4294967295U");
    pp_define(pp, "__INT_FAST64_MAX__", "9223372036854775807LL");
    pp_define(pp, "__UINT_FAST64_MAX__", "18446744073709551615ULL");
    pp_define(pp, "__WCHAR_MAX__", "2147483647");
    pp_define(pp, "__SIZEOF_SHORT__", "2");
    pp_define(pp, "__SIZEOF_INT__", "4");
    pp_define(pp, "__SIZEOF_LONG_LONG__", "8");
    pp_define(pp, "__SIZEOF_WCHAR_T__", "4");
    pp_define(pp, "__ORDER_LITTLE_ENDIAN__", "1234");
    pp_define(pp, "__ORDER_BIG_ENDIAN__", "4321");
    pp_define(pp, "__ORDER_PDP_ENDIAN__", "3412");
    pp_define(pp, "__BYTE_ORDER__", "1234");
    if (g_opts.sse_enabled) pp_define(pp, "__SSE__", "1");
    if (g_opts.sse_enabled && g_opts.sse2_enabled) {
        pp_define(pp, "__SSE2__", "1");
    }

    /* Architecture */
    if (g_opts.target_arch == ARCH_X86) {
        pp_define(pp, "__i386__", "1");
        pp_define(pp, "__i386", "1");
        pp_define(pp, "__SIZE_TYPE__", "unsigned int");
        pp_define(pp, "__PTRDIFF_TYPE__", "int");
        pp_define(pp, "__INTPTR_TYPE__", "int");
        pp_define(pp, "__UINTPTR_TYPE__", "unsigned int");
        pp_define(pp, "__LONG_MAX__", "2147483647L");
        pp_define(pp, "__SIZE_MAX__", "4294967295U");
        pp_define(pp, "__INTPTR_MAX__", "2147483647");
        pp_define(pp, "__UINTPTR_MAX__", "4294967295U");
        pp_define(pp, "__PTRDIFF_MAX__", "2147483647");
        pp_define(pp, "__SIZEOF_LONG__", "4");
        pp_define(pp, "__SIZEOF_POINTER__", "4");
        pp_define(pp, "__SIZEOF_SIZE_T__", "4");
    } else {
        pp_define(pp, "__x86_64__", "1");
        pp_define(pp, "__amd64__", "1");
        pp_define(pp, "__SIZE_TYPE__", "unsigned long long");
        pp_define(pp, "__PTRDIFF_TYPE__", "long long");
        pp_define(pp, "__INTPTR_TYPE__", "long long");
        pp_define(pp, "__UINTPTR_TYPE__", "unsigned long long");
        pp_define(pp, "__LONG_MAX__", "9223372036854775807L");
        pp_define(pp, "__SIZE_MAX__", "18446744073709551615ULL");
        pp_define(pp, "__INTPTR_MAX__", "9223372036854775807LL");
        pp_define(pp, "__UINTPTR_MAX__", "18446744073709551615ULL");
        pp_define(pp, "__PTRDIFF_MAX__", "9223372036854775807LL");
        pp_define(pp, "__SIZEOF_LONG__", "8");
        pp_define(pp, "__SIZEOF_POINTER__", "8");
        pp_define(pp, "__SIZEOF_SIZE_T__", "8");
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
    for (int i = 0; i < pp->once_file_count; ++i) {
        rcc_free((void*)pp->once_files[i]);
    }
    rcc_free(pp->once_files);

    rcc_free(pp);
}

static void pp_normalize_once_path(const char* filename, char* normalized,
                                   size_t capacity) {
    size_t length = 0;
    size_t root_length = 0;
    size_t component_starts[RCC_MAX_PATH];
    size_t component_count = 0;
    const char* cursor = filename;
    bool absolute = false;

    if (!filename || !normalized || capacity == 0) {
        rcc_fatal("invalid #pragma once path");
    }

    if (*cursor == '/' || *cursor == '\\') {
        absolute = true;
        normalized[length++] = '/';
        root_length = length;
        while (*cursor == '/' || *cursor == '\\') ++cursor;
    } else if (isalpha((unsigned char)cursor[0]) && cursor[1] == ':' &&
               (cursor[2] == '/' || cursor[2] == '\\')) {
        absolute = true;
        normalized[length++] = cursor[0];
        normalized[length++] = ':';
        normalized[length++] = '/';
        root_length = length;
        cursor += 3;
        while (*cursor == '/' || *cursor == '\\') ++cursor;
    }

    while (*cursor) {
        const char* component = cursor;
        size_t component_length;

        while (*cursor == '/' || *cursor == '\\') ++cursor;
        if (!*cursor) break;
        component = cursor;
        while (*cursor && *cursor != '/' && *cursor != '\\') ++cursor;
        component_length = (size_t)(cursor - component);

        if (component_length == 1 && component[0] == '.') {
            continue;
        }
        if (component_length == 2 && component[0] == '.' &&
            component[1] == '.') {
            if (component_count > 0) {
                size_t previous_start =
                    component_starts[component_count - 1];
                size_t previous_length = length - previous_start;
                bool previous_is_parent = previous_length == 2 &&
                    normalized[previous_start] == '.' &&
                    normalized[previous_start + 1] == '.';
                if (!previous_is_parent) {
                    length = previous_start == root_length
                        ? root_length : previous_start - 1;
                    --component_count;
                    continue;
                }
            }
            if (absolute) continue;
        }

        if (component_count == RCC_MAX_PATH) {
            rcc_fatal("#pragma once path has too many components");
        }
        if (length > root_length && normalized[length - 1] != '/') {
            if (length + 1 >= capacity) {
                rcc_fatal("#pragma once path is too long");
            }
            normalized[length++] = '/';
        }
        if (length + component_length >= capacity) {
            rcc_fatal("#pragma once path is too long");
        }
        component_starts[component_count++] = length;
        memcpy(normalized + length, component, component_length);
        length += component_length;
    }

    if (length == 0) {
        normalized[length++] = '.';
    }
    normalized[length] = '\0';
}

static bool pp_once_same_path(const char* left, const char* right) {
    char normalized_left[RCC_MAX_PATH];
    char normalized_right[RCC_MAX_PATH];
    pp_normalize_once_path(left, normalized_left, sizeof(normalized_left));
    pp_normalize_once_path(right, normalized_right, sizeof(normalized_right));
    return strcmp(normalized_left, normalized_right) == 0;
}

static bool pp_once_contains(const Preprocessor* pp, const char* filename) {
    if (!pp || !filename) return false;
    for (int i = 0; i < pp->once_file_count; ++i) {
        if (pp_once_same_path(pp->once_files[i], filename)) return true;
    }
    return false;
}

static void pp_mark_once(Preprocessor* pp, const char* filename) {
    if (!pp || !filename || pp_once_contains(pp, filename)) return;
    if (pp->once_file_count == pp->once_file_capacity) {
        int capacity = pp->once_file_capacity == 0
            ? 8 : pp->once_file_capacity * 2;
        pp->once_files = rcc_realloc(
            pp->once_files, (size_t)capacity * sizeof(*pp->once_files));
        pp->once_file_capacity = capacity;
    }
    pp->once_files[pp->once_file_count++] = rcc_strdup(filename);
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
    if (name && (strcmp(name, "__has_builtin") == 0 ||
                 strcmp(name, "__has_include") == 0 ||
                 strcmp(name, "__has_include_next") == 0 ||
                 (!pp->cxx_mode && g_opts.c_standard >= 23 &&
                  (strcmp(name, "__has_embed") == 0 ||
                   strcmp(name, "__has_c_attribute") == 0)))) return true;
    return pp_get_macro(pp, name) != NULL;
}

/* Keep this feature query aligned with builtins that have semantic lowering
 * in the C/C++ frontends. Unknown names are not advertised. */
static bool pp_has_builtin(const char* name) {
    static const char* const builtins[] = {
        "__builtin_add_overflow", "__builtin_add_overflow_p",
        "__builtin_assume_aligned",
        "__builtin_bswap16", "__builtin_bswap32", "__builtin_bswap64",
        "__builtin_clear_padding",
        "__builtin_choose_expr", "__builtin_clrsb", "__builtin_clrsbl",
        "__builtin_clrsbll", "__builtin_clz", "__builtin_clzl",
        "__builtin_clzll", "__builtin_constant_p", "__builtin_ctz",
        "__builtin_ctzl", "__builtin_ctzll", "__builtin_expect",
        "__builtin_expect_with_probability", "__builtin_ffs",
        "__builtin_ffsl", "__builtin_ffsll", "__builtin_huge_val",
        "__builtin_huge_valf", "__builtin_ia32_movntq",
        "__builtin_ia32_packssdw", "__builtin_ia32_packsswb",
        "__builtin_ia32_packuswb", "__builtin_ia32_paddb",
        "__builtin_ia32_paddd", "__builtin_ia32_paddsb",
        "__builtin_ia32_paddsw", "__builtin_ia32_paddusb",
        "__builtin_ia32_paddusw", "__builtin_ia32_paddw",
        "__builtin_ia32_pand", "__builtin_ia32_pandn",
        "__builtin_ia32_pcmpeqb", "__builtin_ia32_pcmpeqd",
        "__builtin_ia32_pcmpeqw", "__builtin_ia32_pcmpgtb",
        "__builtin_ia32_pcmpgtd", "__builtin_ia32_pcmpgtw",
        "__builtin_ia32_pmaddwd", "__builtin_ia32_pmulhw",
        "__builtin_ia32_pmullw", "__builtin_ia32_por",
        "__builtin_ia32_pslld", "__builtin_ia32_pslldi",
        "__builtin_ia32_psllq", "__builtin_ia32_psllqi",
        "__builtin_ia32_psllw", "__builtin_ia32_psllwi",
        "__builtin_ia32_psrad", "__builtin_ia32_psradi",
        "__builtin_ia32_psraw", "__builtin_ia32_psrawi",
        "__builtin_ia32_psrld", "__builtin_ia32_psrldi",
        "__builtin_ia32_psrlq", "__builtin_ia32_psrlqi",
        "__builtin_ia32_psrlw", "__builtin_ia32_psrlwi",
        "__builtin_ia32_psubb", "__builtin_ia32_psubd",
        "__builtin_ia32_psubsb", "__builtin_ia32_psubsw",
        "__builtin_ia32_psubusw",
        "__builtin_ia32_psubusb", "__builtin_ia32_psubw",
        "__builtin_ia32_punpckhbw", "__builtin_ia32_punpckhdq",
        "__builtin_ia32_punpckhwd", "__builtin_ia32_punpcklbw",
        "__builtin_ia32_punpckldq", "__builtin_ia32_punpcklwd",
        "__builtin_ia32_pxor", "__builtin_ia32_vec_init_v2si",
        "__builtin_ia32_vec_init_v4hi", "__builtin_ia32_vec_init_v8qi",
        "__builtin_inf", "__builtin_inff",
        "__builtin_mul_overflow", "__builtin_mul_overflow_p",
        "__builtin_nan", "__builtin_nanf",
        "__builtin_object_size", "__builtin_offsetof", "__builtin_parity",
        "__builtin_parityl", "__builtin_parityll", "__builtin_popcount",
        "__builtin_popcountl", "__builtin_popcountll", "__builtin_prefetch",
        "__builtin_strlen", "__builtin_sub_overflow",
        "__builtin_sub_overflow_p", "__builtin_trap",
        "__builtin_types_compatible_p", "__builtin_unreachable",
        "__builtin_va_arg", "__builtin_va_copy", "__builtin_va_end",
        "__builtin_va_list", "__builtin_va_start",
        "__atomic_add_fetch", "__atomic_always_lock_free",
        "__atomic_and_fetch", "__atomic_clear", "__atomic_compare_exchange",
        "__atomic_compare_exchange_n", "__atomic_exchange",
        "__atomic_exchange_n", "__atomic_fetch_add", "__atomic_fetch_and",
        "__atomic_fetch_max", "__atomic_fetch_min", "__atomic_fetch_nand",
        "__atomic_fetch_or", "__atomic_fetch_sub", "__atomic_fetch_udec",
        "__atomic_fetch_uinc", "__atomic_fetch_xor", "__atomic_is_lock_free",
        "__atomic_load", "__atomic_load_n", "__atomic_nand_fetch",
        "__atomic_or_fetch", "__atomic_signal_fence", "__atomic_store",
        "__atomic_store_n", "__atomic_sub_fetch", "__atomic_test_and_set",
        "__atomic_thread_fence", "__atomic_xor_fetch",
        "__has_nothrow_assign", "__has_nothrow_copy",
        "__has_nothrow_constructor",
        "__has_trivial_constructor",
        "__has_trivial_copy", "__has_trivial_destructor",
        "__has_trivial_assign",
        "__is_assignable",
        "__is_nothrow_assignable",
        "__is_trivially_assignable",
        "__is_constructible", "__is_nothrow_constructible",
        "__is_trivially_constructible",
        "__has_virtual_destructor",
        "__is_abstract", "__is_aggregate",
        "__is_base_of", "__is_class", "__is_trivial",
        "__is_empty", "__is_enum", "__is_final", "__is_polymorphic",
        "__is_function",
        "__is_pod", "__is_standard_layout", "__is_trivially_copyable",
        "__is_trivially_destructible",
        "__is_union", "__underlying_type"
    };
    size_t index;

    if (!name) return false;
    for (index = 0; index < sizeof(builtins) / sizeof(builtins[0]); ++index) {
        if (strcmp(name, builtins[index]) == 0) return true;
    }
    return false;
}

Macro* pp_get_macro(Preprocessor* pp, const char* name) {
    for (Macro* m = pp->macros; m; m = m->next) {
        if (strcmp(m->name, name) == 0) {
            return m;
        }
    }
    return NULL;
}

/* Skip horizontal whitespace and the CR in a CRLF line ending. */
static const char* skip_ws(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\r') p++;
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

/* Read an entire file into memory while preserving its byte count.  Source
 * files still consume the trailing NUL sentinel; binary resources use the
 * reported length so embedded zero bytes are not mistaken for EOF. */
static char* read_file_with_size(const char* filename, size_t max_size,
                                size_t* size_out) {
    if (size_out) *size_out = 0u;
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        rcc_fatal("cannot seek include file '%s'", filename);
        return NULL;
    }
    long file_size = ftell(f);
    if (file_size < 0 || fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        rcc_fatal("cannot determine include file size '%s'", filename);
        return NULL;
    }

    size_t size = (size_t)file_size;
    if ((long)size != file_size || size == (size_t)-1) {
        fclose(f);
        rcc_fatal("include file is too large '%s'", filename);
        return NULL;
    }
    if (size > max_size) size = max_size;

    char* buf = rcc_alloc(size + 1u);
    if (fread(buf, 1, size, f) != size) {
        rcc_free(buf);
        fclose(f);
        rcc_fatal("cannot read include file '%s'", filename);
        return NULL;
    }
    buf[size] = '\0';
    fclose(f);
    if (size_out) *size_out = size;
    return buf;
}

static char* read_file(const char* filename) {
    return read_file_with_size(filename, PP_MAX_FILE_BYTES, NULL);
}

/* C17/C++20 translation phase 1 maps the supported physical line endings to
 * a single newline before splicing, comment removal, or literal lexing.  Do
 * this for every preprocessing entry point, including pp_process_string(),
 * so raw string contents and diagnostics are independent of the checkout's
 * CRLF/LF convention. */
static char* normalize_source_newlines(const char* source) {
    size_t length;
    size_t read_index = 0;
    size_t write_index = 0;
    char* normalized;

    if (!source) return NULL;
    length = strlen(source);
    normalized = rcc_alloc(length + 1u);
    /* A UTF-8 BOM is permitted at the beginning of each source file.  Strip
     * it before preprocessing so included text cannot leak the three bytes
     * into the token stream. */
    if (length >= 3u &&
        (unsigned char)source[0] == 0xEFu &&
        (unsigned char)source[1] == 0xBBu &&
        (unsigned char)source[2] == 0xBFu) {
        read_index = 3u;
    }
    for (; read_index < length; ++read_index) {
        if (source[read_index] == '\r') {
            if (read_index + 1u < length &&
                source[read_index + 1u] == '\n') {
                ++read_index;
            }
            normalized[write_index++] = '\n';
        } else {
            normalized[write_index++] = source[read_index];
        }
    }
    normalized[write_index] = '\0';
    return normalized;
}

/* Find include file */
static bool include_path_contains_file(const char* include_path,
                                       const char* current_file) {
    size_t path_length;

    if (!include_path || !current_file || !*include_path) {
        return false;
    }
    if (strcmp(include_path, ".") == 0) {
        if (current_file[0] == '/' || current_file[0] == '\\' ||
            (isalpha((unsigned char)current_file[0]) &&
             current_file[1] == ':')) {
            return false;
        }
        return true;
    }
    path_length = strlen(include_path);
    if (strncmp(include_path, current_file, path_length) != 0) {
        return false;
    }
    return current_file[path_length] == '/' ||
           current_file[path_length] == '\\';
}

/* Find an include file.  For #include_next, resume after the include-path
 * directory that supplied the current file.  The current-file directory and
 * final working-directory fallback are intentionally skipped so a wrapper
 * cannot include itself again through an accidental search restart. */
static char* find_include(Preprocessor* pp, const char* name, const char* current_file,
                          bool is_system, bool include_next,
                          char* resolved, size_t resolved_size,
                          size_t max_content_size,
                          size_t* content_size) {
    char path[RCC_MAX_PATH];
    int include_path_start = 0;
    int include_path_match = -1;
    size_t include_path_match_length = 0u;
    bool include_path_match_is_dot = false;

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

    if (include_next && current_file) {
        for (int i = 0; i < pp->include_path_count; ++i) {
            const char* include_path = pp->include_paths[i];
            bool is_dot = strcmp(include_path, ".") == 0;
            size_t path_length = is_dot ? 0u : strlen(include_path);
            if (!include_path_contains_file(include_path, current_file)) {
                continue;
            }
            /* A relative current_file also lies beneath ".". Treat that
             * entry only as a fallback; otherwise use the most specific
             * matching directory so an earlier -I wrapper can include_next
             * into the following user directory. Keep the first duplicate
             * directory, which is the one normal lookup searched first. */
            if (include_path_match < 0 ||
                (include_path_match_is_dot && !is_dot) ||
                (!is_dot && !include_path_match_is_dot &&
                 path_length > include_path_match_length)) {
                include_path_match = i;
                include_path_match_length = path_length;
                include_path_match_is_dot = is_dot;
            }
        }
        if (include_path_match >= 0) {
            include_path_start = include_path_match + 1;
        }
    }

    /* For quoted includes, first search relative to current file. */
    if (!include_next && !is_system && current_file) {
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
            char* content = read_file_with_size(path, max_content_size,
                                                content_size);
            if (content) {
                WRITE_RESOLVED_PATH(path);
                return content;
            }
        }
    }

    /* Search include paths */
    for (int i = include_path_start; i < pp->include_path_count; i++) {
        size_t dir_len = strlen(pp->include_paths[i]);
        size_t name_len = strlen(name);
        if (dir_len >= sizeof(path) - 1u ||
            name_len > sizeof(path) - dir_len - 2u) {
            rcc_fatal("include search path is too long");
        }
        memcpy(path, pp->include_paths[i], dir_len);
        path[dir_len] = '/';
        memcpy(path + dir_len + 1u, name, name_len + 1u);
        char* content = read_file_with_size(path, max_content_size,
                                            content_size);
        if (content) {
            WRITE_RESOLVED_PATH(path);
            return content;
        }
    }

    /* Try current directory for ordinary includes only. */
    if (include_next) return NULL;
    if (strlen(name) >= sizeof(path)) {
        rcc_fatal("include path is too long");
    }
    char* content = read_file_with_size(name, max_content_size, content_size);
    if (content) {
        WRITE_RESOLVED_PATH(name);
        return content;
    }

#undef WRITE_RESOLVED_PATH
    return NULL;
}

/* Probe the same search order as #include without recording a dependency or
 * reporting an error. Missing headers are valid false results; malformed
 * syntax is handled by the #if expression parser below. */
static bool pp_has_include(Preprocessor* pp, const char* name,
                           bool is_system, bool include_next) {
    char resolved[RCC_MAX_PATH];
    char* content = find_include(pp, name, pp->current_file, is_system,
                                 include_next, resolved, sizeof(resolved),
                                 0u, NULL);
    if (!content) return false;
    rcc_free(content);
    return true;
}

/* Return the feature-test value for a standard attribute that RCC++ accepts.
 * Warning-only and branch-hint attributes are semantically inert in this
 * warning-light frontend, but their syntax and standard-version rules are
 * checked by the parser.  Unknown attributes remain zero so portable headers
 * can select a fallback instead of assuming support. */
static long pp_cpp_attribute_value(const Preprocessor* pp,
                                  const char* attribute) {
    if (!pp || !pp->cxx_mode || !attribute) return 0;
    if (strcmp(attribute, "nodiscard") == 0 && pp->cxx_standard >= 17) {
        return pp->cxx_standard >= 20 ? 201907L : 201603L;
    }
    if (strcmp(attribute, "deprecated") == 0 && pp->cxx_standard >= 14) {
        return 201309L;
    }
    if ((strcmp(attribute, "maybe_unused") == 0 ||
         strcmp(attribute, "fallthrough") == 0) && pp->cxx_standard >= 17) {
        return 201603L;
    }
    if ((strcmp(attribute, "likely") == 0 ||
         strcmp(attribute, "unlikely") == 0) && pp->cxx_standard >= 20) {
        return 201803L;
    }
    if (strcmp(attribute, "no_unique_address") == 0 &&
        pp->cxx_standard >= 20) {
        return 201803L;
    }
    return 0;
}

static long pp_c_attribute_value(const Preprocessor* pp,
                                const char* attribute) {
    if (!pp || pp->cxx_mode || g_opts.c_standard < 23 || !attribute) {
        return 0;
    }
    if (strcmp(attribute, "noreturn") == 0 ||
        strcmp(attribute, "_Noreturn") == 0 ||
        strcmp(attribute, "__noreturn__") == 0) {
        return 202311L;
    }
    if (strcmp(attribute, "nodiscard") == 0) return 202311L;
    if (strcmp(attribute, "deprecated") == 0) return 202311L;
    if (strcmp(attribute, "maybe_unused") == 0) return 202311L;
    if (strcmp(attribute, "fallthrough") == 0) return 202311L;
    if (strcmp(attribute, "reproducible") == 0) return 202311L;
    if (strcmp(attribute, "unsequenced") == 0) return 202311L;
    return 0;
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

typedef struct {
    char resource_name[RCC_MAX_PATH];
    bool is_system;
    bool has_limit;
    uint64_t limit;
    bool has_prefix;
    const char* prefix;
    size_t prefix_length;
    bool has_suffix;
    const char* suffix;
    size_t suffix_length;
    bool has_if_empty;
    const char* if_empty;
    size_t if_empty_length;
    bool has_unsupported_parameter;
    bool has_unrecognized_parameter;
} PPEmbedSpec;

static int pp_probe_embed_expression(Preprocessor* pp, const char* open_paren,
                                     const char** after, bool* valid);
static long pp_probe_c_attribute_expression(Preprocessor* pp,
                                           const char* open_paren,
                                           const char** after, bool* valid);

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
        {
            bool saw_unsigned = false;
            bool saw_bitint = false;
            unsigned int long_count = 0u;
            while (*p == 'u' || *p == 'U' || *p == 'l' || *p == 'L' ||
                   *p == 'w' || *p == 'W' || *p == 'b' || *p == 'B') {
                if (*p == 'u' || *p == 'U') {
                    if (saw_unsigned) parser->invalid = true;
                    saw_unsigned = true;
                    is_unsigned = true;
                    ++p;
                } else if (*p == 'l' || *p == 'L') {
                    char long_mark = *p++;
                    if (long_count != 0u) parser->invalid = true;
                    long_count = 1u;
                    if (*p == long_mark) {
                        ++p;
                        long_count = 2u;
                    }
                } else if ((*p == 'w' || *p == 'W') &&
                           p[1] == (*p == 'w' ? 'b' : 'B')) {
                    if (saw_bitint || long_count != 0u) {
                        parser->invalid = true;
                    }
                    saw_bitint = true;
                    p += 2;
                } else {
                    parser->invalid = true;
                    ++p;
                }
            }
            if (saw_bitint && g_opts.c_standard < 23) {
                parser->invalid = true;
            }
            if (saw_bitint && !is_unsigned &&
                value > (uint64_t)INT64_MAX) {
                parser->invalid = true;
            }
        }
        parser->input = p;
        if (is_unsigned || value > (uint64_t)INT64_MAX) return pp_expr_unsigned(value);
        return pp_expr_signed((int64_t)value);
    }
    if (isalpha((unsigned char)*p) || *p == '_') {
        char name[RCC_MAX_IDENT];
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
            char ident[RCC_MAX_IDENT];
            const char* end = read_ident(p, ident, sizeof(ident));
            if (strcmp(ident, "defined") == 0) {
                const char* operand = pp_expr_skip(end);
                bool parenthesized = *operand == '(';
                if (parenthesized) operand = pp_expr_skip(operand + 1);
                char name[RCC_MAX_IDENT];
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
            if (strcmp(ident, "__has_builtin") == 0) {
                const char* cursor = pp_expr_skip(end);
                char builtin_name[RCC_MAX_IDENT];
                const char* name_end;
                bool probe_valid = *cursor == '(';

                if (probe_valid) {
                    cursor = pp_expr_skip(cursor + 1);
                    name_end = read_ident(cursor, builtin_name,
                                          sizeof(builtin_name));
                    probe_valid = name_end != cursor;
                    if (probe_valid) {
                        cursor = pp_expr_skip(name_end);
                        probe_valid = *cursor == ')';
                        if (probe_valid) cursor++;
                    }
                }
                if (!probe_valid) {
                    *valid = false;
                    buf_append_str(&protected, "0");
                    p = end;
                    continue;
                }
                buf_append_str(&protected,
                               pp_has_builtin(builtin_name) ? "1" : "0");
                p = cursor;
                continue;
            }
            if (strcmp(ident, "__has_include") == 0 ||
                strcmp(ident, "__has_include_next") == 0) {
                bool include_next =
                    strcmp(ident, "__has_include_next") == 0;
                const char* cursor = pp_expr_skip(end);
                char include_name[RCC_MAX_PATH];
                size_t include_length = 0u;
                bool is_system = false;
                bool include_valid = true;

                if (*cursor != '(') {
                    include_valid = false;
                } else {
                    cursor = pp_expr_skip(cursor + 1);
                    if (*cursor == '<') {
                        is_system = true;
                        cursor++;
                        while (*cursor && *cursor != '>' && *cursor != '\n' &&
                               *cursor != '\r') {
                            if (include_length + 1u >= sizeof(include_name)) {
                                include_valid = false;
                                break;
                            }
                            include_name[include_length++] = *cursor++;
                        }
                        if (*cursor != '>') include_valid = false;
                        if (include_valid) cursor++;
                    } else if (*cursor == '"') {
                        bool closed = false;
                        const char* after = read_string(
                            cursor, include_name, sizeof(include_name), '"',
                            &closed);
                        include_length = strlen(include_name);
                        if (!closed) include_valid = false;
                        cursor = after;
                    } else {
                        include_valid = false;
                    }
                    cursor = pp_expr_skip(cursor);
                    if (*cursor != ')') include_valid = false;
                    else cursor++;
                }

                if (!include_valid || include_length == 0u) {
                    *valid = false;
                    buf_append_str(&protected, "0");
                    p = end;
                    continue;
                }
                buf_append_str(&protected,
                               pp_has_include(pp, include_name, is_system,
                                              include_next)
                                   ? "1" : "0");
                p = cursor;
                continue;
            }
            if (strcmp(ident, "__has_embed") == 0 &&
                !pp->cxx_mode && g_opts.c_standard >= 23) {
                const char* cursor = pp_expr_skip(end);
                bool probe_valid = true;
                int value = pp_probe_embed_expression(
                    pp, cursor, &cursor, &probe_valid);
                if (!probe_valid) {
                    *valid = false;
                    buf_append_str(&protected, "0");
                    p = end;
                    continue;
                }
                char value_text[16];
                snprintf(value_text, sizeof(value_text), "%d", value);
                buf_append_str(&protected, value_text);
                p = cursor;
                continue;
            }
            if (strcmp(ident, "__has_c_attribute") == 0 &&
                !pp->cxx_mode && g_opts.c_standard >= 23) {
                const char* cursor = pp_expr_skip(end);
                bool probe_valid = true;
                long value = pp_probe_c_attribute_expression(
                    pp, cursor, &cursor, &probe_valid);
                if (!probe_valid) {
                    *valid = false;
                    buf_append_str(&protected, "0");
                    p = end;
                    continue;
                }
                char value_text[32];
                snprintf(value_text, sizeof(value_text), "%ld", value);
                buf_append_str(&protected, value_text);
                p = cursor;
                continue;
            }
            if (strcmp(ident, "__has_cpp_attribute") == 0) {
                const char* cursor = pp_expr_skip(end);
                char attribute[256];
                size_t attribute_length = 0u;
                bool probe_valid = true;

                if (*cursor != '(') {
                    probe_valid = false;
                } else {
                    cursor = pp_expr_skip(cursor + 1);
                    while (isalpha((unsigned char)*cursor) ||
                           isdigit((unsigned char)*cursor) ||
                           *cursor == '_') {
                        char component[64];
                        const char* after = read_ident(
                            cursor, component, sizeof(component));
                        size_t component_length = strlen(component);
                        if (attribute_length + component_length + 1u >=
                            sizeof(attribute)) {
                            probe_valid = false;
                            break;
                        }
                        memcpy(attribute + attribute_length, component,
                               component_length);
                        attribute_length += component_length;
                        cursor = after;
                        if (cursor[0] == ':' && cursor[1] == ':') {
                            attribute[attribute_length++] = ':';
                            attribute[attribute_length++] = ':';
                            cursor = pp_expr_skip(cursor + 2);
                        } else {
                            break;
                        }
                    }
                    attribute[attribute_length] = '\0';
                    cursor = pp_expr_skip(cursor);
                    if (*cursor != ')' || attribute_length == 0u) {
                        probe_valid = false;
                    } else {
                        cursor++;
                    }
                }

                if (!probe_valid) {
                    *valid = false;
                    buf_append_str(&protected, "0");
                    p = end;
                    continue;
                }
                char value[32];
                snprintf(value, sizeof(value), "%ld",
                         pp_cpp_attribute_value(pp, attribute));
                buf_append_str(&protected, value);
                p = cursor;
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

static PPExprValue pp_eval_integer_expression(Preprocessor* pp,
                                             const char* expression,
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
    return value;
}

static bool pp_eval_expression(Preprocessor* pp, const char* expression,
                               bool* valid) {
    PPExprValue value = pp_eval_integer_expression(pp, expression, valid);
    return *valid && pp_expr_truth(value);
}

static const char* pp_matching_paren(const char* open,
                                     bool allow_angle_header) {
    const char* p;
    char quoted = '\0';
    char close_stack[256];
    size_t stack_depth = 1u;
    bool in_angle_header = false;

    if (!open || *open != '(') return NULL;
    close_stack[0] = ')';
    p = open + 1;
    if (allow_angle_header && *pp_expr_skip(p) == '<') {
        p = pp_expr_skip(p);
        in_angle_header = true;
    }
    for (; *p && *p != '\n'; ++p) {
        if (quoted) {
            if (*p == '\\' && p[1]) ++p;
            else if (*p == quoted) quoted = '\0';
            continue;
        }
        if (in_angle_header) {
            if (*p == '>') in_angle_header = false;
            continue;
        }
        if (*p == '"' || *p == '\'') {
            quoted = *p;
            continue;
        }
        if (*p == '(' || *p == '[' || *p == '{') {
            if (stack_depth >= sizeof(close_stack)) return NULL;
            close_stack[stack_depth++] = *p == '(' ? ')'
                : *p == '[' ? ']' : '}';
        } else if (*p == ')' || *p == ']' || *p == '}') {
            if (stack_depth == 0u || close_stack[stack_depth - 1u] != *p) {
                return NULL;
            }
            if (--stack_depth == 0u) return p;
        }
    }
    return NULL;
}

static long pp_probe_c_attribute_expression(Preprocessor* pp,
                                           const char* open_paren,
                                           const char** after, bool* valid) {
    const char* close = pp_matching_paren(open_paren, false);
    size_t argument_length;
    char* argument;
    char* expanded;
    const char* cursor;
    char attribute[256];
    size_t attribute_length = 0u;
    long value = 0;

    if (valid) *valid = false;
    if (after) *after = open_paren;
    if (!open_paren || *open_paren != '(' || !close) return 0;
    argument_length = (size_t)(close - open_paren - 1);
    argument = rcc_alloc(argument_length + 1u);
    memcpy(argument, open_paren + 1, argument_length);
    argument[argument_length] = '\0';
    expanded = expand_macros(pp, argument);
    rcc_free(argument);
    if (!expanded) return 0;

    cursor = pp_expr_skip(expanded);
    while (isalpha((unsigned char)*cursor) || *cursor == '_') {
        char component[128];
        const char* component_end = read_ident(
            cursor, component, sizeof(component));
        size_t component_length = strlen(component);
        if (attribute_length + component_length + 1u >= sizeof(attribute)) {
            goto done;
        }
        memcpy(attribute + attribute_length, component, component_length);
        attribute_length += component_length;
        cursor = pp_expr_skip(component_end);
        if (cursor[0] == ':' && cursor[1] == ':') {
            if (attribute_length + 2u >= sizeof(attribute)) goto done;
            attribute[attribute_length++] = ':';
            attribute[attribute_length++] = ':';
            cursor = pp_expr_skip(cursor + 2);
        } else {
            break;
        }
    }
    attribute[attribute_length] = '\0';
    if (attribute_length == 0u || *cursor != '\0') goto done;

    value = pp_c_attribute_value(pp, attribute);
    if (valid) *valid = true;
    if (after) *after = close + 1;

done:
    rcc_free(expanded);
    return value;
}

static bool pp_contains_defined_token(const char* text) {
    const char* p = text;
    while (*p) {
        if (*p == '"' || *p == '\'') {
            char quote = *p++;
            while (*p && *p != quote && *p != '\n') {
                if (*p == '\\' && p[1]) p += 2;
                else ++p;
            }
            if (*p == quote) ++p;
            continue;
        }
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char* start = p++;
            while (isalnum((unsigned char)*p) || *p == '_') ++p;
            if ((size_t)(p - start) == 7u &&
                memcmp(start, "defined", 7u) == 0) return true;
            continue;
        }
        ++p;
    }
    return false;
}

static bool pp_parse_embed_spec(Preprocessor* pp, const char* text,
                                PPEmbedSpec* spec, const char** error) {
    const char* p = pp_expr_skip(text);
    size_t resource_length = 0u;

    memset(spec, 0, sizeof(*spec));
    if (error) *error = "expected a quoted or angle-bracket resource name";
    if (*p == '"') {
        const char* start = ++p;
        while (*p && *p != '"' && *p != '\n') ++p;
        if (*p != '"') return false;
        resource_length = (size_t)(p - start);
        if (resource_length >= sizeof(spec->resource_name)) {
            if (error) *error = "resource name is too long";
            return false;
        }
        memcpy(spec->resource_name, start, resource_length);
        ++p;
    } else if (*p == '<') {
        const char* start = ++p;
        while (*p && *p != '>' && *p != '\n') ++p;
        if (*p != '>') return false;
        resource_length = (size_t)(p - start);
        if (resource_length >= sizeof(spec->resource_name)) {
            if (error) *error = "resource name is too long";
            return false;
        }
        memcpy(spec->resource_name, start, resource_length);
        spec->is_system = true;
        ++p;
    } else {
        return false;
    }
    spec->resource_name[resource_length] = '\0';
    if (resource_length == 0u) return false;

    for (;;) {
        char parameter[128];
        const char* parameter_start;
        const char* argument_start;
        const char* close;
        size_t parameter_length;
        p = pp_expr_skip(p);
        if (!*p || *p == '\r' || *p == '\n') return true;

        parameter_start = p;
        if (!(isalpha((unsigned char)*p) || *p == '_')) {
            if (error) *error = "expected an embed parameter name";
            return false;
        }
        ++p;
        while (isalnum((unsigned char)*p) || *p == '_') ++p;
        if (p[0] == ':' && p[1] == ':') {
            p += 2;
            if (!(isalpha((unsigned char)*p) || *p == '_')) {
                if (error) *error = "expected a name after '::'";
                return false;
            }
            ++p;
            while (isalnum((unsigned char)*p) || *p == '_') ++p;
        }
        parameter_length = (size_t)(p - parameter_start);
        if (parameter_length >= sizeof(parameter)) {
            if (error) *error = "embed parameter name is too long";
            return false;
        }
        memcpy(parameter, parameter_start, parameter_length);
        parameter[parameter_length] = '\0';
        if (strcmp(parameter, "__limit__") == 0) {
            strcpy(parameter, "limit");
        } else if (strcmp(parameter, "__prefix__") == 0) {
            strcpy(parameter, "prefix");
        } else if (strcmp(parameter, "__suffix__") == 0) {
            strcpy(parameter, "suffix");
        } else if (strcmp(parameter, "__if_empty__") == 0) {
            strcpy(parameter, "if_empty");
        }
        p = pp_expr_skip(p);
        if (*p != '(') {
            if (strstr(parameter, "::")) {
                spec->has_unsupported_parameter = true;
                continue;
            }
            if (error) *error = "standard embed parameter requires a parenthesized argument";
            return false;
        }
        close = pp_matching_paren(p, false);
        if (!close) {
            if (error) *error = "unterminated embed parameter argument";
            return false;
        }
        argument_start = p + 1;

        if (strcmp(parameter, "limit") == 0) {
            bool expression_valid = true;
            PPExprValue limit_value;
            size_t argument_length = (size_t)(close - argument_start);
            char* expression;
            if (spec->has_limit) {
                if (error) *error = "duplicate 'limit' embed parameter";
                return false;
            }
            if (argument_length == 0u) {
                if (error) *error = "'limit' requires an integer constant expression";
                return false;
            }
            expression = rcc_alloc(argument_length + 1u);
            memcpy(expression, argument_start, argument_length);
            expression[argument_length] = '\0';
            if (pp_contains_defined_token(expression)) {
                rcc_free(expression);
                if (error) *error = "'defined' is not permitted in a 'limit' expression";
                return false;
            }
            limit_value = pp_eval_integer_expression(
                pp, expression, &expression_valid);
            rcc_free(expression);
            if (!expression_valid ||
                (!limit_value.is_unsigned &&
                 (int64_t)limit_value.bits < 0)) {
                if (error) *error = "'limit' requires a nonnegative integer constant expression";
                return false;
            }
            spec->has_limit = true;
            spec->limit = limit_value.bits;
        } else if (strcmp(parameter, "prefix") == 0) {
            if (spec->has_prefix) {
                if (error) *error = "duplicate 'prefix' embed parameter";
                return false;
            }
            spec->has_prefix = true;
            spec->prefix = argument_start;
            spec->prefix_length = (size_t)(close - argument_start);
        } else if (strcmp(parameter, "suffix") == 0) {
            if (spec->has_suffix) {
                if (error) *error = "duplicate 'suffix' embed parameter";
                return false;
            }
            spec->has_suffix = true;
            spec->suffix = argument_start;
            spec->suffix_length = (size_t)(close - argument_start);
        } else if (strcmp(parameter, "if_empty") == 0) {
            if (spec->has_if_empty) {
                if (error) *error = "duplicate 'if_empty' embed parameter";
                return false;
            }
            spec->has_if_empty = true;
            spec->if_empty = argument_start;
            spec->if_empty_length = (size_t)(close - argument_start);
        } else {
            if (strstr(parameter, "::")) {
                spec->has_unsupported_parameter = true;
            } else {
                spec->has_unrecognized_parameter = true;
            }
        }
        p = close + 1;
    }
}

static int pp_probe_embed_expression(Preprocessor* pp, const char* open_paren,
                                     const char** after, bool* valid) {
    const char* close = pp_matching_paren(open_paren, true);
    size_t argument_length;
    char* argument;
    char* expanded;
    PPEmbedSpec spec;
    const char* error = NULL;
    char resolved[RCC_MAX_PATH];
    size_t resource_size = 0u;
    char* resource;
    int result = 0;

    if (!close) {
        *valid = false;
        if (after) *after = open_paren;
        return 0;
    }
    if (after) *after = close + 1;
    argument_length = (size_t)(close - (open_paren + 1));
    argument = rcc_alloc(argument_length + 1u);
    memcpy(argument, open_paren + 1, argument_length);
    argument[argument_length] = '\0';
    expanded = expand_macros(pp, argument);
    rcc_free(argument);

    if (!pp_parse_embed_spec(pp, expanded, &spec, &error)) {
        *valid = false;
        rcc_free(expanded);
        return 0;
    }
    if (spec.has_unrecognized_parameter) {
        *valid = false;
        rcc_free(expanded);
        return 0;
    }
    if (spec.has_unsupported_parameter) {
        rcc_free(expanded);
        return 0;
    }
    size_t max_content_size = spec.has_limit &&
            spec.limit < (uint64_t)PP_MAX_FILE_BYTES
        ? (size_t)spec.limit : PP_MAX_FILE_BYTES;
    resource = find_include(pp, spec.resource_name, pp->current_file,
                            spec.is_system, false, resolved, sizeof(resolved),
                            max_content_size, &resource_size);
    if (resource) {
        size_t selected_size = resource_size;
        if (spec.has_limit && spec.limit < (uint64_t)selected_size) {
            selected_size = (size_t)spec.limit;
        }
        result = selected_size == 0u ? 2 : 1;
        rcc_free(resource);
    }
    rcc_free(expanded);
    return result;
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

/* Expand the standard _Pragma operator into the directive spelling consumed
 * by the lexer.  The returned pointer is the first character after the
 * closing parenthesis, or NULL when the identifier is not a valid operator
 * invocation so the caller can leave ordinary identifier processing intact. */
static const char* expand_pragma_operator(const char* after_name,
                                          PPBuffer* result) {
    const char* cursor = skip_ws(after_name);
    PPBuffer pragma;

    if (*cursor != '(') return NULL;
    cursor = skip_ws(cursor + 1);
    if (*cursor != '"') return NULL;
    ++cursor;

    buf_init(&pragma);
    while (*cursor && *cursor != '"') {
        if (*cursor == '\\' && cursor[1]) {
            ++cursor;
            buf_append_char(&pragma, *cursor++);
        } else {
            buf_append_char(&pragma, *cursor++);
        }
    }
    if (*cursor != '"') {
        rcc_free(pragma.data);
        return NULL;
    }
    ++cursor;
    cursor = skip_ws(cursor);
    if (*cursor != ')') {
        rcc_free(pragma.data);
        return NULL;
    }
    ++cursor;

    buf_append_str(result, "#pragma ");
    buf_append(result, pragma.data, pragma.size);
    buf_append_char(result, '\n');
    rcc_free(pragma.data);
    return cursor;
}

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
        char ident[RCC_MAX_IDENT];
        return read_ident(p, ident, sizeof(ident));
    }
    return p + (*p ? 1 : 0);
}

static const char* pp_quoted_token_end(const char* p) {
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

static const char* pp_macro_invocation_end(const char* open) {
    const char* p = open;
    int depth = 0;
    while (*p) {
        if (*p == '"' || *p == '\'') {
            p = pp_quoted_token_end(p);
            continue;
        }
        if (*p == '(') {
            depth++;
        } else if (*p == ')' && --depth == 0) {
            return p + 1;
        }
        p++;
    }
    return NULL;
}

/* Extend a normal source-line chunk when a function-like macro call opened
 * on this line closes on a later line. Newlines inside the call are whitespace
 * in a preprocessing argument; the caller restores them after expansion so
 * subsequent diagnostics retain their source line numbers. */
static const char* pp_multiline_macro_extent(Preprocessor* pp,
                                             const char* start,
                                             const char* line_end) {
    const char* extent = line_end;
    const char* p = start;
    while (p < extent) {
        if (*p == '"' || *p == '\'') {
            p = pp_quoted_token_end(p);
            continue;
        }
        if (isalpha((unsigned char)*p) || *p == '_') {
            char identifier[RCC_MAX_IDENT];
            const char* identifier_end = read_ident(
                p, identifier, sizeof(identifier));
            Macro* macro = pp_get_macro(pp, identifier);
            if (macro && macro->param_count >= 0 &&
                !macro_is_expanding(pp, macro)) {
                const char* open = skip_macro_space(identifier_end);
                if (*open == '(') {
                    const char* invocation_end =
                        pp_macro_invocation_end(open);
                    if (invocation_end && invocation_end > line_end) {
                        if (invocation_end > extent) extent = invocation_end;
                        p = invocation_end;
                        continue;
                    }
                }
            }
            p = identifier_end;
            continue;
        }
        p++;
    }
    return extent;
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
                char ident[RCC_MAX_IDENT];
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
            char ident[RCC_MAX_IDENT];
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
            char ident[RCC_MAX_IDENT];
            const char* end = read_ident(p, ident, sizeof(ident));

            if (strcmp(ident, "_Pragma") == 0) {
                const char* after_pragma = expand_pragma_operator(end, &result);
                if (after_pragma) {
                    p = after_pragma;
                    continue;
                }
            }

            Macro* macro = pp_get_macro(pp, ident);
            if (macro && !macro_is_expanding(pp, macro)) {
                if (macro->param_count >= 0) {
                    /* Function-like macro - need arguments */
                    const char* args_start = skip_macro_space(end);
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
                        const char* following_open = skip_macro_space(args_start);
                        size_t expanded_size = strlen(expanded);
                        const char* token_end = expanded + expanded_size;
                        const char* token_start = token_end;
                        while (token_start > expanded &&
                               isspace((unsigned char)token_start[-1])) {
                            token_start--;
                        }
                        token_end = token_start;
                        while (token_start > expanded &&
                               (isalnum((unsigned char)token_start[-1]) ||
                                token_start[-1] == '_')) {
                            token_start--;
                        }
                        if (token_start < token_end &&
                            (isalpha((unsigned char)*token_start) ||
                             *token_start == '_')) {
                            char trailing_ident[RCC_MAX_IDENT];
                            size_t ident_size =
                                (size_t)(token_end - token_start);
                            if (ident_size < sizeof(trailing_ident)) {
                                memcpy(trailing_ident, token_start,
                                       ident_size);
                                trailing_ident[ident_size] = '\0';
                                Macro* trailing_macro = pp_get_macro(
                                    pp, trailing_ident);
                                if (trailing_macro &&
                                    trailing_macro->param_count >= 0 &&
                                    !macro_is_expanding(pp, trailing_macro) &&
                                    *following_open == '(') {
                                    const char* joined_end =
                                        pp_macro_invocation_end(following_open);
                                    if (joined_end) {
                                        PPBuffer joined;
                                        buf_init(&joined);
                                        buf_append_str(&joined, expanded);
                                        buf_append(&joined, args_start,
                                            (size_t)(joined_end - args_start));
                                        char* rescanned = expand_macros(
                                            pp, joined.data);
                                        buf_append_str(&result, rescanned);
                                        rcc_free(rescanned);
                                        rcc_free(joined.data);
                                        rcc_free(expanded);
                                        p = joined_end;
                                        continue;
                                    }
                                }
                            }
                        }
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

static const char* process_embed_directive(Preprocessor* pp, const char* p,
                                           const char* filename,
                                           int source_line,
                                           PPBuffer* output) {
    const char* original_end = skip_to_eol(p);
    size_t directive_length = (size_t)(original_end - p);
    char* directive_text;
    char* expanded;
    PPEmbedSpec spec;
    const char* error = NULL;
    char resolved[RCC_MAX_PATH];
    size_t resource_size = 0u;
    char* resource;

    if (!pp_is_active(pp)) return original_end;
    directive_text = rcc_alloc(directive_length + 1u);
    memcpy(directive_text, p, directive_length);
    directive_text[directive_length] = '\0';
    expanded = expand_macros(pp, directive_text);
    rcc_free(directive_text);
    if (!pp_parse_embed_spec(pp, expanded, &spec, &error)) {
        rcc_error((SourceLoc){filename, source_line, 0},
                  "invalid #embed directive: %s",
                  error ? error : "invalid resource or parameter syntax");
        rcc_free(expanded);
        return original_end;
    }
    if (spec.has_unsupported_parameter || spec.has_unrecognized_parameter) {
        rcc_error((SourceLoc){filename, source_line, 0},
                  "unsupported #embed parameter");
        rcc_free(expanded);
        return original_end;
    }

    resource = find_include(pp, spec.resource_name, filename, spec.is_system,
                            false, resolved, sizeof(resolved),
                            spec.has_limit &&
                                    spec.limit < (uint64_t)PP_MAX_FILE_BYTES
                                ? (size_t)spec.limit : PP_MAX_FILE_BYTES,
                            &resource_size);
    if (!resource) {
        rcc_error((SourceLoc){filename, source_line, 0},
                  "cannot find binary resource: %s", spec.resource_name);
        rcc_free(expanded);
        return original_end;
    }
    pp_add_dependency(pp, resolved);

    size_t selected_size = resource_size;
    if (spec.has_limit && spec.limit < (uint64_t)selected_size) {
        selected_size = (size_t)spec.limit;
    }
    if (selected_size == 0u) {
        if (spec.has_if_empty && spec.if_empty_length > 0u) {
            buf_append(output, spec.if_empty, spec.if_empty_length);
        }
    } else {
        if (spec.has_prefix && spec.prefix_length > 0u) {
            buf_append(output, spec.prefix, spec.prefix_length);
            buf_append_char(output, ' ');
        }
        for (size_t index = 0u; index < selected_size; ++index) {
            char value[4];
            int written;
            if (index != 0u) buf_append_str(output, ", ");
            written = snprintf(value, sizeof(value), "%u",
                               (unsigned int)(unsigned char)resource[index]);
            if (written < 0 || (size_t)written >= sizeof(value)) {
                rcc_fatal("cannot format #embed byte value");
            }
            buf_append(output, value, (size_t)written);
        }
        if (spec.has_suffix && spec.suffix_length > 0u) {
            buf_append_char(output, ' ');
            buf_append(output, spec.suffix, spec.suffix_length);
        }
    }

    rcc_free(resource);
    rcc_free(expanded);
    return original_end;
}

/* Process a single directive */
static const char* process_directive(Preprocessor* pp, const char* p,
                                     const char* filename, int source_line,
                                     PPBuffer* output) {
    p = skip_ws(p + 1); /* Skip '#' and whitespace */

    char directive[64];
    p = read_ident(p, directive, sizeof(directive));
    p = skip_ws(p);

    if (strcmp(directive, "include") == 0 ||
        strcmp(directive, "include_next") == 0) {
        bool include_next = strcmp(directive, "include_next") == 0;
        const char* include_directive = include_next
            ? "#include_next" : "#include";
        if (!pp_is_active(pp)) {
            return skip_to_eol(p);
        }

        if (pp->include_depth >= PP_MAX_INCLUDE_DEPTH) {
            rcc_error((SourceLoc){filename, 0, 0}, "#include nested too deeply");
            return skip_to_eol(p);
        }

        const char* original_end = skip_to_eol(p);
        size_t include_text_length = (size_t)(original_end - p);
        char* include_text = rcc_alloc(include_text_length + 1u);
        memcpy(include_text, p, include_text_length);
        include_text[include_text_length] = '\0';
        char* expanded_include;
        const char* raw_include_start = skip_ws(include_text);
        if (*raw_include_start == '<' || *raw_include_start == '"') {
            /* A delimited header-name is already a preprocessing token.
             * Macro-expanding its spelling can corrupt valid names such as
             * <errno.h> after errno.h has defined `errno`. */
            expanded_include = include_text;
        } else {
            /* Macro form (#include HEADER) is expanded before the resulting
             * quoted or angle-bracket header-name is parsed. */
            expanded_include = expand_macros(pp, include_text);
            rcc_free(include_text);
        }

        const char* include_start = skip_ws(expanded_include);
        bool is_system = (*include_start == '<');
        char inc_name[256];
        char delim = is_system ? '>' : '"';
        bool include_closed = false;
        if (*include_start != '<' && *include_start != '"') {
            rcc_error((SourceLoc){filename, source_line, 0},
                      "%s expects a quoted or angle-bracket path",
                      include_directive);
            rcc_free(expanded_include);
            return original_end;
        }
        const char* include_end = read_string(
            include_start, inc_name, sizeof(inc_name), delim, &include_closed);
        if (!include_closed) {
            rcc_error((SourceLoc){filename, source_line, 0},
                      "unterminated %s path", include_directive);
            rcc_free(expanded_include);
            return original_end;
        }
        if (*skip_ws(include_end) != '\0') {
            rcc_error((SourceLoc){filename, source_line, 0},
                      "unexpected tokens after %s path", include_directive);
            rcc_free(expanded_include);
            return original_end;
        }

        char resolved[RCC_MAX_PATH];
        char* content = find_include(pp, inc_name, filename, is_system,
                                     include_next, resolved, sizeof(resolved),
                                     PP_MAX_FILE_BYTES, NULL);
        if (!content) {
            rcc_error((SourceLoc){filename, 0, 0},
                      "cannot find %s file: %s", include_directive, inc_name);
            rcc_free(expanded_include);
            return original_end;
        }
        rcc_free(expanded_include);
        pp_add_dependency(pp, resolved);

        if (pp_once_contains(pp, resolved)) {
            char return_line[256];
            int written = snprintf(return_line, sizeof(return_line),
                                   "#line %d \"%s\"\n",
                                   source_line + 1, filename);
            if (written < 0 || (size_t)written >= sizeof(return_line)) {
                rcc_fatal("#line include filename is too long");
            }
            buf_append_str(output, return_line);
            rcc_free(content);
            return original_end;
        }

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

        return original_end;
    }

    if (strcmp(directive, "embed") == 0 &&
        !pp->cxx_mode && g_opts.c_standard >= 23) {
        return process_embed_directive(pp, p, filename, source_line, output);
    }

    if (strcmp(directive, "define") == 0) {
        if (!pp_is_active(pp)) {
            return skip_to_eol(p);
        }

        char macro_name[RCC_MAX_IDENT];
        p = read_ident(p, macro_name, sizeof(macro_name));

        if (*p == '(') {
            /* Function-like macro */
            p++; /* Skip '(' */
            const char* params[PP_MAX_PARAMS];
            char param_bufs[PP_MAX_PARAMS][RCC_MAX_IDENT];
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

                char param[RCC_MAX_IDENT];
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

        char name[RCC_MAX_IDENT];
        p = read_ident(p, name, sizeof(name));
        pp_undef(pp, name);
        return skip_to_eol(p);
    }

    if (strcmp(directive, "ifdef") == 0) {
        char name[RCC_MAX_IDENT];
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
        char name[RCC_MAX_IDENT];
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

    if ((strcmp(directive, "elifdef") == 0 ||
         strcmp(directive, "elifndef") == 0) &&
        !pp->cxx_mode && g_opts.c_standard >= 23) {
        char name[RCC_MAX_IDENT];
        bool is_undefined_test = strcmp(directive, "elifndef") == 0;
        bool parent_active;
        bool had_true;
        bool condition;
        const char* identifier_start = skip_ws(p);
        const char* identifier_end;

        if (pp->cond_depth == 0) {
            rcc_error((SourceLoc){filename, source_line, 0},
                      "#%s without #if", directive);
            return skip_to_eol(p);
        }
        if (pp->cond_stack[pp->cond_depth - 1].in_else) {
            rcc_error((SourceLoc){filename, source_line, 0},
                      "#%s after #else", directive);
            return skip_to_eol(p);
        }
        if (!(isalpha((unsigned char)*identifier_start) ||
              *identifier_start == '_')) {
            rcc_error((SourceLoc){filename, source_line, 0},
                      "invalid #%s directive: expected an identifier",
                      directive);
            return skip_to_eol(p);
        }
        identifier_end = read_ident(identifier_start, name, sizeof(name));
        if (*skip_ws(identifier_end) != '\0' &&
            *skip_ws(identifier_end) != '\n') {
            rcc_error((SourceLoc){filename, source_line, 0},
                      "invalid #%s directive: unexpected trailing text",
                      directive);
            return skip_to_eol(p);
        }
        parent_active = pp->cond_depth > 1
            ? pp->cond_stack[pp->cond_depth - 2].active : true;
        had_true = pp->cond_stack[pp->cond_depth - 1].had_true;
        condition = pp_is_defined(pp, name);
        if (is_undefined_test) condition = !condition;
        pp->cond_stack[pp->cond_depth - 1].active =
            parent_active && !had_true && condition;
        if (condition)
            pp->cond_stack[pp->cond_depth - 1].had_true = true;
        return skip_to_eol(identifier_end);
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
            const char* pragma = skip_ws(p);
            const char* pragma_end = pragma;
            while (*pragma_end &&
                   !isspace((unsigned char)*pragma_end)) ++pragma_end;
            if ((size_t)(pragma_end - pragma) == 4u &&
                strncmp(pragma, "once", 4u) == 0 &&
                skip_ws(pragma_end) == end) {
                pp_mark_once(pp, filename);
                return end;
            }
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
        const char* end = skip_to_eol(p);
        size_t line_text_length = (size_t)(end - p);
        char* line_text = rcc_alloc(line_text_length + 1u);
        memcpy(line_text, p, line_text_length);
        line_text[line_text_length] = '\0';
        char* expanded_line = expand_macros(pp, line_text);
        rcc_free(line_text);
        const char* argument = skip_ws(expanded_line);
        const char* number_start = argument;
        char* end_number;
        unsigned long line_number;

        while (isdigit((unsigned char)*argument)) argument++;
        if (argument == number_start) {
            rcc_error((SourceLoc){filename, source_line, 0},
                      "invalid #line directive: expected a positive line number");
            rcc_free(expanded_line);
            return end;
        }
        line_number = strtoul(number_start, &end_number, 10);
        if (line_number == 0u || end_number != argument) {
            rcc_error((SourceLoc){filename, source_line, 0},
                      "invalid #line directive: line number must be positive");
            rcc_free(expanded_line);
            return end;
        }
        argument = skip_ws(argument);
        if (*argument != '\0' && *argument != '"' &&
            !isdigit((unsigned char)*argument)) {
            rcc_error((SourceLoc){filename, source_line, 0},
                      "invalid #line directive: unexpected trailing text");
            rcc_free(expanded_line);
            return end;
        }
        if (pp_is_active(pp)) {
            /* Keep the marker in the preprocessed stream.  The lexer owns
             * the source-location state and already consumes this exact
             * form, so diagnostics and generated locations remain aligned. */
            buf_append_str(output, "#line ");
            buf_append_str(output, expanded_line);
        }
        rcc_free(expanded_line);
        return end;
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
    const char* previous_file = pp->current_file;
    char* normalized_source;
    char* spliced_source;
    buf_init(&output);
    pp->current_file = filename;

    normalized_source = normalize_source_newlines(source);
    if (!normalized_source) {
        rcc_fatal("cannot normalize null preprocessor source");
        return NULL;
    }
    spliced_source = splice_source_lines(normalized_source);
    rcc_free(normalized_source);
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
        const char* chunk_end = pp_multiline_macro_extent(pp, line_start, eol);
        size_t line_len = chunk_end - line_start;
        char* line_buf = rcc_alloc(line_len + 1);
        int consumed_newlines = 0;
        for (size_t index = 0; index < line_len; ++index) {
            if (line_start[index] == '\n') {
                line_buf[index] = ' ';
                consumed_newlines++;
            } else {
                line_buf[index] = line_start[index];
            }
        }
        line_buf[line_len] = '\0';

        /* Expand macros */
        char* expanded = expand_macros(pp, line_buf);
        buf_append_str(&output, expanded);
        rcc_free(line_buf);
        rcc_free(expanded);

        p = chunk_end;
        if (chunk_end > eol) {
            for (int index = 0; index < consumed_newlines; ++index) {
                buf_append_char(&output, '\n');
            }
            line += consumed_newlines;
        } else if (*p == '\n') {
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
    pp->current_file = previous_file;
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
