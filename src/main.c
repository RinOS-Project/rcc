/*
 * RCC - RinOS C Compiler
 * Main entry point
 */

#include "rcc.h"
#include "token.h"
#include "ast.h"
#include "symtab.h"
#include "codegen.h"
#include "driver_policy.h"
#include "optimize.h"
#include "preproc.h"
#include "build_manifest.h"
#include <getopt.h>

/* Print usage */
static void print_usage(void) {
    printf("RCC - RinOS C Compiler v%d.%d.%d\n",
           RCC_VERSION_MAJOR, RCC_VERSION_MINOR, RCC_VERSION_PATCH);
    printf("\n");
    printf("Usage: rcc [options] <input.c>\n");
    printf("\n");
    printf("Options:\n");
    printf("  -o <file>       Output file name\n");
    printf("  -c              Compile to object file (.ro)\n");
    printf("  -E              Preprocess only\n");
    printf("  -S              Output assembly\n");
    printf("  -shared         Create shared library (.rll)\n");
    printf("  -driver         Create driver (.drv)\n");
    printf("  -m32            Generate 32-bit code (default)\n");
    printf("  -m64            Generate 64-bit code\n");
    printf("  --target <triple>  i686-unknown-rinos or x86_64-unknown-rinos\n");
    printf("  --manifest <file>  RIN-BUILD-MANIFEST 1 build contract\n");
    printf("  --rinsign <file>    Isolated v3 signer program\n");
    printf("  --sign-key <file>   Explicit RSA private key (final outputs)\n");
    printf("  --public-key <file> Provisioned PKCS#1 public DER key\n");
    printf("  --sign-profile <p>  Build profile: debug or release\n");
    printf("  -O<level>       Optimization level (0-3)\n");
    printf("  -g              Generate debug info\n");
    printf("\n");
    printf("Preprocessor:\n");
    printf("  -I<path>        Add include path\n");
    printf("  -D<name>[=val]  Define macro\n");
    printf("  -U<name>        Undefine macro\n");
    printf("  -nostdinc       Don't search standard include paths\n");
    printf("  -MMD            Emit user-header dependencies\n");
    printf("  -MF <file>      Write dependencies to <file>\n");
    printf("\n");
    printf("Warnings:\n");
    printf("  -w              Suppress all warnings\n");
    printf("  -Wall           Enable all warnings\n");
    printf("  -Werror         Treat warnings as errors\n");
    printf("  -pedantic       Strict ISO C compliance\n");
    printf("\n");
    printf("Code generation:\n");
    printf("  -ffreestanding  Freestanding environment\n");
    printf("\n");
    printf("Other:\n");
    printf("  -v              Verbose output\n");
    printf("  -h, --help      Show this help\n");
    printf("  --version       Show version\n");
    printf("\n");
    printf("Output formats:\n");
    printf("  .ro             RinOS object file (with -c)\n");
    printf("  .rin            RinOS executable (default)\n");
    printf("  .rll            RinOS shared library\n");
    printf("  .drv            RinOS driver\n");
}

