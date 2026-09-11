/*
 * RCC - encoded x86 function to .ro v2 object bridge
 */

#ifndef RCC_X86_OBJECT_H
#define RCC_X86_OBJECT_H

#include "objfile.h"
#include "x86_encode.h"

bool rcc_x86_object_add_function(
    ObjectFile* object, const char* name, SymbolType symbol_type,
    const RccX86EncodedFunction* encoded,
    char* error, size_t error_size);

#endif /* RCC_X86_OBJECT_H */
