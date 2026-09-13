// SPDX-License-Identifier: Apache-2.0
#ifndef RIN_COMPILER_AQC_H
#define RIN_COMPILER_AQC_H

#include <stddef.h>
#include <stdint.h>

#include <ringpu/rin_shader.h>

#define AQC_MAX_SOURCE_SIZE (1024u * 1024u)
#define AQC_MAX_IDENTIFIER 63u
#define AQC_DIAGNOSTIC_MESSAGE_SIZE 192u
#define AQC_MAX_OUTPUT_SIZE \
    (sizeof(RinShaderHeaderV1) + \
     RIN_SHADER_MAX_INSTRUCTIONS * sizeof(RinShaderInstructionV1))

typedef enum AqcResult {
    AQC_OK = 0,
    AQC_ERROR_INVALID_ARGUMENT = -1,
    AQC_ERROR_SOURCE_TOO_LARGE = -2,
    AQC_ERROR_LEXICAL = -3,
    AQC_ERROR_SYNTAX = -4,
    AQC_ERROR_SEMANTIC = -5,
    AQC_ERROR_LIMIT = -6,
    AQC_ERROR_OUTPUT_TOO_SMALL = -7,
    AQC_ERROR_INTERNAL = -8
} AqcResult;

typedef struct AqcDiagnostic {
    uint32_t line;
    uint32_t column;
    char message[AQC_DIAGNOSTIC_MESSAGE_SIZE];
} AqcDiagnostic;

/* RinCompiler entry point. Compiles one complete UTF-8/ASCII .aq source
 * buffer to RSH1. On failure output_size is zero and diagnostic contains the
 * first error. */
int aqc_compile(const char* source, size_t source_size, void* output,
                size_t output_capacity, size_t* output_size,
                AqcDiagnostic* diagnostic);

#endif /* RIN_COMPILER_AQC_H */
