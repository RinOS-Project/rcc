#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static bool contains_bytes(const uint8_t* data, uint64_t size,
                           const char* text)
{
    size_t length = strlen(text);
    if (length == 0u || size < length) return false;
    for (uint64_t offset = 0u; offset <= size - length; ++offset) {
        if (memcmp(data + offset, text, length) == 0) return true;
    }
    return false;
}

static bool contains_byte_pair(const uint8_t* data, uint64_t size,
                               uint8_t first, uint8_t second)
{
    if (size < 2u) return false;
    for (uint64_t offset = 0u; offset + 1u < size; ++offset) {
        if (data[offset] == first && data[offset + 1u] == second) return true;
    }
    return false;
}

static bool contains_byte(const uint8_t* data, uint64_t size, uint8_t value)
{
    for (uint64_t offset = 0u; offset < size; ++offset) {
        if (data[offset] == value) return true;
    }
    return false;
}

static unsigned count_sections_named(const ObjectFile* object,
                                     const char* name)
{
    unsigned count = 0u;
    if (!object || !name) return 0u;
    for (const ObjSection* section = object->sections; section;
         section = section->next) {
        if (section->name && strcmp(section->name, name) == 0) ++count;
    }
    return count;
}

static uint32_t read_u32(const uint8_t* data, uint64_t offset)
{
    return (uint32_t)data[offset] |
           ((uint32_t)data[offset + 1u] << 8) |
           ((uint32_t)data[offset + 2u] << 16) |
           ((uint32_t)data[offset + 3u] << 24);
}

static bool find_lexical_block_local(const ObjSection* info,
                                     const ObjSection* strings,
                                     const char* variable_name,
                                     uint64_t address_size)
{
    if (!info || !strings || !variable_name) return false;
    for (uint64_t offset = 11u; offset + 15u < info->size; ++offset) {
        uint32_t name_offset;
        uint64_t range_offset = offset + 1u + address_size;
        uint64_t file_offset = range_offset + 4u;
        uint64_t line_offset = file_offset + 1u;
        uint64_t column_offset = line_offset + 4u;
        uint64_t child_offset = column_offset + 4u;
        if (info->data[offset] != 24u ||
            child_offset >= info->size ||
            read_u32(info->data, range_offset) == 0u ||
            info->data[file_offset] == 0u ||
            read_u32(info->data, line_offset) == 0u ||
            read_u32(info->data, column_offset) == 0u ||
            info->data[child_offset] != 4u) {
            continue;
        }
        name_offset = read_u32(info->data, child_offset + 1u);
        if (name_offset < strings->size &&
            strcmp((const char*)strings->data + name_offset,
                   variable_name) == 0) {
            return true;
        }
    }
    return false;
}

static uint64_t read_uleb(const uint8_t* data, uint64_t size,
                          uint64_t* offset)
{
    uint64_t value = 0u;
    unsigned shift = 0u;
    assert(data != NULL && offset != NULL);
    while (*offset < size && shift < 64u) {
        uint8_t byte = data[(*offset)++];
        value |= (uint64_t)(byte & 0x7fu) << shift;
        if ((byte & 0x80u) == 0u) return value;
        shift += 7u;
    }
    assert(0 && "invalid ULEB128");
    return 0u;
}

static unsigned count_line_copy_ops(const ObjSection* line)
{
    uint64_t offset;
    unsigned copies = 0u;
    assert(line != NULL && line->size >= 10u);
    offset = 10u + read_u32(line->data, 6u);
    assert(offset <= line->size);
    while (offset < line->size) {
        uint8_t opcode = line->data[offset++];
        if (opcode == 0u) {
            uint64_t length = read_uleb(line->data, line->size, &offset);
            assert(length > 0u && length <= line->size - offset);
            ++offset;
            offset += length - 1u;
            continue;
        }
        if (opcode == 1u) {
            ++copies;
            continue;
        }
        switch (opcode) {
            case 2u:
            case 4u:
            case 5u:
            case 12u:
                (void)read_uleb(line->data, line->size, &offset);
                break;
            case 3u:
                (void)read_uleb(line->data, line->size, &offset);
                break;
            case 9u:
                assert(offset + 2u <= line->size);
                offset += 2u;
                break;
            case 6u:
            case 7u:
            case 8u:
            case 10u:
            case 11u:
                break;
            default:
                assert(0 && "unexpected DWARF line opcode");
        }
    }
    return copies;
}

