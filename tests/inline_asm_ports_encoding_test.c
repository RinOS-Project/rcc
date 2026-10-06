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

static uint64_t function_end(ObjectFile* object, ObjSymbol* function)
{
    ObjSection* code = code_section(object);
    uint64_t end = code ? code->size : 0u;
    if (!object || !code || !function) return 0u;
    for (ObjSymbol* symbol = object->symbols; symbol; symbol = symbol->next) {
        if (symbol->binding != BIND_CODE || symbol->section != function->section ||
            strstr(symbol->name, "__rcc_label_") != NULL ||
            symbol->value <= function->value || symbol->value >= end) continue;
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

static void verify(const char* path, uint16_t architecture,
                   const char* function_name, const uint8_t* pattern,
                   size_t pattern_size)
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
                          end - function->value, pattern, pattern_size));
    objfile_free(object);
}

static void verify_architecture(const char* path, uint16_t architecture)
{
    static const uint8_t inb_const[] = {0xE4u, 0x60u};
    static const uint8_t inw_const[] = {0x66u, 0xE5u, 0x61u};
    static const uint8_t inl_const[] = {0xE5u, 0x62u};
    static const uint8_t outb_const[] = {0xE6u, 0x63u};
    static const uint8_t outw_const[] = {0x66u, 0xE7u, 0x64u};
    static const uint8_t outl_const[] = {0xE7u, 0x65u};
    static const uint8_t inb_register[] = {0xECu};
    static const uint8_t outb_register[] = {0xEEu};
    static const uint8_t byte_move[] = {0x88u, 0xD8u};
    static const uint8_t word_move[] = {0x66u, 0x89u, 0xD8u};
    static const uint8_t dword_move[] = {0x89u, 0xD8u};
    verify(path, architecture, "asm_port_inb_const", inb_const,
           sizeof(inb_const));
    verify(path, architecture, "asm_port_inw_const", inw_const,
           sizeof(inw_const));
    verify(path, architecture, "asm_port_inl_const", inl_const,
           sizeof(inl_const));
    verify(path, architecture, "asm_port_outb_const", outb_const,
           sizeof(outb_const));
    verify(path, architecture, "asm_port_outw_const", outw_const,
           sizeof(outw_const));
    verify(path, architecture, "asm_port_outl_const", outl_const,
           sizeof(outl_const));
    verify(path, architecture, "asm_port_inb_register", inb_register,
           sizeof(inb_register));
    verify(path, architecture, "asm_port_outb_register", outb_register,
           sizeof(outb_register));
    verify(path, architecture, "asm_modifier_byte_move", byte_move,
           sizeof(byte_move));
    verify(path, architecture, "asm_modifier_word_move", word_move,
           sizeof(word_move));
    verify(path, architecture, "asm_modifier_dword_move", dword_move,
           sizeof(dword_move));
}

static void verify_cpp(const char* path)
{
    static const uint8_t inb_const[] = {0xE4u, 0x66u};
    static const uint8_t outb_register[] = {0xEEu};
    static const uint8_t word_move[] = {0x66u, 0x89u, 0xD8u};
    verify(path, ARCH_X64, "asm_cpp_port_inb_const", inb_const,
           sizeof(inb_const));
    verify(path, ARCH_X64, "asm_cpp_port_outb_register", outb_register,
           sizeof(outb_register));
    verify(path, ARCH_X64, "asm_cpp_modifier_word_move", word_move,
           sizeof(word_move));
}

int main(int argc, char** argv)
{
    assert(argc == 4);
    verify_architecture(argv[1], ARCH_X86);
    verify_architecture(argv[2], ARCH_X64);
    verify_cpp(argv[3]);
    return 0;
}
