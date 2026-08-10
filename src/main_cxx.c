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
#include "preproc.h"
#include "build_manifest.h"
#include <stdarg.h>
#include <getopt.h>

/* C++ specific options */
static bool g_cxx_mode = true;
static int g_cxx_standard = 20;  /* C++20 is the RinOS v3 language contract. */

/* Print usage for rcc++ */
static void print_usage_cxx(void) {
    printf("RCC++ - RinOS C++ Compiler v%d.%d.%d\n",
           RCC_VERSION_MAJOR, RCC_VERSION_MINOR, RCC_VERSION_PATCH);
    printf("\n");
    printf("Usage: rcc++ [options] <input.cpp>\n");
    printf("\n");
    printf("Options:\n");
    printf("  -o <file>       Output file name\n");
    printf("  -c              Compile to .ro v2 object\n");
    printf("  -E              Preprocess only\n");
    printf("  -shared         Create shared library (.rll)\n");
    printf("  -driver         Create driver (.drv)\n");
    printf("  -m32            Generate 32-bit code (default)\n");
    printf("  -m64            Generate 64-bit code\n");
    printf("  --target <triple>  i686-unknown-rinos or x86_64-unknown-rinos\n");
    printf("  --manifest <file>  RIN-BUILD-MANIFEST 1 build contract\n");
    printf("  --rinsign/--sign-key/--public-key  Required final v3 signing inputs\n");
    printf("  -O<level>       Optimization level (0-3)\n");
    printf("  -std=c++<ver>   C++ standard (11, 14, 17, 20)\n");
    printf("  -g              Generate debug info\n");
    printf("  -S              Output assembly\n");
    printf("  -I/-D/-U        Include path and macro controls\n");
    printf("  -nostdinc       Don't search standard include paths\n");
    printf("  -MMD/-MF <file> Emit user-header dependencies\n");
    printf("  -Wall/-Werror   Warning controls\n");
    printf("  -ffreestanding  Freestanding environment\n");
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
                strncpy(g_opts.output_file, optarg, RCC_MAX_PATH - 1);
                break;
            case 'O':
                g_opts.opt_level = atoi(optarg);
                if (g_opts.opt_level < 0) g_opts.opt_level = 0;
                if (g_opts.opt_level > 3) g_opts.opt_level = 3;
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
                strncpy(g_opts.dependency_file, optarg, RCC_MAX_PATH - 1);
                break;
            case 12: g_opts.nostdinc = true; break;
            case 13: g_opts.freestanding = true; break;
            case 14: g_opts.pedantic = true; break;
            case 15: g_opts.manifest_path = optarg; break;
            default:
                return -1;
        }
    }

    /* Get input file */
    if (optind >= argc) {
        fprintf(stderr, "rcc++: error: no input file\n");
        return -1;
    }

    strncpy(g_opts.input_file, argv[optind], RCC_MAX_PATH - 1);

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

    if ((g_opts.output_format == OUTPUT_RIN ||
         g_opts.output_format == OUTPUT_RLL ||
         g_opts.output_format == OUTPUT_DRV) && !g_opts.preprocess_only &&
        !g_opts.emit_unsigned_v3 &&
        (!g_opts.rinsign_path || !g_opts.sign_key || !g_opts.public_key)) {
        fprintf(stderr, "rcc++: error: final v3 output requires --rinsign, --sign-key and --public-key\n");
        return -1;
    }

    /* Default output file */
    if (g_opts.output_file[0] == '\0') {
        const char* ext;
        switch (g_opts.output_format) {
            case OUTPUT_RIN: ext = ".rin"; break;
            case OUTPUT_RLL: ext = ".rll"; break;
            case OUTPUT_DRV: ext = ".drv"; break;
            case OUTPUT_ASM: ext = ".s"; break;
            case OUTPUT_OBJ: ext = ".ro"; break;
            default: ext = ".out"; break;
        }

        strncpy(g_opts.output_file, g_opts.input_file, RCC_MAX_PATH - 1);
        char* dot = strrchr(g_opts.output_file, '.');
        if (dot) *dot = '\0';
        strncat(g_opts.output_file, ext, RCC_MAX_PATH - strlen(g_opts.output_file) - 1);
    }

    return 0;
}

