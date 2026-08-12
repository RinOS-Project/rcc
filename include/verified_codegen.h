/*
 * RCC - production bridge for the verified typed-SSA x86 backend
 */

#ifndef RCC_VERIFIED_CODEGEN_H
#define RCC_VERIFIED_CODEGEN_H

#include "ast.h"

typedef enum {
    RCC_VERIFIED_OBJECT_EMITTED,
    RCC_VERIFIED_OBJECT_FALLBACK,
    RCC_VERIFIED_OBJECT_INVALID,
} RccVerifiedObjectStatus;

RccVerifiedObjectStatus rcc_emit_verified_object(
    const AST* ast, const char* translation_unit,
    const char* output_path, size_t* function_count,
    char* reason, size_t reason_size);

#endif /* RCC_VERIFIED_CODEGEN_H */