static void verify_recursive_aggregate_type(const ObjSection* info,
                                             const ObjSection* strings)
{
    uint64_t aggregate_offset = UINT64_MAX;
    uint64_t next_type_offset = UINT64_MAX;
    assert(info != NULL && strings != NULL);
    for (uint64_t offset = 11u; offset + 9u < info->size; ++offset) {
        uint32_t name_offset;
        if (info->data[offset] != 12u) continue;
        name_offset = read_u32(info->data, offset + 1u);
        if (name_offset < strings->size &&
            strcmp((const char*)strings->data + name_offset,
                   "debug_recursive") == 0) {
            aggregate_offset = offset;
            break;
        }
    }
    assert(aggregate_offset != UINT64_MAX);
    {
        uint64_t child = aggregate_offset + 9u;
        bool found_next = false;
        while (child < info->size && info->data[child] != 0u) {
            uint8_t tag = info->data[child];
            uint32_t name_offset;
            uint64_t expression_offset;
            uint64_t expression_size;
            assert(tag == 14u || tag == 17u);
            assert(child + 9u <= info->size);
            name_offset = read_u32(info->data, child + 1u);
            if (name_offset < strings->size &&
                strcmp((const char*)strings->data + name_offset,
                       "next") == 0) {
                next_type_offset = read_u32(info->data, child + 5u);
                found_next = true;
            }
            expression_offset = child + 9u;
            expression_size = read_uleb(
                info->data, info->size, &expression_offset);
            assert(expression_offset + expression_size <= info->size);
            child = expression_offset + expression_size +
                (tag == 17u ? 8u : 0u);
        }
        assert(child < info->size && info->data[child] == 0u);
        assert(found_next && next_type_offset < info->size);
    }
    assert(info->data[next_type_offset] == 6u);
    assert(next_type_offset + 6u <= info->size);
    assert(read_u32(info->data, next_type_offset + 2u) == aggregate_offset);
}

static void verify_subroutine_type(const ObjSection* info)
{
    if (!info) return;
    for (uint64_t offset = 11u; offset + 10u < info->size; ++offset) {
        uint32_t return_type;
        uint64_t child;
        if (info->data[offset] != 18u) continue;
        return_type = read_u32(info->data, offset + 1u);
        assert(return_type < offset);
        child = offset + 5u;
        if (info->data[child] != 19u) continue;
        while (child + 5u < info->size && info->data[child] == 19u) {
            assert(read_u32(info->data, child + 1u) < offset);
            child += 5u;
        }
        assert(child < info->size && info->data[child] == 0u);
        return;
    }
    assert(0 && "missing parameterized subroutine type DIE");
}

static uint64_t find_global_variable_die(const ObjSection* info,
                                         const ObjSection* strings,
                                         const char* variable_name)
{
    if (!info || !strings || !variable_name) return UINT64_MAX;
    for (uint64_t offset = 11u; offset + 22u < info->size; ++offset) {
        uint32_t name_offset;
        if (info->data[offset] != 9u) continue;
        name_offset = read_u32(info->data, offset + 1u);
        if (name_offset < strings->size &&
            strcmp((const char*)strings->data + name_offset,
                   variable_name) == 0) {
            return offset;
        }
    }
    return UINT64_MAX;
}

static unsigned count_global_variable_dies(const ObjSection* info,
                                           const ObjSection* strings,
                                           const char* variable_name)
{
    unsigned count = 0u;
    if (!info || !strings || !variable_name) return 0u;
    for (uint64_t offset = 11u; offset + 5u < info->size; ++offset) {
        uint32_t name_offset;
        if (info->data[offset] != 9u) continue;
        name_offset = read_u32(info->data, offset + 1u);
        if (name_offset < strings->size &&
            strcmp((const char*)strings->data + name_offset,
                   variable_name) == 0) {
            ++count;
        }
    }
    return count;
}

static bool has_relocation(const ObjSection* section, uint64_t offset,
                           const char* symbol_name, RelocType type)
{
    if (!section) return false;
    for (const ObjReloc* relocation = section->relocs; relocation;
         relocation = relocation->next) {
        if (relocation->offset == offset && relocation->type == type &&
            relocation->symbol_name &&
            (!symbol_name || strcmp(relocation->symbol_name, symbol_name) == 0)) {
            return true;
        }
    }
    return false;
}