/* C++ main function */
int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage_cxx();
        return 1;
    }

    if (parse_cxx_args(argc, argv) < 0) {
        return 1;
    }
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

    /* Add standard include paths (unless -nostdinc). */
    if (!g_opts.nostdinc) {
        pp_add_include_path(pp, ".");
        pp_add_include_path(pp, "include");
        pp_add_include_path(pp, "include/rcc");
        pp_add_include_path(pp, "/rinos/include");
    }
    for (int i = 0; i < g_opts.include_count; ++i) {
        pp_add_include_path(pp, g_opts.include_paths[i]);
    }

    /* C++ predefined macros */
    pp_define(pp, "__cplusplus",
              g_cxx_standard >= 20 ? "202002L" :
              g_cxx_standard >= 17 ? "201703L" :
              g_cxx_standard >= 14 ? "201402L" : "201103L");
    pp_define(pp, "__RCC__", "1");
    pp_define(pp, "__RCXX__", "1");
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
        pp_free(pp);
        return 1;
    }
    if (g_opts.emit_dependencies) {
        if (g_opts.dependency_file[0] == '\0') {
            strncpy(g_opts.dependency_file, g_opts.output_file, RCC_MAX_PATH - 1);
            char* extension = strrchr(g_opts.dependency_file, '.');
            if (extension) *extension = '\0';
            strncat(g_opts.dependency_file, ".d",
                    RCC_MAX_PATH - strlen(g_opts.dependency_file) - 1);
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

    /* Phase 1: Lexical analysis */
    if (g_opts.verbose) {
        printf("Lexing...\n");
    }
    TokenList* tokens = rcc_lex_string(pp_source, g_opts.input_file);
    if (g_error_count > 0) {
        fprintf(stderr, "rcc++: %d error(s) in lexical analysis\n", g_error_count);
        return 1;
    }

    if (g_opts.verbose) {
        printf("Tokens: %d\n", tokens->count);
    }

    /* Phase 2: Parsing */
    if (g_opts.verbose) {
        printf("Parsing (C++ mode)...\n");
    }
    AST* ast = rcc_parse_cxx(tokens);
    if (g_error_count > 0) {
        fprintf(stderr, "rcc++: %d error(s) in parsing\n", g_error_count);
        tokenlist_free(tokens);
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
        return 1;
    }
    if (g_opts.output_format == OUTPUT_DRV &&
        !rcc_validate_driver_policy(ast)) {
        fprintf(stderr, "rcc++: driver policy validation failed\n");
        tokenlist_free(tokens);
        return 1;
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
    char unsigned_path[RCC_MAX_PATH + 32];
    const char* emit_path = g_opts.output_file;
    if (final_artifact && !g_opts.emit_unsigned_v3) {
        if (snprintf(unsigned_path, sizeof(unsigned_path), "%s.rcc-unsigned.tmp",
                     g_opts.output_file) >= (int)sizeof(unsigned_path)) {
            fprintf(stderr, "rcc++: output path is too long for signing stage\n");
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
            emit_ok = rcc_emit_obj(mod, emit_path);
            break;
        case OUTPUT_ASM:
            emit_ok = rcc_emit_asm(mod, emit_path);
            break;
        default:
            fprintf(stderr, "rcc++: unsupported output format\n");
            break;
    }
    if (emit_ok && final_artifact && !g_opts.emit_unsigned_v3) {
        emit_ok = rcc_run_rinsign(emit_path, g_opts.output_file);
        remove(emit_path);
    }

    if (!emit_ok) {
        fprintf(stderr, "rcc++: failed to write output\n");
        codegen_free(mod);
        tokenlist_free(tokens);
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
    return 0;
}
