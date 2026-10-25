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

static void verify_pattern_pair(const char* path, uint16_t architecture,
                                const char* function_name,
                                const uint8_t* first_a, size_t first_a_size,
                                const uint8_t* later_a, size_t later_a_size,
                                const uint8_t* first_b, size_t first_b_size,
                                const uint8_t* later_b, size_t later_b_size)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* code;
    ObjSymbol* function;
    uint64_t end;
    const uint8_t* body;
    uint64_t body_size;
    bool pair_a;
    bool pair_b;

    assert(object != NULL && object->arch == architecture);
    code = code_section(object);
    function = objfile_find_symbol(object, function_name);
    assert(code != NULL && function != NULL && function->section >= 0);
    end = function_end(object, function);
    assert(end > function->value && end <= code->size);
    body = code->data + function->value;
    body_size = end - function->value;
    pair_a = contains_bytes(body, body_size, first_a, first_a_size) &&
             contains_bytes(body, body_size, later_a, later_a_size);
    pair_b = contains_bytes(body, body_size, first_b, first_b_size) &&
             contains_bytes(body, body_size, later_b, later_b_size);
    if (!pair_a && !pair_b) {
        fprintf(stderr, "no valid early-clobber register pair in %s\n",
                function_name);
    }
    assert(pair_a || pair_b);
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

static void verify_earlyclobber_generic(const char* path,
                                        uint16_t architecture,
                                        const char* function_name)
{
    static const uint8_t x86_first_a[] = {0x89u, 0xc2u};
    static const uint8_t x86_later_a[] = {0x89u, 0xcau};
    static const uint8_t x86_first_b[] = {0x89u, 0xc1u};
    static const uint8_t x86_later_b[] = {0x89u, 0xd1u};
    static const uint8_t x64_first_a[] = {0x48u, 0x89u, 0xc1u};
    static const uint8_t x64_later_a[] = {0x4cu, 0x89u, 0xd1u};
    static const uint8_t x64_first_b[] = {0x49u, 0x89u, 0xc2u};
    static const uint8_t x64_later_b[] = {0x49u, 0x89u, 0xcau};

    if (architecture == ARCH_X64) {
        verify_pattern_pair(path, architecture, function_name,
                            x64_first_a, sizeof(x64_first_a),
                            x64_later_a, sizeof(x64_later_a),
                            x64_first_b, sizeof(x64_first_b),
                            x64_later_b, sizeof(x64_later_b));
    } else {
        verify_pattern_pair(path, architecture, function_name,
                            x86_first_a, sizeof(x86_first_a),
                            x86_later_a, sizeof(x86_later_a),
                            x86_first_b, sizeof(x86_first_b),
                            x86_later_b, sizeof(x86_later_b));
    }
}

static void verify_earlyclobber_generic_read_write(
    const char* path, uint16_t architecture, const char* function_name)
{
    static const uint8_t x86_first_a[] = {
        0xbau, 0x00u, 0x00u, 0x00u, 0x00u
    };
    static const uint8_t x86_later_a[] = {0x89u, 0xcau};
    static const uint8_t x86_first_b[] = {
        0xb9u, 0x00u, 0x00u, 0x00u, 0x00u
    };
    static const uint8_t x86_later_b[] = {0x89u, 0xd1u};
    static const uint8_t x64_first_a[] = {
        0x48u, 0xb9u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
    };
    static const uint8_t x64_later_a[] = {0x4cu, 0x89u, 0xd1u};
    static const uint8_t x64_first_b[] = {
        0x49u, 0xbau, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u
    };
    static const uint8_t x64_later_b[] = {0x49u, 0x89u, 0xcau};

    if (architecture == ARCH_X64) {
        verify_pattern_pair(path, architecture, function_name,
                            x64_first_a, sizeof(x64_first_a),
                            x64_later_a, sizeof(x64_later_a),
                            x64_first_b, sizeof(x64_first_b),
                            x64_later_b, sizeof(x64_later_b));
    } else {
        verify_pattern_pair(path, architecture, function_name,
                            x86_first_a, sizeof(x86_first_a),
                            x86_later_a, sizeof(x86_later_a),
                            x86_first_b, sizeof(x86_first_b),
                            x86_later_b, sizeof(x86_later_b));
    }
}