static bool has_relocation_symbol(const ObjSection* section,
                                  const char* symbol_name, RelocType type)
{
    if (!section || !symbol_name) return false;
    for (const ObjReloc* relocation = section->relocs; relocation;
         relocation = relocation->next) {
        if (relocation->type == type && relocation->symbol_name &&
            strcmp(relocation->symbol_name, symbol_name) == 0) {
            return true;
        }
    }
    return false;
}

static bool has_positive_relocation_addend(const ObjSection* section)
{
    if (!section) return false;
    for (const ObjReloc* relocation = section->relocs; relocation;
         relocation = relocation->next) {
        if (relocation->addend > 0) return true;
    }
    return false;
}

static void verify_global_variable(const ObjSection* info,
                                   const ObjSection* strings,
                                   const char* variable_name,
                                   const char* relocation_symbol,
                                   const char* linkage_name,
                                   uint64_t address_size,
                                   bool external)
{
    uint64_t die_offset = find_global_variable_die(
        info, strings, variable_name);
    uint64_t offset;
    uint64_t expression_size;
    uint64_t address_offset;
    uint32_t linkage_offset;

    assert(die_offset != UINT64_MAX);
    assert(count_global_variable_dies(info, strings, variable_name) == 1u);
    offset = die_offset + 1u;
    assert(read_u32(info->data, offset + 4u) < info->size);
    assert(info->data[offset + 4u] != 0u);
    offset += 4u + 4u + 4u + 4u + 4u;
    assert(info->data[offset] == (external ? 1u : 0u));
    ++offset;
    linkage_offset = read_u32(info->data, offset);
    assert(linkage_offset < strings->size &&
           strings->data[linkage_offset] != 0u);
    if (linkage_name) {
        assert(strcmp((const char*)strings->data + linkage_offset,
                      linkage_name) == 0);
    }
    offset += 4u;
    expression_size = read_uleb(info->data, info->size, &offset);
    assert(expression_size == address_size + 1u);
    assert(offset + expression_size <= info->size);
    assert(info->data[offset++] == 0x03u); /* DW_OP_addr */
    address_offset = offset;
    for (uint64_t byte = 0u; byte < address_size; ++byte) {
        assert(info->data[offset++] == 0u);
    }
    assert(has_relocation(info, address_offset, relocation_symbol,
                          address_size == 8u ? RELOC_ABS64 : RELOC_ABS32U));
}

static void verify_first_frame_fde(const ObjSection* frame,
                                   uint16_t architecture)
{
    uint32_t cie_length;
    uint64_t fde_offset;
    uint32_t fde_length;
    uint64_t instruction_offset;
    uint64_t instruction_end;
    uint32_t pointer_size = architecture == ARCH_X64 ? 8u : 4u;
    uint8_t frame_register = architecture == ARCH_X64 ? 6u : 5u;

    assert(frame != NULL && frame->size >= 8u);
    cie_length = read_u32(frame->data, 0u);
    assert(cie_length > 0u);
    fde_offset = 4u + cie_length;
    assert(fde_offset + 8u <= frame->size);
    fde_length = read_u32(frame->data, fde_offset);
    assert(fde_length >= 4u + pointer_size * 2u + 8u);
    assert(fde_offset + 4u + fde_length <= frame->size);
    assert(read_u32(frame->data, fde_offset + 4u) == 0u);
    instruction_offset = fde_offset + 4u + 4u + pointer_size * 2u;
    instruction_end = fde_offset + 4u + fde_length;
    assert(instruction_offset + 8u <= instruction_end);
    /* push fp; the saved FP is two words below the new CFA. */
    assert(frame->data[instruction_offset++] == 0x41u);
    assert(frame->data[instruction_offset++] == 0x0eu);
    assert(frame->data[instruction_offset++] == (uint8_t)(pointer_size * 2u));
    assert(frame->data[instruction_offset++] ==
           (uint8_t)(0x80u + frame_register));
    assert(frame->data[instruction_offset++] == 1u);
    /* The advance must land after the complete mov fp,sp instruction. */
    assert(frame->data[instruction_offset++] ==
           (uint8_t)(architecture == ARCH_X64 ? 0x43u : 0x42u));
    assert(frame->data[instruction_offset++] == 0x0du);
    assert(frame->data[instruction_offset++] == frame_register);
    /* The epilogue changes the CFA back to SP and restores the saved FP. */
    assert(contains_byte_pair(frame->data + instruction_offset,
                              instruction_end - instruction_offset,
                              0x0du,
                              architecture == ARCH_X64 ? 7u : 4u));
    assert(contains_byte(frame->data + instruction_offset,
                         instruction_end - instruction_offset,
                         (uint8_t)(0xc0u + frame_register)));
}

