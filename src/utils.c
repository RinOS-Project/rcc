#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

/*
 * RCC - RinOS C Compiler
 * Utility functions and global state
 */

#include "rcc.h"
#include "rin_formats_v3.h"
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <sys/stat.h>
#include <windows.h>
#else
#include <sys/stat.h>
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

bool rcc_parse_optimization_level(const char* value, int* level_out) {
    if (!value || !level_out || value[0] < '0' || value[0] > '3' ||
        value[1] != '\0') {
        return false;
    }
    *level_out = value[0] - '0';
    return true;
}

bool rcc_parse_signing_profile(const char* value, SigningProfile* profile_out) {
    if (!value || !profile_out) return false;
    if (strcmp(value, "debug") == 0) {
        *profile_out = SIGN_PROFILE_DEBUG;
        return true;
    }
    if (strcmp(value, "release") == 0) {
        *profile_out = SIGN_PROFILE_RELEASE;
        return true;
    }
    return false;
}

const char* rcc_signing_profile_name(SigningProfile profile) {
    switch (profile) {
        case SIGN_PROFILE_DEBUG: return "debug";
        case SIGN_PROFILE_RELEASE: return "release";
        default: return "unspecified";
    }
}

static bool string_present(const char* value) {
    return value && value[0] != '\0';
}

bool rcc_validate_signing_options(const char* tool_name, bool final_artifact) {
    const char* tool = string_present(tool_name) ? tool_name : "rcc";
    bool has_signing_program = string_present(g_opts.rinsign_path) ||
                               string_present(g_opts.sign_key) ||
                               string_present(g_opts.public_key) ||
                               string_present(g_opts.python_path);
    bool has_signing_input = has_signing_program ||
                             g_opts.signing_profile_explicit;
    if (g_opts.emit_unsigned_v3) {
        if (has_signing_program) {
            fprintf(stderr,
                    "%s: --emit-unsigned-v3 cannot be combined with signer or key options\n",
                    tool);
            return false;
        }
        return true;
    }
    if (!final_artifact) {
        if (has_signing_input) {
            fprintf(stderr, "%s: signing options require a final v3 artifact\n",
                    tool);
            return false;
        }
        return true;
    }
    if (g_opts.signing_profile == SIGN_PROFILE_UNSPECIFIED) {
        fprintf(stderr,
                "%s: final v3 output requires --sign-profile debug or release\n",
                tool);
        return false;
    }
    if (!string_present(g_opts.rinsign_path) ||
        !string_present(g_opts.sign_key) ||
        !string_present(g_opts.public_key)) {
        fprintf(stderr,
                "%s: final v3 output requires --rinsign, --sign-key and --public-key\n",
                tool);
        return false;
    }
    return true;
}

