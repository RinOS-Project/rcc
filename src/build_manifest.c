/* SPDX-License-Identifier: MIT */
#include "build_manifest.h"

#include <ctype.h>
#include <stdarg.h>

#define MANIFEST_LINE_MAX 1024u

static bool manifest_error(char* error, size_t capacity, const char* format, ...)
{
    va_list arguments;
    if (error && capacity != 0u) {
        va_start(arguments, format);
        vsnprintf(error, capacity, format, arguments);
        va_end(arguments);
        error[capacity - 1u] = '\0';
    }
    return false;
}

static char* manifest_trim(char* text)
{
    char* end;
    while (*text && isspace((unsigned char)*text)) ++text;
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) --end;
    *end = '\0';
    return text;
}

static bool manifest_identifier(const char* text)
{
    if (!text || !(isalpha((unsigned char)*text) || *text == '_')) return false;
    for (++text; *text; ++text) {
        if (!(isalnum((unsigned char)*text) || *text == '_')) return false;
    }
    return true;
}

static bool manifest_target(const char* value, TargetArch* target)
{
    if (strcmp(value, RCC_TARGET_I686) == 0) {
        *target = ARCH_X86;
        return true;
    }
    if (strcmp(value, RCC_TARGET_X86_64) == 0) {
        *target = ARCH_X64;
        return true;
    }
    return false;
}

static bool manifest_artifact(const char* value, RccManifestArtifact* artifact)
{
    if (strcmp(value, "executable") == 0) {
        *artifact = RCC_MANIFEST_ARTIFACT_EXECUTABLE;
    } else if (strcmp(value, "library") == 0) {
        *artifact = RCC_MANIFEST_ARTIFACT_LIBRARY;
    } else if (strcmp(value, "driver") == 0) {
        *artifact = RCC_MANIFEST_ARTIFACT_DRIVER;
    } else if (strcmp(value, "object") == 0) {
        *artifact = RCC_MANIFEST_ARTIFACT_OBJECT;
    } else {
        return false;
    }
    return true;
}