static void verify_i686_aligned_frame_fde(const ObjSection* frame)
{
    uint32_t cie_length;
    uint64_t fde_offset;
    uint32_t fde_length;
    uint64_t instruction_offset;
    uint64_t instruction_end;

    assert(frame != NULL && frame->size >= 8u);
    cie_length = read_u32(frame->data, 0u);
    assert(cie_length > 0u);
    fde_offset = 4u + cie_length;
    assert(fde_offset + 8u <= frame->size);
    fde_length = read_u32(frame->data, fde_offset);
    assert(fde_length >= 4u + 8u + 16u);
    assert(fde_offset + 4u + fde_length <= frame->size);
    assert(read_u32(frame->data, fde_offset + 4u) == 0u);
    instruction_offset = fde_offset + 4u + 4u + 8u;
    instruction_end = fde_offset + 4u + fde_length;
    assert(instruction_offset + 16u <= instruction_end);
    /* push ebp; the saved EBP is two words below the CFA. */
    assert(frame->data[instruction_offset++] == 0x41u);
    assert(frame->data[instruction_offset++] == 0x0eu);
    assert(frame->data[instruction_offset++] == 0x08u);
    assert(frame->data[instruction_offset++] == 0x85u);
    assert(frame->data[instruction_offset++] == 1u);
    /* The aligned copy sequence completes at offset 21. */
    assert(frame->data[instruction_offset++] == 0x54u);
    assert(frame->data[instruction_offset++] == 0x0cu);
    assert(frame->data[instruction_offset++] == 0x04u);
    assert(frame->data[instruction_offset++] == 0x08u);
    /* mov ebp,esp: use the stable aligned frame register thereafter. */
    assert(frame->data[instruction_offset++] == 0x42u);
    assert(frame->data[instruction_offset++] == 0x0du);
    assert(frame->data[instruction_offset++] == 0x05u);
    assert(contains_byte_pair(frame->data + instruction_offset,
                              instruction_end - instruction_offset,
                              0x0du, 0x04u));
    assert(contains_byte(frame->data + instruction_offset,
                         instruction_end - instruction_offset, 0xc5u));
}

static void verify_aligned_debug_object(const char* path,
                                        uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* line;
    ObjSection* info;
    ObjSection* frame;
    assert(object != NULL && object->arch == architecture);
    line = objfile_get_section(object, ".debug_line");
    info = objfile_get_section(object, ".debug_info");
    frame = objfile_get_section(object, ".debug_frame");
    assert(line != NULL && info != NULL && frame != NULL);
    assert(contains_bytes(line->data, line->size,
                          "tests/debug_info_aligned.c"));
    assert(frame->relocs != NULL && frame->size > 24u);
    if (architecture == ARCH_X86) {
        verify_i686_aligned_frame_fde(frame);
    } else {
        verify_first_frame_fde(frame, architecture);
    }
    objfile_free(object);
}

static uint64_t find_function_die(const ObjSection* info,
                                  const ObjSection* strings,
                                  const char* function_name,
                                  uint64_t address_size)
{
    if (!info || !strings || !function_name) return UINT64_MAX;
    for (uint64_t offset = 11u; offset + 5u < info->size; ++offset) {
        uint32_t name_offset;
        if (info->data[offset] != 2u) continue;
        name_offset = read_u32(info->data, offset + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset,
                   function_name) != 0) {
            continue;
        }
        if (offset + 1u + 4u + address_size + 4u + 1u + 4u > info->size) {
            return UINT64_MAX;
        }
        return offset;
    }
    return UINT64_MAX;
}

static uint8_t read_inline_attribute(const ObjSection* info,
                                     uint64_t function_offset,
                                     uint64_t address_size,
                                     bool has_return_type)
{
    uint64_t offset = function_offset + 1u + 4u + address_size + 4u + 1u +
                      4u + 4u + 1u + 4u + (has_return_type ? 4u : 0u);
    assert(info != NULL);
    assert(offset + 4u < info->size);
    /* DW_FORM_exprloc for the current frame-base expression is two bytes:
     * length 1 followed by DW_OP_breg{5,6} and a zero SLEB displacement. */
    assert(info->data[offset] == 2u);
    offset += 3u;
    return info->data[offset];
}