/* Parse command line */
static int parse_args(int argc, char** argv) {
    /* Default options */
    g_opts.output_format = OUTPUT_RIN;
    g_opts.target_arch = ARCH_X86;
    g_opts.opt_level = 0;
    g_opts.include_count = 0;
    g_opts.define_count = 0;
    g_opts.undef_count = 0;

    /* Manual argument parsing for better GCC compatibility */
    for (int i = 1; i < argc; i++) {
        const char* arg = argv[i];

        if (arg[0] != '-') {
            /* Input file */
            if (g_opts.input_file[0] != '\0') {
                fprintf(stderr, "rcc: error: multiple input files not supported\n");
                return -1;
            }
            strncpy(g_opts.input_file, arg, RCC_MAX_PATH - 1);
            continue;
        }

        /* Options starting with - */
        if (strcmp(arg, "-c") == 0) {
            g_opts.output_format = OUTPUT_OBJ;
            g_opts.output_format_explicit = true;
        } else if (strcmp(arg, "-E") == 0) {
            g_opts.preprocess_only = true;
        } else if (strcmp(arg, "-S") == 0) {
            g_opts.output_format = OUTPUT_ASM;
            g_opts.output_format_explicit = true;
        } else if (strcmp(arg, "-MMD") == 0) {
            g_opts.emit_dependencies = true;
        } else if (strcmp(arg, "-MF") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "rcc: error: -MF requires an argument\n");
                return -1;
            }
            strncpy(g_opts.dependency_file, argv[++i], RCC_MAX_PATH - 1);
        } else if (strncmp(arg, "-MF", 3) == 0) {
            strncpy(g_opts.dependency_file, arg + 3, RCC_MAX_PATH - 1);
        } else if (strcmp(arg, "-g") == 0) {
            g_opts.debug_info = true;
        } else if (strcmp(arg, "-v") == 0) {
            g_opts.verbose = true;
        } else if (strcmp(arg, "-w") == 0) {
            g_opts.wall = false;  /* Suppress warnings */
        } else if (strcmp(arg, "-Wall") == 0) {
            g_opts.wall = true;
        } else if (strcmp(arg, "-Werror") == 0) {
            g_opts.warnings_as_errors = true;
        } else if (strcmp(arg, "-pedantic") == 0) {
            g_opts.pedantic = true;
        } else if (strcmp(arg, "-nostdinc") == 0) {
            g_opts.nostdinc = true;
        } else if (strcmp(arg, "-ffreestanding") == 0) {
            g_opts.freestanding = true;
        } else if (strcmp(arg, "-shared") == 0 || strcmp(arg, "--shared") == 0) {
            g_opts.output_format = OUTPUT_RLL;
            g_opts.output_format_explicit = true;
        } else if (strcmp(arg, "-driver") == 0 || strcmp(arg, "--driver") == 0) {
            g_opts.output_format = OUTPUT_DRV;
            g_opts.output_format_explicit = true;
        } else if (strcmp(arg, "-m32") == 0) {
            if (g_opts.target_explicit && g_opts.target_arch != ARCH_X86) {
                fprintf(stderr, "rcc: error: -m32 conflicts with --target=%s\n",
                        rcc_target_triple(g_opts.target_arch));
                return -1;
            }
            g_opts.target_arch = ARCH_X86;
            g_opts.target_explicit = true;
        } else if (strcmp(arg, "-m64") == 0) {
            if (g_opts.target_explicit && g_opts.target_arch != ARCH_X64) {
                fprintf(stderr, "rcc: error: -m64 conflicts with --target=%s\n",
                        rcc_target_triple(g_opts.target_arch));
                return -1;
            }
            g_opts.target_arch = ARCH_X64;
            g_opts.target_explicit = true;
        } else if (strcmp(arg, "--target") == 0 || strncmp(arg, "--target=", 9) == 0) {
            const char* triple;
            TargetArch target_arch;
            if (strcmp(arg, "--target") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "rcc: error: --target requires an argument\n");
                    return -1;
                }
                triple = argv[++i];
            } else {
                triple = arg + 9;
            }
            if (!rcc_parse_target_triple(triple, &target_arch)) {
                fprintf(stderr, "rcc: error: unsupported target triple: %s\n", triple);
                return -1;
            }
            if (g_opts.target_explicit && g_opts.target_arch != target_arch) {
                fprintf(stderr, "rcc: error: conflicting target selections\n");
                return -1;
            }
            g_opts.target_arch = target_arch;
            g_opts.target_explicit = true;
        } else if (strcmp(arg, "--manifest") == 0 ||
                   strncmp(arg, "--manifest=", 11) == 0) {
            if (strcmp(arg, "--manifest") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "rcc: error: --manifest requires an argument\n");
                    return -1;
                }
                g_opts.manifest_path = argv[++i];
            } else {
                g_opts.manifest_path = arg + 11;
            }
            if (!g_opts.manifest_path[0]) {
                fprintf(stderr, "rcc: error: empty --manifest path\n");
                return -1;
            }
        } else if (strcmp(arg, "--rinsign") == 0 ||
                   strcmp(arg, "--sign-key") == 0 ||
                   strcmp(arg, "--public-key") == 0 ||
                   strcmp(arg, "--python") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "rcc: error: %s requires an argument\n", arg);
                return -1;
            }
            if (strcmp(arg, "--rinsign") == 0) g_opts.rinsign_path = argv[++i];
            else if (strcmp(arg, "--sign-key") == 0) g_opts.sign_key = argv[++i];
            else if (strcmp(arg, "--public-key") == 0) g_opts.public_key = argv[++i];
            else g_opts.python_path = argv[++i];
        } else if (strcmp(arg, "--sign-profile") == 0 ||
                   strncmp(arg, "--sign-profile=", 15) == 0) {
            const char* profile;
            if (strcmp(arg, "--sign-profile") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "rcc: error: --sign-profile requires an argument\n");
                    return -1;
                }
                profile = argv[++i];
            } else {
                profile = arg + 15;
            }
            if (!rcc_parse_signing_profile(profile, &g_opts.signing_profile)) {
                fprintf(stderr, "rcc: error: signing profile must be debug or release\n");
                return -1;
            }
            g_opts.signing_profile_explicit = true;
        } else if (strcmp(arg, "--emit-unsigned-v3") == 0) {
            g_opts.emit_unsigned_v3 = true;
        } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_usage();
            exit(0);
        } else if (strcmp(arg, "--version") == 0) {
            printf("rcc %d.%d.%d\n",
                   RCC_VERSION_MAJOR, RCC_VERSION_MINOR, RCC_VERSION_PATCH);
            exit(0);
        } else if (strcmp(arg, "-o") == 0) {
            /* -o <file> */
            if (i + 1 >= argc) {
                fprintf(stderr, "rcc: error: -o requires an argument\n");
                return -1;
            }
            strncpy(g_opts.output_file, argv[++i], RCC_MAX_PATH - 1);
        } else if (strncmp(arg, "-o", 2) == 0) {
            /* -o<file> */
            strncpy(g_opts.output_file, arg + 2, RCC_MAX_PATH - 1);
        } else if (strncmp(arg, "-O", 2) == 0) {
            /* -O<level> */
            g_opts.opt_level = atoi(arg + 2);
            if (g_opts.opt_level < 0) g_opts.opt_level = 0;
            if (g_opts.opt_level > 3) g_opts.opt_level = 3;
        } else if (strcmp(arg, "-I") == 0) {
            /* -I <path> */
            if (i + 1 >= argc) {
                fprintf(stderr, "rcc: error: -I requires an argument\n");
                return -1;
            }
            if (g_opts.include_count >= RCC_MAX_INCLUDES) {
                fprintf(stderr, "rcc: error: too many include paths\n");
                return -1;
            }
            g_opts.include_paths[g_opts.include_count++] = argv[++i];
        } else if (strncmp(arg, "-I", 2) == 0) {
            /* -I<path> */
            if (g_opts.include_count >= RCC_MAX_INCLUDES) {
                fprintf(stderr, "rcc: error: too many include paths\n");
                return -1;
            }
            g_opts.include_paths[g_opts.include_count++] = arg + 2;
        } else if (strcmp(arg, "-D") == 0) {
            /* -D <name>[=value] */
            if (i + 1 >= argc) {
                fprintf(stderr, "rcc: error: -D requires an argument\n");
                return -1;
            }
            if (g_opts.define_count >= RCC_MAX_DEFINES) {
                fprintf(stderr, "rcc: error: too many defines\n");
                return -1;
            }
            g_opts.defines[g_opts.define_count++] = argv[++i];
        } else if (strncmp(arg, "-D", 2) == 0) {
            /* -D<name>[=value] */
            if (g_opts.define_count >= RCC_MAX_DEFINES) {
                fprintf(stderr, "rcc: error: too many defines\n");
                return -1;
            }
            g_opts.defines[g_opts.define_count++] = arg + 2;
        } else if (strcmp(arg, "-U") == 0) {
            /* -U <name> */
            if (i + 1 >= argc) {
                fprintf(stderr, "rcc: error: -U requires an argument\n");
                return -1;
            }
            if (g_opts.undef_count >= RCC_MAX_DEFINES) {
                fprintf(stderr, "rcc: error: too many undefines\n");
                return -1;
            }
            g_opts.undefines[g_opts.undef_count++] = argv[++i];
        } else if (strncmp(arg, "-U", 2) == 0) {
            /* -U<name> */
            if (g_opts.undef_count >= RCC_MAX_DEFINES) {
                fprintf(stderr, "rcc: error: too many undefines\n");
                return -1;
            }
            g_opts.undefines[g_opts.undef_count++] = arg + 2;
        } else if (strncmp(arg, "-f", 2) == 0) {
            /* Ignore unknown -f options */
            if (g_opts.verbose) {
                fprintf(stderr, "rcc: warning: ignoring unknown option: %s\n", arg);
            }
        } else if (strncmp(arg, "-W", 2) == 0) {
            /* Ignore unknown -W options */
            if (g_opts.verbose) {
                fprintf(stderr, "rcc: warning: ignoring unknown option: %s\n", arg);
            }
        } else {
            fprintf(stderr, "rcc: error: unknown option: %s\n", arg);
            return -1;
        }
    }

    /* Check input file */
    if (g_opts.input_file[0] == '\0') {
        fprintf(stderr, "rcc: error: no input file\n");
        return -1;
    }
    if (g_opts.manifest_path) {
        RccBuildManifest manifest;
        char error[RCC_BUILD_MANIFEST_ERROR_MAX];
        if (!rcc_manifest_load(g_opts.manifest_path, &manifest,
                               error, sizeof(error)) ||
            !rcc_manifest_apply_compiler(&manifest, &g_opts,
                                         error, sizeof(error))) {
            fprintf(stderr, "rcc: error: %s\n", error);
            return -1;
        }
    }
    if (!rcc_validate_signing_options(
            "rcc", !g_opts.preprocess_only &&
                   (g_opts.output_format == OUTPUT_RIN ||
                    g_opts.output_format == OUTPUT_RLL ||
                    g_opts.output_format == OUTPUT_DRV))) {
        return -1;
    }

    /* Default output file */
    if (g_opts.output_file[0] == '\0') {
        const char* ext;
        switch (g_opts.output_format) {
            case OUTPUT_OBJ: ext = ".ro"; break;
            case OUTPUT_RIN: ext = ".rin"; break;
            case OUTPUT_RLL: ext = ".rll"; break;
            case OUTPUT_DRV: ext = ".drv"; break;
            case OUTPUT_ASM: ext = ".s"; break;
            default: ext = ".out"; break;
        }

        /* Remove .c extension and add new extension */
        strncpy(g_opts.output_file, g_opts.input_file, RCC_MAX_PATH - 1);
        char* dot = strrchr(g_opts.output_file, '.');
        if (dot) *dot = '\0';
        strncat(g_opts.output_file, ext, RCC_MAX_PATH - strlen(g_opts.output_file) - 1);
    }

    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (parse_args(argc, argv) < 0) {
        return 1;
    }
    type_configure_target(g_opts.target_arch);

    if (g_opts.verbose) {
        printf("Input:  %s\n", g_opts.input_file);
        printf("Output: %s\n", g_opts.output_file);
        printf("Format: %s\n",
               g_opts.output_format == OUTPUT_RIN ? ".rin" :
               g_opts.output_format == OUTPUT_RLL ? ".rll" :
               g_opts.output_format == OUTPUT_DRV ? ".drv" : ".s");
        printf("Target: %s\n", rcc_target_triple(g_opts.target_arch));
    }

    /* Phase 0: Preprocessing */
    if (g_opts.verbose) {
        printf("Preprocessing...\n");
    }
    Preprocessor* pp = pp_new();

    /* Add standard include paths (unless -nostdinc) */
    if (!g_opts.nostdinc) {
        pp_add_include_path(pp, ".");
        pp_add_include_path(pp, "include");
        pp_add_include_path(pp, "include/rcc");  /* RCC intrinsic headers */
        pp_add_include_path(pp, "/rinos/include");
    }

    /* Add command-line include paths */
    for (int i = 0; i < g_opts.include_count; i++) {
        pp_add_include_path(pp, g_opts.include_paths[i]);
    }

    /* Apply command-line defines */
    for (int i = 0; i < g_opts.define_count; i++) {
        const char* def = g_opts.defines[i];
        char* eq = strchr(def, '=');
        if (eq) {
            /* -DNAME=VALUE */
            char name[256];
            size_t len = eq - def;
            if (len > 255) len = 255;
            strncpy(name, def, len);
            name[len] = '\0';
            pp_define(pp, name, eq + 1);
        } else {
            /* -DNAME (define as 1) */
            pp_define(pp, def, "1");
        }
    }

    /* Apply command-line undefines */
    for (int i = 0; i < g_opts.undef_count; i++) {
        pp_undef(pp, g_opts.undefines[i]);
    }

    /* Define freestanding macro if requested */
    if (g_opts.freestanding) {
        pp_define(pp, "__FREESTANDING__", "1");
    }

    char* pp_source = pp_process_file(pp, g_opts.input_file);
    if (!pp_source || g_error_count > 0) {
        fprintf(stderr, "rcc: %d error(s) in preprocessing\n", g_error_count);
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
            fprintf(stderr, "rcc: error: cannot write dependency file: %s\n",
                    g_opts.dependency_file);
            rcc_free(pp_source);
            pp_free(pp);
            return 1;
        }
    }

    /* -E: Output preprocessed source and exit */
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
        fprintf(stderr, "rcc: %d error(s) in lexical analysis\n", g_error_count);
        return 1;
    }

    if (g_opts.verbose) {
        printf("Tokens: %d\n", tokens->count);
        Token* tok = tokens->head;
        while (tok && tok->type != TOK_EOF) {
            printf("  %s:%d:%d %s",
                   tok->loc.filename, tok->loc.line, tok->loc.column,
                   token_type_str(tok->type));
            if (tok->type == TOK_IDENT || tok->type == TOK_STRING_LIT) {
                printf(" '%s'", tok->value.str_val);
            } else if (tok->type == TOK_INT_LIT) {
                printf(" %lld", (long long)tok->value.int_val);
            }
            printf("\n");
            tok = tok->next;
        }
    }

    /* Phase 2: Parsing */
    if (g_opts.verbose) {
        printf("Parsing...\n");
    }
    AST* ast = rcc_parse(tokens);
    if (g_error_count > 0) {
        fprintf(stderr, "rcc: %d error(s) in parsing\n", g_error_count);
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
        for (DeclList* d = ast->decls; d; d = d->next) {
            Decl* decl = d->decl;
            printf("  %s: %s", decl->name,
                   decl->kind == DECL_FUNC ? "function" :
                   decl->kind == DECL_VAR ? "variable" : "other");
            if (decl->kind == DECL_FUNC && decl->func_body) {
                printf(" (defined)");
            }
            printf("\n");
        }
    }

    /* Phase 3: Semantic analysis */
    if (g_opts.verbose) {
        printf("Semantic analysis...\n");
    }
    if (!rcc_sema(ast)) {
        fprintf(stderr, "rcc: %d error(s) in semantic analysis\n", g_error_count);
        tokenlist_free(tokens);
        return 1;
    }
    if (g_opts.output_format == OUTPUT_DRV &&
        !rcc_validate_driver_policy(ast)) {
        fprintf(stderr, "rcc: driver policy validation failed\n");
        tokenlist_free(tokens);
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
        fprintf(stderr, "rcc: code generation failed with %d error(s)\n",
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
    char unsigned_path[RCC_MAX_PATH + 64];
    const char* emit_path = g_opts.output_file;
    if (final_artifact && !g_opts.emit_unsigned_v3) {
        if (!rcc_create_signing_temp(g_opts.output_file, "rcc-unsigned",
                                     unsigned_path, sizeof(unsigned_path))) {
            perror("rcc: cannot create unsigned staging file");
            return 1;
        }
        emit_path = unsigned_path;
    }
    switch (g_opts.output_format) {
        case OUTPUT_OBJ:
            emit_ok = rcc_emit_obj(mod, g_opts.output_file);
            break;
        case OUTPUT_RIN:
            emit_ok = rcc_emit(mod, emit_path);
            break;
        case OUTPUT_RLL:
            emit_ok = rcc_emit_rll(mod, ast, emit_path);
            break;
        case OUTPUT_DRV:
            emit_ok = rcc_emit_drv(mod, ast, emit_path);
            break;
        case OUTPUT_ASM:
            emit_ok = rcc_emit_asm(mod, emit_path);
            break;
        default:
            fprintf(stderr, "rcc: unsupported output format\n");
            break;
    }
    if (final_artifact && !g_opts.emit_unsigned_v3) {
        if (emit_ok) emit_ok = rcc_run_rinsign(emit_path, g_opts.output_file);
        remove(emit_path);
    }

    if (!emit_ok) {
        fprintf(stderr, "rcc: failed to write output\n");
        codegen_free(mod);
        tokenlist_free(tokens);
        return 1;
    }

    const char* fmt_name =
        g_opts.output_format == OUTPUT_OBJ ? ".ro" :
        g_opts.output_format == OUTPUT_RIN ? ".rin" :
        g_opts.output_format == OUTPUT_RLL ? ".rll" :
        g_opts.output_format == OUTPUT_DRV ? ".drv" :
        g_opts.output_format == OUTPUT_ASM ? ".s" : "?";

    printf("rcc: compiled '%s' -> '%s' (%s)\n", g_opts.input_file, g_opts.output_file, fmt_name);
    printf("      %d tokens, %d declarations, %lu bytes\n",
           tokens->count, decl_count, (unsigned long)(mod->code.size + mod->data.size + 64));

    codegen_free(mod);
    tokenlist_free(tokens);
    rcc_free(pp_source);
    pp_free(pp);
    return 0;
}
