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
    RCC_X86_CODE_RELOC_TLSOFF32S,
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
    /* Borrowed AST declaration; valid while the originating AST is alive. */
    const void* declaration;
    /* DWARF frame-base-relative byte offset after final frame layout. */
    int64_t frame_offset;
    /* Runtime alignment when the source local exceeds the ABI stack alignment. */
    uint32_t alignment;
} RccX86CodeLocalLocation;

typedef struct {
    /* PC at the return instruction and the first PC after that instruction. */
    uint32_t return_pc;
    uint32_t resume_pc;
} RccX86CodeEpilogue;

typedef struct {
    RccX86HardwareGpr gpr;
    /* Bytes below the frame pointer where the incoming value is saved. */
    uint32_t frame_offset;
    /* First PC after the store that makes the saved-value rule valid. */
    uint32_t save_pc;
} RccX86CodeCalleeSave;

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
    RccX86CodeLocalLocation* local_locations;
    size_t local_location_count;
    RccX86CodeEpilogue* epilogues;
    size_t epilogue_count;
    RccX86CodeCalleeSave* callee_saves;
    size_t callee_save_count;
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
