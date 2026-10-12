/*
 * RLD - RinOS Linker
 * Main entry point
 */

#include "rcc.h"
#include "linker.h"
#include "build_manifest.h"
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <string.h>

static bool rld_valid_export_name(const char* name)
{
    size_t length;
    if (!name || !name[0]) return false;
    length = strlen(name);
    if (length > 127u ||
        !((name[0] >= 'A' && name[0] <= 'Z') ||
          (name[0] >= 'a' && name[0] <= 'z') || name[0] == '_')) {
        return false;
    }
    for (size_t index = 1u; index < length; ++index) {
        char value = name[index];
        if (!((value >= 'A' && value <= 'Z') ||
              (value >= 'a' && value <= 'z') ||
              (value >= '0' && value <= '9') || value == '_' ||
              value == '.' || value == '$')) {
            return false;
        }
    }
    return true;
}

static bool rld_valid_export_version(const char* version)
{
    size_t length;
    if (!version || !version[0]) return false;
    length = strlen(version);
    if (length > 63u) return false;
    for (size_t index = 0u; index < length; ++index) {
        unsigned char value = (unsigned char)version[index];
        if (value < 0x21u || value > 0x7eu) return false;
    }
    return true;
}

static bool rld_parse_export(char* value, LinkExportSpec* spec)
{
    char* separator = strchr(value, '@');
    const char* version = NULL;
    if (separator != NULL) {
        *separator = '\0';
        version = separator + 1;
    }
    if (!rld_valid_export_name(value) ||
        (separator != NULL && !rld_valid_export_version(version))) {
        return false;
    }
    spec->symbol = value;
    spec->version = version;
    return true;
}

static void print_usage(void) {
    printf("RLD - RinOS Linker v%d.%d.%d\n",
           RCC_VERSION_MAJOR, RCC_VERSION_MINOR, RCC_VERSION_PATCH);
    printf("\n");
    printf("Usage: rld [options] <input.ro...>\n");
    printf("\n");
    printf("Options:\n");
    printf("  -o <file>     Output file name (default: a.rin)\n");
    printf("  -e <symbol>   Entry point symbol (default: main)\n");
    printf("  -T <addr>     Base address (default: 0x10000)\n");
    printf("  -shared       Create shared library (.rll)\n");
    printf("  -m32          Link for 32-bit (default)\n");
    printf("  -m64          Link for 64-bit\n");
    printf("  --target <triple>  i686-unknown-rinos or x86_64-unknown-rinos\n");
    printf("  --manifest <file>  RIN-BUILD-MANIFEST 1 build contract\n");
    printf("  --rinsign/--sign-key/--public-key  Required final v3 signing inputs\n");
    printf("  --sign-profile <p>               Build profile: debug or release\n");
    printf("  --dep <name.rll>                  Add a signed dependency\n");
    printf("  --import <symbol>=<dep>@<kind>    Add function/data import\n");
    printf("  --export <symbol>[@<version>]     Add a shared-library export\n");
    printf("  -v            Verbose output\n");
    printf("  -h, --help    Show this help\n");
    printf("  --version     Show version\n");
    printf("\n");
    printf("Examples:\n");
    printf("  rld -o app.rin main.ro utils.ro\n");
    printf("  rld -shared -o lib.rll module.ro\n");
}

