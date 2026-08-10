/* SPDX-License-Identifier: MIT */
#ifndef RCC_BUILD_MANIFEST_H
#define RCC_BUILD_MANIFEST_H

#include "rcc.h"

#define RCC_BUILD_MANIFEST_SCHEMA 1u
#define RCC_BUILD_MANIFEST_ERROR_MAX 256u

typedef enum RccManifestArtifact {
    RCC_MANIFEST_ARTIFACT_INVALID = 0,
    RCC_MANIFEST_ARTIFACT_EXECUTABLE = 1,
    RCC_MANIFEST_ARTIFACT_LIBRARY = 2,
    RCC_MANIFEST_ARTIFACT_DRIVER = 3,
    RCC_MANIFEST_ARTIFACT_OBJECT = 4
} RccManifestArtifact;

typedef struct RccBuildManifest {
    uint32_t struct_size;
    uint32_t schema;
    bool target_present;
    bool artifact_present;
    bool entry_present;
    TargetArch target_arch;
    RccManifestArtifact artifact;
    char entry[RCC_MAX_IDENT];
} RccBuildManifest;

bool rcc_manifest_load(const char* path, RccBuildManifest* manifest,
                       char* error, size_t error_capacity);

bool rcc_manifest_apply_compiler(const RccBuildManifest* manifest,
                                 CompilerOptions* options,
                                 char* error, size_t error_capacity);

#endif /* RCC_BUILD_MANIFEST_H */
