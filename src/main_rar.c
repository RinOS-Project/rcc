/*
 * RAR - RinOS Archiver
 * Main entry point
 */

#include "rcc.h"
#include "archive.h"
#include <getopt.h>
#include <string.h>

typedef enum {
    RAR_CREATE,     /* Create archive */
    RAR_EXTRACT,    /* Extract member */
    RAR_LIST,       /* List contents */
    RAR_HELP        /* Show help */
} RarMode;

static RarMode g_mode = RAR_CREATE;
static bool g_verbose = false;
static char* g_output = NULL;
static char* g_member = NULL;  /* For extract */

static void print_usage(void) {
    printf("RAR - RinOS Archiver v%d.%d.%d\n",
           RCC_VERSION_MAJOR, RCC_VERSION_MINOR, RCC_VERSION_PATCH);
    printf("\n");
    printf("Usage: rar <command> [options] <archive> [files...]\n");
    printf("\n");
    printf("Commands:\n");
    printf("  r, create     Create archive from object files\n");
    printf("  x, extract    Extract member from archive\n");
    printf("  t, list       List archive contents\n");
    printf("\n");
    printf("Options:\n");
    printf("  -o <file>     Output file (for extract)\n");
    printf("  -v            Verbose output\n");
    printf("  -h, --help    Show this help\n");
    printf("  --version     Show version\n");
    printf("\n");
    printf("Examples:\n");
    printf("  rar r libfoo.ra foo.ro bar.ro    Create archive\n");
    printf("  rar t libfoo.ra                  List contents\n");
    printf("  rar x libfoo.ra foo.ro           Extract member\n");
}

static int parse_args(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return -1;
    }

    /* Parse command */
    const char* cmd = argv[1];
    if (strcmp(cmd, "r") == 0 || strcmp(cmd, "create") == 0) {
        g_mode = RAR_CREATE;
    } else if (strcmp(cmd, "x") == 0 || strcmp(cmd, "extract") == 0) {
        g_mode = RAR_EXTRACT;
    } else if (strcmp(cmd, "t") == 0 || strcmp(cmd, "list") == 0) {
        g_mode = RAR_LIST;
    } else if (strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0) {
        g_mode = RAR_HELP;
        return 0;
    } else if (strcmp(cmd, "--version") == 0) {
        printf("rar %d.%d.%d\n",
               RCC_VERSION_MAJOR, RCC_VERSION_MINOR, RCC_VERSION_PATCH);
        exit(0);
    } else {
        fprintf(stderr, "rar: unknown command: %s\n", cmd);
        return -1;
    }

    /* Parse options after command */
    static struct option long_options[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},
        {0, 0, 0, 0}
    };

    optind = 2;  /* Start after command */
    int opt;
    while ((opt = getopt_long(argc, argv, "o:vh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'o':
                g_output = optarg;
                break;
            case 'v':
                g_verbose = true;
                break;
            case 'h':
                g_mode = RAR_HELP;
                return 0;
            case 'V':
                printf("rar %d.%d.%d\n",
                       RCC_VERSION_MAJOR, RCC_VERSION_MINOR, RCC_VERSION_PATCH);
                exit(0);
            default:
                return -1;
        }
    }

    return 0;
}

int main(int argc, char** argv) {
    if (parse_args(argc, argv) < 0) {
        return 1;
    }

    if (g_mode == RAR_HELP) {
        print_usage();
        return 0;
    }

    /* Get remaining arguments */
    int arg_start = optind;
    int arg_count = argc - arg_start;

    if (arg_count < 1) {
        fprintf(stderr, "rar: missing archive name\n");
        return 1;
    }

    char* archive = argv[arg_start];

    switch (g_mode) {
        case RAR_CREATE: {
            if (arg_count < 2) {
                fprintf(stderr, "rar: no input files\n");
                return 1;
            }
            char** inputs = &argv[arg_start + 1];
            int input_count = arg_count - 1;

            if (g_verbose) {
                printf("Creating archive: %s\n", archive);
                for (int i = 0; i < input_count; i++) {
                    printf("  + %s\n", inputs[i]);
                }
            }

            if (!rar_create(archive, inputs, input_count)) {
                return 1;
            }

            printf("rar: created '%s' (%d member%s)\n",
                   archive, input_count, input_count == 1 ? "" : "s");
            break;
        }

        case RAR_LIST: {
            if (!rar_list(archive)) {
                return 1;
            }
            break;
        }

        case RAR_EXTRACT: {
            if (arg_count < 2) {
                fprintf(stderr, "rar: specify member to extract\n");
                return 1;
            }
            char* member = argv[arg_start + 1];

            if (!rar_extract(archive, member, g_output)) {
                return 1;
            }
            break;
        }

        default:
            break;
    }

    return 0;
}