bool rcc_create_signing_temp(const char* output_path, const char* stage,
                             char* temp_path, size_t capacity) {
    int written;
    int descriptor;
    if (!string_present(output_path) || !string_present(stage) || !temp_path ||
        capacity == 0u || strchr(stage, '/') || strchr(stage, '\\')) {
        errno = EINVAL;
        return false;
    }
    written = snprintf(temp_path, capacity, "%s.%s-XXXXXX", output_path, stage);
    if (written < 0 || (size_t)written >= capacity) {
        errno = ENAMETOOLONG;
        return false;
    }
#if defined(_WIN32)
    if (_mktemp_s(temp_path, capacity) != 0) return false;
    descriptor = _open(temp_path,
                       _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY,
                       _S_IREAD | _S_IWRITE);
#else
    descriptor = mkstemp(temp_path);
#endif
    if (descriptor < 0) return false;
#if defined(_WIN32)
    if (_close(descriptor) != 0) {
#else
    if (close(descriptor) != 0) {
#endif
        int saved_errno = errno;
        remove(temp_path);
        errno = saved_errno;
        return false;
    }
    return true;
}

static bool invoke_rinsign(const char* unsigned_path, const char* signed_path) {
    const char* python = g_opts.python_path ? g_opts.python_path : "python3";
    const char* const arguments[] = {
        python, g_opts.rinsign_path, unsigned_path, "-o", signed_path,
        "--key", g_opts.sign_key, "--public-key", g_opts.public_key, NULL
    };
    int status;

    if (!string_present(unsigned_path) || !string_present(signed_path) ||
        !string_present(g_opts.rinsign_path) ||
        !string_present(g_opts.sign_key) ||
        !string_present(g_opts.public_key)) {
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
    for (;;) {
        pid_t waited = waitpid(pid, &status, 0);
        if (waited == pid) break;
        if (waited < 0 && errno == EINTR) continue;
        perror("rcc: cannot wait for rinsign");
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

static bool publish_signed_output(const char* staged_path,
                                  const char* output_path) {
#if defined(_WIN32)
    if (!MoveFileExA(staged_path, output_path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        errno = EIO;
        return false;
    }
    return true;
#else
    return rename(staged_path, output_path) == 0;
#endif
}

static uint16_t read_le16(const uint8_t* bytes) {
    return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static uint32_t read_le32(const uint8_t* bytes) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static bool validate_signed_staging(const char* path) {
    union {
        RinHeaderV3 image;
        RinDriverHeaderV3 driver;
        uint8_t bytes[256];
    } header;
    uint8_t envelope[48];
    uint64_t signature_offset;
    uint32_t signature_size;
    uint16_t signature_algorithm;
    uint16_t hash_algorithm;
    const uint8_t* content_hash;
    uint32_t signed_flag;
    uint32_t flags;
    uint64_t file_size;
    bool hash_present = false;
    bool signer_identity_present = false;
    FILE* file = fopen(path, "rb");
    long end;
    size_t i;
    if (!file) return false;
    if (fseek(file, 0, SEEK_END) != 0 || (end = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    file_size = (uint64_t)(unsigned long)end;
    if (file_size < sizeof(header.bytes) + sizeof(envelope) ||
        fread(header.bytes, 1, sizeof(header.bytes), file) !=
            sizeof(header.bytes)) {
        fclose(file);
        return false;
    }
    if (header.image.magic == RIN_IMAGE_MAGIC &&
        header.image.version == RIN_IMAGE_VERSION_3 &&
        header.image.header_size == sizeof(RinHeaderV3)) {
        flags = header.image.flags;
        signed_flag = RIN_IMAGE_SIGNED;
        signature_offset = header.image.signature_offset;
        signature_size = header.image.signature_size;
        signature_algorithm = header.image.signature_algorithm;
        hash_algorithm = header.image.hash_algorithm;
        content_hash = header.image.content_hash;
    } else if (header.driver.magic == RIN_DRIVER_IMAGE_MAGIC &&
               header.driver.version == RIN_DRIVER_IMAGE_VERSION_3 &&
               header.driver.header_size == sizeof(RinDriverHeaderV3)) {
        flags = header.driver.flags;
        signed_flag = RIN_DRIVER_IMAGE_SIGNED;
        signature_offset = header.driver.signature_offset;
        signature_size = header.driver.signature_size;
        signature_algorithm = header.driver.signature_algorithm;
        hash_algorithm = header.driver.hash_algorithm;
        content_hash = header.driver.content_hash;
    } else {
        fclose(file);
        return false;
    }
    for (i = 0; i < 32u; ++i) hash_present |= content_hash[i] != 0u;
    if (!(flags & signed_flag) || !hash_present ||
        signature_algorithm != RIN_IMAGE_SIGNATURE_RSA_PKCS1_SHA256 ||
        hash_algorithm != RIN_IMAGE_HASH_SHA256 ||
        signature_offset < sizeof(header.bytes) ||
        signature_size < sizeof(envelope) ||
        signature_offset > file_size ||
        (uint64_t)signature_size > file_size - signature_offset ||
        signature_offset + signature_size != file_size ||
        signature_offset > (uint64_t)LONG_MAX ||
        fseek(file, (long)signature_offset, SEEK_SET) != 0 ||
        fread(envelope, 1, sizeof(envelope), file) != sizeof(envelope)) {
        fclose(file);
        return false;
    }
    fclose(file);
    for (i = 12u; i < 44u; ++i) {
        signer_identity_present |= envelope[i] != 0u;
    }
    return read_le32(envelope) == UINT32_C(0x31534452) &&
           read_le16(envelope + 4) == 1u &&
           read_le16(envelope + 6) == sizeof(envelope) &&
           read_le16(envelope + 8) == signature_algorithm &&
           read_le16(envelope + 10) >= 256u &&
           read_le16(envelope + 10) <= 512u &&
           signer_identity_present &&
           signature_size == sizeof(envelope) + read_le16(envelope + 10) &&
           read_le32(envelope + 44) == 0u;
}

bool rcc_run_rinsign(const char* unsigned_path, const char* output_path) {
    char signed_path[RCC_MAX_PATH + 64];
    bool signed_ok;
    if (!string_present(unsigned_path) || !string_present(output_path) ||
        strcmp(unsigned_path, output_path) == 0) {
        fprintf(stderr, "rcc: invalid signing input/output paths\n");
        return false;
    }
    if (!rcc_create_signing_temp(output_path, "rcc-signed", signed_path,
                                 sizeof(signed_path))) {
        perror("rcc: cannot create signed staging file");
        return false;
    }
    signed_ok = invoke_rinsign(unsigned_path, signed_path);
    if (signed_ok && !validate_signed_staging(signed_path)) {
        fprintf(stderr, "rcc: rinsign produced an invalid signed v3 artifact\n");
        signed_ok = false;
    }
    if (signed_ok && !publish_signed_output(signed_path, output_path)) {
        perror("rcc: cannot publish signed output");
        signed_ok = false;
    }
    if (!signed_ok) remove(signed_path);
    return signed_ok;
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

bool rcc_tool_relative_path(const char* tool_path, const char* relative_path,
                            char* output, size_t output_size) {
    const char* slash;
    const char* backslash;
    const char* separator;
    size_t directory_size;
    size_t relative_size;

    if (!tool_path || !relative_path || !output || output_size == 0u) {
        return false;
    }
    slash = strrchr(tool_path, '/');
    backslash = strrchr(tool_path, '\\');
    separator = slash;
    if (backslash && (!separator || backslash > separator)) {
        separator = backslash;
    }
    if (!separator) return false;
    directory_size = (size_t)(separator - tool_path) + 1u;
    relative_size = strlen(relative_path);
    if (directory_size > output_size - 1u ||
        relative_size > output_size - directory_size - 1u) {
        return false;
    }
    memcpy(output, tool_path, directory_size);
    memcpy(output + directory_size, relative_path, relative_size + 1u);
    return true;
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
