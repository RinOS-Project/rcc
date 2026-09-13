/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <string.h>

#include "build_manifest.h"

int main(void)
{
    RccBuildManifest manifest;
    CompilerOptions options;
    char error[RCC_BUILD_MANIFEST_ERROR_MAX];
    memset(&manifest, 0, sizeof(manifest));
    memset(&options, 0, sizeof(options));
    memset(error, 0, sizeof(error));

    assert(rcc_manifest_load("tests/build_manifest_valid.rbm", &manifest,
                             error, sizeof(error)));
    assert(manifest.schema == 1u && manifest.target_arch == ARCH_X64);
    assert(manifest.artifact == RCC_MANIFEST_ARTIFACT_LIBRARY);
    assert(manifest.entry_present && strcmp(manifest.entry, "rin_start") == 0);
    assert(manifest.signing_present &&
           manifest.signing_profile == SIGN_PROFILE_RELEASE);
    assert(!rcc_manifest_apply_compiler(&manifest, &options,
                                        error, sizeof(error)));
    assert(strstr(error, "only accepted by rld") != NULL);

    assert(!rcc_manifest_load("tests/build_manifest_invalid.rbm", &manifest,
                              error, sizeof(error)));
    assert(strstr(error, "duplicate target") != NULL);

    assert(!rcc_manifest_load("tests/build_manifest_object_signed.rbm",
                              &manifest, error, sizeof(error)));
    assert(strstr(error, "object artifacts cannot declare signing") != NULL);

    assert(rcc_manifest_load("tests/build_manifest_compiler.rbm", &manifest,
                             error, sizeof(error)));
    memset(&options, 0, sizeof(options));
    options.output_format = OUTPUT_RIN;
    options.target_arch = ARCH_X86;
    assert(rcc_manifest_apply_compiler(&manifest, &options,
                                       error, sizeof(error)));
    assert(options.target_arch == ARCH_X64 && options.target_explicit);
    assert(options.output_format == OUTPUT_RLL &&
           options.output_format_explicit);
    assert(options.signing_profile == SIGN_PROFILE_DEBUG &&
           options.signing_profile_explicit);

    memset(&options, 0, sizeof(options));
    options.output_format = OUTPUT_OBJ;
    options.output_format_explicit = true;
    options.target_arch = ARCH_X64;
    assert(!rcc_manifest_apply_compiler(&manifest, &options,
                                        error, sizeof(error)));
    assert(strstr(error, "artifact conflicts") != NULL);
    return 0;
}