static uint32_t read_return_type_ref(const ObjSection* info,
                                     uint64_t function_offset,
                                     uint64_t address_size)
{
    uint64_t offset = function_offset + 1u + 4u + address_size + 4u + 1u +
                      4u + 4u + 1u + 4u;
    assert(info != NULL);
    assert(offset + 4u <= info->size);
    return read_u32(info->data, offset);
}

static void verify_debug_object(const char* path, uint16_t architecture,
                                uint16_t language, const char* source_file,
                                const char* function_name,
                                const char* inline_function_name)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* line;
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    ObjSection* frame;
    assert(object != NULL);
    assert(object->arch == architecture);
    line = objfile_get_section(object, ".debug_line");
    assert(line != NULL);
    assert(line->type == SECT_DEBUG_LINE);
    assert(line->flags == 0u && line->size == line->memory_size);
    assert(line->size > 16u);
    assert(line->data[4] == 4u && line->data[5] == 0u);
    assert(line->relocs != NULL);
    assert(has_positive_relocation_addend(line));
    assert(contains_bytes(line->data, line->size, source_file));
    assert(contains_byte_pair(line->data, line->size, 5u, 1u));
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    frame = objfile_get_section(object, ".debug_frame");
    assert(info != NULL && info->type == SECT_DEBUG_INFO);
    assert(abbrev != NULL && abbrev->type == SECT_DEBUG_ABBREV);
    assert(strings != NULL && strings->type == SECT_DEBUG_STR);
    assert(frame != NULL && frame->type == SECT_DEBUG_FRAME);
    assert(frame->flags == 0u && frame->size == frame->memory_size);
    assert(frame->size > 24u && frame->relocs != NULL);
    assert(frame->data[4] == 0xffu && frame->data[5] == 0xffu &&
           frame->data[6] == 0xffu && frame->data[7] == 0xffu);
    assert(contains_byte(frame->data, frame->size, 0x0cu));
    assert(contains_byte(frame->data, frame->size, 0x0du));
    verify_first_frame_fde(frame, architecture);
    assert(info->relocs != NULL && info->size > 16u);
    assert(info->data[4] == 4u && info->data[5] == 0u);
    assert(info->data[16] == (uint8_t)language);
    assert(info->data[17] == (uint8_t)(language >> 8));
    {
        uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
        uint64_t function_offset = find_function_die(
            info, strings, function_name, address_size);
        uint64_t column_offset = function_offset + 1u + 4u + address_size +
                                  4u + 1u + 4u;
        assert(function_offset != UINT64_MAX);
        assert(info->data[function_offset] == 2u);
        assert(info->size >= column_offset + 4u);
        assert(read_u32(info->data, column_offset) == 1u);
        assert(read_return_type_ref(info, function_offset, address_size) <
               info->size);
        assert(info->data[read_return_type_ref(info, function_offset,
                                               address_size)] == 5u);
        assert(read_inline_attribute(info, function_offset, address_size,
                                     true) == 0u);
        if (inline_function_name) {
            uint64_t inline_offset = find_function_die(
                info, strings, inline_function_name, address_size);
            assert(inline_offset != UINT64_MAX);
            assert(read_return_type_ref(info, inline_offset, address_size) <
                   info->size);
            assert(info->data[read_return_type_ref(info, inline_offset,
                                                   address_size)] == 5u);
            assert(read_inline_attribute(info, inline_offset, address_size,
                                         true) == 3u);
        }
    }
    assert(abbrev->size > 8u && strings->size > 1u && strings->data[0] == 0u);
    assert(contains_bytes(strings->data, strings->size, function_name));
    if (language == 0x000cu) {
        verify_global_variable(info, strings, "debug_global_data",
                               "debug_global_data",
                               "debug_global_data",
                               architecture == ARCH_X64 ? 8u : 4u, true);
        verify_global_variable(info, strings, "debug_file_static", NULL,
                               NULL,
                               architecture == ARCH_X64 ? 8u : 4u, false);
        verify_global_variable(info, strings, "debug_line_static", NULL,
                               NULL,
                               architecture == ARCH_X64 ? 8u : 4u, false);
        verify_global_variable(info, strings, "debug_const_data",
                               "debug_const_data", "debug_const_data",
                               architecture == ARCH_X64 ? 8u : 4u, true);
        verify_global_variable(info, strings, "debug_volatile_data",
                               "debug_volatile_data", "debug_volatile_data",
                               architecture == ARCH_X64 ? 8u : 4u, true);
        verify_global_variable(info, strings, "debug_restrict_data",
                               "debug_restrict_data", "debug_restrict_data",
                               architecture == ARCH_X64 ? 8u : 4u, true);
        verify_global_variable(info, strings, "debug_atomic_data",
                               "debug_atomic_data", "debug_atomic_data",
                               architecture == ARCH_X64 ? 8u : 4u, true);
        assert(contains_bytes(strings->data, strings->size,
                              "debug_info_parameters"));
        assert(contains_bytes(strings->data, strings->size, "left"));
        assert(contains_bytes(strings->data, strings->size, "right"));
        assert(contains_bytes(strings->data, strings->size, "sum"));
        assert(contains_bytes(strings->data, strings->size, "pointer"));
        assert(contains_bytes(strings->data, strings->size,
                              "debug_aggregate"));
        assert(contains_bytes(strings->data, strings->size, "first"));
        assert(contains_bytes(strings->data, strings->size, "second"));
        assert(contains_bytes(strings->data, strings->size, "debug_enum"));
        verify_recursive_aggregate_type(info, strings);
        assert(contains_bytes(strings->data, strings->size,
                              "DEBUG_ENUM_NEGATIVE"));
        assert(contains_bytes(strings->data, strings->size,
                              "DEBUG_ENUM_POSITIVE"));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x05u, 0x00u));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x34u, 0x00u));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x13u, 0x01u));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x0du, 0x00u));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x01u, 0x01u));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x04u, 0x01u));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x28u, 0x00u));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x11u, 0x0du));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x49u, 0x13u));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x3au, 0x06u));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x3bu, 0x06u));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x39u, 0x06u));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x40u, 0x18u));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x24u, 0x00u));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x0fu, 0x00u));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x15u, 0x01u));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x05u, 0x00u));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x26u, 0x00u));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x35u, 0x00u));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x37u, 0x00u));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x47u, 0x00u));
        assert(contains_byte_pair(abbrev->data, abbrev->size, 0x0bu, 0x01u));
        assert(contains_bytes(strings->data, strings->size, "nested"));
        assert(find_lexical_block_local(
            info, strings, "nested", architecture == ARCH_X64 ? 8u : 4u));
        verify_subroutine_type(info);
        assert(contains_byte(info->data, info->size,
                             architecture == ARCH_X64 ? 0x76u : 0x75u));
        assert(contains_byte(info->data, info->size, 0x23u));
        {
            bool pointer_type_referenced = false;
            for (uint64_t offset = 0u; offset + 9u < info->size; ++offset) {
                uint32_t type_offset;
                if (info->data[offset] != 4u) continue;
                type_offset = read_u32(info->data, offset + 5u);
                if (type_offset < info->size &&
                    info->data[type_offset] == 6u) {
                    pointer_type_referenced = true;
                    break;
                }
            }
            assert(pointer_type_referenced);
        }
    } else if (language == 0x0021u) {
        verify_global_variable(info, strings, "debug_cpp_global", NULL,
                               NULL,
                               architecture == ARCH_X64 ? 8u : 4u, true);
        verify_global_variable(info, strings, "debug_cpp_static", NULL,
                               NULL,
                               architecture == ARCH_X64 ? 8u : 4u, false);
    }
    objfile_free(object);
}

