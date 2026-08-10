/*
 * RCC - RinOS C Compiler
 * Common definitions
 */

#ifndef RCC_H
#define RCC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Version */
#define RCC_VERSION_MAJOR 0
#define RCC_VERSION_MINOR 1
#define RCC_VERSION_PATCH 0

/* Limits */
#define RCC_MAX_PATH        260
#define RCC_MAX_IDENT       256
#define RCC_MAX_STRING      4096
#define RCC_MAX_ERRORS      100

/* Output format */
typedef enum {
    OUTPUT_RIN,     /* Executable (.rin) */
    OUTPUT_RLL,     /* Library (.rll) */
    OUTPUT_DRV,     /* Driver (.drv) */
    OUTPUT_ASM,     /* Assembly (.s) */
    OUTPUT_OBJ,     /* Object file (.ro) */
} OutputFormat;

/* Target architecture */
typedef enum {
    ARCH_X86,       /* 32-bit x86 */
    ARCH_X64,       /* 64-bit x86-64 */
} TargetArch;

/* The key itself is always supplied by the invoking RinOS build profile. */
typedef enum {
    SIGN_PROFILE_UNSPECIFIED = 0,
    SIGN_PROFILE_DEBUG,
    SIGN_PROFILE_RELEASE,
} SigningProfile;

/* Maximum include paths and defines */
#define RCC_MAX_INCLUDES 64
#define RCC_MAX_DEFINES 128

/* Compiler options */
typedef struct {
    char input_file[RCC_MAX_PATH];
    char output_file[RCC_MAX_PATH];
    OutputFormat output_format;
    bool output_format_explicit;
    TargetArch target_arch;
    bool target_explicit;
    int opt_level;              /* 0-3 */
    bool debug_info;
    bool warnings_as_errors;
    bool verbose;

    /* Preprocessor options */
    const char* include_paths[RCC_MAX_INCLUDES];
    int include_count;
    const char* defines[RCC_MAX_DEFINES];     /* "NAME" or "NAME=VALUE" */
    int define_count;
    const char* undefines[RCC_MAX_DEFINES];
    int undef_count;
    bool preprocess_only;       /* -E */
    bool emit_dependencies;     /* -MMD */
    char dependency_file[RCC_MAX_PATH]; /* -MF */

    /* Code generation options */
    bool freestanding;          /* -ffreestanding */
    bool nostdinc;              /* -nostdinc */
    bool wall;                  /* -Wall */
    bool pedantic;              /* -pedantic */

    /* Final artifact signing.  Keys are paths only and are never embedded. */
    const char* sign_key;
    const char* public_key;
    const char* rinsign_path;
    const char* python_path;
    const char* manifest_path;
    SigningProfile signing_profile;
    bool signing_profile_explicit;
    bool emit_unsigned_v3;      /* Internal packaging/debug stage only. */
} CompilerOptions;

/* Canonical RinOS target triples accepted by every toolchain frontend. */
#define RCC_TARGET_I686   "i686-unknown-rinos"
#define RCC_TARGET_X86_64 "x86_64-unknown-rinos"

bool rcc_parse_target_triple(const char* triple, TargetArch* arch_out);
const char* rcc_target_triple(TargetArch arch);
bool rcc_parse_signing_profile(const char* value, SigningProfile* profile_out);
const char* rcc_signing_profile_name(SigningProfile profile);
bool rcc_validate_signing_options(const char* tool_name, bool final_artifact);
bool rcc_create_signing_temp(const char* output_path, const char* stage,
                             char* temp_path, size_t capacity);
bool rcc_run_rinsign(const char* unsigned_path, const char* output_path);

/* Source location */
typedef struct {
    const char* filename;
    int line;
    int column;
} SourceLoc;

/* Error reporting */
void rcc_error(SourceLoc loc, const char* fmt, ...);
void rcc_warning(SourceLoc loc, const char* fmt, ...);
void rcc_fatal(const char* fmt, ...);

/* Memory allocation */
void* rcc_alloc(size_t size);
void* rcc_realloc(void* ptr, size_t size);
char* rcc_strdup(const char* s);
void rcc_free(void* ptr);

/* String interning */
const char* rcc_intern(const char* str);

/* Global state */
extern CompilerOptions g_opts;
extern int g_error_count;
extern int g_warning_count;

/* Main compilation phases */
struct TokenList;
struct AST;
struct Module;
struct Preprocessor;

/* Preprocessor */
struct Preprocessor* rcc_preproc_new(void);
void rcc_preproc_free(struct Preprocessor* pp);
void rcc_preproc_add_include(struct Preprocessor* pp, const char* path);
void rcc_preproc_define(struct Preprocessor* pp, const char* name, const char* value);
char* rcc_preproc(struct Preprocessor* pp, const char* filename);

/* Lexer */
struct TokenList* rcc_lex(const char* filename);
struct TokenList* rcc_lex_string(const char* source, const char* filename);
struct AST* rcc_parse(struct TokenList* tokens);
bool rcc_sema(struct AST* ast);
struct Module* rcc_codegen(struct AST* ast);
bool rcc_emit(struct Module* mod, const char* outfile);
bool rcc_emit_rll(struct Module* mod, struct AST* ast, const char* outfile);
bool rcc_emit_drv(struct Module* mod, struct AST* ast, const char* outfile);
bool rcc_emit_asm(struct Module* mod, const char* outfile);

#endif /* RCC_H */
