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

static bool contains_vector_encoding(const uint8_t* data, uint64_t size,
                                     bool avx, bool load)
{
    uint8_t opcode = load ? 0x6Fu : 0x7Fu;
    for (uint64_t offset = 0u; offset < size; ++offset) {
        if (avx) {
            if (size - offset >= 4u && data[offset] == 0xC4u &&
                data[offset + 2u] == 0x7Eu &&
                data[offset + 3u] == opcode) return true;
        } else if (data[offset] == 0xF3u) {
            if (size - offset >= 3u && data[offset + 1u] == 0x0Fu &&
                data[offset + 2u] == opcode) return true;
            if (size - offset >= 4u &&
                (data[offset + 1u] & 0xF0u) == 0x40u &&
                data[offset + 2u] == 0x0Fu &&
                data[offset + 3u] == opcode) return true;
        }
    }
    return false;
}

static bool contains_compare_immediate_memory(const uint8_t* data,
                                              uint64_t size)
{
    for (uint64_t offset = 0u; offset < size; ++offset) {
        uint64_t opcode_offset = offset;
        uint8_t modrm;
        uint64_t displacement_offset;
        uint64_t immediate_offset;
        if ((data[opcode_offset] & 0xF0u) == 0x40u) ++opcode_offset;
        if (size - opcode_offset < 7u || data[opcode_offset] != 0x83u) {
            continue;
        }
        modrm = data[opcode_offset + 1u];
        if (((modrm >> 3) & 7u) != 7u || (modrm >> 6) != 2u) continue;
        displacement_offset = opcode_offset + 2u;
        if ((modrm & 7u) == 4u) ++displacement_offset;
        immediate_offset = displacement_offset + 4u;
        if (immediate_offset < size &&
            data[displacement_offset] == 0x04u &&
            data[displacement_offset + 1u] == 0x03u &&
            data[displacement_offset + 2u] == 0x00u &&
            data[displacement_offset + 3u] == 0x00u &&
            data[immediate_offset] == 0x02u) return true;
    }
    return false;
}

static void verify_vector_moves(const char* path)
{
    static const uint8_t vzeroupper[] = {0xC5u, 0xF8u, 0x77u};
    static const uint8_t xgetbv[] = {0x0Fu, 0x01u, 0xD0u};
    ObjectFile* object = objfile_read(path);
    ObjSection* code;
    ObjSymbol* ymm_function;
    ObjSymbol* xmm_function;
    ObjSymbol* xgetbv_function;
    ObjSymbol* compare_function;
    uint64_t ymm_end;
    uint64_t xmm_end;
    uint64_t xgetbv_end;
    uint64_t compare_end;
    assert(object != NULL && object->arch == ARCH_X64);
    code = code_section(object);
    ymm_function = objfile_find_symbol(object, "asm_vector_ymm_move");
    xmm_function = objfile_find_symbol(object, "asm_vector_xmm_move");
    xgetbv_function = objfile_find_symbol(object, "asm_xgetbv_instruction");
    compare_function = objfile_find_symbol(object,
                                           "asm_cmp_memory_immediate");
    assert(code != NULL && ymm_function != NULL && xmm_function != NULL &&
           xgetbv_function != NULL && compare_function != NULL);
    ymm_end = function_end(object, ymm_function);
    xmm_end = function_end(object, xmm_function);
    xgetbv_end = function_end(object, xgetbv_function);
    compare_end = function_end(object, compare_function);
    assert(ymm_end > ymm_function->value && ymm_end <= code->size &&
           xmm_end > xmm_function->value && xmm_end <= code->size &&
           xgetbv_end > xgetbv_function->value && xgetbv_end <= code->size &&
           compare_end > compare_function->value && compare_end <= code->size);
    assert(contains_vector_encoding(code->data + ymm_function->value,
                                    ymm_end - ymm_function->value, true, true));
    assert(contains_vector_encoding(code->data + ymm_function->value,
                                    ymm_end - ymm_function->value, true, false));
    assert(contains_bytes(code->data + ymm_function->value,
                          ymm_end - ymm_function->value,
                          vzeroupper, sizeof(vzeroupper)));
    assert(contains_vector_encoding(code->data + xmm_function->value,
                                    xmm_end - xmm_function->value, false, true));
    assert(contains_vector_encoding(code->data + xmm_function->value,
                                    xmm_end - xmm_function->value, false, false));
    assert(contains_bytes(code->data + xgetbv_function->value,
                          xgetbv_end - xgetbv_function->value,
                          xgetbv, sizeof(xgetbv)));
    assert(contains_compare_immediate_memory(
        code->data + compare_function->value,
        compare_end - compare_function->value));
    objfile_free(object);
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
    verify_vector_moves(argv[2]);
    verify_cpp(argv[3]);
    return 0;
}
