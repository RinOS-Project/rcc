/*
 * RCC - typed SSA to verified x86 encoding pipeline
 */

#ifndef RCC_X86_PIPELINE_H
#define RCC_X86_PIPELINE_H

#include "ir.h"
#include "x86_encode.h"

bool rcc_x86_encode_ir_function(
    const RccIrFunction* function, RccX86Target target,
    RccX86EncodedFunction* encoded_out,
    char* error, size_t error_size);

#endif /* RCC_X86_PIPELINE_H */