static void verify_without_debug(const char* path)
{
    ObjectFile* object = objfile_read(path);
    assert(object != NULL);
    assert(objfile_get_section(object, ".debug_line") == NULL);
    assert(objfile_get_section(object, ".debug_info") == NULL);
    assert(objfile_get_section(object, ".debug_abbrev") == NULL);
    assert(objfile_get_section(object, ".debug_str") == NULL);
    assert(objfile_get_section(object, ".debug_frame") == NULL);
    objfile_free(object);
}

static void verify_verified_debug_object(const char* path,
                                         uint16_t architecture)
{
    const char* static_symbol =
        "tests/verified_backend_debug.c::verified_debug_static";
    ObjectFile* object = objfile_read(path);
    ObjSection* line;
    ObjSection* info;
    ObjSection* strings;
    ObjSection* frame;
    assert(object != NULL && object->arch == architecture);
    line = objfile_get_section(object, ".debug_line");
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    frame = objfile_get_section(object, ".debug_frame");
    assert(line != NULL && info != NULL && strings != NULL && frame != NULL);
    assert(contains_bytes(line->data, line->size,
                          "tests/verified_backend_debug.c"));
    assert(contains_bytes(strings->data, strings->size,
                          "verified_debug_static"));
    assert(contains_bytes(strings->data, strings->size,
                          "verified_debug_entry"));
    assert(line->relocs != NULL && info->relocs != NULL &&
           frame->relocs != NULL);
    assert(objfile_find_symbol(object, static_symbol) != NULL);
    assert(has_relocation_symbol(
        line, static_symbol,
        architecture == ARCH_X64 ? RELOC_ABS64 : RELOC_ABS32U));
    assert(has_relocation_symbol(
        info, static_symbol,
        architecture == ARCH_X64 ? RELOC_ABS64 : RELOC_ABS32U));
    assert(has_relocation_symbol(
        frame, static_symbol,
        architecture == ARCH_X64 ? RELOC_ABS64 : RELOC_ABS32U));
    assert(count_line_copy_ops(line) >= 8u);
    verify_first_frame_fde(frame, architecture);
    assert(find_function_die(info, strings, "verified_debug_static",
                             architecture == ARCH_X64 ? 8u : 4u) !=
           UINT64_MAX);
    assert(find_function_die(info, strings, "verified_debug_entry",
                             architecture == ARCH_X64 ? 8u : 4u) !=
           UINT64_MAX);
    assert(find_lexical_block_local(
        info, strings, "local", architecture == ARCH_X64 ? 8u : 4u));
    assert(find_lexical_block_local(
        info, strings, "nested", architecture == ARCH_X64 ? 8u : 4u));
    objfile_free(object);
}

