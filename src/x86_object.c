/*
 * RCC - encoded x86 function to .ro v2 object bridge
 */

#include "rcc.h"
#include "x86_object.h"

#include <stdarg.h>

static bool x86_object_error(char* error, size_t error_size,
                             const char* format, ...) {
    if (error && error_size != 0u) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(error, error_size, format, arguments);
        va_end(arguments);
    }
    return false;
}

static int x86_object_section_index(const ObjectFile* object,
                                    const ObjSection* target) {
    const ObjSection* section;
    int index = 0;
    for (section = object->sections; section;
         section = section->next, ++index) {
        if (section == target) return index;
    }
    return -1;
}

bool rcc_x86_object_add_function(
    ObjectFile* object, const char* name, SymbolType symbol_type,
    const RccX86EncodedFunction* encoded,
    char* error, size_t error_size) {
    ObjSection* text;
    ObjSymbol* function_symbol;
    uint16_t expected_arch;
    uint64_t function_offset;
    int section_index;
    char encoded_error[256];
    if (error && error_size != 0u) error[0] = '\0';
    if (!object || !name || !name[0] || !encoded ||
        (symbol_type != SYM_LOCAL && symbol_type != SYM_GLOBAL &&
         symbol_type != SYM_WEAK) ||
        !rcc_x86_verify_encoded_function(
            encoded, encoded_error, sizeof(encoded_error))) {
        return x86_object_error(
            error, error_size, "x86 object function input is invalid");
    }
    expected_arch = encoded->target == RCC_X86_TARGET_X86_64
        ? ARCH_X64 : ARCH_X86;
    if (object->arch != expected_arch) {
        return x86_object_error(
            error, error_size, "x86 object architecture mismatch");
    }
    function_symbol = objfile_find_symbol(object, name);
    if (function_symbol &&
        (function_symbol->type != SYM_UNDEF ||
         function_symbol->binding != BIND_CODE ||
         function_symbol->section != -1)) {
        return x86_object_error(
            error, error_size, "x86 object function symbol is duplicate");
    }
    for (size_t index = 0u; index < encoded->relocation_count; ++index) {
        ObjSymbol* target = objfile_find_symbol(
            object, encoded->relocations[index].symbol);
        if (target && target->binding != BIND_CODE) {
            return x86_object_error(
                error, error_size,
                "x86 call relocation target is not a code symbol");
        }
    }
    text = objfile_get_section(object, ".text");
    if (text &&
        (text->type != SECT_CODE ||
         (text->flags & (SECT_FLAG_ALLOC | SECT_FLAG_EXEC)) !=
             (SECT_FLAG_ALLOC | SECT_FLAG_EXEC) ||
         (text->flags & SECT_FLAG_WRITE) != 0u)) {
        return x86_object_error(
            error, error_size, "x86 object .text contract is invalid");
    }
    if (!text) {
        text = objfile_add_section(
            object, ".text", SECT_CODE,
            SECT_FLAG_ALLOC | SECT_FLAG_EXEC);
    }
    section_align(text, 16u);
    function_offset = section_add_bytes(
        text, encoded->code, encoded->code_size);
    section_index = x86_object_section_index(object, text);
    if (section_index < 0) {
        return x86_object_error(
            error, error_size, "x86 object .text section is detached");
    }
    if (function_symbol) {
        function_symbol->type = symbol_type;
        function_symbol->binding = BIND_CODE;
        function_symbol->section = section_index;
        function_symbol->value = function_offset;
        function_symbol->size = encoded->code_size;
    } else {
        objfile_add_symbol(
            object, name, symbol_type, BIND_CODE, section_index,
            function_offset, encoded->code_size);
    }
    for (size_t index = 0u; index < encoded->relocation_count; ++index) {
        const RccX86CodeRelocation* relocation =
            &encoded->relocations[index];
        if (!objfile_find_symbol(object, relocation->symbol)) {
            objfile_add_symbol(
                object, relocation->symbol, SYM_UNDEF, BIND_CODE,
                -1, 0u, 0u);
        }
        objfile_add_reloc(
            object, section_index,
            function_offset + relocation->offset,
            relocation->symbol, RELOC_REL32, relocation->addend);
    }
    return true;
}