int main(int argc, char** argv)
{
    assert(argc == 5);
    static const uint8_t interrupt[] = {0xcdu, 0x80u};
    static const uint8_t mov_imm8[] = {0xb0u, 0x7fu};
    static const uint8_t mov_imm16[] = {0x66u, 0xb8u, 0x34u, 0x12u};
    static const uint8_t mov_imm32[] = {
        0xb8u, 0x78u, 0x56u, 0x34u, 0x12u
    };
    static const uint8_t i686_memory_input[] = {0x8bu, 0x45u, 0x08u};
    static const uint8_t i686_base_memory_load[] = {0x8bu, 0x08u};
    static const uint8_t i686_base_memory_store[] = {0x89u, 0x03u};
    static const uint8_t i686_base_memory_lea[] = {0x8du, 0x41u, 0x04u};
    static const uint8_t i686_esp_memory[] = {0x8bu, 0x04u, 0x24u};
    static const uint8_t i686_ebp_memory[] = {0x8bu, 0x45u, 0x00u};
    static const uint8_t i686_xor_test[] = {
        0x31u, 0xc8u, 0x85u, 0xc0u
    };
    static const uint8_t i686_indirect_jump[] = {0xffu, 0xe0u};
    static const uint8_t i686_push_pop[] = {0x50u, 0x58u};
    static const uint8_t i686_rep_movsl[] = {0xf3u, 0xa5u};
    static const uint8_t i686_rep_stosl[] = {0xf3u, 0xabu};
    static const uint8_t mov_imm64[] = {
        0x48u, 0xb8u, 0x88u, 0x77u, 0x66u, 0x55u,
        0x44u, 0x33u, 0x22u, 0x11u
    };
    verify(argv[1], ARCH_X86, "asm_placeholder_move");
    verify(argv[2], ARCH_X64, "asm_placeholder_move");
    verify(argv[3], ARCH_X64, "asm_cpp_placeholder_move");
    verify_pattern(argv[1], ARCH_X86, "asm_memory_input_move",
                   i686_memory_input, sizeof(i686_memory_input));
    verify_pattern(argv[1], ARCH_X86, "asm_base_memory_move",
                   i686_base_memory_load, sizeof(i686_base_memory_load));
    verify_pattern(argv[1], ARCH_X86, "asm_base_memory_store",
                   i686_base_memory_store, sizeof(i686_base_memory_store));
    verify_pattern(argv[1], ARCH_X86, "asm_base_memory_lea",
                   i686_base_memory_lea, sizeof(i686_base_memory_lea));
    verify_pattern(argv[1], ARCH_X86, "asm_esp_memory_move",
                   i686_esp_memory, sizeof(i686_esp_memory));
    verify_pattern(argv[1], ARCH_X86, "asm_ebp_memory_move",
                   i686_ebp_memory, sizeof(i686_ebp_memory));
    verify_pattern(argv[1], ARCH_X86, "asm_x86_xor_test",
                   i686_xor_test, sizeof(i686_xor_test));
    verify_pattern(argv[1], ARCH_X86, "asm_x86_indirect_jump",
                   i686_indirect_jump, sizeof(i686_indirect_jump));
    verify_pattern(argv[1], ARCH_X86, "asm_x86_push_pop",
                   i686_push_pop, sizeof(i686_push_pop));
    verify_pattern(argv[1], ARCH_X86, "asm_i686_rep_movsl",
                   i686_rep_movsl, sizeof(i686_rep_movsl));
    verify_pattern(argv[1], ARCH_X86, "asm_i686_rep_stosl",
                   i686_rep_stosl, sizeof(i686_rep_stosl));
    verify_pattern(argv[4], ARCH_X86, "asm_cpp_memory_input_move",
                   i686_memory_input, sizeof(i686_memory_input));
    verify_generic(argv[1], ARCH_X86, "asm_generic_placeholder_move");
    verify_generic(argv[2], ARCH_X64, "asm_generic_placeholder_move");
    verify_generic(argv[1], ARCH_X86, "asm_general_input_move");
    verify_generic(argv[2], ARCH_X64, "asm_general_input_move");
    verify_generic_output(argv[1], ARCH_X86, "asm_generic_output_move");
    verify_generic_output(argv[2], ARCH_X64, "asm_generic_output_move");
    verify_generic_output(argv[3], ARCH_X64, "asm_cpp_generic_output_move");
    verify_earlyclobber_generic(argv[1], ARCH_X86,
                                "asm_earlyclobber_generic");
    verify_earlyclobber_generic(argv[2], ARCH_X64,
                                "asm_earlyclobber_generic");
    verify_earlyclobber_generic(argv[3], ARCH_X64,
                                "asm_cpp_earlyclobber_generic");
    verify_earlyclobber_generic_read_write(
        argv[1], ARCH_X86, "asm_earlyclobber_generic_read_write");
    verify_earlyclobber_generic_read_write(
        argv[2], ARCH_X64, "asm_earlyclobber_generic_read_write");
    verify_earlyclobber_generic_read_write(
        argv[3], ARCH_X64, "asm_cpp_earlyclobber_generic_read_write");
    verify_pattern(argv[1], ARCH_X86, "asm_mov_imm8",
                   mov_imm8, sizeof(mov_imm8));
    verify_pattern(argv[2], ARCH_X64, "asm_mov_imm8",
                   mov_imm8, sizeof(mov_imm8));
    verify_pattern(argv[3], ARCH_X64, "asm_cpp_mov_imm8",
                   mov_imm8, sizeof(mov_imm8));
    verify_pattern(argv[1], ARCH_X86, "asm_mov_imm16",
                   mov_imm16, sizeof(mov_imm16));
    verify_pattern(argv[2], ARCH_X64, "asm_mov_imm16",
                   mov_imm16, sizeof(mov_imm16));
    verify_pattern(argv[3], ARCH_X64, "asm_cpp_mov_imm16",
                   mov_imm16, sizeof(mov_imm16));
    verify_pattern(argv[1], ARCH_X86, "asm_mov_imm32",
                   mov_imm32, sizeof(mov_imm32));
    verify_pattern(argv[2], ARCH_X64, "asm_mov_imm32",
                   mov_imm32, sizeof(mov_imm32));
    verify_pattern(argv[3], ARCH_X64, "asm_cpp_mov_imm32",
                   mov_imm32, sizeof(mov_imm32));
    verify_pattern(argv[2], ARCH_X64, "asm_mov_imm64",
                   mov_imm64, sizeof(mov_imm64));
    verify_pattern(argv[3], ARCH_X64, "asm_cpp_mov_imm64",
                   mov_imm64, sizeof(mov_imm64));
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
    verify_pattern(argv[1], ARCH_X86, "asm_c_modifier_interrupt",
                   interrupt, sizeof(interrupt));
    verify_pattern(argv[2], ARCH_X64, "asm_c_modifier_interrupt",
                   interrupt, sizeof(interrupt));
    verify_pattern(argv[3], ARCH_X64, "asm_cpp_c_modifier_interrupt",
                   interrupt, sizeof(interrupt));
    {
        static const uint8_t negated_interrupt[] = {0xcdu, 0xfbu};
        verify_pattern(argv[1], ARCH_X86, "asm_n_modifier_interrupt",
                       negated_interrupt, sizeof(negated_interrupt));
        verify_pattern(argv[2], ARCH_X64, "asm_n_modifier_interrupt",
                       negated_interrupt, sizeof(negated_interrupt));
        verify_pattern(argv[3], ARCH_X64, "asm_cpp_n_modifier_interrupt",
                       negated_interrupt, sizeof(negated_interrupt));
    }
    {
        static const uint8_t q_modifier[] = {0x48u, 0x89u, 0xd8u};
        verify_pattern(argv[2], ARCH_X64, "asm_x64_q_modifier",
                       q_modifier, sizeof(q_modifier));
        verify_pattern(argv[3], ARCH_X64, "asm_cpp_x64_q_modifier",
                       q_modifier, sizeof(q_modifier));
    }
    return 0;
}