static void verify_verified_global_debug_object(const char* path,
                                                uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* line;
    ObjSection* info;
    ObjSection* strings;
    ObjSection* frame;
    assert(object != NULL && object->arch == architecture);
    assert(count_sections_named(object, ".debug_line") == 1u);
    assert(count_sections_named(object, ".debug_info") == 1u);
    assert(count_sections_named(object, ".debug_abbrev") == 1u);
    assert(count_sections_named(object, ".debug_str") == 1u);
    assert(count_sections_named(object, ".debug_frame") == 1u);
    line = objfile_get_section(object, ".debug_line");
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    frame = objfile_get_section(object, ".debug_frame");
    assert(line != NULL && info != NULL && strings != NULL && frame != NULL);
    assert(contains_bytes(line->data, line->size,
                          "tests/verified_backend_globals.c"));
    assert(contains_bytes(strings->data, strings->size,
                          "verified_global_data"));
    assert(contains_bytes(strings->data, strings->size,
                          "verified_global_read"));
    assert(line->relocs != NULL && info->relocs != NULL &&
           frame->relocs != NULL);
    verify_first_frame_fde(frame, architecture);
    objfile_free(object);
}

int main(int argc, char** argv)
{
    assert(argc == 14);
    verify_debug_object(argv[1], ARCH_X86, 0x000cu,
                        "tests/debug_info.c", "debug_line_entry",
                        "debug_declared_inline");
    verify_debug_object(argv[2], ARCH_X64, 0x000cu,
                        "tests/debug_info.c", "debug_line_entry",
                        "debug_declared_inline");
    verify_debug_object(argv[3], ARCH_X64, 0x002bu,
                        "tests/hello.cpp", "main", NULL);
    verify_without_debug(argv[4]);
    verify_aligned_debug_object(argv[5], ARCH_X86);
    verify_aligned_debug_object(argv[6], ARCH_X64);
    verify_debug_object(argv[7], ARCH_X64, 0x001au,
                        "tests/hello.cpp", "main", NULL);
    verify_debug_object(argv[8], ARCH_X64, 0x0021u,
                        "tests/hello.cpp", "main", NULL);
    verify_debug_object(argv[9], ARCH_X64, 0x002au,
                        "tests/hello.cpp", "main", NULL);
    verify_verified_debug_object(argv[10], ARCH_X86);
    verify_verified_debug_object(argv[11], ARCH_X64);
    verify_verified_global_debug_object(argv[12], ARCH_X86);
    verify_verified_global_debug_object(argv[13], ARCH_X64);
    return 0;
}
