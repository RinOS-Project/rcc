#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static ObjSection* code_section(ObjectFile* object)
{
    for (ObjSection* section = object ? object->sections : NULL;
         section; section = section->next) {
        if (section->type == SECT_CODE) return section;
    }
    return NULL;
}

static bool internal_label(const char* name)
{
    return name && strstr(name, "__rcc_label_") != NULL;
}

static uint64_t function_end(ObjectFile* object, ObjSymbol* function)
{
    ObjSection* code = code_section(object);
    uint64_t end = code ? code->size : 0u;
    if (!object || !code || !function) return 0u;
    for (ObjSymbol* symbol = object->symbols; symbol; symbol = symbol->next) {
        if (symbol->binding != BIND_CODE || symbol->section != function->section ||
            internal_label(symbol->name) || symbol->value <= function->value ||
            symbol->value >= end) continue;
        end = symbol->value;
    }
    return end;
}

static bool contains_bytes(const uint8_t* data, uint64_t size,
                           const uint8_t* pattern, size_t pattern_size)
{
    if (!data || !pattern || pattern_size == 0u || size < pattern_size) {
        return false;
    }
    for (uint64_t offset = 0u; offset <= size - pattern_size; ++offset) {
        if (memcmp(data + offset, pattern, pattern_size) == 0) return true;
    }
    return false;
}

static void verify_pattern(const char* path, uint16_t architecture,
                           const char* function_name,
                           const uint8_t* pattern, size_t pattern_size)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* code;
    ObjSymbol* function;
    uint64_t end;

    assert(object != NULL && object->arch == architecture);
    code = code_section(object);
    function = objfile_find_symbol(object, function_name);
    assert(code != NULL && function != NULL && function->section >= 0);
    end = function_end(object, function);
    assert(end > function->value && end <= code->size);
    assert(contains_bytes(code->data + function->value,
                          end - function->value,
                          pattern, pattern_size));
    objfile_free(object);
}

static void verify(const char* path, uint16_t architecture,
                   const char* function_name)
{
    static const uint8_t x86_move[] = {0x89u, 0xd8u};
    static const uint8_t x64_move[] = {0x48u, 0x89u, 0xd8u};
    verify_pattern(path, architecture, function_name,
                   architecture == ARCH_X64 ? x64_move : x86_move,
                   architecture == ARCH_X64 ? sizeof(x64_move)
                                            : sizeof(x86_move));
}

static void verify_generic(const char* path, uint16_t architecture,
                          const char* function_name)
{
    static const uint8_t x86_move[] = {0x89u, 0xc8u};
    static const uint8_t x64_move[] = {0x4cu, 0x89u, 0xd0u};
    verify_pattern(path, architecture, function_name,
                   architecture == ARCH_X64 ? x64_move : x86_move,
                   architecture == ARCH_X64 ? sizeof(x64_move)
                                            : sizeof(x86_move));
}

static void verify_generic_output(const char* path, uint16_t architecture,
                                   const char* function_name)
{
    static const uint8_t x86_move[] = {0x89u, 0xc1u};
    static const uint8_t x64_move[] = {0x49u, 0x89u, 0xc2u};
    verify_pattern(path, architecture, function_name,
                   architecture == ARCH_X64 ? x64_move : x86_move,
                   architecture == ARCH_X64 ? sizeof(x64_move)
                                            : sizeof(x86_move));
}

int main(int argc, char** argv)
{
    assert(argc == 4);
    static const uint8_t interrupt[] = {0xcdu, 0x80u};
    verify(argv[1], ARCH_X86, "asm_placeholder_move");
    verify(argv[2], ARCH_X64, "asm_placeholder_move");
    verify(argv[3], ARCH_X64, "asm_cpp_placeholder_move");
    verify_generic(argv[1], ARCH_X86, "asm_generic_placeholder_move");
    verify_generic(argv[2], ARCH_X64, "asm_generic_placeholder_move");
    verify_generic(argv[1], ARCH_X86, "asm_general_input_move");
    verify_generic(argv[2], ARCH_X64, "asm_general_input_move");
    verify_generic_output(argv[1], ARCH_X86, "asm_generic_output_move");
    verify_generic_output(argv[2], ARCH_X64, "asm_generic_output_move");
    verify_generic_output(argv[3], ARCH_X64, "asm_cpp_generic_output_move");
    verify_generic_output(argv[1], ARCH_X86, "asm_general_output_move");
    verify_generic_output(argv[2], ARCH_X64, "asm_general_output_move");
    verify_generic(argv[3], ARCH_X64, "asm_cpp_general_input_move");
    verify_generic_output(argv[3], ARCH_X64, "asm_cpp_general_output_move");
    verify_pattern(argv[1], ARCH_X86, "asm_immediate_interrupt",
                   interrupt, sizeof(interrupt));
    verify_pattern(argv[2], ARCH_X64, "asm_immediate_interrupt",
                   interrupt, sizeof(interrupt));
    verify_pattern(argv[3], ARCH_X64, "asm_cpp_immediate_interrupt",
                   interrupt, sizeof(interrupt));
    return 0;
}
