/*
 * RCC - verified i686/AMD64 legal-IR byte encoding
 */

#ifndef RCC_X86_ENCODE_H
#define RCC_X86_ENCODE_H

#include "x86_legalize.h"

typedef enum {
    RCC_X86_CODE_RELOC_REL32,
    RCC_X86_CODE_RELOC_ABS32U,
    RCC_X86_CODE_RELOC_ABS64,
    RCC_X86_CODE_RELOC_CODE_ABS32U,
    RCC_X86_CODE_RELOC_CODE_ABS64,
} RccX86CodeRelocationType;

typedef struct {
    uint32_t offset;
    RccX86CodeRelocationType type;
    int64_t addend;
    char* symbol;
} RccX86CodeRelocation;

typedef struct {
    uint32_t offset;
    uint32_t size;
    /* Borrowed AST statement; valid only while the encoded function's
     * originating AST remains alive. */
    const void* source_statement;
} RccX86CodeSourceRange;

typedef struct {
    RccX86Target target;
    uint8_t* code;
    size_t code_size;
    uint32_t* block_offsets;
    size_t block_count;
    RccX86CodeRelocation* relocations;
    size_t relocation_count;
    RccX86CodeSourceRange* source_ranges;
    size_t source_range_count;
} RccX86EncodedFunction;

bool rcc_x86_encode_function(
    const RccX86LegalFunction* function,
    const RccMirRegisterPolicy* policy,
    RccX86EncodedFunction* encoded,
    char* error, size_t error_size);
bool rcc_x86_verify_encoded_function(
    const RccX86EncodedFunction* encoded,
    char* error, size_t error_size);
void rcc_x86_encoded_function_release(RccX86EncodedFunction* encoded);

#endif /* RCC_X86_ENCODE_H */