bool rcc_manifest_load(const char* path, RccBuildManifest* manifest,
                       char* error, size_t error_capacity)
{
    FILE* file;
    char line[MANIFEST_LINE_MAX];
    unsigned line_number = 0u;
    bool magic_seen = false;
    if (!path || !manifest) {
        return manifest_error(error, error_capacity, "invalid manifest arguments");
    }
    memset(manifest, 0, sizeof(*manifest));
    manifest->struct_size = sizeof(*manifest);
    manifest->schema = RCC_BUILD_MANIFEST_SCHEMA;
    file = fopen(path, "rb");
    if (!file) {
        return manifest_error(error, error_capacity,
                              "cannot open build manifest: %s", path);
    }
    while (fgets(line, sizeof(line), file)) {
        char* text;
        char* equals;
        char* key;
        char* value;
        size_t length;
        ++line_number;
        length = strlen(line);
        if (length == sizeof(line) - 1u && line[length - 1u] != '\n' &&
            !feof(file)) {
            fclose(file);
            return manifest_error(error, error_capacity,
                                  "%s:%u: line is too long", path, line_number);
        }
        text = manifest_trim(line);
        if (*text == '\0' || *text == '#') continue;
        if (!magic_seen) {
            if (strcmp(text, "RIN-BUILD-MANIFEST 1") != 0) {
                fclose(file);
                return manifest_error(
                    error, error_capacity,
                    "%s:%u: expected RIN-BUILD-MANIFEST 1", path, line_number);
            }
            magic_seen = true;
            continue;
        }
        equals = strchr(text, '=');
        if (!equals || strchr(equals + 1, '=')) {
            fclose(file);
            return manifest_error(error, error_capacity,
                                  "%s:%u: expected key = value", path,
                                  line_number);
        }
        *equals = '\0';
        key = manifest_trim(text);
        value = manifest_trim(equals + 1);
        if (*key == '\0' || *value == '\0') {
            fclose(file);
            return manifest_error(error, error_capacity,
                                  "%s:%u: empty key or value", path,
                                  line_number);
        }
        if (strcmp(key, "target") == 0) {
            if (manifest->target_present) {
                fclose(file);
                return manifest_error(error, error_capacity,
                                      "%s:%u: duplicate target", path,
                                      line_number);
            }
            if (!manifest_target(value, &manifest->target_arch)) {
                fclose(file);
                return manifest_error(error, error_capacity,
                                      "%s:%u: unsupported target: %s", path,
                                      line_number, value);
            }
            manifest->target_present = true;
        } else if (strcmp(key, "artifact") == 0) {
            if (manifest->artifact_present) {
                fclose(file);
                return manifest_error(error, error_capacity,
                                      "%s:%u: duplicate artifact", path,
                                      line_number);
            }
            if (!manifest_artifact(value, &manifest->artifact)) {
                fclose(file);
                return manifest_error(error, error_capacity,
                                      "%s:%u: unsupported artifact: %s", path,
                                      line_number, value);
            }
            manifest->artifact_present = true;
        } else if (strcmp(key, "entry") == 0) {
            if (manifest->entry_present || !manifest_identifier(value) ||
                strlen(value) >= sizeof(manifest->entry)) {
                fclose(file);
                return manifest_error(error, error_capacity,
                                      "%s:%u: invalid or duplicate entry", path,
                                      line_number);
            }
            strcpy(manifest->entry, value);
            manifest->entry_present = true;
        } else {
            fclose(file);
            return manifest_error(error, error_capacity,
                                  "%s:%u: unknown key: %s", path,
                                  line_number, key);
        }
    }
    if (ferror(file)) {
        fclose(file);
        return manifest_error(error, error_capacity,
                              "cannot read build manifest: %s", path);
    }
    fclose(file);
    if (!magic_seen || !manifest->target_present ||
        !manifest->artifact_present) {
        return manifest_error(error, error_capacity,
                              "%s: target and artifact are required", path);
    }
    return true;
}

bool rcc_manifest_apply_compiler(const RccBuildManifest* manifest,
                                 CompilerOptions* options,
                                 char* error, size_t error_capacity)
{
    OutputFormat format;
    if (!manifest || manifest->struct_size != sizeof(*manifest) ||
        manifest->schema != RCC_BUILD_MANIFEST_SCHEMA || !options ||
        !manifest->target_present || !manifest->artifact_present) {
        return manifest_error(error, error_capacity, "invalid build manifest");
    }
    if (manifest->entry_present) {
        return manifest_error(error, error_capacity,
                              "entry is only accepted by rld");
    }
    if (options->preprocess_only) {
        return manifest_error(error, error_capacity,
                              "build manifest cannot be combined with -E");
    }
    switch (manifest->artifact) {
        case RCC_MANIFEST_ARTIFACT_EXECUTABLE: format = OUTPUT_RIN; break;
        case RCC_MANIFEST_ARTIFACT_LIBRARY: format = OUTPUT_RLL; break;
        case RCC_MANIFEST_ARTIFACT_DRIVER: format = OUTPUT_DRV; break;
        case RCC_MANIFEST_ARTIFACT_OBJECT: format = OUTPUT_OBJ; break;
        default:
            return manifest_error(error, error_capacity,
                                  "unsupported compiler artifact");
    }
    if (options->target_explicit &&
        options->target_arch != manifest->target_arch) {
        return manifest_error(error, error_capacity,
                              "CLI target conflicts with build manifest");
    }
    if (options->output_format_explicit && options->output_format != format) {
        return manifest_error(error, error_capacity,
                              "CLI artifact conflicts with build manifest");
    }
    options->target_arch = manifest->target_arch;
    options->target_explicit = true;
    options->output_format = format;
    options->output_format_explicit = true;
    return true;
}