static int parse_args(int argc, char** argv) {
    /* Default options */
    memset(&g_linker_opts, 0, sizeof(g_linker_opts));
    g_linker_opts.arch = ARCH_X86;
    g_linker_opts.base_addr = 0x10000;
    g_linker_opts.entry = "main";

    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},
        {"shared", no_argument, 0, 1},
        {"target", required_argument, 0, 2},
        {"rinsign", required_argument, 0, 3},
        {"sign-key", required_argument, 0, 4},
        {"public-key", required_argument, 0, 5},
        {"python", required_argument, 0, 6},
        {"emit-unsigned-v3", no_argument, 0, 7},
        {"dep", required_argument, 0, 8},
        {"import", required_argument, 0, 9},
        {"manifest", required_argument, 0, 10},
        {"sign-profile", required_argument, 0, 11},
        {"export", required_argument, 0, 12},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long_only(argc, argv, "o:e:T:m:vh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'o':
                if (!rcc_copy_path(g_linker_opts.output_file,
                                   sizeof(g_linker_opts.output_file), optarg)) {
                    fprintf(stderr, "rld: error: output path is too long (maximum %u bytes)\n",
                            (unsigned)(sizeof(g_linker_opts.output_file) - 1u));
                    return -1;
                }
                g_linker_opts.output_explicit = true;
                break;
            case 'e':
                g_linker_opts.entry = optarg;
                g_linker_opts.entry_explicit = true;
                break;
            case 'T': {
                char* end = NULL;
                unsigned long long value;
                errno = 0;
                value = strtoull(optarg, &end, 0);
                if (errno == ERANGE || end == optarg || !end || *end != '\0') {
                    fprintf(stderr, "rld: invalid base address: %s\n", optarg);
                    return -1;
                }
                g_linker_opts.base_addr = (uint64_t)value;
                break;
            }
            case 'm':
                if (strcmp(optarg, "32") == 0) {
                    if (g_linker_opts.arch_explicit && g_linker_opts.arch != ARCH_X86) {
                        fprintf(stderr, "rld: conflicting target selections\n");
                        return -1;
                    }
                    g_linker_opts.arch = ARCH_X86;
                    g_linker_opts.arch_explicit = true;
                } else if (strcmp(optarg, "64") == 0) {
                    if (g_linker_opts.arch_explicit && g_linker_opts.arch != ARCH_X64) {
                        fprintf(stderr, "rld: conflicting target selections\n");
                        return -1;
                    }
                    g_linker_opts.arch = ARCH_X64;
                    g_linker_opts.arch_explicit = true;
                } else {
                    fprintf(stderr, "rld: unknown architecture: -m%s\n", optarg);
                    return -1;
                }
                break;
            case 'v':
                g_linker_opts.verbose = true;
                break;
            case 'h':
                print_usage();
                exit(0);
            case 'V':
                printf("rld %d.%d.%d\n",
                       RCC_VERSION_MAJOR, RCC_VERSION_MINOR, RCC_VERSION_PATCH);
                exit(0);
            case 1:  /* --shared */
                g_linker_opts.shared = true;
                g_linker_opts.shared_explicit = true;
                break;
            case 2: { /* --target */
                TargetArch target_arch;
                if (!rcc_parse_target_triple(optarg, &target_arch)) {
                    fprintf(stderr, "rld: unsupported target triple: %s\n", optarg);
                    return -1;
                }
                if (g_linker_opts.arch_explicit && g_linker_opts.arch != target_arch) {
                    fprintf(stderr, "rld: conflicting target selections\n");
                    return -1;
                }
                g_linker_opts.arch = target_arch;
                g_linker_opts.arch_explicit = true;
                break;
            }
            case 10:
                g_linker_opts.manifest_path = optarg;
                break;
            case 11:
                if (!rcc_parse_signing_profile(optarg,
                                               &g_opts.signing_profile)) {
                    fprintf(stderr, "rld: signing profile must be debug or release\n");
                    return -1;
                }
                g_opts.signing_profile_explicit = true;
                break;
            case 3: g_opts.rinsign_path = optarg; break;
            case 4: g_opts.sign_key = optarg; break;
            case 5: g_opts.public_key = optarg; break;
            case 6: g_opts.python_path = optarg; break;
            case 7: g_opts.emit_unsigned_v3 = true; break;
            case 8:
                if (g_linker_opts.dependency_count >= RLD_MAX_DEPENDENCIES) {
                    fprintf(stderr, "rld: too many dependencies\n");
                    return -1;
                }
                g_linker_opts.dependencies[g_linker_opts.dependency_count++] = optarg;
                break;
            case 9: {
                char* equals;
                char* at;
                LinkImportSpec* import;
                if (g_linker_opts.import_count >= RLD_MAX_IMPORTS) {
                    fprintf(stderr, "rld: too many imports\n");
                    return -1;
                }
                equals = strchr(optarg, '=');
                at = equals ? strrchr(equals + 1, '@') : NULL;
                if (!equals || equals == optarg || !at || at == equals + 1 ||
                    at[1] == '\0') {
                    fprintf(stderr, "rld: invalid --import syntax: %s\n", optarg);
                    return -1;
                }
                *equals = '\0';
                *at = '\0';
                import = &g_linker_opts.imports[g_linker_opts.import_count++];
                import->symbol = optarg;
                import->dependency = equals + 1;
                if (strcmp(at + 1, "function") == 0) {
                    import->kind = RIN_SYMBOL_FUNCTION;
                } else if (strcmp(at + 1, "data") == 0) {
                    import->kind = RIN_SYMBOL_DATA;
                } else {
                    fprintf(stderr, "rld: import kind must be function or data\n");
                    return -1;
                }
                break;
            }
            case 12:
                if (g_linker_opts.export_count >= RLD_MAX_EXPORTS) {
                    fprintf(stderr, "rld: too many exports\n");
                    return -1;
                }
                if (!rld_parse_export(optarg,
                                      &g_linker_opts.exports[
                                          g_linker_opts.export_count])) {
                    fprintf(stderr, "rld: invalid --export syntax: %s\n", optarg);
                    return -1;
                }
                ++g_linker_opts.export_count;
                break;
            default:
                return -1;
        }
    }

    /* Get input files */
    if (optind >= argc) {
        fprintf(stderr, "rld: error: no input files\n");
        return -1;
    }

    g_linker_opts.input_files = &argv[optind];
    g_linker_opts.input_count = argc - optind;

    if (g_linker_opts.manifest_path) {
        RccBuildManifest manifest;
        char error[RCC_BUILD_MANIFEST_ERROR_MAX];
        bool manifest_shared;
        if (!rcc_manifest_load(g_linker_opts.manifest_path, &manifest,
                               error, sizeof(error))) {
            fprintf(stderr, "rld: error: %s\n", error);
            return -1;
        }
        if (manifest.artifact != RCC_MANIFEST_ARTIFACT_EXECUTABLE &&
            manifest.artifact != RCC_MANIFEST_ARTIFACT_LIBRARY) {
            fprintf(stderr, "rld: error: manifest artifact is not linkable by rld\n");
            return -1;
        }
        manifest_shared = manifest.artifact == RCC_MANIFEST_ARTIFACT_LIBRARY;
        if (g_linker_opts.arch_explicit &&
            g_linker_opts.arch != manifest.target_arch) {
            fprintf(stderr, "rld: error: CLI target conflicts with build manifest\n");
            return -1;
        }
        if (g_linker_opts.shared_explicit &&
            g_linker_opts.shared != manifest_shared) {
            fprintf(stderr, "rld: error: CLI artifact conflicts with build manifest\n");
            return -1;
        }
        if (g_linker_opts.entry_explicit && manifest.entry_present &&
            strcmp(g_linker_opts.entry, manifest.entry) != 0) {
            fprintf(stderr, "rld: error: CLI entry conflicts with build manifest\n");
            return -1;
        }
        g_linker_opts.arch = manifest.target_arch;
        g_linker_opts.arch_explicit = true;
        g_linker_opts.shared = manifest_shared;
        g_linker_opts.shared_explicit = true;
        if (manifest.entry_present) {
            if (!rcc_copy_path(g_linker_opts.manifest_entry,
                               sizeof(g_linker_opts.manifest_entry),
                               manifest.entry)) {
                fprintf(stderr, "rld: error: manifest entry is too long\n");
                return -1;
            }
            g_linker_opts.entry = g_linker_opts.manifest_entry;
        }
        if (!rcc_manifest_apply_signing(&manifest, &g_opts,
                                        error, sizeof(error))) {
            fprintf(stderr, "rld: error: %s\n", error);
            return -1;
        }
    }
    if (!g_linker_opts.output_explicit) {
        strcpy(g_linker_opts.output_file,
               g_linker_opts.shared ? "a.rll" : "a.rin");
    }

    if (g_linker_opts.export_count != 0 && !g_linker_opts.shared) {
        fprintf(stderr, "rld: --export is only valid with --shared\n");
        return -1;
    }

    if (!rcc_validate_signing_options("rld", true)) {
        return -1;
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

    if (g_linker_opts.verbose) {
        printf("Output: %s\n", g_linker_opts.output_file);
        printf("Target: %s\n",
               g_linker_opts.arch_explicit ? rcc_target_triple(g_linker_opts.arch)
                                           : "auto (from .ro v2)");
        printf("Base:   0x%" PRIx64 "\n", g_linker_opts.base_addr);
        printf("Entry:  %s\n", g_linker_opts.entry);
        printf("Input:  %d files\n", g_linker_opts.input_count);
    }

    char unsigned_path[RCC_MAX_PATH + 64];
    const char* link_output = g_linker_opts.output_file;
    if (!g_opts.emit_unsigned_v3) {
        if (!rcc_create_signing_temp(g_linker_opts.output_file, "rld-unsigned",
                                     unsigned_path, sizeof(unsigned_path))) {
            perror("rld: cannot create unsigned staging file");
            return 1;
        }
        link_output = unsigned_path;
    }
    if (!rld_link(g_linker_opts.input_files, g_linker_opts.input_count,
                  link_output)) {
        if (!g_opts.emit_unsigned_v3) remove(link_output);
        return 1;
    }
    if (!g_opts.emit_unsigned_v3) {
        bool signed_ok = rcc_run_rinsign(link_output, g_linker_opts.output_file);
        remove(link_output);
        if (!signed_ok) return 1;
    }

    printf("rld: linked '%s' (%d input file%s)\n",
           g_linker_opts.output_file,
           g_linker_opts.input_count,
           g_linker_opts.input_count == 1 ? "" : "s");

    return 0;
}
