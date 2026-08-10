/*
 * RCC - RinOS C Compiler
 * Utility functions and global state
 */

#include "rcc.h"
#include <stdarg.h>
#if defined(_WIN32)
#include <process.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

/* Global state */
CompilerOptions g_opts = {0};
int g_error_count = 0;
int g_warning_count = 0;

bool rcc_parse_target_triple(const char* triple, TargetArch* arch_out) {
    if (!triple || !arch_out) {
        return false;
    }
    if (strcmp(triple, RCC_TARGET_I686) == 0) {
        *arch_out = ARCH_X86;
        return true;
    }
    if (strcmp(triple, RCC_TARGET_X86_64) == 0) {
        *arch_out = ARCH_X64;
        return true;
    }
    return false;
}

const char* rcc_target_triple(TargetArch arch) {
    return arch == ARCH_X64 ? RCC_TARGET_X86_64 : RCC_TARGET_I686;
}

bool rcc_run_rinsign(const char* unsigned_path, const char* output_path) {
    const char* python = g_opts.python_path ? g_opts.python_path : "python3";
    const char* const arguments[] = {
        python, g_opts.rinsign_path, unsigned_path, "-o", output_path,
        "--key", g_opts.sign_key, "--public-key", g_opts.public_key, NULL
    };
    int status;

    if (!unsigned_path || !output_path || !g_opts.rinsign_path ||
        !g_opts.sign_key || !g_opts.public_key) {
        fprintf(stderr, "rcc: final v3 output requires --rinsign, --sign-key and --public-key\n");
        return false;
    }
#if defined(_WIN32)
    status = (int)_spawnvp(_P_WAIT, python, arguments);
    if (status == -1) {
        perror("rcc: cannot start rinsign");
        return false;
    }
    return status == 0;
#else
    pid_t pid = fork();
    if (pid < 0) {
        perror("rcc: cannot fork rinsign");
        return false;
    }
    if (pid == 0) {
        execvp(python, (char* const*)arguments);
        perror("rcc: cannot start rinsign");
        _exit(127);
    }
    do {
        status = 0;
    } while (waitpid(pid, &status, 0) < 0);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

/* String interning hash table */
#define INTERN_SIZE 4096
static const char* intern_table[INTERN_SIZE];

static unsigned int hash_string(const char* s) {
    unsigned int h = 0;
    while (*s) {
        h = h * 31 + (unsigned char)*s++;
    }
    return h;
}

const char* rcc_intern(const char* str) {
    unsigned int h = hash_string(str) % INTERN_SIZE;
    unsigned int start = h;

    do {
        if (!intern_table[h]) {
            intern_table[h] = rcc_strdup(str);
            return intern_table[h];
        }
        if (strcmp(intern_table[h], str) == 0) {
            return intern_table[h];
        }
        h = (h + 1) % INTERN_SIZE;
    } while (h != start);

    rcc_fatal("string intern table full");
    return NULL;
}

/* Memory allocation */
void* rcc_alloc(size_t size) {
    void* p = malloc(size);
    if (!p) {
        rcc_fatal("out of memory");
    }
    memset(p, 0, size);
    return p;
}

void* rcc_realloc(void* ptr, size_t size) {
    void* p = realloc(ptr, size);
    if (!p && size > 0) {
        rcc_fatal("out of memory");
    }
    return p;
}

char* rcc_strdup(const char* s) {
    size_t len = strlen(s);
    char* p = rcc_alloc(len + 1);
    memcpy(p, s, len + 1);
    return p;
}

void rcc_free(void* ptr) {
    free(ptr);
}

/* Error reporting */
void rcc_error(SourceLoc loc, const char* fmt, ...) {
    fprintf(stderr, "%s:%d:%d: error: ", loc.filename, loc.line, loc.column);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    g_error_count++;

    if (g_error_count >= RCC_MAX_ERRORS) {
        rcc_fatal("too many errors");
    }
}

void rcc_warning(SourceLoc loc, const char* fmt, ...) {
    fprintf(stderr, "%s:%d:%d: warning: ", loc.filename, loc.line, loc.column);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    g_warning_count++;

    if (g_opts.warnings_as_errors) {
        g_error_count++;
    }
}

void rcc_fatal(const char* fmt, ...) {
    fprintf(stderr, "rcc: fatal error: ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(1);
}
