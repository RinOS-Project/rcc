#include "objfile.h"
#include "rin_formats_v3.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t read_uleb(const uint8_t* data, uint64_t size,
                          uint64_t* offset);
static int64_t read_sleb(const uint8_t* data, uint64_t size,
                         uint64_t* offset);

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

static bool contains_sequence(const uint8_t* data, uint64_t size,
                              const uint8_t* sequence, uint64_t length)
{
    if (!data || !sequence || length == 0u || size < length) return false;
    for (uint64_t offset = 0u; offset <= size - length; ++offset) {
        if (memcmp(data + offset, sequence, (size_t)length) == 0) return true;
    }
    return false;
}

static void verify_overaligned_location_mask(const ObjSection* locations,
                                             uint16_t architecture)
{
    static const uint8_t x86_mask[] = {
        0x23u, 0x1fu, 0x10u, 0xe0u, 0xffu, 0xffu, 0xffu, 0x0fu, 0x1au
    };
    static const uint8_t x64_mask[] = {
        0x23u, 0x1fu, 0x10u, 0xe0u, 0xffu, 0xffu, 0xffu, 0xffu,
        0xffu, 0xffu, 0xffu, 0xffu, 0x01u, 0x1au
    };
    const uint8_t* mask = architecture == ARCH_X64 ? x64_mask : x86_mask;
    uint64_t mask_size = architecture == ARCH_X64
        ? sizeof(x64_mask) : sizeof(x86_mask);
    assert(locations != NULL &&
           contains_sequence(locations->data, locations->size,
                             mask, mask_size));
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

static void verify_object_pointer_parameter(const char* path,
                                            uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    bool found = false;

    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && abbrev != NULL && strings != NULL);
    assert(contains_byte_pair(abbrev->data, abbrev->size, 0x64u, 0x13u));
    assert(contains_byte_pair(abbrev->data, abbrev->size, 0x34u, 0x0cu));

    for (uint64_t die = 11u; die + 1u + 4u + address_size + 4u + 4u +
             4u + 4u + 1u + 4u <= info->size; ++die) {
        uint32_t name_offset;
        uint32_t linkage_name_offset;
        uint32_t object_pointer_offset;
        uint32_t containing_type_offset;
        uint64_t cursor;
        uint64_t expression_size;
        uint64_t parameter_cursor;
        uint64_t parameter_expression_size;
        uint32_t parameter_name_offset;

        if (info->data[die] != 26u && info->data[die] != 29u) continue;
        name_offset = read_u32(info->data, die + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset, "read") != 0) {
            continue;
        }
        linkage_name_offset = read_u32(
            info->data, die + 1u + 4u + address_size + 4u + 4u + 4u +
                       4u + 1u);
        assert(linkage_name_offset < strings->size);
        assert(strcmp((const char*)strings->data + linkage_name_offset,
                      "read") != 0);
        assert(strings->data[linkage_name_offset] == '_');
        cursor = die + 1u + 4u + address_size + 4u + 4u + 4u + 4u +
                 1u + 4u + 4u;
        assert(cursor < info->size);
        expression_size = read_uleb(info->data, info->size, &cursor);
        assert(cursor + expression_size + 5u <= info->size);
        cursor += expression_size;
        ++cursor; /* DW_AT_inline */
        object_pointer_offset = read_u32(info->data, cursor);
        containing_type_offset = read_u32(info->data, cursor + 4u);
        cursor += 8u;
        assert(object_pointer_offset == cursor + 2u);
        assert(containing_type_offset < info->size);
        assert(info->data[containing_type_offset] == 12u);
        {
            uint32_t containing_name_offset =
                read_u32(info->data, containing_type_offset + 1u);
            assert(containing_name_offset < strings->size);
            assert(strcmp((const char*)strings->data + containing_name_offset,
                          "DebugMemberObject") == 0);
        }
        assert(info->data[object_pointer_offset] == 27u);
        parameter_name_offset = read_u32(info->data,
                                         object_pointer_offset + 1u);
        assert(parameter_name_offset < strings->size);
        assert(strcmp((const char*)strings->data + parameter_name_offset,
                      "this") == 0);
        parameter_cursor = object_pointer_offset + 1u + 4u + 4u +
                           4u + 4u + 4u;
        parameter_expression_size = read_uleb(
            info->data, info->size, &parameter_cursor);
        assert(parameter_cursor + parameter_expression_size < info->size);
        parameter_cursor += parameter_expression_size;
        assert(info->data[parameter_cursor] == 1u);
        found = true;
        break;
    }
    assert(found);
    objfile_free(object);
}

static void verify_static_member_containing_type(const char* path,
                                                 uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    bool found = false;

    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && abbrev != NULL && strings != NULL);
    assert(contains_byte_pair(abbrev->data, abbrev->size, 0x1du, 0x13u));

    for (uint64_t die = 11u; die + 1u + 4u + address_size + 4u + 4u +
             4u + 4u + 1u + 4u <= info->size; ++die) {
        uint32_t name_offset;
        uint32_t linkage_name_offset;
        uint32_t containing_type_offset;
        uint64_t cursor;
        uint64_t expression_size;

        if (info->data[die] != 28u && info->data[die] != 30u) continue;
        name_offset = read_u32(info->data, die + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset,
                   "create_value") != 0) {
            continue;
        }
        cursor = die + 1u + 4u + address_size + 4u + 4u + 4u + 4u + 1u;
        linkage_name_offset = read_u32(info->data, cursor);
        assert(linkage_name_offset < strings->size);
        assert(strcmp((const char*)strings->data + linkage_name_offset,
                      "create_value") != 0);
        assert(strings->data[linkage_name_offset] == '_');
        cursor += 8u; /* DW_AT_linkage_name and DW_AT_type */
        expression_size = read_uleb(info->data, info->size, &cursor);
        assert(cursor + expression_size + 5u <= info->size);
        cursor += expression_size + 1u; /* frame base, DW_AT_inline */
        containing_type_offset = read_u32(info->data, cursor);
        assert(containing_type_offset < info->size);
        assert(info->data[containing_type_offset] == 12u);
        {
            uint32_t containing_name_offset = read_u32(
                info->data, containing_type_offset + 1u);
            assert(containing_name_offset < strings->size);
            assert(strcmp((const char*)strings->data + containing_name_offset,
                          "DebugMemberObject") == 0);
        }
        assert(info->data[cursor + 4u] == 1u);
        found = true;
        break;
    }
    assert(found);
    objfile_free(object);
}

static void verify_cxx_member_accessibility(const char* path,
                                            uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    bool found_type = false;
    bool found_private = false;
    bool found_protected = false;
    bool found_public = false;

    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && abbrev != NULL && strings != NULL);
    assert(contains_byte_pair(abbrev->data, abbrev->size, 0x32u, 0x0bu));

    for (uint64_t type_die = 11u; type_die + 9u <= info->size; ++type_die) {
        uint32_t type_name_offset;
        uint64_t cursor;
        if (info->data[type_die] != 12u) continue;
        type_name_offset = read_u32(info->data, type_die + 1u);
        if (type_name_offset >= strings->size ||
            strcmp((const char*)strings->data + type_name_offset,
                   "DebugMemberObject") != 0) {
            continue;
        }
        found_type = true;
        cursor = type_die + 9u;
        while (cursor < info->size && info->data[cursor] != 0u) {
            uint8_t member_abbreviation = info->data[cursor++];
            uint32_t member_name_offset;
            uint64_t expression_size;
            uint8_t accessibility;
            if (member_abbreviation == 46u) {
                assert(cursor + 21u <= info->size);
                cursor += 21u;
                continue;
            }
            if (member_abbreviation != 14u && member_abbreviation != 17u) {
                fprintf(stderr,
                        "unexpected class child abbreviation %u at %llu\n",
                        (unsigned)member_abbreviation,
                        (unsigned long long)(cursor - 1u));
            }
            assert(member_abbreviation == 14u || member_abbreviation == 17u);
            assert(cursor + 8u <= info->size);
            member_name_offset = read_u32(info->data, cursor);
            assert(member_name_offset < strings->size);
            cursor += 8u; /* member name and type reference */
            expression_size = read_uleb(info->data, info->size, &cursor);
            assert(expression_size <= info->size - cursor);
            cursor += expression_size;
            if (member_abbreviation == 17u) {
                assert(cursor + 8u <= info->size);
                cursor += 8u; /* bit size and data bit offset */
            }
            assert(cursor < info->size);
            accessibility = info->data[cursor++];
            if (strcmp((const char*)strings->data + member_name_offset,
                       "secret") == 0) {
                assert(accessibility == 3u);
                found_private = true;
            } else if (strcmp(
                           (const char*)strings->data + member_name_offset,
                           "protected_value") == 0) {
                assert(accessibility == 2u);
                found_protected = true;
            } else if (strcmp(
                           (const char*)strings->data + member_name_offset,
                           "value") == 0) {
                assert(accessibility == 1u);
                found_public = true;
            }
        }
        break;
    }
    assert(found_type && found_private && found_protected && found_public);
    objfile_free(object);
}

static uint64_t find_structure_type_die(const ObjSection* info,
                                        const ObjSection* strings,
                                        const char* name)
{
    if (!info || !strings || !name) return UINT64_MAX;
    for (uint64_t offset = 11u; offset + 9u <= info->size; ++offset) {
        uint32_t name_offset;
        if (info->data[offset] != 12u) continue;
        name_offset = read_u32(info->data, offset + 1u);
        if (name_offset < strings->size &&
            strcmp((const char*)strings->data + name_offset, name) == 0) {
            return offset;
        }
    }
    return UINT64_MAX;
}

static const char* verify_skip_cxx_member_die(const ObjSection* info,
                                             const ObjSection* strings,
                                             uint64_t* cursor,
                                             uint8_t abbreviation)
{
    uint32_t name_offset;
    uint64_t expression_size;
    assert(info != NULL && strings != NULL && cursor != NULL);
    assert(abbreviation == 14u || abbreviation == 17u);
    assert(*cursor + 8u <= info->size);
    name_offset = read_u32(info->data, *cursor);
    assert(name_offset < strings->size);
    *cursor += 8u; /* name and type reference */
    expression_size = read_uleb(info->data, info->size, cursor);
    assert(expression_size <= info->size - *cursor);
    *cursor += expression_size;
    if (abbreviation == 17u) {
        assert(*cursor + 8u <= info->size);
        *cursor += 8u; /* bit size and data bit offset */
    }
    assert(*cursor < info->size);
    ++*cursor; /* accessibility */
    return (const char*)strings->data + name_offset;
}

static void verify_virtual_inheritance_child(
    const ObjSection* info, const ObjSection* strings, uint64_t* cursor,
    const char* expected_base, uint16_t architecture,
    uint32_t expected_vbase_pointer_offset, uint32_t expected_table_index,
    uint8_t expected_access)
{
    uint32_t type_offset;
    uint32_t type_name_offset;
    uint64_t expression_size;
    uint64_t expression_end;
    uint64_t expression_cursor;
    uint64_t value;
    uint32_t pointer_size = architecture == ARCH_X64 ? 8u : 4u;
    assert(info != NULL && strings != NULL && cursor != NULL);
    assert(*cursor + 4u <= info->size);
    type_offset = read_u32(info->data, *cursor);
    *cursor += 4u;
    assert((uint64_t)type_offset + 5u <= info->size &&
           info->data[type_offset] == 12u);
    type_name_offset = read_u32(info->data, type_offset + 1u);
    assert(type_name_offset < strings->size &&
           strcmp((const char*)strings->data + type_name_offset,
                  expected_base) == 0);

    expression_size = read_uleb(info->data, info->size, cursor);
    assert(expression_size <= info->size - *cursor);
    expression_end = *cursor + expression_size;
    expression_cursor = *cursor;
    assert(expression_cursor < expression_end &&
           info->data[expression_cursor++] == 0x12u); /* DW_OP_dup */
    assert(expression_cursor < expression_end &&
           info->data[expression_cursor++] == 0x23u); /* plus_uconst */
    value = read_uleb(info->data, expression_end, &expression_cursor);
    assert(value == expected_vbase_pointer_offset);
    assert(expression_cursor + 2u <= expression_end &&
           info->data[expression_cursor++] == 0x94u &&
           info->data[expression_cursor++] == pointer_size);
    assert(expression_cursor < expression_end &&
           info->data[expression_cursor++] == 0x10u); /* DW_OP_constu */
    value = read_uleb(info->data, expression_end, &expression_cursor);
    assert(value == (uint64_t)expected_table_index * pointer_size);
    assert(expression_cursor + 2u <= expression_end &&
           info->data[expression_cursor++] == 0x22u &&
           info->data[expression_cursor++] == 0x94u);
    assert(expression_cursor < expression_end &&
           info->data[expression_cursor++] == pointer_size);
    assert(expression_cursor < expression_end &&
           info->data[expression_cursor++] == 0x22u &&
           expression_cursor == expression_end);
    *cursor = expression_end;
    assert(*cursor + 2u <= info->size);
    assert(info->data[(*cursor)++] == 1u); /* DW_VIRTUALITY_virtual */
    assert(info->data[(*cursor)++] == expected_access);
}

static void verify_cxx_inheritance_dies(const char* path,
                                        uint16_t architecture)
{
    static const uint8_t inheritance_abbrev[] = {
        43u, 0x1cu, 0u, 0x49u, 0x13u, 0x02u, 0x18u, 0x32u, 0x0bu, 0u, 0u
    };
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    uint64_t derived_die;
    bool found_public_left = false;
    bool found_private_right = false;
    bool found_own_member = false;
    bool leaked_inherited_member = false;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && abbrev != NULL && strings != NULL);
    assert(contains_sequence(abbrev->data, abbrev->size,
                             inheritance_abbrev,
                             sizeof(inheritance_abbrev)));

    derived_die = find_structure_type_die(
        info, strings, "DebugInheritanceDerived");
    assert(derived_die != UINT64_MAX);
    {
        uint64_t cursor = derived_die + 9u;
        while (cursor < info->size && info->data[cursor] != 0u) {
            uint8_t child = info->data[cursor++];
            if (child == 43u) {
                uint32_t base_type_offset;
                uint32_t base_name_offset;
                uint64_t expression_size;
                uint64_t expression_cursor;
                uint64_t base_offset;
                uint8_t accessibility;
                assert(cursor + 4u <= info->size);
                base_type_offset = read_u32(info->data, cursor);
                cursor += 4u;
                assert((uint64_t)base_type_offset + 5u <= info->size &&
                       info->data[base_type_offset] == 12u);
                base_name_offset = read_u32(info->data,
                                            base_type_offset + 1u);
                assert(base_name_offset < strings->size);
                expression_size = read_uleb(info->data, info->size,
                                            &cursor);
                assert(expression_size == 2u &&
                       cursor + expression_size + 1u <= info->size &&
                       info->data[cursor] == 0x23u);
                expression_cursor = cursor + 1u;
                base_offset = read_uleb(info->data,
                                        cursor + expression_size,
                                        &expression_cursor);
                assert(expression_cursor == cursor + expression_size);
                cursor += expression_size;
                accessibility = info->data[cursor++];
                if (strcmp((const char*)strings->data + base_name_offset,
                           "DebugInheritanceLeft") == 0) {
                    assert(base_offset == 0u && accessibility == 1u);
                    found_public_left = true;
                } else if (strcmp(
                               (const char*)strings->data + base_name_offset,
                               "DebugInheritanceRight") == 0) {
                    assert(base_offset == 4u && accessibility == 3u);
                    found_private_right = true;
                } else {
                    assert(false && "unexpected direct base type");
                }
            } else {
                uint32_t member_name_offset;
                uint64_t expression_size;
                assert(child == 14u || child == 17u);
                assert(cursor + 8u <= info->size);
                member_name_offset = read_u32(info->data, cursor);
                assert(member_name_offset < strings->size);
                cursor += 8u;
                expression_size = read_uleb(info->data, info->size,
                                            &cursor);
                assert(expression_size <= info->size - cursor);
                cursor += expression_size;
                if (child == 17u) {
                    assert(cursor + 8u <= info->size);
                    cursor += 8u;
                }
                assert(cursor < info->size);
                ++cursor; /* DW_AT_accessibility */
                if (strcmp((const char*)strings->data + member_name_offset,
                           "own") == 0) {
                    found_own_member = true;
                }
                if (strcmp((const char*)strings->data + member_name_offset,
                           "left") == 0 ||
                    strcmp((const char*)strings->data + member_name_offset,
                           "right") == 0) {
                    leaked_inherited_member = true;
                }
            }
        }
    }
    assert(found_public_left && found_private_right && found_own_member &&
           !leaked_inherited_member);

    /* Direct virtual bases use the real vbase-pointer-table expression, while
     * a non-virtual intermediate keeps its own fixed subobject offset. */
    {
        uint64_t virtual_die = find_structure_type_die(
            info, strings, "DebugVirtualInheritanceDerived");
        unsigned virtual_base_count = 0u;
        bool found_direct_member = false;
        bool leaked_virtual_member = false;
        uint64_t cursor;
        assert(virtual_die != UINT64_MAX);
        cursor = virtual_die + 9u;
        while (cursor < info->size && info->data[cursor] != 0u) {
            uint8_t child = info->data[cursor++];
            if (child == 45u) {
                const char* base_name = virtual_base_count == 0u
                    ? "DebugVirtualInheritanceBase"
                    : "DebugVirtualInheritanceOther";
                static const uint8_t virtual_inheritance_abbrev[] = {
                    45u, 0x1cu, 0u, 0x49u, 0x13u, 0x02u, 0x18u,
                    0x4cu, 0x0bu, 0x32u, 0x0bu, 0u, 0u
                };
                assert(contains_sequence(abbrev->data, abbrev->size,
                                         virtual_inheritance_abbrev,
                                         sizeof(virtual_inheritance_abbrev)));
                verify_virtual_inheritance_child(
                    info, strings, &cursor, base_name, architecture,
                    architecture == ARCH_X64 ? 8u : 4u,
                    virtual_base_count, 1u);
                ++virtual_base_count;
                continue;
            }
            assert(child == 14u || child == 17u);
            {
                const char* member_name = verify_skip_cxx_member_die(
                    info, strings, &cursor, child);
                if (strcmp(member_name, "direct") == 0) {
                    found_direct_member = true;
                }
                if (strcmp(member_name, "virtual_member") == 0) {
                    leaked_virtual_member = true;
                }
            }
        }
        assert(virtual_base_count == 2u && found_direct_member &&
               !leaked_virtual_member);
    }
    {
        uint64_t base_die = find_structure_type_die(
            info, strings, "DebugVirtualInheritanceBase");
        uint64_t layer_die = find_structure_type_die(
            info, strings, "DebugVirtualInheritanceLayer");
        bool found_base_member = false;
        bool found_nonvirtual_base = false;
        bool found_layer_member = false;
        uint64_t cursor;
        assert(base_die != UINT64_MAX && layer_die != UINT64_MAX);
        cursor = base_die + 9u;
        while (cursor < info->size && info->data[cursor] != 0u) {
            uint8_t child = info->data[cursor++];
            assert(child == 14u || child == 17u);
            if (strcmp(verify_skip_cxx_member_die(info, strings, &cursor,
                                                 child),
                       "virtual_member") == 0) {
                found_base_member = true;
            }
        }
        assert(found_base_member);

        cursor = layer_die + 9u;
        while (cursor < info->size && info->data[cursor] != 0u) {
            uint8_t child = info->data[cursor++];
            if (child == 43u) {
                uint32_t base_type_offset;
                uint32_t base_name_offset;
                uint64_t expression_size;
                assert(cursor + 4u <= info->size);
                base_type_offset = read_u32(info->data, cursor);
                cursor += 4u;
                assert((uint64_t)base_type_offset + 5u <= info->size &&
                       info->data[base_type_offset] == 12u);
                base_name_offset = read_u32(info->data,
                                            base_type_offset + 1u);
                assert(base_name_offset < strings->size &&
                       strcmp((const char*)strings->data + base_name_offset,
                              "DebugVirtualInheritanceDerived") == 0);
                expression_size = read_uleb(info->data, info->size,
                                            &cursor);
                assert(expression_size == 2u &&
                       info->data[cursor] == 0x23u &&
                       info->data[cursor + 1u] == 0u);
                cursor += expression_size;
                assert(cursor < info->size && info->data[cursor++] == 1u);
                found_nonvirtual_base = true;
                continue;
            }
            assert(child == 14u || child == 17u);
            if (strcmp(verify_skip_cxx_member_die(info, strings, &cursor,
                                                 child),
                       "layer") == 0) {
                found_layer_member = true;
            }
        }
        assert(found_nonvirtual_base && found_layer_member);
    }
    objfile_free(object);
}

static void verify_typedef_die(const char* path, uint16_t architecture,
                              const char* typedef_name,
                              const char* expected_base_type)
{
    static const uint8_t typedef_abbrev[] = {
        44u, 0x16u, 0u, 0x03u, 0x0eu, 0x49u, 0x13u, 0x3au, 0x06u,
        0x3bu, 0x06u, 0x39u, 0x06u, 0u, 0u
    };
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    bool found = false;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && abbrev != NULL && strings != NULL);
    assert(contains_sequence(abbrev->data, abbrev->size,
                             typedef_abbrev, sizeof(typedef_abbrev)));
    for (uint64_t die = 11u; die + 21u <= info->size; ++die) {
        uint32_t name_offset;
        uint32_t type_offset;
        uint32_t file_index;
        uint32_t line;
        uint32_t column;
        uint32_t base_name_offset;
        if (info->data[die] != 44u) continue;
        name_offset = read_u32(info->data, die + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset,
                   typedef_name) != 0) {
            continue;
        }
        type_offset = read_u32(info->data, die + 5u);
        file_index = read_u32(info->data, die + 9u);
        line = read_u32(info->data, die + 13u);
        column = read_u32(info->data, die + 17u);
        assert((uint64_t)type_offset + 5u <= info->size &&
               (info->data[type_offset] == 5u ||
                info->data[type_offset] == 15u ||
                info->data[type_offset] == 36u));
        base_name_offset = read_u32(info->data, type_offset + 1u);
        assert(base_name_offset < strings->size &&
               strcmp((const char*)strings->data + base_name_offset,
                      expected_base_type) == 0);
        assert(info->data[type_offset + 5u] ==
                   (strcmp(expected_base_type, "unsigned short") == 0
                        ? 2u
                        : strcmp(expected_base_type,
                                 "unsigned long long") == 0 ? 8u : 4u) &&
               info->data[type_offset + 6u] == 0x07u);
        assert(file_index > 0u && line > 0u && column > 0u);
        found = true;
        break;
    }
    assert(found);
    objfile_free(object);
}

static void verify_class_template_parameter_dies(const char* path,
                                                uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    uint64_t class_die;
    uint64_t cursor;
    uint32_t name_offset;
    uint32_t type_offset;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && abbrev != NULL && strings != NULL);
    assert(contains_byte_pair(abbrev->data, abbrev->size, 0x2fu, 0x00u));
    assert(contains_byte_pair(abbrev->data, abbrev->size, 0x30u, 0x00u));
    class_die = UINT64_MAX;
    for (uint64_t offset = 11u; offset + 9u <= info->size; ++offset) {
        uint32_t class_name_offset;
        const char* class_name;
        if (info->data[offset] != 12u) continue;
        class_name_offset = read_u32(info->data, offset + 1u);
        if (class_name_offset >= strings->size) continue;
        class_name = (const char*)strings->data + class_name_offset;
        if (strncmp(class_name, "DebugTemplateBox.",
                    sizeof("DebugTemplateBox.") - 1u) == 0) {
            class_die = offset;
            break;
        }
    }
    assert(class_die != UINT64_MAX);
    cursor = class_die + 9u;

    assert(cursor < info->size && info->data[cursor++] == 47u);
    assert(cursor + 8u <= info->size);
    name_offset = read_u32(info->data, cursor);
    type_offset = read_u32(info->data, cursor + 4u);
    cursor += 8u;
    assert(name_offset < strings->size &&
           strcmp((const char*)strings->data + name_offset, "Element") == 0);
    assert(type_offset + 9u <= info->size && info->data[type_offset] == 12u);
    name_offset = read_u32(info->data, type_offset + 1u);
    assert(name_offset < strings->size &&
           strcmp((const char*)strings->data + name_offset,
                  "DebugTemplatePayload") == 0);

    assert(cursor < info->size && info->data[cursor++] == 48u);
    assert(cursor + 8u <= info->size);
    name_offset = read_u32(info->data, cursor);
    type_offset = read_u32(info->data, cursor + 4u);
    cursor += 8u;
    assert(name_offset < strings->size &&
           strcmp((const char*)strings->data + name_offset, "Count") == 0);
    assert(type_offset + 7u <= info->size && info->data[type_offset] == 5u);
    name_offset = read_u32(info->data, type_offset + 1u);
    assert(name_offset < strings->size &&
           strcmp((const char*)strings->data + name_offset, "int") == 0);
    assert(read_sleb(info->data, info->size, &cursor) == 3);

    assert(cursor < info->size && info->data[cursor++] == 48u);
    assert(cursor + 8u <= info->size);
    name_offset = read_u32(info->data, cursor);
    type_offset = read_u32(info->data, cursor + 4u);
    cursor += 8u;
    assert(name_offset < strings->size &&
           strcmp((const char*)strings->data + name_offset, "Bias") == 0);
    assert(type_offset + 7u <= info->size && info->data[type_offset] == 5u);
    name_offset = read_u32(info->data, type_offset + 1u);
    assert(name_offset < strings->size &&
           strcmp((const char*)strings->data + name_offset, "int") == 0);
    assert(read_sleb(info->data, info->size, &cursor) == -7);

    assert(cursor < info->size && info->data[cursor++] == 49u);
    assert(cursor + 8u <= info->size);
    name_offset = read_u32(info->data, cursor);
    type_offset = read_u32(info->data, cursor + 4u);
    cursor += 8u;
    assert(name_offset < strings->size &&
           strcmp((const char*)strings->data + name_offset, "Mask") == 0);
    assert(type_offset + 7u <= info->size && info->data[type_offset] == 5u);
    name_offset = read_u32(info->data, type_offset + 1u);
    assert(name_offset < strings->size &&
           strcmp((const char*)strings->data + name_offset,
                  "unsigned int") == 0);
    assert(read_uleb(info->data, info->size, &cursor) == UINT32_C(0xf0000000));
    assert(cursor < info->size && info->data[cursor] == 14u);
    objfile_free(object);
}

static void verify_class_template_template_parameter_die(
    const char* path, uint16_t architecture)
{
    static const uint8_t template_template_abbreviation[] = {
        54u, 0x86u, 0x82u, 0x01u, 0u, 0x03u, 0x0eu,
        0x90u, 0x42u, 0x0eu, 0u, 0u
    };
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    bool found_holder = false;
    unsigned global_parameter_count = 0u;
    unsigned qualified_parameter_count = 0u;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && abbrev != NULL && strings != NULL);
    assert(contains_sequence(abbrev->data, abbrev->size,
                             template_template_abbreviation,
                             sizeof(template_template_abbreviation)));

    for (uint64_t die = 11u; die + 10u <= info->size; ++die) {
        uint32_t class_name_offset;
        const char* class_name;
        if (info->data[die] != 12u) continue;
        class_name_offset = read_u32(info->data, die + 1u);
        if (class_name_offset >= strings->size) continue;
        class_name = (const char*)strings->data + class_name_offset;
        if (strncmp(class_name, "DebugTemplateTemplateHolder.",
                    sizeof("DebugTemplateTemplateHolder.") - 1u) != 0) {
            continue;
        }
        found_holder = true;
        if (info->data[die + 9u] != 54u) continue;
        {
            uint32_t parameter_name_offset = read_u32(info->data, die + 10u);
            uint32_t template_name_offset = read_u32(info->data, die + 14u);
            if (parameter_name_offset >= strings->size ||
                template_name_offset >= strings->size) {
                continue;
            }
            if (strcmp((const char*)strings->data + parameter_name_offset,
                       "Template") != 0) {
                continue;
            }
            if (strcmp((const char*)strings->data + template_name_offset,
                       "DebugTemplateTemplateLeaf") == 0) {
                ++global_parameter_count;
            } else if (strcmp(
                           (const char*)strings->data + template_name_offset,
                           "debug_template_namespace::Leaf") == 0) {
                ++qualified_parameter_count;
            }
        }
    }
    assert(found_holder && global_parameter_count == 1u &&
           qualified_parameter_count == 1u);
    objfile_free(object);
}

static void verify_class_template_template_parameter_pack_dies(
    const char* path, uint16_t architecture)
{
    static const uint8_t template_template_pack_abbreviation[] = {
        50u, 0x87u, 0x82u, 0x01u, 1u, 0x03u, 0x0eu, 0u, 0u
    };
    static const uint8_t template_template_pack_element_abbreviation[] = {
        55u, 0x86u, 0x82u, 0x01u, 0u, 0x90u, 0x42u, 0x0eu, 0u, 0u
    };
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    unsigned holder_count = 0u;
    unsigned empty_pack_count = 0u;
    unsigned single_pack_count = 0u;
    unsigned two_element_pack_count = 0u;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && abbrev != NULL && strings != NULL);
    assert(contains_sequence(abbrev->data, abbrev->size,
                             template_template_pack_abbreviation,
                             sizeof(template_template_pack_abbreviation)));
    assert(contains_sequence(abbrev->data, abbrev->size,
                             template_template_pack_element_abbreviation,
                             sizeof(template_template_pack_element_abbreviation)));

    for (uint64_t die = 11u; die + 14u <= info->size; ++die) {
        uint32_t class_name_offset;
        const char* class_name;
        uint32_t pack_name_offset;
        uint64_t cursor;
        unsigned element_count = 0u;
        bool found_global_template = false;
        bool found_qualified_template = false;
        if (info->data[die] != 12u) continue;
        class_name_offset = read_u32(info->data, die + 1u);
        if (class_name_offset >= strings->size) continue;
        class_name = (const char*)strings->data + class_name_offset;
        if (strncmp(class_name, "DebugTemplateTemplatePackHolder.",
                    sizeof("DebugTemplateTemplatePackHolder.") - 1u) != 0) {
            continue;
        }
        ++holder_count;
        cursor = die + 9u;
        assert(info->data[cursor++] == 50u);
        pack_name_offset = read_u32(info->data, cursor);
        cursor += 4u;
        assert(pack_name_offset < strings->size &&
               strcmp((const char*)strings->data + pack_name_offset,
                      "Template") == 0);
        while (cursor < info->size && info->data[cursor] != 0u) {
            uint32_t template_name_offset;
            const char* template_name;
            assert(cursor + 5u <= info->size &&
                   info->data[cursor] == 55u);
            template_name_offset = read_u32(info->data, cursor + 1u);
            assert(template_name_offset < strings->size);
            template_name = (const char*)strings->data +
                template_name_offset;
            if (strcmp(template_name, "DebugTemplateTemplateLeaf") == 0) {
                found_global_template = true;
            } else if (strcmp(template_name,
                              "debug_template_namespace::Leaf") == 0) {
                found_qualified_template = true;
            } else {
                assert(false);
            }
            ++element_count;
            cursor += 5u;
        }
        assert(cursor < info->size && info->data[cursor++] == 0u);
        assert(cursor < info->size);
        if (element_count == 0u) {
            ++empty_pack_count;
        } else if (element_count == 1u && found_global_template) {
            ++single_pack_count;
        } else if (element_count == 2u && found_global_template &&
                   found_qualified_template) {
            ++two_element_pack_count;
        } else {
            assert(false);
        }
    }
    assert(holder_count == 3u && empty_pack_count == 1u &&
           single_pack_count == 1u && two_element_pack_count == 1u);
    objfile_free(object);
}

static void verify_class_template_parameter_pack_dies(
    const char* path, uint16_t architecture)
{
    static const uint8_t pack_abbreviation[] = {
        50u, 0x87u, 0x82u, 0x01u, 1u, 0x03u, 0x0eu, 0u, 0u
    };
    static const uint8_t pack_element_abbreviation[] = {
        51u, 0x2fu, 0u, 0x49u, 0x13u, 0u, 0u
    };
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    unsigned empty_pack_count = 0u;
    unsigned two_element_pack_count = 0u;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && abbrev != NULL && strings != NULL);
    assert(contains_sequence(abbrev->data, abbrev->size,
                             pack_abbreviation,
                             sizeof(pack_abbreviation)));
    assert(contains_sequence(abbrev->data, abbrev->size,
                             pack_element_abbreviation,
                             sizeof(pack_element_abbreviation)));

    for (uint64_t die = 11u; die + 9u <= info->size; ++die) {
        uint32_t class_name_offset;
        const char* class_name;
        uint64_t cursor;
        uint32_t pack_name_offset;
        unsigned element_count = 0u;
        if (info->data[die] != 12u) continue;
        class_name_offset = read_u32(info->data, die + 1u);
        if (class_name_offset >= strings->size) continue;
        class_name = (const char*)strings->data + class_name_offset;
        if (strncmp(class_name, "DebugTemplatePack.",
                    sizeof("DebugTemplatePack.") - 1u) != 0) {
            continue;
        }

        cursor = die + 9u;
        assert(cursor < info->size && info->data[cursor++] == 47u);
        assert(cursor + 8u <= info->size);
        cursor += 8u; /* fixed Head template argument */
        assert(cursor < info->size && info->data[cursor++] == 50u);
        assert(cursor + 4u <= info->size);
        pack_name_offset = read_u32(info->data, cursor);
        cursor += 4u;
        assert(pack_name_offset < strings->size &&
               strcmp((const char*)strings->data + pack_name_offset,
                      "Tail") == 0);
        while (cursor < info->size && info->data[cursor] == 51u) {
            uint32_t type_offset;
            uint32_t type_name_offset;
            assert(cursor + 5u <= info->size);
            ++cursor;
            type_offset = read_u32(info->data, cursor);
            cursor += 4u;
            assert((uint64_t)type_offset + 7u <= info->size &&
                   info->data[type_offset] == 5u);
            type_name_offset = read_u32(info->data, type_offset + 1u);
            assert(type_name_offset < strings->size);
            if (element_count == 0u) {
                assert(strcmp((const char*)strings->data + type_name_offset,
                              "short") == 0);
            } else if (element_count == 1u) {
                assert(strcmp((const char*)strings->data + type_name_offset,
                              "long") == 0);
            } else {
                assert(0 && "unexpected extra class-template pack element");
            }
            ++element_count;
        }
        assert(cursor < info->size && info->data[cursor++] == 0u);
        if (element_count == 0u) {
            ++empty_pack_count;
        } else {
            assert(element_count == 2u);
            ++two_element_pack_count;
        }
    }
    assert(empty_pack_count == 1u && two_element_pack_count == 1u);
    objfile_free(object);
}

static void verify_class_non_type_template_parameter_pack_dies(
    const char* path, uint16_t architecture, const char* class_prefix,
    const char* expected_type, bool is_unsigned,
    uint64_t expected_unsigned_value)
{
    static const uint8_t signed_pack_element_abbreviation[] = {
        52u, 0x30u, 0u, 0x49u, 0x13u, 0x1cu, 0x0du, 0u, 0u
    };
    static const uint8_t unsigned_pack_element_abbreviation[] = {
        53u, 0x30u, 0u, 0x49u, 0x13u, 0x1cu, 0x0fu, 0u, 0u
    };
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    unsigned empty_count = 0u;
    unsigned populated_count = 0u;
    unsigned expected_children = is_unsigned ? 1u : 2u;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && abbrev != NULL && strings != NULL);
    assert(contains_sequence(
        abbrev->data, abbrev->size,
        is_unsigned ? unsigned_pack_element_abbreviation
                    : signed_pack_element_abbreviation,
        is_unsigned ? sizeof(unsigned_pack_element_abbreviation)
                    : sizeof(signed_pack_element_abbreviation)));

    for (uint64_t die = 11u; die + 9u <= info->size; ++die) {
        uint32_t class_name_offset;
        const char* class_name;
        uint64_t cursor;
        uint32_t type_offset;
        uint32_t type_name_offset;
        uint32_t pack_name_offset;
        unsigned element_count = 0u;
        if (info->data[die] != 12u) continue;
        class_name_offset = read_u32(info->data, die + 1u);
        if (class_name_offset >= strings->size) continue;
        class_name = (const char*)strings->data + class_name_offset;
        if (strncmp(class_name, class_prefix, strlen(class_prefix)) != 0) {
            continue;
        }

        cursor = die + 9u;
        assert(cursor + 9u <= info->size && info->data[cursor++] == 47u);
        type_offset = read_u32(info->data, cursor + 4u);
        cursor += 8u;
        assert((uint64_t)type_offset + 7u <= info->size &&
               info->data[type_offset] == 5u);
        type_name_offset = read_u32(info->data, type_offset + 1u);
        assert(type_name_offset < strings->size &&
               strcmp((const char*)strings->data + type_name_offset,
                      "int") == 0);
        assert(cursor < info->size && info->data[cursor++] == 50u);
        assert(cursor + 4u <= info->size);
        pack_name_offset = read_u32(info->data, cursor);
        cursor += 4u;
        assert(pack_name_offset < strings->size &&
               strcmp((const char*)strings->data + pack_name_offset,
                      "Values") == 0);
        while (cursor < info->size &&
               info->data[cursor] == (is_unsigned ? 53u : 52u)) {
            uint64_t value_offset;
            int64_t signed_value;
            uint64_t unsigned_value;
            assert(cursor + 5u <= info->size);
            ++cursor;
            type_offset = read_u32(info->data, cursor);
            cursor += 4u;
            assert((uint64_t)type_offset + 7u <= info->size &&
                   info->data[type_offset] == 5u);
            type_name_offset = read_u32(info->data, type_offset + 1u);
            assert(type_name_offset < strings->size &&
                   strcmp((const char*)strings->data + type_name_offset,
                          expected_type) == 0);
            value_offset = cursor;
            if (is_unsigned) {
                unsigned_value = read_uleb(info->data, info->size, &cursor);
                assert(element_count == 0u &&
                       unsigned_value == expected_unsigned_value);
            } else {
                signed_value = read_sleb(info->data, info->size, &cursor);
                assert((element_count == 0u && signed_value == -2) ||
                       (element_count == 1u && signed_value == 7));
            }
            assert(cursor > value_offset);
            ++element_count;
        }
        assert(cursor < info->size && info->data[cursor++] == 0u);
        if (element_count == 0u) {
            assert(!is_unsigned);
            ++empty_count;
        } else {
            assert(element_count == expected_children);
            ++populated_count;
        }
    }
    assert(empty_count == (is_unsigned ? 0u : 1u));
    assert(populated_count == 1u);
    objfile_free(object);
}

static void verify_zero_argument_non_type_template_pack_die(
    const char* path, uint16_t architecture, const char* class_prefix)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    unsigned found = 0u;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && strings != NULL);
    for (uint64_t die = 11u; die + 9u <= info->size; ++die) {
        uint32_t class_name_offset;
        const char* class_name;
        uint64_t cursor;
        uint32_t pack_name_offset;
        if (info->data[die] != 12u) continue;
        class_name_offset = read_u32(info->data, die + 1u);
        if (class_name_offset >= strings->size) continue;
        class_name = (const char*)strings->data + class_name_offset;
        if (strncmp(class_name, class_prefix, strlen(class_prefix)) != 0) {
            continue;
        }
        cursor = die + 9u;
        assert(cursor < info->size && info->data[cursor++] == 50u);
        assert(cursor + 4u <= info->size);
        pack_name_offset = read_u32(info->data, cursor);
        cursor += 4u;
        assert(pack_name_offset < strings->size &&
               strcmp((const char*)strings->data + pack_name_offset,
                      "Values") == 0);
        assert(cursor < info->size && info->data[cursor++] == 0u);
        ++found;
    }
    assert(found == 1u);
    objfile_free(object);
}

static void verify_class_dependent_non_type_template_parameter_pack_dies(
    const char* path, uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    unsigned empty_count = 0u;
    unsigned signed_count = 0u;
    unsigned unsigned_count = 0u;
    unsigned same_value_different_type_count = 0u;
    unsigned enum_count = 0u;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && strings != NULL);

    for (uint64_t die = 11u; die + 9u <= info->size; ++die) {
        uint32_t class_name_offset;
        const char* class_name;
        uint64_t cursor;
        uint32_t type_offset;
        uint32_t type_name_offset;
        uint32_t value_type_offset;
        const char* value_type_name;
        bool value_is_enum;
        uint32_t pack_name_offset;
        unsigned element_count = 0u;
        if (info->data[die] != 12u) continue;
        class_name_offset = read_u32(info->data, die + 1u);
        if (class_name_offset >= strings->size) continue;
        class_name = (const char*)strings->data + class_name_offset;
        if (strncmp(class_name, "DebugTemplateDependentValuePack.",
                    sizeof("DebugTemplateDependentValuePack.") - 1u) != 0) {
            continue;
        }

        cursor = die + 9u;
        assert(cursor < info->size && info->data[cursor++] == 47u);
        assert(cursor + 8u <= info->size);
        type_offset = read_u32(info->data, cursor + 4u);
        cursor += 8u;
        assert((uint64_t)type_offset + 5u <= info->size &&
               (info->data[type_offset] == 5u ||
                info->data[type_offset] == 15u ||
                info->data[type_offset] == 36u));
        value_type_offset = type_offset;
        value_is_enum = info->data[type_offset] == 15u ||
                        info->data[type_offset] == 36u;
        type_name_offset = read_u32(info->data, type_offset + 1u);
        assert(type_name_offset < strings->size);
        value_type_name = (const char*)strings->data + type_name_offset;
        assert(cursor < info->size && info->data[cursor++] == 50u);
        assert(cursor + 4u <= info->size);
        pack_name_offset = read_u32(info->data, cursor);
        cursor += 4u;
        assert(pack_name_offset < strings->size &&
               strcmp((const char*)strings->data + pack_name_offset,
                      "Values") == 0);
        while (cursor < info->size &&
               (info->data[cursor] == 52u || info->data[cursor] == 53u)) {
            bool is_unsigned = info->data[cursor] == 53u;
            uint64_t value;
            assert(cursor + 5u <= info->size);
            ++cursor;
            type_offset = read_u32(info->data, cursor);
            cursor += 4u;
            assert(type_offset == value_type_offset);
            if (is_unsigned) {
                value = read_uleb(info->data, info->size, &cursor);
                assert(!value_is_enum &&
                       strcmp(value_type_name, "unsigned short") == 0 &&
                       element_count == 0u && value == UINT16_C(0xf000));
            } else {
                int64_t signed_value = read_sleb(info->data, info->size,
                                                 &cursor);
                if (value_is_enum) {
                    assert(strcmp(value_type_name,
                                  "DebugTemplateDependentValueEnum") == 0 &&
                           ((element_count == 0u && signed_value == -3) ||
                            (element_count == 1u && signed_value == 12)));
                } else if (strcmp(value_type_name, "short") == 0) {
                    assert((element_count == 0u && signed_value == -2) ||
                           (element_count == 1u && signed_value == 7));
                } else {
                    assert(strcmp(value_type_name, "int") == 0 &&
                           element_count == 0u && signed_value == 7);
                }
            }
            ++element_count;
        }
        assert(cursor < info->size && info->data[cursor++] == 0u);
        if (strcmp(value_type_name, "short") == 0) {
            if (element_count == 0u) ++empty_count;
            else {
                assert(element_count == 2u);
                ++signed_count;
            }
        } else if (strcmp(value_type_name, "unsigned short") == 0) {
            assert(element_count == 1u);
            ++unsigned_count;
        } else if (value_is_enum) {
            assert(strcmp(value_type_name,
                          "DebugTemplateDependentValueEnum") == 0 &&
                   element_count == 2u);
            ++enum_count;
        } else {
            assert(strcmp(value_type_name, "int") == 0 &&
                   element_count == 1u);
            ++same_value_different_type_count;
        }
    }
    assert(empty_count == 1u && signed_count == 1u && unsigned_count == 1u &&
           same_value_different_type_count == 1u && enum_count == 1u);
    objfile_free(object);
}

static void verify_class_nested_dependent_non_type_pack_die(
    const char* path, uint16_t architecture, const char* class_prefix,
    const char* expected_owner_name)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    unsigned found = 0u;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && strings != NULL);

    for (uint64_t die = 11u; die + 9u <= info->size; ++die) {
        uint32_t class_name_offset;
        const char* class_name;
        uint64_t cursor;
        uint32_t owner_type_offset;
        uint32_t owner_name_offset;
        const char* owner_name;
        uint32_t pack_name_offset;
        unsigned value_count = 0u;
        if (info->data[die] != 12u) continue;
        class_name_offset = read_u32(info->data, die + 1u);
        if (class_name_offset >= strings->size) continue;
        class_name = (const char*)strings->data + class_name_offset;
        if (strncmp(class_name, class_prefix, strlen(class_prefix)) != 0) {
            continue;
        }

        cursor = die + 9u;
        assert(cursor < info->size && info->data[cursor++] == 47u);
        assert(cursor + 8u <= info->size);
        owner_type_offset = read_u32(info->data, cursor + 4u);
        cursor += 8u;
        assert(owner_type_offset + 5u <= info->size &&
               info->data[owner_type_offset] == 12u);
        owner_name_offset = read_u32(info->data, owner_type_offset + 1u);
        assert(owner_name_offset < strings->size);
        owner_name = (const char*)strings->data + owner_name_offset;
        assert(strcmp(owner_name, expected_owner_name) == 0);

        assert(cursor < info->size && info->data[cursor++] == 50u);
        assert(cursor + 4u <= info->size);
        pack_name_offset = read_u32(info->data, cursor);
        cursor += 4u;
        assert(pack_name_offset < strings->size &&
               strcmp((const char*)strings->data + pack_name_offset,
                      "Values") == 0);
        while (cursor < info->size && info->data[cursor] == 52u) {
            uint32_t value_type_offset;
            uint32_t value_type_name_offset;
            int64_t value;
            assert(cursor + 5u <= info->size);
            ++cursor;
            value_type_offset = read_u32(info->data, cursor);
            cursor += 4u;
            assert(value_type_offset + 5u <= info->size &&
                   info->data[value_type_offset] == 5u);
            value_type_name_offset = read_u32(info->data,
                                              value_type_offset + 1u);
            assert(value_type_name_offset < strings->size &&
                   strcmp((const char*)strings->data +
                              value_type_name_offset,
                          "short") == 0);
            value = read_sleb(info->data, info->size, &cursor);
            assert((value_count == 0u && value == -4) ||
                   (value_count == 1u && value == 19));
            ++value_count;
        }
        assert(value_count == 2u && cursor < info->size &&
               info->data[cursor++] == 0u);
        ++found;
    }
    assert(found == 1u);
    objfile_free(object);
}

static void verify_class_nested_dependent_scalar_value_die(
    const char* path, uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    unsigned found = 0u;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && strings != NULL);

    for (uint64_t die = 11u; die + 9u <= info->size; ++die) {
        uint32_t class_name_offset;
        const char* class_name;
        uint64_t cursor;
        uint32_t name_offset;
        uint32_t owner_type_offset;
        uint32_t owner_name_offset;
        uint32_t value_type_offset;
        uint32_t value_type_name_offset;
        if (info->data[die] != 12u) continue;
        class_name_offset = read_u32(info->data, die + 1u);
        if (class_name_offset >= strings->size) continue;
        class_name = (const char*)strings->data + class_name_offset;
        if (strncmp(class_name, "DebugTemplateNestedScalarValue.",
                    sizeof("DebugTemplateNestedScalarValue.") - 1u) != 0) {
            continue;
        }
        cursor = die + 9u;
        assert(cursor < info->size && info->data[cursor++] == 47u);
        assert(cursor + 8u <= info->size);
        name_offset = read_u32(info->data, cursor);
        owner_type_offset = read_u32(info->data, cursor + 4u);
        cursor += 8u;
        assert(name_offset < strings->size &&
               strcmp((const char*)strings->data + name_offset,
                      "Owner") == 0);
        assert(owner_type_offset + 5u <= info->size &&
               info->data[owner_type_offset] == 12u);
        owner_name_offset = read_u32(info->data, owner_type_offset + 1u);
        assert(owner_name_offset < strings->size &&
               strcmp((const char*)strings->data + owner_name_offset,
                      "DebugTemplateNestedValueOwner") == 0);

        assert(cursor < info->size && info->data[cursor++] == 48u);
        assert(cursor + 8u <= info->size);
        name_offset = read_u32(info->data, cursor);
        value_type_offset = read_u32(info->data, cursor + 4u);
        cursor += 8u;
        assert(name_offset < strings->size &&
               strcmp((const char*)strings->data + name_offset,
                      "Value") == 0);
        assert(value_type_offset + 5u <= info->size &&
               info->data[value_type_offset] == 5u);
        value_type_name_offset = read_u32(info->data,
                                          value_type_offset + 1u);
        assert(value_type_name_offset < strings->size &&
               strcmp((const char*)strings->data +
                          value_type_name_offset,
                      "short") == 0);
        assert(read_sleb(info->data, info->size, &cursor) == -6);
        ++found;
    }
    assert(found == 1u);
    objfile_free(object);
}

static void verify_cxx_class_type_alias(const char* path,
                                        uint16_t architecture,
                                        const char* alias_name,
                                        const char* expected_type,
                                        uint8_t expected_access)
{
    static const uint8_t class_alias_abbrev[] = {
        46u, 0x16u, 0u, 0x03u, 0x0eu, 0x49u, 0x13u, 0x3au, 0x06u,
        0x3bu, 0x06u, 0x39u, 0x06u, 0x32u, 0x0bu, 0u, 0u
    };
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    uint64_t class_die;
    uint64_t cursor;
    bool found = false;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && abbrev != NULL && strings != NULL);
    assert(contains_sequence(abbrev->data, abbrev->size,
                             class_alias_abbrev,
                             sizeof(class_alias_abbrev)));
    class_die = find_structure_type_die(info, strings, "DebugMemberObject");
    assert(class_die != UINT64_MAX);
    cursor = class_die + 9u;
    while (cursor < info->size && info->data[cursor] != 0u) {
        uint8_t child = info->data[cursor++];
        if (child == 46u) {
            uint32_t name_offset;
            uint32_t type_offset;
            uint32_t file_index;
            uint32_t line;
            uint32_t column;
            uint32_t type_name_offset;
            assert(cursor + 21u <= info->size);
            name_offset = read_u32(info->data, cursor);
            type_offset = read_u32(info->data, cursor + 4u);
            file_index = read_u32(info->data, cursor + 8u);
            line = read_u32(info->data, cursor + 12u);
            column = read_u32(info->data, cursor + 16u);
            assert(name_offset < strings->size);
            if (name_offset < strings->size &&
                strcmp((const char*)strings->data + name_offset,
                       alias_name) == 0) {
                assert((uint64_t)type_offset + 7u <= info->size &&
                       info->data[type_offset] == 5u);
                type_name_offset = read_u32(info->data, type_offset + 1u);
                assert(type_name_offset < strings->size &&
                       strcmp((const char*)strings->data + type_name_offset,
                              expected_type) == 0);
                assert(info->data[type_offset + 5u] ==
                       (strcmp(expected_type, "unsigned short") == 0
                            ? 2u
                            : strcmp(expected_type,
                                     "unsigned long long") == 0 ? 8u : 4u));
                assert(file_index > 0u && line > 0u && column > 0u &&
                       info->data[cursor + 20u] == expected_access);
                found = true;
            }
            cursor += 21u;
            continue;
        }
        assert(child == 14u || child == 17u);
        (void)verify_skip_cxx_member_die(info, strings, &cursor, child);
    }
    assert(found);
    objfile_free(object);
}

static uint64_t find_function_die(const ObjSection* info,
                                  const ObjSection* strings,
                                  const char* function_name,
                                  uint64_t address_size);

static void verify_alias_scope_children(
    const ObjSection* info, const ObjSection* strings, uint64_t* cursor,
    uint64_t address_size, unsigned depth, int* local_alias_depth,
    int* nested_alias_depth, int* template_alias_depth)
{
    while (*cursor < info->size) {
        uint8_t abbreviation = info->data[(*cursor)++];
        if (abbreviation == 0u) return;
        if (abbreviation == 3u || abbreviation == 4u) {
            uint64_t expression_size;
            assert(*cursor + 20u <= info->size);
            *cursor += 20u; /* name, type, file, line, and column */
            expression_size = read_uleb(info->data, info->size, cursor);
            assert(expression_size <= info->size - *cursor);
            *cursor += expression_size;
        } else if (abbreviation == 39u) {
            assert(*cursor + 24u <= info->size);
            *cursor += 24u; /* variable attributes and loclist offset */
        } else if (abbreviation == 44u) {
            uint32_t name_offset;
            uint32_t type_offset;
            uint32_t file_index;
            uint32_t line;
            uint32_t column;
            uint32_t type_name_offset;
            assert(*cursor + 20u <= info->size);
            name_offset = read_u32(info->data, *cursor);
            type_offset = read_u32(info->data, *cursor + 4u);
            file_index = read_u32(info->data, *cursor + 8u);
            line = read_u32(info->data, *cursor + 12u);
            column = read_u32(info->data, *cursor + 16u);
            assert(name_offset < strings->size &&
                   (uint64_t)type_offset + 7u <= info->size &&
                   info->data[type_offset] == 5u);
            type_name_offset = read_u32(info->data, type_offset + 1u);
            assert(type_name_offset < strings->size && file_index > 0u &&
                   line > 0u && column > 0u);
            if (strcmp((const char*)strings->data + name_offset,
                       "LocalWord") == 0) {
                assert(strcmp((const char*)strings->data + type_name_offset,
                              "unsigned short") == 0 &&
                       info->data[type_offset + 5u] == 2u && line == 68u);
                *local_alias_depth = (int)depth;
            } else if (strcmp((const char*)strings->data + name_offset,
                              "NestedWord") == 0) {
                assert(strcmp((const char*)strings->data + type_name_offset,
                              "unsigned long long") == 0 &&
                       info->data[type_offset + 5u] == 8u && line == 71u);
                *nested_alias_depth = (int)depth;
            } else if (strcmp((const char*)strings->data + name_offset,
                              "TemplateWord") == 0) {
                assert(strcmp((const char*)strings->data + type_name_offset,
                              "unsigned short") == 0 &&
                       info->data[type_offset + 5u] == 2u && line == 80u);
                *template_alias_depth = (int)depth;
            }
            *cursor += 20u;
        } else if (abbreviation == 24u || abbreviation == 25u) {
            uint64_t header_size = abbreviation == 24u
                ? address_size + 16u : 16u;
            assert(header_size <= info->size - *cursor);
            *cursor += header_size;
            verify_alias_scope_children(
                info, strings, cursor, address_size, depth + 1u,
                local_alias_depth, nested_alias_depth, template_alias_depth);
        } else if (abbreviation == 47u) {
            assert(8u <= info->size - *cursor);
            *cursor += 8u;
        } else if (abbreviation == 48u || abbreviation == 49u) {
            assert(8u <= info->size - *cursor);
            *cursor += 8u;
            (void)(abbreviation == 48u
                ? read_sleb(info->data, info->size, cursor)
                : (int64_t)read_uleb(info->data, info->size, cursor));
        } else {
            assert(false && "unexpected function-scope DIE abbreviation");
        }
    }
    assert(false && "unterminated function-scope DIE children");
}

static void verify_local_type_alias_scope(const char* path,
                                          uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint64_t function_die;
    uint64_t cursor;
    uint64_t expression_size;
    int local_alias_depth = -1;
    int nested_alias_depth = -1;
    int template_alias_depth = -1;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && strings != NULL);
    function_die = find_function_die(info, strings,
                                     "debug_local_alias_entry",
                                     address_size);
    assert(function_die != UINT64_MAX);
    cursor = function_die + 1u + 4u + address_size + 4u + 4u + 4u + 4u +
             1u + 4u + 4u;
    expression_size = read_uleb(info->data, info->size, &cursor);
    assert(expression_size <= info->size - cursor);
    cursor += expression_size;
    assert(cursor + 2u <= info->size);
    cursor += 2u; /* inline and prototyped */
    verify_alias_scope_children(info, strings, &cursor, address_size, 0u,
                                &local_alias_depth, &nested_alias_depth,
                                &template_alias_depth);
    assert(local_alias_depth >= 0 && nested_alias_depth > local_alias_depth);
    function_die = find_function_die(info, strings,
                                     "debug_template_local_alias_entry",
                                     address_size);
    assert(function_die != UINT64_MAX);
    cursor = function_die + 1u + 4u + address_size + 4u + 4u + 4u + 4u +
             1u + 4u + 4u;
    expression_size = read_uleb(info->data, info->size, &cursor);
    assert(expression_size <= info->size - cursor);
    cursor += expression_size;
    assert(cursor + 2u <= info->size);
    cursor += 2u; /* inline and prototyped */
    verify_alias_scope_children(info, strings, &cursor, address_size, 0u,
                                &local_alias_depth, &nested_alias_depth,
                                &template_alias_depth);
    assert(template_alias_depth >= 0 && template_alias_depth <= 1);
    objfile_free(object);
}

static bool has_reference_type_die(const ObjSection* info,
                                   uint8_t abbreviation)
{
    if (!info || !info->data) return false;
    for (uint64_t offset = 11u; offset + 5u <= info->size; ++offset) {
        uint32_t referred_type_offset;
        if (info->data[offset] != abbreviation) continue;
        referred_type_offset = read_u32(info->data, offset + 1u);
        if (referred_type_offset < info->size &&
            info->data[referred_type_offset] == 5u) {
            return true;
        }
    }
    return false;
}

static void verify_cxx_reference_type_dies(const char* path,
                                           uint16_t architecture)
{
    static const uint8_t reference_abbrev[] = {
        37u, 0x10u, 0u, 0x49u, 0x13u, 0u, 0u
    };
    static const uint8_t rvalue_reference_abbrev[] = {
        38u, 0x42u, 0u, 0x49u, 0x13u, 0u, 0u
    };
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    assert(info != NULL && abbrev != NULL);
    assert(contains_sequence(abbrev->data, abbrev->size,
                             reference_abbrev, sizeof(reference_abbrev)));
    assert(contains_sequence(abbrev->data, abbrev->size,
                             rvalue_reference_abbrev,
                             sizeof(rvalue_reference_abbrev)));
    assert(has_reference_type_die(info, 37u));
    assert(has_reference_type_die(info, 38u));
    objfile_free(object);
}

static void verify_cxx_method_accessibility(const char* path,
                                            uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    bool found_private = false;
    bool found_protected = false;
    bool found_public = false;
    bool found_static_public = false;

    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && abbrev != NULL && strings != NULL);
    assert(contains_byte_pair(abbrev->data, abbrev->size, 0x32u, 0x0bu));

    for (uint64_t die = 11u; die < info->size; ++die) {
        uint8_t die_abbreviation = info->data[die];
        uint32_t name_offset;
        uint64_t cursor;
        uint64_t expression_size;
        uint32_t containing_type_offset;
        uint8_t accessibility;
        const char* name;

        if (die_abbreviation != 29u && die_abbreviation != 30u) continue;
        if (die + 1u + 4u > info->size) continue;
        name_offset = read_u32(info->data, die + 1u);
        if (name_offset >= strings->size) continue;
        name = (const char*)strings->data + name_offset;
        if (strcmp(name, "secret_value") != 0 &&
            strcmp(name, "protected_read") != 0 &&
            strcmp(name, "read") != 0 &&
            strcmp(name, "create_value") != 0) {
            continue;
        }

        cursor = die + 1u + 4u + address_size + 4u + 4u + 4u + 4u + 1u +
                 4u + 4u;
        if (cursor >= info->size) continue;
        expression_size = read_uleb(info->data, info->size, &cursor);
        if (expression_size > info->size - cursor) continue;
        cursor += expression_size;
        if (cursor >= info->size) continue;
        ++cursor; /* DW_AT_inline */
        if (die_abbreviation == 29u) {
            if (info->size - cursor < 4u) continue;
            cursor += 4u; /* DW_AT_object_pointer */
        }
        if (info->size - cursor < 5u) continue;
        containing_type_offset = read_u32(info->data, cursor);
        cursor += 4u;
        accessibility = info->data[cursor];
        assert(info->data[cursor + 1u] == 1u);
        assert(containing_type_offset < info->size);
        assert(info->data[containing_type_offset] == 12u);
        {
            uint32_t class_name_offset = read_u32(
                info->data, containing_type_offset + 1u);
            assert(class_name_offset < strings->size);
            assert(strcmp((const char*)strings->data + class_name_offset,
                          "DebugMemberObject") == 0);
        }
        if (strcmp(name, "secret_value") == 0) {
            assert(die_abbreviation == 29u && accessibility == 3u);
            found_private = true;
        } else if (strcmp(name, "protected_read") == 0) {
            assert(die_abbreviation == 29u && accessibility == 2u);
            found_protected = true;
        } else if (strcmp(name, "read") == 0) {
            assert(die_abbreviation == 29u && accessibility == 1u);
            found_public = true;
        } else {
            assert(die_abbreviation == 30u && accessibility == 1u);
            found_static_public = true;
        }
    }

    assert(found_private && found_protected && found_public &&
           found_static_public);
    objfile_free(object);
}

static bool find_lexical_block_local(const ObjSection* info,
                                     const ObjSection* strings,
                                     const char* variable_name,
                                     uint64_t address_size)
{
    if (!info || !strings || !variable_name) return false;
    for (uint64_t offset = 11u; offset + 5u < info->size; ++offset) {
        bool ranged;
        uint32_t name_offset;
        uint64_t range_offset;
        uint64_t range_end;
        uint64_t file_offset;
        uint64_t line_offset;
        uint64_t column_offset;
        uint64_t child_offset;
        ranged = info->data[offset] == 25u;
        if (!ranged && info->data[offset] != 24u) continue;
        range_offset = ranged ? offset + 1u : offset + 1u + address_size;
        file_offset = range_offset + 4u;
        line_offset = file_offset + 4u;
        column_offset = line_offset + 4u;
        child_offset = column_offset + 4u;
        range_end = child_offset + 5u;
        if (range_end > info->size ||
            (!ranged && read_u32(info->data, range_offset) == 0u) ||
            read_u32(info->data, file_offset) == 0u ||
            read_u32(info->data, line_offset) == 0u ||
            read_u32(info->data, column_offset) == 0u ||
            (info->data[child_offset] != 4u &&
             info->data[child_offset] != 39u)) {
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

static bool find_relocation_addend(const ObjSection* section,
                                   uint64_t offset, int64_t* addend)
{
    if (!section || !addend) return false;
    for (const ObjReloc* relocation = section->relocs; relocation;
         relocation = relocation->next) {
        if (relocation->offset != offset) continue;
        *addend = relocation->addend;
        return true;
    }
    return false;
}

static bool verify_location_starts_after_simple_scope(
    const ObjSection* info, const ObjSection* strings,
    const ObjSection* locations, const char* variable_name,
    uint64_t address_size)
{
    int64_t scope_start;
    int64_t location_start;
    bool found_scope = false;
    bool found_location = false;

    if (!info || !strings || !locations || !variable_name) return false;
    for (uint64_t offset = 11u; offset + 5u < info->size; ++offset) {
        uint64_t range_offset;
        uint64_t child_offset;
        uint32_t name_offset;
        if (info->data[offset] != 24u) continue;
        range_offset = offset + 1u + address_size;
        child_offset = range_offset + 4u + 4u + 4u + 4u;
        if (child_offset + 5u > info->size ||
            (info->data[child_offset] != 4u &&
             info->data[child_offset] != 39u)) {
            continue;
        }
        name_offset = read_u32(info->data, child_offset + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset,
                   variable_name) != 0) {
            continue;
        }
        if (!find_relocation_addend(info, offset + 1u, &scope_start)) {
            return false;
        }
        found_scope = true;
        break;
    }
    for (uint64_t die = 11u; die + 25u <= info->size; ++die) {
        uint32_t name_offset;
        uint32_t list_offset;
        if (info->data[die] != 39u) continue;
        name_offset = read_u32(info->data, die + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset,
                   variable_name) != 0) {
            continue;
        }
        list_offset = read_u32(info->data, die + 21u);
        if (list_offset >= locations->size ||
            !find_relocation_addend(locations, list_offset,
                                    &location_start)) {
            return false;
        }
        found_location = true;
        break;
    }
    return found_scope && found_location && location_start > scope_start;
}

static bool verify_location_starts_after_ranges_scope(
    const ObjSection* info, const ObjSection* strings,
    const ObjSection* locations, const ObjSection* ranges,
    const char* variable_name, uint64_t address_size)
{
    int64_t scope_start = 0;
    int64_t location_start;
    bool found_scope = false;
    bool found_location = false;
    if (!info || !strings || !locations || !ranges || !variable_name) {
        return false;
    }
    for (uint64_t offset = 11u; offset + 17u <= info->size; ++offset) {
        uint32_t name_offset;
        uint32_t list_offset;
        uint64_t child_offset;
        uint64_t cursor;
        bool saw_range = false;
        if (info->data[offset] != 25u) continue;
        child_offset = offset + 17u;
        if (child_offset + 5u > info->size ||
            (info->data[child_offset] != 4u &&
             info->data[child_offset] != 39u)) {
            continue;
        }
        name_offset = read_u32(info->data, child_offset + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset,
                   variable_name) != 0) {
            continue;
        }
        list_offset = read_u32(info->data, offset + 1u);
        cursor = list_offset;
        while (cursor <= ranges->size &&
               address_size * 2u <= ranges->size - cursor) {
            int64_t start;
            int64_t end;
            bool has_start = find_relocation_addend(ranges, cursor, &start);
            bool has_end = find_relocation_addend(
                ranges, cursor + address_size, &end);
            if (!has_start && !has_end) break;
            if (!has_start || !has_end || end <= start) return false;
            if (!saw_range || start < scope_start) scope_start = start;
            saw_range = true;
            cursor += address_size * 2u;
        }
        if (!saw_range) continue;
        found_scope = true;
        break;
    }
    for (uint64_t die = 11u; die + 25u <= info->size; ++die) {
        uint32_t name_offset;
        uint32_t list_offset;
        if (info->data[die] != 39u) continue;
        name_offset = read_u32(info->data, die + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset,
                   variable_name) != 0) {
            continue;
        }
        list_offset = read_u32(info->data, die + 21u);
        if (list_offset >= locations->size ||
            !find_relocation_addend(locations, list_offset,
                                    &location_start)) {
            return false;
        }
        found_location = true;
        break;
    }
    return found_scope && found_location && location_start > scope_start;
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

static uint64_t verify_enum_type_underlying(const ObjSection* info,
                                            const ObjSection* abbrev,
                                            const ObjSection* strings,
                                            const char* type_name,
                                            const char* underlying_name,
                                            uint8_t expected_size,
                                            uint8_t expected_encoding,
                                            bool scoped)
{
    static const uint8_t enum_type_abbrev[] = {
        15u, 0x04u, 1u, 0x03u, 0x0eu, 0x49u, 0x13u,
        0x0bu, 0x0bu, 0u, 0u
    };
    static const uint8_t scoped_enum_type_abbrev[] = {
        36u, 0x04u, 1u, 0x03u, 0x0eu, 0x49u, 0x13u,
        0x0bu, 0x0bu, 0x6du, 0x0cu, 0u, 0u
    };
    const uint8_t* expected_abbrev = scoped ? scoped_enum_type_abbrev
                                            : enum_type_abbrev;
    size_t expected_abbrev_size = scoped ? sizeof(scoped_enum_type_abbrev)
                                         : sizeof(enum_type_abbrev);
    assert(info != NULL && abbrev != NULL && strings != NULL);
    assert(contains_sequence(abbrev->data, abbrev->size, expected_abbrev,
                             expected_abbrev_size));
    for (uint64_t offset = 0u;
         offset + (scoped ? 11u : 10u) <= info->size; ++offset) {
        uint32_t name_offset;
        uint64_t underlying_offset;
        uint32_t underlying_name_offset;
        if (info->data[offset] != (scoped ? 36u : 15u)) continue;
        name_offset = read_u32(info->data, offset + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset, type_name) != 0) {
            continue;
        }
        assert(info->data[offset + 9u] == expected_size);
        if (scoped) assert(info->data[offset + 10u] == 1u);
        underlying_offset = read_u32(info->data, offset + 5u);
        assert(underlying_offset != 0u &&
               underlying_offset + 7u <= info->size);
        assert(info->data[underlying_offset] == 5u); /* DW_TAG_base_type */
        underlying_name_offset = read_u32(info->data,
                                          underlying_offset + 1u);
        assert(underlying_name_offset < strings->size);
        if (strcmp((const char*)strings->data + underlying_name_offset,
                   underlying_name) != 0) {
            fprintf(stderr, "enum %s references underlying type '%s', "
                            "expected '%s' (DIE offset %llu)\n",
                    type_name,
                    (const char*)strings->data + underlying_name_offset,
                    underlying_name,
                    (unsigned long long)underlying_offset);
        }
        assert(strcmp((const char*)strings->data + underlying_name_offset,
                      underlying_name) == 0);
        assert(info->data[underlying_offset + 5u] == expected_size);
        assert(info->data[underlying_offset + 6u] == expected_encoding);
        return offset;
    }
    if (contains_bytes(strings->data, strings->size, type_name)) {
        fprintf(stderr, "DWARF enum DIE %s was not found (scoped=%d)\n",
                type_name, scoped ? 1 : 0);
        for (uint64_t offset = 0u; offset + 5u <= info->size; ++offset) {
            uint32_t name_offset;
            if (info->data[offset] != 15u && info->data[offset] != 36u) {
                continue;
            }
            name_offset = read_u32(info->data, offset + 1u);
            if (name_offset < strings->size) {
                fprintf(stderr, "enum DIE candidate id=%u offset=%llu name=%s\n",
                        info->data[offset], (unsigned long long)offset,
                        (const char*)strings->data + name_offset);
            }
        }
    }
    assert(0 && "enumeration DIE with expected underlying type not found");
    return 0u;
}

static void verify_enum_underlying_dwarf(const char* path,
                                         uint16_t architecture,
                                         const char* type_name,
                                         const char* underlying_name,
                                         uint8_t expected_size,
                                         uint8_t expected_encoding,
                                         bool scoped)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && abbrev != NULL && strings != NULL);
    (void)verify_enum_type_underlying(info, abbrev, strings, type_name,
                                      underlying_name, expected_size,
                                      expected_encoding, scoped);
    objfile_free(object);
}

static void verify_unsigned_enum_dwarf(const char* path,
                                       uint16_t architecture,
                                       const char* type_name,
                                       const char* underlying_name,
                                       const char* enumerator_name,
                                       bool scoped,
                                       uint8_t expected_size,
                                       uint64_t expected_value)
{
    static const uint8_t unsigned_enumerator_abbrev[] = {
        35u, 0x28u, 0u, 0x03u, 0x0eu, 0x1cu, 0x0fu, 0u, 0u
    };
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    uint64_t enum_offset;
    uint64_t child;
    uint32_t enumerator_name_offset;
    uint64_t value;

    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && abbrev != NULL && strings != NULL);
    assert(contains_bytes(strings->data, strings->size, type_name));
    assert(contains_bytes(strings->data, strings->size, enumerator_name));
    assert(contains_sequence(abbrev->data, abbrev->size,
                             unsigned_enumerator_abbrev,
                             sizeof(unsigned_enumerator_abbrev)));
    enum_offset = verify_enum_type_underlying(
        info, abbrev, strings, type_name, underlying_name, expected_size,
        0x07u, scoped); /* DW_ATE_unsigned on the referenced base type */
    child = enum_offset + (scoped ? 11u : 10u);
    assert(child < info->size && info->data[child++] == 35u);
    assert(child + 4u <= info->size);
    enumerator_name_offset = read_u32(info->data, child);
    child += 4u;
    assert(enumerator_name_offset < strings->size);
    assert(strcmp((const char*)strings->data + enumerator_name_offset,
                  enumerator_name) == 0);
    value = read_uleb(info->data, info->size, &child);
    assert(value == expected_value);
    assert(child < info->size && info->data[child] == 0u);
    objfile_free(object);
}

static void verify_scoped_enum_dwarf(const char* path,
                                     uint16_t architecture,
                                     const char* type_name,
                                     const char* underlying_name,
                                     uint8_t underlying_size,
                                     uint8_t underlying_encoding,
                                     uint8_t enumerator_abbrev,
                                     const char* enumerator_name,
                                     int64_t enumerator_value)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    uint64_t enum_offset;
    uint64_t child;
    uint32_t enumerator_name_offset;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && abbrev != NULL && strings != NULL);
    enum_offset = verify_enum_type_underlying(
        info, abbrev, strings, type_name, underlying_name, underlying_size,
        underlying_encoding, true);
    child = enum_offset + 11u;
    assert(child < info->size &&
           info->data[child++] == enumerator_abbrev);
    assert(child + 4u <= info->size);
    enumerator_name_offset = read_u32(info->data, child);
    child += 4u;
    assert(enumerator_name_offset < strings->size);
    assert(strcmp((const char*)strings->data + enumerator_name_offset,
                  enumerator_name) == 0);
    if (enumerator_abbrev == 35u) {
        assert(enumerator_value >= 0);
        assert(read_uleb(info->data, info->size, &child) ==
               (uint64_t)enumerator_value);
    } else {
        assert(enumerator_abbrev == 16u);
        assert(read_sleb(info->data, info->size, &child) ==
               enumerator_value);
    }
    assert(child < info->size && info->data[child] == 0u);
    objfile_free(object);
}

static int64_t read_sleb(const uint8_t* data, uint64_t size,
                         uint64_t* offset)
{
    uint64_t value = 0u;
    unsigned shift = 0u;
    uint8_t byte = 0u;
    assert(data != NULL && offset != NULL);
    while (*offset < size && shift < 64u) {
        byte = data[(*offset)++];
        value |= (uint64_t)(byte & 0x7fu) << shift;
        shift += 7u;
        if ((byte & 0x80u) == 0u) {
            if ((byte & 0x40u) != 0u && shift < 64u) {
                value |= UINT64_MAX << shift;
            }
            return (int64_t)value;
        }
    }
    assert(0 && "invalid SLEB128");
    return 0;
}

static void verify_signed_enum_dwarf(const char* path,
                                    uint16_t architecture,
                                    const char* type_name,
                                    const char* underlying_name,
                                    const char* enumerator_name,
                                    uint8_t expected_size,
                                    uint8_t expected_encoding,
                                    int64_t expected_value)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    uint64_t enum_offset;
    uint64_t child;
    uint32_t name_offset;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && abbrev != NULL && strings != NULL);
    enum_offset = verify_enum_type_underlying(
        info, abbrev, strings, type_name, underlying_name, expected_size,
        expected_encoding, false);
    child = enum_offset + 10u;
    assert(child < info->size && info->data[child++] == 16u);
    assert(child + 4u <= info->size);
    name_offset = read_u32(info->data, child);
    child += 4u;
    assert(name_offset < strings->size);
    assert(strcmp((const char*)strings->data + name_offset,
                  enumerator_name) == 0);
    assert(read_sleb(info->data, info->size, &child) == expected_value);
    assert(child < info->size && info->data[child] == 16u);
    objfile_free(object);
}

static bool try_read_uleb(const uint8_t* data, uint64_t size,
                          uint64_t* offset, uint64_t* result)
{
    uint64_t value = 0u;
    unsigned shift = 0u;
    if (!data || !offset || !result) return false;
    while (*offset < size && shift < 64u) {
        uint8_t byte = data[(*offset)++];
        value |= (uint64_t)(byte & 0x7fu) << shift;
        if ((byte & 0x80u) == 0u) {
            *result = value;
            return true;
        }
        shift += 7u;
    }
    return false;
}

static bool try_read_sleb(const uint8_t* data, uint64_t size,
                          uint64_t* offset, int64_t* result)
{
    uint64_t value = 0u;
    unsigned shift = 0u;
    uint8_t byte = 0u;
    if (!data || !offset || !result) return false;
    while (*offset < size && shift < 64u) {
        byte = data[(*offset)++];
        value |= (uint64_t)(byte & 0x7fu) << shift;
        shift += 7u;
        if ((byte & 0x80u) == 0u) {
            if ((byte & 0x40u) != 0u && shift < 64u) {
                value |= UINT64_MAX << shift;
            }
            *result = (int64_t)value;
            return true;
        }
    }
    return false;
}

static bool find_variable_location(const ObjSection* info,
                                  const ObjSection* strings,
                                  const ObjSection* locations,
                                  const char* variable_name,
                                  uint16_t architecture,
                                  bool* has_location,
                                  bool* is_location_list,
                                  int64_t* frame_offset)
{
    uint8_t frame_register = architecture == ARCH_X64 ? 0x76u : 0x75u;
    if (!info || !strings || !variable_name || !has_location ||
        !is_location_list || !frame_offset) return false;
    for (uint64_t die = 11u; die + 21u <= info->size; ++die) {
        uint8_t abbreviation = info->data[die];
        uint32_t name_offset;
        bool has_expression;
        uint64_t expression_cursor;
        uint64_t expression_size;
        uint64_t expression_end;
        if (abbreviation != 3u && abbreviation != 4u &&
            abbreviation != 27u && abbreviation != 31u &&
            abbreviation != 32u && abbreviation != 33u &&
            abbreviation != 39u && abbreviation != 54u &&
            abbreviation != 55u && abbreviation != 56u) continue;
        name_offset = read_u32(info->data, die + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset,
                   variable_name) != 0) continue;
        has_expression = abbreviation == 3u || abbreviation == 4u ||
            abbreviation == 27u;
        *is_location_list = abbreviation == 39u;
        *has_location = has_expression || *is_location_list;
        *frame_offset = 0;
        if (abbreviation == 54u || abbreviation == 55u ||
            abbreviation == 56u) return true;
        if (*is_location_list) {
            uint32_t list_offset;
            uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
            uint64_t cursor;
            bool saw_entry = false;
            bool has_previous_range = false;
            int64_t previous_range_end = 0;
            if (!locations || die + 25u > info->size) return false;
            list_offset = read_u32(info->data, die + 21u);
            if (list_offset > locations->size) return false;
            cursor = list_offset;
            for (;;) {
                const ObjReloc* start_reloc = NULL;
                const ObjReloc* end_reloc = NULL;
                uint16_t expression_length;
                uint64_t expression_limit;
                uint64_t expression_cursor;
                int64_t current_frame_offset;
                if (cursor > locations->size ||
                    address_size * 2u > locations->size - cursor) {
                    return false;
                }
                for (const ObjReloc* reloc = locations->relocs; reloc;
                     reloc = reloc->next) {
                    if (reloc->offset == cursor) {
                        assert(reloc->type == (architecture == ARCH_X64
                                                   ? RELOC_ABS64
                                                   : RELOC_ABS32U));
                        start_reloc = reloc;
                    }
                    if (reloc->offset == cursor + address_size) {
                        assert(reloc->type == (architecture == ARCH_X64
                                                   ? RELOC_ABS64
                                                   : RELOC_ABS32U));
                        end_reloc = reloc;
                    }
                }
                if (!start_reloc && !end_reloc) {
                    if (!saw_entry) {
                        fprintf(stderr,
                                "empty DWARF location list for '%s'\n",
                                variable_name);
                    }
                    assert(saw_entry);
                    for (uint64_t byte = cursor;
                         byte < cursor + address_size * 2u; ++byte) {
                        assert(locations->data[byte] == 0u);
                    }
                    return true;
                }
                assert(start_reloc && end_reloc &&
                       start_reloc->addend < end_reloc->addend);
                assert(!has_previous_range ||
                       start_reloc->addend >= previous_range_end);
                previous_range_end = end_reloc->addend;
                has_previous_range = true;
                cursor += address_size * 2u;
                if (cursor + 2u > locations->size) return false;
                expression_length = (uint16_t)locations->data[cursor] |
                    (uint16_t)((uint16_t)locations->data[cursor + 1u] << 8);
                cursor += 2u;
                if (expression_length < 2u ||
                    expression_length > locations->size - cursor) {
                    return false;
                }
                expression_cursor = cursor;
                expression_limit = cursor + expression_length;
                assert(locations->data[expression_cursor] == frame_register);
                ++expression_cursor;
                if (!try_read_sleb(locations->data, expression_limit,
                                   &expression_cursor,
                                   &current_frame_offset)) {
                    return false;
                }
                assert(expression_cursor == expression_limit ||
                       (expression_cursor + 1u == expression_limit &&
                        locations->data[expression_cursor] == 0x06u) ||
                       strcmp(variable_name, "aligned") == 0);
                if (!saw_entry) *frame_offset = current_frame_offset;
                saw_entry = true;
                cursor = expression_limit;
            }
        }
        if (!has_expression) return true;
        expression_cursor = die + 1u + 20u;
        expression_size = read_uleb(
            info->data, info->size, &expression_cursor);
        if (expression_size < 2u ||
            expression_size > info->size - expression_cursor) {
            continue;
        }
        expression_end = expression_cursor + expression_size;
        if (info->data[expression_cursor] != frame_register) continue;
        expression_cursor++;
        if (!try_read_sleb(info->data, expression_end, &expression_cursor,
                           frame_offset) ||
            (expression_cursor != expression_end &&
             !(expression_cursor + 1u == expression_end &&
               info->data[expression_cursor] == 0x06u) &&
             strcmp(variable_name, "aligned") != 0)) {
            continue;
        }
        return true;
    }
    return false;
}

static bool find_variable_const_value(const ObjSection* info,
                                      const ObjSection* strings,
                                      const char* variable_name,
                                      int64_t* value)
{
    if (!info || !strings || !variable_name || !value) return false;
    for (uint64_t die = 11u; die + 21u <= info->size; ++die) {
        uint32_t name_offset;
        uint64_t value_offset;
        if (info->data[die] != 54u) continue;
        name_offset = read_u32(info->data, die + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset,
                   variable_name) != 0) continue;
        value_offset = die + 21u;
        *value = read_sleb(info->data, info->size, &value_offset);
        return true;
    }
    return false;
}

static void assert_variable_has_no_location(
    const ObjSection* info, const ObjSection* strings,
    const ObjSection* locations, const char* variable_name,
    uint16_t architecture)
{
    bool has_location = false;
    bool is_location_list = false;
    int64_t frame_offset = 0;
    assert(find_variable_location(info, strings, locations, variable_name,
                                  architecture, &has_location,
                                  &is_location_list, &frame_offset));
    assert(!has_location && !is_location_list);
}

static bool find_variable_unsigned_const_value(
    const ObjSection* info, const ObjSection* strings,
    const char* variable_name, uint64_t* value)
{
    if (!info || !strings || !variable_name || !value) return false;
    for (uint64_t die = 11u; die + 21u <= info->size; ++die) {
        uint32_t name_offset;
        uint64_t value_offset;
        if (info->data[die] != 55u) continue;
        name_offset = read_u32(info->data, die + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset,
                   variable_name) != 0) continue;
        value_offset = die + 21u;
        *value = read_uleb(info->data, info->size, &value_offset);
        return true;
    }
    return false;
}

static bool find_variable_float_const_bits(
    const ObjSection* info, const ObjSection* strings,
    const char* variable_name, uint8_t* byte_size, uint64_t* bits)
{
    if (!info || !strings || !variable_name || !byte_size || !bits) {
        return false;
    }
    for (uint64_t die = 11u; die + 22u <= info->size; ++die) {
        uint32_t name_offset;
        uint64_t cursor;
        uint8_t size;
        uint64_t value = 0u;
        if (info->data[die] != 56u) continue;
        name_offset = read_u32(info->data, die + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset,
                   variable_name) != 0) continue;
        cursor = die + 21u;
        size = info->data[cursor++];
        if ((size != 4u && size != 8u) || cursor + size > info->size) {
            return false;
        }
        for (uint8_t byte_index = 0u; byte_index < size; ++byte_index) {
            value |= (uint64_t)info->data[cursor + byte_index] <<
                     (byte_index * 8u);
        }
        *byte_size = size;
        *bits = value;
        return true;
    }
    return false;
}

static bool find_location_list_bounds(
    const ObjSection* info, const ObjSection* strings,
    const ObjSection* locations, const char* variable_name,
    const char* function_symbol, uint16_t architecture,
    int64_t* range_start, int64_t* range_end)
{
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    if (!info || !strings || !locations || !variable_name ||
        !function_symbol || !range_start || !range_end) return false;
    for (uint64_t die = 11u; die + 25u <= info->size; ++die) {
        uint32_t name_offset;
        uint32_t list_offset;
        uint64_t cursor;
        int64_t minimum_start = 0;
        int64_t maximum_end = 0;
        bool saw_entry = false;
        if (info->data[die] != 39u) continue;
        name_offset = read_u32(info->data, die + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset,
                   variable_name) != 0) continue;
        list_offset = read_u32(info->data, die + 21u);
        if (list_offset >= locations->size) return false;
        cursor = list_offset;
        for (;;) {
            const ObjReloc* start_reloc = NULL;
            const ObjReloc* end_reloc = NULL;
            uint16_t expression_size;
            for (const ObjReloc* reloc = locations->relocs; reloc;
                 reloc = reloc->next) {
                if (reloc->offset == cursor) start_reloc = reloc;
                if (reloc->offset == cursor + address_size) {
                    end_reloc = reloc;
                }
            }
            if (!start_reloc && !end_reloc) {
                if (!saw_entry) return false;
                *range_start = minimum_start;
                *range_end = maximum_end;
                return true;
            }
            if (!start_reloc || !end_reloc ||
                !start_reloc->symbol_name || !end_reloc->symbol_name ||
                strcmp(start_reloc->symbol_name, function_symbol) != 0 ||
                strcmp(end_reloc->symbol_name, function_symbol) != 0 ||
                cursor > locations->size ||
                address_size * 2u + 2u > locations->size - cursor) {
                return false;
            }
            if (end_reloc->addend <= start_reloc->addend) return false;
            if (!saw_entry || start_reloc->addend < minimum_start) {
                minimum_start = start_reloc->addend;
            }
            if (!saw_entry || end_reloc->addend > maximum_end) {
                maximum_end = end_reloc->addend;
            }
            saw_entry = true;
            cursor += address_size * 2u;
            expression_size = (uint16_t)locations->data[cursor] |
                (uint16_t)((uint16_t)locations->data[cursor + 1u] << 8);
            cursor += 2u;
            if (expression_size > locations->size - cursor) return false;
            cursor += expression_size;
        }
    }
    return false;
}

static void verify_cxx_for_initializer_scope(const char* path,
                                             uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    ObjSection* locations;
    int64_t loop_scope_start;
    int64_t loop_scope_end;
    int64_t outer_scope_start;
    int64_t outer_scope_end;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    locations = objfile_get_section(object, ".debug_loc");
    assert(info != NULL && strings != NULL && locations != NULL);
    assert(find_location_list_bounds(
        info, strings, locations, "loop_index",
        "debug_cxx_for_initializer_scope", architecture,
        &loop_scope_start, &loop_scope_end));
    assert(find_location_list_bounds(
        info, strings, locations, "outer_value",
        "debug_cxx_for_initializer_scope", architecture,
        &outer_scope_start, &outer_scope_end));
    assert(loop_scope_start < loop_scope_end);
    assert(outer_scope_start < loop_scope_start);
    assert(loop_scope_end < outer_scope_end);
    objfile_free(object);
}

static void verify_multiple_return_frame_fde(const ObjSection* frame,
                                             const char* function_symbol,
                                             uint16_t architecture);
static void verify_saved_callee_register_rules(
    const ObjSection* frame, const char* function_symbol,
    uint16_t architecture);

static void verify_optimized_verified_debug_object(
    const char* path, uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    ObjSection* frame;
    ObjSection* locations;
    ObjSection* ranges;
    bool has_location;
    bool is_location_list;
    int64_t value_offset;
    int64_t local_offset;
    int64_t nested_offset;
    int64_t aligned_offset;
    int64_t branch_offset;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    frame = objfile_get_section(object, ".debug_frame");
    locations = objfile_get_section(object, ".debug_loc");
    ranges = objfile_get_section(object, ".debug_ranges");
    assert(info != NULL && strings != NULL && frame != NULL &&
           locations != NULL);
    verify_multiple_return_frame_fde(
        frame, "tests/verified_backend_debug.c::verified_debug_multi_return",
        architecture);
    verify_saved_callee_register_rules(
        frame, "verified_debug_preserved_registers", architecture);
    assert(find_variable_location(info, strings, locations, "value",
                                  architecture, &has_location,
                                  &is_location_list, &value_offset));
    assert(has_location && value_offset < 0 && value_offset % 4 == 0);
    assert(find_variable_location(info, strings, locations, "local",
                                  architecture, &has_location,
                                  &is_location_list, &local_offset));
    assert(has_location && local_offset < 0 && local_offset % 4 == 0 &&
           local_offset != value_offset);
    assert(find_variable_location(info, strings, locations, "nested",
                                  architecture, &has_location,
                                  &is_location_list, &nested_offset));
    assert(has_location && nested_offset < 0 && nested_offset % 4 == 0 &&
           nested_offset != value_offset && nested_offset != local_offset);
    assert(find_variable_location(info, strings, locations, "aligned",
                                  architecture, &has_location,
                                  &is_location_list, &aligned_offset));
    assert(has_location && aligned_offset < 0 && is_location_list);
    assert(find_variable_location(info, strings, locations, "branch_value",
                                  architecture, &has_location,
                                  &is_location_list, &branch_offset));
    assert(has_location && branch_offset < 0 && is_location_list);
    assert(verify_location_starts_after_ranges_scope(
        info, strings, locations, ranges, "no_code_local",
        architecture == ARCH_X64 ? 8u : 4u));
    assert(is_location_list);
    assert_variable_has_no_location(info, strings, locations,
                                    "no_code_unlocated", architecture);
    assert(find_variable_location(
        info, strings, locations, "optimized_constant", architecture,
        &has_location, &is_location_list, &branch_offset));
    assert(!has_location && !is_location_list);
    assert(find_variable_location(
        info, strings, locations, "optimized_negative", architecture,
        &has_location, &is_location_list, &branch_offset));
    assert(!has_location && !is_location_list);
    assert(find_variable_location(
        info, strings, locations, "optimized_unsigned_high", architecture,
        &has_location, &is_location_list, &branch_offset));
    assert(!has_location && !is_location_list);
    assert(find_variable_location(
        info, strings, locations, "optimized_unsigned_narrow", architecture,
        &has_location, &is_location_list, &branch_offset));
    assert(!has_location && !is_location_list);
    assert(find_variable_location(
        info, strings, locations, "optimized_float", architecture,
        &has_location, &is_location_list, &branch_offset));
    assert(!has_location && !is_location_list);
    assert(find_variable_location(
        info, strings, locations, "optimized_double", architecture,
        &has_location, &is_location_list, &branch_offset));
    assert(!has_location && !is_location_list);
    assert(find_variable_location(
        info, strings, locations, "optimized_enum", architecture,
        &has_location, &is_location_list, &branch_offset));
    assert(!has_location && !is_location_list);
    {
        int64_t constant_value;
        int64_t negative_value;
        int64_t enum_value;
        uint64_t unsigned_high_value;
        uint64_t unsigned_narrow_value;
        uint8_t float_size;
        uint8_t double_size;
        uint64_t float_bits;
        uint64_t double_bits;
        assert(find_variable_const_value(
            info, strings, "optimized_constant", &constant_value));
        assert(find_variable_const_value(
            info, strings, "optimized_negative", &negative_value));
        assert(find_variable_const_value(
            info, strings, "optimized_enum", &enum_value));
        assert(enum_value == -19);
        assert(constant_value == 42 && negative_value == -23);
        assert(find_variable_unsigned_const_value(
            info, strings, "optimized_unsigned_high", &unsigned_high_value));
        assert(unsigned_high_value == UINT64_MAX);
        assert(find_variable_unsigned_const_value(
            info, strings, "optimized_unsigned_narrow",
            &unsigned_narrow_value));
        assert(unsigned_narrow_value == UINT32_MAX);
        assert(find_variable_float_const_bits(
            info, strings, "optimized_float", &float_size, &float_bits));
        assert(float_size == 4u && float_bits == UINT32_C(0x3fa00000));
        assert(find_variable_float_const_bits(
            info, strings, "optimized_double", &double_size, &double_bits));
        assert(double_size == 8u &&
               double_bits == UINT64_C(0xc00c000000000000));
    }
    {
        int64_t branch_start;
        int64_t branch_end;
        int64_t result_start;
        int64_t result_end;
        int64_t loop_start;
        int64_t loop_end;
        int64_t outer_start;
        int64_t outer_end;
        assert(find_location_list_bounds(
            info, strings, locations, "branch_value",
            "verified_debug_branch_scope", architecture,
            &branch_start, &branch_end));
        assert(find_location_list_bounds(
            info, strings, locations, "result",
            "verified_debug_branch_scope", architecture,
            &result_start, &result_end));
        assert(result_start < branch_start && branch_start < branch_end);
        assert(branch_end < result_end);
        assert(find_location_list_bounds(
            info, strings, locations, "verified_loop_index",
            "verified_debug_for_scope", architecture,
            &loop_start, &loop_end));
        assert(find_location_list_bounds(
            info, strings, locations, "verified_outer_value",
            "verified_debug_for_scope", architecture,
            &outer_start, &outer_end));
        assert(loop_start < loop_end && outer_start < outer_end);
        assert(loop_end < outer_end);
    }
    verify_overaligned_location_mask(locations, architecture);
    objfile_free(object);
}

static void verify_verified_cpp_inline_debug(
    const char* path, uint16_t architecture);

static void verify_optimized_cxx_verified_debug_object(
    const char* path, uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    ObjSection* frame;
    ObjSection* locations;
    bool has_location;
    bool is_location_list;
    int64_t value_offset;
    int64_t local_offset;
    int64_t nested_offset;
    int64_t aligned_offset;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    frame = objfile_get_section(object, ".debug_frame");
    locations = objfile_get_section(object, ".debug_loc");
    assert(info != NULL && strings != NULL && frame != NULL &&
           locations != NULL);
    assert(find_variable_location(info, strings, locations, "value",
                                  architecture, &has_location,
                                  &is_location_list, &value_offset));
    assert(has_location && value_offset < 0 && value_offset % 4 == 0);
    assert(find_variable_location(info, strings, locations, "local",
                                  architecture, &has_location,
                                  &is_location_list, &local_offset));
    assert(has_location && local_offset < 0 && local_offset % 4 == 0 &&
           local_offset != value_offset);
    assert(find_variable_location(info, strings, locations, "nested",
                                  architecture, &has_location,
                                  &is_location_list, &nested_offset));
    assert(has_location && nested_offset < 0 && nested_offset % 4 == 0 &&
           nested_offset != value_offset && nested_offset != local_offset);
    assert(find_variable_location(info, strings, locations, "aligned",
                                  architecture, &has_location,
                                  &is_location_list, &aligned_offset));
    assert(has_location && aligned_offset < 0 && is_location_list);
    {
        int64_t loop_start;
        int64_t loop_end;
        int64_t outer_start;
        int64_t outer_end;
        assert(find_location_list_bounds(
            info, strings, locations, "verified_cpp_loop_index",
            "verified_cpp_debug_for_scope", architecture,
            &loop_start, &loop_end));
        assert(find_location_list_bounds(
            info, strings, locations, "verified_cpp_outer_value",
            "verified_cpp_debug_for_scope", architecture,
            &outer_start, &outer_end));
        assert(loop_start < loop_end && outer_start < outer_end);
        assert(loop_end < outer_end);
    }
    verify_overaligned_location_mask(locations, architecture);
    assert(is_location_list);
    {
        int64_t constant_value;
        bool has_constant_value;
        assert(find_variable_location(
            info, strings, locations, "verified_cpp_optimized_constant",
            architecture, &has_location, &is_location_list, &aligned_offset));
        has_constant_value = find_variable_const_value(
            info, strings, "verified_cpp_optimized_constant", &constant_value);
        if (has_location) {
            assert(!has_constant_value);
        } else {
            assert(!is_location_list && has_constant_value &&
                   constant_value == 31);
        }
    }
    verify_verified_cpp_inline_debug(path, architecture);
    objfile_free(object);
}

static void verify_vla_variable_location(const ObjSection* info,
                                         const ObjSection* strings,
                                         const ObjSection* locations,
                                         uint16_t architecture)
{
    bool found = false;
    uint8_t frame_register = architecture == ARCH_X64 ? 0x76u : 0x75u;
    assert(info != NULL && strings != NULL);
    for (uint64_t die = 11u; die + 1u + 20u < info->size; ++die) {
        uint32_t name_offset;
        uint64_t expression_offset;
        uint64_t expression_size;
        uint64_t displacement_offset;
        if (info->data[die] != 4u && info->data[die] != 39u) continue;
        name_offset = read_u32(info->data, die + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset,
                   "debug_vla_values") != 0) {
            continue;
        }
        if (info->data[die] == 39u) {
            uint32_t list_offset = read_u32(info->data, die + 21u);
            uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
            uint64_t cursor = (uint64_t)list_offset + address_size * 2u;
            uint16_t list_expression_size;
            assert(locations != NULL && list_offset < locations->size);
            assert(cursor + 2u <= locations->size);
            list_expression_size = (uint16_t)locations->data[cursor] |
                (uint16_t)((uint16_t)locations->data[cursor + 1u] << 8);
            expression_offset = cursor + 2u;
            assert(list_expression_size >= 3u &&
                   list_expression_size <= locations->size -
                                              expression_offset);
            assert(locations->data[expression_offset] == frame_register);
            displacement_offset = expression_offset + 1u;
            assert(read_sleb(locations->data,
                             expression_offset + list_expression_size,
                             &displacement_offset) < 0);
            assert(displacement_offset + 1u ==
                   expression_offset + list_expression_size);
            assert(locations->data[displacement_offset] == 0x06u);
            assert(locations->relocs != NULL);
        } else {
            expression_offset = die + 1u + 20u;
            expression_size = read_uleb(
                info->data, info->size, &expression_offset);
            assert(expression_size >= 3u &&
                   expression_size <= info->size - expression_offset);
            assert(info->data[expression_offset] == frame_register);
            displacement_offset = expression_offset + 1u;
            assert(read_sleb(info->data,
                             expression_offset + expression_size,
                             &displacement_offset) < 0);
            assert(displacement_offset + 1u ==
                   expression_offset + expression_size);
            assert(info->data[displacement_offset] == 0x06u);
        }
        found = true;
        break;
    }
    assert(found);
}

static void verify_vla_bound_dies(const ObjSection* info,
                                  const ObjSection* abbrev,
                                  uint16_t architecture)
{
    uint8_t frame_register = architecture == ARCH_X64 ? 0x76u : 0x75u;
    unsigned constant_element_counts = 0u;
    unsigned nested_element_counts = 0u;
    assert(info != NULL && abbrev != NULL);
    assert(contains_byte_pair(abbrev->data, abbrev->size, 0x37u, 0x18u));
    for (uint64_t die = 11u; die + 2u < info->size; ++die) {
        uint64_t cursor;
        uint64_t expression_end;
        uint64_t element_size;
        int64_t frame_offset;
        uint8_t expression_size;
        if (info->data[die] != 34u) continue;
        expression_size = info->data[die + 1u];
        if (expression_size < 6u || expression_size > 32u ||
            die + 2u + expression_size > info->size ||
            info->data[die + 2u] != frame_register) {
            continue;
        }
        cursor = die + 3u;
        expression_end = die + 2u + expression_size;
        if (!try_read_sleb(info->data, expression_end, &cursor,
                           &frame_offset) || frame_offset >= 0 ||
            cursor >= expression_end || info->data[cursor++] != 0x06u) {
            continue;
        }
        if (cursor < expression_end && info->data[cursor] == 0x10u) {
            ++cursor; /* DW_OP_constu */
            if (try_read_uleb(info->data, expression_end, &cursor,
                              &element_size) && element_size == 4u &&
                cursor + 1u == expression_end &&
                info->data[cursor] == 0x1bu) {
                ++constant_element_counts;
            }
            continue;
        }
        if (cursor < expression_end &&
            info->data[cursor++] == frame_register &&
            try_read_sleb(info->data, expression_end, &cursor,
                          &frame_offset) && frame_offset < 0 &&
            cursor + 2u == expression_end &&
            info->data[cursor] == 0x06u &&
            info->data[cursor + 1u] == 0x1bu) {
            ++nested_element_counts;
        }
    }
    assert(constant_element_counts >= 2u);
    assert(nested_element_counts >= 1u);
}

static void verify_multidimensional_vla_type(const ObjSection* info,
                                             const ObjSection* strings)
{
    bool found = false;
    assert(info != NULL && strings != NULL);
    for (uint64_t die = 11u; die + 9u < info->size; ++die) {
        uint32_t name_offset;
        uint32_t type_offset;
        uint32_t element_type_offset;
        if (info->data[die] != 4u && info->data[die] != 39u) continue;
        name_offset = read_u32(info->data, die + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset,
                   "debug_vla_matrix_values") != 0) {
            continue;
        }
        type_offset = read_u32(info->data, die + 5u);
        assert(type_offset < info->size && info->data[type_offset] == 10u);
        assert(type_offset + 5u < info->size &&
               info->data[type_offset + 5u] == 34u);
        element_type_offset = read_u32(info->data, type_offset + 1u);
        assert(element_type_offset < info->size &&
               info->data[element_type_offset] == 10u);
        assert(element_type_offset + 5u < info->size &&
               info->data[element_type_offset + 5u] == 34u);
        found = true;
        break;
    }
    assert(found);
}

static void verify_vla_pointer_type(const ObjSection* info,
                                    const ObjSection* strings,
                                    const char* variable_name,
                                    uint8_t variable_abbreviation,
                                    uint16_t architecture)
{
    bool found = false;
    uint8_t frame_register = architecture == ARCH_X64 ? 0x76u : 0x75u;
    assert(info != NULL && strings != NULL && variable_name != NULL);
    for (uint64_t die = 11u; die + 9u < info->size; ++die) {
        uint32_t name_offset;
        uint32_t pointer_type_offset;
        uint32_t array_type_offset;
        uint64_t expression_cursor;
        uint64_t expression_end;
        uint64_t element_size;
        int64_t frame_offset;
        uint8_t expression_size;
        if (info->data[die] != variable_abbreviation &&
            !(variable_abbreviation == 4u && info->data[die] == 39u)) {
            continue;
        }
        name_offset = read_u32(info->data, die + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset,
                   variable_name) != 0) {
            continue;
        }
        pointer_type_offset = read_u32(info->data, die + 5u);
        assert(pointer_type_offset < info->size &&
               info->data[pointer_type_offset] == 6u);
        array_type_offset = read_u32(info->data, pointer_type_offset + 2u);
        assert(array_type_offset < info->size &&
               info->data[array_type_offset] == 10u);
        assert(array_type_offset + 5u < info->size &&
               info->data[array_type_offset + 5u] == 34u);
        expression_size = info->data[array_type_offset + 6u];
        expression_cursor = array_type_offset + 7u;
        expression_end = expression_cursor + expression_size;
        assert(expression_size >= 6u && expression_size <= 32u &&
               expression_end <= info->size &&
               info->data[expression_cursor++] == frame_register);
        assert(try_read_sleb(info->data, expression_end, &expression_cursor,
                             &frame_offset) && frame_offset < 0);
        assert(expression_cursor < expression_end &&
               info->data[expression_cursor++] == 0x06u);
        assert(expression_cursor < expression_end &&
               info->data[expression_cursor++] == 0x10u);
        assert(try_read_uleb(info->data, expression_end, &expression_cursor,
                             &element_size) && element_size == 4u);
        assert(expression_cursor + 1u == expression_end &&
               info->data[expression_cursor] == 0x1bu);
        found = true;
        break;
    }
    assert(found);
}

static bool line_table_has_row(const ObjSection* line,
                               uint32_t expected_line)
{
    uint64_t offset;
    int64_t current_line = 1;
    uint8_t line_base;
    uint8_t line_range;
    uint8_t opcode_base;
    assert(line != NULL && line->size >= 16u);
    offset = 10u + read_u32(line->data, 6u);
    line_base = line->data[12u];
    line_range = line->data[13u];
    opcode_base = line->data[14u];
    assert(offset <= line->size && line_range != 0u && opcode_base > 1u);
    while (offset < line->size) {
        uint8_t opcode = line->data[offset++];
        if (opcode == 0u) {
            uint64_t length = read_uleb(line->data, line->size, &offset);
            uint64_t end = offset + length;
            uint8_t extended;
            assert(length > 0u && end <= line->size);
            extended = line->data[offset++];
            if (extended == 1u) return false;
            offset = end;
            continue;
        }
        if (opcode >= opcode_base) {
            uint8_t adjusted = (uint8_t)(opcode - opcode_base);
            current_line += (int8_t)line_base +
                            (int64_t)(adjusted % line_range);
            if (current_line == (int64_t)expected_line) return true;
            continue;
        }
        switch (opcode) {
            case 1u:
                if (current_line == (int64_t)expected_line) return true;
                break;
            case 2u:
            case 4u:
            case 5u:
            case 12u:
                (void)read_uleb(line->data, line->size, &offset);
                break;
            case 3u:
                current_line += read_sleb(line->data, line->size, &offset);
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
    return false;
}

static void verify_legacy_statement_line_rows(const char* path,
                                               uint16_t architecture)
{
    static const uint32_t expected_lines[] = {102u, 103u, 104u, 106u, 108u};
    ObjectFile* object = objfile_read(path);
    ObjSection* line;
    assert(object != NULL && object->arch == architecture);
    line = objfile_get_section(object, ".debug_line");
    assert(line != NULL);
    for (size_t index = 0u;
         index < sizeof(expected_lines) / sizeof(expected_lines[0]);
         ++index) {
        assert(line_table_has_row(line, expected_lines[index]));
    }
    objfile_free(object);
}

static bool line_table_file_matches(const ObjSection* line,
                                    uint32_t expected_index,
                                    const char* expected_name)
{
    uint64_t offset;
    uint64_t header_end;
    uint32_t file_index = 0u;
    assert(line != NULL && line->size >= 16u && expected_index > 0u);
    header_end = 10u + read_u32(line->data, 6u);
    assert(header_end <= line->size);
    offset = 15u + (uint64_t)(line->data[14u] - 1u);
    assert(offset <= header_end);
    while (offset < header_end && line->data[offset] != 0u) {
        while (offset < header_end && line->data[offset] != 0u) ++offset;
        assert(offset < header_end);
        ++offset;
    }
    assert(offset < header_end);
    ++offset; /* include_directories terminator */
    while (offset < header_end && line->data[offset] != 0u) {
        uint64_t name_offset = offset;
        while (offset < header_end && line->data[offset] != 0u) ++offset;
        assert(offset < header_end);
        ++offset;
        ++file_index;
        (void)read_uleb(line->data, header_end, &offset);
        (void)read_uleb(line->data, header_end, &offset);
        (void)read_uleb(line->data, header_end, &offset);
        if (file_index == expected_index) {
            return strcmp((const char*)line->data + name_offset,
                          expected_name) == 0;
        }
    }
    return false;
}

static void verify_debug_abbreviation(const ObjSection* abbrev,
                                      uint64_t expected_code,
                                      uint64_t expected_tag,
                                      uint8_t expected_children,
                                      const uint8_t* expected_attributes,
                                      size_t expected_attribute_count)
{
    uint64_t offset = 0u;
    assert(abbrev != NULL);
    while (offset < abbrev->size) {
        uint64_t code = read_uleb(abbrev->data, abbrev->size, &offset);
        uint64_t tag;
        uint8_t has_children;
        size_t attribute_index = 0u;
        if (code == 0u) break;
        tag = read_uleb(abbrev->data, abbrev->size, &offset);
        assert(offset < abbrev->size);
        has_children = abbrev->data[offset++];
        while (offset < abbrev->size) {
            uint64_t attribute = read_uleb(abbrev->data,
                                           abbrev->size, &offset);
            uint64_t form = read_uleb(abbrev->data,
                                      abbrev->size, &offset);
            if (attribute == 0u && form == 0u) break;
            if (code == expected_code) {
                assert(attribute_index * 2u + 1u <
                       expected_attribute_count);
                assert(attribute ==
                       expected_attributes[attribute_index * 2u]);
                assert(form ==
                       expected_attributes[attribute_index * 2u + 1u]);
            }
            ++attribute_index;
        }
        if (code == expected_code) {
            assert(tag == expected_tag &&
                   has_children == expected_children);
            assert(attribute_index * 2u == expected_attribute_count);
            return;
        }
    }
    assert(0 && "expected DWARF abbreviation was not emitted");
}

static void verify_verified_cpp_inline_call_site(
    const char* path, uint16_t architecture, const char* host_name,
    const char* inline_name, uint32_t expected_line,
    uint32_t expected_column)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    ObjSection* line;
    ObjSymbol* function;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    bool found = false;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    line = objfile_get_section(object, ".debug_line");
    function = objfile_find_symbol(object, host_name);
    assert(info != NULL && strings != NULL && line != NULL);
    assert(function != NULL && function->size != 0u);
    for (uint64_t die = 11u;
         die + 1u + 4u + address_size + 16u <= info->size; ++die) {
        uint32_t origin_offset;
        uint32_t name_offset;
        uint32_t call_file;
        uint32_t call_line;
        uint32_t call_column;
        uint64_t low_pc_offset;
        uint64_t call_length;
        bool has_relocation = false;
        if (info->data[die] != 41u) continue;
        origin_offset = read_u32(info->data, die + 1u);
        if ((uint64_t)origin_offset + 25u > info->size ||
            info->data[origin_offset] != 40u) continue;
        name_offset = read_u32(info->data, origin_offset + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset,
                   inline_name) != 0) continue;
        low_pc_offset = die + 1u + 4u;
        call_length = read_u32(
            info->data, low_pc_offset + address_size);
        call_file = read_u32(
            info->data, low_pc_offset + address_size + 4u);
        call_line = read_u32(
            info->data, low_pc_offset + address_size + 8u);
        call_column = read_u32(
            info->data, low_pc_offset + address_size + 12u);
        assert(call_length != 0u && call_file != 0u &&
               call_line == expected_line &&
               call_column == expected_column);
        assert(line_table_file_matches(
            line, call_file, "tests/verified_backend_debug.cpp"));
        for (const ObjReloc* relocation = info->relocs;
             relocation; relocation = relocation->next) {
            if (relocation->offset != low_pc_offset) continue;
            if (relocation->symbol_name == NULL ||
                strcmp(relocation->symbol_name, host_name) != 0) {
                break;
            }
            assert(relocation->addend >= 0);
            assert((uint64_t)relocation->addend <= function->size &&
                   call_length <=
                       function->size - (uint64_t)relocation->addend);
            found = true;
            has_relocation = true;
            break;
        }
        if (has_relocation) break;
    }
    assert(found);
    objfile_free(object);
}

static void verify_verified_cpp_inline_debug(
    const char* path, uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    ObjSection* line;
    ObjSymbol* function;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint64_t middle_die = UINT64_MAX;
    uint64_t leaf_die = UINT64_MAX;
    uint64_t leaf_children_end = 0u;
    uint64_t middle_start = 0u;
    uint64_t middle_end = 0u;
    uint64_t leaf_start = 0u;
    uint64_t leaf_end = 0u;
    uint32_t middle_call_line = 0u;
    uint32_t middle_call_column = 0u;
    uint32_t leaf_call_line = 0u;
    uint32_t leaf_call_column = 0u;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    line = objfile_get_section(object, ".debug_line");
    function = objfile_find_symbol(object, "verified_cpp_debug_inline_host");
    assert(info != NULL && strings != NULL && line != NULL);
    assert(function != NULL && function->size != 0u);
    for (uint64_t die = 11u;
         die + 1u + 4u + address_size + 16u <= info->size; ++die) {
        uint32_t origin_offset;
        uint32_t name_offset;
        uint32_t call_file;
        uint32_t call_line;
        uint32_t call_column;
        uint64_t low_pc_offset;
        uint64_t call_length;
        const char* inline_name;
        bool has_relocation = false;
        if (info->data[die] != 41u) continue;
        origin_offset = read_u32(info->data, die + 1u);
        if ((uint64_t)origin_offset + 25u > info->size ||
            info->data[origin_offset] != 40u) continue;
        name_offset = read_u32(info->data, origin_offset + 1u);
        if (name_offset >= strings->size) continue;
        inline_name = (const char*)strings->data + name_offset;
        if (strcmp(inline_name, "verified_cpp_debug_inline_middle") != 0 &&
            strcmp(inline_name, "verified_cpp_debug_inline_leaf") != 0) {
            continue;
        }
        low_pc_offset = die + 1u + 4u;
        call_length = read_u32(
            info->data, low_pc_offset + address_size);
        call_file = read_u32(
            info->data, low_pc_offset + address_size + 4u);
        call_line = read_u32(
            info->data, low_pc_offset + address_size + 8u);
        call_column = read_u32(
            info->data, low_pc_offset + address_size + 12u);
        assert(call_length != 0u && call_file != 0u && call_line != 0u &&
               call_column != 0u);
        assert(line_table_file_matches(
            line, call_file, "tests/verified_backend_debug.cpp"));
        for (const ObjReloc* relocation = info->relocs;
             relocation; relocation = relocation->next) {
            if (relocation->offset != low_pc_offset) continue;
            if (relocation->symbol_name == NULL ||
                strcmp(relocation->symbol_name,
                       "verified_cpp_debug_inline_host") != 0) {
                break;
            }
            assert(relocation->addend >= 0);
            uint64_t range_start = (uint64_t)relocation->addend;
            uint64_t range_end;
            assert(range_start <= function->size &&
                   call_length <= function->size - range_start);
            range_end = range_start + call_length;
            if (strcmp(inline_name,
                       "verified_cpp_debug_inline_middle") == 0) {
                middle_die = die;
                middle_start = range_start;
                middle_end = range_end;
                middle_call_line = call_line;
                middle_call_column = call_column;
            } else {
                leaf_die = die;
                leaf_children_end =
                    die + 1u + 4u + address_size + 4u + 12u;
                leaf_start = range_start;
                leaf_end = range_end;
                leaf_call_line = call_line;
                leaf_call_column = call_column;
            }
            has_relocation = true;
            break;
        }
        if (!has_relocation) continue;
    }
    assert(middle_end > middle_start && leaf_end > leaf_start);
    assert(middle_start <= leaf_start && leaf_end <= middle_end);
    assert(middle_call_line == 43u && middle_call_column == 12u &&
           leaf_call_line == 38u && leaf_call_column == 12u);
    assert(middle_die < leaf_die);
    {
        uint64_t child = leaf_children_end;
        while (child < info->size && info->data[child] != 0u) {
            uint32_t parameter_origin;
            uint64_t expression_size;
            uint64_t expression_end;
            int64_t frame_offset;
            assert(info->data[child++] == 67u);
            parameter_origin = read_u32(info->data, child);
            child += 4u;
            assert(parameter_origin < info->size);
            expression_size = read_uleb(info->data, info->size, &child);
            assert(expression_size >= 2u &&
                   expression_size <= info->size - child);
            expression_end = child + expression_size;
            assert(info->data[child++] ==
                   (architecture == ARCH_X64 ? 0x76u : 0x75u));
            frame_offset = read_sleb(info->data, info->size, &child);
            assert(child == expression_end && frame_offset != 0);
        }
        assert(child + 1u < info->size && info->data[child] == 0u &&
               info->data[child + 1u] == 0u);
    }
    objfile_free(object);
    verify_verified_cpp_inline_call_site(
        path, architecture, "verified_cpp_debug_inline_member_host",
        "VerifiedCppDebugInlineMember::call", 56u, 12u);
    verify_verified_cpp_inline_call_site(
        path, architecture, "verified_cpp_debug_inline_pointer_host",
        "verified_cpp_debug_inline_pointer_target", 69u, 12u);
    verify_verified_cpp_inline_call_site(
        path, architecture, "verified_cpp_debug_inline_array_host",
        "verified_cpp_debug_inline_array_second", 89u, 12u);
}

static void verify_inline_frame_parameter_die(const ObjSection* info,
                                             uint64_t* cursor,
                                             uint16_t architecture)
{
    uint32_t origin;
    uint64_t expression_size;
    uint64_t expression_end;
    int64_t frame_offset;
    assert(info != NULL && cursor != NULL && *cursor < info->size);
    assert(info->data[(*cursor)++] == 67u);
    origin = read_u32(info->data, *cursor);
    *cursor += 4u;
    assert(origin < info->size && info->data[origin] == 42u);
    expression_size = read_uleb(info->data, info->size, cursor);
    assert(expression_size >= 2u && expression_size <= info->size - *cursor);
    expression_end = *cursor + expression_size;
    assert(info->data[(*cursor)++] ==
           (architecture == ARCH_X64 ? 0x76u : 0x75u));
    frame_offset = read_sleb(info->data, info->size, cursor);
    assert(*cursor == expression_end && frame_offset != 0);
}

static void verify_inline_dynamic_scalar_parameter(
    const char* path, uint16_t architecture, const char* inline_name,
    const char* call_site_name, const char* source_name) {
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    ObjSection* locations;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint64_t call_die = UINT64_MAX;
    bool has_location;
    bool is_location_list;
    int64_t source_frame_offset;
    assert(object && object->arch == architecture && inline_name &&
           call_site_name && source_name);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    locations = objfile_get_section(object, ".debug_loc");
    assert(info && strings && locations);
    for (uint64_t die = 11u;
         die + 1u + 4u + address_size + 4u + 12u <= info->size; ++die) {
        uint32_t origin;
        uint32_t name_offset;
        uint64_t low_pc_offset;
        bool matches_call = false;
        if (info->data[die] != 41u) continue;
        origin = read_u32(info->data, die + 1u);
        if (origin >= info->size || info->data[origin] != 40u) continue;
        name_offset = read_u32(info->data, origin + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset,
                   inline_name) != 0) {
            continue;
        }
        low_pc_offset = die + 1u + 4u;
        for (const ObjReloc* relocation = info->relocs; relocation;
             relocation = relocation->next) {
            if (relocation->offset == low_pc_offset &&
                relocation->symbol_name &&
                strcmp(relocation->symbol_name, call_site_name) == 0) {
                matches_call = true;
                break;
            }
        }
        if (!matches_call) continue;
        assert(call_die == UINT64_MAX);
        call_die = die;
    }
    assert(call_die != UINT64_MAX);
    assert(find_variable_location(
        info, strings, locations, source_name, architecture, &has_location,
        &is_location_list, &source_frame_offset));
    assert(has_location && source_frame_offset != 0);
    {
        uint64_t concrete_parameter = call_die + 1u + 4u + address_size +
            4u + 12u;
        uint64_t expression_offset = concrete_parameter + 5u;
        uint64_t expression_size;
        uint64_t expression_end;
        uint32_t abstract_parameter = read_u32(info->data, call_die + 1u) +
            27u;
        assert(info->data[concrete_parameter] == 67u &&
               read_u32(info->data, concrete_parameter + 1u) ==
                   abstract_parameter);
        expression_size = read_uleb(
            info->data, info->size, &expression_offset);
        expression_end = expression_offset + expression_size;
        assert(expression_size >= 2u && expression_end <= info->size &&
               info->data[expression_offset++] ==
                   (architecture == ARCH_X64 ? 0x76u : 0x75u) &&
               read_sleb(info->data, expression_end, &expression_offset) ==
                   source_frame_offset &&
               expression_offset == expression_end);
    }
    objfile_free(object);
}

static void verify_inline_parameter_address_constant(
    const ObjSection* info, uint64_t call_die, uint16_t architecture,
    const char* symbol_name, int64_t expected_addend) {
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint64_t concrete_parameter;
    uint32_t origin_parameter;
    uint64_t address_offset;
    bool found_address_relocation = false;
    assert(info && info->data && call_die < info->size && symbol_name);
    concrete_parameter = call_die + 1u + 4u + address_size + 4u + 12u;
    origin_parameter = read_u32(info->data, call_die + 1u) + 27u;
    address_offset = concrete_parameter + 5u;
    assert(info->data[concrete_parameter] == 68u);
    assert(read_u32(info->data, concrete_parameter + 1u) == origin_parameter);
    for (uint64_t byte = 0u; byte < address_size; ++byte) {
        assert(info->data[address_offset + byte] == 0u);
    }
    for (const ObjReloc* relocation = info->relocs;
         relocation; relocation = relocation->next) {
        if (relocation->offset == address_offset &&
            relocation->type == (architecture == ARCH_X64
                ? RELOC_ABS64 : RELOC_ABS32U) &&
            relocation->addend == expected_addend &&
            relocation->symbol_name &&
            strcmp(relocation->symbol_name, symbol_name) == 0) {
            found_address_relocation = true;
            break;
        }
    }
    assert(found_address_relocation);
}

static void verify_inline_parameter_frame_address(
    const ObjSection* info, const ObjSection* strings,
    const ObjSection* locations, uint64_t call_die,
    uint16_t architecture, const char* variable_name,
    int64_t expected_adjustment, bool dereference_source) {
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint64_t concrete_parameter;
    uint32_t origin_parameter;
    uint64_t expression_offset;
    uint64_t expression_size;
    uint64_t expression_end;
    int64_t expression_frame_offset;
    int64_t variable_frame_offset;
    bool variable_has_location;
    bool variable_is_location_list;
    assert(info && info->data && call_die < info->size && variable_name);
    concrete_parameter = call_die + 1u + 4u + address_size + 4u + 12u;
    origin_parameter = read_u32(info->data, call_die + 1u) + 27u;
    expression_offset = concrete_parameter + 5u;
    expression_size = read_uleb(
        info->data, info->size, &expression_offset);
    if (info->data[concrete_parameter] != 67u) {
        fprintf(stderr,
                "DWARF inline parameter %s has abbreviation %u; "
                "expected frame-value location 67 (adjustment %lld)\n",
                variable_name, (unsigned)info->data[concrete_parameter],
                (long long)expected_adjustment);
    }
    assert(info->data[concrete_parameter] == 67u);
    assert(read_u32(info->data, concrete_parameter + 1u) == origin_parameter);
    assert(expression_size >= 3u);
    assert(info->data[expression_offset] ==
           (architecture == ARCH_X64 ? 0x76u : 0x75u));
    expression_end = expression_offset + expression_size;
    ++expression_offset;
    expression_frame_offset = read_sleb(
        info->data, info->size, &expression_offset);
    if (dereference_source) {
        assert(expression_offset < expression_end &&
               info->data[expression_offset++] == 0x06u); /* DW_OP_deref */
    }
    if (expected_adjustment > 0) {
        uint64_t encoded_adjustment;
        assert(expression_offset < expression_end &&
               info->data[expression_offset++] == 0x23u);
        encoded_adjustment = read_uleb(
            info->data, expression_end, &expression_offset);
        assert(encoded_adjustment == (uint64_t)expected_adjustment);
    } else if (expected_adjustment < 0) {
        int64_t encoded_adjustment;
        assert(expression_offset < expression_end &&
               info->data[expression_offset++] == 0x11u);
        encoded_adjustment = read_sleb(
            info->data, expression_end, &expression_offset);
        assert(encoded_adjustment == expected_adjustment);
        assert(expression_offset < expression_end &&
               info->data[expression_offset++] == 0x22u);
    }
    assert(expression_offset + 1u == expression_end);
    assert(info->data[expression_offset] == 0x9fu); /* DW_OP_stack_value */
    assert(find_variable_location(
        info, strings, locations, variable_name, architecture,
        &variable_has_location, &variable_is_location_list,
        &variable_frame_offset));
    assert(variable_has_location &&
           variable_frame_offset == expression_frame_offset);
}

typedef struct VerifyInlinePointerIndexTerm {
    const char* name;
    uint8_t size;
    bool is_unsigned;
    uint64_t element_size;
    bool subtract;
    uint8_t group_count;
    /* For a scalar term, group_width optionally expects a cast mask width. */
    uint8_t group_width;
    bool group_subtract;
    int64_t constant_value;
} VerifyInlinePointerIndexTerm;

static void verify_inline_parameter_frame_pointer_indices(
    const ObjSection* info, const ObjSection* strings,
    const ObjSection* locations, uint64_t call_die,
    uint16_t architecture, const char* pointer_name,
    const VerifyInlinePointerIndexTerm* index_terms, size_t index_count,
    int64_t address_addend, bool dereference_source,
    const uint8_t* cast_steps, size_t cast_step_count,
    int64_t signed_divisor, int64_t pointer_adjustment) {
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint64_t concrete_parameter;
    uint64_t expression_offset;
    uint64_t expression_size;
    uint64_t expression_end;
    int64_t pointer_expression_offset;
    int64_t pointer_variable_offset;
    bool pointer_has_location;
    bool pointer_is_location_list;
    assert(info && info->data && call_die < info->size && pointer_name &&
           index_terms && index_count > 0u && index_count <= 32u &&
           address_addend >= 0 &&
           (cast_step_count == 0u || cast_steps != NULL));
    concrete_parameter = call_die + 1u + 4u + address_size + 4u + 12u;
    assert(info->data[concrete_parameter] == 67u);
    assert(read_u32(info->data, concrete_parameter + 1u) ==
           read_u32(info->data, call_die + 1u) + 27u);
    expression_offset = concrete_parameter + 5u;
    expression_size = read_uleb(info->data, info->size, &expression_offset);
    expression_end = expression_offset + expression_size;
    assert(expression_size != 0u && expression_end <= info->size);
    assert(info->data[expression_offset++] ==
           (architecture == ARCH_X64 ? 0x76u : 0x75u));
    pointer_expression_offset = read_sleb(
        info->data, expression_end, &expression_offset);
    if (address_addend > 0) {
        assert(expression_offset < expression_end &&
               info->data[expression_offset++] == 0x23u);
        assert(read_uleb(info->data, expression_end, &expression_offset) ==
               (uint64_t)address_addend);
    }
    if (dereference_source) {
        assert(expression_offset < expression_end &&
               info->data[expression_offset++] == 0x06u); /* DW_OP_deref */
    }
    assert(expression_offset < expression_end);
    for (size_t index = 0u; index < index_count;) {
        const VerifyInlinePointerIndexTerm* term = &index_terms[index];
        uint64_t shift;
        int64_t index_expression_offset;
        int64_t index_variable_offset;
        bool index_has_location;
        bool index_is_location_list;
        if (term->group_count != 0u) {
            assert(term->group_count >= 1u &&
                   index + term->group_count <= index_count &&
                   term->group_width > 0u &&
                   term->group_width <= address_size &&
                   term->element_size > 0u);
            for (size_t group_index = 0u;
                 group_index < term->group_count; ++group_index) {
                const VerifyInlinePointerIndexTerm* group_term =
                    &index_terms[index + group_index];
                if (group_term->name) {
                    assert(group_term->size > 0u &&
                           group_term->size <= address_size &&
                           group_term->is_unsigned &&
                           group_term->size == term->group_width);
                    assert(expression_offset < expression_end &&
                           info->data[expression_offset++] ==
                               (architecture == ARCH_X64 ? 0x76u : 0x75u));
                    index_expression_offset = read_sleb(
                        info->data, expression_end, &expression_offset);
                    assert(expression_offset + 2u <= expression_end &&
                           info->data[expression_offset++] == 0x94u &&
                           info->data[expression_offset++] ==
                               group_term->size);
                    assert(find_variable_location(
                        info, strings, locations, group_term->name,
                        architecture, &index_has_location,
                        &index_is_location_list, &index_variable_offset));
                    assert(index_has_location &&
                           index_variable_offset == index_expression_offset);
                    if (group_term->constant_value > 1) {
                        assert(expression_offset < expression_end &&
                               info->data[expression_offset++] == 0x10u &&
                               read_uleb(info->data, expression_end,
                                         &expression_offset) ==
                                   (uint64_t)group_term->constant_value);
                        assert(expression_offset < expression_end &&
                               info->data[expression_offset++] == 0x1eu);
                    }
                } else {
                    assert(group_term->size == term->group_width &&
                           group_term->is_unsigned &&
                           expression_offset < expression_end &&
                           info->data[expression_offset++] == 0x10u &&
                           read_uleb(
                               info->data, expression_end,
                               &expression_offset) ==
                               (uint64_t)group_term->constant_value);
                }
                if (group_index != 0u) {
                    assert(expression_offset < expression_end &&
                           info->data[expression_offset++] ==
                               (group_term->subtract ? 0x1cu : 0x22u));
                }
            }
            if (term->group_width < address_size) {
                uint64_t mask =
                    (UINT64_C(1) << (term->group_width * 8u)) - 1u;
                assert(expression_offset < expression_end &&
                       info->data[expression_offset++] == 0x10u &&
                       read_uleb(
                           info->data, expression_end, &expression_offset) ==
                           mask);
                assert(expression_offset < expression_end &&
                       info->data[expression_offset++] == 0x1au);
            }
            assert(expression_offset < expression_end &&
                   info->data[expression_offset++] == 0x10u &&
                   read_uleb(info->data, expression_end, &expression_offset) ==
                       term->element_size);
            assert(expression_offset + 2u <= expression_end &&
                   info->data[expression_offset++] == 0x1eu &&
                   info->data[expression_offset++] ==
                       (term->group_subtract ? 0x1cu : 0x22u));
            index += term->group_count;
            continue;
        }
        assert(term->name && term->size > 0u && term->size <= address_size);
        shift = address_size * 8u - term->size * 8u;
        assert(expression_offset < expression_end &&
               info->data[expression_offset++] ==
                   (architecture == ARCH_X64 ? 0x76u : 0x75u));
        index_expression_offset = read_sleb(
            info->data, expression_end, &expression_offset);
        assert(expression_offset + 2u <= expression_end &&
               info->data[expression_offset++] == 0x94u &&
               info->data[expression_offset++] == term->size);
        if (!term->is_unsigned && shift != 0u) {
            assert(expression_offset < expression_end &&
                   info->data[expression_offset++] == 0x10u);
            assert(read_uleb(
                info->data, expression_end, &expression_offset) == shift);
            assert(expression_offset < expression_end &&
                   info->data[expression_offset++] == 0x24u);
            assert(expression_offset < expression_end &&
                   info->data[expression_offset++] == 0x10u);
            assert(read_uleb(
                info->data, expression_end, &expression_offset) == shift);
            assert(expression_offset < expression_end &&
                   info->data[expression_offset++] == 0x26u);
        }
        if (index == 0u) {
            for (size_t step = 0u; step < cast_step_count; ++step) {
                uint8_t encoded_width = cast_steps[step];
                bool sign_extend = (encoded_width & 0x80u) != 0u;
                uint8_t cast_width = encoded_width & 0x7fu;
                assert(cast_width > 0u && cast_width < address_size);
                if (sign_extend) {
                    uint64_t cast_shift =
                        (address_size - cast_width) * 8u;
                    assert(expression_offset < expression_end &&
                           info->data[expression_offset++] == 0x10u &&
                           read_uleb(info->data, expression_end,
                                     &expression_offset) == cast_shift &&
                           expression_offset < expression_end &&
                           info->data[expression_offset++] == 0x24u &&
                           expression_offset < expression_end &&
                           info->data[expression_offset++] == 0x10u &&
                           read_uleb(info->data, expression_end,
                                     &expression_offset) == cast_shift &&
                           expression_offset < expression_end &&
                           info->data[expression_offset++] == 0x26u);
                } else {
                    uint64_t cast_mask =
                        (UINT64_C(1) << (cast_width * 8u)) - 1u;
                    assert(expression_offset < expression_end &&
                           info->data[expression_offset++] == 0x10u &&
                           read_uleb(info->data, expression_end,
                                     &expression_offset) == cast_mask &&
                           expression_offset < expression_end &&
                           info->data[expression_offset++] == 0x1au);
                }
            }
        }
        if (term->group_width != 0u) {
            assert(term->group_width <= address_size);
            if (term->group_width < address_size) {
                uint64_t mask = term->group_width == 8u ? UINT64_MAX
                    : (UINT64_C(1) << (term->group_width * 8u)) - 1u;
                assert(expression_offset < expression_end &&
                       info->data[expression_offset++] == 0x10u &&
                       read_uleb(
                           info->data, expression_end,
                           &expression_offset) == mask);
                assert(expression_offset < expression_end &&
                       info->data[expression_offset++] == 0x1au);
            }
        }
        if (index == 0u && signed_divisor != 0) {
            assert(!term->is_unsigned && term->group_count == 0u &&
                   expression_offset < expression_end &&
                   info->data[expression_offset++] == 0x11u &&
                   read_sleb(info->data, expression_end,
                             &expression_offset) == signed_divisor &&
                   expression_offset < expression_end &&
                   info->data[expression_offset++] == 0x1bu);
        }
        if ((term->is_unsigned && term->constant_value > 1) ||
            (!term->is_unsigned &&
             (term->constant_value < 0 || term->constant_value > 1))) {
            assert(expression_offset < expression_end &&
                   info->data[expression_offset++] ==
                       (term->is_unsigned ? 0x10u : 0x11u));
            if (term->is_unsigned) {
                assert(read_uleb(info->data, expression_end,
                                 &expression_offset) ==
                       (uint64_t)term->constant_value);
            } else {
                assert(read_sleb(info->data, expression_end,
                                 &expression_offset) ==
                       term->constant_value);
            }
            assert(expression_offset < expression_end &&
                   info->data[expression_offset++] == 0x1eu);
        }
        assert(expression_offset < expression_end &&
               info->data[expression_offset++] == 0x10u);
        assert(read_uleb(
            info->data, expression_end, &expression_offset) ==
            term->element_size);
        assert(expression_offset + 2u <= expression_end &&
               info->data[expression_offset++] == 0x1eu &&
               info->data[expression_offset++] ==
                   (term->subtract ? 0x1cu : 0x22u));
        assert(find_variable_location(
            info, strings, locations, term->name, architecture,
            &index_has_location, &index_is_location_list,
            &index_variable_offset));
        assert(index_has_location &&
               index_variable_offset == index_expression_offset);
        ++index;
    }
    if (pointer_adjustment > 0) {
        assert(expression_offset < expression_end &&
               info->data[expression_offset++] == 0x23u &&
               read_uleb(info->data, expression_end, &expression_offset) ==
                   (uint64_t)pointer_adjustment);
    } else if (pointer_adjustment < 0) {
        assert(expression_offset < expression_end &&
               info->data[expression_offset++] == 0x11u &&
               read_sleb(info->data, expression_end, &expression_offset) ==
                   pointer_adjustment &&
               expression_offset < expression_end &&
               info->data[expression_offset++] == 0x22u);
    }
    assert(expression_offset + 1u == expression_end &&
           info->data[expression_offset] == 0x9fu);
    assert(find_variable_location(
        info, strings, locations, pointer_name, architecture,
        &pointer_has_location, &pointer_is_location_list,
        &pointer_variable_offset));
    assert(pointer_has_location &&
           pointer_variable_offset == pointer_expression_offset);
}

static void verify_inline_parameter_is_not_attributed(
    const ObjSection* info, uint64_t call_die, uint16_t architecture) {
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint64_t first_child = call_die + 1u + 4u + address_size + 4u + 12u;
    assert(info && info->data && call_die < info->size);
    assert(first_child < info->size && info->data[first_child] == 0u);
}

static void verify_inline_debug_object(const char* path,
                                      uint16_t architecture)
{
    static const uint8_t origin_attributes[] = {
        0x03u, 0x0eu, 0x3au, 0x06u, 0x3bu, 0x06u, 0x39u, 0x06u,
        0x6eu, 0x0eu, 0x49u, 0x13u, 0x20u, 0x0bu, 0x27u, 0x0cu
    };
    static const uint8_t call_attributes[] = {
        0x31u, 0x13u, 0x11u, 0x01u, 0x12u, 0x06u,
        0x58u, 0x06u, 0x59u, 0x06u, 0x57u, 0x06u
    };
    static const uint8_t parameter_attributes[] = {
        0x03u, 0x0eu, 0x49u, 0x13u, 0x3au, 0x06u,
        0x3bu, 0x06u, 0x39u, 0x06u
    };
    static const uint8_t abstract_local_attributes[] = {
        0x03u, 0x0eu, 0x49u, 0x13u, 0x3au, 0x06u,
        0x3bu, 0x06u, 0x39u, 0x06u
    };
    static const uint8_t signed_local_attributes[] = {
        0x31u, 0x13u, 0x1cu, 0x0du
    };
    static const uint8_t unsigned_local_attributes[] = {
        0x31u, 0x13u, 0x1cu, 0x0fu
    };
    static const uint8_t float_local_attributes[] = {
        0x31u, 0x13u, 0x1cu, 0x0au
    };
    static const uint8_t signed_parameter_attributes[] = {
        0x31u, 0x13u, 0x1cu, 0x0du
    };
    static const uint8_t unsigned_parameter_attributes[] = {
        0x31u, 0x13u, 0x1cu, 0x0fu
    };
    static const uint8_t float_parameter_attributes[] = {
        0x31u, 0x13u, 0x1cu, 0x0au
    };
    static const uint8_t pointer_parameter_attributes[] = {
        0x31u, 0x13u, 0x1cu, 0x01u
    };
    static const uint8_t symbolic_pointer_parameter_attributes[] = {
        0x31u, 0x13u, 0x1cu, 0x01u
    };
    static const uint8_t dynamic_parameter_attributes[] = {
        0x31u, 0x13u, 0x02u, 0x18u
    };
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    ObjSection* line;
    ObjSection* locations;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint64_t outer_die = UINT64_MAX;
    uint64_t inner_die = UINT64_MAX;
    uint64_t outer_start = 0u;
    uint64_t outer_end = 0u;
    uint64_t inner_start = 0u;
    uint64_t inner_end = 0u;
    uint64_t inline_local_origin = UINT64_MAX;
    uint64_t inline_local_call = UINT64_MAX;
    uint64_t constant_inline_call = UINT64_MAX;
    uint64_t unsigned_constant_inline_call = UINT64_MAX;
    uint64_t converted_signed_inline_call = UINT64_MAX;
    uint64_t unsigned_to_signed_inline_call = UINT64_MAX;
    uint64_t converted_unsigned_inline_call = UINT64_MAX;
    uint64_t float_inline_call = UINT64_MAX;
    uint64_t double_inline_call = UINT64_MAX;
    uint64_t float_to_double_inline_call = UINT64_MAX;
    uint64_t double_to_float_inline_call = UINT64_MAX;
    uint64_t float_expression_inline_call = UINT64_MAX;
    uint64_t null_pointer_inline_call = UINT64_MAX;
    uint64_t enum_inline_call = UINT64_MAX;
    uint64_t global_pointer_inline_call = UINT64_MAX;
    uint64_t address_constant_inline_call = UINT64_MAX;
    uint64_t static_address_constant_inline_call = UINT64_MAX;
    uint64_t readonly_address_constant_inline_call = UINT64_MAX;
    uint64_t cast_address_constant_inline_call = UINT64_MAX;
    uint64_t array_element_address_constant_inline_call = UINT64_MAX;
    uint64_t array_computed_index_inline_call = UINT64_MAX;
    uint64_t array_unsigned_wrap_index_inline_call = UINT64_MAX;
    uint64_t array_computed_index_out_of_bounds_inline_call = UINT64_MAX;
    uint64_t array_plus_constant_inline_call = UINT64_MAX;
    uint64_t constant_plus_array_inline_call = UINT64_MAX;
    uint64_t array_one_past_inline_call = UINT64_MAX;
    uint64_t array_cast_constant_inline_call = UINT64_MAX;
    uint64_t array_unsigned_modulo_inline_call = UINT64_MAX;
    uint64_t array_interior_minus_inline_call = UINT64_MAX;
    uint64_t array_interior_plus_inline_call = UINT64_MAX;
    uint64_t nested_array_interior_minus_inline_call = UINT64_MAX;
    uint64_t object_one_past_inline_call = UINT64_MAX;
    uint64_t array_out_of_bounds_inline_call = UINT64_MAX;
    uint64_t array_before_begin_inline_call = UINT64_MAX;
    uint64_t nested_array_before_begin_inline_call = UINT64_MAX;
    uint64_t member_address_constant_inline_call = UINT64_MAX;
    uint64_t nested_array_address_constant_inline_call = UINT64_MAX;
    uint64_t matrix_subarray_interior_plus_inline_call = UINT64_MAX;
    uint64_t matrix_subarray_one_past_inline_call = UINT64_MAX;
    uint64_t matrix_subarray_cross_row_inline_call = UINT64_MAX;
    uint64_t function_address_constant_inline_call = UINT64_MAX;
    uint64_t function_designator_inline_call = UINT64_MAX;
    uint64_t static_pointer_inline_call = UINT64_MAX;
    uint64_t automatic_pointer_inline_call = UINT64_MAX;
    uint64_t automatic_address_inline_call = UINT64_MAX;
    uint64_t automatic_const_address_inline_call = UINT64_MAX;
    uint64_t automatic_void_address_inline_call = UINT64_MAX;

    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    line = objfile_get_section(object, ".debug_line");
    locations = objfile_get_section(object, ".debug_loc");
    assert(info != NULL && abbrev != NULL && strings != NULL && line != NULL &&
           locations != NULL);
    verify_debug_abbreviation(abbrev, 40u, 0x2eu,
                              1u,
                              origin_attributes,
                              sizeof(origin_attributes));
    verify_debug_abbreviation(abbrev, 41u, 0x1du,
                              1u,
                              call_attributes, sizeof(call_attributes));
    verify_debug_abbreviation(abbrev, 42u, 0x05u,
                              0u,
                              parameter_attributes,
                              sizeof(parameter_attributes));
    verify_debug_abbreviation(abbrev, 32u, 0x34u, 0u,
                              abstract_local_attributes,
                              sizeof(abstract_local_attributes));
    verify_debug_abbreviation(abbrev, 60u, 0x34u, 0u,
                              signed_local_attributes,
                              sizeof(signed_local_attributes));
    verify_debug_abbreviation(abbrev, 61u, 0x34u, 0u,
                              unsigned_local_attributes,
                              sizeof(unsigned_local_attributes));
    verify_debug_abbreviation(abbrev, 62u, 0x34u, 0u,
                              float_local_attributes,
                              sizeof(float_local_attributes));
    verify_debug_abbreviation(abbrev, 63u, 0x05u, 0u,
                              signed_parameter_attributes,
                              sizeof(signed_parameter_attributes));
    verify_debug_abbreviation(abbrev, 64u, 0x05u, 0u,
                              unsigned_parameter_attributes,
                              sizeof(unsigned_parameter_attributes));
    verify_debug_abbreviation(abbrev, 65u, 0x05u, 0u,
                              float_parameter_attributes,
                              sizeof(float_parameter_attributes));
    verify_debug_abbreviation(abbrev, 66u, 0x05u, 0u,
                              pointer_parameter_attributes,
                              sizeof(pointer_parameter_attributes));
    verify_debug_abbreviation(abbrev, 67u, 0x05u, 0u,
                              dynamic_parameter_attributes,
                              sizeof(dynamic_parameter_attributes));
    verify_debug_abbreviation(abbrev, 68u, 0x05u, 0u,
                              symbolic_pointer_parameter_attributes,
                              sizeof(symbolic_pointer_parameter_attributes));

    for (uint64_t die = 11u; die + 1u + 4u + address_size + 4u + 12u <=
         info->size; ++die) {
        uint32_t origin_offset;
        uint32_t name_offset;
        uint32_t origin_file;
        uint32_t origin_line;
        uint32_t origin_column;
        uint32_t type_offset;
        uint32_t call_file;
        uint32_t call_line;
        uint32_t call_column;
        uint64_t low_pc_offset;
        uint64_t cursor;
        uint64_t parameter_die;
        uint32_t parameter_name;
        uint32_t parameter_type;
        uint32_t parameter_file;
        const char* inline_name;
        uint64_t call_length;
        bool has_call_relocation = false;
        if (info->data[die] != 41u) continue;
        if (die + 1u + 4u + address_size + 4u + 12u > info->size) continue;
        origin_offset = read_u32(info->data, die + 1u);
        if (origin_offset >= info->size || info->data[origin_offset] != 40u) {
            continue;
        }
        name_offset = read_u32(info->data, origin_offset + 1u);
        if (name_offset >= strings->size) continue;
        inline_name = (const char*)strings->data + name_offset;
        if (strcmp(inline_name, "debug_declared_inline") != 0 &&
            strcmp(inline_name, "debug_nested_inline") != 0 &&
            strcmp(inline_name, "debug_local_inline") != 0 &&
            strcmp(inline_name, "debug_constant_inline") != 0 &&
            strcmp(inline_name, "debug_unsigned_constant_inline") != 0 &&
            strcmp(inline_name, "debug_converted_signed_inline") != 0 &&
            strcmp(inline_name, "debug_converted_unsigned_inline") != 0 &&
            strcmp(inline_name, "debug_float_parameter_inline") != 0 &&
            strcmp(inline_name, "debug_double_parameter_inline") != 0 &&
            strcmp(inline_name,
                   "debug_float_to_double_parameter_inline") != 0 &&
            strcmp(inline_name,
                   "debug_double_to_float_parameter_inline") != 0 &&
            strcmp(inline_name,
                   "debug_float_expression_parameter_inline") != 0 &&
            strcmp(inline_name,
                   "debug_null_pointer_parameter_inline") != 0 &&
            strcmp(inline_name,
                   "debug_enum_parameter_inline") != 0 &&
            strcmp(inline_name,
                   "debug_global_pointer_parameter_inline") != 0 &&
            strcmp(inline_name,
                   "debug_const_global_pointer_parameter_inline") != 0 &&
            strcmp(inline_name,
                   "debug_void_pointer_parameter_inline") != 0 &&
            strcmp(inline_name,
                   "debug_function_pointer_parameter_inline") != 0) {
            continue;
        }
        origin_file = read_u32(info->data, origin_offset + 5u);
        origin_line = read_u32(info->data, origin_offset + 9u);
        origin_column = read_u32(info->data, origin_offset + 13u);
        type_offset = read_u32(info->data, origin_offset + 21u);
        assert(origin_file > 0u && origin_line > 0u && origin_column > 0u);
        assert(info->data[origin_offset + 25u] == 1u &&
               info->data[origin_offset + 26u] == 1u);
        assert(type_offset < info->size && info->data[type_offset] == 5u);
        parameter_die = (uint64_t)origin_offset + 27u;
        assert(parameter_die + 22u <= info->size &&
               info->data[parameter_die] == 42u);
        parameter_name = read_u32(info->data, parameter_die + 1u);
        parameter_type = read_u32(info->data, parameter_die + 5u);
        parameter_file = read_u32(info->data, parameter_die + 9u);
        assert(parameter_name < strings->size &&
               strcmp((const char*)strings->data + parameter_name,
                      "value") == 0);
        assert(parameter_type < info->size);
        if (strcmp(inline_name, "debug_null_pointer_parameter_inline") == 0 ||
            strcmp(inline_name,
                   "debug_global_pointer_parameter_inline") == 0 ||
            strcmp(inline_name,
                   "debug_const_global_pointer_parameter_inline") == 0 ||
            strcmp(inline_name,
                   "debug_void_pointer_parameter_inline") == 0 ||
            strcmp(inline_name,
                   "debug_function_pointer_parameter_inline") == 0) {
            assert(info->data[parameter_type] == 6u);
        } else if (strcmp(inline_name,
                          "debug_enum_parameter_inline") == 0) {
            uint32_t enum_name;
            assert(info->data[parameter_type] == 15u ||
                   info->data[parameter_type] == 36u);
            enum_name = read_u32(info->data, parameter_type + 1u);
            assert(enum_name < strings->size &&
                   strcmp((const char*)strings->data + enum_name,
                          "debug_enum") == 0);
        } else {
            assert(info->data[parameter_type] == 5u);
        }
        assert(parameter_file > 0u &&
               read_u32(info->data, parameter_die + 13u) > 0u &&
               read_u32(info->data, parameter_die + 17u) > 0u);
        assert(line_table_file_matches(line, parameter_file,
                                       "tests/debug_info_inline.h"));
        if (strcmp(inline_name, "debug_local_inline") == 0) {
            uint64_t local_die = parameter_die + 21u;
            uint32_t local_name;
            uint32_t local_type;
            uint32_t local_file;
            assert(local_die + 22u <= info->size &&
                   info->data[local_die] == 32u);
            local_name = read_u32(info->data, local_die + 1u);
            local_type = read_u32(info->data, local_die + 5u);
            local_file = read_u32(info->data, local_die + 9u);
            assert(local_name < strings->size &&
                   strcmp((const char*)strings->data + local_name,
                          "inline_local_value") == 0);
            assert(local_type < info->size &&
                   info->data[local_type] == 20u);
            {
                uint32_t unqualified_type = read_u32(
                    info->data, (uint64_t)local_type + 1u);
                assert(unqualified_type < info->size &&
                       info->data[unqualified_type] == 5u);
            }
            assert(local_file > 0u &&
                   read_u32(info->data, local_die + 13u) > 0u &&
                   read_u32(info->data, local_die + 17u) > 0u);
            assert(line_table_file_matches(
                line, local_file, "tests/debug_info_inline.h"));
            assert(info->data[local_die + 21u] == 0u);
            inline_local_origin = local_die;
        } else if (strcmp(inline_name,
                          "debug_null_pointer_parameter_inline") == 0) {
            uint64_t second_parameter = parameter_die + 21u;
            uint32_t second_name = read_u32(info->data,
                                             second_parameter + 1u);
            uint32_t second_type = read_u32(info->data,
                                             second_parameter + 5u);
            assert(info->data[second_parameter] == 42u);
            assert(second_name < strings->size &&
                   strcmp((const char*)strings->data + second_name,
                          "other") == 0);
            assert(second_type < info->size &&
                   info->data[second_type] == 6u);
            assert(info->data[second_parameter + 21u] == 0u);
        } else {
            assert(info->data[parameter_die + 21u] == 0u);
        }

        low_pc_offset = die + 1u + 4u;
        cursor = low_pc_offset + address_size + 4u;
        call_file = read_u32(info->data, cursor);
        call_line = read_u32(info->data, cursor + 4u);
        call_column = read_u32(info->data, cursor + 8u);
        call_length = read_u32(info->data,
                               low_pc_offset + address_size);
        assert(call_length > 0u);
        assert(call_file > 0u && call_line > 0u && call_column > 0u);
        assert(line_table_file_matches(
            line, call_file,
            (strcmp(inline_name, "debug_declared_inline") == 0 ||
             strcmp(inline_name, "debug_local_inline") == 0 ||
             strcmp(inline_name, "debug_constant_inline") == 0 ||
             strcmp(inline_name,
                    "debug_unsigned_constant_inline") == 0 ||
             strcmp(inline_name, "debug_converted_signed_inline") == 0 ||
             strcmp(inline_name, "debug_converted_unsigned_inline") == 0 ||
             strcmp(inline_name, "debug_float_parameter_inline") == 0 ||
             strcmp(inline_name, "debug_double_parameter_inline") == 0 ||
             strcmp(inline_name,
                    "debug_float_to_double_parameter_inline") == 0 ||
             strcmp(inline_name,
                    "debug_double_to_float_parameter_inline") == 0 ||
             strcmp(inline_name,
                    "debug_float_expression_parameter_inline") == 0 ||
             strcmp(inline_name,
                    "debug_null_pointer_parameter_inline") == 0 ||
             strcmp(inline_name, "debug_enum_parameter_inline") == 0 ||
             strcmp(inline_name,
                    "debug_global_pointer_parameter_inline") == 0 ||
             strcmp(inline_name,
                    "debug_const_global_pointer_parameter_inline") == 0 ||
             strcmp(inline_name,
                    "debug_void_pointer_parameter_inline") == 0 ||
             strcmp(inline_name,
                    "debug_function_pointer_parameter_inline") == 0)
                ? "tests/debug_info.c"
                : "tests/debug_info_inline.h"));
        assert(line_table_file_matches(line, origin_file,
                                       "tests/debug_info_inline.h"));

        for (const ObjReloc* relocation = info->relocs; relocation;
             relocation = relocation->next) {
            bool root_call;
            if (relocation->offset != low_pc_offset) continue;
            assert(relocation->symbol_name != NULL);
            root_call =
                strcmp(relocation->symbol_name, "debug_inline_entry") == 0 ||
                strcmp(relocation->symbol_name,
                       "debug_inline_local_entry") == 0 ||
                strcmp(relocation->symbol_name,
                       "debug_inline_constant_entry") == 0 ||
                strcmp(relocation->symbol_name,
                       "debug_inline_unsigned_constant_entry") == 0 ||
                strcmp(relocation->symbol_name,
                       "debug_inline_converted_signed_entry") == 0 ||
                strcmp(relocation->symbol_name,
                       "debug_inline_unsigned_to_signed_entry") == 0 ||
                strcmp(relocation->symbol_name,
                       "debug_inline_converted_unsigned_entry") == 0 ||
                strcmp(relocation->symbol_name,
                       "debug_inline_float_constant_entry") == 0 ||
                strcmp(relocation->symbol_name,
                       "debug_inline_double_constant_entry") == 0 ||
                strcmp(relocation->symbol_name,
                       "debug_inline_float_to_double_constant_entry") == 0 ||
                strcmp(relocation->symbol_name,
                       "debug_inline_double_to_float_constant_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_float_expression_constant_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_nonconstant_float_entry") == 0 ||
                strcmp(relocation->symbol_name,
                       "debug_inline_nonconstant_double_entry") == 0 ||
                strcmp(relocation->symbol_name,
                       "debug_inline_nonconstant_float_identity_cast_entry") ==
                    0 ||
                strcmp(relocation->symbol_name,
                       "debug_inline_nonconstant_double_identity_cast_entry") ==
                    0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_null_pointer_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_enum_constant_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_global_pointer_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_address_constant_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_static_address_constant_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_readonly_address_constant_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_cast_address_constant_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_array_element_address_constant_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_array_computed_index_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_array_unsigned_wrap_index_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_array_computed_index_out_of_bounds_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_array_plus_constant_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_constant_plus_array_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_array_one_past_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_array_cast_constant_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_array_unsigned_modulo_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_object_one_past_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_array_out_of_bounds_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_array_interior_minus_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_array_interior_plus_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_array_before_begin_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_nested_array_interior_minus_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_nested_array_before_begin_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_member_address_constant_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_nested_array_address_constant_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_matrix_subarray_interior_plus_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_matrix_subarray_one_past_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_matrix_subarray_cross_row_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_function_address_constant_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_function_designator_constant_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_static_pointer_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_automatic_pointer_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_automatic_address_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_automatic_const_address_entry") == 0;
            root_call = root_call ||
                strcmp(relocation->symbol_name,
                       "debug_inline_automatic_void_address_entry") == 0;
            assert(root_call ||
                   (strcmp(relocation->symbol_name,
                           "debug_declared_inline") == 0 &&
                    strcmp(inline_name,
                           "debug_nested_inline") == 0));
            assert(relocation->addend >= 0);
            has_call_relocation = true;
            if (root_call &&
                strcmp(inline_name, "debug_declared_inline") == 0 &&
                outer_die == UINT64_MAX) {
                outer_die = die;
                outer_start = (uint64_t)relocation->addend;
                outer_end = outer_start + call_length;
            } else if (root_call &&
                       strcmp(inline_name, "debug_nested_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_entry") == 0 &&
                       inner_die == UINT64_MAX) {
                inner_die = die;
                inner_start = (uint64_t)relocation->addend;
                inner_end = inner_start + call_length;
            } else if (root_call &&
                       strcmp(inline_name, "debug_local_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_local_entry") == 0 &&
                       inline_local_call == UINT64_MAX) {
                inline_local_call = die;
            } else if (root_call &&
                       strcmp(inline_name, "debug_constant_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_constant_entry") == 0 &&
                       constant_inline_call == UINT64_MAX) {
                constant_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_unsigned_constant_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_unsigned_constant_entry") == 0 &&
                       unsigned_constant_inline_call == UINT64_MAX) {
                unsigned_constant_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_converted_signed_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_converted_signed_entry") == 0 &&
                       converted_signed_inline_call == UINT64_MAX) {
                converted_signed_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_converted_signed_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_unsigned_to_signed_entry") == 0 &&
                       unsigned_to_signed_inline_call == UINT64_MAX) {
                unsigned_to_signed_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_converted_unsigned_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_converted_unsigned_entry") == 0 &&
                       converted_unsigned_inline_call == UINT64_MAX) {
                converted_unsigned_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_float_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_float_constant_entry") == 0 &&
                       float_inline_call == UINT64_MAX) {
                float_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_double_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_double_constant_entry") == 0 &&
                       double_inline_call == UINT64_MAX) {
                double_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_float_to_double_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_float_to_double_constant_entry") == 0 &&
                       float_to_double_inline_call == UINT64_MAX) {
                float_to_double_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_double_to_float_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_double_to_float_constant_entry") == 0 &&
                       double_to_float_inline_call == UINT64_MAX) {
                double_to_float_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_float_expression_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_float_expression_constant_entry") == 0 &&
                       float_expression_inline_call == UINT64_MAX) {
                float_expression_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_null_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_null_pointer_entry") == 0 &&
                       null_pointer_inline_call == UINT64_MAX) {
                null_pointer_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_enum_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_enum_constant_entry") == 0 &&
                       enum_inline_call == UINT64_MAX) {
                enum_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_global_pointer_entry") == 0 &&
                       global_pointer_inline_call == UINT64_MAX) {
                global_pointer_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_address_constant_entry") == 0 &&
                       address_constant_inline_call == UINT64_MAX) {
                address_constant_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_static_address_constant_entry") == 0 &&
                       static_address_constant_inline_call == UINT64_MAX) {
                static_address_constant_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_const_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_readonly_address_constant_entry") == 0 &&
                       readonly_address_constant_inline_call == UINT64_MAX) {
                readonly_address_constant_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_void_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_cast_address_constant_entry") == 0 &&
                       cast_address_constant_inline_call == UINT64_MAX) {
                cast_address_constant_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_array_element_address_constant_entry") == 0 &&
                       array_element_address_constant_inline_call == UINT64_MAX) {
                array_element_address_constant_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_array_computed_index_entry") == 0 &&
                       array_computed_index_inline_call == UINT64_MAX) {
                array_computed_index_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_array_unsigned_wrap_index_entry") == 0 &&
                       array_unsigned_wrap_index_inline_call == UINT64_MAX) {
                array_unsigned_wrap_index_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_array_computed_index_out_of_bounds_entry") == 0 &&
                       array_computed_index_out_of_bounds_inline_call == UINT64_MAX) {
                array_computed_index_out_of_bounds_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_array_plus_constant_entry") == 0 &&
                       array_plus_constant_inline_call == UINT64_MAX) {
                array_plus_constant_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_constant_plus_array_entry") == 0 &&
                       constant_plus_array_inline_call == UINT64_MAX) {
                constant_plus_array_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_array_one_past_entry") == 0 &&
                       array_one_past_inline_call == UINT64_MAX) {
                array_one_past_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_array_cast_constant_entry") == 0 &&
                       array_cast_constant_inline_call == UINT64_MAX) {
                array_cast_constant_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_array_unsigned_modulo_entry") == 0 &&
                       array_unsigned_modulo_inline_call == UINT64_MAX) {
                array_unsigned_modulo_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_object_one_past_entry") == 0 &&
                       object_one_past_inline_call == UINT64_MAX) {
                object_one_past_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_array_out_of_bounds_entry") == 0 &&
                       array_out_of_bounds_inline_call == UINT64_MAX) {
                array_out_of_bounds_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_array_interior_minus_entry") == 0 &&
                       array_interior_minus_inline_call == UINT64_MAX) {
                array_interior_minus_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_array_interior_plus_entry") == 0 &&
                       array_interior_plus_inline_call == UINT64_MAX) {
                array_interior_plus_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_array_before_begin_entry") == 0 &&
                       array_before_begin_inline_call == UINT64_MAX) {
                array_before_begin_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_nested_array_interior_minus_entry") == 0 &&
                       nested_array_interior_minus_inline_call == UINT64_MAX) {
                nested_array_interior_minus_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_nested_array_before_begin_entry") == 0 &&
                       nested_array_before_begin_inline_call == UINT64_MAX) {
                nested_array_before_begin_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_member_address_constant_entry") == 0 &&
                       member_address_constant_inline_call == UINT64_MAX) {
                member_address_constant_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_nested_array_address_constant_entry") == 0 &&
                       nested_array_address_constant_inline_call == UINT64_MAX) {
                nested_array_address_constant_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_matrix_subarray_interior_plus_entry") == 0 &&
                       matrix_subarray_interior_plus_inline_call == UINT64_MAX) {
                matrix_subarray_interior_plus_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_matrix_subarray_one_past_entry") == 0 &&
                       matrix_subarray_one_past_inline_call == UINT64_MAX) {
                matrix_subarray_one_past_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_matrix_subarray_cross_row_entry") == 0 &&
                       matrix_subarray_cross_row_inline_call == UINT64_MAX) {
                matrix_subarray_cross_row_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_function_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_function_address_constant_entry") == 0 &&
                       function_address_constant_inline_call == UINT64_MAX) {
                function_address_constant_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_function_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_function_designator_constant_entry") == 0 &&
                       function_designator_inline_call == UINT64_MAX) {
                function_designator_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_static_pointer_entry") == 0 &&
                       static_pointer_inline_call == UINT64_MAX) {
                static_pointer_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_automatic_pointer_entry") == 0 &&
                       automatic_pointer_inline_call == UINT64_MAX) {
                automatic_pointer_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_automatic_address_entry") == 0 &&
                       automatic_address_inline_call == UINT64_MAX) {
                automatic_address_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_const_global_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_automatic_const_address_entry") == 0 &&
                       automatic_const_address_inline_call == UINT64_MAX) {
                automatic_const_address_inline_call = die;
            } else if (root_call &&
                       strcmp(inline_name,
                              "debug_void_pointer_parameter_inline") == 0 &&
                       strcmp(relocation->symbol_name,
                              "debug_inline_automatic_void_address_entry") == 0 &&
                       automatic_void_address_inline_call == UINT64_MAX) {
                automatic_void_address_inline_call = die;
            }
            break;
        }
        assert(has_call_relocation);
    }
    assert(outer_die != UINT64_MAX);
    assert(inner_die != UINT64_MAX);
    assert(outer_start <= inner_start && inner_end <= outer_end &&
           inner_end < outer_end);
    assert(inline_local_call != UINT64_MAX);
    assert(inline_local_origin != UINT64_MAX);
    assert(constant_inline_call != UINT64_MAX);
    assert(unsigned_constant_inline_call != UINT64_MAX);
    assert(converted_signed_inline_call != UINT64_MAX);
    assert(unsigned_to_signed_inline_call != UINT64_MAX);
    assert(converted_unsigned_inline_call != UINT64_MAX);
    assert(float_inline_call != UINT64_MAX);
    assert(double_inline_call != UINT64_MAX);
    assert(float_to_double_inline_call != UINT64_MAX);
    assert(double_to_float_inline_call != UINT64_MAX);
    assert(float_expression_inline_call != UINT64_MAX);
    assert(null_pointer_inline_call != UINT64_MAX);
    assert(enum_inline_call != UINT64_MAX);
    assert(global_pointer_inline_call != UINT64_MAX);
    assert(address_constant_inline_call != UINT64_MAX);
    assert(static_address_constant_inline_call != UINT64_MAX);
    assert(readonly_address_constant_inline_call != UINT64_MAX);
    assert(cast_address_constant_inline_call != UINT64_MAX);
    assert(array_element_address_constant_inline_call != UINT64_MAX);
    assert(array_computed_index_inline_call != UINT64_MAX);
    assert(array_unsigned_wrap_index_inline_call != UINT64_MAX);
    assert(array_computed_index_out_of_bounds_inline_call != UINT64_MAX);
    assert(array_plus_constant_inline_call != UINT64_MAX);
    assert(constant_plus_array_inline_call != UINT64_MAX);
    assert(array_one_past_inline_call != UINT64_MAX);
    assert(array_cast_constant_inline_call != UINT64_MAX);
    assert(array_unsigned_modulo_inline_call != UINT64_MAX);
    assert(array_interior_minus_inline_call != UINT64_MAX);
    assert(array_interior_plus_inline_call != UINT64_MAX);
    assert(nested_array_interior_minus_inline_call != UINT64_MAX);
    assert(object_one_past_inline_call != UINT64_MAX);
    assert(array_out_of_bounds_inline_call != UINT64_MAX);
    assert(array_before_begin_inline_call != UINT64_MAX);
    assert(nested_array_before_begin_inline_call != UINT64_MAX);
    assert(member_address_constant_inline_call != UINT64_MAX);
    assert(nested_array_address_constant_inline_call != UINT64_MAX);
    assert(matrix_subarray_interior_plus_inline_call != UINT64_MAX);
    assert(matrix_subarray_one_past_inline_call != UINT64_MAX);
    assert(matrix_subarray_cross_row_inline_call != UINT64_MAX);
    assert(function_address_constant_inline_call != UINT64_MAX);
    assert(function_designator_inline_call != UINT64_MAX);
    assert(static_pointer_inline_call != UINT64_MAX);
    assert(automatic_pointer_inline_call != UINT64_MAX);
    assert(automatic_address_inline_call != UINT64_MAX);
    assert(automatic_const_address_inline_call != UINT64_MAX);
    assert(automatic_void_address_inline_call != UINT64_MAX);
    {
        uint64_t child = outer_die + 1u + 4u + address_size + 4u + 12u;
        while (child < inner_die) {
            verify_inline_frame_parameter_die(info, &child, architecture);
        }
        assert(child == inner_die);
    }
    {
        uint64_t child =
            inner_die + 1u + 4u + address_size + 4u + 12u;
        verify_inline_frame_parameter_die(info, &child, architecture);
        assert(child + 1u < info->size && info->data[child] == 0u &&
               info->data[child + 1u] == 0u);
    }
    {
        uint64_t concrete_local =
            inline_local_call + 1u + 4u + address_size + 4u + 12u;
        uint64_t value_offset = concrete_local + 5u;
        assert(info->data[concrete_local] == 60u);
        assert(read_u32(info->data, concrete_local + 1u) ==
               inline_local_origin);
        assert(read_sleb(info->data, info->size, &value_offset) == 7);
        assert(info->data[value_offset++] == 0u);
        assert(info->data[value_offset] == 0u);
    }
    {
        uint64_t concrete_parameter = constant_inline_call + 1u + 4u +
            address_size + 4u + 12u;
        uint64_t value_offset = concrete_parameter + 5u;
        uint64_t origin_parameter =
            read_u32(info->data, constant_inline_call + 1u);
        assert(origin_parameter < info->size &&
               info->data[origin_parameter] == 40u);
        origin_parameter += 27u;
        assert(info->data[concrete_parameter] == 63u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        assert(read_sleb(info->data, info->size, &value_offset) == 23);
        assert(info->data[value_offset] == 0u);
    }
    {
        uint64_t concrete_parameter = unsigned_constant_inline_call + 1u +
            4u + address_size + 4u + 12u;
        uint64_t value_offset = concrete_parameter + 5u;
        uint64_t origin_parameter =
            read_u32(info->data, unsigned_constant_inline_call + 1u) + 27u;
        assert(info->data[concrete_parameter] == 64u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        assert(read_uleb(info->data, info->size, &value_offset) ==
               UINT64_MAX);
        assert(info->data[value_offset] == 0u);
    }
    {
        uint64_t concrete_parameter = converted_signed_inline_call + 1u +
            4u + address_size + 4u + 12u;
        uint64_t value_offset = concrete_parameter + 5u;
        assert(info->data[concrete_parameter] == 63u);
        assert(read_sleb(info->data, info->size, &value_offset) == 23);
        assert(info->data[value_offset] == 0u);
    }
    {
        uint64_t concrete_parameter = unsigned_to_signed_inline_call + 1u +
            4u + address_size + 4u + 12u;
        uint64_t value_offset = concrete_parameter + 5u;
        assert(info->data[concrete_parameter] == 63u);
        assert(read_sleb(info->data, info->size, &value_offset) == -1);
        assert(info->data[value_offset] == 0u);
    }
    {
        uint64_t concrete_parameter = converted_unsigned_inline_call + 1u +
            4u + address_size + 4u + 12u;
        uint64_t value_offset = concrete_parameter + 5u;
        assert(info->data[concrete_parameter] == 64u);
        assert(read_uleb(info->data, info->size, &value_offset) ==
               UINT32_MAX);
        assert(info->data[value_offset] == 0u);
    }
    {
        uint64_t concrete_parameter = float_inline_call + 1u + 4u +
            address_size + 4u + 12u;
        uint64_t origin_parameter =
            read_u32(info->data, float_inline_call + 1u) + 27u;
        assert(info->data[concrete_parameter] == 65u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        assert(info->data[concrete_parameter + 5u] == 4u);
        assert(info->data[concrete_parameter + 6u] == 0x00u);
        assert(info->data[concrete_parameter + 7u] == 0x00u);
        assert(info->data[concrete_parameter + 8u] == 0xc0u);
        assert(info->data[concrete_parameter + 9u] == 0x3fu);
        assert(info->data[concrete_parameter + 10u] == 0u);
    }
    {
        uint64_t concrete_parameter = double_inline_call + 1u + 4u +
            address_size + 4u + 12u;
        uint64_t origin_parameter =
            read_u32(info->data, double_inline_call + 1u) + 27u;
        assert(info->data[concrete_parameter] == 65u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        assert(info->data[concrete_parameter + 5u] == 8u);
        assert(info->data[concrete_parameter + 6u] == 0x00u);
        assert(info->data[concrete_parameter + 7u] == 0x00u);
        assert(info->data[concrete_parameter + 8u] == 0x00u);
        assert(info->data[concrete_parameter + 9u] == 0x00u);
        assert(info->data[concrete_parameter + 10u] == 0x00u);
        assert(info->data[concrete_parameter + 11u] == 0x00u);
        assert(info->data[concrete_parameter + 12u] == 0x02u);
        assert(info->data[concrete_parameter + 13u] == 0x40u);
        assert(info->data[concrete_parameter + 14u] == 0u);
    }
    {
        uint64_t concrete_parameter = float_to_double_inline_call + 1u + 4u +
            address_size + 4u + 12u;
        uint64_t origin_parameter =
            read_u32(info->data, float_to_double_inline_call + 1u) + 27u;
        assert(info->data[concrete_parameter] == 65u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        assert(info->data[concrete_parameter + 5u] == 8u);
        assert(info->data[concrete_parameter + 6u] == 0x00u);
        assert(info->data[concrete_parameter + 7u] == 0x00u);
        assert(info->data[concrete_parameter + 8u] == 0x00u);
        assert(info->data[concrete_parameter + 9u] == 0xa0u);
        assert(info->data[concrete_parameter + 10u] == 0x99u);
        assert(info->data[concrete_parameter + 11u] == 0x99u);
        assert(info->data[concrete_parameter + 12u] == 0xf1u);
        assert(info->data[concrete_parameter + 13u] == 0x3fu);
        assert(info->data[concrete_parameter + 14u] == 0u);
    }
    {
        uint64_t concrete_parameter = double_to_float_inline_call + 1u + 4u +
            address_size + 4u + 12u;
        uint64_t origin_parameter =
            read_u32(info->data, double_to_float_inline_call + 1u) + 27u;
        assert(info->data[concrete_parameter] == 65u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        assert(info->data[concrete_parameter + 5u] == 4u);
        assert(info->data[concrete_parameter + 6u] == 0xcdu);
        assert(info->data[concrete_parameter + 7u] == 0xccu);
        assert(info->data[concrete_parameter + 8u] == 0x8cu);
        assert(info->data[concrete_parameter + 9u] == 0x3fu);
        assert(info->data[concrete_parameter + 10u] == 0u);
    }
    {
        uint64_t concrete_parameter = float_expression_inline_call + 1u + 4u +
            address_size + 4u + 12u;
        uint64_t origin_parameter =
            read_u32(info->data, float_expression_inline_call + 1u) + 27u;
        assert(info->data[concrete_parameter] == 65u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        assert(info->data[concrete_parameter + 5u] == 8u);
        assert(info->data[concrete_parameter + 6u] == 0x00u);
        assert(info->data[concrete_parameter + 7u] == 0x00u);
        assert(info->data[concrete_parameter + 8u] == 0x00u);
        assert(info->data[concrete_parameter + 9u] == 0x80u);
        assert(info->data[concrete_parameter + 10u] == 0x66u);
        assert(info->data[concrete_parameter + 11u] == 0x66u);
        assert(info->data[concrete_parameter + 12u] == 0x0au);
        assert(info->data[concrete_parameter + 13u] == 0x40u);
        assert(info->data[concrete_parameter + 14u] == 0u);
    }
    {
        uint64_t concrete_parameter = null_pointer_inline_call + 1u + 4u +
            address_size + 4u + 12u;
        uint64_t origin_parameter =
            read_u32(info->data, null_pointer_inline_call + 1u) + 27u;
        uint64_t second_parameter;
        assert(info->data[concrete_parameter] == 66u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        for (uint64_t byte = 0u; byte < address_size; ++byte) {
            assert(info->data[concrete_parameter + 5u + byte] == 0u);
        }
        second_parameter = concrete_parameter + 5u + address_size;
        verify_inline_frame_parameter_die(info, &second_parameter,
                                          architecture);
        assert(second_parameter + 1u < info->size &&
               info->data[second_parameter] == 0u &&
               info->data[second_parameter + 1u] == 0u);
    }
    {
        uint64_t concrete_parameter = enum_inline_call + 1u + 4u +
            address_size + 4u + 12u;
        uint64_t origin_parameter =
            read_u32(info->data, enum_inline_call + 1u) + 27u;
        uint64_t value_offset = concrete_parameter + 5u;
        assert(info->data[concrete_parameter] == 63u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        assert(read_sleb(info->data, info->size, &value_offset) == -2);
        assert(info->data[value_offset] == 0u);
    }
    {
        uint64_t concrete_parameter = global_pointer_inline_call + 1u + 4u +
            address_size + 4u + 12u;
        uint32_t origin_parameter = read_u32(
            info->data, global_pointer_inline_call + 1u) + 27u;
        uint64_t expression_offset = concrete_parameter + 5u;
        uint64_t expression_size = read_uleb(
            info->data, info->size, &expression_offset);
        bool found_source_relocation = false;
        assert(info->data[concrete_parameter] == 67u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        assert(expression_size == address_size + 1u);
        assert(info->data[expression_offset] == 0x03u);
        for (const ObjReloc* relocation = info->relocs; relocation;
             relocation = relocation->next) {
            if (relocation->offset == expression_offset + 1u &&
                relocation->type == (architecture == ARCH_X64
                    ? RELOC_ABS64 : RELOC_ABS32U) &&
                relocation->symbol_name &&
                strcmp(relocation->symbol_name,
                       "debug_inline_global_pointer_source") == 0) {
                found_source_relocation = true;
                break;
            }
        }
        assert(found_source_relocation);
    }
    {
        uint64_t concrete_parameter = static_pointer_inline_call + 1u + 4u +
            address_size + 4u + 12u;
        uint32_t origin_parameter = read_u32(
            info->data, static_pointer_inline_call + 1u) + 27u;
        uint64_t expression_offset = concrete_parameter + 5u;
        uint64_t expression_size = read_uleb(
            info->data, info->size, &expression_offset);
        bool found_source_relocation = false;
        assert(info->data[concrete_parameter] == 67u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        assert(expression_size == address_size + 1u);
        assert(info->data[expression_offset] == 0x03u);
        for (const ObjReloc* relocation = info->relocs; relocation;
             relocation = relocation->next) {
            if (relocation->offset == expression_offset + 1u &&
                relocation->type == (architecture == ARCH_X64
                    ? RELOC_ABS64 : RELOC_ABS32U) &&
                relocation->symbol_name &&
                strncmp(relocation->symbol_name, "tests/debug_info.c::",
                        strlen("tests/debug_info.c::")) == 0 &&
                strstr(relocation->symbol_name,
                       "debug_inline_static_pointer_source") != NULL) {
                found_source_relocation = true;
                break;
            }
        }
        assert(found_source_relocation);
    }
    {
        uint64_t concrete_parameter = address_constant_inline_call + 1u + 4u +
            address_size + 4u + 12u;
        uint32_t origin_parameter = read_u32(
            info->data, address_constant_inline_call + 1u) + 27u;
        uint64_t address_offset = concrete_parameter + 5u;
        bool found_address_relocation = false;
        assert(info->data[concrete_parameter] == 68u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        for (uint64_t byte = 0u; byte < address_size; ++byte) {
            assert(info->data[address_offset + byte] == 0u);
        }
        for (const ObjReloc* relocation = info->relocs; relocation;
             relocation = relocation->next) {
            if (relocation->offset == address_offset &&
                relocation->type == (architecture == ARCH_X64
                    ? RELOC_ABS64 : RELOC_ABS32U) &&
                relocation->symbol_name &&
                strcmp(relocation->symbol_name,
                       "debug_inline_addressed_pointer_source") == 0) {
                found_address_relocation = true;
                break;
            }
        }
        assert(found_address_relocation);
    }
    {
        uint64_t concrete_parameter = static_address_constant_inline_call +
            1u + 4u + address_size + 4u + 12u;
        uint32_t origin_parameter = read_u32(
            info->data, static_address_constant_inline_call + 1u) + 27u;
        uint64_t address_offset = concrete_parameter + 5u;
        bool found_address_relocation = false;
        assert(info->data[concrete_parameter] == 68u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        for (uint64_t byte = 0u; byte < address_size; ++byte) {
            assert(info->data[address_offset + byte] == 0u);
        }
        for (const ObjReloc* relocation = info->relocs; relocation;
             relocation = relocation->next) {
            if (relocation->offset == address_offset &&
                relocation->type == (architecture == ARCH_X64
                    ? RELOC_ABS64 : RELOC_ABS32U) &&
                relocation->symbol_name &&
                strncmp(relocation->symbol_name, "tests/debug_info.c::",
                        strlen("tests/debug_info.c::")) == 0 &&
                strstr(relocation->symbol_name,
                       "debug_inline_addressed_static_source") != NULL) {
                found_address_relocation = true;
                break;
            }
        }
        assert(found_address_relocation);
    }
    {
        uint64_t concrete_parameter = readonly_address_constant_inline_call +
            1u + 4u + address_size + 4u + 12u;
        uint32_t origin_parameter = read_u32(
            info->data, readonly_address_constant_inline_call + 1u) + 27u;
        uint64_t address_offset = concrete_parameter + 5u;
        bool found_address_relocation = false;
        assert(info->data[concrete_parameter] == 68u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        for (uint64_t byte = 0u; byte < address_size; ++byte) {
            assert(info->data[address_offset + byte] == 0u);
        }
        for (const ObjReloc* relocation = info->relocs; relocation;
             relocation = relocation->next) {
            if (relocation->offset == address_offset &&
                relocation->type == (architecture == ARCH_X64
                    ? RELOC_ABS64 : RELOC_ABS32U) &&
                relocation->symbol_name &&
                strcmp(relocation->symbol_name,
                       "debug_inline_readonly_addressed_pointer_source") == 0) {
                found_address_relocation = true;
                break;
            }
        }
        assert(found_address_relocation);
    }
    {
        uint64_t concrete_parameter = cast_address_constant_inline_call +
            1u + 4u + address_size + 4u + 12u;
        uint32_t origin_parameter = read_u32(
            info->data, cast_address_constant_inline_call + 1u) + 27u;
        uint64_t address_offset = concrete_parameter + 5u;
        bool found_address_relocation = false;
        assert(info->data[concrete_parameter] == 68u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        for (uint64_t byte = 0u; byte < address_size; ++byte) {
            assert(info->data[address_offset + byte] == 0u);
        }
        for (const ObjReloc* relocation = info->relocs; relocation;
             relocation = relocation->next) {
            if (relocation->offset == address_offset &&
                relocation->type == (architecture == ARCH_X64
                    ? RELOC_ABS64 : RELOC_ABS32U) &&
                relocation->symbol_name &&
                strcmp(relocation->symbol_name,
                       "debug_inline_addressed_pointer_source") == 0) {
                found_address_relocation = true;
                break;
            }
        }
        assert(found_address_relocation);
    }
    {
        uint64_t concrete_parameter =
            array_element_address_constant_inline_call +
            1u + 4u + address_size + 4u + 12u;
        uint32_t origin_parameter = read_u32(
            info->data, array_element_address_constant_inline_call + 1u) + 27u;
        uint64_t address_offset = concrete_parameter + 5u;
        bool found_address_relocation = false;
        assert(info->data[concrete_parameter] == 68u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        for (uint64_t byte = 0u; byte < address_size; ++byte) {
            assert(info->data[address_offset + byte] == 0u);
        }
        for (const ObjReloc* relocation = info->relocs; relocation;
             relocation = relocation->next) {
            if (relocation->offset == address_offset &&
                relocation->type == (architecture == ARCH_X64
                    ? RELOC_ABS64 : RELOC_ABS32U) &&
                relocation->addend == 8 && relocation->symbol_name &&
                strcmp(relocation->symbol_name,
                       "debug_inline_addressed_array_source") == 0) {
                found_address_relocation = true;
                break;
            }
        }
        assert(found_address_relocation);
    }
    {
        uint64_t concrete_parameter = member_address_constant_inline_call +
            1u + 4u + address_size + 4u + 12u;
        uint32_t origin_parameter = read_u32(
            info->data, member_address_constant_inline_call + 1u) + 27u;
        uint64_t address_offset = concrete_parameter + 5u;
        bool found_address_relocation = false;
        assert(info->data[concrete_parameter] == 68u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        for (uint64_t byte = 0u; byte < address_size; ++byte) {
            assert(info->data[address_offset + byte] == 0u);
        }
        for (const ObjReloc* relocation = info->relocs; relocation;
             relocation = relocation->next) {
            if (relocation->offset == address_offset &&
                relocation->type == (architecture == ARCH_X64
                    ? RELOC_ABS64 : RELOC_ABS32U) &&
                relocation->addend == 4 && relocation->symbol_name &&
                strcmp(relocation->symbol_name,
                       "debug_inline_addressed_member_source") == 0) {
                found_address_relocation = true;
                break;
            }
        }
        assert(found_address_relocation);
    }
    {
        uint64_t concrete_parameter =
            nested_array_address_constant_inline_call +
            1u + 4u + address_size + 4u + 12u;
        uint32_t origin_parameter = read_u32(
            info->data, nested_array_address_constant_inline_call + 1u) + 27u;
        uint64_t address_offset = concrete_parameter + 5u;
        bool found_address_relocation = false;
        assert(info->data[concrete_parameter] == 68u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        for (uint64_t byte = 0u; byte < address_size; ++byte) {
            assert(info->data[address_offset + byte] == 0u);
        }
        for (const ObjReloc* relocation = info->relocs; relocation;
             relocation = relocation->next) {
            if (relocation->offset == address_offset &&
                relocation->type == (architecture == ARCH_X64
                    ? RELOC_ABS64 : RELOC_ABS32U) &&
                relocation->addend == 8 && relocation->symbol_name &&
                strcmp(relocation->symbol_name,
                       "debug_inline_addressed_nested_source") == 0) {
                found_address_relocation = true;
                break;
            }
        }
        assert(found_address_relocation);
    }
    {
        uint64_t concrete_parameter = function_address_constant_inline_call +
            1u + 4u + address_size + 4u + 12u;
        uint32_t origin_parameter = read_u32(
            info->data, function_address_constant_inline_call + 1u) + 27u;
        uint64_t address_offset = concrete_parameter + 5u;
        bool found_address_relocation = false;
        assert(info->data[concrete_parameter] == 68u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        for (uint64_t byte = 0u; byte < address_size; ++byte) {
            assert(info->data[address_offset + byte] == 0u);
        }
        for (const ObjReloc* relocation = info->relocs; relocation;
             relocation = relocation->next) {
            if (relocation->offset == address_offset &&
                relocation->type == (architecture == ARCH_X64
                    ? RELOC_ABS64 : RELOC_ABS32U) &&
                relocation->symbol_name &&
                strncmp(relocation->symbol_name, "tests/debug_info.c::",
                        strlen("tests/debug_info.c::")) == 0 &&
                strstr(relocation->symbol_name,
                       "debug_inline_addressed_function_target") != NULL) {
                found_address_relocation = true;
                break;
            }
        }
        assert(found_address_relocation);
    }
    {
        uint64_t concrete_parameter = function_designator_inline_call +
            1u + 4u + address_size + 4u + 12u;
        uint32_t origin_parameter = read_u32(
            info->data, function_designator_inline_call + 1u) + 27u;
        uint64_t address_offset = concrete_parameter + 5u;
        bool found_address_relocation = false;
        assert(info->data[concrete_parameter] == 68u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        for (uint64_t byte = 0u; byte < address_size; ++byte) {
            assert(info->data[address_offset + byte] == 0u);
        }
        for (const ObjReloc* relocation = info->relocs; relocation;
             relocation = relocation->next) {
            if (relocation->offset == address_offset &&
                relocation->type == (architecture == ARCH_X64
                    ? RELOC_ABS64 : RELOC_ABS32U) &&
                relocation->symbol_name &&
                strncmp(relocation->symbol_name, "tests/debug_info.c::",
                        strlen("tests/debug_info.c::")) == 0 &&
                strstr(relocation->symbol_name,
                       "debug_inline_addressed_function_target") != NULL) {
                found_address_relocation = true;
                break;
            }
        }
        assert(found_address_relocation);
    }
    {
        uint64_t concrete_parameter = automatic_pointer_inline_call + 1u + 4u +
            address_size + 4u + 12u;
        uint32_t origin_parameter = read_u32(
            info->data, automatic_pointer_inline_call + 1u) + 27u;
        uint64_t expression_offset = concrete_parameter + 5u;
        uint64_t expression_size = read_uleb(
            info->data, info->size, &expression_offset);
        uint64_t expression_end;
        int64_t frame_offset;
        int64_t source_frame_offset;
        bool source_has_location;
        bool source_is_location_list;
        assert(info->data[concrete_parameter] == 67u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        assert(expression_size >= 2u);
        assert(info->data[expression_offset] ==
               (architecture == ARCH_X64 ? 0x76u : 0x75u));
        expression_end = expression_offset + expression_size;
        ++expression_offset;
        frame_offset = read_sleb(info->data, info->size, &expression_offset);
        assert(expression_offset == expression_end && frame_offset != 0);
        assert(find_variable_location(
            info, strings, locations,
            "debug_inline_automatic_pointer_source", architecture,
            &source_has_location, &source_is_location_list,
            &source_frame_offset));
        assert(source_has_location && source_frame_offset == frame_offset);
    }
    verify_inline_parameter_frame_address(
        info, strings, locations, automatic_address_inline_call,
        architecture, "debug_inline_automatic_address_source", 0, false);
    verify_inline_parameter_frame_address(
        info, strings, locations, automatic_const_address_inline_call,
        architecture, "debug_inline_automatic_const_address_source", 0,
        false);
    verify_inline_parameter_frame_address(
        info, strings, locations, automatic_void_address_inline_call,
        architecture, "debug_inline_automatic_void_address_source", 0,
        false);
    verify_inline_parameter_address_constant(
        info, array_plus_constant_inline_call, architecture,
        "debug_inline_addressed_array_source", 12);
    verify_inline_parameter_address_constant(
        info, array_computed_index_inline_call, architecture,
        "debug_inline_addressed_array_source", 8);
    verify_inline_parameter_address_constant(
        info, array_unsigned_wrap_index_inline_call, architecture,
        "debug_inline_addressed_array_source", 0);
    verify_inline_parameter_address_constant(
        info, constant_plus_array_inline_call, architecture,
        "debug_inline_addressed_array_source", 4);
    verify_inline_parameter_address_constant(
        info, array_one_past_inline_call, architecture,
        "debug_inline_addressed_array_source", 16);
    verify_inline_parameter_address_constant(
        info, array_cast_constant_inline_call, architecture,
        "debug_inline_addressed_array_source", 16);
    verify_inline_parameter_address_constant(
        info, array_unsigned_modulo_inline_call, architecture,
        "debug_inline_addressed_array_source", 8);
    verify_inline_parameter_address_constant(
        info, array_interior_minus_inline_call, architecture,
        "debug_inline_addressed_array_source", 4);
    verify_inline_parameter_address_constant(
        info, array_interior_plus_inline_call, architecture,
        "debug_inline_addressed_array_source", 12);
    verify_inline_parameter_address_constant(
        info, nested_array_interior_minus_inline_call, architecture,
        "debug_inline_addressed_nested_source", 8);
    verify_inline_parameter_address_constant(
        info, object_one_past_inline_call, architecture,
        "debug_inline_addressed_pointer_source", 4);
    verify_inline_parameter_is_not_attributed(
        info, array_out_of_bounds_inline_call, architecture);
    verify_inline_parameter_is_not_attributed(
        info, array_computed_index_out_of_bounds_inline_call, architecture);
    verify_inline_parameter_is_not_attributed(
        info, array_before_begin_inline_call, architecture);
    verify_inline_parameter_is_not_attributed(
        info, nested_array_before_begin_inline_call, architecture);
    verify_inline_parameter_address_constant(
        info, matrix_subarray_interior_plus_inline_call, architecture,
        "debug_inline_addressed_matrix_source", 20);
    verify_inline_parameter_address_constant(
        info, matrix_subarray_one_past_inline_call, architecture,
        "debug_inline_addressed_matrix_source", 24);
    verify_inline_parameter_is_not_attributed(
        info, matrix_subarray_cross_row_inline_call, architecture);
    objfile_free(object);
}

static void verify_cxx_inline_nullptr_parameter(const char* path,
                                                uint16_t architecture)
{
    static const uint8_t pointer_parameter_attributes[] = {
        0x31u, 0x13u, 0x1cu, 0x01u
    };
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    ObjSection* line;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    bool found = false;

    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    line = objfile_get_section(object, ".debug_line");
    assert(info != NULL && abbrev != NULL && strings != NULL && line != NULL);
    verify_debug_abbreviation(abbrev, 66u, 0x05u, 0u,
                              pointer_parameter_attributes,
                              sizeof(pointer_parameter_attributes));

    for (uint64_t die = 11u;
         die + 1u + 4u + address_size + 4u + 12u <= info->size; ++die) {
        uint32_t origin;
        uint32_t name;
        uint64_t first_parameter;
        uint64_t second_parameter;
        uint64_t low_pc_offset;
        uint64_t concrete_parameter;
        uint64_t call_length;
        uint32_t value_type;
        uint32_t other_type;
        uint32_t call_file;
        uint32_t call_line;
        uint32_t call_column;
        bool has_host_relocation = false;

        if (info->data[die] != 41u) continue;
        origin = read_u32(info->data, die + 1u);
        if ((uint64_t)origin + 69u > info->size ||
            info->data[origin] != 40u) continue;
        name = read_u32(info->data, (uint64_t)origin + 1u);
        if (name >= strings->size ||
            strcmp((const char*)strings->data + name,
                   "debug_nullptr_parameter_inline") != 0) continue;

        first_parameter = (uint64_t)origin + 27u;
        second_parameter = first_parameter + 21u;
        assert(info->data[first_parameter] == 42u &&
               info->data[second_parameter] == 42u);
        name = read_u32(info->data, first_parameter + 1u);
        assert(name < strings->size &&
               strcmp((const char*)strings->data + name, "value") == 0);
        value_type = read_u32(info->data, first_parameter + 5u);
        assert(value_type < info->size && info->data[value_type] == 6u);
        name = read_u32(info->data, second_parameter + 1u);
        assert(name < strings->size &&
               strcmp((const char*)strings->data + name, "other") == 0);
        other_type = read_u32(info->data, second_parameter + 5u);
        assert(other_type < info->size && info->data[other_type] == 6u);

        low_pc_offset = die + 1u + 4u;
        call_length = read_u32(info->data, low_pc_offset + address_size);
        call_file = read_u32(info->data,
                             low_pc_offset + address_size + 4u);
        call_line = read_u32(info->data,
                             low_pc_offset + address_size + 8u);
        call_column = read_u32(info->data,
                               low_pc_offset + address_size + 12u);
        assert(call_length != 0u && call_file != 0u && call_line != 0u &&
               call_column != 0u);
        assert(line_table_file_matches(line, call_file,
                                       "tests/debug_info_cxx_member.cpp"));
        for (const ObjReloc* relocation = info->relocs;
             relocation; relocation = relocation->next) {
            if (relocation->offset != low_pc_offset) continue;
            assert(relocation->symbol_name != NULL &&
                   strcmp(relocation->symbol_name,
                          "debug_nullptr_parameter_entry") == 0);
            has_host_relocation = true;
            break;
        }
        assert(has_host_relocation);

        concrete_parameter = die + 1u + 4u + address_size + 4u + 12u;
        assert(info->data[concrete_parameter] == 66u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               first_parameter);
        for (uint64_t byte = 0u; byte < address_size; ++byte) {
            assert(info->data[concrete_parameter + 5u + byte] == 0u);
        }
        {
            uint64_t second_parameter = concrete_parameter + 5u +
                address_size;
            verify_inline_frame_parameter_die(info, &second_parameter,
                                              architecture);
            assert(second_parameter + 1u < info->size &&
                   info->data[second_parameter] == 0u &&
                   info->data[second_parameter + 1u] == 0u);
        }
        found = true;
        break;
    }
    assert(found);
    objfile_free(object);
}

static void verify_cxx_inline_adjusted_base_parameter(
    const char* path, uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    ObjSection* line;
    ObjSection* locations;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint64_t static_call_die = UINT64_MAX;
    uint64_t automatic_call_die = UINT64_MAX;

    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    line = objfile_get_section(object, ".debug_line");
    locations = objfile_get_section(object, ".debug_loc");
    assert(info != NULL && strings != NULL && line != NULL);
    for (uint64_t die = 11u;
         die + 1u + 4u + address_size + 16u <= info->size; ++die) {
        uint32_t origin;
        uint32_t name;
        uint32_t call_file;
        uint64_t low_pc_offset;
        if (info->data[die] != 41u) continue;
        origin = read_u32(info->data, die + 1u);
        if ((uint64_t)origin + 25u > info->size ||
            info->data[origin] != 40u) continue;
        name = read_u32(info->data, (uint64_t)origin + 1u);
        if (name >= strings->size ||
            strcmp((const char*)strings->data + name,
                   "debug_inline_adjusted_base_parameter_inline") != 0) {
            continue;
        }
        low_pc_offset = die + 1u + 4u;
        call_file = read_u32(
            info->data, low_pc_offset + address_size + 4u);
        assert(call_file != 0u && line_table_file_matches(
            line, call_file, "tests/debug_info_cxx_member.cpp"));
        for (const ObjReloc* relocation = info->relocs;
             relocation; relocation = relocation->next) {
            if (relocation->offset != low_pc_offset ||
                !relocation->symbol_name) continue;
            if (strcmp(relocation->symbol_name,
                       "debug_inline_adjusted_base_entry") == 0) {
                static_call_die = die;
                break;
            }
            if (strcmp(relocation->symbol_name,
                       "debug_inline_adjusted_base_automatic_entry") == 0) {
                automatic_call_die = die;
                break;
            }
        }
        if (static_call_die != UINT64_MAX &&
            automatic_call_die != UINT64_MAX) break;
    }
    assert(static_call_die != UINT64_MAX &&
           automatic_call_die != UINT64_MAX);
    verify_inline_parameter_address_constant(
        info, static_call_die, architecture,
        "debug_inline_adjusted_pointer_source", 4);
    verify_inline_parameter_frame_address(
        info, strings, locations, automatic_call_die, architecture,
        "debug_inline_adjusted_automatic_source", 4, false);
    objfile_free(object);
}

static void verify_cxx_inline_automatic_pointer_value(
    const char* path, uint16_t architecture, const char* entry_name,
    const char* variable_name, int64_t expected_offset,
    bool dereference_source)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    ObjSection* line;
    ObjSection* locations;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint64_t call_die = UINT64_MAX;

    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    line = objfile_get_section(object, ".debug_line");
    locations = objfile_get_section(object, ".debug_loc");
    assert(info != NULL && strings != NULL && line != NULL);
    for (uint64_t die = 11u;
         die + 1u + 4u + address_size + 16u <= info->size; ++die) {
        uint32_t origin;
        uint32_t name;
        uint32_t call_file;
        uint64_t low_pc_offset;
        bool has_entry_relocation = false;
        if (info->data[die] != 41u) continue;
        origin = read_u32(info->data, die + 1u);
        if ((uint64_t)origin + 25u > info->size ||
            info->data[origin] != 40u) continue;
        name = read_u32(info->data, (uint64_t)origin + 1u);
        if (name >= strings->size ||
            strcmp((const char*)strings->data + name,
                   "debug_inline_automatic_subobject_parameter_inline") != 0) {
            continue;
        }
        low_pc_offset = die + 1u + 4u;
        call_file = read_u32(
            info->data, low_pc_offset + address_size + 4u);
        if (call_file == 0u || !line_table_file_matches(
                line, call_file, "tests/debug_info_cxx_member.cpp")) {
            continue;
        }
        for (const ObjReloc* relocation = info->relocs;
             relocation; relocation = relocation->next) {
            if (relocation->offset == low_pc_offset &&
                relocation->symbol_name &&
                strcmp(relocation->symbol_name, entry_name) == 0) {
                has_entry_relocation = true;
                break;
            }
        }
        if (has_entry_relocation) {
            call_die = die;
            break;
        }
    }
    assert(call_die != UINT64_MAX);
    verify_inline_parameter_frame_address(
        info, strings, locations, call_die, architecture, variable_name,
        expected_offset, dereference_source);
    objfile_free(object);
}

static void verify_cxx_inline_automatic_pointer_indices_impl(
    const char* path, uint16_t architecture, const char* entry_name,
    const char* pointer_name,
    const VerifyInlinePointerIndexTerm* index_terms, size_t index_count,
    int64_t address_addend, bool dereference_source,
    const uint8_t* cast_steps, size_t cast_step_count,
    int64_t signed_divisor, int64_t pointer_adjustment)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    ObjSection* line;
    ObjSection* locations;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint64_t call_die = UINT64_MAX;

    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    line = objfile_get_section(object, ".debug_line");
    locations = objfile_get_section(object, ".debug_loc");
    assert(info != NULL && strings != NULL && line != NULL);
    for (uint64_t die = 11u;
         die + 1u + 4u + address_size + 16u <= info->size; ++die) {
        uint32_t origin;
        uint32_t name;
        uint32_t call_file;
        uint64_t low_pc_offset;
        bool has_entry_relocation = false;
        if (info->data[die] != 41u) continue;
        origin = read_u32(info->data, die + 1u);
        if ((uint64_t)origin + 25u > info->size ||
            info->data[origin] != 40u) continue;
        name = read_u32(info->data, (uint64_t)origin + 1u);
        if (name >= strings->size ||
            strcmp((const char*)strings->data + name,
                   "debug_inline_automatic_subobject_parameter_inline") != 0) {
            continue;
        }
        low_pc_offset = die + 1u + 4u;
        call_file = read_u32(
            info->data, low_pc_offset + address_size + 4u);
        if (call_file == 0u || !line_table_file_matches(
                line, call_file, "tests/debug_info_cxx_member.cpp")) {
            continue;
        }
        for (const ObjReloc* relocation = info->relocs;
             relocation; relocation = relocation->next) {
            if (relocation->offset == low_pc_offset &&
                relocation->symbol_name &&
                strcmp(relocation->symbol_name, entry_name) == 0) {
                has_entry_relocation = true;
                break;
            }
        }
        if (has_entry_relocation) {
            call_die = die;
            break;
        }
    }
    assert(call_die != UINT64_MAX);
    verify_inline_parameter_frame_pointer_indices(
        info, strings, locations, call_die, architecture,
        pointer_name, index_terms, index_count, address_addend,
        dereference_source, cast_steps, cast_step_count, signed_divisor,
        pointer_adjustment);
    objfile_free(object);
}

static void verify_cxx_inline_automatic_pointer_indices(
    const char* path, uint16_t architecture, const char* entry_name,
    const char* pointer_name,
    const VerifyInlinePointerIndexTerm* index_terms, size_t index_count,
    int64_t address_addend, bool dereference_source)
{
    verify_cxx_inline_automatic_pointer_indices_impl(
        path, architecture, entry_name, pointer_name, index_terms,
        index_count, address_addend, dereference_source, NULL, 0u, 0, 0);
}

static void verify_cxx_inline_automatic_pointer_indices_with_adjustment(
    const char* path, uint16_t architecture, const char* entry_name,
    const char* pointer_name,
    const VerifyInlinePointerIndexTerm* index_terms, size_t index_count,
    int64_t address_addend, bool dereference_source,
    int64_t pointer_adjustment)
{
    verify_cxx_inline_automatic_pointer_indices_impl(
        path, architecture, entry_name, pointer_name, index_terms,
        index_count, address_addend, dereference_source, NULL, 0u, 0,
        pointer_adjustment);
}

static void verify_cxx_inline_automatic_pointer_divided_index(
    const char* path, uint16_t architecture, const char* entry_name,
    const char* pointer_name, const char* index_name,
    int64_t signed_divisor) {
    const VerifyInlinePointerIndexTerm term = {
        index_name, 4u, false, 4u, false, 0u, 0u, false, 0u
    };
    verify_cxx_inline_automatic_pointer_indices_impl(
        path, architecture, entry_name, pointer_name, &term, 1u,
        0, true, NULL, 0u, signed_divisor, 0);
}

static void verify_cxx_inline_automatic_pointer_index(
    const char* path, uint16_t architecture, const char* entry_name,
    const char* pointer_name, const char* index_name,
    int64_t address_addend, bool dereference_source, uint8_t index_size,
    bool index_unsigned, uint64_t element_size, bool subtract_index)
{
    const VerifyInlinePointerIndexTerm term = {
        index_name, index_size, index_unsigned, element_size, subtract_index,
        0u, 0u, false, 0u
    };
    verify_cxx_inline_automatic_pointer_indices(
        path, architecture, entry_name, pointer_name, &term, 1u,
        address_addend, dereference_source);
}

static void verify_cxx_inline_constexpr_parameter(
    const char* path, uint16_t architecture, const char* inline_name,
    const char* host_name, int64_t expected_value)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    ObjSection* line;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint64_t call_die = UINT64_MAX;

    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    line = objfile_get_section(object, ".debug_line");
    assert(info != NULL && strings != NULL && line != NULL);
    for (uint64_t die = 11u;
         die + 1u + 4u + address_size + 16u <= info->size; ++die) {
        uint32_t origin;
        uint32_t name;
        uint32_t call_file;
        uint64_t low_pc_offset;
        bool has_entry_relocation = false;
        if (info->data[die] != 41u) continue;
        origin = read_u32(info->data, die + 1u);
        if ((uint64_t)origin + 25u > info->size ||
            info->data[origin] != 40u) continue;
        name = read_u32(info->data, (uint64_t)origin + 1u);
        if (name >= strings->size ||
            strcmp((const char*)strings->data + name, inline_name) != 0) {
            continue;
        }
        low_pc_offset = die + 1u + 4u;
        call_file = read_u32(
            info->data, low_pc_offset + address_size + 4u);
        if (call_file == 0u || !line_table_file_matches(
                line, call_file, "tests/debug_info_cxx_member.cpp")) {
            continue;
        }
        for (const ObjReloc* relocation = info->relocs;
             relocation; relocation = relocation->next) {
            if (relocation->offset != low_pc_offset) continue;
            assert(relocation->symbol_name != NULL &&
                   strcmp(relocation->symbol_name, host_name) == 0);
            has_entry_relocation = true;
            break;
        }
        if (has_entry_relocation) {
            call_die = die;
            break;
        }
    }
    assert(call_die != UINT64_MAX);
    {
        uint64_t concrete_parameter = call_die + 1u + 4u + address_size +
            4u + 12u;
        uint64_t origin_parameter = read_u32(info->data, call_die + 1u) + 27u;
        uint64_t value_offset = concrete_parameter + 5u;
        assert(info->data[concrete_parameter] == 63u);
        assert(read_u32(info->data, concrete_parameter + 1u) ==
               origin_parameter);
        assert(read_sleb(info->data, info->size, &value_offset) ==
               expected_value);
        assert(value_offset < info->size && info->data[value_offset] == 0u);
    }
    objfile_free(object);
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
            expression_offset += expression_size + (tag == 17u ? 8u : 0u);
            assert(expression_offset < info->size);
            assert(info->data[expression_offset] >= 1u &&
                   info->data[expression_offset] <= 3u);
            child = expression_offset + 1u;
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

static void verify_cxx_member_pointer_type(const char* path,
                                          uint16_t architecture,
                                          const char* variable_name,
                                          bool function_member)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    uint64_t variable_die;
    uint64_t member_pointer_die;
    uint32_t member_type;
    uint32_t containing_type;
    uint64_t expected_size;

    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && abbrev != NULL && strings != NULL);
    assert(contains_byte_pair(abbrev->data, abbrev->size, 57u, 0x1fu));
    variable_die = find_global_variable_die(info, strings, variable_name);
    assert(variable_die != UINT64_MAX && variable_die + 9u <= info->size);
    member_pointer_die = read_u32(info->data, variable_die + 5u);
    assert(member_pointer_die + 10u <= info->size &&
           info->data[member_pointer_die] == 57u);
    expected_size = function_member
        ? (architecture == ARCH_X64 ? 16u : 8u)
        : (architecture == ARCH_X64 ? 8u : 4u);
    assert(info->data[member_pointer_die + 1u] == expected_size);
    member_type = read_u32(info->data, member_pointer_die + 2u);
    containing_type = read_u32(info->data, member_pointer_die + 6u);
    assert(member_type < info->size && containing_type < info->size);
    if (function_member) {
        /* C++ cv-qualification on a member-function type is represented by
         * a qualifier DIE wrapping the subroutine type. */
        if (info->data[member_type] == 20u) {
            uint32_t qualified_type;
            assert(member_type + 5u <= info->size);
            qualified_type = read_u32(info->data, member_type + 1u);
            assert(qualified_type < info->size &&
                   info->data[qualified_type] == 18u);
        } else {
            assert(info->data[member_type] == 18u);
        }
    } else {
        assert(info->data[member_type] == 5u);
    }
    assert(containing_type == find_structure_type_die(
        info, strings, "DebugMemberObject"));
    objfile_free(object);
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

static void verify_cie_preserved_register_rules(
    const ObjSection* frame, uint16_t architecture)
{
    uint32_t cie_length;
    uint64_t cie_end;
    uint64_t cursor = 8u;
    uint32_t same_value_registers = 0u;
    uint32_t expected_registers;
    assert(frame != NULL && frame->size >= 12u);
    cie_length = read_u32(frame->data, 0u);
    cie_end = 4u + cie_length;
    assert(cie_end <= frame->size && frame->data[cursor++] == 1u);
    while (cursor < cie_end && frame->data[cursor++] != 0u) {
    }
    (void)read_uleb(frame->data, cie_end, &cursor);
    while (cursor < cie_end) {
        uint8_t byte = frame->data[cursor++];
        if ((byte & 0x80u) == 0u) break;
    }
    (void)read_uleb(frame->data, cie_end, &cursor);
    while (cursor < cie_end) {
        uint8_t opcode = frame->data[cursor++];
        if (opcode == 0x08u) {
            uint64_t reg = read_uleb(frame->data, cie_end, &cursor);
            assert(reg < 32u);
            same_value_registers |= UINT32_C(1) << reg;
        } else if (opcode == 0x0cu) {
            (void)read_uleb(frame->data, cie_end, &cursor);
            (void)read_uleb(frame->data, cie_end, &cursor);
        } else if ((opcode & 0xc0u) == 0x80u) {
            (void)read_uleb(frame->data, cie_end, &cursor);
        } else {
            assert(0 && "unexpected CIE frame instruction");
        }
    }
    expected_registers = architecture == ARCH_X64
        ? ((UINT32_C(1) << 3u) | (UINT32_C(1) << 6u) |
           (UINT32_C(0x0f) << 12u))
        : ((UINT32_C(1) << 3u) | (UINT32_C(1) << 5u) |
           (UINT32_C(1) << 6u) | (UINT32_C(1) << 7u));
    assert((same_value_registers & expected_registers) ==
           expected_registers);
}

static void verify_saved_callee_register_rules(
    const ObjSection* frame, const char* function_symbol,
    uint16_t architecture)
{
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint64_t first_save_pc[32];
    uint64_t last_restore_pc[32];
    uint32_t saved_registers = 0u;
    uint32_t restored_registers = 0u;
    bool found_fde = false;
    assert(frame != NULL && function_symbol != NULL);
    verify_cie_preserved_register_rules(frame, architecture);
    for (size_t reg = 0u; reg < 32u; ++reg) {
        first_save_pc[reg] = UINT64_MAX;
        last_restore_pc[reg] = UINT64_MAX;
    }
    for (const ObjReloc* relocation = frame->relocs; relocation;
         relocation = relocation->next) {
        uint64_t fde_offset;
        uint64_t fde_end;
        uint64_t cursor;
        uint32_t fde_length;
        uint64_t pc = 0u;
        if (!relocation->symbol_name ||
            strcmp(relocation->symbol_name, function_symbol) != 0 ||
            relocation->offset < 8u) continue;
        fde_offset = relocation->offset - 8u;
        assert(fde_offset + 4u <= frame->size);
        fde_length = read_u32(frame->data, fde_offset);
        fde_end = fde_offset + 4u + fde_length;
        assert(fde_end <= frame->size);
        cursor = fde_offset + 4u + 4u + address_size * 2u;
        assert(cursor <= fde_end);
        found_fde = true;
        while (cursor < fde_end) {
            uint8_t opcode = frame->data[cursor++];
            if ((opcode & 0xc0u) == 0x40u) {
                pc += opcode & 0x3fu;
            } else if ((opcode & 0xc0u) == 0x80u) {
                uint8_t reg = opcode & 0x3fu;
                uint64_t offset = read_uleb(
                    frame->data, fde_end, &cursor);
                if (pc > 0u && offset > 0u && reg < 32u) {
                    saved_registers |= UINT32_C(1) << reg;
                    if (first_save_pc[reg] == UINT64_MAX) {
                        first_save_pc[reg] = pc;
                    }
                }
            } else if ((opcode & 0xc0u) == 0xc0u) {
                uint8_t reg = opcode & 0x3fu;
                if (reg < 32u) {
                    restored_registers |= UINT32_C(1) << reg;
                    last_restore_pc[reg] = pc;
                }
            } else if (opcode == 0x00u) {
                continue;
            } else if (opcode == 0x02u) {
                assert(cursor < fde_end);
                pc += frame->data[cursor++];
            } else if (opcode == 0x03u) {
                assert(cursor + 2u <= fde_end);
                pc += (uint16_t)frame->data[cursor] |
                    (uint16_t)((uint16_t)frame->data[cursor + 1u] << 8u);
                cursor += 2u;
            } else if (opcode == 0x04u) {
                assert(cursor + 4u <= fde_end);
                pc += read_u32(frame->data, cursor);
                cursor += 4u;
            } else if (opcode == 0x0cu) {
                (void)read_uleb(frame->data, fde_end, &cursor);
                (void)read_uleb(frame->data, fde_end, &cursor);
            } else if (opcode == 0x0du || opcode == 0x0eu) {
                (void)read_uleb(frame->data, fde_end, &cursor);
            } else {
                assert(0 && "unexpected DWARF frame instruction");
            }
        }
        break;
    }
    assert(found_fde);
    if (architecture == ARCH_X64) {
        uint32_t preserved =
            (saved_registers & restored_registers) &
            ((UINT32_C(1) << 3u) | (UINT32_C(0x0f) << 12u));
        assert(preserved != 0u);
        for (uint32_t reg = 0u; reg < 32u; ++reg) {
            if ((preserved & (UINT32_C(1) << reg)) != 0u) {
                assert(last_restore_pc[reg] > first_save_pc[reg]);
            }
        }
    } else {
        uint32_t preserved = (saved_registers & restored_registers) &
            ((UINT32_C(1) << 3u) | (UINT32_C(1) << 6u) |
             (UINT32_C(1) << 7u));
        assert(preserved != 0u);
        for (uint32_t reg = 0u; reg < 32u; ++reg) {
            if ((preserved & (UINT32_C(1) << reg)) != 0u) {
                assert(last_restore_pc[reg] > first_save_pc[reg]);
            }
        }
    }
}

static void verify_function_frame_fde(const ObjSection* frame,
                                      const char* function_symbol,
                                      uint16_t architecture)
{
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint8_t frame_register = architecture == ARCH_X64 ? 6u : 5u;
    assert(frame != NULL && function_symbol != NULL);
    for (const ObjReloc* relocation = frame->relocs; relocation;
         relocation = relocation->next) {
        uint64_t fde_offset;
        uint64_t fde_end;
        uint64_t range_offset;
        uint64_t instructions_offset;
        uint32_t fde_length;
        if (!relocation->symbol_name ||
            strcmp(relocation->symbol_name, function_symbol) != 0 ||
            relocation->offset < 8u) {
            continue;
        }
        fde_offset = relocation->offset - 8u;
        assert(fde_offset + 8u + address_size * 2u <= frame->size);
        fde_length = read_u32(frame->data, fde_offset);
        fde_end = fde_offset + 4u + fde_length;
        assert(fde_end <= frame->size);
        assert(read_u32(frame->data, fde_offset + 4u) == 0u);
        range_offset = fde_offset + 8u + address_size;
        assert(read_u32(frame->data, range_offset) != 0u);
        if (address_size == 8u) {
            assert(read_u32(frame->data, range_offset + 4u) == 0u);
        }
        instructions_offset = fde_offset + 8u + address_size * 2u;
        assert(contains_byte_pair(
            frame->data + instructions_offset,
            fde_end - instructions_offset, 0x0du, frame_register));
        return;
    }
    assert(0 && "function has no relocated debug-frame FDE");
}

static void verify_static_reference_debug_object(const char* path,
                                                 uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* frame;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    frame = objfile_get_section(object, ".debug_frame");
    assert(info != NULL && info->size != 0u && frame != NULL);
    assert(objfile_find_symbol(object,
                               "_Z27local_polymorphic_referencev") != NULL);
    assert(objfile_find_symbol(
               object,
               "_Z40local_polymorphic_virtual_base_referencev") != NULL);
    verify_function_frame_fde(
        frame, "_Z27local_polymorphic_referencev", architecture);
    verify_function_frame_fde(
        frame, "_Z40local_polymorphic_virtual_base_referencev",
        architecture);
    objfile_free(object);
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

static void verify_tls_variable_location(ObjectFile* object,
                                         const ObjSection* info,
                                         const ObjSection* strings,
                                         const char* variable_name,
                                         const char* symbol_name,
                                         bool external)
{
    ObjSection* tls;
    ObjSection* section_cursor;
    ObjSymbol* symbol;
    ObjReloc* tls_relocation = NULL;
    uint64_t die_offset = find_global_variable_die(
        info, strings, variable_name);
    uint64_t offset;
    uint64_t expression_size;
    uint64_t tls_offset;
    int tls_section_index = 0;

    assert(object != NULL && die_offset != UINT64_MAX);
    assert(count_global_variable_dies(info, strings, variable_name) == 1u);
    tls = objfile_get_section(object, ".tls");
    assert(tls != NULL && tls->type == SECT_TLS);
    for (section_cursor = object->sections;
         section_cursor && section_cursor != tls;
         section_cursor = section_cursor->next) {
        ++tls_section_index;
    }
    assert(section_cursor == tls);
    offset = die_offset + 1u + 5u * sizeof(uint32_t);
    assert(offset + 5u <= info->size &&
           info->data[offset] == (external ? 1u : 0u));
    offset += 1u + sizeof(uint32_t); /* external flag and linkage name */
    expression_size = read_uleb(info->data, info->size, &offset);
    assert(expression_size == 6u && offset + expression_size <= info->size);
    assert(info->data[offset++] == 0x0du); /* DW_OP_const4s */
    tls_offset = offset;
    assert(info->data[offset++] == 0u && info->data[offset++] == 0u &&
           info->data[offset++] == 0u && info->data[offset++] == 0u);
    assert(info->data[offset] == 0x9bu); /* DW_OP_form_tls_address */
    for (ObjReloc* relocation = info->relocs; relocation;
         relocation = relocation->next) {
        if (relocation->offset == tls_offset &&
            relocation->type == RELOC_TLSOFF32S) {
            assert(tls_relocation == NULL);
            tls_relocation = relocation;
        }
    }
    assert(tls_relocation != NULL && tls_relocation->symbol_name != NULL);
    if (symbol_name) {
        assert(strcmp(tls_relocation->symbol_name, symbol_name) == 0);
    }
    symbol = objfile_find_symbol(object, tls_relocation->symbol_name);
    assert(symbol != NULL &&
           symbol->type == (external ? SYM_GLOBAL : SYM_LOCAL) &&
           symbol->binding == BIND_TLS &&
           symbol->section == tls_section_index);
}

static void verify_tls_global_variable(ObjectFile* object,
                                      const ObjSection* info,
                                      const ObjSection* strings,
                                      const char* variable_name)
{
    verify_tls_variable_location(object, info, strings, variable_name,
                                 variable_name, true);
}

static void verify_tls_global_object(const char* path,
                                     const char* variable_name)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    assert(object != NULL);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && strings != NULL);
    verify_tls_global_variable(object, info, strings, variable_name);
    objfile_free(object);
}

static bool has_epilogue_cfa_restore(const ObjSection* frame,
                                     uint64_t instruction_offset,
                                     uint64_t instruction_end,
                                     uint8_t stack_register,
                                     uint8_t frame_register,
                                     uint8_t pointer_size)
{
    for (uint64_t offset = instruction_offset;
         offset + 4u < instruction_end; ++offset) {
        if (frame->data[offset] == 0x0du &&
            frame->data[offset + 1u] == stack_register &&
            frame->data[offset + 2u] == 0x0eu &&
            frame->data[offset + 3u] == pointer_size &&
            frame->data[offset + 4u] ==
                (uint8_t)(0xc0u + frame_register)) {
            return true;
        }
    }
    return false;
}

static size_t count_epilogue_cfa_restores(const ObjSection* frame,
                                         uint64_t instruction_offset,
                                         uint64_t instruction_end,
                                         uint8_t stack_register,
                                         uint8_t frame_register,
                                         uint8_t pointer_size)
{
    size_t count = 0u;
    for (uint64_t offset = instruction_offset;
         offset + 4u < instruction_end; ++offset) {
        if (frame->data[offset] == 0x0du &&
            frame->data[offset + 1u] == stack_register &&
            frame->data[offset + 2u] == 0x0eu &&
            frame->data[offset + 3u] == pointer_size &&
            frame->data[offset + 4u] ==
                (uint8_t)(0xc0u + frame_register)) {
            ++count;
        }
    }
    return count;
}

static size_t count_frame_body_restores(const ObjSection* frame,
                                       uint64_t instruction_offset,
                                       uint64_t instruction_end,
                                       uint8_t frame_register,
                                       uint8_t pointer_size)
{
    size_t count = 0u;
    for (uint64_t offset = instruction_offset;
         offset + 5u < instruction_end; ++offset) {
        if (frame->data[offset] == 0x0du &&
            frame->data[offset + 1u] == frame_register &&
            frame->data[offset + 2u] == 0x0eu &&
            frame->data[offset + 3u] == (uint8_t)(pointer_size * 2u) &&
            frame->data[offset + 4u] ==
                (uint8_t)(0x80u + frame_register) &&
            frame->data[offset + 5u] == 2u) {
            ++count;
        }
    }
    return count;
}

static void verify_multiple_return_frame_fde(const ObjSection* frame,
                                             const char* function_symbol,
                                             uint16_t architecture)
{
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint8_t frame_register = architecture == ARCH_X64 ? 6u : 5u;
    uint8_t stack_register = architecture == ARCH_X64 ? 7u : 4u;
    uint8_t pointer_size = architecture == ARCH_X64 ? 8u : 4u;
    assert(frame != NULL && function_symbol != NULL);
    for (const ObjReloc* relocation = frame->relocs; relocation;
         relocation = relocation->next) {
        uint64_t fde_offset;
        uint64_t instruction_offset;
        uint64_t instruction_end;
        uint32_t fde_length;
        if (!relocation->symbol_name ||
            strcmp(relocation->symbol_name, function_symbol) != 0 ||
            relocation->offset < 8u) {
            continue;
        }
        fde_offset = relocation->offset - 8u;
        assert(fde_offset + 8u + address_size * 2u <= frame->size);
        fde_length = read_u32(frame->data, fde_offset);
        instruction_offset = fde_offset + 8u + address_size * 2u;
        instruction_end = fde_offset + 4u + fde_length;
        assert(instruction_end <= frame->size);
        assert(read_u32(frame->data, fde_offset + 4u) == 0u);
        assert(count_epilogue_cfa_restores(
                   frame, instruction_offset, instruction_end,
                   stack_register, frame_register, pointer_size) == 3u);
        assert(count_frame_body_restores(
                   frame, instruction_offset, instruction_end,
                   frame_register, pointer_size) == 2u);
        return;
    }
    assert(0 && "function has no relocated multi-return debug-frame FDE");
}

static void verify_first_frame_fde(const ObjSection* frame,
                                   uint16_t architecture)
{
    uint32_t cie_length;
    uint64_t fde_offset;
    uint32_t fde_length;
    uint64_t instruction_offset;
    uint64_t instruction_end;
    uint64_t address_range_offset;
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
    address_range_offset = fde_offset + 4u + 4u + pointer_size;
    assert(address_range_offset + pointer_size <= frame->size);
    assert(read_u32(frame->data, address_range_offset) != 0u);
    if (pointer_size == 8u) {
        /* The FDE range is a zero-extended 32-bit code size. */
        assert(read_u32(frame->data, address_range_offset + 4u) == 0u);
    }
    instruction_offset = fde_offset + 4u + 4u + pointer_size * 2u;
    instruction_end = fde_offset + 4u + fde_length;
    assert(instruction_offset + 8u <= instruction_end);
    /* push fp; the saved FP is two words below the new CFA. */
    assert(frame->data[instruction_offset++] == 0x41u);
    assert(frame->data[instruction_offset++] == 0x0eu);
    assert(frame->data[instruction_offset++] == (uint8_t)(pointer_size * 2u));
    assert(frame->data[instruction_offset++] ==
           (uint8_t)(0x80u + frame_register));
    /* Saved FP is CFA - 2 words; CFA - 1 word holds the return address. */
    assert(frame->data[instruction_offset++] == 2u);
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
    assert(has_epilogue_cfa_restore(
        frame, instruction_offset, instruction_end,
        architecture == ARCH_X64 ? 7u : 4u, frame_register,
        (uint8_t)pointer_size));
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
    assert(frame->data[instruction_offset++] == 2u);
    /* After `and esp`, EBP and the return address temporarily live in EAX
     * and EDX until the aligned stack copies complete at offset 21. */
    assert(frame->data[instruction_offset++] == 0x4du);
    assert(frame->data[instruction_offset++] == 0x0cu);
    assert(frame->data[instruction_offset++] == 0x04u);
    assert(frame->data[instruction_offset++] == 0x08u);
    assert(frame->data[instruction_offset++] == 0x09u);
    assert(frame->data[instruction_offset++] == 0x05u);
    assert(frame->data[instruction_offset++] == 0x00u);
    assert(frame->data[instruction_offset++] == 0x09u);
    assert(frame->data[instruction_offset++] == 0x08u);
    assert(frame->data[instruction_offset++] == 0x02u);
    assert(frame->data[instruction_offset++] == 0x47u);
    assert(frame->data[instruction_offset++] == 0x85u);
    assert(frame->data[instruction_offset++] == 0x02u);
    assert(frame->data[instruction_offset++] == 0xc8u);
    /* mov ebp,esp: use the stable aligned frame register thereafter. */
    assert(frame->data[instruction_offset++] == 0x42u);
    assert(frame->data[instruction_offset++] == 0x0du);
    assert(frame->data[instruction_offset++] == 0x05u);
    assert(contains_byte_pair(frame->data + instruction_offset,
                              instruction_end - instruction_offset,
                              0x0du, 0x04u));
    assert(has_epilogue_cfa_restore(
        frame, instruction_offset, instruction_end, 4u, 5u, 4u));
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
        if (offset + 1u + 4u + address_size + 4u + 4u + 4u > info->size) {
            return UINT64_MAX;
        }
        return offset;
    }
    return UINT64_MAX;
}

static void verify_large_file_table(const char* path,
                                    uint16_t architecture,
                                    const char* expected_file)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    ObjSection* line;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint64_t function_die;
    uint32_t file_index;
    uint32_t scope_file_index = 0u;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    line = objfile_get_section(object, ".debug_line");
    assert(info != NULL && strings != NULL && line != NULL);
    function_die = find_function_die(info, strings,
                                     "debug_info_large_file_259",
                                     address_size);
    assert(function_die != UINT64_MAX);
    file_index = read_u32(info->data,
                          function_die + 1u + 4u + address_size + 4u);
    assert(file_index > 255u);
    assert(line_table_file_matches(line, file_index, expected_file));
    assert(find_lexical_block_local(
        info, strings, "debug_info_large_file_local", address_size));
    for (uint64_t offset = 11u; offset + 17u <= info->size; ++offset) {
        bool ranged = info->data[offset] == 25u;
        uint64_t range_offset;
        uint64_t file_offset;
        uint64_t child_offset;
        uint32_t name_offset;
        if (!ranged && info->data[offset] != 24u) continue;
        range_offset = ranged ? offset + 1u : offset + 1u + address_size;
        file_offset = range_offset + 4u;
        child_offset = file_offset + 12u;
        if (child_offset + 5u > info->size ||
            (info->data[child_offset] != 4u &&
             info->data[child_offset] != 39u)) {
            continue;
        }
        name_offset = read_u32(info->data, child_offset + 1u);
        if (name_offset < strings->size &&
            strcmp((const char*)strings->data + name_offset,
                   "debug_info_large_file_local") == 0) {
            scope_file_index = read_u32(info->data, file_offset);
            break;
        }
    }
    assert(scope_file_index > 255u);
    assert(line_table_file_matches(line, scope_file_index, expected_file));
    objfile_free(object);
}

static void verify_dependent_compound_type_die(
    const ObjSection* info, const ObjSection* strings, uint32_t type_die,
    uint8_t expected_type_tag, bool member_pointer)
{
    uint32_t base_type;
    assert(type_die + 2u <= info->size &&
           info->data[type_die] == expected_type_tag);
    if (expected_type_tag == 6u) {
        base_type = read_u32(info->data, type_die + 2u);
    } else if (expected_type_tag == 37u) {
        base_type = read_u32(info->data, type_die + 1u);
    } else {
        uint32_t owner_type;
        uint32_t owner_name;
        assert(member_pointer && expected_type_tag == 57u &&
               type_die + 10u <= info->size);
        base_type = read_u32(info->data, type_die + 2u);
        owner_type = read_u32(info->data, type_die + 6u);
        assert(owner_type + 5u <= info->size &&
               info->data[owner_type] == 12u);
        owner_name = read_u32(info->data, owner_type + 1u);
        assert(owner_name < strings->size &&
               strcmp((const char*)strings->data + owner_name,
                      "DebugTemplateCompoundTypeOwner") == 0);
    }
    assert(base_type + 6u <= info->size && info->data[base_type] == 5u);
    {
        uint32_t base_name = read_u32(info->data, base_type + 1u);
        assert(base_name < strings->size &&
               strcmp((const char*)strings->data + base_name, "short") == 0);
    }
}

static void verify_dependent_compound_function_type(
    const char* path, uint16_t architecture, const char* function_name,
    uint8_t expected_type_tag, bool member_pointer)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint64_t function_die;
    uint64_t cursor;
    uint64_t expression_size;
    uint32_t result_type;
    uint32_t parameter_type = UINT32_MAX;
    bool found_parameter = false;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && strings != NULL);
    function_die = find_function_die(info, strings, function_name,
                                     address_size);
    assert(function_die != UINT64_MAX);
    result_type = read_u32(info->data,
                           function_die + 26u + address_size);
    verify_dependent_compound_type_die(
        info, strings, result_type, expected_type_tag, member_pointer);
    cursor = function_die + 30u + address_size;
    expression_size = read_uleb(info->data, info->size, &cursor);
    assert(expression_size <= info->size - cursor);
    cursor += expression_size + 2u; /* frame base, inline, prototype */
    while (cursor < info->size && info->data[cursor] != 0u) {
        uint8_t abbreviation = info->data[cursor];
        if (abbreviation == 47u) {
            assert(cursor + 9u <= info->size);
            cursor += 9u; /* instantiated function-template type parameter */
        } else if (abbreviation == 3u) {
            uint32_t parameter_name;
            uint32_t child_type;
            uint64_t parameter_start = cursor;
            cursor += 1u;
            assert(cursor + 20u <= info->size);
            parameter_name = read_u32(info->data, cursor);
            child_type = read_u32(info->data, cursor + 4u);
            cursor += 20u; /* name, type, file, line, and column */
            expression_size = read_uleb(info->data, info->size, &cursor);
            assert(expression_size <= info->size - cursor);
            cursor += expression_size;
            if (parameter_name < strings->size &&
                strcmp((const char*)strings->data + parameter_name,
                       "value") == 0) {
                verify_dependent_compound_type_die(
                    info, strings, child_type, expected_type_tag,
                    member_pointer);
                parameter_type = child_type;
                found_parameter = true;
                break;
            }
            assert(cursor > parameter_start);
        } else {
            assert(0 && "unexpected child in dependent-type function DIE");
        }
    }
    assert(found_parameter && parameter_type != UINT32_MAX);
    objfile_free(object);
}

static uint64_t function_template_children_start(
    const ObjSection* info, uint64_t function_die, uint64_t address_size);

static void verify_dependent_function_parameter_pack(
    const char* path, uint16_t architecture, const char* function_name,
    uint8_t expected_type_tag, bool array_reference,
    bool const_reference)
{
    static const char* const owner_names[] = {
        "DebugTemplateCompoundPackShortOwner",
        "DebugTemplateCompoundPackLongOwner"
    };
    static const char* const base_type_names[] = {"short", "long"};
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint64_t function_die;
    uint64_t cursor;
    unsigned argument_count = 0u;
    unsigned parameter_count = 0u;

    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && strings != NULL);
    function_die = find_function_die(info, strings, function_name,
                                     address_size);
    assert(function_die != UINT64_MAX);
    cursor = function_template_children_start(info, function_die,
                                              address_size);

    /* The owner pack DIE confirms the specialization uses the two distinct
     * owner types whose nested aliases expand to short and long. */
    assert(cursor + 5u <= info->size && info->data[cursor++] == 50u);
    {
        uint32_t pack_name = read_u32(info->data, cursor);
        cursor += 4u;
        assert(pack_name < strings->size &&
               strcmp((const char*)strings->data + pack_name, "Owners") == 0);
    }
    while (cursor < info->size && info->data[cursor] == 51u) {
        uint32_t owner_type;
        uint32_t owner_name;
        assert(cursor + 5u <= info->size && argument_count < 2u);
        ++cursor;
        owner_type = read_u32(info->data, cursor);
        cursor += 4u;
        assert(owner_type + 5u <= info->size &&
               info->data[owner_type] == 12u);
        owner_name = read_u32(info->data, owner_type + 1u);
        assert(owner_name < strings->size &&
               strcmp((const char*)strings->data + owner_name,
                      owner_names[argument_count]) == 0);
        ++argument_count;
    }
    assert(argument_count == 2u && cursor < info->size &&
           info->data[cursor++] == 0u);

    while (cursor < info->size && info->data[cursor] == 3u) {
        uint32_t parameter_type;
        uint64_t expression_size;
        assert(parameter_count < 2u && cursor + 21u <= info->size);
        ++cursor;
        cursor += 4u; /* generated parameter name */
        parameter_type = read_u32(info->data, cursor);
        cursor += 16u; /* type, file, line, and column */
        expression_size = read_uleb(info->data, info->size, &cursor);
        assert(expression_size <= info->size - cursor);
        cursor += expression_size;
        assert(parameter_type + 5u <= info->size &&
               info->data[parameter_type] == expected_type_tag);
        {
            uint32_t base_type;
            uint32_t base_name;
            if (array_reference) {
                uint32_t array_type;
                uint32_t subrange_type;
                assert(expected_type_tag == 37u &&
                       parameter_type + 5u <= info->size);
                array_type = read_u32(info->data, parameter_type + 1u);
                assert(array_type + 13u <= info->size &&
                       info->data[array_type] == 10u);
                base_type = read_u32(info->data, array_type + 1u);
                subrange_type = array_type + 5u;
                assert(info->data[subrange_type] == 11u &&
                       read_u32(info->data, subrange_type + 1u) == 1u &&
                       info->data[subrange_type + 5u] == 0u &&
                       info->data[subrange_type + 6u] == 0u);
            } else if (expected_type_tag == 6u &&
                       strcmp(function_name,
                              "debug_template_dependent_array_pointer_pack") == 0) {
                uint32_t array_type;
                uint32_t subrange_type;
                assert(parameter_type + 6u <= info->size);
                array_type = read_u32(info->data, parameter_type + 2u);
                assert(array_type + 12u <= info->size &&
                       info->data[array_type] == 10u);
                base_type = read_u32(info->data, array_type + 1u);
                subrange_type = array_type + 5u;
                assert(info->data[subrange_type] == 11u &&
                       read_u32(info->data, subrange_type + 1u) == 1u &&
                       info->data[subrange_type + 5u] == 0u &&
                       info->data[subrange_type + 6u] == 0u);
            } else if (expected_type_tag == 5u) {
                base_type = parameter_type;
            } else if (expected_type_tag == 37u) {
                base_type = read_u32(info->data, parameter_type + 1u);
                if (const_reference) {
                    assert(base_type + 5u <= info->size &&
                           info->data[base_type] == 20u);
                    base_type = read_u32(info->data, base_type + 1u);
                }
            } else {
                base_type = read_u32(info->data, parameter_type + 2u);
            }
            assert(base_type + 5u <= info->size &&
                   info->data[base_type] == 5u);
            base_name = read_u32(info->data, base_type + 1u);
            assert(base_name < strings->size &&
                   strcmp((const char*)strings->data + base_name,
                          base_type_names[parameter_count]) == 0);
            if (expected_type_tag == 57u) {
                uint32_t owner_type;
                uint32_t owner_name;
                assert(parameter_type + 10u <= info->size);
                owner_type = read_u32(info->data, parameter_type + 6u);
                assert(owner_type + 5u <= info->size &&
                       info->data[owner_type] == 12u);
                owner_name = read_u32(info->data, owner_type + 1u);
                assert(owner_name < strings->size &&
                       strcmp((const char*)strings->data + owner_name,
                              owner_names[parameter_count]) == 0);
            }
        }
        ++parameter_count;
    }
    assert(parameter_count == 2u);
    objfile_free(object);
}

static void verify_function_template_parameter_dies(
    const char* path, uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint64_t function_die;
    uint64_t cursor;
    uint64_t expression_size;
    uint32_t name_offset;
    uint32_t type_offset;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && strings != NULL);
    function_die = find_function_die(info, strings,
                                     "debug_template_function",
                                     address_size);
    assert(function_die != UINT64_MAX);
    cursor = function_die + 1u + 4u + address_size + 4u + 4u + 4u + 4u +
             1u + 4u + 4u;
    expression_size = read_uleb(info->data, info->size, &cursor);
    assert(expression_size <= info->size - cursor);
    cursor += expression_size + 2u; /* frame base, inline, and prototype */

    assert(cursor < info->size && info->data[cursor++] == 47u);
    assert(cursor + 8u <= info->size);
    name_offset = read_u32(info->data, cursor);
    type_offset = read_u32(info->data, cursor + 4u);
    cursor += 8u;
    assert(name_offset < strings->size &&
           strcmp((const char*)strings->data + name_offset, "Value") == 0);
    assert(type_offset + 7u <= info->size && info->data[type_offset] == 5u);
    name_offset = read_u32(info->data, type_offset + 1u);
    assert(name_offset < strings->size &&
           strcmp((const char*)strings->data + name_offset,
                  "unsigned short") == 0);

    assert(cursor < info->size && info->data[cursor++] == 48u);
    assert(cursor + 8u <= info->size);
    name_offset = read_u32(info->data, cursor);
    type_offset = read_u32(info->data, cursor + 4u);
    cursor += 8u;
    assert(name_offset < strings->size &&
           strcmp((const char*)strings->data + name_offset, "Bias") == 0);
    assert(type_offset + 7u <= info->size && info->data[type_offset] == 5u);
    name_offset = read_u32(info->data, type_offset + 1u);
    assert(name_offset < strings->size &&
           strcmp((const char*)strings->data + name_offset, "int") == 0);
    assert(read_sleb(info->data, info->size, &cursor) == -7);
    objfile_free(object);
}

static uint64_t function_template_children_start(
    const ObjSection* info, uint64_t function_die, uint64_t address_size)
{
    uint64_t cursor = function_die + 1u + 4u + address_size + 4u + 4u + 4u +
                      4u + 1u + 4u + 4u;
    uint64_t expression_size = read_uleb(info->data, info->size, &cursor);
    assert(expression_size <= info->size - cursor);
    cursor += expression_size + 2u; /* inline and prototype attributes */
    return cursor;
}

static void verify_function_template_template_parameter_dies(
    const char* path, uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    unsigned global_template_count = 0u;
    unsigned namespace_template_count = 0u;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && strings != NULL);

    for (uint64_t die = 11u; die + 5u <= info->size; ++die) {
        uint32_t function_name_offset;
        uint32_t parameter_name_offset;
        uint32_t template_name_offset;
        uint32_t type_offset;
        uint64_t cursor;
        const char* template_name;
        if (info->data[die] != 2u) continue;
        function_name_offset = read_u32(info->data, die + 1u);
        if (function_name_offset >= strings->size ||
            strcmp((const char*)strings->data + function_name_offset,
                   "debug_template_template_function") != 0) {
            continue;
        }
        cursor = function_template_children_start(info, die, address_size);
        assert(cursor + 9u <= info->size && info->data[cursor++] == 54u);
        parameter_name_offset = read_u32(info->data, cursor);
        template_name_offset = read_u32(info->data, cursor + 4u);
        cursor += 8u;
        assert(parameter_name_offset < strings->size &&
               strcmp((const char*)strings->data + parameter_name_offset,
                      "Template") == 0);
        assert(template_name_offset < strings->size);
        template_name = (const char*)strings->data + template_name_offset;
        if (strcmp(template_name, "DebugTemplateTemplateLeaf") == 0) {
            ++global_template_count;
        } else if (strcmp(template_name,
                          "debug_template_namespace::Leaf") == 0) {
            ++namespace_template_count;
        } else {
            assert(false);
        }

        assert(cursor + 9u <= info->size && info->data[cursor++] == 47u);
        parameter_name_offset = read_u32(info->data, cursor);
        type_offset = read_u32(info->data, cursor + 4u);
        assert(parameter_name_offset < strings->size &&
               strcmp((const char*)strings->data + parameter_name_offset,
                      "Value") == 0);
        assert((uint64_t)type_offset + 7u <= info->size &&
               info->data[type_offset] == 5u);
        parameter_name_offset = read_u32(info->data, type_offset + 1u);
        assert(parameter_name_offset < strings->size &&
               strcmp((const char*)strings->data + parameter_name_offset,
                      "int") == 0);
    }
    assert(global_template_count == 1u && namespace_template_count == 1u);
    objfile_free(object);
}

static void verify_function_template_nontype_signature_die(
    const char* path, uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    unsigned matching_function_count = 0u;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && strings != NULL);

    for (uint64_t die = 11u; die + 5u <= info->size; ++die) {
        uint32_t function_name_offset;
        uint32_t parameter_name_offset;
        uint32_t template_name_offset;
        uint32_t value_type_offset;
        uint64_t cursor;
        if (info->data[die] != 2u) continue;
        function_name_offset = read_u32(info->data, die + 1u);
        if (function_name_offset >= strings->size ||
            strcmp((const char*)strings->data + function_name_offset,
                   "debug_template_value_template_function") != 0) {
            continue;
        }
        ++matching_function_count;
        cursor = function_template_children_start(info, die, address_size);
        assert(cursor + 9u <= info->size && info->data[cursor++] == 54u);
        parameter_name_offset = read_u32(info->data, cursor);
        template_name_offset = read_u32(info->data, cursor + 4u);
        cursor += 8u;
        assert(parameter_name_offset < strings->size &&
               strcmp((const char*)strings->data + parameter_name_offset,
                      "Template") == 0);
        assert(template_name_offset < strings->size &&
               strcmp((const char*)strings->data + template_name_offset,
                      "DebugTemplateValueTemplateLeaf") == 0);

        assert(cursor + 13u <= info->size && info->data[cursor++] == 48u);
        parameter_name_offset = read_u32(info->data, cursor);
        value_type_offset = read_u32(info->data, cursor + 4u);
        cursor += 8u;
        assert(parameter_name_offset < strings->size &&
               strcmp((const char*)strings->data + parameter_name_offset,
                      "Value") == 0);
        assert((uint64_t)value_type_offset + 7u <= info->size &&
               info->data[value_type_offset] == 5u);
        parameter_name_offset = read_u32(info->data, value_type_offset + 1u);
        assert(parameter_name_offset < strings->size &&
               strcmp((const char*)strings->data + parameter_name_offset,
                      "int") == 0);
        assert(read_sleb(info->data, info->size, &cursor) == 7);
    }
    assert(matching_function_count == 1u);
    objfile_free(object);
}

static void verify_function_template_type_pack_dies(
    const char* path, uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    unsigned empty_count = 0u;
    unsigned populated_count = 0u;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && strings != NULL);

    for (uint64_t die = 11u; die + 5u <= info->size; ++die) {
        uint32_t function_name_offset;
        uint64_t cursor;
        uint32_t name_offset;
        uint32_t type_offset;
        unsigned element_count = 0u;
        if (info->data[die] != 2u) continue;
        function_name_offset = read_u32(info->data, die + 1u);
        if (function_name_offset >= strings->size ||
            strcmp((const char*)strings->data + function_name_offset,
                   "debug_template_function_type_pack") != 0) {
            continue;
        }
        cursor = function_template_children_start(info, die, address_size);
        assert(cursor < info->size && info->data[cursor++] == 47u);
        assert(cursor + 8u <= info->size);
        name_offset = read_u32(info->data, cursor);
        type_offset = read_u32(info->data, cursor + 4u);
        cursor += 8u;
        assert(name_offset < strings->size &&
               strcmp((const char*)strings->data + name_offset, "Head") == 0);
        assert((uint64_t)type_offset + 7u <= info->size &&
               info->data[type_offset] == 5u);
        name_offset = read_u32(info->data, type_offset + 1u);
        assert(name_offset < strings->size &&
               strcmp((const char*)strings->data + name_offset, "int") == 0);

        assert(cursor < info->size && info->data[cursor++] == 50u);
        assert(cursor + 4u <= info->size);
        name_offset = read_u32(info->data, cursor);
        cursor += 4u;
        assert(name_offset < strings->size &&
               strcmp((const char*)strings->data + name_offset, "Tail") == 0);
        while (cursor < info->size && info->data[cursor] == 51u) {
            assert(cursor + 5u <= info->size);
            ++cursor;
            type_offset = read_u32(info->data, cursor);
            cursor += 4u;
            assert((uint64_t)type_offset + 7u <= info->size &&
                   info->data[type_offset] == 5u);
            name_offset = read_u32(info->data, type_offset + 1u);
            assert(name_offset < strings->size);
            if (element_count == 0u) {
                assert(strcmp((const char*)strings->data + name_offset,
                              "short") == 0);
            } else if (element_count == 1u) {
                assert(strcmp((const char*)strings->data + name_offset,
                              "long") == 0);
            } else {
                assert(0 && "unexpected function type-pack element");
            }
            ++element_count;
        }
        assert(cursor < info->size && info->data[cursor++] == 0u);
        if (element_count == 0u) {
            ++empty_count;
        } else {
            assert(element_count == 2u);
            ++populated_count;
        }
    }
    assert(empty_count == 1u && populated_count == 1u);
    objfile_free(object);
}

static void verify_function_template_signed_pack_dies(
    const char* path, uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    unsigned empty_count = 0u;
    unsigned populated_count = 0u;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && strings != NULL);

    for (uint64_t die = 11u; die + 5u <= info->size; ++die) {
        uint32_t function_name_offset;
        uint64_t cursor;
        uint32_t name_offset;
        uint32_t type_offset;
        unsigned element_count = 0u;
        if (info->data[die] != 2u) continue;
        function_name_offset = read_u32(info->data, die + 1u);
        if (function_name_offset >= strings->size ||
            strcmp((const char*)strings->data + function_name_offset,
                   "debug_template_function_signed_pack") != 0) {
            continue;
        }
        cursor = function_template_children_start(info, die, address_size);
        assert(cursor < info->size && info->data[cursor++] == 48u);
        assert(cursor + 8u <= info->size);
        name_offset = read_u32(info->data, cursor);
        type_offset = read_u32(info->data, cursor + 4u);
        cursor += 8u;
        assert(name_offset < strings->size &&
               strcmp((const char*)strings->data + name_offset, "Seed") == 0);
        assert((uint64_t)type_offset + 7u <= info->size &&
               info->data[type_offset] == 5u);
        name_offset = read_u32(info->data, type_offset + 1u);
        assert(name_offset < strings->size &&
               strcmp((const char*)strings->data + name_offset, "int") == 0);
        assert(read_sleb(info->data, info->size, &cursor) == 3);

        assert(cursor < info->size && info->data[cursor++] == 50u);
        assert(cursor + 4u <= info->size);
        name_offset = read_u32(info->data, cursor);
        cursor += 4u;
        assert(name_offset < strings->size &&
               strcmp((const char*)strings->data + name_offset,
                      "Values") == 0);
        while (cursor < info->size && info->data[cursor] == 52u) {
            int64_t value;
            assert(cursor + 5u <= info->size);
            ++cursor;
            type_offset = read_u32(info->data, cursor);
            cursor += 4u;
            assert((uint64_t)type_offset + 7u <= info->size &&
                   info->data[type_offset] == 5u);
            name_offset = read_u32(info->data, type_offset + 1u);
            assert(name_offset < strings->size &&
                   strcmp((const char*)strings->data + name_offset,
                          "int") == 0);
            value = read_sleb(info->data, info->size, &cursor);
            assert((element_count == 0u && value == -2) ||
                   (element_count == 1u && value == 7));
            ++element_count;
        }
        assert(cursor < info->size && info->data[cursor++] == 0u);
        if (element_count == 0u) {
            ++empty_count;
        } else {
            assert(element_count == 2u);
            ++populated_count;
        }
    }
    assert(empty_count == 1u && populated_count == 1u);
    objfile_free(object);
}

static void verify_function_template_unsigned_pack_die(
    const char* path, uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    unsigned empty_count = 0u;
    unsigned populated_count = 0u;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && strings != NULL);

    for (uint64_t die = 11u; die + 5u <= info->size; ++die) {
        uint32_t function_name_offset;
        uint64_t cursor;
        uint32_t name_offset;
        uint32_t type_offset;
        unsigned element_count = 0u;
        if (info->data[die] != 2u) continue;
        function_name_offset = read_u32(info->data, die + 1u);
        if (function_name_offset >= strings->size ||
            strcmp((const char*)strings->data + function_name_offset,
                   "debug_template_function_unsigned_pack") != 0) {
            continue;
        }
        cursor = function_template_children_start(info, die, address_size);
        assert(cursor < info->size && info->data[cursor++] == 50u);
        assert(cursor + 4u <= info->size);
        name_offset = read_u32(info->data, cursor);
        cursor += 4u;
        assert(name_offset < strings->size &&
               strcmp((const char*)strings->data + name_offset,
                      "Values") == 0);
        while (cursor < info->size && info->data[cursor] == 53u) {
            uint64_t value;
            assert(cursor + 5u <= info->size);
            ++cursor;
            type_offset = read_u32(info->data, cursor);
            cursor += 4u;
            assert((uint64_t)type_offset + 7u <= info->size &&
                   info->data[type_offset] == 5u);
            name_offset = read_u32(info->data, type_offset + 1u);
            assert(name_offset < strings->size &&
                   strcmp((const char*)strings->data + name_offset,
                          "unsigned int") == 0);
            value = read_uleb(info->data, info->size, &cursor);
            assert(element_count == 0u &&
                   value == UINT32_C(0xf0000000));
            ++element_count;
        }
        assert(cursor < info->size && info->data[cursor++] == 0u);
        if (element_count == 0u) {
            ++empty_count;
        } else {
            assert(element_count == 1u);
            ++populated_count;
        }
    }
    assert(empty_count == 1u && populated_count == 1u);
    objfile_free(object);
}

static void verify_function_template_dependent_value_pack_dies(
    const char* path, uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    unsigned empty_count = 0u;
    unsigned signed_count = 0u;
    unsigned unsigned_count = 0u;
    unsigned same_value_different_type_count = 0u;
    unsigned enum_count = 0u;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && strings != NULL);

    for (uint64_t die = 11u; die + 5u <= info->size; ++die) {
        uint32_t function_name_offset;
        uint64_t cursor;
        uint32_t name_offset;
        uint32_t type_offset;
        uint32_t value_type_offset;
        const char* value_type_name;
        bool value_is_enum;
        unsigned element_count = 0u;
        if (info->data[die] != 2u) continue;
        function_name_offset = read_u32(info->data, die + 1u);
        if (function_name_offset >= strings->size ||
            strcmp((const char*)strings->data + function_name_offset,
                   "debug_template_function_dependent_value_pack") != 0) {
            continue;
        }
        cursor = function_template_children_start(info, die, address_size);
        assert(cursor < info->size && info->data[cursor++] == 47u);
        assert(cursor + 8u <= info->size);
        name_offset = read_u32(info->data, cursor);
        type_offset = read_u32(info->data, cursor + 4u);
        cursor += 8u;
        assert(name_offset < strings->size &&
               strcmp((const char*)strings->data + name_offset,
                      "ValueType") == 0);
        assert((uint64_t)type_offset + 5u <= info->size &&
               (info->data[type_offset] == 5u ||
                info->data[type_offset] == 15u ||
                info->data[type_offset] == 36u));
        name_offset = read_u32(info->data, type_offset + 1u);
        assert(name_offset < strings->size);
        value_type_offset = type_offset;
        value_type_name = (const char*)strings->data + name_offset;
        value_is_enum = info->data[type_offset] == 15u ||
                        info->data[type_offset] == 36u;
        assert(info->data[type_offset] == 5u || value_is_enum);

        assert(cursor < info->size && info->data[cursor++] == 50u);
        assert(cursor + 4u <= info->size);
        name_offset = read_u32(info->data, cursor);
        cursor += 4u;
        assert(name_offset < strings->size &&
               strcmp((const char*)strings->data + name_offset,
                      "Values") == 0);
        while (cursor < info->size &&
               (info->data[cursor] == 52u || info->data[cursor] == 53u)) {
            bool is_unsigned = info->data[cursor] == 53u;
            assert(cursor + 5u <= info->size);
            ++cursor;
            type_offset = read_u32(info->data, cursor);
            cursor += 4u;
            assert(type_offset == value_type_offset);
            if (is_unsigned) {
                uint64_t value = read_uleb(info->data, info->size, &cursor);
                assert(!value_is_enum &&
                       strcmp(value_type_name, "unsigned short") == 0 &&
                       element_count == 0u && value == UINT16_C(0xf000));
            } else {
                int64_t value = read_sleb(info->data, info->size, &cursor);
                if (value_is_enum) {
                    assert(strcmp(value_type_name,
                                  "DebugTemplateDependentValueEnum") == 0 &&
                           ((element_count == 0u && value == -3) ||
                            (element_count == 1u && value == 12)));
                } else if (strcmp(value_type_name, "short") == 0) {
                    assert((element_count == 0u && value == -2) ||
                           (element_count == 1u && value == 7));
                } else {
                    assert(strcmp(value_type_name, "int") == 0 &&
                           element_count == 0u && value == 7);
                }
            }
            ++element_count;
        }
        assert(cursor < info->size && info->data[cursor++] == 0u);
        if (strcmp(value_type_name, "short") == 0) {
            if (element_count == 0u) ++empty_count;
            else {
                assert(element_count == 2u);
                ++signed_count;
            }
        } else if (strcmp(value_type_name, "unsigned short") == 0) {
            assert(element_count == 1u);
            ++unsigned_count;
        } else if (value_is_enum) {
            assert(strcmp(value_type_name,
                          "DebugTemplateDependentValueEnum") == 0 &&
                   element_count == 2u);
            ++enum_count;
        } else {
            assert(strcmp(value_type_name, "int") == 0 &&
                   element_count == 1u);
            ++same_value_different_type_count;
        }
    }
    assert(empty_count == 1u && signed_count == 1u && unsigned_count == 1u &&
           same_value_different_type_count == 1u && enum_count == 1u);
    objfile_free(object);
}

static void verify_function_template_nested_dependent_value_pack_die(
    const char* path, uint16_t architecture, const char* expected_function,
    const char* expected_owner_name)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    unsigned found = 0u;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && strings != NULL);

    for (uint64_t die = 11u; die + 5u <= info->size; ++die) {
        uint32_t function_name_offset;
        uint64_t cursor;
        uint32_t name_offset;
        uint32_t owner_type_offset;
        uint32_t owner_name_offset;
        uint32_t pack_name_offset;
        unsigned value_count = 0u;
        if (info->data[die] != 2u) continue;
        function_name_offset = read_u32(info->data, die + 1u);
        if (function_name_offset >= strings->size ||
            strcmp((const char*)strings->data + function_name_offset,
                   expected_function) != 0) {
            continue;
        }

        cursor = function_template_children_start(info, die, address_size);
        assert(cursor < info->size && info->data[cursor++] == 47u);
        assert(cursor + 8u <= info->size);
        name_offset = read_u32(info->data, cursor);
        owner_type_offset = read_u32(info->data, cursor + 4u);
        cursor += 8u;
        assert(name_offset < strings->size &&
               strcmp((const char*)strings->data + name_offset,
                      "Owner") == 0);
        assert(owner_type_offset + 5u <= info->size &&
               info->data[owner_type_offset] == 12u);
        owner_name_offset = read_u32(info->data, owner_type_offset + 1u);
        assert(owner_name_offset < strings->size &&
               strcmp((const char*)strings->data + owner_name_offset,
                      expected_owner_name) == 0);

        assert(cursor < info->size && info->data[cursor++] == 50u);
        assert(cursor + 4u <= info->size);
        pack_name_offset = read_u32(info->data, cursor);
        cursor += 4u;
        assert(pack_name_offset < strings->size &&
               strcmp((const char*)strings->data + pack_name_offset,
                      "Values") == 0);
        while (cursor < info->size && info->data[cursor] == 52u) {
            uint32_t value_type_offset;
            uint32_t value_type_name_offset;
            int64_t value;
            assert(cursor + 5u <= info->size);
            ++cursor;
            value_type_offset = read_u32(info->data, cursor);
            cursor += 4u;
            assert(value_type_offset + 5u <= info->size &&
                   info->data[value_type_offset] == 5u);
            value_type_name_offset = read_u32(info->data,
                                              value_type_offset + 1u);
            assert(value_type_name_offset < strings->size &&
                   strcmp((const char*)strings->data +
                              value_type_name_offset,
                          "short") == 0);
            value = read_sleb(info->data, info->size, &cursor);
            assert((value_count == 0u && value == -4) ||
                   (value_count == 1u && value == 19));
            ++value_count;
        }
        assert(value_count == 2u && cursor < info->size &&
               info->data[cursor++] == 0u);
        ++found;
    }
    assert(found == 1u);
    objfile_free(object);
}

static void verify_function_template_nested_dependent_scalar_value_die(
    const char* path, uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    unsigned found = 0u;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && strings != NULL);

    for (uint64_t die = 11u; die + 5u <= info->size; ++die) {
        uint32_t function_name_offset;
        uint64_t cursor;
        uint32_t name_offset;
        uint32_t owner_type_offset;
        uint32_t owner_name_offset;
        uint32_t value_type_offset;
        uint32_t value_type_name_offset;
        if (info->data[die] != 2u) continue;
        function_name_offset = read_u32(info->data, die + 1u);
        if (function_name_offset >= strings->size ||
            strcmp((const char*)strings->data + function_name_offset,
                   "debug_template_function_nested_scalar_value") != 0) {
            continue;
        }

        cursor = function_template_children_start(info, die, address_size);
        assert(cursor < info->size && info->data[cursor++] == 47u);
        assert(cursor + 8u <= info->size);
        name_offset = read_u32(info->data, cursor);
        owner_type_offset = read_u32(info->data, cursor + 4u);
        cursor += 8u;
        assert(name_offset < strings->size &&
               strcmp((const char*)strings->data + name_offset,
                      "Owner") == 0);
        assert(owner_type_offset + 5u <= info->size &&
               info->data[owner_type_offset] == 12u);
        owner_name_offset = read_u32(info->data, owner_type_offset + 1u);
        assert(owner_name_offset < strings->size &&
               strcmp((const char*)strings->data + owner_name_offset,
                      "DebugTemplateNestedValueOwner") == 0);

        assert(cursor < info->size && info->data[cursor++] == 48u);
        assert(cursor + 8u <= info->size);
        name_offset = read_u32(info->data, cursor);
        value_type_offset = read_u32(info->data, cursor + 4u);
        cursor += 8u;
        assert(name_offset < strings->size &&
               strcmp((const char*)strings->data + name_offset,
                      "Value") == 0);
        assert(value_type_offset + 5u <= info->size &&
               info->data[value_type_offset] == 5u);
        value_type_name_offset = read_u32(info->data,
                                          value_type_offset + 1u);
        assert(value_type_name_offset < strings->size &&
               strcmp((const char*)strings->data + value_type_name_offset,
                      "short") == 0);
        assert(read_sleb(info->data, info->size, &cursor) == -6);
        ++found;
    }
    assert(found == 1u);
    objfile_free(object);
}

static void verify_enum_template_value_dies(const char* path,
                                            uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    uint64_t enum_offset;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    unsigned class_count = 0u;
    unsigned function_count = 0u;
    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && abbrev != NULL && strings != NULL);
    enum_offset = verify_enum_type_underlying(
        info, abbrev, strings, "DebugTemplateDependentValueEnum", "short",
        2u, 0x05u, true);

    for (uint64_t die = 11u; die + 9u <= info->size; ++die) {
        uint32_t name_offset;
        uint32_t type_offset;
        uint64_t cursor;
        if (info->data[die] != 12u) continue;
        name_offset = read_u32(info->data, die + 1u);
        if (name_offset >= strings->size ||
            strncmp((const char*)strings->data + name_offset,
                    "DebugTemplateEnumValue.",
                    sizeof("DebugTemplateEnumValue.") - 1u) != 0) {
            continue;
        }
        cursor = die + 9u;
        assert(cursor < info->size && info->data[cursor++] == 48u);
        assert(cursor + 8u <= info->size);
        name_offset = read_u32(info->data, cursor);
        type_offset = read_u32(info->data, cursor + 4u);
        cursor += 8u;
        assert(name_offset < strings->size &&
               strcmp((const char*)strings->data + name_offset,
                      "Value") == 0 &&
               type_offset == enum_offset &&
               read_sleb(info->data, info->size, &cursor) == -3);
        ++class_count;
    }

    for (uint64_t die = 11u; die + 5u <= info->size; ++die) {
        uint32_t name_offset;
        uint32_t type_offset;
        uint64_t cursor;
        if (info->data[die] != 2u) continue;
        name_offset = read_u32(info->data, die + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset,
                   "debug_template_function_enum_value") != 0) {
            continue;
        }
        cursor = function_template_children_start(info, die, address_size);
        assert(cursor < info->size && info->data[cursor++] == 48u);
        assert(cursor + 8u <= info->size);
        name_offset = read_u32(info->data, cursor);
        type_offset = read_u32(info->data, cursor + 4u);
        cursor += 8u;
        assert(name_offset < strings->size &&
               strcmp((const char*)strings->data + name_offset,
                      "Value") == 0 &&
               type_offset == enum_offset &&
               read_sleb(info->data, info->size, &cursor) == 12);
        ++function_count;
    }
    assert(class_count == 1u && function_count == 1u);
    objfile_free(object);
}

static uint8_t read_inline_attribute(const ObjSection* info,
                                     uint64_t function_offset,
                                     uint64_t address_size,
                                     bool has_return_type)
{
    uint64_t offset = function_offset + 1u + 4u + address_size + 4u + 4u +
                      4u + 4u + 1u + 4u + (has_return_type ? 4u : 0u);
    assert(info != NULL);
    assert(offset + 4u < info->size);
    /* DW_FORM_exprloc for the current frame-base expression is two bytes:
     * length 1 followed by DW_OP_breg{5,6} and a zero SLEB displacement. */
    assert(info->data[offset] == 2u);
    offset += 3u;
    return info->data[offset];
}

static uint8_t read_prototyped_attribute(const ObjSection* info,
                                         const ObjSection* strings,
                                         const char* function_name,
                                         uint64_t address_size)
{
    uint64_t function_offset = find_function_die(
        info, strings, function_name, address_size);
    uint64_t offset;
    assert(function_offset != UINT64_MAX);
    offset = function_offset + 1u + 4u + address_size + 4u + 4u + 4u +
             4u + 1u + 4u + 4u;
    assert(offset + 4u < info->size);
    assert(info->data[offset] == 2u); /* DW_AT_frame_base exprloc */
    offset += 4u; /* frame base expression and DW_AT_inline */
    return info->data[offset];
}

static uint32_t read_return_type_ref(const ObjSection* info,
                                     uint64_t function_offset,
                                     uint64_t address_size)
{
    uint64_t offset = function_offset + 1u + 4u + address_size + 4u + 4u +
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
    ObjSection* locations;
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
    locations = objfile_get_section(object, ".debug_loc");
    assert(info != NULL && info->type == SECT_DEBUG_INFO);
    assert(abbrev != NULL && abbrev->type == SECT_DEBUG_ABBREV);
    assert(strings != NULL && strings->type == SECT_DEBUG_STR);
    assert(contains_byte_pair(abbrev->data, abbrev->size, 0x27u, 0x0cu));
    assert(frame != NULL && frame->type == SECT_DEBUG_FRAME);
    assert(frame->flags == 0u && frame->size == frame->memory_size);
    assert(frame->size > 24u && frame->relocs != NULL);
    assert(frame->data[4] == 0xffu && frame->data[5] == 0xffu &&
           frame->data[6] == 0xffu && frame->data[7] == 0xffu);
    assert(contains_byte(frame->data, frame->size, 0x0cu));
    assert(contains_byte(frame->data, frame->size, 0x0du));
    verify_first_frame_fde(frame, architecture);
    if (strcmp(source_file, "tests/debug_info.c") == 0) {
        verify_multiple_return_frame_fde(
            frame, "tests/debug_info.c::debug_aggregate_sum", architecture);
    }
    assert(info->relocs != NULL && info->size > 16u);
    assert(info->data[4] == 4u && info->data[5] == 0u);
    assert(info->data[16] == (uint8_t)language);
    assert(info->data[17] == (uint8_t)(language >> 8));
    {
        uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
        uint64_t function_offset = find_function_die(
            info, strings, function_name, address_size);
        uint64_t column_offset = function_offset + 1u + 4u + address_size +
                                  4u + 4u + 4u;
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
        uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
        bool has_location;
        bool is_location_list;
        int64_t frame_offset;
        assert(read_prototyped_attribute(
                   info, strings, "debug_prototype_function", address_size) ==
               1u);
        assert(read_prototyped_attribute(
                   info, strings, "debug_no_prototype_function", address_size) ==
               0u);
        assert(read_prototyped_attribute(
                   info, strings, "debug_info_parameters", address_size) ==
               1u);
        verify_vla_variable_location(info, strings, locations, architecture);
        if (strcmp(source_file, "tests/debug_info.c") == 0) {
            int64_t loop_scope_start;
            int64_t loop_scope_end;
            int64_t outer_scope_start;
            int64_t outer_scope_end;
            assert(locations != NULL && locations->type == SECT_DEBUG_LOC &&
                   locations->relocs != NULL);
            assert(find_variable_location(
                info, strings, locations, "debug_vla_values", architecture,
                &has_location, &is_location_list, &frame_offset));
            assert(has_location && is_location_list && frame_offset < 0);
            assert(find_variable_location(
                info, strings, locations, "nested", architecture,
                &has_location, &is_location_list, &frame_offset));
            assert(has_location && is_location_list && frame_offset < 0);
            assert(verify_location_starts_after_simple_scope(
                info, strings, locations, "nested",
                architecture == ARCH_X64 ? 8u : 4u));
            assert(find_location_list_bounds(
                info, strings, locations, "loop_index",
                "debug_for_initializer_scope", architecture,
                &loop_scope_start, &loop_scope_end));
            assert(find_location_list_bounds(
                info, strings, locations, "outer_value",
                "debug_for_initializer_scope", architecture,
                &outer_scope_start, &outer_scope_end));
            assert(loop_scope_start < loop_scope_end);
            assert(outer_scope_start < loop_scope_start);
            assert(loop_scope_end < outer_scope_end);
        }
        verify_vla_bound_dies(info, abbrev, architecture);
        verify_multidimensional_vla_type(info, strings);
        verify_vla_pointer_type(info, strings,
                                "debug_vla_parameter_values", 3u,
                                architecture);
        verify_vla_pointer_type(info, strings,
                                "debug_vla_pointer_values", 3u,
                                architecture);
        verify_vla_pointer_type(info, strings,
                                "debug_vla_local_pointer_values", 4u,
                                architecture);
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
        verify_tls_global_variable(object, info, strings, "debug_tls_data");
        verify_tls_variable_location(
            object, info, strings, "debug_tls_static_local",
            NULL, false);
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
                if (info->data[offset] != 4u &&
                    info->data[offset] != 39u) continue;
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
    assert(objfile_get_section(object, ".debug_ranges") == NULL);
    assert(objfile_get_section(object, ".debug_loc") == NULL);
    objfile_free(object);
}

static void verify_linked_image_excludes_debug_sections(
    const char* path, uint16_t architecture)
{
    FILE* file = fopen(path, "rb");
    RinHeaderV3 header;
    RinSectionV3* sections;
    uint32_t unwind_count = 0u;
    assert(file != NULL);
    assert(fread(&header, sizeof(header), 1u, file) == 1u);
    assert(header.magic == RIN_IMAGE_MAGIC &&
           header.version == RIN_IMAGE_VERSION_3 &&
           header.architecture == architecture &&
           header.abi_major == RIN_IMAGE_ABI_MAJOR &&
           header.abi_minor >= RIN_IMAGE_ABI_MINOR_UNWIND_FORMAT);
    assert(header.section_count > 0u && header.section_count <= 64u);
    sections = calloc(header.section_count, sizeof(*sections));
    assert(sections != NULL);
    assert(header.section_table_offset <= (uint64_t)LONG_MAX);
    assert(fseek(file, (long)header.section_table_offset, SEEK_SET) == 0);
    assert(fread(sections, sizeof(*sections), header.section_count, file) ==
           header.section_count);
    for (uint32_t index = 0u; index < header.section_count; ++index) {
        assert(sections[index].type >= RIN_IMAGE_SECTION_CODE &&
               sections[index].type <= RIN_IMAGE_SECTION_FINI_ARRAY);
        if (sections[index].type == RIN_IMAGE_SECTION_UNWIND) {
            ++unwind_count;
            assert(sections[index].flags ==
                   (RIN_IMAGE_SECTION_READ |
                    RIN_IMAGE_SECTION_UNWIND_DWARF_DEBUG_FRAME));
        }
    }
    assert(unwind_count == 1u);
    free(sections);
    assert(fclose(file) == 0);
}

static void verify_global_initializer_unwind(const char* path,
                                             uint16_t architecture,
                                             bool expect_debug_info)
{
    const char* initializer_symbol =
        "tests/c_global_initializers.c::__rcc_global_init";
    ObjectFile* object = objfile_read(path);
    ObjSection* unwind;
    assert(object != NULL && object->arch == architecture);
    unwind = objfile_get_section(object, ".unwind");
    assert(unwind != NULL);
    assert(has_relocation_symbol(
        unwind, initializer_symbol,
        architecture == ARCH_X64 ? RELOC_ABS64 : RELOC_ABS32U));
    assert((objfile_get_section(object, ".debug_info") != NULL) ==
           expect_debug_info);
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
    ObjSection* ranges;
    ObjSection* locations;
    bool has_location;
    bool is_location_list;
    int64_t value_offset;
    int64_t local_offset;
    int64_t nested_offset;
    int64_t aligned_offset;
    int64_t branch_offset;
    assert(object != NULL && object->arch == architecture);
    line = objfile_get_section(object, ".debug_line");
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    frame = objfile_get_section(object, ".debug_frame");
    ranges = objfile_get_section(object, ".debug_ranges");
    locations = objfile_get_section(object, ".debug_loc");
    assert(line != NULL && info != NULL && strings != NULL && frame != NULL);
    assert(contains_bytes(line->data, line->size,
                          "tests/verified_backend_debug.c"));
    assert(contains_bytes(strings->data, strings->size,
                          "verified_debug_static"));
    assert(contains_bytes(strings->data, strings->size,
                          "verified_debug_entry"));
    assert(line->relocs != NULL && info->relocs != NULL &&
           frame->relocs != NULL);
    assert(ranges != NULL && ranges->relocs != NULL && ranges->size > 0u);
    assert(locations != NULL && locations->relocs != NULL &&
           locations->size > 0u);
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
    verify_multiple_return_frame_fde(
        frame, "tests/verified_backend_debug.c::verified_debug_multi_return",
        architecture);
    assert(find_function_die(info, strings, "verified_debug_static",
                             architecture == ARCH_X64 ? 8u : 4u) !=
           UINT64_MAX);
    assert(find_function_die(info, strings, "verified_debug_entry",
                             architecture == ARCH_X64 ? 8u : 4u) !=
           UINT64_MAX);
    assert(find_function_die(
        info, strings, "verified_debug_preserved_registers",
        architecture == ARCH_X64 ? 8u : 4u) != UINT64_MAX);
    verify_saved_callee_register_rules(
        frame, "verified_debug_preserved_registers", architecture);
    assert(find_lexical_block_local(
        info, strings, "local", architecture == ARCH_X64 ? 8u : 4u));
    assert(find_lexical_block_local(
        info, strings, "nested", architecture == ARCH_X64 ? 8u : 4u));
    assert(find_variable_location(info, strings, locations, "value",
                                  architecture, &has_location,
                                  &is_location_list, &value_offset));
    assert(has_location && value_offset < 0 && value_offset % 4 == 0);
    assert(find_variable_location(info, strings, locations, "local",
                                  architecture, &has_location,
                                  &is_location_list, &local_offset));
    assert(has_location && local_offset < 0 && local_offset % 4 == 0 &&
           local_offset != value_offset);
    assert(find_variable_location(info, strings, locations, "nested",
                                  architecture, &has_location,
                                  &is_location_list, &nested_offset));
    assert(has_location && nested_offset < 0 && nested_offset % 4 == 0 &&
           nested_offset != value_offset && nested_offset != local_offset);
    assert(find_variable_location(info, strings, locations, "aligned",
                                  architecture, &has_location,
                                  &is_location_list, &aligned_offset));
    assert(has_location && aligned_offset < 0 && is_location_list);
    assert(find_variable_location(info, strings, locations, "branch_value",
                                  architecture, &has_location,
                                  &is_location_list, &branch_offset));
    assert(has_location && branch_offset < 0 && is_location_list);
    assert(verify_location_starts_after_ranges_scope(
        info, strings, locations, ranges, "no_code_local",
        architecture == ARCH_X64 ? 8u : 4u));
    assert_variable_has_no_location(info, strings, locations,
                                    "no_code_unlocated", architecture);
    {
        int64_t branch_start;
        int64_t branch_end;
        int64_t result_start;
        int64_t result_end;
        int64_t loop_start;
        int64_t loop_end;
        int64_t outer_start;
        int64_t outer_end;
        assert(find_location_list_bounds(
            info, strings, locations, "branch_value",
            "verified_debug_branch_scope", architecture,
            &branch_start, &branch_end));
        assert(find_location_list_bounds(
            info, strings, locations, "result",
            "verified_debug_branch_scope", architecture,
            &result_start, &result_end));
        assert(result_start < branch_start && branch_start < branch_end);
        assert(branch_end < result_end);
        assert(find_location_list_bounds(
            info, strings, locations, "verified_loop_index",
            "verified_debug_for_scope", architecture,
            &loop_start, &loop_end));
        assert(find_location_list_bounds(
            info, strings, locations, "verified_outer_value",
            "verified_debug_for_scope", architecture,
            &outer_start, &outer_end));
        assert(loop_start < loop_end && outer_start < outer_end);
        assert(loop_end < outer_end);
    }
    verify_overaligned_location_mask(locations, architecture);
    assert(is_location_list);
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
    verify_tls_global_variable(object, info, strings, "verified_debug_tls");
    assert(line->relocs != NULL && info->relocs != NULL &&
           frame->relocs != NULL);
    verify_first_frame_fde(frame, architecture);
    objfile_free(object);
}

static void verify_verified_fallback_debug_object(const char* path,
                                                  uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* line;
    ObjSection* info;
    ObjSection* strings;
    ObjSection* frame;
    ObjSection* locations;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint64_t prefix_die;
    uint64_t fallback_die;
    assert(object != NULL && object->arch == architecture);
    line = objfile_get_section(object, ".debug_line");
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    frame = objfile_get_section(object, ".debug_frame");
    locations = objfile_get_section(object, ".debug_loc");
    assert(line != NULL && info != NULL && strings != NULL &&
           frame != NULL);
    assert(contains_bytes(line->data, line->size,
                          "tests/verified_backend_debug_fallback.cpp"));
    prefix_die = find_function_die(info, strings,
                                   "verified_debug_fallback_prefix",
                                   address_size);
    fallback_die = find_function_die(info, strings,
                                     "verified_debug_fallback_conversion",
                                     address_size);
    assert(prefix_die != UINT64_MAX && fallback_die != UINT64_MAX);
    assert(prefix_die < fallback_die);
    {
        const char* variable_names[] = {"local", "nested"};
        for (size_t index = 0u;
             index < sizeof(variable_names) / sizeof(variable_names[0]);
             ++index) {
            bool has_location;
            bool is_location_list;
            int64_t frame_offset;
            assert(find_variable_location(
                info, strings, locations, variable_names[index],
                architecture, &has_location, &is_location_list,
                &frame_offset));
            if (is_location_list) {
                int64_t range_start;
                int64_t range_end;
                assert(find_location_list_bounds(
                    info, strings, locations, variable_names[index],
                    "verified_debug_fallback_prefix", architecture,
                    &range_start, &range_end));
                assert(range_start >= 0 && range_start < range_end);
            }
        }
    }
    objfile_free(object);
}

static void verify_inline_integer_expression_normalization(
    const uint8_t* data, uint64_t expression_end, uint64_t* expression_offset,
    uint16_t architecture, bool is_unsigned, uint8_t value_size) {
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    if (value_size == address_size) return;
    if (is_unsigned) {
        assert(*expression_offset < expression_end &&
               data[(*expression_offset)++] == 0x10u &&
               read_uleb(data, expression_end, expression_offset) ==
                   ((UINT64_C(1) << ((unsigned int)value_size * 8u)) - 1u) &&
               *expression_offset < expression_end &&
               data[(*expression_offset)++] == 0x1au);
        return;
    }
    {
        uint64_t shift = (address_size - value_size) * 8u;
        assert(*expression_offset < expression_end &&
               data[(*expression_offset)++] == 0x10u &&
               read_uleb(data, expression_end, expression_offset) == shift &&
               *expression_offset < expression_end &&
               data[(*expression_offset)++] == 0x24u &&
               *expression_offset < expression_end &&
               data[(*expression_offset)++] == 0x10u &&
               read_uleb(data, expression_end, expression_offset) == shift &&
               *expression_offset < expression_end &&
               data[(*expression_offset)++] == 0x26u);
    }
}

static int64_t verify_inline_integer_signed_bits(
    uint64_t value, unsigned int address_bits) {
    uint64_t mask = address_bits == 64u
        ? UINT64_MAX : (UINT64_C(1) << address_bits) - 1u;
    uint64_t sign_bit = UINT64_C(1) << (address_bits - 1u);
    value &= mask;
    if ((value & sign_bit) == 0u) return (int64_t)value;
    return -1 - (int64_t)(mask ^ value);
}

static bool verify_inline_integer_comparison_value(
    uint64_t left, uint64_t right, bool is_unsigned,
    unsigned int bit_width, uint8_t operation) {
    uint64_t mask;
    int64_t signed_left;
    int64_t signed_right;
    assert(bit_width > 0u && bit_width <= 64u);
    mask = bit_width == 64u
        ? UINT64_MAX : (UINT64_C(1) << bit_width) - 1u;
    left &= mask;
    right &= mask;
    signed_left = verify_inline_integer_signed_bits(left, bit_width);
    signed_right = verify_inline_integer_signed_bits(right, bit_width);
    if (is_unsigned) {
        switch (operation) {
            case 0x29u: return left == right;
            case 0x2au: return left >= right;
            case 0x2bu: return left > right;
            case 0x2cu: return left <= right;
            case 0x2du: return left < right;
            case 0x2eu: return left != right;
            default: assert(!"invalid inline integer comparison operation");
        }
    } else {
        switch (operation) {
            case 0x29u: return signed_left == signed_right;
            case 0x2au: return signed_left >= signed_right;
            case 0x2bu: return signed_left > signed_right;
            case 0x2cu: return signed_left <= signed_right;
            case 0x2du: return signed_left < signed_right;
            case 0x2eu: return signed_left != signed_right;
            default: assert(!"invalid inline integer comparison operation");
        }
    }
    return false;
}

static uint64_t evaluate_inline_integer_comparison_expression_with_secondary(
    const uint8_t* data, uint64_t expression_end, uint64_t expression_offset,
    uint16_t architecture, int64_t source_frame_offset,
    uint8_t source_value_size, uint64_t source_value,
    int64_t secondary_frame_offset, uint8_t secondary_value_size,
    uint64_t secondary_value) {
    uint64_t stack[16];
    size_t stack_size = 0u;
    int64_t pending_frame_offset = 0;
    bool has_pending_frame_offset = false;
    unsigned int address_bits = architecture == ARCH_X64 ? 64u : 32u;
    uint64_t address_mask = address_bits == 64u
        ? UINT64_MAX : (UINT64_C(1) << address_bits) - 1u;
    while (expression_offset < expression_end) {
        uint8_t opcode = data[expression_offset++];
        uint64_t lhs;
        uint64_t rhs;
        if (opcode == 0x75u || opcode == 0x76u) {
            pending_frame_offset = read_sleb(
                data, expression_end, &expression_offset);
            has_pending_frame_offset = true;
        } else if (opcode == 0x94u) {
            uint8_t value_size;
            uint64_t value_mask;
            bool i686_wide_secondary_word =
                architecture == ARCH_X86 && secondary_value_size == 8u &&
                secondary_frame_offset <= INT64_MAX - 4 &&
                pending_frame_offset == secondary_frame_offset + 4;
            bool i686_wide_source = architecture == ARCH_X86 &&
                source_value_size == 8u;
            assert(expression_offset < expression_end &&
                   stack_size < sizeof(stack) / sizeof(stack[0]) &&
                   has_pending_frame_offset);
            value_size = data[expression_offset++];
            if (secondary_value_size != 0u &&
                (pending_frame_offset == secondary_frame_offset ||
                 i686_wide_secondary_word)) {
                /* i686 expressions read a frame-backed 64-bit RHS by words. */
                if (architecture == ARCH_X86 &&
                    secondary_value_size == 8u && value_size == 4u) {
                    stack[stack_size++] = pending_frame_offset ==
                            secondary_frame_offset
                        ? (uint32_t)secondary_value
                        : (uint32_t)(secondary_value >> 32u);
                } else {
                    assert(value_size == secondary_value_size);
                    value_mask = value_size == 8u ? UINT64_MAX
                        : (UINT64_C(1) << (value_size * 8u)) - 1u;
                    stack[stack_size++] = secondary_value & value_mask;
                }
            } else if (i686_wide_source) {
                assert(value_size == 4u &&
                       (pending_frame_offset == source_frame_offset ||
                        pending_frame_offset == source_frame_offset + 4));
                stack[stack_size++] = pending_frame_offset == source_frame_offset
                    ? (uint32_t)source_value
                    : (uint32_t)(source_value >> 32u);
            } else {
                assert(value_size == source_value_size &&
                       pending_frame_offset == source_frame_offset);
                value_mask = value_size == 8u ? UINT64_MAX
                    : (UINT64_C(1) << (value_size * 8u)) - 1u;
                stack[stack_size++] = (uint64_t)source_value & value_mask;
            }
            has_pending_frame_offset = false;
        } else if (opcode == 0x10u) {
            assert(stack_size < sizeof(stack) / sizeof(stack[0]));
            stack[stack_size++] = read_uleb(
                data, expression_end, &expression_offset) & address_mask;
        } else if (opcode == 0x11u) {
            assert(stack_size < sizeof(stack) / sizeof(stack[0]));
            stack[stack_size++] =
                (uint64_t)read_sleb(data, expression_end, &expression_offset) &
                address_mask;
        } else if (opcode == 0x1au || opcode == 0x1cu || opcode == 0x1eu ||
                   opcode == 0x21u || opcode == 0x22u || opcode == 0x27u ||
                   (opcode >= 0x29u && opcode <= 0x2eu)) {
            assert(stack_size >= 2u);
            rhs = stack[--stack_size];
            lhs = stack[--stack_size];
            if (opcode == 0x1au) {
                stack[stack_size++] = lhs & rhs;
            } else if (opcode == 0x1cu) {
                stack[stack_size++] = (lhs - rhs) & address_mask;
            } else if (opcode == 0x1eu) {
                stack[stack_size++] = (lhs * rhs) & address_mask;
            } else if (opcode == 0x21u) {
                stack[stack_size++] = lhs | rhs;
            } else if (opcode == 0x22u) {
                stack[stack_size++] = (lhs + rhs) & address_mask;
            } else if (opcode == 0x27u) {
                stack[stack_size++] = lhs ^ rhs;
            } else if (opcode == 0x29u) {
                stack[stack_size++] = lhs == rhs;
            } else if (opcode >= 0x2au && opcode <= 0x2eu) {
                stack[stack_size++] = (uint64_t)
                    verify_inline_integer_comparison_value(
                        lhs, rhs, false, address_bits, opcode);
            }
        } else if (opcode == 0x1fu) {
            assert(stack_size != 0u);
            stack[stack_size - 1u] =
                (UINT64_C(0) - stack[stack_size - 1u]) & address_mask;
        } else if (opcode == 0x20u) {
            assert(stack_size != 0u);
            stack[stack_size - 1u] = ~stack[stack_size - 1u] & address_mask;
        } else if (opcode == 0x24u || opcode == 0x25u || opcode == 0x26u) {
            int64_t signed_lhs;
            assert(stack_size >= 2u);
            rhs = stack[--stack_size];
            lhs = stack[--stack_size];
            assert(rhs < address_bits);
            if (opcode == 0x24u) {
                stack[stack_size++] = (lhs << rhs) & address_mask;
            } else if (opcode == 0x25u) {
                stack[stack_size++] = lhs >> rhs;
            } else {
                signed_lhs = verify_inline_integer_signed_bits(
                    lhs, address_bits);
                stack[stack_size++] = (uint64_t)(signed_lhs >> rhs) &
                    address_mask;
            }
        } else if (opcode == 0x9fu) {
            assert(!has_pending_frame_offset && stack_size == 1u &&
                   expression_offset == expression_end);
            return stack[0] & address_mask;
        } else {
            fprintf(stderr,
                    "unexpected enum comparison DWARF opcode %#x at %llu\n",
                    opcode, (unsigned long long)(expression_offset - 1u));
            assert(!"unexpected opcode in enum comparison expression");
        }
    }
    assert(!"enum comparison expression has no DW_OP_stack_value");
    return 0u;
}

static uint64_t evaluate_inline_integer_comparison_expression(
    const uint8_t* data, uint64_t expression_end, uint64_t expression_offset,
    uint16_t architecture, int64_t source_frame_offset,
    uint8_t source_value_size, uint64_t source_value) {
    return evaluate_inline_integer_comparison_expression_with_secondary(
        data, expression_end, expression_offset, architecture,
        source_frame_offset, source_value_size, source_value, 0, 0u, 0u);
}

static unsigned int inline_i686_u64_constant_division_chunk_width(
    uint32_t divisor) {
    unsigned int width = 16u;
    assert(divisor != 0u && divisor <= UINT32_MAX);
    while (width > 1u &&
           divisor > (UINT64_C(2147483648) / (UINT64_C(1) << width))) {
        --width;
    }
    if (divisor > UINT32_C(1073741824)) width = 1u;
    return width;
}

static uint64_t evaluate_inline_i686_u64_constant_division(
    const uint8_t* data, uint64_t expression_end, uint64_t expression_offset,
    int64_t source_frame_offset, uint64_t dividend, uint64_t divisor) {
    uint32_t stack[128];
    size_t stack_size = 0u;
    int64_t pending_frame_offset = 0;
    bool has_pending_frame_offset = false;
    uint64_t result = 0u;
    unsigned int piece_count = 0u;
    unsigned int chunk_width = divisor <= UINT32_MAX
        ? inline_i686_u64_constant_division_chunk_width((uint32_t)divisor)
        : 1u;
    unsigned int chunk_count = divisor <= UINT32_MAX
        ? (64u + chunk_width - 1u) / chunk_width : 1u;
    size_t expected_stack_size =
        expression_offset + 1u < expression_end &&
        data[expression_offset] == 0x10u &&
        data[expression_offset + 1u] == 0u
        ? chunk_count + 1u : 1u;
    assert(divisor != 0u && source_frame_offset <= INT64_MAX - 6);
    while (expression_offset < expression_end) {
        uint8_t opcode = data[expression_offset++];
        if (opcode == 0x75u) { /* DW_OP_breg5 */
            assert(!has_pending_frame_offset);
            pending_frame_offset = read_sleb(
                data, expression_end, &expression_offset);
            has_pending_frame_offset = true;
        } else if (opcode == 0x94u) { /* DW_OP_deref_size */
            uint32_t value;
            assert(expression_offset < expression_end &&
                   data[expression_offset++] == 4u &&
                   has_pending_frame_offset &&
                   stack_size < sizeof(stack) / sizeof(stack[0]));
            if (pending_frame_offset == source_frame_offset) {
                value = (uint32_t)dividend;
            } else {
                assert(pending_frame_offset == source_frame_offset + 4);
                value = (uint32_t)(dividend >> 32u);
            }
            stack[stack_size++] = value;
            has_pending_frame_offset = false;
        } else if (opcode == 0x10u) { /* DW_OP_constu */
            assert(stack_size < sizeof(stack) / sizeof(stack[0]));
            stack[stack_size++] = (uint32_t)read_uleb(
                data, expression_end, &expression_offset);
        } else if (opcode == 0x12u) { /* DW_OP_dup */
            assert(stack_size > 0u &&
                   stack_size < sizeof(stack) / sizeof(stack[0]));
            stack[stack_size] = stack[stack_size - 1u];
            ++stack_size;
        } else if (opcode == 0x13u) { /* DW_OP_drop */
            assert(stack_size > 0u);
            --stack_size;
        } else if (opcode == 0x15u) { /* DW_OP_pick */
            unsigned int index;
            uint32_t value;
            assert(expression_offset < expression_end &&
                   stack_size < sizeof(stack) / sizeof(stack[0]));
            index = data[expression_offset++];
            assert(index < stack_size);
            value = stack[stack_size - 1u - index];
            stack[stack_size++] = value;
        } else if (opcode == 0x16u) { /* DW_OP_swap */
            uint32_t temporary;
            assert(stack_size >= 2u);
            temporary = stack[stack_size - 1u];
            stack[stack_size - 1u] = stack[stack_size - 2u];
            stack[stack_size - 2u] = temporary;
        } else if (opcode == 0x17u) { /* DW_OP_rot */
            uint32_t bottom;
            uint32_t middle;
            uint32_t top;
            assert(stack_size >= 3u);
            bottom = stack[stack_size - 3u];
            middle = stack[stack_size - 2u];
            top = stack[stack_size - 1u];
            stack[stack_size - 3u] = top;
            stack[stack_size - 2u] = bottom;
            stack[stack_size - 1u] = middle;
        } else if (opcode == 0x1au || opcode == 0x1bu ||
                   opcode == 0x1cu || opcode == 0x1du ||
                   opcode == 0x1eu || opcode == 0x27u ||
                   opcode == 0x21u || opcode == 0x22u ||
                   opcode == 0x24u || opcode == 0x25u ||
                   opcode == 0x2au || opcode == 0x2eu) {
            uint32_t lhs;
            uint32_t rhs;
            assert(stack_size >= 2u);
            rhs = stack[--stack_size];
            lhs = stack[--stack_size];
            if (opcode == 0x1au) {
                stack[stack_size++] = lhs & rhs;
            } else if (opcode == 0x21u) {
                stack[stack_size++] = lhs | rhs;
            } else if (opcode == 0x1bu || opcode == 0x1du) {
                assert(lhs <= INT32_MAX && rhs > 0u && rhs <= INT32_MAX);
                stack[stack_size++] = opcode == 0x1bu
                    ? lhs / rhs : lhs % rhs;
            } else if (opcode == 0x1cu) {
                stack[stack_size++] = lhs - rhs;
            } else if (opcode == 0x27u) {
                stack[stack_size++] = lhs ^ rhs;
            } else if (opcode == 0x2au) {
                int64_t signed_lhs = lhs <= INT32_MAX
                    ? (int64_t)lhs : (int64_t)lhs - INT64_C(0x100000000);
                int64_t signed_rhs = rhs <= INT32_MAX
                    ? (int64_t)rhs : (int64_t)rhs - INT64_C(0x100000000);
                stack[stack_size++] = signed_lhs >= signed_rhs;
            } else if (opcode == 0x2eu) {
                stack[stack_size++] = lhs != rhs;
            } else if (opcode == 0x1eu) {
                stack[stack_size++] = lhs * rhs;
            } else if (opcode == 0x22u) {
                stack[stack_size++] = lhs + rhs;
            } else if (opcode == 0x24u) {
                assert(rhs < 32u);
                stack[stack_size++] = lhs << rhs;
            } else {
                assert(rhs < 32u);
                stack[stack_size++] = lhs >> rhs;
            }
        } else if (opcode == 0x28u || opcode == 0x2fu) {
            int64_t displacement;
            uint16_t encoded;
            bool take_branch = true;
            assert(expression_offset + 2u <= expression_end);
            encoded = (uint16_t)data[expression_offset] |
                (uint16_t)((uint16_t)data[expression_offset + 1u] << 8u);
            expression_offset += 2u;
            displacement = encoded < 0x8000u
                ? (int64_t)encoded : (int64_t)encoded - 0x10000;
            if (opcode == 0x28u) {
                assert(stack_size != 0u);
                take_branch = stack[--stack_size] != 0u;
            }
            if (take_branch) {
                int64_t target = (int64_t)expression_offset + displacement;
                assert(target >= 0 && (uint64_t)target <= expression_end);
                expression_offset = (uint64_t)target;
            }
        } else if (opcode == 0x9fu) { /* DW_OP_stack_value */
            if (stack_size != expected_stack_size) {
                fprintf(stderr,
                        "i686 u64 divmod stack mismatch: divisor=%llu "
                        "stack=%zu expected=%zu pieces=%u\n",
                        (unsigned long long)divisor, stack_size,
                        expected_stack_size,
                        piece_count);
            }
            assert(stack_size == expected_stack_size);
        } else if (opcode == 0x93u) { /* DW_OP_piece */
            assert(read_uleb(data, expression_end, &expression_offset) == 4u &&
                   stack_size == expected_stack_size && piece_count < 2u);
            result |= (uint64_t)stack[--stack_size] << (piece_count * 32u);
            ++piece_count;
        } else {
            fprintf(stderr,
                    "unexpected i686 u64 divmod opcode %#x at %llu\n",
                    opcode, (unsigned long long)(expression_offset - 1u));
            assert(!"unexpected opcode in i686 u64 constant divmod");
        }
    }
    assert(!has_pending_frame_offset && stack_size == 0u && piece_count == 2u);
    return result;
}

static void verify_inline_i686_u64_constant_division_expression(
    const uint8_t* data, uint64_t expression_end, uint64_t expression_start,
    const ObjSection* info, const ObjSection* strings,
    const ObjSection* locations, const char* source_name,
    uint64_t divisor, bool remainder) {
    static const uint64_t values[] = {
        0u, 1u, 2u, 3u, 4u, UINT64_C(0xffffffff),
        UINT64_C(0x100000000), UINT64_C(0x7fffffffffffffff),
        UINT64_C(0x8000000000000000), UINT64_MAX
    };
    uint64_t offset = expression_start;
    bool has_variable_location;
    bool variable_is_location_list;
    int64_t variable_frame_offset;
    int64_t source_frame_offset;
    assert(source_name && divisor != 0u);
    assert(find_variable_location(
        info, strings, locations, source_name, ARCH_X86,
        &has_variable_location, &variable_is_location_list,
        &variable_frame_offset));
    assert(has_variable_location);
    source_frame_offset = variable_frame_offset;
    assert(source_frame_offset <= INT64_MAX - 6);
    for (size_t index = 0u; index < sizeof(values) / sizeof(values[0]);
         ++index) {
        uint64_t actual = evaluate_inline_i686_u64_constant_division(
            data, expression_end, offset, source_frame_offset,
            values[index], divisor);
        uint64_t expected = remainder
            ? values[index] % divisor : values[index] / divisor;
        if (actual != expected) {
            fprintf(stderr,
                    "i686 u64 %s mismatch: value=%#llx divisor=%llu "
                    "actual=%#llx expected=%#llx\n",
                    remainder ? "remainder" : "divide",
                    (unsigned long long)values[index],
                    (unsigned long long)divisor,
                    (unsigned long long)actual,
                    (unsigned long long)expected);
        }
        assert(actual == expected);
    }
}

static uint64_t verify_inline_unsigned_division_expression(
    const uint8_t* data, uint64_t expression_end, uint64_t expression_offset,
    uint16_t architecture, uint64_t dividend, uint64_t divisor,
    int64_t dividend_frame_offset, int64_t divisor_frame_offset,
    bool has_dynamic_divisor) {
    uint64_t stack[32];
    size_t stack_size = 1u;
    int64_t pending_frame_offset = 0;
    bool has_pending_frame_offset = false;
    unsigned int address_bits = architecture == ARCH_X64 ? 64u : 32u;
    uint64_t address_mask = address_bits == 64u
        ? UINT64_MAX : (UINT64_C(1) << address_bits) - 1u;
    stack[0] = dividend & address_mask;
    while (expression_offset < expression_end) {
        uint8_t opcode = data[expression_offset++];
        uint64_t lhs;
        uint64_t rhs;
        if (opcode == 0x75u || opcode == 0x76u) { /* DW_OP_breg5/6 */
            pending_frame_offset = read_sleb(
                data, expression_end, &expression_offset);
            has_pending_frame_offset = true;
        } else if (opcode == 0x94u) { /* DW_OP_deref_size */
            uint8_t value_size;
            assert(expression_offset < expression_end &&
                   stack_size < sizeof(stack) / sizeof(stack[0]) &&
                   has_pending_frame_offset);
            value_size = data[expression_offset++];
            assert(value_size > 0u && value_size <= address_bits / 8u);
            if (has_dynamic_divisor &&
                pending_frame_offset == divisor_frame_offset) {
                stack[stack_size++] = divisor & address_mask;
            } else {
                assert(pending_frame_offset == dividend_frame_offset);
                stack[stack_size++] = dividend & address_mask;
            }
            has_pending_frame_offset = false;
        } else if (opcode == 0x10u) { /* DW_OP_constu */
            assert(stack_size < sizeof(stack) / sizeof(stack[0]));
            stack[stack_size++] =
                read_uleb(data, expression_end, &expression_offset) &
                address_mask;
        } else if (opcode == 0x11u) { /* DW_OP_consts */
            assert(stack_size < sizeof(stack) / sizeof(stack[0]));
            stack[stack_size++] =
                (uint64_t)read_sleb(data, expression_end, &expression_offset) &
                address_mask;
        } else if (opcode == 0x12u) { /* DW_OP_dup */
            assert(stack_size != 0u &&
                   stack_size < sizeof(stack) / sizeof(stack[0]));
            stack[stack_size] = stack[stack_size - 1u];
            ++stack_size;
        } else if (opcode == 0x13u) { /* DW_OP_drop */
            assert(stack_size != 0u);
            --stack_size;
        } else if (opcode == 0x15u) { /* DW_OP_pick */
            uint8_t index;
            assert(expression_offset < expression_end &&
                   stack_size < sizeof(stack) / sizeof(stack[0]));
            index = data[expression_offset++];
            assert((size_t)index < stack_size);
            stack[stack_size] = stack[stack_size - 1u - index];
            ++stack_size;
        } else if (opcode == 0x16u) { /* DW_OP_swap */
            uint64_t temporary;
            assert(stack_size >= 2u);
            temporary = stack[stack_size - 1u];
            stack[stack_size - 1u] = stack[stack_size - 2u];
            stack[stack_size - 2u] = temporary;
        } else if (opcode == 0x17u) { /* DW_OP_rot */
            uint64_t bottom;
            uint64_t middle;
            uint64_t top;
            assert(stack_size >= 3u);
            bottom = stack[stack_size - 3u];
            middle = stack[stack_size - 2u];
            top = stack[stack_size - 1u];
            stack[stack_size - 3u] = top;
            stack[stack_size - 2u] = bottom;
            stack[stack_size - 1u] = middle;
        } else if (opcode == 0x1au || opcode == 0x27u ||
                   opcode == 0x1cu || opcode == 0x1eu ||
                   opcode == 0x22u || opcode == 0x24u ||
                   opcode == 0x25u || opcode == 0x1bu ||
                   opcode == 0x2au) {
            assert(stack_size >= 2u);
            rhs = stack[--stack_size];
            lhs = stack[--stack_size];
            if (opcode == 0x1au) {
                stack[stack_size++] = lhs & rhs;
            } else if (opcode == 0x27u) {
                stack[stack_size++] = lhs ^ rhs;
            } else if (opcode == 0x1cu) {
                stack[stack_size++] = (lhs - rhs) & address_mask;
            } else if (opcode == 0x1eu) {
                stack[stack_size++] = (lhs * rhs) & address_mask;
            } else if (opcode == 0x22u) {
                stack[stack_size++] = (lhs + rhs) & address_mask;
            } else if (opcode == 0x24u) {
                assert(rhs < address_bits);
                stack[stack_size++] = (lhs << rhs) & address_mask;
            } else if (opcode == 0x25u) {
                assert(rhs < address_bits);
                stack[stack_size++] = lhs >> rhs;
            } else if (opcode == 0x1bu) {
                int64_t signed_lhs = verify_inline_integer_signed_bits(
                    lhs, address_bits);
                int64_t signed_rhs = verify_inline_integer_signed_bits(
                    rhs, address_bits);
                assert(signed_rhs != 0 &&
                       !(signed_lhs == INT64_MIN && signed_rhs == -1));
                stack[stack_size++] =
                    (uint64_t)(signed_lhs / signed_rhs) & address_mask;
            } else {
                stack[stack_size++] =
                    verify_inline_integer_signed_bits(lhs, address_bits) >=
                    verify_inline_integer_signed_bits(rhs, address_bits);
            }
        } else if (opcode == 0x28u || opcode == 0x2fu) {
            int64_t displacement;
            uint16_t encoded;
            bool take_branch = true;
            assert(expression_offset + 2u <= expression_end);
            encoded = (uint16_t)data[expression_offset] |
                (uint16_t)((uint16_t)data[expression_offset + 1u] << 8u);
            expression_offset += 2u;
            displacement = encoded < 0x8000u
                ? (int64_t)encoded : (int64_t)encoded - 0x10000;
            if (opcode == 0x28u) {
                assert(stack_size != 0u);
                take_branch = stack[--stack_size] != 0u;
            }
            if (take_branch) {
                int64_t target = (int64_t)expression_offset + displacement;
                assert(target >= 0 && (uint64_t)target <= expression_end);
                expression_offset = (uint64_t)target;
            }
        } else if (opcode == 0x9fu) { /* DW_OP_stack_value */
            assert(stack_size == 1u && expression_offset == expression_end);
            return stack[0] & address_mask;
        } else {
            assert(!"unexpected opcode in unsigned divmod expression");
            return 0u;
        }
    }
    assert(!"unsigned divmod expression has no DW_OP_stack_value");
    return 0u;
}

static void verify_inline_unsigned_division_expression_values(
    const uint8_t* data, uint64_t expression_end, uint64_t expression_offset,
    uint16_t architecture, bool remainder, uint64_t fixed_divisor,
    uint8_t value_size, int64_t dividend_frame_offset,
    int64_t divisor_frame_offset,
    bool has_dynamic_divisor) {
    unsigned int value_bits = (unsigned int)value_size * 8u;
    uint64_t sign_bit = UINT64_C(1) << (value_bits - 1u);
    uint64_t value_mask = value_bits == 64u
        ? UINT64_MAX : (UINT64_C(1) << value_bits) - 1u;
    const uint64_t values[] = {
        0u, 1u, 2u, 3u, sign_bit - 1u, sign_bit, sign_bit + 1u,
        value_mask - 1u, value_mask
    };
    assert(value_size > 0u && value_bits <= 64u &&
           (has_dynamic_divisor ||
            (fixed_divisor != 0u && fixed_divisor <= value_mask)));
    for (size_t index = 0u; index < sizeof(values) / sizeof(values[0]);
         ++index) {
        uint64_t divisor = has_dynamic_divisor
            ? (sign_bit | (values[index] >> 2u)) : fixed_divisor;
        assert(divisor != 0u && divisor <= value_mask);
        uint64_t expected = remainder
            ? values[index] % divisor : values[index] / divisor;
        uint64_t actual = verify_inline_unsigned_division_expression(
            data, expression_end, expression_offset, architecture,
            values[index] & value_mask, divisor, dividend_frame_offset,
            divisor_frame_offset, has_dynamic_divisor);
        assert(actual == expected);
    }
}

static void verify_inline_expression_byte(
    const uint8_t* data, uint64_t expression_end, uint64_t* offset,
    uint8_t expected) {
    assert(*offset < expression_end && data[(*offset)++] == expected);
}

static void verify_inline_expression_frame_u32(
    const uint8_t* data, uint64_t expression_end, uint64_t* offset,
    int64_t expected_frame_offset) {
    verify_inline_expression_byte(data, expression_end, offset, 0x75u);
    assert(read_sleb(data, expression_end, offset) == expected_frame_offset);
    verify_inline_expression_byte(data, expression_end, offset, 0x94u);
    verify_inline_expression_byte(data, expression_end, offset, 4u);
}

static void verify_inline_expression_frame_for_architecture(
    const uint8_t* data, uint64_t expression_end, uint64_t* offset,
    int architecture, int64_t expected_frame_offset, uint8_t value_size) {
    verify_inline_expression_byte(
        data, expression_end, offset,
        architecture == ARCH_X64 ? 0x76u : 0x75u);
    assert(read_sleb(data, expression_end, offset) == expected_frame_offset);
    verify_inline_expression_byte(data, expression_end, offset, 0x94u);
    verify_inline_expression_byte(data, expression_end, offset, value_size);
}

static void verify_inline_expression_constu(
    const uint8_t* data, uint64_t expression_end, uint64_t* offset,
    uint64_t expected) {
    verify_inline_expression_byte(data, expression_end, offset, 0x10u);
    assert(read_uleb(data, expression_end, offset) == expected);
}

static void verify_inline_u64_add_constant_expression(
    const uint8_t* data, uint64_t expression_end, uint64_t expression_start,
    const ObjSection* info, const ObjSection* strings,
    const ObjSection* locations, const char* source_name,
    int64_t source_frame_offset, uint64_t constant, bool is_unsigned) {
    static const uint64_t unsigned_values[] = {
        0u, 1u, UINT64_C(0xffffffff), UINT64_C(0x100000000),
        UINT64_C(0x1fffffffe), UINT64_C(0x7fffffffffffffff),
        UINT64_C(0xfffffffffffffffe), UINT64_MAX
    };
    static const uint64_t signed_values[] = {
        0u, 1u, UINT64_MAX, UINT64_C(0x7fffffffffffffff),
        UINT64_C(0x8000000000000007), UINT64_C(0x8000000000000008),
        UINT64_C(0xfffffffffffffff9), UINT64_C(0x8000000012345678)
    };
    const uint64_t* values = is_unsigned ? unsigned_values : signed_values;
    size_t value_count = is_unsigned
        ? sizeof(unsigned_values) / sizeof(unsigned_values[0])
        : sizeof(signed_values) / sizeof(signed_values[0]);
    uint64_t offset = expression_start;
    uint32_t low_constant = (uint32_t)constant;
    uint32_t high_constant = (uint32_t)(constant >> 32u);
    uint64_t sign_bit = UINT64_C(0x80000000);
    bool has_variable_location;
    bool variable_is_location_list;
    int64_t variable_frame_offset;

    verify_inline_expression_frame_u32(
        data, expression_end, &offset, source_frame_offset);
    verify_inline_expression_byte(data, expression_end, &offset, 0x10u);
    assert(read_uleb(data, expression_end, &offset) == low_constant);
    verify_inline_expression_byte(data, expression_end, &offset, 0x22u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x9fu);
    verify_inline_expression_byte(data, expression_end, &offset, 0x93u);
    assert(read_uleb(data, expression_end, &offset) == 4u);

    assert(source_frame_offset <= INT64_MAX - 4);
    verify_inline_expression_frame_u32(
        data, expression_end, &offset, source_frame_offset + 4);
    verify_inline_expression_byte(data, expression_end, &offset, 0x10u);
    assert(read_uleb(data, expression_end, &offset) == high_constant);
    verify_inline_expression_byte(data, expression_end, &offset, 0x22u);
    verify_inline_expression_frame_u32(
        data, expression_end, &offset, source_frame_offset);
    verify_inline_expression_byte(data, expression_end, &offset, 0x10u);
    assert(read_uleb(data, expression_end, &offset) == low_constant);
    verify_inline_expression_byte(data, expression_end, &offset, 0x22u);
    verify_inline_expression_frame_u32(
        data, expression_end, &offset, source_frame_offset);
    verify_inline_expression_byte(data, expression_end, &offset, 0x10u);
    assert(read_uleb(data, expression_end, &offset) == sign_bit);
    verify_inline_expression_byte(data, expression_end, &offset, 0x27u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x16u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x10u);
    assert(read_uleb(data, expression_end, &offset) == sign_bit);
    verify_inline_expression_byte(data, expression_end, &offset, 0x27u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x16u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x2du);
    verify_inline_expression_byte(data, expression_end, &offset, 0x22u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x9fu);
    verify_inline_expression_byte(data, expression_end, &offset, 0x93u);
    assert(read_uleb(data, expression_end, &offset) == 4u);
    assert(offset == expression_end);

    assert(find_variable_location(
        info, strings, locations, source_name, ARCH_X86,
        &has_variable_location, &variable_is_location_list,
        &variable_frame_offset));
    assert(has_variable_location &&
           variable_frame_offset == source_frame_offset);
    for (size_t index = 0u; index < value_count; ++index) {
        uint64_t value = values[index];
        uint64_t low_sum = (uint64_t)(uint32_t)value + low_constant;
        uint32_t low_result = (uint32_t)low_sum;
        uint32_t high_result = (uint32_t)(value >> 32u) + high_constant +
            (uint32_t)(low_sum >> 32u);
        uint64_t actual = ((uint64_t)high_result << 32u) | low_result;
        assert(actual == value + constant);
    }
}

typedef struct InlineU64Pair {
    uint64_t left;
    uint64_t right;
} InlineU64Pair;

static void verify_inline_u64_arithmetic_two_variables_expression(
    const uint8_t* data, uint64_t expression_end, uint64_t expression_start,
    const ObjSection* info, const ObjSection* strings,
    const ObjSection* locations, const char* left_source_name,
    const char* right_source_name, int64_t left_frame_offset,
    bool is_unsigned, bool is_subtract) {
    static const InlineU64Pair unsigned_values[] = {
        {0u, 0u},
        {1u, 0u},
        {0u, 1u},
        {UINT64_C(0xffffffff), 1u},
        {UINT64_C(0xffffffff), UINT64_C(0xffffffff)},
        {UINT64_C(0x100000000), UINT64_C(0xffffffff)},
        {UINT64_C(0x7fffffffffffffff), UINT64_C(0x8000000000000001)},
        {UINT64_MAX, 1u}
    };
    static const InlineU64Pair signed_values[] = {
        {0u, 0u},
        {1u, 0u},
        {0u, 1u},
        {UINT64_C(0xffffffff), 1u},
        {UINT64_C(0xffffffff), UINT64_C(0xffffffff)},
        {UINT64_C(0x100000000), UINT64_C(0xffffffff)},
        {UINT64_C(0x7fffffffffffffff), 0u},
        {UINT64_C(0x8000000000000000), 0u},
        {UINT64_C(0x8000000000000001), 1u}
    };
    const InlineU64Pair* values = is_unsigned ? unsigned_values : signed_values;
    size_t value_count = is_unsigned
        ? sizeof(unsigned_values) / sizeof(unsigned_values[0])
        : sizeof(signed_values) / sizeof(signed_values[0]);
    const uint64_t sign_bit = UINT64_C(0x80000000);
    uint64_t offset = expression_start;
    int64_t right_frame_offset;
    bool has_variable_location;
    bool variable_is_location_list;
    int64_t variable_frame_offset;
    assert(left_source_name && right_source_name &&
           left_frame_offset <= INT64_MAX - 4);

    assert(find_variable_location(
        info, strings, locations, right_source_name, ARCH_X86,
        &has_variable_location, &variable_is_location_list,
        &right_frame_offset));
    assert(has_variable_location && right_frame_offset <= INT64_MAX - 4);

    verify_inline_expression_frame_u32(
        data, expression_end, &offset, left_frame_offset);
    verify_inline_expression_frame_u32(
        data, expression_end, &offset, right_frame_offset);
    verify_inline_expression_byte(
        data, expression_end, &offset, is_subtract ? 0x1cu : 0x22u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x9fu);
    verify_inline_expression_byte(data, expression_end, &offset, 0x93u);
    assert(read_uleb(data, expression_end, &offset) == 4u);

    verify_inline_expression_frame_u32(
        data, expression_end, &offset, left_frame_offset + 4);
    verify_inline_expression_frame_u32(
        data, expression_end, &offset, right_frame_offset + 4);
    verify_inline_expression_byte(
        data, expression_end, &offset, is_subtract ? 0x1cu : 0x22u);
    if (is_subtract) {
        verify_inline_expression_frame_u32(
            data, expression_end, &offset, left_frame_offset);
        verify_inline_expression_constu(data, expression_end, &offset,
                                       sign_bit);
        verify_inline_expression_byte(data, expression_end, &offset, 0x27u);
        verify_inline_expression_frame_u32(
            data, expression_end, &offset, right_frame_offset);
        verify_inline_expression_constu(data, expression_end, &offset,
                                       sign_bit);
        verify_inline_expression_byte(data, expression_end, &offset, 0x27u);
        verify_inline_expression_byte(data, expression_end, &offset, 0x2du);
        verify_inline_expression_byte(data, expression_end, &offset, 0x1cu);
    } else {
        verify_inline_expression_frame_u32(
            data, expression_end, &offset, left_frame_offset);
        verify_inline_expression_frame_u32(
            data, expression_end, &offset, right_frame_offset);
        verify_inline_expression_byte(data, expression_end, &offset, 0x22u);
        verify_inline_expression_frame_u32(
            data, expression_end, &offset, left_frame_offset);
        verify_inline_expression_constu(data, expression_end, &offset,
                                       sign_bit);
        verify_inline_expression_byte(data, expression_end, &offset, 0x27u);
        verify_inline_expression_byte(data, expression_end, &offset, 0x16u);
        verify_inline_expression_constu(data, expression_end, &offset,
                                       sign_bit);
        verify_inline_expression_byte(data, expression_end, &offset, 0x27u);
        verify_inline_expression_byte(data, expression_end, &offset, 0x16u);
        verify_inline_expression_byte(data, expression_end, &offset, 0x2du);
        verify_inline_expression_byte(data, expression_end, &offset, 0x22u);
    }
    verify_inline_expression_byte(data, expression_end, &offset, 0x9fu);
    verify_inline_expression_byte(data, expression_end, &offset, 0x93u);
    assert(read_uleb(data, expression_end, &offset) == 4u);
    assert(offset == expression_end);

    assert(find_variable_location(
        info, strings, locations, left_source_name, ARCH_X86,
        &has_variable_location, &variable_is_location_list,
        &variable_frame_offset));
    assert(has_variable_location &&
           variable_frame_offset == left_frame_offset);
    assert(find_variable_location(
        info, strings, locations, right_source_name, ARCH_X86,
        &has_variable_location, &variable_is_location_list,
        &variable_frame_offset));
    assert(has_variable_location &&
           variable_frame_offset == right_frame_offset);

    for (size_t index = 0u; index < value_count; ++index) {
        uint64_t left = values[index].left;
        uint64_t right = values[index].right;
        uint32_t left_low = (uint32_t)left;
        uint32_t right_low = (uint32_t)right;
        uint32_t low_result = is_subtract
            ? left_low - right_low : left_low + right_low;
        uint32_t high_result = is_subtract
            ? (uint32_t)(left >> 32u) - (uint32_t)(right >> 32u) -
                (uint32_t)(left_low < right_low)
            : (uint32_t)(left >> 32u) + (uint32_t)(right >> 32u) +
                (uint32_t)(((uint64_t)left_low + right_low) >> 32u);
        uint64_t actual = ((uint64_t)high_result << 32u) | low_result;
        assert(actual == (is_subtract ? left - right : left + right));
    }
}

static void verify_inline_u64_bitwise_two_variables_expression(
    const uint8_t* data, uint64_t expression_end, uint64_t expression_start,
    const ObjSection* info, const ObjSection* strings,
    const ObjSection* locations, const char* left_source_name,
    const char* right_source_name, int64_t left_frame_offset,
    uint8_t operation) {
    static const InlineU64Pair values[] = {
        {0u, 0u},
        {1u, 0u},
        {0u, 1u},
        {UINT64_C(0xffffffff), 1u},
        {UINT64_C(0xffffffff), UINT64_C(0xffffffff)},
        {UINT64_C(0x100000000), UINT64_C(0xffffffff)},
        {UINT64_C(0x7fffffffffffffff), UINT64_C(0x8000000000000001)},
        {UINT64_MAX, 1u}
    };
    uint64_t offset = expression_start;
    int64_t right_frame_offset;
    bool has_variable_location;
    bool variable_is_location_list;
    int64_t variable_frame_offset;
    assert(operation == 0x1au || operation == 0x21u || operation == 0x27u);
    assert(left_source_name && right_source_name &&
           left_frame_offset <= INT64_MAX - 4);
    assert(find_variable_location(
        info, strings, locations, right_source_name, ARCH_X86,
        &has_variable_location, &variable_is_location_list,
        &right_frame_offset));
    assert(has_variable_location && right_frame_offset <= INT64_MAX - 4);

    for (unsigned int word = 0u; word < 2u; ++word) {
        int64_t word_offset = (int64_t)(word * 4u);
        verify_inline_expression_frame_u32(
            data, expression_end, &offset, left_frame_offset + word_offset);
        verify_inline_expression_frame_u32(
            data, expression_end, &offset, right_frame_offset + word_offset);
        verify_inline_expression_byte(data, expression_end, &offset, operation);
        verify_inline_expression_byte(data, expression_end, &offset, 0x9fu);
        verify_inline_expression_byte(data, expression_end, &offset, 0x93u);
        assert(read_uleb(data, expression_end, &offset) == 4u);
    }
    assert(offset == expression_end);

    assert(find_variable_location(
        info, strings, locations, left_source_name, ARCH_X86,
        &has_variable_location, &variable_is_location_list,
        &variable_frame_offset));
    assert(has_variable_location &&
           variable_frame_offset == left_frame_offset);
    assert(find_variable_location(
        info, strings, locations, right_source_name, ARCH_X86,
        &has_variable_location, &variable_is_location_list,
        &variable_frame_offset));
    assert(has_variable_location &&
           variable_frame_offset == right_frame_offset);

    for (size_t index = 0u; index < sizeof(values) / sizeof(values[0]);
         ++index) {
        uint64_t left = values[index].left;
        uint64_t right = values[index].right;
        uint32_t left_low = (uint32_t)left;
        uint32_t right_low = (uint32_t)right;
        uint32_t left_high = (uint32_t)(left >> 32u);
        uint32_t right_high = (uint32_t)(right >> 32u);
        uint32_t actual_low = operation == 0x1au
            ? left_low & right_low
            : operation == 0x21u ? left_low | right_low
                                 : left_low ^ right_low;
        uint32_t actual_high = operation == 0x1au
            ? left_high & right_high
            : operation == 0x21u ? left_high | right_high
                                 : left_high ^ right_high;
        uint64_t actual = ((uint64_t)actual_high << 32u) | actual_low;
        uint64_t expected = operation == 0x1au ? left & right
            : operation == 0x21u ? left | right : left ^ right;
        assert(actual == expected);
    }
}

static uint64_t evaluate_inline_u64_mul_two_variables_expression(
    const uint8_t* data, uint64_t expression_end, uint64_t expression_start,
    int64_t left_frame_offset, int64_t right_frame_offset,
    uint64_t left_value, uint64_t right_value) {
    uint32_t stack[16];
    size_t stack_size = 0u;
    uint64_t result = 0u;
    uint64_t offset = expression_start;
    int64_t pending_frame_offset = 0;
    unsigned int piece_count = 0u;
    bool has_pending_frame_offset = false;
    while (offset < expression_end) {
        uint8_t opcode = data[offset++];
        if (opcode == 0x75u) { /* DW_OP_breg5 */
            assert(!has_pending_frame_offset);
            pending_frame_offset = read_sleb(data, expression_end, &offset);
            has_pending_frame_offset = true;
        } else if (opcode == 0x94u) { /* DW_OP_deref_size */
            uint32_t value;
            assert(has_pending_frame_offset && offset < expression_end &&
                   data[offset++] == 4u &&
                   stack_size < sizeof(stack) / sizeof(stack[0]));
            if (pending_frame_offset == left_frame_offset) {
                value = (uint32_t)left_value;
            } else if (pending_frame_offset == left_frame_offset + 4) {
                value = (uint32_t)(left_value >> 32u);
            } else if (pending_frame_offset == right_frame_offset) {
                value = (uint32_t)right_value;
            } else {
                assert(pending_frame_offset == right_frame_offset + 4);
                value = (uint32_t)(right_value >> 32u);
            }
            stack[stack_size++] = value;
            has_pending_frame_offset = false;
        } else if (opcode == 0x10u) { /* DW_OP_constu */
            assert(stack_size < sizeof(stack) / sizeof(stack[0]));
            stack[stack_size++] = (uint32_t)read_uleb(
                data, expression_end, &offset);
        } else if (opcode == 0x1au || opcode == 0x1eu ||
                   opcode == 0x22u || opcode == 0x25u) {
            uint32_t left;
            uint32_t right;
            assert(stack_size >= 2u);
            right = stack[--stack_size];
            left = stack[--stack_size];
            if (opcode == 0x1au) {
                stack[stack_size++] = left & right;
            } else if (opcode == 0x1eu) {
                stack[stack_size++] = left * right;
            } else if (opcode == 0x22u) {
                stack[stack_size++] = left + right;
            } else {
                assert(right < 32u);
                stack[stack_size++] = left >> right;
            }
        } else if (opcode == 0x9fu) { /* DW_OP_stack_value */
            assert(stack_size == 1u);
        } else if (opcode == 0x93u) { /* DW_OP_piece */
            uint64_t piece_size = read_uleb(data, expression_end, &offset);
            assert(piece_size == 4u && stack_size == 1u && piece_count < 2u);
            result |= (uint64_t)stack[--stack_size] << (piece_count * 32u);
            ++piece_count;
        } else {
            assert(!"unexpected opcode in two-variable wide multiply");
        }
    }
    assert(!has_pending_frame_offset && stack_size == 0u && piece_count == 2u);
    return result;
}

static void verify_inline_u64_mul_two_variables_expression(
    const uint8_t* data, uint64_t expression_end, uint64_t expression_start,
    const ObjSection* info, const ObjSection* strings,
    const ObjSection* locations, const char* left_source_name,
    const char* right_source_name, int64_t left_frame_offset,
    bool is_unsigned) {
    static const InlineU64Pair unsigned_values[] = {
        {0u, 0u},
        {1u, UINT64_MAX},
        {UINT64_C(0xffffffff), 1u},
        {UINT64_C(0x100000000), UINT64_C(0xffffffff)},
        {UINT64_C(0x123456789abcdef0), UINT64_C(0xfedcba9876543210)},
        {UINT64_C(0xffff0000ffff0001), UINT64_C(0x0000ffff0000ffff)},
        {UINT64_MAX, UINT64_MAX},
        {UINT64_MAX, 2u}
    };
    static const InlineU64Pair signed_values[] = {
        {0u, 0u},
        {1u, UINT64_MAX},
        {UINT64_MAX, 1u},
        {2u, 3u},
        {UINT64_MAX, 2u},
        {UINT64_C(0x8000000000000000), 1u},
        {UINT64_C(0x7fffffffffffffff), 1u},
        {UINT64_C(0x8000000000000000), 0u}
    };
    const InlineU64Pair* values = is_unsigned ? unsigned_values : signed_values;
    size_t value_count = is_unsigned
        ? sizeof(unsigned_values) / sizeof(unsigned_values[0])
        : sizeof(signed_values) / sizeof(signed_values[0]);
    bool has_variable_location;
    bool variable_is_location_list;
    int64_t left_location_offset;
    int64_t right_frame_offset;
    int64_t right_location_offset;
    assert(left_source_name && right_source_name &&
           left_frame_offset <= INT64_MAX - 4);
    assert(find_variable_location(
        info, strings, locations, left_source_name, ARCH_X86,
        &has_variable_location, &variable_is_location_list,
        &left_location_offset));
    assert(has_variable_location &&
           left_location_offset == left_frame_offset);
    assert(find_variable_location(
        info, strings, locations, right_source_name, ARCH_X86,
        &has_variable_location, &variable_is_location_list,
        &right_location_offset));
    assert(has_variable_location && right_location_offset <= INT64_MAX - 4);
    right_frame_offset = right_location_offset;

    for (size_t index = 0u; index < value_count; ++index) {
        uint64_t actual =
            evaluate_inline_u64_mul_two_variables_expression(
                data, expression_end, expression_start, left_frame_offset,
                right_frame_offset, values[index].left, values[index].right);
        assert(actual == values[index].left * values[index].right);
    }
}

static void verify_inline_u64_bitwise_constant_expression(
    const uint8_t* data, uint64_t expression_end, uint64_t expression_start,
    const ObjSection* info, const ObjSection* strings,
    const ObjSection* locations, const char* source_name,
    int64_t source_frame_offset, uint64_t constant, uint8_t operation,
    bool is_unsigned) {
    static const uint64_t unsigned_values[] = {
        0u, 1u, UINT64_C(0xffffffff), UINT64_C(0x100000000),
        UINT64_C(0x1fffffffe), UINT64_C(0x7fffffffffffffff),
        UINT64_C(0xfffffffffffffffe), UINT64_MAX
    };
    static const uint64_t signed_values[] = {
        0u, 1u, UINT64_MAX, UINT64_C(0x7fffffffffffffff),
        UINT64_C(0x8000000000000007), UINT64_C(0x8000000000000008),
        UINT64_C(0xfffffffffffffff9), UINT64_C(0x8000000012345678)
    };
    const uint64_t* values = is_unsigned ? unsigned_values : signed_values;
    size_t value_count = is_unsigned
        ? sizeof(unsigned_values) / sizeof(unsigned_values[0])
        : sizeof(signed_values) / sizeof(signed_values[0]);
    uint64_t offset = expression_start;
    bool has_variable_location;
    bool variable_is_location_list;
    int64_t variable_frame_offset;

    assert(operation == 0x1au || operation == 0x21u || operation == 0x27u);
    for (unsigned int word = 0u; word < 2u; ++word) {
        uint32_t word_constant = word == 0u
            ? (uint32_t)constant : (uint32_t)(constant >> 32u);
        verify_inline_expression_frame_u32(
            data, expression_end, &offset,
            source_frame_offset + (int64_t)(word * 4u));
        verify_inline_expression_byte(data, expression_end, &offset, 0x10u);
        assert(read_uleb(data, expression_end, &offset) == word_constant);
        verify_inline_expression_byte(data, expression_end, &offset,
                                      operation);
        verify_inline_expression_byte(data, expression_end, &offset, 0x9fu);
        verify_inline_expression_byte(data, expression_end, &offset, 0x93u);
        assert(read_uleb(data, expression_end, &offset) == 4u);
    }
    assert(offset == expression_end);

    assert(find_variable_location(
        info, strings, locations, source_name, ARCH_X86,
        &has_variable_location, &variable_is_location_list,
        &variable_frame_offset));
    assert(has_variable_location &&
           variable_frame_offset == source_frame_offset);
    for (size_t index = 0u; index < value_count; ++index) {
        uint64_t expected = operation == 0x1au ? values[index] & constant :
            operation == 0x21u ? values[index] | constant :
            values[index] ^ constant;
        uint32_t low_result = operation == 0x1au
            ? (uint32_t)values[index] & (uint32_t)constant :
            operation == 0x21u
                ? (uint32_t)values[index] | (uint32_t)constant :
                (uint32_t)values[index] ^ (uint32_t)constant;
        uint32_t high_result = operation == 0x1au
            ? (uint32_t)(values[index] >> 32u) & (uint32_t)(constant >> 32u) :
            operation == 0x21u
                ? (uint32_t)(values[index] >> 32u) |
                    (uint32_t)(constant >> 32u) :
                (uint32_t)(values[index] >> 32u) ^
                    (uint32_t)(constant >> 32u);
        uint64_t actual = ((uint64_t)high_result << 32u) | low_result;
        assert(actual == expected);
    }
}

static void verify_inline_u64_shift_constant_expression(
    const uint8_t* data, uint64_t expression_end, uint64_t expression_start,
    const ObjSection* info, const ObjSection* strings,
    const ObjSection* locations, const char* source_name,
    int64_t source_frame_offset, bool is_unsigned, uint64_t shift_count,
    bool is_left_shift) {
    static const uint64_t unsigned_values[] = {
        0u, 1u, UINT64_C(0xffffffff), UINT64_C(0x100000000),
        UINT64_C(0x1fffffffe), UINT64_C(0x7fffffffffffffff),
        UINT64_C(0xfffffffffffffffe), UINT64_MAX
    };
    static const uint64_t signed_left_values[] = {
        0u, 1u, UINT64_C(0xffffffff), UINT64_C(0x100000000),
        UINT64_C(0x1fffffffe), UINT64_C(0x123456789abcdef),
        UINT64_C(0x2000000000000000), UINT64_C(0x3fffffffffffffff)
    };
    static const uint64_t signed_right_values[] = {
        0u, 1u, UINT64_MAX, UINT64_C(0x7fffffffffffffff),
        UINT64_C(0x8000000000000000), UINT64_C(0x8000000000000007),
        UINT64_C(0xfffffffffffffff9), UINT64_C(0xffffffff00000000)
    };
    const uint64_t* values = is_unsigned ? unsigned_values
        : is_left_shift ? signed_left_values : signed_right_values;
    size_t value_count = is_unsigned
        ? sizeof(unsigned_values) / sizeof(unsigned_values[0])
        : is_left_shift
            ? sizeof(signed_left_values) / sizeof(signed_left_values[0])
            : sizeof(signed_right_values) / sizeof(signed_right_values[0]);
    uint64_t offset = expression_start;
    bool has_variable_location;
    bool variable_is_location_list;
    int64_t variable_frame_offset;

    assert(source_frame_offset <= INT64_MAX - 4);
    assert(shift_count == 1u || (!is_left_shift && shift_count == 33u));
    if (is_left_shift) {
        verify_inline_expression_frame_u32(
            data, expression_end, &offset, source_frame_offset);
        verify_inline_expression_constu(data, expression_end, &offset, 1u);
        verify_inline_expression_byte(data, expression_end, &offset, 0x24u);
        verify_inline_expression_byte(data, expression_end, &offset, 0x9fu);
        verify_inline_expression_byte(data, expression_end, &offset, 0x93u);
        assert(read_uleb(data, expression_end, &offset) == 4u);
        verify_inline_expression_frame_u32(
            data, expression_end, &offset, source_frame_offset + 4);
        verify_inline_expression_constu(data, expression_end, &offset, 1u);
        verify_inline_expression_byte(data, expression_end, &offset, 0x24u);
        verify_inline_expression_frame_u32(
            data, expression_end, &offset, source_frame_offset);
        verify_inline_expression_constu(data, expression_end, &offset, 31u);
        verify_inline_expression_byte(data, expression_end, &offset, 0x25u);
        verify_inline_expression_byte(data, expression_end, &offset, 0x21u);
    } else if (shift_count == 1u) {
        verify_inline_expression_frame_u32(
            data, expression_end, &offset, source_frame_offset);
        verify_inline_expression_constu(data, expression_end, &offset, 1u);
        verify_inline_expression_byte(data, expression_end, &offset, 0x25u);
        verify_inline_expression_frame_u32(
            data, expression_end, &offset, source_frame_offset + 4);
        verify_inline_expression_constu(data, expression_end, &offset, 31u);
        verify_inline_expression_byte(data, expression_end, &offset, 0x24u);
        verify_inline_expression_byte(data, expression_end, &offset, 0x21u);
        verify_inline_expression_byte(data, expression_end, &offset, 0x9fu);
        verify_inline_expression_byte(data, expression_end, &offset, 0x93u);
        assert(read_uleb(data, expression_end, &offset) == 4u);
        verify_inline_expression_frame_u32(
            data, expression_end, &offset, source_frame_offset + 4);
        verify_inline_expression_constu(data, expression_end, &offset, 1u);
        verify_inline_expression_byte(
            data, expression_end, &offset, is_unsigned ? 0x25u : 0x26u);
    } else {
        verify_inline_expression_frame_u32(
            data, expression_end, &offset, source_frame_offset + 4);
        verify_inline_expression_constu(data, expression_end, &offset, 1u);
        verify_inline_expression_byte(
            data, expression_end, &offset, is_unsigned ? 0x25u : 0x26u);
        verify_inline_expression_byte(data, expression_end, &offset, 0x9fu);
        verify_inline_expression_byte(data, expression_end, &offset, 0x93u);
        assert(read_uleb(data, expression_end, &offset) == 4u);
        if (is_unsigned) {
            verify_inline_expression_constu(data, expression_end, &offset, 0u);
        } else {
            verify_inline_expression_frame_u32(
                data, expression_end, &offset, source_frame_offset + 4);
            verify_inline_expression_constu(data, expression_end, &offset, 31u);
            verify_inline_expression_byte(data, expression_end, &offset, 0x26u);
        }
    }
    verify_inline_expression_byte(data, expression_end, &offset, 0x9fu);
    verify_inline_expression_byte(data, expression_end, &offset, 0x93u);
    assert(read_uleb(data, expression_end, &offset) == 4u);
    assert(offset == expression_end);

    assert(find_variable_location(
        info, strings, locations, source_name, ARCH_X86,
        &has_variable_location, &variable_is_location_list,
        &variable_frame_offset));
    assert(has_variable_location &&
           variable_frame_offset == source_frame_offset);
    for (size_t index = 0u; index < value_count; ++index) {
        uint32_t low = (uint32_t)values[index];
        uint32_t high = (uint32_t)(values[index] >> 32u);
        uint32_t low_result;
        uint32_t high_result;
        if (is_left_shift) {
            low_result = low << 1u;
            high_result = (high << 1u) | (low >> 31u);
        } else if (shift_count == 1u) {
            low_result = (low >> 1u) | (high << 31u);
            high_result = (high >> 1u) |
                (!is_unsigned ? high & UINT32_C(0x80000000) : 0u);
        } else {
            low_result = (high >> 1u) |
                (!is_unsigned ? high & UINT32_C(0x80000000) : 0u);
            high_result = !is_unsigned &&
                    (high & UINT32_C(0x80000000)) != 0u
                ? UINT32_MAX : 0u;
        }
        uint64_t actual = ((uint64_t)high_result << 32u) | low_result;
        uint64_t expected;
        if (is_left_shift) {
            expected = values[index] << shift_count;
        } else if (is_unsigned ||
                   (values[index] & UINT64_C(0x8000000000000000)) == 0u) {
            expected = values[index] >> shift_count;
        } else {
            expected = (values[index] >> shift_count) |
                (UINT64_MAX << (64u - shift_count));
        }
        assert(actual == expected);
    }
}

static uint64_t evaluate_inline_u64_variable_shift_expression(
    const uint8_t* data, uint64_t expression_end, uint64_t expression_start,
    int64_t source_frame_offset, int64_t shift_count_frame_offset,
    uint64_t source_value, uint64_t shift_count) {
    uint32_t stack[16];
    size_t stack_size = 0u;
    uint64_t offset = expression_start;
    int64_t pending_frame_offset = 0;
    bool has_pending_frame_offset = false;
    uint64_t result = 0u;
    unsigned int piece_count = 0u;
    while (offset < expression_end) {
        uint8_t opcode = data[offset++];
        if (opcode == 0x75u) { /* DW_OP_breg5 */
            assert(!has_pending_frame_offset);
            pending_frame_offset = read_sleb(data, expression_end, &offset);
            has_pending_frame_offset = true;
        } else if (opcode == 0x94u) { /* DW_OP_deref_size */
            uint32_t value;
            assert(offset < expression_end && data[offset++] == 4u &&
                   has_pending_frame_offset &&
                   stack_size < sizeof(stack) / sizeof(stack[0]));
            if (pending_frame_offset == shift_count_frame_offset) {
                value = (uint32_t)shift_count;
            } else if (pending_frame_offset == source_frame_offset) {
                value = (uint32_t)source_value;
            } else {
                assert(source_frame_offset <= INT64_MAX - 4 &&
                       pending_frame_offset == source_frame_offset + 4);
                value = (uint32_t)(source_value >> 32u);
            }
            stack[stack_size++] = value;
            has_pending_frame_offset = false;
        } else if (opcode == 0x10u) { /* DW_OP_constu */
            assert(stack_size < sizeof(stack) / sizeof(stack[0]));
            stack[stack_size++] = (uint32_t)read_uleb(
                data, expression_end, &offset);
        } else if (opcode == 0x1au || opcode == 0x1cu || opcode == 0x21u ||
                   opcode == 0x22u ||
                   opcode == 0x24u || opcode == 0x25u ||
                   opcode == 0x26u || opcode == 0x29u ||
                   opcode == 0x2au) {
            uint32_t lhs;
            uint32_t rhs;
            assert(stack_size >= 2u);
            rhs = stack[--stack_size];
            lhs = stack[--stack_size];
            if (opcode == 0x1cu) {
                stack[stack_size++] = lhs - rhs;
            } else if (opcode == 0x1au) {
                stack[stack_size++] = lhs & rhs;
            } else if (opcode == 0x21u) {
                stack[stack_size++] = lhs | rhs;
            } else if (opcode == 0x22u) {
                stack[stack_size++] = lhs + rhs;
            } else if (opcode == 0x24u) {
                assert(rhs < 32u);
                stack[stack_size++] = lhs << rhs;
            } else if (opcode == 0x25u) {
                assert(rhs < 32u);
                stack[stack_size++] = lhs >> rhs;
            } else if (opcode == 0x26u) {
                assert(rhs < 32u);
                stack[stack_size++] = (lhs >> rhs) |
                    ((lhs & UINT32_C(0x80000000)) == 0u || rhs == 0u
                         ? 0u : UINT32_MAX << (32u - rhs));
            } else if (opcode == 0x29u) {
                stack[stack_size++] = lhs == rhs;
            } else {
                stack[stack_size++] = lhs >= rhs;
            }
        } else if (opcode == 0x28u || opcode == 0x2fu) {
            uint16_t encoded;
            int32_t displacement;
            bool take_branch = true;
            assert(offset + 2u <= expression_end);
            encoded = (uint16_t)data[offset] |
                (uint16_t)((uint16_t)data[offset + 1u] << 8u);
            offset += 2u;
            displacement = encoded < 0x8000u
                ? (int32_t)encoded : (int32_t)encoded - 0x10000;
            if (opcode == 0x28u) {
                assert(stack_size != 0u);
                take_branch = stack[--stack_size] != 0u;
            }
            if (take_branch) {
                int64_t target = (int64_t)offset + displacement;
                assert(target >= 0 && (uint64_t)target <= expression_end);
                offset = (uint64_t)target;
            }
        } else if (opcode == 0x9fu) { /* DW_OP_stack_value */
            assert(stack_size == 1u);
        } else if (opcode == 0x93u) { /* DW_OP_piece */
            uint64_t piece_size = read_uleb(data, expression_end, &offset);
            assert(piece_size == 4u && stack_size == 1u && piece_count < 2u);
            result |= (uint64_t)stack[--stack_size] << (piece_count * 32u);
            ++piece_count;
        } else {
            assert(!"unexpected opcode in variable wide shift");
        }
    }
    assert(!has_pending_frame_offset && stack_size == 0u && piece_count == 2u);
    return result;
}

static void verify_inline_u64_variable_shift_expression(
    const uint8_t* data, uint64_t expression_end, uint64_t expression_start,
    const ObjSection* info, const ObjSection* strings,
    const ObjSection* locations, const char* source_name,
    const char* shift_count_source_name, bool is_left_shift,
    bool is_unsigned, uint32_t shift_count_adjustment,
    bool wide_shift_count, bool modulo_64_count) {
    static const uint64_t unsigned_values[] = {
        0u, 1u, UINT64_C(0xffffffff), UINT64_C(0x100000000),
        UINT64_C(0x1fffffffe), UINT64_C(0x7fffffffffffffff),
        UINT64_C(0xfffffffffffffffe), UINT64_MAX
    };
    static const uint64_t signed_values[] = {
        0u, 1u, UINT64_MAX, UINT64_C(0x7fffffffffffffff),
        UINT64_C(0x8000000000000000), UINT64_C(0x8000000000000007),
        UINT64_C(0xfffffffffffffff9), UINT64_C(0xffffffff00000000)
    };
    const uint64_t* values = is_unsigned ? unsigned_values : signed_values;
    size_t value_count = is_unsigned
        ? sizeof(unsigned_values) / sizeof(unsigned_values[0])
        : sizeof(signed_values) / sizeof(signed_values[0]);
    static const uint32_t shift_counts[] = {0u, 1u, 31u, 32u, 33u, 63u};
    static const uint32_t plus_one_shift_counts[] = {
        UINT32_MAX, 0u, 30u, 31u, 32u, 62u
    };
    static const uint64_t plus_one_wide_shift_counts[] = {
        UINT64_MAX, 0u, 30u, 31u, 32u, 62u
    };
    bool has_location;
    bool is_location_list;
    int64_t source_frame_offset;
    int64_t shift_count_frame_offset;
    assert(find_variable_location(
        info, strings, locations, source_name, ARCH_X86, &has_location,
        &is_location_list, &source_frame_offset));
    assert(has_location);
    assert(find_variable_location(
        info, strings, locations, shift_count_source_name, ARCH_X86,
        &has_location, &is_location_list, &shift_count_frame_offset));
    assert(has_location);
    if (modulo_64_count) {
        uint64_t offset = expression_start;
        verify_inline_expression_byte(data, expression_end, &offset, 0x75u);
        assert(read_sleb(data, expression_end, &offset) ==
               shift_count_frame_offset);
        verify_inline_expression_byte(data, expression_end, &offset, 0x94u);
        verify_inline_expression_byte(data, expression_end, &offset, 4u);
        if (shift_count_adjustment != 0u) {
            verify_inline_expression_constu(
                data, expression_end, &offset, shift_count_adjustment);
            verify_inline_expression_byte(data, expression_end, &offset,
                                          0x22u);
        }
        verify_inline_expression_byte(data, expression_end, &offset, 0x10u);
        assert(read_uleb(data, expression_end, &offset) == 63u);
        verify_inline_expression_byte(data, expression_end, &offset, 0x1au);
    }
    for (size_t count_index = 0u;
         count_index < sizeof(shift_counts) / sizeof(shift_counts[0]);
         ++count_index) {
        uint64_t input_shift_count = modulo_64_count
            ? (UINT64_C(0x12345678) << 32u) | shift_counts[count_index]
            : shift_count_adjustment == 0u
                ? shift_counts[count_index]
                : wide_shift_count
                    ? plus_one_wide_shift_counts[count_index]
                    : plus_one_shift_counts[count_index];
        uint64_t shift_count = modulo_64_count
            ? (input_shift_count + shift_count_adjustment) % 64u
            : input_shift_count + shift_count_adjustment;
        if (!wide_shift_count) shift_count = (uint32_t)shift_count;
        if (shift_count >= 64u) continue;
        for (size_t value_index = 0u; value_index < value_count;
             ++value_index) {
            uint64_t value = values[value_index];
            if (!is_unsigned && is_left_shift &&
                ((value & UINT64_C(0x8000000000000000)) != 0u ||
                 value > ((uint64_t)INT64_MAX >> shift_count))) {
                continue;
            }
            uint64_t actual =
                evaluate_inline_u64_variable_shift_expression(
                    data, expression_end, expression_start,
                    source_frame_offset, shift_count_frame_offset,
                    value, input_shift_count);
            uint64_t expected;
            if (is_left_shift) {
                expected = value << shift_count;
            } else if (is_unsigned || shift_count == 0u ||
                       (value & UINT64_C(0x8000000000000000)) == 0u) {
                expected = value >> shift_count;
            } else {
                expected = (value >> shift_count) |
                    (UINT64_MAX << (64u - shift_count));
            }
            if (actual != expected) {
                fprintf(stderr,
                        "inline wide shift mismatch: source=%s count=%s "
                        "mod64=%d input=%llu shift=%llu value=%llu "
                        "actual=%llu expected=%llu\n",
                        source_name, shift_count_source_name,
                        modulo_64_count ? 1 : 0,
                        (unsigned long long)input_shift_count,
                        (unsigned long long)shift_count,
                        (unsigned long long)value,
                        (unsigned long long)actual,
                        (unsigned long long)expected);
            }
            assert(actual == expected);
        }
    }
}

static void verify_inline_u64_sub_constant_expression(
    const uint8_t* data, uint64_t expression_end, uint64_t expression_start,
    const ObjSection* info, const ObjSection* strings,
    const ObjSection* locations, const char* source_name,
    int64_t source_frame_offset, uint64_t constant, bool is_unsigned) {
    static const uint64_t unsigned_values[] = {
        0u, 1u, UINT64_C(0xffffffff), UINT64_C(0x100000000),
        UINT64_C(0x1fffffffe), UINT64_C(0x7fffffffffffffff),
        UINT64_C(0xfffffffffffffffe), UINT64_MAX
    };
    static const uint64_t signed_values[] = {
        0u, 1u, UINT64_MAX, UINT64_C(0x7fffffffffffffff),
        UINT64_C(0x8000000000000007), UINT64_C(0x8000000000000008),
        UINT64_C(0xfffffffffffffff9), UINT64_C(0x8000000012345678)
    };
    const uint64_t* values = is_unsigned ? unsigned_values : signed_values;
    size_t value_count = is_unsigned
        ? sizeof(unsigned_values) / sizeof(unsigned_values[0])
        : sizeof(signed_values) / sizeof(signed_values[0]);
    uint64_t offset = expression_start;
    uint32_t low_constant = (uint32_t)constant;
    uint32_t high_constant = (uint32_t)(constant >> 32u);
    const uint64_t sign_bit = UINT64_C(0x80000000);
    bool has_variable_location;
    bool variable_is_location_list;
    int64_t variable_frame_offset;

    assert(source_frame_offset <= INT64_MAX - 4);
    verify_inline_expression_frame_u32(
        data, expression_end, &offset, source_frame_offset);
    verify_inline_expression_byte(data, expression_end, &offset, 0x10u);
    assert(read_uleb(data, expression_end, &offset) == low_constant);
    verify_inline_expression_byte(data, expression_end, &offset, 0x1cu);
    verify_inline_expression_byte(data, expression_end, &offset, 0x9fu);
    verify_inline_expression_byte(data, expression_end, &offset, 0x93u);
    assert(read_uleb(data, expression_end, &offset) == 4u);

    verify_inline_expression_frame_u32(
        data, expression_end, &offset, source_frame_offset + 4);
    verify_inline_expression_byte(data, expression_end, &offset, 0x10u);
    assert(read_uleb(data, expression_end, &offset) == high_constant);
    verify_inline_expression_byte(data, expression_end, &offset, 0x1cu);
    verify_inline_expression_frame_u32(
        data, expression_end, &offset, source_frame_offset);
    verify_inline_expression_byte(data, expression_end, &offset, 0x10u);
    assert(read_uleb(data, expression_end, &offset) == sign_bit);
    verify_inline_expression_byte(data, expression_end, &offset, 0x27u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x10u);
    assert(read_uleb(data, expression_end, &offset) == low_constant);
    verify_inline_expression_byte(data, expression_end, &offset, 0x10u);
    assert(read_uleb(data, expression_end, &offset) == sign_bit);
    verify_inline_expression_byte(data, expression_end, &offset, 0x27u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x2du);
    verify_inline_expression_byte(data, expression_end, &offset, 0x1cu);
    verify_inline_expression_byte(data, expression_end, &offset, 0x9fu);
    verify_inline_expression_byte(data, expression_end, &offset, 0x93u);
    assert(read_uleb(data, expression_end, &offset) == 4u);
    assert(offset == expression_end);

    assert(find_variable_location(
        info, strings, locations, source_name, ARCH_X86,
        &has_variable_location, &variable_is_location_list,
        &variable_frame_offset));
    assert(has_variable_location &&
           variable_frame_offset == source_frame_offset);
    for (size_t index = 0u; index < value_count; ++index) {
        uint32_t low_result = (uint32_t)values[index] - low_constant;
        uint32_t borrow = (uint32_t)values[index] < low_constant;
        uint32_t high_result =
            (uint32_t)(values[index] >> 32u) - high_constant - borrow;
        uint64_t actual = ((uint64_t)high_result << 32u) | low_result;
        assert(actual == values[index] - constant);
    }
}

static void verify_inline_u64_mul_constant_expression(
    const uint8_t* data, uint64_t expression_end, uint64_t expression_start,
    const ObjSection* info, const ObjSection* strings,
    const ObjSection* locations, const char* source_name,
    int64_t source_frame_offset, uint64_t constant, bool is_unsigned) {
    static const uint64_t unsigned_values[] = {
        0u, 1u, UINT64_C(0xffffffff), UINT64_C(0x100000000),
        UINT64_C(0x1fffffffe), UINT64_C(0x7fffffffffffffff),
        UINT64_C(0xfffffffffffffffe), UINT64_MAX
    };
    static const uint64_t signed_values[] = {
        0u, 1u, UINT64_MAX, (uint64_t)(INT64_MAX / 3),
        (uint64_t)(INT64_MIN / 3), UINT64_C(0x100000000),
        (uint64_t)(-INT64_C(0x100000000)), UINT64_C(0xfffffffffffffff9)
    };
    const uint64_t* values = is_unsigned ? unsigned_values : signed_values;
    size_t value_count = is_unsigned
        ? sizeof(unsigned_values) / sizeof(unsigned_values[0])
        : sizeof(signed_values) / sizeof(signed_values[0]);
    const uint32_t mask16 = UINT32_C(0xffff);
    const uint32_t low_constant = (uint32_t)constant;
    const uint32_t high_constant = (uint32_t)(constant >> 32u);
    uint64_t offset = expression_start;
    bool has_variable_location;
    bool variable_is_location_list;
    int64_t variable_frame_offset;

    assert(source_frame_offset <= INT64_MAX - 4);
    verify_inline_expression_frame_u32(
        data, expression_end, &offset, source_frame_offset);
    verify_inline_expression_constu(data, expression_end, &offset,
                                    low_constant);
    verify_inline_expression_byte(data, expression_end, &offset, 0x1eu);
    verify_inline_expression_byte(data, expression_end, &offset, 0x9fu);
    verify_inline_expression_byte(data, expression_end, &offset, 0x93u);
    assert(read_uleb(data, expression_end, &offset) == 4u);

    verify_inline_expression_frame_u32(
        data, expression_end, &offset, source_frame_offset + 4);
    verify_inline_expression_constu(data, expression_end, &offset,
                                    low_constant);
    verify_inline_expression_byte(data, expression_end, &offset, 0x1eu);
    verify_inline_expression_frame_u32(
        data, expression_end, &offset, source_frame_offset);
    verify_inline_expression_constu(data, expression_end, &offset,
                                    high_constant);
    verify_inline_expression_byte(data, expression_end, &offset, 0x1eu);
    verify_inline_expression_byte(data, expression_end, &offset, 0x22u);

    /* high32(low * low_constant), expressed with 16-bit partial products. */
    verify_inline_expression_frame_u32(
        data, expression_end, &offset, source_frame_offset);
    verify_inline_expression_constu(data, expression_end, &offset, mask16);
    verify_inline_expression_byte(data, expression_end, &offset, 0x1au);
    verify_inline_expression_constu(data, expression_end, &offset,
                                    low_constant & mask16);
    verify_inline_expression_byte(data, expression_end, &offset, 0x1eu);
    verify_inline_expression_constu(data, expression_end, &offset, 16u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x25u);

    verify_inline_expression_frame_u32(
        data, expression_end, &offset, source_frame_offset);
    verify_inline_expression_constu(data, expression_end, &offset, mask16);
    verify_inline_expression_byte(data, expression_end, &offset, 0x1au);
    verify_inline_expression_constu(data, expression_end, &offset,
                                    low_constant >> 16u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x1eu);
    verify_inline_expression_constu(data, expression_end, &offset, mask16);
    verify_inline_expression_byte(data, expression_end, &offset, 0x1au);
    verify_inline_expression_byte(data, expression_end, &offset, 0x22u);

    verify_inline_expression_frame_u32(
        data, expression_end, &offset, source_frame_offset);
    verify_inline_expression_constu(data, expression_end, &offset, 16u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x25u);
    verify_inline_expression_constu(data, expression_end, &offset,
                                    low_constant & mask16);
    verify_inline_expression_byte(data, expression_end, &offset, 0x1eu);
    verify_inline_expression_constu(data, expression_end, &offset, mask16);
    verify_inline_expression_byte(data, expression_end, &offset, 0x1au);
    verify_inline_expression_byte(data, expression_end, &offset, 0x22u);
    verify_inline_expression_constu(data, expression_end, &offset, 16u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x25u);

    verify_inline_expression_frame_u32(
        data, expression_end, &offset, source_frame_offset);
    verify_inline_expression_constu(data, expression_end, &offset, mask16);
    verify_inline_expression_byte(data, expression_end, &offset, 0x1au);
    verify_inline_expression_constu(data, expression_end, &offset,
                                    low_constant >> 16u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x1eu);
    verify_inline_expression_constu(data, expression_end, &offset, 16u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x25u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x22u);

    verify_inline_expression_frame_u32(
        data, expression_end, &offset, source_frame_offset);
    verify_inline_expression_constu(data, expression_end, &offset, 16u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x25u);
    verify_inline_expression_constu(data, expression_end, &offset,
                                    low_constant & mask16);
    verify_inline_expression_byte(data, expression_end, &offset, 0x1eu);
    verify_inline_expression_constu(data, expression_end, &offset, 16u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x25u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x22u);

    verify_inline_expression_frame_u32(
        data, expression_end, &offset, source_frame_offset);
    verify_inline_expression_constu(data, expression_end, &offset, 16u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x25u);
    verify_inline_expression_constu(data, expression_end, &offset,
                                    low_constant >> 16u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x1eu);
    verify_inline_expression_byte(data, expression_end, &offset, 0x22u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x22u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x9fu);
    verify_inline_expression_byte(data, expression_end, &offset, 0x93u);
    assert(read_uleb(data, expression_end, &offset) == 4u);
    assert(offset == expression_end);

    assert(find_variable_location(
        info, strings, locations, source_name, ARCH_X86,
        &has_variable_location, &variable_is_location_list,
        &variable_frame_offset));
    assert(has_variable_location &&
           variable_frame_offset == source_frame_offset);
    for (size_t index = 0u; index < value_count; ++index) {
        uint32_t low = (uint32_t)values[index];
        uint32_t high = (uint32_t)(values[index] >> 32u);
        uint32_t multiplier_low = low_constant;
        uint32_t a0 = low & mask16;
        uint32_t a1 = low >> 16u;
        uint32_t m0 = multiplier_low & mask16;
        uint32_t m1 = multiplier_low >> 16u;
        uint32_t middle = (uint32_t)(((uint64_t)a0 * m0) >> 16u) +
            (uint32_t)((a0 * m1) & mask16) +
            (uint32_t)((a1 * m0) & mask16);
        uint32_t low_product_high = a1 * m1 +
            (uint32_t)((a0 * m1) >> 16u) +
            (uint32_t)((a1 * m0) >> 16u) + (middle >> 16u);
        uint32_t expected_low_product_high =
            (uint32_t)(((uint64_t)low * multiplier_low) >> 32u);
        uint32_t low_result = low * multiplier_low;
        uint32_t high_result = high * multiplier_low +
            low * high_constant + low_product_high;
        uint64_t actual = ((uint64_t)high_result << 32u) | low_result;
        assert(low_product_high == expected_low_product_high);
        assert(actual == values[index] * constant);
    }
}

static void verify_inline_u64_bitnot_expression(
    const uint8_t* data, uint64_t expression_end, uint64_t expression_start,
    const ObjSection* info, const ObjSection* strings,
    const ObjSection* locations, const char* source_name,
    int64_t source_frame_offset, bool is_unsigned) {
    static const uint64_t unsigned_values[] = {
        0u, 1u, UINT64_C(0xffffffff), UINT64_C(0x100000000),
        UINT64_C(0x1fffffffe), UINT64_C(0x7fffffffffffffff),
        UINT64_C(0xfffffffffffffffe), UINT64_MAX
    };
    static const uint64_t signed_values[] = {
        0u, 1u, UINT64_MAX, UINT64_C(0x7fffffffffffffff),
        UINT64_C(0x8000000000000007), UINT64_C(0x8000000000000008),
        UINT64_C(0xfffffffffffffff9), UINT64_C(0x8000000012345678)
    };
    const uint64_t* values = is_unsigned ? unsigned_values : signed_values;
    size_t value_count = is_unsigned
        ? sizeof(unsigned_values) / sizeof(unsigned_values[0])
        : sizeof(signed_values) / sizeof(signed_values[0]);
    uint64_t offset = expression_start;
    bool has_variable_location;
    bool variable_is_location_list;
    int64_t variable_frame_offset;
    assert(source_frame_offset <= INT64_MAX - 4);
    for (unsigned int word = 0u; word < 2u; ++word) {
        verify_inline_expression_frame_u32(
            data, expression_end, &offset,
            source_frame_offset + (int64_t)(word * 4u));
        verify_inline_expression_byte(data, expression_end, &offset, 0x20u);
        verify_inline_expression_byte(data, expression_end, &offset, 0x9fu);
        verify_inline_expression_byte(data, expression_end, &offset, 0x93u);
        assert(read_uleb(data, expression_end, &offset) == 4u);
    }
    assert(offset == expression_end);
    assert(find_variable_location(
        info, strings, locations, source_name, ARCH_X86,
        &has_variable_location, &variable_is_location_list,
        &variable_frame_offset));
    assert(has_variable_location &&
           variable_frame_offset == source_frame_offset);
    for (size_t index = 0u; index < value_count; ++index) {
        uint32_t low = ~(uint32_t)values[index];
        uint32_t high = ~(uint32_t)(values[index] >> 32u);
        uint64_t actual = ((uint64_t)high << 32u) | low;
        assert(actual == ~values[index]);
    }
}

static void verify_inline_u64_negate_expression(
    const uint8_t* data, uint64_t expression_end, uint64_t expression_start,
    const ObjSection* info, const ObjSection* strings,
    const ObjSection* locations, const char* source_name,
    int64_t source_frame_offset, bool is_unsigned) {
    static const uint64_t unsigned_values[] = {
        0u, 1u, UINT64_C(0xffffffff), UINT64_C(0x100000000),
        UINT64_C(0x1fffffffe), UINT64_C(0x7fffffffffffffff),
        UINT64_C(0xfffffffffffffffe), UINT64_MAX
    };
    static const uint64_t signed_values[] = {
        0u, 1u, UINT64_MAX, UINT64_C(0x7fffffffffffffff),
        UINT64_C(0x8000000000000007), UINT64_C(0x8000000000000008),
        UINT64_C(0xfffffffffffffff9), UINT64_C(0x8000000012345678)
    };
    const uint64_t* values = is_unsigned ? unsigned_values : signed_values;
    size_t value_count = is_unsigned
        ? sizeof(unsigned_values) / sizeof(unsigned_values[0])
        : sizeof(signed_values) / sizeof(signed_values[0]);
    uint64_t offset = expression_start;
    bool has_variable_location;
    bool variable_is_location_list;
    int64_t variable_frame_offset;
    assert(source_frame_offset <= INT64_MAX - 4);

    verify_inline_expression_frame_u32(
        data, expression_end, &offset, source_frame_offset);
    verify_inline_expression_constu(data, expression_end, &offset, 0u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x16u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x1cu);
    verify_inline_expression_byte(data, expression_end, &offset, 0x9fu);
    verify_inline_expression_byte(data, expression_end, &offset, 0x93u);
    assert(read_uleb(data, expression_end, &offset) == 4u);
    verify_inline_expression_frame_u32(
        data, expression_end, &offset, source_frame_offset + 4);
    verify_inline_expression_constu(data, expression_end, &offset, 0u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x16u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x1cu);
    verify_inline_expression_frame_u32(
        data, expression_end, &offset, source_frame_offset);
    verify_inline_expression_constu(data, expression_end, &offset, 0u);
    verify_inline_expression_byte(data, expression_end, &offset, 0x2eu);
    verify_inline_expression_byte(data, expression_end, &offset, 0x1cu);
    verify_inline_expression_byte(data, expression_end, &offset, 0x9fu);
    verify_inline_expression_byte(data, expression_end, &offset, 0x93u);
    assert(read_uleb(data, expression_end, &offset) == 4u);
    assert(offset == expression_end);

    assert(find_variable_location(
        info, strings, locations, source_name, ARCH_X86,
        &has_variable_location, &variable_is_location_list,
        &variable_frame_offset));
    assert(has_variable_location &&
           variable_frame_offset == source_frame_offset);
    for (size_t index = 0u; index < value_count; ++index) {
        uint32_t low = (uint32_t)values[index];
        uint32_t high = (uint32_t)(values[index] >> 32u);
        uint32_t low_result = 0u - low;
        uint32_t high_result = 0u - high - (low != 0u);
        uint64_t actual = ((uint64_t)high_result << 32u) | low_result;
        assert(actual == 0u - values[index]);
    }
}

static const char* inline_integer_comparison_entry_for_source(
    const char* source_name) {
    static const struct {
        const char* source;
        const char* entry;
    } entries[] = {
        {"debug_inline_nonconstant_integer_compare_less_entry_source",
         "debug_inline_nonconstant_integer_compare_less_entry"},
        {"debug_inline_nonconstant_integer_compare_greater_entry_source",
         "debug_inline_nonconstant_integer_compare_greater_entry"},
        {"debug_inline_nonconstant_integer_compare_less_equal_entry_source",
         "debug_inline_nonconstant_integer_compare_less_equal_entry"},
        {"debug_inline_nonconstant_integer_compare_greater_equal_entry_source",
         "debug_inline_nonconstant_integer_compare_greater_equal_entry"},
        {"debug_inline_nonconstant_integer_compare_not_equal_entry_source",
         "debug_inline_nonconstant_integer_compare_not_equal_entry"},
        {"debug_inline_nonconstant_unsigned_integer_compare_less_entry_source",
         "debug_inline_nonconstant_unsigned_integer_compare_less_entry"},
        {"debug_inline_nonconstant_integer_compare_mixed_entry_source",
         "debug_inline_nonconstant_integer_compare_mixed_entry"},
        {"debug_inline_nonconstant_integer_compare_implicit_entry_source",
         "debug_inline_nonconstant_integer_compare_implicit_entry"},
        {"debug_inline_nonconstant_integer_compare_promoted_short_entry_source",
         "debug_inline_nonconstant_integer_compare_promoted_short_entry"},
        {"debug_inline_nonconstant_integer_compare_promoted_unsigned_short_entry_source",
         "debug_inline_nonconstant_integer_compare_promoted_unsigned_short_entry"},
        {"debug_inline_nonconstant_long_compare_unsigned_int_entry_source",
         "debug_inline_nonconstant_long_compare_unsigned_int_entry"},
        {"debug_inline_nonconstant_unsigned_long_compare_signed_int_entry_source",
         "debug_inline_nonconstant_unsigned_long_compare_signed_int_entry"},
        {"debug_inline_unsigned_enum_long_compare_entry_source",
         "debug_inline_unsigned_enum_long_compare_entry"},
        {"debug_inline_unsigned_char_enum_int_compare_entry_source",
         "debug_inline_unsigned_char_enum_int_compare_entry"},
        {"debug_inline_unsigned_short_enum_long_compare_entry_source",
         "debug_inline_unsigned_short_enum_long_compare_entry"},
        {"debug_inline_unsigned_char_enum_signed_char_enum_compare_entry_source",
         "debug_inline_unsigned_char_enum_signed_char_enum_compare_entry"},
        {"debug_inline_signed_char_enum_unsigned_int_compare_entry_source",
         "debug_inline_signed_char_enum_unsigned_int_compare_entry"},
        {"debug_inline_signed_long_long_compare_entry_source",
         "debug_inline_signed_long_long_compare_entry"},
        {"debug_inline_unsigned_long_long_compare_entry_source",
         "debug_inline_unsigned_long_long_compare_entry"},
        {"debug_inline_signed_long_long_less_equal_entry_source",
         "debug_inline_signed_long_long_less_equal_entry"},
        {"debug_inline_signed_long_long_greater_entry_source",
         "debug_inline_signed_long_long_greater_entry"},
        {"debug_inline_unsigned_long_long_greater_equal_entry_source",
         "debug_inline_unsigned_long_long_greater_equal_entry"},
        {"debug_inline_unsigned_long_long_equal_entry_source",
         "debug_inline_unsigned_long_long_equal_entry"},
        {"debug_inline_unsigned_long_long_not_equal_entry_source",
         "debug_inline_unsigned_long_long_not_equal_entry"},
        {"debug_inline_signed_long_long_unsigned_int_compare_entry_source",
         "debug_inline_signed_long_long_unsigned_int_compare_entry"},
        {"debug_inline_unsigned_long_long_signed_int_compare_entry_source",
         "debug_inline_unsigned_long_long_signed_int_compare_entry"},
        {"debug_inline_signed_long_long_int_compare_entry_source",
         "debug_inline_signed_long_long_int_compare_entry"},
        {"debug_inline_unsigned_long_long_unsigned_int_compare_entry_source",
         "debug_inline_unsigned_long_long_unsigned_int_compare_entry"},
        {"debug_inline_signed_long_long_signed_char_compare_entry_source",
         "debug_inline_signed_long_long_signed_char_compare_entry"},
        {"debug_inline_signed_long_long_unsigned_char_compare_entry_source",
         "debug_inline_signed_long_long_unsigned_char_compare_entry"},
        {"debug_inline_unsigned_long_long_signed_short_compare_entry_source",
         "debug_inline_unsigned_long_long_signed_short_compare_entry"},
        {"debug_inline_unsigned_long_long_unsigned_short_compare_entry_source",
         "debug_inline_unsigned_long_long_unsigned_short_compare_entry"},
        {"debug_inline_signed_long_long_bool_compare_entry_source",
         "debug_inline_signed_long_long_bool_compare_entry"},
        {"debug_inline_unsigned_long_long_signed_char_enum_compare_entry_source",
         "debug_inline_unsigned_long_long_signed_char_enum_compare_entry"},
        {"debug_inline_signed_long_long_unsigned_short_enum_compare_entry_source",
         "debug_inline_signed_long_long_unsigned_short_enum_compare_entry"},
        {"debug_inline_signed_long_long_unsigned_int_enum_compare_entry_source",
         "debug_inline_signed_long_long_unsigned_int_enum_compare_entry"},
        {"debug_inline_signed_long_long_unsigned_char_enum_compare_entry_source",
         "debug_inline_signed_long_long_unsigned_char_enum_compare_entry"},
        {"debug_inline_signed_long_long_signed_short_enum_compare_entry_source",
         "debug_inline_signed_long_long_signed_short_enum_compare_entry"},
        {"debug_inline_signed_long_long_signed_int_enum_compare_entry_source",
         "debug_inline_signed_long_long_signed_int_enum_compare_entry"},
        {"debug_inline_signed_long_long_scoped_unsigned_char_enum_compare_entry_source",
         "debug_inline_signed_long_long_scoped_unsigned_char_enum_compare_entry"},
        {"debug_inline_signed_long_long_scoped_signed_short_enum_compare_entry_source",
         "debug_inline_signed_long_long_scoped_signed_short_enum_compare_entry"},
        {"debug_inline_signed_long_long_scoped_unsigned_short_enum_compare_entry_source",
         "debug_inline_signed_long_long_scoped_unsigned_short_enum_compare_entry"},
        {"debug_inline_signed_long_long_scoped_signed_int_enum_compare_entry_source",
         "debug_inline_signed_long_long_scoped_signed_int_enum_compare_entry"},
        {"debug_inline_unsigned_long_long_scoped_unsigned_wide_enum_compare_entry_source",
         "debug_inline_unsigned_long_long_scoped_unsigned_wide_enum_compare_entry"},
        {"debug_inline_unsigned_long_long_scoped_signed_short_enum_to_unsigned_compare_entry_source",
         "debug_inline_unsigned_long_long_scoped_signed_short_enum_to_unsigned_compare_entry"},
        {"debug_inline_signed_long_long_add_one_compare_entry_source",
         "debug_inline_signed_long_long_add_one_compare_entry"},
        {"debug_inline_signed_long_long_sub_one_compare_entry_source",
         "debug_inline_signed_long_long_sub_one_compare_entry"},
        {"debug_inline_signed_long_long_add_compare_entry_source",
         "debug_inline_signed_long_long_add_compare_entry"},
        {"debug_inline_unsigned_long_long_add_compare_entry_source",
         "debug_inline_unsigned_long_long_add_compare_entry"},
        {"debug_inline_signed_long_long_sub_compare_entry_source",
         "debug_inline_signed_long_long_sub_compare_entry"},
        {"debug_inline_unsigned_long_long_sub_compare_entry_source",
         "debug_inline_unsigned_long_long_sub_compare_entry"},
        {"debug_inline_signed_long_long_bitand_compare_entry_source",
         "debug_inline_signed_long_long_bitand_compare_entry"},
        {"debug_inline_signed_long_long_bitor_compare_entry_source",
         "debug_inline_signed_long_long_bitor_compare_entry"},
        {"debug_inline_signed_long_long_bitxor_compare_entry_source",
         "debug_inline_signed_long_long_bitxor_compare_entry"},
        {"debug_inline_signed_long_long_bitnot_compare_entry_source",
         "debug_inline_signed_long_long_bitnot_compare_entry"},
        {"debug_inline_signed_long_long_nested_bitwise_compare_entry_source",
         "debug_inline_signed_long_long_nested_bitwise_compare_entry"},
        {"debug_inline_signed_long_long_nested_add_bitxor_compare_entry_source",
         "debug_inline_signed_long_long_nested_add_bitxor_compare_entry"},
        {"debug_inline_signed_long_long_nested_sub_bitxor_compare_entry_source",
         "debug_inline_signed_long_long_nested_sub_bitxor_compare_entry"},
        {"debug_inline_signed_long_long_mul_small_compare_entry_source",
         "debug_inline_signed_long_long_mul_small_compare_entry"},
        {"debug_inline_signed_long_long_mul_wide_constant_compare_entry_source",
         "debug_inline_signed_long_long_mul_wide_constant_compare_entry"},
        {"debug_inline_signed_long_long_nested_add_mul_wide_constant_compare_entry_source",
         "debug_inline_signed_long_long_nested_add_mul_wide_constant_compare_entry"},
        {"debug_inline_signed_long_long_negated_bitwise_compare_entry_source",
         "debug_inline_signed_long_long_negated_bitwise_compare_entry"},
        {"debug_inline_unsigned_long_long_shift_compare_entry_source",
         "debug_inline_unsigned_long_long_shift_compare_entry"},
        {"debug_inline_signed_long_long_shift_compare_entry_source",
         "debug_inline_signed_long_long_shift_compare_entry"}
    };
    if (!source_name) return NULL;
    for (size_t index = 0u; index < sizeof(entries) / sizeof(entries[0]);
         ++index) {
        if (strcmp(source_name, entries[index].source) == 0) {
            return entries[index].entry;
        }
    }
    return NULL;
}

static const char* inline_integer_comparison_secondary_source(
    const char* source_name) {
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_int_compare_entry_source") == 0) {
        return "debug_inline_signed_long_long_int_compare_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_unsigned_long_long_unsigned_int_compare_entry_source") == 0) {
        return "debug_inline_unsigned_long_long_unsigned_int_compare_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_signed_char_compare_entry_source") == 0) {
        return "debug_inline_signed_long_long_signed_char_compare_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_unsigned_char_compare_entry_source") == 0) {
        return "debug_inline_signed_long_long_unsigned_char_compare_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_unsigned_long_long_signed_short_compare_entry_source") == 0) {
        return "debug_inline_unsigned_long_long_signed_short_compare_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_unsigned_long_long_unsigned_short_compare_entry_source") == 0) {
        return "debug_inline_unsigned_long_long_unsigned_short_compare_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_bool_compare_entry_source") == 0) {
        return "debug_inline_signed_long_long_bool_compare_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_unsigned_long_long_signed_char_enum_compare_entry_source") == 0) {
        return "debug_inline_unsigned_long_long_signed_char_enum_compare_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_unsigned_short_enum_compare_entry_source") == 0) {
        return "debug_inline_signed_long_long_unsigned_short_enum_compare_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_unsigned_int_enum_compare_entry_source") == 0) {
        return "debug_inline_signed_long_long_unsigned_int_enum_compare_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_unsigned_char_enum_compare_entry_source") == 0) {
        return "debug_inline_signed_long_long_unsigned_char_enum_compare_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_signed_short_enum_compare_entry_source") == 0) {
        return "debug_inline_signed_long_long_signed_short_enum_compare_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_signed_int_enum_compare_entry_source") == 0) {
        return "debug_inline_signed_long_long_signed_int_enum_compare_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_scoped_unsigned_char_enum_compare_entry_source") == 0) {
        return "debug_inline_signed_long_long_scoped_unsigned_char_enum_compare_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_scoped_signed_short_enum_compare_entry_source") == 0) {
        return "debug_inline_signed_long_long_scoped_signed_short_enum_compare_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_scoped_unsigned_short_enum_compare_entry_source") == 0) {
        return "debug_inline_signed_long_long_scoped_unsigned_short_enum_compare_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_scoped_signed_int_enum_compare_entry_source") == 0) {
        return "debug_inline_signed_long_long_scoped_signed_int_enum_compare_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_unsigned_long_long_scoped_unsigned_wide_enum_compare_entry_source") == 0) {
        return "debug_inline_unsigned_long_long_scoped_unsigned_wide_enum_compare_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_unsigned_long_long_scoped_signed_short_enum_to_unsigned_compare_entry_source") == 0) {
        return "debug_inline_unsigned_long_long_scoped_signed_short_enum_to_unsigned_compare_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_add_compare_entry_source") == 0) {
        return "debug_inline_signed_long_long_add_compare_entry_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_unsigned_long_long_add_compare_entry_source") == 0) {
        return "debug_inline_unsigned_long_long_add_compare_entry_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_sub_compare_entry_source") == 0) {
        return "debug_inline_signed_long_long_sub_compare_entry_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_unsigned_long_long_sub_compare_entry_source") == 0) {
        return "debug_inline_unsigned_long_long_sub_compare_entry_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_bitand_compare_entry_source") == 0) {
        return "debug_inline_signed_long_long_bitand_compare_entry_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_bitor_compare_entry_source") == 0) {
        return "debug_inline_signed_long_long_bitor_compare_entry_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_bitxor_compare_entry_source") == 0) {
        return "debug_inline_signed_long_long_bitxor_compare_entry_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_nested_bitwise_compare_entry_source") == 0) {
        return "debug_inline_signed_long_long_nested_bitwise_compare_entry_rhs";
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_negated_bitwise_compare_entry_source") == 0) {
        return "debug_inline_signed_long_long_negated_bitwise_compare_entry_rhs";
    }
    return NULL;
}

static uint8_t inline_integer_comparison_secondary_size(
    const char* source_name) {
    if (source_name &&
        (strcmp(source_name,
                "debug_inline_signed_long_long_signed_char_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_unsigned_char_compare_entry_source") == 0)) {
        return 1u;
    }
    if (source_name &&
        (strcmp(source_name,
                "debug_inline_unsigned_long_long_signed_short_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_unsigned_short_compare_entry_source") == 0)) {
        return 2u;
    }
    if (source_name &&
        (strcmp(source_name,
                "debug_inline_signed_long_long_int_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_unsigned_int_compare_entry_source") == 0)) {
        return 4u;
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_bool_compare_entry_source") == 0) {
        return 1u;
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_unsigned_long_long_signed_char_enum_compare_entry_source") == 0) {
        return 1u;
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_unsigned_short_enum_compare_entry_source") == 0) {
        return 2u;
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_unsigned_int_enum_compare_entry_source") == 0) {
        return 4u;
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_unsigned_char_enum_compare_entry_source") == 0) {
        return 1u;
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_signed_short_enum_compare_entry_source") == 0) {
        return 2u;
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_signed_int_enum_compare_entry_source") == 0) {
        return 4u;
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_scoped_unsigned_char_enum_compare_entry_source") == 0) {
        return 1u;
    }
    if (source_name &&
        (strcmp(source_name,
                "debug_inline_signed_long_long_scoped_signed_short_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_scoped_unsigned_short_enum_compare_entry_source") == 0)) {
        return 2u;
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_scoped_signed_int_enum_compare_entry_source") == 0) {
        return 4u;
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_unsigned_long_long_scoped_unsigned_wide_enum_compare_entry_source") == 0) {
        return 8u;
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_unsigned_long_long_scoped_signed_short_enum_to_unsigned_compare_entry_source") == 0) {
        return 2u;
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_add_compare_entry_source") == 0) {
        return 8u;
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_unsigned_long_long_add_compare_entry_source") == 0) {
        return 8u;
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_sub_compare_entry_source") == 0) {
        return 8u;
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_unsigned_long_long_sub_compare_entry_source") == 0) {
        return 8u;
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_bitand_compare_entry_source") == 0) {
        return 8u;
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_bitor_compare_entry_source") == 0) {
        return 8u;
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_bitxor_compare_entry_source") == 0) {
        return 8u;
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_nested_bitwise_compare_entry_source") == 0) {
        return 8u;
    }
    if (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_negated_bitwise_compare_entry_source") == 0) {
        return 8u;
    }
    return 0u;
}

static bool inline_integer_comparison_secondary_is_unsigned(
    const char* source_name) {
    return source_name &&
        (strcmp(source_name,
                "debug_inline_signed_long_long_unsigned_char_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_unsigned_short_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_unsigned_int_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_unsigned_short_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_unsigned_int_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_unsigned_char_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_scoped_unsigned_char_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_scoped_unsigned_short_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_scoped_unsigned_wide_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_add_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_sub_compare_entry_source") == 0);
}

static bool inline_integer_comparison_secondary_is_bool(
    const char* source_name) {
    return source_name && strcmp(
        source_name,
        "debug_inline_signed_long_long_bool_compare_entry_source") == 0;
}

static void verify_inline_integer_expression_parameter(
    const char* path, uint16_t architecture, const char* inline_name,
    const char* source_name, bool is_unsigned, uint8_t expected_operation,
    uint8_t value_size, int64_t expected_constant,
    const char* divisor_source_name,
    bool evaluate_full_width_unsigned) {
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* strings;
    ObjSection* locations;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint64_t call_die = UINT64_MAX;
    uint64_t origin_die = UINT64_MAX;
    uint64_t parameter_die;
    uint64_t expression_offset;
    uint64_t expression_size;
    uint64_t expression_end;
    uint64_t expression_start;
    int64_t frame_offset;
    int64_t variable_frame_offset;
    int64_t divisor_frame_offset = 0;
    bool has_variable_location;
    bool variable_is_location_list;
    bool has_divisor_variable_location = false;
    bool divisor_is_location_list = false;
    const char* comparison_entry =
        inline_integer_comparison_entry_for_source(source_name);
    bool mixed_conversion_comparison = source_name &&
        (strcmp(source_name,
                "debug_inline_nonconstant_integer_compare_mixed_entry_source")
             == 0 ||
         strcmp(source_name,
                "debug_inline_nonconstant_integer_compare_implicit_entry_source")
             == 0);
    bool integer_promotion_comparison = source_name &&
        strcmp(source_name,
               "debug_inline_nonconstant_integer_compare_promoted_short_entry_source")
            == 0;
    bool unsigned_integer_promotion_comparison = source_name &&
        strcmp(source_name,
               "debug_inline_nonconstant_integer_compare_promoted_unsigned_short_entry_source")
            == 0;
    bool mixed_rank_comparison = source_name &&
        strcmp(source_name,
               "debug_inline_nonconstant_long_compare_unsigned_int_entry_source")
            == 0;
    bool enum_rank_comparison = source_name &&
        strcmp(source_name,
               "debug_inline_unsigned_enum_long_compare_entry_source") == 0;
    bool promoted_enum_comparison = source_name &&
        strcmp(source_name,
               "debug_inline_unsigned_char_enum_int_compare_entry_source") == 0;
    bool promoted_unsigned_short_enum_comparison = source_name &&
        strcmp(source_name,
               "debug_inline_unsigned_short_enum_long_compare_entry_source") == 0;
    bool enum_to_enum_comparison = source_name &&
        strcmp(source_name,
               "debug_inline_unsigned_char_enum_signed_char_enum_compare_entry_source")
            == 0;
    bool signed_char_enum_conversion_comparison = source_name &&
        strcmp(source_name,
               "debug_inline_signed_char_enum_unsigned_int_compare_entry_source")
            == 0;
    bool unsigned_long_conversion_comparison = source_name &&
        strcmp(source_name,
               "debug_inline_nonconstant_unsigned_long_compare_signed_int_entry_source")
            == 0;
    bool wide_integer_comparison = source_name &&
        (strcmp(source_name,
                "debug_inline_signed_long_long_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_less_equal_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_greater_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_greater_equal_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_equal_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_not_equal_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_unsigned_int_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_signed_int_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_int_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_unsigned_int_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_signed_char_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_unsigned_char_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_signed_short_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_unsigned_short_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_bool_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_signed_char_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_unsigned_short_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_unsigned_int_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_unsigned_char_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_signed_short_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_signed_int_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_scoped_unsigned_char_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_scoped_signed_short_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_scoped_unsigned_short_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_scoped_signed_int_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_scoped_unsigned_wide_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_scoped_signed_short_enum_to_unsigned_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_bitnot_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_add_one_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_sub_one_compare_entry_source") == 0);
    bool narrow_wide_comparison = source_name &&
        (strcmp(source_name,
                "debug_inline_signed_long_long_unsigned_int_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_signed_int_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_int_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_unsigned_int_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_signed_char_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_unsigned_char_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_signed_short_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_unsigned_short_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_bool_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_signed_char_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_unsigned_short_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_unsigned_int_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_unsigned_char_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_signed_short_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_signed_int_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_scoped_unsigned_char_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_scoped_signed_short_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_scoped_unsigned_short_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_scoped_signed_int_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_scoped_unsigned_wide_enum_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_unsigned_long_long_scoped_signed_short_enum_to_unsigned_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_bitnot_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_add_one_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_sub_one_compare_entry_source") == 0);
    wide_integer_comparison = wide_integer_comparison ||
        (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_add_compare_entry_source") == 0);
    narrow_wide_comparison = narrow_wide_comparison ||
        (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_add_compare_entry_source") == 0);
    wide_integer_comparison = wide_integer_comparison ||
        (source_name && strcmp(
            source_name,
            "debug_inline_unsigned_long_long_add_compare_entry_source") == 0);
    narrow_wide_comparison = narrow_wide_comparison ||
        (source_name && strcmp(
            source_name,
            "debug_inline_unsigned_long_long_add_compare_entry_source") == 0);
    wide_integer_comparison = wide_integer_comparison ||
        (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_sub_compare_entry_source") == 0);
    narrow_wide_comparison = narrow_wide_comparison ||
        (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_sub_compare_entry_source") == 0);
    wide_integer_comparison = wide_integer_comparison ||
        (source_name && strcmp(
            source_name,
            "debug_inline_unsigned_long_long_sub_compare_entry_source") == 0);
    narrow_wide_comparison = narrow_wide_comparison ||
        (source_name && strcmp(
            source_name,
            "debug_inline_unsigned_long_long_sub_compare_entry_source") == 0);
    wide_integer_comparison = wide_integer_comparison ||
        (source_name && (strcmp(
            source_name,
            "debug_inline_signed_long_long_bitand_compare_entry_source") == 0 ||
            strcmp(source_name,
            "debug_inline_signed_long_long_bitor_compare_entry_source") == 0 ||
            strcmp(source_name,
            "debug_inline_signed_long_long_bitxor_compare_entry_source") == 0 ||
            strcmp(source_name,
            "debug_inline_signed_long_long_nested_bitwise_compare_entry_source") == 0 ||
            strcmp(source_name,
            "debug_inline_signed_long_long_nested_add_bitxor_compare_entry_source") == 0 ||
            strcmp(source_name,
            "debug_inline_signed_long_long_nested_sub_bitxor_compare_entry_source") == 0 ||
            strcmp(source_name,
            "debug_inline_signed_long_long_mul_small_compare_entry_source") == 0 ||
            strcmp(source_name,
            "debug_inline_signed_long_long_mul_wide_constant_compare_entry_source") == 0 ||
            strcmp(source_name,
            "debug_inline_signed_long_long_nested_add_mul_wide_constant_compare_entry_source") == 0 ||
            strcmp(source_name,
            "debug_inline_signed_long_long_negated_bitwise_compare_entry_source") == 0));
    wide_integer_comparison = wide_integer_comparison ||
        (source_name && strcmp(
            source_name,
            "debug_inline_unsigned_long_long_shift_compare_entry_source") == 0);
    narrow_wide_comparison = narrow_wide_comparison ||
        (source_name && strcmp(
            source_name,
            "debug_inline_unsigned_long_long_shift_compare_entry_source") == 0);
    wide_integer_comparison = wide_integer_comparison ||
        (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_shift_compare_entry_source") == 0);
    narrow_wide_comparison = narrow_wide_comparison ||
        (source_name && strcmp(
            source_name,
            "debug_inline_signed_long_long_shift_compare_entry_source") == 0);
    wide_integer_comparison = wide_integer_comparison ||
        (source_name && strcmp(
            source_name,
            "debug_inline_nonconstant_unsigned_integer_logical_not_wide_shift_source") == 0);
    narrow_wide_comparison = narrow_wide_comparison ||
        (source_name && strcmp(
            source_name,
            "debug_inline_nonconstant_unsigned_integer_logical_not_wide_shift_source") == 0);
    narrow_wide_comparison = narrow_wide_comparison ||
        (source_name && (strcmp(
            source_name,
            "debug_inline_signed_long_long_bitand_compare_entry_source") == 0 ||
            strcmp(source_name,
            "debug_inline_signed_long_long_bitor_compare_entry_source") == 0 ||
            strcmp(source_name,
            "debug_inline_signed_long_long_bitxor_compare_entry_source") == 0 ||
            strcmp(source_name,
            "debug_inline_signed_long_long_nested_bitwise_compare_entry_source") == 0 ||
            strcmp(source_name,
            "debug_inline_signed_long_long_nested_add_bitxor_compare_entry_source") == 0 ||
            strcmp(source_name,
            "debug_inline_signed_long_long_nested_sub_bitxor_compare_entry_source") == 0 ||
            strcmp(source_name,
            "debug_inline_signed_long_long_mul_small_compare_entry_source") == 0 ||
            strcmp(source_name,
            "debug_inline_signed_long_long_mul_wide_constant_compare_entry_source") == 0 ||
            strcmp(source_name,
            "debug_inline_signed_long_long_nested_add_mul_wide_constant_compare_entry_source") == 0 ||
            strcmp(source_name,
            "debug_inline_signed_long_long_negated_bitwise_compare_entry_source") == 0));
    bool add_secondary_wide_comparison = source_name && strcmp(
        source_name,
        "debug_inline_signed_long_long_add_compare_entry_source") == 0;
    bool add_secondary_unsigned_wide_comparison = source_name && strcmp(
        source_name,
        "debug_inline_unsigned_long_long_add_compare_entry_source") == 0;
    bool sub_secondary_wide_comparison = source_name && strcmp(
        source_name,
        "debug_inline_signed_long_long_sub_compare_entry_source") == 0;
    bool sub_secondary_unsigned_wide_comparison = source_name && strcmp(
        source_name,
        "debug_inline_unsigned_long_long_sub_compare_entry_source") == 0;
    bool bitand_secondary_comparison = source_name && strcmp(
        source_name,
        "debug_inline_signed_long_long_bitand_compare_entry_source") == 0;
    bool bitor_secondary_comparison = source_name && strcmp(
        source_name,
        "debug_inline_signed_long_long_bitor_compare_entry_source") == 0;
    bool bitxor_secondary_comparison = source_name && strcmp(
        source_name,
        "debug_inline_signed_long_long_bitxor_compare_entry_source") == 0;
    bool nested_bitwise_comparison = source_name && strcmp(
        source_name,
        "debug_inline_signed_long_long_nested_bitwise_compare_entry_source") == 0;
    bool negated_bitwise_comparison = source_name && strcmp(
        source_name,
        "debug_inline_signed_long_long_negated_bitwise_compare_entry_source") == 0;
    bool nested_add_bitxor_wide_comparison = source_name && strcmp(
        source_name,
        "debug_inline_signed_long_long_nested_add_bitxor_compare_entry_source") == 0;
    bool nested_sub_bitxor_wide_comparison = source_name && strcmp(
        source_name,
        "debug_inline_signed_long_long_nested_sub_bitxor_compare_entry_source") == 0;
    bool wide_multiply_constant_comparison = source_name &&
        (strcmp(source_name,
                "debug_inline_signed_long_long_mul_small_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_mul_wide_constant_compare_entry_source") == 0 ||
         strcmp(source_name,
                "debug_inline_signed_long_long_nested_add_mul_wide_constant_compare_entry_source") == 0);
    bool shift_left_wide_comparison = source_name && strcmp(
        source_name,
        "debug_inline_unsigned_long_long_shift_compare_entry_source") == 0;
    bool computed_wide_logical_not = source_name && strcmp(
        source_name,
        "debug_inline_nonconstant_unsigned_integer_logical_not_wide_shift_source") == 0;
    bool signed_right_shift_wide_comparison = source_name && strcmp(
        source_name,
        "debug_inline_signed_long_long_shift_compare_entry_source") == 0;
    bool bitnot_wide_comparison = source_name && strcmp(
        source_name,
        "debug_inline_signed_long_long_bitnot_compare_entry_source") == 0;
    bool add_one_wide_comparison = source_name &&
        strcmp(source_name,
               "debug_inline_signed_long_long_add_one_compare_entry_source") == 0;
    bool sub_one_wide_comparison = source_name &&
        strcmp(source_name,
               "debug_inline_signed_long_long_sub_one_compare_entry_source") == 0;
    bool comparison_is_unsigned = is_unsigned;
    uint8_t comparison_value_size = value_size;
    const char* call_site_name = comparison_entry ? comparison_entry :
        expected_operation == 0xc4u
        ? "debug_inline_nonconstant_signed_long_long_lshift_computed_mod64_count_entry"
        : expected_operation == 0x29u &&
              strcmp(source_name,
                     "debug_inline_nonconstant_integer_logical_not_equality_source") == 0
        ? "debug_inline_nonconstant_integer_logical_not_equality_entry"
        : expected_operation == 0x29u &&
              strcmp(source_name,
                     "debug_inline_nonconstant_integer_logical_not_wide_source") == 0
        ? "debug_inline_nonconstant_integer_logical_not_wide_entry"
        : expected_operation == 0x29u &&
              strcmp(source_name,
                     "debug_inline_nonconstant_unsigned_integer_logical_not_wide_shift_source") == 0
        ? "debug_inline_nonconstant_unsigned_integer_logical_not_wide_shift_entry"
        : expected_operation == 0x29u
        ? "debug_inline_nonconstant_integer_logical_not_entry"
        : expected_operation == 0xc3u
        ? "debug_inline_nonconstant_signed_long_long_lshift_mod64_count_entry"
        : expected_operation == 0xc2u
        ? "debug_inline_nonconstant_unsigned_long_long_lshift_mod64_count_entry"
        : expected_operation == 0xc1u
        ? "debug_inline_nonconstant_unsigned_long_long_lshift_wide_computed_count_entry"
        : expected_operation == 0xc0u
        ? "debug_inline_nonconstant_unsigned_long_long_lshift_computed_count_entry"
        : (expected_operation >= 0xdau && expected_operation <= 0xdfu)
        ? "debug_inline_nonconstant_unsigned_long_long_wide_divmod_entry"
        : expected_operation == 0xd2u
        ? "debug_inline_nonconstant_unsigned_long_long_lshift_variable_entry"
        : expected_operation == 0xd3u
            ? "debug_inline_nonconstant_unsigned_long_long_rshift_variable_entry"
            : expected_operation == 0xd4u
                ? "debug_inline_nonconstant_signed_long_long_lshift_variable_entry"
                : expected_operation == 0xd5u
                    ? "debug_inline_nonconstant_signed_long_long_rshift_variable_entry"
            : expected_operation == 0xd6u
                ? "debug_inline_nonconstant_unsigned_long_long_lshift_wide_count_entry"
                : expected_operation == 0xd7u
                    ? "debug_inline_nonconstant_unsigned_long_long_rshift_wide_count_entry"
                    : expected_operation == 0xd8u
                        ? "debug_inline_nonconstant_signed_long_long_lshift_wide_count_entry"
                        : expected_operation == 0xd9u
                            ? "debug_inline_nonconstant_signed_long_long_rshift_wide_count_entry"
            : expected_operation == 0xd0u
        ? "debug_inline_nonconstant_signed_long_long_lshift_entry"
        : expected_operation == 0xd1u
            ? "debug_inline_nonconstant_signed_long_long_rshift_entry"
            : expected_operation == 0xeeu
                ? "debug_inline_nonconstant_unsigned_long_long_lshift_entry"
                : expected_operation == 0xefu
                    ? "debug_inline_nonconstant_unsigned_long_long_rshift_entry"
            : expected_operation == 0xedu
        ? "debug_inline_nonconstant_signed_long_long_mul_constant_entry"
        : expected_operation == 0xebu
        ? "debug_inline_nonconstant_signed_long_long_bitnot_entry"
        : expected_operation == 0xecu
            ? "debug_inline_nonconstant_signed_long_long_negate_entry"
            : expected_operation == 0xe2u
        ? "debug_inline_nonconstant_signed_long_long_and_constant_entry"
        : expected_operation == 0xe3u
            ? "debug_inline_nonconstant_signed_long_long_or_constant_entry"
            : expected_operation == 0xeau
                ? "debug_inline_nonconstant_signed_long_long_xor_constant_entry"
                : expected_operation == 0xe1u
        ? "debug_inline_nonconstant_signed_long_long_add_constant_entry"
        : expected_operation == 0xe0u
            ? "debug_inline_nonconstant_signed_long_long_sub_constant_entry"
            : expected_operation == 0xe9u
        ? "debug_inline_nonconstant_signed_long_long_add_two_variables_entry"
        : expected_operation == 0xe8u
            ? "debug_inline_nonconstant_signed_long_long_sub_two_variables_entry"
            : expected_operation == 0xe7u
                ? "debug_inline_nonconstant_signed_long_long_and_two_variables_entry"
                : expected_operation == 0xe6u
                    ? "debug_inline_nonconstant_signed_long_long_or_two_variables_entry"
                    : expected_operation == 0xe5u
                        ? "debug_inline_nonconstant_signed_long_long_xor_two_variables_entry"
                        : expected_operation == 0xe4u
                            ? "debug_inline_nonconstant_signed_long_long_mul_two_variables_entry"
                            : expected_operation == 0xf8u
                                ? "debug_inline_nonconstant_unsigned_long_long_add_two_variables_entry"
                                : expected_operation == 0xf7u
                                    ? "debug_inline_nonconstant_unsigned_long_long_sub_two_variables_entry"
                                    : expected_operation == 0xf6u
                                        ? "debug_inline_nonconstant_unsigned_long_long_and_two_variables_entry"
                                        : expected_operation == 0xf5u
                                            ? "debug_inline_nonconstant_unsigned_long_long_or_two_variables_entry"
                                            : expected_operation == 0xf4u
                                                ? "debug_inline_nonconstant_unsigned_long_long_xor_two_variables_entry"
                                                : expected_operation == 0xf3u
                                                    ? "debug_inline_nonconstant_unsigned_long_long_mul_two_variables_entry"
                                                    : "debug_inline_nonconstant_integer_entry";
    if (mixed_rank_comparison) {
        comparison_is_unsigned = architecture == ARCH_X86;
        comparison_value_size = (uint8_t)address_size;
    } else if (enum_rank_comparison) {
        comparison_is_unsigned = architecture == ARCH_X86;
        comparison_value_size = (uint8_t)address_size;
    } else if (unsigned_long_conversion_comparison) {
        comparison_value_size = (uint8_t)address_size;
    } else if (promoted_enum_comparison) {
        comparison_is_unsigned = false;
        comparison_value_size = 4u;
    } else if (promoted_unsigned_short_enum_comparison) {
        comparison_is_unsigned = false;
        comparison_value_size = (uint8_t)address_size;
    } else if (enum_to_enum_comparison) {
        comparison_is_unsigned = false;
        comparison_value_size = 4u;
    } else if (signed_char_enum_conversion_comparison) {
        comparison_is_unsigned = true;
        comparison_value_size = 4u;
    }
    assert(object != NULL && object->arch == architecture && inline_name &&
           source_name &&
           (expected_operation == 0xe0u || expected_operation == 0xe1u ||
            expected_operation == 0xe2u || expected_operation == 0xe3u ||
            expected_operation == 0xeau || expected_operation == 0xedu ||
            expected_operation == 0xebu ||
            expected_operation == 0xecu || expected_operation == 0x29u ||
            expected_operation == 0x2au || expected_operation == 0x2bu ||
            expected_operation == 0x2cu || expected_operation == 0x2du ||
            expected_operation == 0x2eu ||
            expected_operation == 0xc0u ||
            expected_operation == 0xc1u || expected_operation == 0xc2u ||
            expected_operation == 0xc3u || expected_operation == 0xc4u ||
            (expected_operation >= 0xdau && expected_operation <= 0xdfu) ||
            expected_operation == 0xe4u || expected_operation == 0xe5u ||
            expected_operation == 0xe6u || expected_operation == 0xe7u ||
            expected_operation == 0xe8u || expected_operation == 0xe9u ||
            expected_operation == 0xf3u || expected_operation == 0xf4u ||
            expected_operation == 0xf5u ||
            expected_operation == 0xf6u || expected_operation == 0xf7u ||
            expected_operation == 0xf8u ||
            expected_operation == 0xf9u ||
            expected_operation == 0xfau ||
            expected_operation == 0x22u || expected_operation == 0x1cu ||
            expected_operation == 0x1eu || expected_operation == 0x1fu ||
            expected_operation == 0x20u ||
            expected_operation == 0x1au || expected_operation == 0x21u ||
            expected_operation == 0x27u || expected_operation == 0x1bu ||
            expected_operation == 0x1du ||
            expected_operation == 0xd0u || expected_operation == 0xd1u ||
            expected_operation == 0xd2u || expected_operation == 0xd3u ||
            expected_operation == 0xd4u || expected_operation == 0xd5u ||
            expected_operation == 0xd6u || expected_operation == 0xd7u ||
            expected_operation == 0xd8u || expected_operation == 0xd9u ||
            expected_operation == 0xeeu || expected_operation == 0xefu ||
            expected_operation == 0x24u ||
            expected_operation == 0x25u || expected_operation == 0x26u ||
            expected_operation == 0xfbu || expected_operation == 0xfcu ||
            expected_operation == 0xfdu || expected_operation == 0xfeu));
    info = objfile_get_section(object, ".debug_info");
    strings = objfile_get_section(object, ".debug_str");
    locations = objfile_get_section(object, ".debug_loc");
    assert(info != NULL && strings != NULL);
    for (uint64_t die = 11u; die + 1u + 4u + address_size + 4u + 12u <=
         info->size; ++die) {
        uint32_t candidate_origin;
        uint32_t name_offset;
        uint64_t low_pc_offset;
        bool matches_call = false;
        if (info->data[die] != 41u) continue;
        candidate_origin = read_u32(info->data, die + 1u);
        if (candidate_origin >= info->size ||
            info->data[candidate_origin] != 40u) continue;
        name_offset = read_u32(info->data, candidate_origin + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset,
                   inline_name) != 0) {
            continue;
        }
        low_pc_offset = die + 1u + 4u;
        for (const ObjReloc* relocation = info->relocs; relocation;
             relocation = relocation->next) {
            if (relocation->offset == low_pc_offset &&
                relocation->symbol_name &&
                strcmp(relocation->symbol_name, call_site_name) == 0) {
                matches_call = true;
                break;
            }
        }
        if (!matches_call) continue;
        assert(call_die == UINT64_MAX);
        call_die = die;
        origin_die = candidate_origin;
    }
    if (call_die == UINT64_MAX || origin_die == UINT64_MAX) {
        fprintf(stderr, "missing inline DWARF call for %s (%s)\n",
                inline_name, path);
        assert(call_die != UINT64_MAX && origin_die != UINT64_MAX);
    }
    parameter_die = call_die + 1u + 4u + address_size + 4u + 12u;
    if (parameter_die + 5u >= info->size ||
        info->data[parameter_die] != 67u ||
        read_u32(info->data, parameter_die + 1u) != origin_die + 27u) {
        fprintf(stderr,
                "inline parameter DIE mismatch: path=%s op=0x%02x call=%s "
                "parameter=%llu origin=%llu tag=%u ref=%u expected-ref=%u\n",
                path, expected_operation, call_site_name,
                (unsigned long long)parameter_die,
                (unsigned long long)origin_die,
                parameter_die < info->size ? info->data[parameter_die] : 0u,
                parameter_die + 5u <= info->size
                    ? read_u32(info->data, parameter_die + 1u) : 0u,
                (unsigned int)(origin_die + 27u));
        assert(parameter_die + 5u < info->size &&
               info->data[parameter_die] == 67u &&
               read_u32(info->data, parameter_die + 1u) == origin_die + 27u);
    }
    expression_offset = parameter_die + 5u;
    expression_size = read_uleb(
        info->data, info->size, &expression_offset);
    expression_start = expression_offset;
    expression_end = expression_offset + expression_size;
    assert(expression_end <= info->size && expression_size > 0u);
    if (architecture == ARCH_X86 && is_unsigned && value_size == 8u &&
        !divisor_source_name && expected_constant > 0 &&
        expected_constant <= INT64_MAX &&
        (expected_operation == 0x1bu || expected_operation == 0x1du ||
         (expected_operation >= 0xdau && expected_operation <= 0xdfu))) {
        verify_inline_i686_u64_constant_division_expression(
            info->data, expression_end, expression_start, info, strings,
            locations, source_name, (uint64_t)expected_constant,
            expected_operation == 0x1du || expected_operation == 0xdbu ||
            expected_operation == 0xddu || expected_operation == 0xdfu);
        objfile_free(object);
        return;
    }
    if (wide_integer_comparison &&
        (architecture == ARCH_X86 || narrow_wide_comparison)) {
        static const uint64_t signed_values[] = {
            UINT64_C(0x8000000000000000), UINT64_MAX, 0u, 1u,
            UINT64_C(0xfffffffffffffff8), UINT64_C(0xfffffffffffffff9),
            6u, 7u,
            UINT64_C(0x00000000ffffffff), UINT64_C(0x00000001ffffffff),
            UINT64_C(0x0000000100000000),
            UINT64_C(0x7fffffffffffffff)
        };
        static const uint64_t unsigned_values[] = {
            0u, 1u, UINT64_C(0x7fffffff), UINT64_C(0xffffffff),
            UINT64_C(0x00000001ffffffff),
            UINT64_C(0x7fffffffffffffff), UINT64_C(0x8000000000000000),
            UINT64_C(0x8000000000000001), UINT64_MAX
        };
        assert(expected_operation >= 0x29u && expected_operation <= 0x2eu &&
               value_size == 8u && comparison_value_size == 8u &&
               find_variable_location(
                   info, strings, locations, source_name, architecture,
                   &has_variable_location, &variable_is_location_list,
                   &variable_frame_offset) && has_variable_location);
        frame_offset = variable_frame_offset;
        const uint64_t* values = is_unsigned
            ? unsigned_values : signed_values;
        size_t value_count = is_unsigned
            ? sizeof(unsigned_values) / sizeof(unsigned_values[0])
            : sizeof(signed_values) / sizeof(signed_values[0]);
        const char* secondary_source =
            inline_integer_comparison_secondary_source(source_name);
        int64_t secondary_frame_offset = 0;
        uint8_t secondary_size =
            inline_integer_comparison_secondary_size(source_name);
        if (secondary_source) assert(secondary_size > 0u);
        for (size_t index = 0u; index < value_count; ++index) {
            if (signed_right_shift_wide_comparison) {
                int64_t signed_value = verify_inline_integer_signed_bits(
                    values[index], 64u);
                uint64_t actual =
                    evaluate_inline_integer_comparison_expression(
                        info->data, expression_end, expression_start,
                        architecture, frame_offset, value_size, values[index]);
                uint64_t shifted = (uint64_t)signed_value ^
                    (uint64_t)(signed_value >> 1u) ^
                    (uint64_t)(signed_value >> 31u) ^
                    (uint64_t)(signed_value >> 32u) ^
                    (uint64_t)(signed_value >> 33u) ^
                    (uint64_t)(signed_value >> 63u);
                bool compared = verify_inline_integer_comparison_value(
                    shifted, (uint64_t)expected_constant, false, 64u,
                    expected_operation);
                assert(actual == !compared);
            } else if (computed_wide_logical_not) {
                uint64_t actual =
                    evaluate_inline_integer_comparison_expression(
                        info->data, expression_end, expression_start,
                        architecture, frame_offset, value_size, values[index]);
                assert(actual == (values[index] >> 33u ? 0u : 1u));
            } else if (shift_left_wide_comparison) {
                uint64_t actual =
                    evaluate_inline_integer_comparison_expression(
                        info->data, expression_end, expression_start,
                        architecture, frame_offset, value_size, values[index]);
                uint64_t shifted = values[index] ^
                    (values[index] << 31u) ^
                    (values[index] << 32u) ^
                    (values[index] << 33u) ^
                    (values[index] >> 0u) ^
                    (values[index] >> 1u) ^
                    (values[index] >> 31u) ^
                    (values[index] >> 32u) ^
                    (values[index] >> 33u) ^
                    (values[index] >> 63u);
                bool compared = verify_inline_integer_comparison_value(
                    shifted, (uint64_t)expected_constant,
                    is_unsigned, 64u, expected_operation);
                assert(actual == !compared);
            } else if (bitnot_wide_comparison) {
                uint64_t actual =
                    evaluate_inline_integer_comparison_expression(
                        info->data, expression_end, expression_start,
                        architecture, frame_offset, value_size, values[index]);
                bool compared = verify_inline_integer_comparison_value(
                    ~values[index], (uint64_t)expected_constant, is_unsigned,
                    64u, expected_operation);
                assert(actual == !compared);
            } else if (nested_add_bitxor_wide_comparison ||
                       nested_sub_bitxor_wide_comparison) {
                int64_t signed_value = verify_inline_integer_signed_bits(
                    values[index], 64u);
                if (nested_add_bitxor_wide_comparison &&
                    signed_value == INT64_MAX) continue;
                if (nested_sub_bitxor_wide_comparison &&
                    signed_value == INT64_MIN) continue;
                int64_t first = nested_add_bitxor_wide_comparison
                    ? signed_value + 1 : signed_value - 1;
                uint64_t bitwise = (uint64_t)first ^
                    UINT64_C(0x123456789abcdef0);
                int64_t bitwise_signed =
                    verify_inline_integer_signed_bits(bitwise, 64u);
                if ((nested_add_bitxor_wide_comparison &&
                     bitwise_signed > INT64_MAX - 2) ||
                    (nested_sub_bitxor_wide_comparison &&
                     bitwise_signed < INT64_MIN + 2)) continue;
                uint64_t evaluated = nested_add_bitxor_wide_comparison
                    ? bitwise + 2u : bitwise - 2u;
                uint64_t actual =
                    evaluate_inline_integer_comparison_expression(
                        info->data, expression_end, expression_start,
                        architecture, frame_offset, value_size, values[index]);
                bool compared = verify_inline_integer_comparison_value(
                    evaluated, (uint64_t)expected_constant, false, 64u,
                    expected_operation);
                assert(actual == !compared);
            } else if (wide_multiply_constant_comparison) {
                bool add_before_multiply = strcmp(
                    source_name,
                    "debug_inline_signed_long_long_nested_add_mul_wide_constant_compare_entry_source") == 0;
                int64_t multiplier = strcmp(
                    source_name,
                    "debug_inline_signed_long_long_mul_small_compare_entry_source") == 0
                    ? INT64_C(3) : INT64_C(0x123456789abcdef0);
                int64_t signed_value = verify_inline_integer_signed_bits(
                    values[index], 64u);
                if (add_before_multiply) {
                    if (signed_value == INT64_MAX) continue;
                    ++signed_value;
                }
                if (signed_value > INT64_MAX / multiplier ||
                    signed_value < INT64_MIN / multiplier) continue;
                uint64_t evaluated =
                    (uint64_t)(signed_value * multiplier);
                uint64_t actual =
                    evaluate_inline_integer_comparison_expression(
                        info->data, expression_end, expression_start,
                        architecture, frame_offset, value_size, values[index]);
                bool compared = verify_inline_integer_comparison_value(
                    evaluated, (uint64_t)expected_constant, false, 64u,
                    expected_operation);
                assert(actual == !compared);
            } else if (add_one_wide_comparison || sub_one_wide_comparison) {
                if (values[index] == (uint64_t)INT64_MAX) continue;
                if (sub_one_wide_comparison &&
                    values[index] == UINT64_C(0x8000000000000000)) continue;
                uint64_t evaluated_left = add_one_wide_comparison
                    ? values[index] + 1u : values[index] - 1u;
                uint64_t actual =
                    evaluate_inline_integer_comparison_expression(
                        info->data, expression_end, expression_start,
                        architecture, frame_offset, value_size, values[index]);
                bool compared = verify_inline_integer_comparison_value(
                    evaluated_left, (uint64_t)expected_constant, is_unsigned,
                    64u, expected_operation);
                assert(actual == !compared);
            } else if (secondary_source) {
                unsigned int secondary_bits = (unsigned int)secondary_size * 8u;
                uint64_t secondary_sign = UINT64_C(1) << (secondary_bits - 1u);
                uint64_t secondary_max = secondary_bits == 64u
                    ? UINT64_MAX
                    : (UINT64_C(1) << secondary_bits) - 1u;
                uint64_t signed_rhs_values[] = {
                    secondary_sign, secondary_max, 0u, 1u,
                    secondary_sign - 1u
                };
                uint64_t unsigned_rhs_values[] = {
                    0u, 1u, secondary_sign - 1u, secondary_sign,
                    secondary_max
                };
                uint64_t bool_rhs_values[] = {0u, 1u};
                bool rhs_is_unsigned =
                    inline_integer_comparison_secondary_is_unsigned(
                        source_name);
                bool rhs_is_bool =
                    inline_integer_comparison_secondary_is_bool(source_name);
                const uint64_t* secondary_values = rhs_is_bool
                    ? bool_rhs_values
                    : rhs_is_unsigned ? unsigned_rhs_values
                                      : signed_rhs_values;
                size_t secondary_count = rhs_is_bool
                    ? sizeof(bool_rhs_values) / sizeof(bool_rhs_values[0])
                    : sizeof(signed_rhs_values) /
                          sizeof(signed_rhs_values[0]);
                assert(find_variable_location(
                    info, strings, locations, secondary_source, architecture,
                    &has_variable_location, &variable_is_location_list,
                    &secondary_frame_offset) && has_variable_location &&
                       secondary_size > 0u);
                for (size_t secondary_index = 0u;
                     secondary_index < secondary_count; ++secondary_index) {
                    uint64_t rhs_value = secondary_values[secondary_index];
                    uint64_t comparison_rhs = rhs_is_unsigned
                        ? rhs_value
                        : (uint64_t)verify_inline_integer_signed_bits(
                              rhs_value, secondary_bits);
                    int64_t signed_left = verify_inline_integer_signed_bits(
                        values[index], 64u);
                    int64_t signed_right = verify_inline_integer_signed_bits(
                        rhs_value, secondary_bits);
                    if (add_secondary_wide_comparison &&
                        ((signed_right > 0 &&
                          signed_left > INT64_MAX - signed_right) ||
                         (signed_right < 0 &&
                          signed_left < INT64_MIN - signed_right))) {
                        continue;
                    }
                    if (sub_secondary_wide_comparison &&
                        ((signed_right > 0 &&
                          signed_left < INT64_MIN + signed_right) ||
                         (signed_right < 0 &&
                          signed_left > INT64_MAX + signed_right))) {
                        continue;
                    }
                    uint64_t actual =
                        evaluate_inline_integer_comparison_expression_with_secondary(
                            info->data, expression_end, expression_start,
                            architecture, frame_offset, value_size,
                            values[index], secondary_frame_offset,
                            secondary_size,
                            rhs_value);
                    bool is_wide_derived_expression =
                        add_secondary_wide_comparison ||
                        add_secondary_unsigned_wide_comparison ||
                        sub_secondary_wide_comparison ||
                        sub_secondary_unsigned_wide_comparison ||
                        bitand_secondary_comparison ||
                        bitor_secondary_comparison ||
                        bitxor_secondary_comparison ||
                        nested_bitwise_comparison ||
                        negated_bitwise_comparison;
                    uint64_t expected_left =
                        add_secondary_wide_comparison ||
                                add_secondary_unsigned_wide_comparison
                            ? values[index] + rhs_value
                            : sub_secondary_wide_comparison ||
                                      sub_secondary_unsigned_wide_comparison
                                ? values[index] - rhs_value
                                : bitand_secondary_comparison
                                    ? values[index] & rhs_value
                                    : bitor_secondary_comparison
                                        ? values[index] | rhs_value
                                        : bitxor_secondary_comparison
                                            ? values[index] ^ rhs_value
                                            : nested_bitwise_comparison
                                                ? ((values[index] & rhs_value) |
                                                   values[index]) ^ rhs_value
                                                : negated_bitwise_comparison
                                                    ? 0u -
                                                        (values[index] &
                                                         rhs_value)
                                                    : values[index];
                    if (negated_bitwise_comparison &&
                        (values[index] & rhs_value) ==
                            UINT64_C(0x8000000000000000)) {
                        continue;
                    }
                    bool compared = verify_inline_integer_comparison_value(
                        expected_left,
                        is_wide_derived_expression
                            ? (uint64_t)expected_constant : comparison_rhs,
                        is_unsigned, 64u, expected_operation);
                    assert(actual == !compared);
                }
            } else {
                uint64_t actual =
                    evaluate_inline_integer_comparison_expression(
                        info->data, expression_end, expression_start,
                        architecture, frame_offset, value_size, values[index]);
                bool compared = verify_inline_integer_comparison_value(
                    values[index], (uint64_t)expected_constant, is_unsigned,
                    64u, expected_operation);
                assert(actual == !compared);
            }
        }
        objfile_free(object);
        return;
    }
    assert(info->data[expression_offset++] ==
           (architecture == ARCH_X64 ? 0x76u : 0x75u));
    frame_offset = read_sleb(info->data, expression_end, &expression_offset);
    if (comparison_entry) {
        uint64_t offset = expression_start;
        bool full_width_unsigned_ordering = comparison_is_unsigned &&
            comparison_value_size == address_size &&
            (expected_operation == 0x2au || expected_operation == 0x2bu ||
             expected_operation == 0x2cu || expected_operation == 0x2du);
        uint64_t sign_bit = UINT64_C(1) << (address_size * 8u - 1u);
        assert((promoted_enum_comparison || enum_to_enum_comparison ||
                signed_char_enum_conversion_comparison
                    ? comparison_value_size == 4u
                    : ((wide_integer_comparison ||
                        promoted_unsigned_short_enum_comparison ||
                        mixed_rank_comparison || enum_rank_comparison ||
                        unsigned_long_conversion_comparison)
                           ? comparison_value_size == address_size
                           : value_size == 4u)) &&
               (expected_operation >= 0x29u && expected_operation <= 0x2eu));
        verify_inline_expression_frame_for_architecture(
            info->data, expression_end, &offset, architecture, frame_offset,
            enum_rank_comparison ? 4u :
            (promoted_enum_comparison || enum_to_enum_comparison ||
                 signed_char_enum_conversion_comparison ||
                 promoted_unsigned_short_enum_comparison)
                ? value_size : comparison_value_size);
        if (mixed_rank_comparison) {
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                false, comparison_value_size);
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                comparison_is_unsigned, comparison_value_size);
        } else if (enum_rank_comparison) {
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture, true, 4u);
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                comparison_is_unsigned, comparison_value_size);
        } else if (unsigned_long_conversion_comparison) {
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                comparison_is_unsigned, comparison_value_size);
        } else if (promoted_enum_comparison) {
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                true, value_size);
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                false, comparison_value_size);
        } else if (promoted_unsigned_short_enum_comparison) {
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                true, value_size);
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                false, comparison_value_size);
        } else if (enum_to_enum_comparison) {
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                true, value_size);
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                false, comparison_value_size);
        } else if (signed_char_enum_conversion_comparison) {
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                false, value_size);
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                true, comparison_value_size);
        } else if (unsigned_integer_promotion_comparison) {
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                false, value_size);
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                true, 2u);
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                true, value_size);
        } else if (integer_promotion_comparison) {
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                false, value_size);
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                false, 2u);
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                false, value_size);
        } else if (mixed_conversion_comparison) {
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                false, value_size);
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                true, value_size);
        } else {
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                is_unsigned, value_size);
        }
        if (full_width_unsigned_ordering) {
            verify_inline_expression_constu(
                info->data, expression_end, &offset, sign_bit);
            verify_inline_expression_byte(info->data, expression_end,
                                          &offset, 0x27u);
        }
        if (enum_rank_comparison) {
            verify_inline_expression_byte(
                info->data, expression_end, &offset, 0x11u);
            assert(read_sleb(info->data, expression_end, &offset) ==
                   expected_constant);
        } else if (unsigned_long_conversion_comparison) {
            verify_inline_expression_byte(
                info->data, expression_end, &offset, 0x11u);
            assert(read_sleb(info->data, expression_end, &offset) ==
                   expected_constant);
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                false, 4u);
        } else if (promoted_enum_comparison) {
            verify_inline_expression_byte(
                info->data, expression_end, &offset, 0x11u);
            assert(read_sleb(info->data, expression_end, &offset) ==
                   expected_constant);
        } else if (promoted_unsigned_short_enum_comparison) {
            verify_inline_expression_byte(
                info->data, expression_end, &offset, 0x11u);
            assert(read_sleb(info->data, expression_end, &offset) ==
                   expected_constant);
        } else if (enum_to_enum_comparison) {
            verify_inline_expression_byte(
                info->data, expression_end, &offset, 0x11u);
            assert(read_sleb(info->data, expression_end, &offset) ==
                   expected_constant);
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &offset, architecture,
                false, 1u);
        } else if (comparison_is_unsigned || mixed_rank_comparison) {
            verify_inline_expression_constu(
                info->data, expression_end, &offset,
                (uint64_t)expected_constant);
        } else {
            verify_inline_expression_byte(info->data, expression_end,
                                          &offset, 0x11u);
            assert(read_sleb(info->data, expression_end, &offset) ==
                   expected_constant);
        }
        if (mixed_rank_comparison && architecture == ARCH_X64) {
            verify_inline_expression_constu(
                info->data, expression_end, &offset, UINT32_MAX);
            verify_inline_expression_byte(
                info->data, expression_end, &offset, 0x1au);
        }
        verify_inline_integer_expression_normalization(
            info->data, expression_end, &offset, architecture,
            comparison_is_unsigned, comparison_value_size);
        if (full_width_unsigned_ordering) {
            verify_inline_expression_constu(
                info->data, expression_end, &offset, sign_bit);
            verify_inline_expression_byte(info->data, expression_end,
                                          &offset, 0x27u);
        }
        verify_inline_expression_byte(info->data, expression_end, &offset,
                                      expected_operation);
        verify_inline_integer_expression_normalization(
            info->data, expression_end, &offset, architecture, false, 4u);
        verify_inline_expression_constu(
            info->data, expression_end, &offset, 0u);
        verify_inline_expression_byte(info->data, expression_end, &offset,
                                      0x29u);
        verify_inline_integer_expression_normalization(
            info->data, expression_end, &offset, architecture, false, 4u);
        verify_inline_expression_byte(info->data, expression_end, &offset,
                                      0x9fu);
        assert(offset == expression_end);
        assert(find_variable_location(
            info, strings, locations, source_name, architecture,
            &has_variable_location, &variable_is_location_list,
            &variable_frame_offset));
        assert(has_variable_location && variable_frame_offset == frame_offset);
        if (wide_integer_comparison) {
            static const uint64_t signed_values[] = {
                UINT64_C(0x8000000000000000), UINT64_MAX, 0u, 1u,
                UINT64_C(0x7fffffffffffffff)
            };
            static const uint64_t unsigned_values[] = {
                0u, 1u, UINT64_C(0x7fffffff), UINT64_C(0xffffffff),
                UINT64_C(0x7fffffffffffffff), UINT64_C(0x8000000000000000),
                UINT64_C(0x8000000000000001), UINT64_MAX
            };
            const uint64_t* values = is_unsigned
                ? unsigned_values : signed_values;
            size_t value_count = is_unsigned
                ? sizeof(unsigned_values) / sizeof(unsigned_values[0])
                : sizeof(signed_values) / sizeof(signed_values[0]);
            for (size_t index = 0u; index < value_count; ++index) {
                uint64_t actual =
                    evaluate_inline_integer_comparison_expression(
                        info->data, expression_end, expression_start,
                        architecture, frame_offset, value_size, values[index]);
                bool compared = verify_inline_integer_comparison_value(
                    values[index], (uint64_t)expected_constant, is_unsigned,
                    64u, expected_operation);
                assert(actual == !compared);
            }
        } else if (enum_rank_comparison) {
            static const uint32_t values[] = {
                0u, 1u, UINT32_C(0x7fffffff), UINT32_C(0x80000000),
                UINT32_C(0xfffffffe), UINT32_MAX
            };
            for (size_t index = 0u;
                 index < sizeof(values) / sizeof(values[0]); ++index) {
                uint64_t actual =
                    evaluate_inline_integer_comparison_expression(
                        info->data, expression_end, expression_start,
                        architecture, frame_offset, 4u, values[index]);
                uint64_t expected = architecture == ARCH_X64 ||
                    values[index] == UINT32_MAX;
                assert(actual == expected);
            }
        } else if (unsigned_long_conversion_comparison) {
            static const uint64_t values32[] = {
                0u, 1u, UINT64_C(0x7fffffff), UINT64_C(0x80000000),
                UINT64_C(0xfffffffe), UINT32_MAX
            };
            static const uint64_t values64[] = {
                0u, 1u, UINT64_C(0xffffffff), UINT64_C(0x100000000),
                UINT64_C(0x7fffffffffffffff), UINT64_C(0x8000000000000000),
                UINT64_MAX - 1u, UINT64_MAX
            };
            const uint64_t* values = architecture == ARCH_X64
                ? values64 : values32;
            size_t value_count = architecture == ARCH_X64
                ? sizeof(values64) / sizeof(values64[0])
                : sizeof(values32) / sizeof(values32[0]);
            uint64_t maximum = architecture == ARCH_X64
                ? UINT64_MAX : UINT32_MAX;
            for (size_t index = 0u; index < value_count; ++index) {
                uint64_t actual =
                    evaluate_inline_integer_comparison_expression(
                        info->data, expression_end, expression_start,
                        architecture, frame_offset,
                        comparison_value_size, values[index]);
                assert(actual == (values[index] == maximum));
            }
        } else if (promoted_enum_comparison) {
            static const uint64_t values[] = {0u, 1u, 127u, 128u, 254u, 255u};
            for (size_t index = 0u;
                 index < sizeof(values) / sizeof(values[0]); ++index) {
                uint64_t actual =
                    evaluate_inline_integer_comparison_expression(
                        info->data, expression_end, expression_start,
                        architecture, frame_offset, value_size, values[index]);
                assert(actual == 1u);
            }
        } else if (promoted_unsigned_short_enum_comparison) {
            static const uint64_t values[] = {
                0u, 1u, 0x7fffu, 0x8000u, 0xfffeu, 0xffffu
            };
            for (size_t index = 0u;
                 index < sizeof(values) / sizeof(values[0]); ++index) {
                uint64_t actual =
                    evaluate_inline_integer_comparison_expression(
                        info->data, expression_end, expression_start,
                        architecture, frame_offset, value_size, values[index]);
                assert(actual == 1u);
            }
        } else if (enum_to_enum_comparison) {
            static const uint64_t values[] = {0u, 1u, 127u, 128u, 254u, 255u};
            for (size_t index = 0u;
                 index < sizeof(values) / sizeof(values[0]); ++index) {
                uint64_t actual =
                    evaluate_inline_integer_comparison_expression(
                        info->data, expression_end, expression_start,
                        architecture, frame_offset, value_size, values[index]);
                assert(actual == 1u);
            }
        } else if (signed_char_enum_conversion_comparison) {
            static const uint64_t values[] = {
                0x80u, 0xffu, 0u, 1u, 0x7fu
            };
            for (size_t index = 0u;
                 index < sizeof(values) / sizeof(values[0]); ++index) {
                uint64_t actual =
                    evaluate_inline_integer_comparison_expression(
                        info->data, expression_end, expression_start,
                        architecture, frame_offset, value_size, values[index]);
                assert(actual == ((values[index] & 0x80u) != 0u));
            }
        }
        objfile_free(object);
        return;
    }
    if (expected_operation == 0x29u && architecture == ARCH_X86 &&
        value_size == 8u) {
        static const uint64_t values[] = {
            0u, 1u, UINT64_C(0xffffffff), UINT64_C(0x100000000),
            UINT64_C(0x8000000000000000), UINT64_MAX
        };
        uint64_t offset = expression_start;
        assert(!is_unsigned && frame_offset <= INT64_MAX - 4);
        verify_inline_expression_frame_u32(
            info->data, expression_end, &offset, frame_offset);
        verify_inline_expression_frame_u32(
            info->data, expression_end, &offset, frame_offset + 4);
        verify_inline_expression_byte(info->data, expression_end, &offset, 0x21u);
        verify_inline_expression_constu(info->data, expression_end, &offset, 0u);
        verify_inline_expression_byte(info->data, expression_end, &offset, 0x29u);
        verify_inline_integer_expression_normalization(
            info->data, expression_end, &offset, architecture, false, 4u);
        verify_inline_expression_byte(info->data, expression_end, &offset, 0x9fu);
        assert(offset == expression_end);
        assert(find_variable_location(
            info, strings, locations, source_name, architecture,
            &has_variable_location, &variable_is_location_list,
            &variable_frame_offset));
        assert(has_variable_location && variable_frame_offset == frame_offset);
        for (size_t index = 0u; index < sizeof(values) / sizeof(values[0]);
             ++index) {
            uint32_t low = (uint32_t)values[index];
            uint32_t high = (uint32_t)(values[index] >> 32u);
            assert(((low | high) == 0u) == (values[index] == 0u));
        }
        objfile_free(object);
        return;
    }
    if (expected_operation == 0xfeu) {
        assert(is_unsigned && value_size > address_size &&
               expression_offset == expression_end);
        assert(find_variable_location(
            info, strings, locations, source_name,
            architecture, &has_variable_location, &variable_is_location_list,
            &variable_frame_offset));
        assert(has_variable_location && variable_frame_offset == frame_offset);
        objfile_free(object);
        return;
    }
    if (expected_operation == 0xfdu || expected_operation == 0xe1u) {
        bool expected_unsigned = expected_operation == 0xfdu;
        assert(architecture == ARCH_X86 && is_unsigned == expected_unsigned &&
               value_size == 8u &&
               (!expected_unsigned || expected_constant >= 0));
        verify_inline_u64_add_constant_expression(
            info->data, expression_end, expression_start, info, strings,
            locations, source_name, frame_offset,
            (uint64_t)expected_constant, is_unsigned);
        objfile_free(object);
        return;
    }
    if (expected_operation == 0xfcu || expected_operation == 0xe0u) {
        bool expected_unsigned = expected_operation == 0xfcu;
        assert(architecture == ARCH_X86 && is_unsigned == expected_unsigned &&
               value_size == 8u &&
               (!expected_unsigned || expected_constant >= 0));
        verify_inline_u64_sub_constant_expression(
            info->data, expression_end, expression_start, info, strings,
            locations, source_name, frame_offset,
            (uint64_t)expected_constant, is_unsigned);
        objfile_free(object);
        return;
    }
    if (expected_operation == 0xfbu || expected_operation == 0xebu) {
        bool expected_unsigned = expected_operation == 0xfbu;
        assert(architecture == ARCH_X86 && is_unsigned == expected_unsigned &&
               value_size == 8u);
        verify_inline_u64_bitnot_expression(
            info->data, expression_end, expression_start, info, strings,
            locations, source_name, frame_offset, is_unsigned);
        objfile_free(object);
        return;
    }
    if (expected_operation == 0xfau || expected_operation == 0xedu) {
        bool expected_unsigned = expected_operation == 0xfau;
        assert(architecture == ARCH_X86 && is_unsigned == expected_unsigned &&
               value_size == 8u &&
               (!expected_unsigned || expected_constant >= 0));
        verify_inline_u64_mul_constant_expression(
            info->data, expression_end, expression_start, info, strings,
            locations, source_name, frame_offset,
            (uint64_t)expected_constant, is_unsigned);
        objfile_free(object);
        return;
    }
    if (expected_operation == 0xc0u || expected_operation == 0xc1u ||
        expected_operation == 0xc2u || expected_operation == 0xc3u ||
        expected_operation == 0xc4u ||
        (expected_operation >= 0xd2u && expected_operation <= 0xd9u)) {
        bool expected_unsigned = expected_operation == 0xc0u ||
            expected_operation == 0xc1u || expected_operation == 0xc2u ||
            expected_operation == 0xd2u ||
            expected_operation == 0xd3u || expected_operation == 0xd6u ||
            expected_operation == 0xd7u;
        assert(architecture == ARCH_X86 && is_unsigned == expected_unsigned &&
               value_size == 8u && divisor_source_name);
        verify_inline_u64_variable_shift_expression(
            info->data, expression_end, expression_start, info, strings,
            locations, source_name, divisor_source_name,
            expected_operation == 0xc0u || expected_operation == 0xc1u ||
                expected_operation == 0xc2u ||
                expected_operation == 0xc3u ||
                expected_operation == 0xc4u ||
                expected_operation == 0xd2u ||
                expected_operation == 0xd4u || expected_operation == 0xd6u ||
                expected_operation == 0xd8u,
            is_unsigned,
            expected_operation == 0xc0u || expected_operation == 0xc1u ||
                    expected_operation == 0xc4u
                ? 1u : 0u,
            expected_operation == 0xc1u || expected_operation == 0xd6u ||
                expected_operation == 0xd7u || expected_operation == 0xd8u ||
                expected_operation == 0xd9u || expected_operation == 0xc2u ||
                expected_operation == 0xc3u ||
                expected_operation == 0xc4u,
            expected_operation == 0xc2u || expected_operation == 0xc3u ||
                expected_operation == 0xc4u);
        objfile_free(object);
        return;
    }
    if (expected_operation == 0xd0u || expected_operation == 0xd1u ||
        expected_operation == 0xeeu || expected_operation == 0xefu) {
        bool has_shift_source_location;
        bool shift_source_is_location_list;
        int64_t shift_source_frame_offset;
        assert(architecture == ARCH_X86 && value_size == 8u &&
               expected_constant >= 0);
        assert(find_variable_location(
            info, strings, locations, source_name, architecture,
            &has_shift_source_location, &shift_source_is_location_list,
            &shift_source_frame_offset));
        assert(has_shift_source_location);
        verify_inline_u64_shift_constant_expression(
            info->data, expression_end, expression_start, info, strings,
            locations, source_name, shift_source_frame_offset, is_unsigned,
            (uint64_t)expected_constant,
            expected_operation == 0xd0u || expected_operation == 0xeeu);
        objfile_free(object);
        return;
    }
    if (expected_operation == 0xf9u || expected_operation == 0xecu) {
        bool expected_unsigned = expected_operation == 0xf9u;
        assert(architecture == ARCH_X86 && is_unsigned == expected_unsigned &&
               value_size == 8u);
        verify_inline_u64_negate_expression(
            info->data, expression_end, expression_start, info, strings,
            locations, source_name, frame_offset, is_unsigned);
        objfile_free(object);
        return;
    }
    if (expected_operation == 0xf8u || expected_operation == 0xf7u ||
        expected_operation == 0xe9u || expected_operation == 0xe8u) {
        bool expected_unsigned = expected_operation == 0xf8u ||
            expected_operation == 0xf7u;
        assert(architecture == ARCH_X86 && is_unsigned == expected_unsigned &&
               value_size == 8u && divisor_source_name &&
               expected_constant == 0);
        verify_inline_u64_arithmetic_two_variables_expression(
            info->data, expression_end, expression_start, info, strings,
            locations, source_name, divisor_source_name, frame_offset,
            is_unsigned,
            expected_operation == 0xf7u || expected_operation == 0xe8u);
        objfile_free(object);
        return;
    }
    if (expected_operation == 0xf6u || expected_operation == 0xf5u ||
        expected_operation == 0xf4u || expected_operation == 0xe7u ||
        expected_operation == 0xe6u || expected_operation == 0xe5u) {
        bool expected_unsigned = expected_operation == 0xf6u ||
            expected_operation == 0xf5u || expected_operation == 0xf4u;
        uint8_t operation = expected_operation == 0xf6u ||
            expected_operation == 0xe7u ? 0x1au
            : expected_operation == 0xf5u || expected_operation == 0xe6u
                ? 0x21u : 0x27u;
        assert(architecture == ARCH_X86 && is_unsigned == expected_unsigned &&
               value_size == 8u && divisor_source_name &&
               expected_constant == 0);
        verify_inline_u64_bitwise_two_variables_expression(
            info->data, expression_end, expression_start, info, strings,
            locations, source_name, divisor_source_name, frame_offset,
            operation);
        objfile_free(object);
        return;
    }
    if (expected_operation == 0xf3u || expected_operation == 0xe4u) {
        bool expected_unsigned = expected_operation == 0xf3u;
        assert(architecture == ARCH_X86 && is_unsigned == expected_unsigned &&
               value_size == 8u && divisor_source_name &&
               expected_constant == 0);
        verify_inline_u64_mul_two_variables_expression(
            info->data, expression_end, expression_start, info, strings,
            locations, source_name, divisor_source_name, frame_offset,
            is_unsigned);
        objfile_free(object);
        return;
    }
    if (architecture == ARCH_X86 && value_size == 8u &&
        ((is_unsigned &&
          (expected_operation == 0x1au || expected_operation == 0x21u ||
           expected_operation == 0x27u)) ||
         (!is_unsigned &&
          (expected_operation == 0xe2u || expected_operation == 0xe3u ||
           expected_operation == 0xeau)))) {
        uint8_t operation = expected_operation == 0xe2u ? 0x1au :
            expected_operation == 0xe3u ? 0x21u :
            expected_operation == 0xeau ? 0x27u : expected_operation;
        verify_inline_u64_bitwise_constant_expression(
            info->data, expression_end, expression_start, info, strings,
            locations, source_name, frame_offset,
            (uint64_t)expected_constant, operation, is_unsigned);
        objfile_free(object);
        return;
    }
    assert(expression_offset + 2u <= expression_end &&
           info->data[expression_offset++] == 0x94u &&
           info->data[expression_offset++] == value_size);
    verify_inline_integer_expression_normalization(
        info->data, expression_end, &expression_offset, architecture,
        is_unsigned, value_size);
    if (evaluate_full_width_unsigned) {
        uint64_t divisor = (uint64_t)expected_constant;
        if (divisor_source_name) {
            assert(find_variable_location(
                info, strings, locations, divisor_source_name,
                architecture, &has_divisor_variable_location,
                &divisor_is_location_list, &divisor_frame_offset));
            assert(has_divisor_variable_location);
        }
        verify_inline_unsigned_division_expression_values(
            info->data, expression_end, expression_offset, architecture,
            expected_operation == 0x1du, divisor, value_size, frame_offset,
            divisor_frame_offset, divisor_source_name != NULL);
        assert(find_variable_location(
            info, strings, locations, source_name,
            architecture, &has_variable_location, &variable_is_location_list,
            &variable_frame_offset));
        assert(has_variable_location && variable_frame_offset == frame_offset);
        objfile_free(object);
        return;
    }
    /* 0x1d is a test-case marker: % is verified as a full synthesized
       expression, never as the DW_OP_mod opcode itself. */
    if (expected_operation == 0x1du) {
        assert(expression_offset < expression_end &&
               info->data[expression_offset++] == 0x12u);
        for (size_t term = 0u; term < 2u; ++term) {
            if (is_unsigned) {
                assert(expression_offset < expression_end &&
                       info->data[expression_offset++] == 0x10u &&
                       read_uleb(info->data, expression_end,
                                 &expression_offset) ==
                           (uint64_t)expected_constant);
            } else {
                assert(expression_offset < expression_end &&
                       info->data[expression_offset++] == 0x11u &&
                       read_sleb(info->data, expression_end,
                                 &expression_offset) == expected_constant);
            }
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &expression_offset, architecture,
                is_unsigned, value_size);
            assert(expression_offset < expression_end &&
                   info->data[expression_offset++] ==
                       (term == 0u ? 0x1bu : 0x1eu));
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &expression_offset, architecture,
                is_unsigned, value_size);
        }
        assert(expression_offset < expression_end &&
               info->data[expression_offset++] == 0x1cu);
        verify_inline_integer_expression_normalization(
            info->data, expression_end, &expression_offset, architecture,
            is_unsigned, value_size);
    } else {
        if (expected_operation == 0x29u) {
            if (strcmp(source_name,
                       "debug_inline_nonconstant_integer_logical_not_equality_source") == 0) {
                verify_inline_expression_byte(
                    info->data, expression_end, &expression_offset, 0x11u);
                assert(read_sleb(info->data, expression_end,
                                 &expression_offset) == 0);
                verify_inline_integer_expression_normalization(
                    info->data, expression_end, &expression_offset,
                    architecture, false, 4u);
                verify_inline_expression_byte(
                    info->data, expression_end, &expression_offset, 0x29u);
                verify_inline_integer_expression_normalization(
                    info->data, expression_end, &expression_offset,
                    architecture, false, 4u);
            }
            verify_inline_expression_constu(
                info->data, expression_end, &expression_offset, 0u);
        } else if (expected_operation != 0x1fu &&
                   expected_operation != 0x20u) {
            if (is_unsigned) {
                assert(expression_offset < expression_end &&
                       info->data[expression_offset++] == 0x10u &&
                       read_uleb(info->data, expression_end,
                                 &expression_offset) ==
                           (uint64_t)expected_constant);
            } else {
                assert(expression_offset < expression_end &&
                       info->data[expression_offset++] == 0x11u &&
                       read_sleb(info->data, expression_end,
                                 &expression_offset) == expected_constant);
            }
            verify_inline_integer_expression_normalization(
                info->data, expression_end, &expression_offset, architecture,
                is_unsigned, value_size);
        }
        assert(expression_offset < expression_end &&
               info->data[expression_offset++] == expected_operation);
        verify_inline_integer_expression_normalization(
            info->data, expression_end, &expression_offset, architecture,
            is_unsigned, expected_operation == 0x29u ? 4u : value_size);
    }
    assert(expression_offset + 1u == expression_end &&
           info->data[expression_offset] == 0x9fu);
    assert(find_variable_location(
        info, strings, locations, source_name,
        architecture, &has_variable_location, &variable_is_location_list,
        &variable_frame_offset));
    assert(has_variable_location && variable_frame_offset == frame_offset);
    objfile_free(object);
}

typedef struct VerifyInlineIntegerExpressionCase {
    const char* inline_name;
    const char* source_name;
    bool is_unsigned;
    uint8_t value_size;
    uint8_t operation;
    int64_t constant;
    const char* divisor_source_name;
    bool evaluate_full_width_unsigned;
} VerifyInlineIntegerExpressionCase;

static void verify_inline_integer_expression_cases(
    const char* path, uint16_t architecture) {
    static const VerifyInlineIntegerExpressionCase cases[] = {
        {"debug_nonconstant_integer_parameter_inline",
         "debug_inline_nonconstant_integer_source", false, 4u, 0x22u, 5,
         NULL, false},
        {"debug_nonconstant_integer_subtract_parameter_inline",
         "debug_inline_nonconstant_integer_source", false, 4u, 0x1cu, 3,
         NULL, false},
        {"debug_nonconstant_integer_multiply_parameter_inline",
         "debug_inline_nonconstant_integer_source", false, 4u, 0x1eu, 3,
         NULL, false},
        {"debug_nonconstant_integer_divide_parameter_inline",
         "debug_inline_nonconstant_integer_source", false, 4u, 0x1bu, 3,
         NULL, false},
        {"debug_nonconstant_integer_remainder_parameter_inline",
         "debug_inline_nonconstant_integer_source", false, 4u, 0x1du, 5,
         NULL, false},
        {"debug_nonconstant_integer_negate_parameter_inline",
         "debug_inline_nonconstant_integer_source", false, 4u, 0x1fu, 0,
         NULL, false},
        {"debug_nonconstant_integer_bitnot_parameter_inline",
         "debug_inline_nonconstant_integer_source", false, 4u, 0x20u, 0,
         NULL, false},
        {"debug_nonconstant_integer_parameter_inline",
         "debug_inline_nonconstant_integer_logical_not_source", false, 4u,
         0x29u, 0, NULL, false},
        {"debug_nonconstant_integer_parameter_inline",
         "debug_inline_nonconstant_integer_logical_not_wide_source", false,
         8u, 0x29u, 0, NULL, false},
        {"debug_nonconstant_integer_parameter_inline",
         "debug_inline_nonconstant_unsigned_integer_logical_not_wide_shift_source",
         true, 8u, 0x29u, 0, NULL, false},
        {"debug_nonconstant_integer_parameter_inline",
         "debug_inline_nonconstant_integer_logical_not_equality_source",
         false, 4u, 0x29u, 0, NULL, false},
        {"debug_nonconstant_integer_parameter_inline",
         "debug_inline_nonconstant_integer_compare_less_entry_source",
         false, 4u, 0x2du, 0, NULL, false},
        {"debug_nonconstant_integer_parameter_inline",
         "debug_inline_nonconstant_integer_compare_greater_entry_source",
         false, 4u, 0x2bu, 0, NULL, false},
        {"debug_nonconstant_integer_parameter_inline",
         "debug_inline_nonconstant_integer_compare_less_equal_entry_source",
         false, 4u, 0x2cu, 0, NULL, false},
        {"debug_nonconstant_integer_parameter_inline",
         "debug_inline_nonconstant_integer_compare_greater_equal_entry_source",
         false, 4u, 0x2au, 0, NULL, false},
        {"debug_nonconstant_integer_parameter_inline",
         "debug_inline_nonconstant_integer_compare_not_equal_entry_source",
         false, 4u, 0x2eu, 0, NULL, false},
        {"debug_nonconstant_integer_parameter_inline",
         "debug_inline_nonconstant_unsigned_integer_compare_less_entry_source",
         true, 4u, 0x2du, INT64_C(0x80000000), NULL, false},
        {"debug_nonconstant_integer_parameter_inline",
         "debug_inline_nonconstant_integer_compare_mixed_entry_source",
         true, 4u, 0x2du, INT64_C(0x80000000), NULL, false},
        {"debug_nonconstant_integer_parameter_inline",
         "debug_inline_nonconstant_integer_compare_implicit_entry_source",
         true, 4u, 0x2du, INT64_C(0x80000000), NULL, false},
        {"debug_nonconstant_integer_parameter_inline",
         "debug_inline_nonconstant_integer_compare_promoted_short_entry_source",
         false, 4u, 0x2du, 0, NULL, false},
        {"debug_nonconstant_integer_parameter_inline",
         "debug_inline_nonconstant_integer_compare_promoted_unsigned_short_entry_source",
         true, 4u, 0x2du, 0x8000, NULL, false},
        {"debug_nonconstant_integer_parameter_inline",
         "debug_inline_nonconstant_long_compare_unsigned_int_entry_source",
         false, 0u, 0x2du, INT64_C(0x80000000), NULL, false},
        {"debug_nonconstant_integer_parameter_inline",
         "debug_inline_nonconstant_unsigned_long_compare_signed_int_entry_source",
         true, 0u, 0x2du, -1, NULL, false},
        {"debug_nonconstant_integer_bitand_parameter_inline",
         "debug_inline_nonconstant_integer_source", false, 4u, 0x1au, 7,
         NULL, false},
        {"debug_nonconstant_integer_bitor_parameter_inline",
         "debug_inline_nonconstant_integer_source", false, 4u, 0x21u, 8,
         NULL, false},
        {"debug_nonconstant_integer_bitxor_parameter_inline",
         "debug_inline_nonconstant_integer_source", false, 4u, 0x27u, 15,
         NULL, false},
        {"debug_nonconstant_integer_lshift_parameter_inline",
         "debug_inline_nonconstant_integer_source", false, 4u, 0x24u, 2,
         NULL, false},
        {"debug_nonconstant_integer_rshift_parameter_inline",
         "debug_inline_nonconstant_integer_source", false, 4u, 0x26u, 1,
         NULL, false},
        {"debug_nonconstant_unsigned_integer_rshift_parameter_inline",
         "debug_inline_nonconstant_unsigned_integer_source", true, 4u,
         0x25u, 1, NULL, false},
        {"debug_nonconstant_unsigned_integer_divide_parameter_inline",
         "debug_inline_nonconstant_unsigned_integer_source", true, 4u,
         0x1bu, 3, NULL, true},
        {"debug_nonconstant_unsigned_integer_remainder_parameter_inline",
         "debug_inline_nonconstant_unsigned_integer_source", true, 4u,
         0x1du, 5, NULL, true},
        {"debug_nonconstant_unsigned_integer_high_divide_inline",
         "debug_inline_nonconstant_unsigned_integer_source", true, 4u,
         0x1bu, 0, "debug_inline_nonconstant_unsigned_high_divisor_source",
         true},
        {"debug_nonconstant_unsigned_integer_high_remainder_inline",
         "debug_inline_nonconstant_unsigned_integer_source", true, 4u,
         0x1du, 0, "debug_inline_nonconstant_unsigned_high_divisor_source",
         true},
        {"debug_nonconstant_unsigned_long_long_divide_inline",
         "debug_inline_nonconstant_unsigned_long_long_source", true, 8u,
         0x1bu, 3, NULL, true},
        {"debug_nonconstant_unsigned_long_long_remainder_inline",
         "debug_inline_nonconstant_unsigned_long_long_source", true, 8u,
         0x1du, 5, NULL, true},
        {"debug_nonconstant_unsigned_long_long_wide_divide_inline",
         "debug_inline_nonconstant_unsigned_long_long_wide_source", true, 8u,
         0xdau, 32769, NULL, true},
        {"debug_nonconstant_unsigned_long_long_wide_remainder_inline",
         "debug_inline_nonconstant_unsigned_long_long_wide_source", true, 8u,
         0xdbu, 65536, NULL, true},
        {"debug_nonconstant_unsigned_long_long_wider_divide_inline",
         "debug_inline_nonconstant_unsigned_long_long_wide_source", true, 8u,
         0xdcu, 65537, NULL, true},
        {"debug_nonconstant_unsigned_long_long_wider_remainder_inline",
         "debug_inline_nonconstant_unsigned_long_long_wide_source", true, 8u,
         0xddu, 131072, NULL, true},
        {"debug_nonconstant_unsigned_long_long_widest_divide_inline",
         "debug_inline_nonconstant_unsigned_long_long_wide_source", true, 8u,
         0xdeu, INT64_C(1073741825), NULL, true},
        {"debug_nonconstant_unsigned_long_long_widest_remainder_inline",
         "debug_inline_nonconstant_unsigned_long_long_wide_source", true, 8u,
         0xdfu, INT64_C(2147483647), NULL, true},
        {"debug_nonconstant_unsigned_long_long_u32_divide_inline",
         "debug_inline_nonconstant_unsigned_long_long_wide_source", true, 8u,
         0xdeu, INT64_C(2147483649), NULL, true},
        {"debug_nonconstant_unsigned_long_long_u32_remainder_inline",
         "debug_inline_nonconstant_unsigned_long_long_wide_source", true, 8u,
         0xdfu, INT64_C(4294967295), NULL, true},
        {"debug_nonconstant_unsigned_long_long_power_divide_inline",
         "debug_inline_nonconstant_unsigned_long_long_wide_source", true, 8u,
         0xdau, INT64_C(4294967296), NULL, true},
        {"debug_nonconstant_unsigned_long_long_power_remainder_inline",
         "debug_inline_nonconstant_unsigned_long_long_wide_source", true, 8u,
         0xdbu, INT64_C(4294967296), NULL, true},
        {"debug_nonconstant_unsigned_long_long_wide_power_divide_inline",
         "debug_inline_nonconstant_unsigned_long_long_wide_source", true, 8u,
         0xdau, INT64_C(1099511627776), NULL, true},
        {"debug_nonconstant_unsigned_long_long_wide_power_remainder_inline",
         "debug_inline_nonconstant_unsigned_long_long_wide_source", true, 8u,
         0xdbu, INT64_C(4611686018427387904), NULL, true},
        {"debug_nonconstant_unsigned_long_long_high_divide_inline",
         "debug_inline_nonconstant_unsigned_long_long_source", true, 8u,
         0x1bu, 0,
         "debug_inline_nonconstant_unsigned_long_long_high_divisor_source",
         true},
        {"debug_nonconstant_unsigned_long_long_high_remainder_inline",
         "debug_inline_nonconstant_unsigned_long_long_source", true, 8u,
         0x1du, 0,
         "debug_inline_nonconstant_unsigned_long_long_high_divisor_source",
         true},
        {"debug_nonconstant_unsigned_long_long_identity_inline",
         "debug_inline_nonconstant_unsigned_long_long_source", true, 8u,
         0xfeu, 0, NULL, false},
        {"debug_nonconstant_unsigned_long_long_add_inline",
         "debug_inline_nonconstant_unsigned_long_long_source", true, 8u,
         0xfdu, INT64_C(0x100000005), NULL, false},
        {"debug_nonconstant_unsigned_long_long_add_two_variables_inline",
         "debug_inline_nonconstant_unsigned_long_long_add_left_source",
         true, 8u,
         0xf8u, 0,
         "debug_inline_nonconstant_unsigned_long_long_rhs_source", false},
        {"debug_nonconstant_unsigned_long_long_sub_two_variables_inline",
         "debug_inline_nonconstant_unsigned_long_long_sub_left_source",
         true, 8u,
         0xf7u, 0,
         "debug_inline_nonconstant_unsigned_long_long_sub_rhs_source", false},
        {"debug_nonconstant_unsigned_long_long_and_two_variables_inline",
         "debug_inline_nonconstant_unsigned_long_long_and_left_source",
         true, 8u,
         0xf6u, 0,
         "debug_inline_nonconstant_unsigned_long_long_and_rhs_source", false},
        {"debug_nonconstant_unsigned_long_long_or_two_variables_inline",
         "debug_inline_nonconstant_unsigned_long_long_or_left_source",
         true, 8u,
         0xf5u, 0,
         "debug_inline_nonconstant_unsigned_long_long_or_rhs_source", false},
        {"debug_nonconstant_unsigned_long_long_xor_two_variables_inline",
         "debug_inline_nonconstant_unsigned_long_long_xor_left_source",
         true, 8u,
         0xf4u, 0,
         "debug_inline_nonconstant_unsigned_long_long_xor_rhs_source", false},
        {"debug_nonconstant_unsigned_long_long_mul_two_variables_inline",
         "debug_inline_nonconstant_unsigned_long_long_mul_left_source",
         true, 8u,
         0xf3u, 0,
         "debug_inline_nonconstant_unsigned_long_long_mul_rhs_source", false},
        {"debug_nonconstant_signed_long_long_add_two_variables_inline",
         "debug_inline_nonconstant_signed_long_long_add_left_source",
         false, 8u,
         0xe9u, 0,
         "debug_inline_nonconstant_signed_long_long_add_rhs_source", false},
        {"debug_nonconstant_signed_long_long_sub_two_variables_inline",
         "debug_inline_nonconstant_signed_long_long_sub_left_source",
         false, 8u,
         0xe8u, 0,
         "debug_inline_nonconstant_signed_long_long_sub_rhs_source", false},
        {"debug_nonconstant_signed_long_long_and_two_variables_inline",
         "debug_inline_nonconstant_signed_long_long_and_left_source",
         false, 8u,
         0xe7u, 0,
         "debug_inline_nonconstant_signed_long_long_and_rhs_source", false},
        {"debug_nonconstant_signed_long_long_or_two_variables_inline",
         "debug_inline_nonconstant_signed_long_long_or_left_source",
         false, 8u,
         0xe6u, 0,
         "debug_inline_nonconstant_signed_long_long_or_rhs_source", false},
        {"debug_nonconstant_signed_long_long_xor_two_variables_inline",
         "debug_inline_nonconstant_signed_long_long_xor_left_source",
         false, 8u,
         0xe5u, 0,
         "debug_inline_nonconstant_signed_long_long_xor_rhs_source", false},
        {"debug_nonconstant_signed_long_long_mul_two_variables_inline",
         "debug_inline_nonconstant_signed_long_long_mul_left_source",
         false, 8u,
         0xe4u, 0,
         "debug_inline_nonconstant_signed_long_long_mul_rhs_source", false},
        {"debug_nonconstant_signed_long_long_add_constant_inline",
         "debug_inline_nonconstant_signed_long_long_add_constant_source",
         false, 8u,
         0xe1u, -7,
         NULL, false},
        {"debug_nonconstant_signed_long_long_sub_constant_inline",
         "debug_inline_nonconstant_signed_long_long_sub_constant_source",
         false, 8u,
         0xe0u, 7,
         NULL, false},
        {"debug_nonconstant_signed_long_long_and_constant_inline",
         "debug_inline_nonconstant_signed_long_long_and_constant_source",
         false, 8u,
         0xe2u, -INT64_C(0x0f0ff0f00ff00ff0), NULL, false},
        {"debug_nonconstant_signed_long_long_or_constant_inline",
         "debug_inline_nonconstant_signed_long_long_or_constant_source",
         false, 8u,
         0xe3u, INT64_C(0x4000000100000001), NULL, false},
        {"debug_nonconstant_signed_long_long_xor_constant_inline",
         "debug_inline_nonconstant_signed_long_long_xor_constant_source",
         false, 8u,
         0xeau, -INT64_C(0x100000001), NULL, false},
        {"debug_nonconstant_signed_long_long_bitnot_inline",
         "debug_inline_nonconstant_signed_long_long_bitnot_source",
         false, 8u,
         0xebu, 0, NULL, false},
        {"debug_nonconstant_signed_long_long_negate_inline",
         "debug_inline_nonconstant_signed_long_long_negate_source",
         false, 8u,
         0xecu, 0, NULL, false},
        {"debug_nonconstant_signed_long_long_mul_constant_inline",
         "debug_inline_nonconstant_signed_long_long_mul_constant_source",
         false, 8u,
         0xedu, -3, NULL, false},
        {"debug_nonconstant_unsigned_long_long_multiply_inline",
         "debug_inline_nonconstant_unsigned_long_long_source", true, 8u,
         0xfau, INT64_C(0x0f0ff0f00ff00ff0), NULL, false},
        {"debug_nonconstant_unsigned_long_long_bitand_inline",
         "debug_inline_nonconstant_unsigned_long_long_source", true, 8u,
         0x1au, INT64_C(0x0f0ff0f00ff00ff0), NULL, false},
        {"debug_nonconstant_unsigned_long_long_bitor_inline",
         "debug_inline_nonconstant_unsigned_long_long_source", true, 8u,
         0x21u, INT64_C(0x4000000100000001), NULL, false},
        {"debug_nonconstant_unsigned_long_long_bitxor_inline",
         "debug_inline_nonconstant_unsigned_long_long_source", true, 8u,
         0x27u, INT64_MAX, NULL, false},
        {"debug_nonconstant_unsigned_long_long_sub_inline",
         "debug_inline_nonconstant_unsigned_long_long_source", true, 8u,
         0xfcu, INT64_C(0x100000005), NULL, false},
        {"debug_nonconstant_unsigned_long_long_bitnot_inline",
         "debug_inline_nonconstant_unsigned_long_long_source", true, 8u,
         0xfbu, 0, NULL, false},
        {"debug_nonconstant_unsigned_long_long_negate_inline",
         "debug_inline_nonconstant_unsigned_long_long_source", true, 8u,
         0xf9u, 0, NULL, false},
        {"debug_nonconstant_unsigned_long_long_lshift_inline",
         "debug_inline_nonconstant_unsigned_long_long_lshift_source", true, 8u,
         0xeeu, 1, NULL, false},
        {"debug_nonconstant_unsigned_long_long_rshift_inline",
         "debug_inline_nonconstant_unsigned_long_long_rshift_source", true, 8u,
         0xefu, 33, NULL, false},
        {"debug_nonconstant_signed_long_long_lshift_inline",
         "debug_inline_nonconstant_signed_long_long_lshift_source", false, 8u,
         0xd0u, 1, NULL, false},
        {"debug_nonconstant_signed_long_long_rshift_inline",
         "debug_inline_nonconstant_signed_long_long_rshift_source", false, 8u,
         0xd1u, 33, NULL, false},
        {"debug_nonconstant_unsigned_long_long_lshift_variable_inline",
         "debug_inline_nonconstant_unsigned_long_long_lshift_variable_source",
         true, 8u, 0xd2u, 0,
         "debug_inline_nonconstant_unsigned_long_long_lshift_variable_count",
         false},
        {"debug_nonconstant_unsigned_long_long_lshift_variable_inline",
         "debug_inline_nonconstant_unsigned_long_long_lshift_computed_count_source",
         true, 8u, 0xc0u, 0,
         "debug_inline_nonconstant_unsigned_long_long_lshift_computed_count_count",
         false},
        {"debug_nonconstant_unsigned_long_long_lshift_variable_inline",
         "debug_inline_nonconstant_unsigned_long_long_lshift_wide_computed_count_source",
         true, 8u, 0xc1u, 0,
         "debug_inline_nonconstant_unsigned_long_long_lshift_wide_computed_count_count",
         false},
        {"debug_nonconstant_unsigned_long_long_lshift_variable_inline",
         "debug_inline_nonconstant_unsigned_long_long_lshift_mod64_count_source",
         true, 8u, 0xc2u, 0,
         "debug_inline_nonconstant_unsigned_long_long_lshift_mod64_count_count",
         false},
        {"debug_nonconstant_signed_long_long_lshift_variable_inline",
         "debug_inline_nonconstant_signed_long_long_lshift_mod64_count_source",
         false, 8u, 0xc3u, 0,
         "debug_inline_nonconstant_signed_long_long_lshift_mod64_count_count",
         false},
        {"debug_nonconstant_signed_long_long_lshift_variable_inline",
         "debug_inline_nonconstant_signed_long_long_lshift_computed_mod64_count_source",
         false, 8u, 0xc4u, 1,
         "debug_inline_nonconstant_signed_long_long_lshift_computed_mod64_count_count",
         false},
        {"debug_nonconstant_unsigned_long_long_rshift_variable_inline",
         "debug_inline_nonconstant_unsigned_long_long_rshift_variable_source",
         true, 8u, 0xd3u, 0,
         "debug_inline_nonconstant_unsigned_long_long_rshift_variable_count",
         false},
        {"debug_nonconstant_signed_long_long_lshift_variable_inline",
         "debug_inline_nonconstant_signed_long_long_lshift_variable_source",
         false, 8u, 0xd4u, 0,
         "debug_inline_nonconstant_signed_long_long_lshift_variable_count",
         false},
        {"debug_nonconstant_signed_long_long_rshift_variable_inline",
         "debug_inline_nonconstant_signed_long_long_rshift_variable_source",
         false, 8u, 0xd5u, 0,
         "debug_inline_nonconstant_signed_long_long_rshift_variable_count",
         false},
        {"debug_nonconstant_unsigned_long_long_lshift_variable_inline",
         "debug_inline_nonconstant_unsigned_long_long_lshift_wide_count_source",
         true, 8u, 0xd6u, 0,
         "debug_inline_nonconstant_unsigned_long_long_lshift_wide_count_count",
         false},
        {"debug_nonconstant_unsigned_long_long_rshift_variable_inline",
         "debug_inline_nonconstant_unsigned_long_long_rshift_wide_count_source",
         true, 8u, 0xd7u, 0,
         "debug_inline_nonconstant_unsigned_long_long_rshift_wide_count_count",
         false},
        {"debug_nonconstant_signed_long_long_lshift_variable_inline",
         "debug_inline_nonconstant_signed_long_long_lshift_wide_count_source",
         false, 8u, 0xd8u, 0,
         "debug_inline_nonconstant_signed_long_long_lshift_wide_count_count",
         false},
        {"debug_nonconstant_signed_long_long_rshift_variable_inline",
         "debug_inline_nonconstant_signed_long_long_rshift_wide_count_source",
         false, 8u, 0xd9u, 0,
         "debug_inline_nonconstant_signed_long_long_rshift_wide_count_count",
         false}
    };
    uint8_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    for (size_t index = 0u; index < sizeof(cases) / sizeof(cases[0]);
         ++index) {
        bool i686_u64_bitwise = architecture == ARCH_X86 &&
            cases[index].value_size == 8u &&
            (cases[index].operation == 0x1au ||
             cases[index].operation == 0x21u ||
             cases[index].operation == 0x27u ||
             cases[index].operation == 0xe2u ||
             cases[index].operation == 0xe3u ||
             cases[index].operation == 0xeau);
        bool i686_u64_constant_divmod = architecture == ARCH_X86 &&
            cases[index].value_size == 8u && cases[index].is_unsigned &&
            !cases[index].divisor_source_name &&
            (cases[index].operation == 0x1bu ||
             cases[index].operation == 0x1du ||
             (cases[index].operation >= 0xdau &&
              cases[index].operation <= 0xdfu));
        if ((cases[index].operation == 0xe0u ||
             cases[index].operation == 0xe1u ||
             cases[index].operation == 0xe2u ||
             cases[index].operation == 0xe3u ||
             cases[index].operation == 0xeau ||
             cases[index].operation == 0xedu ||
             cases[index].operation == 0xebu ||
             cases[index].operation == 0xecu ||
             cases[index].operation == 0xc0u ||
             cases[index].operation == 0xc1u ||
             cases[index].operation == 0xc2u ||
             cases[index].operation == 0xc3u ||
             cases[index].operation == 0xc4u ||
             cases[index].operation == 0xd0u ||
             cases[index].operation == 0xd1u ||
             cases[index].operation == 0xd2u ||
             cases[index].operation == 0xd3u ||
             cases[index].operation == 0xd4u ||
             cases[index].operation == 0xd5u ||
             cases[index].operation == 0xd6u ||
             cases[index].operation == 0xd7u ||
             cases[index].operation == 0xd8u ||
             cases[index].operation == 0xd9u ||
             cases[index].operation == 0xeeu ||
             cases[index].operation == 0xefu ||
             cases[index].operation == 0xe4u ||
             cases[index].operation == 0xe5u ||
             cases[index].operation == 0xe6u ||
             cases[index].operation == 0xe7u ||
             cases[index].operation == 0xe8u ||
             cases[index].operation == 0xe9u ||
             cases[index].operation == 0xfeu ||
             cases[index].operation == 0xfdu ||
             cases[index].operation == 0xfcu ||
             cases[index].operation == 0xfbu ||
             cases[index].operation == 0xf3u ||
             cases[index].operation == 0xf4u ||
             cases[index].operation == 0xf5u ||
             cases[index].operation == 0xf6u ||
             cases[index].operation == 0xf7u ||
             cases[index].operation == 0xf8u ||
             cases[index].operation == 0xf9u ||
             cases[index].operation == 0xfau ||
             (cases[index].operation >= 0xdau &&
              cases[index].operation <= 0xdfu)) && architecture != ARCH_X86)
            continue;
        if (cases[index].value_size > address_size &&
            cases[index].operation != 0xe0u &&
            cases[index].operation != 0xe1u &&
            cases[index].operation != 0xe2u &&
            cases[index].operation != 0xe3u &&
            cases[index].operation != 0xeau &&
            cases[index].operation != 0xedu &&
            cases[index].operation != 0xebu &&
            cases[index].operation != 0xecu &&
            cases[index].operation != 0xc0u &&
            cases[index].operation != 0xc1u &&
            cases[index].operation != 0xc2u &&
            cases[index].operation != 0xc3u &&
            cases[index].operation != 0xc4u &&
            cases[index].operation != 0xd0u &&
            cases[index].operation != 0xd1u &&
            cases[index].operation != 0xd2u &&
            cases[index].operation != 0xd3u &&
            cases[index].operation != 0xd4u &&
            cases[index].operation != 0xd5u &&
            cases[index].operation != 0xd6u &&
            cases[index].operation != 0xd7u &&
            cases[index].operation != 0xd8u &&
            cases[index].operation != 0xd9u &&
            cases[index].operation != 0xeeu &&
            cases[index].operation != 0xefu &&
            cases[index].operation != 0xe4u &&
            cases[index].operation != 0xe5u &&
            cases[index].operation != 0xe6u &&
            cases[index].operation != 0xe7u &&
            cases[index].operation != 0xe8u &&
            cases[index].operation != 0xe9u &&
            cases[index].operation != 0xfeu &&
            cases[index].operation != 0xfdu &&
            cases[index].operation != 0xfcu &&
            cases[index].operation != 0xfbu &&
            cases[index].operation != 0xf3u &&
            cases[index].operation != 0xf4u &&
            cases[index].operation != 0xf5u &&
            cases[index].operation != 0xf6u &&
            cases[index].operation != 0xf7u &&
            cases[index].operation != 0xf8u &&
            cases[index].operation != 0xf9u &&
            cases[index].operation != 0xfau &&
            !(cases[index].operation >= 0xdau &&
              cases[index].operation <= 0xdfu) && !i686_u64_bitwise &&
            !i686_u64_constant_divmod) continue;
        verify_inline_integer_expression_parameter(
            path, architecture, cases[index].inline_name,
            cases[index].source_name, cases[index].is_unsigned,
            cases[index].operation, cases[index].value_size,
            cases[index].constant, cases[index].divisor_source_name,
            cases[index].evaluate_full_width_unsigned &&
                (cases[index].value_size == address_size ||
                 cases[index].divisor_source_name != NULL));
    }
    if (architecture == ARCH_X86) {
        verify_inline_integer_expression_parameter(
            path, architecture, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_nonconstant_integer_logical_not_wide_source",
            false, 0x29u, 8u, 0, NULL, false);
    }
}

int main(int argc, char** argv)
{
    const VerifyInlinePointerIndexTerm compound_pointer_indices[] = {
        {"debug_inline_automatic_compound_signed_index", 1u, false, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_compound_unsigned_index", 4u, true, 4u,
         true, 0u, 0u, false, 0u}
    };
    const VerifyInlinePointerIndexTerm computed_pointer_indices[] = {
        {"debug_inline_automatic_computed_signed_index", 1u, false, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_computed_int_index", 4u, false, 4u,
         false, 0u, 0u, false, 0u}
    };
    const VerifyInlinePointerIndexTerm nested_array_pointer_indices[] = {
        {"debug_inline_automatic_nested_row_index", 4u, false, 20u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_nested_column_index", 4u, false, 4u,
         false, 0u, 0u, false, 0u}
    };
    const VerifyInlinePointerIndexTerm nested_array_constant_row_index[] = {
        {"debug_inline_automatic_nested_constant_row_column", 4u, false,
         4u, false, 0u, 0u, false, 0u}
    };
    const VerifyInlinePointerIndexTerm
        nested_array_constant_column_index[] = {
            {"debug_inline_automatic_nested_constant_column_row", 4u,
             false, 20u, false, 0u, 0u, false, 0u}
        };
    const VerifyInlinePointerIndexTerm
        pointer_nested_array_indices[] = {
            {"debug_inline_automatic_pointer_nested_row_index", 4u,
             false, 20u, false, 0u, 0u, false, 0u},
            {"debug_inline_automatic_pointer_nested_column_index", 4u,
             false, 4u, false, 0u, 0u, false, 0u}
        };
    const VerifyInlinePointerIndexTerm
        pointer_nested_constant_row_index[] = {
            {"debug_inline_automatic_pointer_constant_row_column", 4u,
             false, 4u, false, 0u, 0u, false, 0u}
        };
    const VerifyInlinePointerIndexTerm
        pointer_nested_constant_column_index[] = {
            {"debug_inline_automatic_pointer_constant_column_row", 4u,
             false, 20u, false, 0u, 0u, false, 0u}
        };
    const VerifyInlinePointerIndexTerm mixed_width_x64_indices[] = {
        {"debug_inline_automatic_mixed_width_signed_index", 4u, false, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_mixed_width_unsigned_index", 8u, true, 4u,
         false, 0u, 0u, false, 0u}
    };
    const VerifyInlinePointerIndexTerm
        signed_to_narrow_unsigned_index[] = {
            {"debug_inline_automatic_signed_to_narrow_unsigned_index",
             2u, false, 4u, false, 0u, 4u, false, 0u}
        };
    const VerifyInlinePointerIndexTerm
        signed_narrow_unsigned_widen_index[] = {
            {"debug_inline_automatic_signed_narrow_unsigned_index",
             2u, false, 4u, false, 0u, 4u, false, 0u}
        };
    const VerifyInlinePointerIndexTerm
        signed_narrow_unsigned_multistage_index[] = {
            {"debug_inline_automatic_signed_narrow_unsigned_multistage_index",
             2u, false, 4u, false, 0u, 4u, false, 0u}
        };
    const VerifyInlinePointerIndexTerm
        signed_narrow_unsigned_signedwide_index[] = {
            {"debug_inline_automatic_signed_narrow_unsigned_signedwide_index",
             2u, false, 4u, false, 0u, 4u, false, 0u}
        };
    const VerifyInlinePointerIndexTerm multiple_mask_x64_index[] = {
        {"debug_inline_automatic_multiple_mask_index", 1u, false, 4u,
         false, 0u, 0u, false, 0u}
    };
    const uint8_t multiple_mask_x64_steps[] = {4u, 2u};
    const VerifyInlinePointerIndexTerm unsigned_computed_x86_indices[] = {
        {"debug_inline_automatic_unsigned_first_index", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_unsigned_second_index", 4u, true, 4u,
         true, 0u, 0u, false, 0u}
    };
    const VerifyInlinePointerIndexTerm unsigned_computed_x64_indices[] = {
        {"debug_inline_automatic_unsigned_first_index", 8u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_unsigned_second_index", 8u, true, 4u,
         true, 0u, 0u, false, 0u}
    };
    const VerifyInlinePointerIndexTerm narrow_unsigned_computed_x86_indices[] = {
        {"debug_inline_automatic_narrow_unsigned_first_index", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_narrow_unsigned_second_index", 4u, true, 4u,
         true, 0u, 0u, false, 0u}
    };
    const VerifyInlinePointerIndexTerm narrow_unsigned_computed_x64_indices[] = {
        {"debug_inline_automatic_narrow_unsigned_first_index", 4u, true, 4u,
         false, 2u, 4u, false, 0u},
        {"debug_inline_automatic_narrow_unsigned_second_index", 4u, true, 4u,
         true, 0u, 0u, false, 0u}
    };
    const VerifyInlinePointerIndexTerm
        narrow_unsigned_constant_index_x64_terms[] = {
            {"debug_inline_automatic_narrow_unsigned_constant_index", 4u,
             true, 4u, false, 2u, 4u, false, 0u},
            {NULL, 4u, true, 4u, false, 0u, 0u, false, 1u}
        };
    const VerifyInlinePointerIndexTerm
        large_unsigned_constant_index_x64_terms[] = {
            {NULL, 8u, true, 4u, false, 1u, 8u, false,
             UINT64_C(0x8000000000000000)}
        };
    const VerifyInlinePointerIndexTerm many_unsigned_index_x86_terms[] = {
        {"debug_inline_automatic_many_unsigned_index_0", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_1", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_2", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_3", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_4", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_5", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_6", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_7", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_8", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_9", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_10", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_11", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_12", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_13", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_14", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_15", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_16", 4u, true, 4u,
         false, 0u, 0u, false, 0u}
    };
    const VerifyInlinePointerIndexTerm many_unsigned_index_x64_terms[] = {
        {"debug_inline_automatic_many_unsigned_index_0", 4u, true, 4u,
         false, 17u, 4u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_1", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_2", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_3", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_4", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_5", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_6", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_7", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_8", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_9", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_10", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_11", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_12", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_13", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_14", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_15", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_many_unsigned_index_16", 4u, true, 4u,
         false, 0u, 0u, false, 0u}
    };
    const VerifyInlinePointerIndexTerm scaled_unsigned_index_x86_terms[] = {
        {"debug_inline_automatic_scaled_unsigned_index", 4u, true, 4u,
         false, 0u, 0u, false, 2u},
        {"debug_inline_automatic_scaled_unsigned_index", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_add_unsigned_index", 4u, true, 4u,
         false, 0u, 0u, false, 0u}
    };
    const VerifyInlinePointerIndexTerm scaled_unsigned_index_x64_terms[] = {
        {"debug_inline_automatic_scaled_unsigned_index", 4u, true, 4u,
         false, 3u, 4u, false, 2u},
        {"debug_inline_automatic_scaled_unsigned_index", 4u, true, 4u,
         false, 0u, 0u, false, 0u},
        {"debug_inline_automatic_add_unsigned_index", 4u, true, 4u,
         false, 0u, 0u, false, 0u}
    };
    const VerifyInlinePointerIndexTerm scaled_signed_index_terms[] = {
        {"debug_inline_automatic_scaled_signed_index", 4u, false, 4u,
         false, 0u, 0u, false, 3u},
        {"debug_inline_automatic_add_signed_index", 4u, false, 4u,
         false, 0u, 0u, false, 0u}
    };
    const VerifyInlinePointerIndexTerm neg_scaled_signed_index_terms[] = {
        {"debug_inline_automatic_neg_scaled_signed_index", 4u, false, 4u,
         false, 0u, 0u, false, -3},
        {"debug_inline_automatic_neg_add_signed_index", 4u, false, 4u,
         false, 0u, 0u, false, 0u}
    };
    if (argc == 10 &&
        strcmp(argv[1], "--verify-inline-integer-expression") == 0) {
        typedef struct {
            const char* source;
            bool is_unsigned;
            uint8_t operation;
            int64_t constant;
        } AdditionalWideComparison;
        static const AdditionalWideComparison additional_wide_comparisons[] = {
            {"debug_inline_signed_long_long_less_equal_entry_source",
             false, 0x2cu, 0},
            {"debug_inline_signed_long_long_greater_entry_source",
             false, 0x2bu, 0},
            {"debug_inline_unsigned_long_long_greater_equal_entry_source",
             true, 0x2au, INT64_MIN},
            {"debug_inline_unsigned_long_long_equal_entry_source",
             true, 0x29u, INT64_MIN},
            {"debug_inline_unsigned_long_long_not_equal_entry_source",
             true, 0x2eu, INT64_MIN},
            {"debug_inline_signed_long_long_unsigned_int_compare_entry_source",
             false, 0x2du, INT64_C(0x80000000)},
            {"debug_inline_unsigned_long_long_signed_int_compare_entry_source",
             true, 0x2du, -1},
            {"debug_inline_signed_long_long_int_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_unsigned_long_long_unsigned_int_compare_entry_source",
             true, 0x2du, 0},
            {"debug_inline_signed_long_long_signed_char_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_signed_long_long_unsigned_char_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_unsigned_long_long_signed_short_compare_entry_source",
             true, 0x2du, 0},
            {"debug_inline_unsigned_long_long_unsigned_short_compare_entry_source",
             true, 0x2du, 0},
            {"debug_inline_signed_long_long_bool_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_unsigned_long_long_signed_char_enum_compare_entry_source",
             true, 0x2du, 0},
            {"debug_inline_signed_long_long_unsigned_short_enum_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_signed_long_long_unsigned_int_enum_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_signed_long_long_unsigned_char_enum_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_signed_long_long_signed_short_enum_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_signed_long_long_signed_int_enum_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_signed_long_long_scoped_unsigned_char_enum_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_signed_long_long_scoped_signed_short_enum_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_signed_long_long_scoped_unsigned_short_enum_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_signed_long_long_scoped_signed_int_enum_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_unsigned_long_long_scoped_unsigned_wide_enum_compare_entry_source",
             true, 0x2du, 0},
            {"debug_inline_unsigned_long_long_scoped_signed_short_enum_to_unsigned_compare_entry_source",
             true, 0x2du, 0},
            {"debug_inline_signed_long_long_add_one_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_signed_long_long_sub_one_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_signed_long_long_add_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_unsigned_long_long_add_compare_entry_source",
             true, 0x2du, INT64_C(0x100000000)},
            {"debug_inline_signed_long_long_sub_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_unsigned_long_long_sub_compare_entry_source",
             true, 0x2du, INT64_C(0x100000000)},
            {"debug_inline_signed_long_long_bitand_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_signed_long_long_bitor_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_signed_long_long_bitxor_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_signed_long_long_bitnot_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_signed_long_long_nested_bitwise_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_signed_long_long_nested_add_bitxor_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_signed_long_long_nested_sub_bitxor_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_signed_long_long_mul_small_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_signed_long_long_mul_wide_constant_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_signed_long_long_nested_add_mul_wide_constant_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_signed_long_long_negated_bitwise_compare_entry_source",
             false, 0x2du, 0},
            {"debug_inline_unsigned_long_long_shift_compare_entry_source",
             true, 0x2du, INT64_MIN},
            {"debug_inline_signed_long_long_shift_compare_entry_source",
             false, 0x2du, -1}
        };
        const char* wide_paths[] = {argv[6], argv[7], argv[8], argv[9]};
        const uint16_t wide_architectures[] = {
            ARCH_X86, ARCH_X64, ARCH_X86, ARCH_X64
        };
        verify_inline_integer_expression_cases(argv[2], ARCH_X86);
        verify_inline_integer_expression_cases(argv[3], ARCH_X64);
        verify_inline_integer_expression_cases(argv[4], ARCH_X86);
        verify_inline_integer_expression_cases(argv[5], ARCH_X64);
        for (size_t path_index = 0u;
             path_index < sizeof(wide_paths) / sizeof(wide_paths[0]);
             ++path_index) {
            for (size_t comparison_index = 0u;
                 comparison_index < sizeof(additional_wide_comparisons) /
                     sizeof(additional_wide_comparisons[0]);
                 ++comparison_index) {
                const AdditionalWideComparison* comparison =
                    &additional_wide_comparisons[comparison_index];
                verify_inline_integer_expression_parameter(
                    wide_paths[path_index], wide_architectures[path_index],
                    "debug_nonconstant_integer_parameter_inline",
                    comparison->source, comparison->is_unsigned,
                    comparison->operation, 8u, comparison->constant,
                    NULL, false);
            }
        }
        verify_inline_integer_expression_parameter(
            argv[6], ARCH_X86, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_unsigned_enum_long_compare_entry_source",
            false, 0x2du, 0u, -1, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[6], ARCH_X86, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_unsigned_short_enum_long_compare_entry_source",
            false, 0x2du, 2u, -1, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[6], ARCH_X86, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_signed_long_long_compare_entry_source",
            false, 0x2du, 8u, 0, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[6], ARCH_X86, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_unsigned_long_long_compare_entry_source",
            true, 0x2du, 8u, INT64_MIN, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[6], ARCH_X86, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_unsigned_char_enum_int_compare_entry_source",
            false, 0x2du, 1u, -1, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[6], ARCH_X86, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_unsigned_char_enum_signed_char_enum_compare_entry_source",
            false, 0x2du, 1u, -1, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[6], ARCH_X86, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_signed_char_enum_unsigned_int_compare_entry_source",
            true, 0x2du, 1u, INT64_C(0x80000000), NULL, false);
        verify_inline_integer_expression_parameter(
            argv[7], ARCH_X64, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_unsigned_enum_long_compare_entry_source",
            false, 0x2du, 0u, -1, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[7], ARCH_X64, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_unsigned_short_enum_long_compare_entry_source",
            false, 0x2du, 2u, -1, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[7], ARCH_X64, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_signed_long_long_compare_entry_source",
            false, 0x2du, 8u, 0, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[7], ARCH_X64, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_unsigned_long_long_compare_entry_source",
            true, 0x2du, 8u, INT64_MIN, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[7], ARCH_X64, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_unsigned_char_enum_int_compare_entry_source",
            false, 0x2du, 1u, -1, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[7], ARCH_X64, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_unsigned_char_enum_signed_char_enum_compare_entry_source",
            false, 0x2du, 1u, -1, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[7], ARCH_X64, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_signed_char_enum_unsigned_int_compare_entry_source",
            true, 0x2du, 1u, INT64_C(0x80000000), NULL, false);
        verify_inline_integer_expression_parameter(
            argv[8], ARCH_X86, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_unsigned_enum_long_compare_entry_source",
            false, 0x2du, 0u, -1, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[8], ARCH_X86, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_unsigned_short_enum_long_compare_entry_source",
            false, 0x2du, 2u, -1, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[8], ARCH_X86, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_signed_long_long_compare_entry_source",
            false, 0x2du, 8u, 0, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[8], ARCH_X86, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_unsigned_long_long_compare_entry_source",
            true, 0x2du, 8u, INT64_MIN, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[8], ARCH_X86, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_unsigned_char_enum_int_compare_entry_source",
            false, 0x2du, 1u, -1, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[8], ARCH_X86, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_unsigned_char_enum_signed_char_enum_compare_entry_source",
            false, 0x2du, 1u, -1, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[8], ARCH_X86, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_signed_char_enum_unsigned_int_compare_entry_source",
            true, 0x2du, 1u, INT64_C(0x80000000), NULL, false);
        verify_inline_integer_expression_parameter(
            argv[9], ARCH_X64, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_unsigned_enum_long_compare_entry_source",
            false, 0x2du, 0u, -1, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[9], ARCH_X64, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_unsigned_short_enum_long_compare_entry_source",
            false, 0x2du, 2u, -1, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[9], ARCH_X64, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_signed_long_long_compare_entry_source",
            false, 0x2du, 8u, 0, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[9], ARCH_X64, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_unsigned_long_long_compare_entry_source",
            true, 0x2du, 8u, INT64_MIN, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[9], ARCH_X64, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_unsigned_char_enum_int_compare_entry_source",
            false, 0x2du, 1u, -1, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[9], ARCH_X64, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_unsigned_char_enum_signed_char_enum_compare_entry_source",
            false, 0x2du, 1u, -1, NULL, false);
        verify_inline_integer_expression_parameter(
            argv[9], ARCH_X64, "debug_nonconstant_integer_parameter_inline",
            "debug_inline_signed_char_enum_unsigned_int_compare_entry_source",
            true, 0x2du, 1u, INT64_C(0x80000000), NULL, false);
        return 0;
    }
    assert(argc == 44);
    verify_debug_object(argv[1], ARCH_X86, 0x000cu,
                        "tests/debug_info.c", "debug_line_entry",
                        "debug_declared_inline");
    verify_enum_underlying_dwarf(argv[1], ARCH_X86, "debug_enum", "int",
                                 4u, 0x05u, false);
    verify_legacy_statement_line_rows(argv[1], ARCH_X86);
    verify_debug_object(argv[2], ARCH_X64, 0x000cu,
                        "tests/debug_info.c", "debug_line_entry",
                        "debug_declared_inline");
    verify_enum_underlying_dwarf(argv[2], ARCH_X64, "debug_enum", "int",
                                 4u, 0x05u, false);
    verify_signed_enum_dwarf(argv[1], ARCH_X86, "debug_enum", "int",
                             "DEBUG_ENUM_NEGATIVE", 4u, 0x05u, -2);
    verify_signed_enum_dwarf(argv[2], ARCH_X64, "debug_enum", "int",
                             "DEBUG_ENUM_NEGATIVE", 4u, 0x05u, -2);
    verify_typedef_die(argv[1], ARCH_X86, "DebugInfoWord", "unsigned int");
    verify_typedef_die(argv[2], ARCH_X64, "DebugInfoWord", "unsigned int");
    verify_legacy_statement_line_rows(argv[2], ARCH_X64);
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
    verify_object_pointer_parameter(argv[14], ARCH_X86);
    verify_object_pointer_parameter(argv[15], ARCH_X64);
    verify_cxx_member_pointer_type(argv[14], ARCH_X86,
                                   "debug_data_member_pointer", false);
    verify_cxx_member_pointer_type(argv[15], ARCH_X64,
                                   "debug_data_member_pointer", false);
    verify_cxx_member_pointer_type(argv[14], ARCH_X86,
                                   "debug_function_member_pointer", true);
    verify_cxx_member_pointer_type(argv[15], ARCH_X64,
                                   "debug_function_member_pointer", true);
    verify_static_member_containing_type(argv[14], ARCH_X86);
    verify_static_member_containing_type(argv[15], ARCH_X64);
    verify_cxx_member_accessibility(argv[14], ARCH_X86);
    verify_cxx_member_accessibility(argv[15], ARCH_X64);
    verify_cxx_inheritance_dies(argv[14], ARCH_X86);
    verify_cxx_inheritance_dies(argv[15], ARCH_X64);
    verify_typedef_die(argv[14], ARCH_X86, "DebugMemberWord",
                       "unsigned int");
    verify_typedef_die(argv[15], ARCH_X64, "DebugMemberWord",
                       "unsigned int");
    verify_typedef_die(argv[14], ARCH_X86, "DebugUsingWord",
                       "unsigned short");
    verify_typedef_die(argv[15], ARCH_X64, "DebugUsingWord",
                       "unsigned short");
    verify_typedef_die(argv[14], ARCH_X86,
                       "DebugAliasScope::NamespaceWord",
                       "unsigned long long");
    verify_typedef_die(argv[15], ARCH_X64,
                       "DebugAliasScope::NamespaceWord",
                       "unsigned long long");
    verify_cxx_class_type_alias(argv[14], ARCH_X86, "PublicWord",
                                "unsigned short", 1u);
    verify_cxx_class_type_alias(argv[15], ARCH_X64, "PublicWord",
                                "unsigned short", 1u);
    verify_cxx_class_type_alias(argv[14], ARCH_X86, "ProtectedWord",
                                "unsigned long long", 2u);
    verify_cxx_class_type_alias(argv[15], ARCH_X64, "ProtectedWord",
                                "unsigned long long", 2u);
    verify_cxx_class_type_alias(argv[14], ARCH_X86, "PrivateWord",
                                "unsigned int", 3u);
    verify_cxx_class_type_alias(argv[15], ARCH_X64, "PrivateWord",
                                "unsigned int", 3u);
    verify_local_type_alias_scope(argv[14], ARCH_X86);
    verify_local_type_alias_scope(argv[15], ARCH_X64);
    verify_cxx_method_accessibility(argv[14], ARCH_X86);
    verify_cxx_method_accessibility(argv[15], ARCH_X64);
    verify_cxx_reference_type_dies(argv[14], ARCH_X86);
    verify_cxx_reference_type_dies(argv[15], ARCH_X64);
    verify_cxx_for_initializer_scope(argv[14], ARCH_X86);
    verify_cxx_for_initializer_scope(argv[15], ARCH_X64);
    verify_optimized_verified_debug_object(argv[16], ARCH_X86);
    verify_optimized_verified_debug_object(argv[17], ARCH_X64);
    verify_unsigned_enum_dwarf(argv[18], ARCH_X86, "DebugUnsignedEnum",
                               "unsigned long long", "maximum", true,
                               8u, UINT64_MAX);
    verify_unsigned_enum_dwarf(argv[19], ARCH_X64, "DebugUnsignedEnum",
                               "unsigned long long", "maximum", true,
                               8u, UINT64_MAX);
    verify_unsigned_enum_dwarf(argv[18], ARCH_X86,
                               "DebugInferredUnsignedEnum",
                               "unsigned long long", "inferred_maximum",
                               false, 8u, UINT64_MAX);
    verify_unsigned_enum_dwarf(argv[19], ARCH_X64,
                               "DebugInferredUnsignedEnum",
                               "unsigned long", "inferred_maximum", false,
                               8u, UINT64_MAX);
    verify_unsigned_enum_dwarf(argv[18], ARCH_X86,
                               "DebugInferredUnsignedInt",
                               "unsigned int", "inferred_uint_max", false,
                               4u, UINT32_MAX);
    verify_unsigned_enum_dwarf(argv[19], ARCH_X64,
                               "DebugInferredUnsignedInt",
                               "unsigned int", "inferred_uint_max", false,
                               4u, UINT32_MAX);
    verify_optimized_cxx_verified_debug_object(argv[20], ARCH_X86);
    verify_optimized_cxx_verified_debug_object(argv[21], ARCH_X64);
    verify_linked_image_excludes_debug_sections(argv[22], RIN_ARCH_X86);
    verify_linked_image_excludes_debug_sections(argv[23], RIN_ARCH_X86_64);
    verify_global_initializer_unwind(argv[30], ARCH_X86, true);
    verify_global_initializer_unwind(argv[31], ARCH_X64, true);
    verify_global_initializer_unwind(argv[32], ARCH_X86, false);
    verify_global_initializer_unwind(argv[33], ARCH_X64, false);
    verify_enum_underlying_dwarf(argv[18], ARCH_X86, "DebugSignedEnum",
                                 "char", 1u, 0x06u, false);
    verify_enum_underlying_dwarf(argv[19], ARCH_X64, "DebugSignedEnum",
                                 "char", 1u, 0x06u, false);
    verify_scoped_enum_dwarf(argv[18], ARCH_X86, "DebugScopedEnum",
                             "unsigned short", 2u, 0x07u, 35u,
                             "scoped_value", 7u);
    verify_scoped_enum_dwarf(argv[19], ARCH_X64, "DebugScopedEnum",
                             "unsigned short", 2u, 0x07u, 35u,
                             "scoped_value", 7u);
    verify_scoped_enum_dwarf(argv[18], ARCH_X86, "DebugScopedStructEnum",
                             "char", 1u, 0x06u, 16u,
                             "struct_scoped_value", -1);
    verify_scoped_enum_dwarf(argv[19], ARCH_X64, "DebugScopedStructEnum",
                             "char", 1u, 0x06u, 16u,
                             "struct_scoped_value", -1);
    verify_tls_global_object(argv[18], "debug_cpp_tls_data");
    verify_tls_global_object(argv[19], "debug_cpp_tls_data");
    verify_inline_debug_object(argv[24], ARCH_X86);
    verify_inline_debug_object(argv[25], ARCH_X64);
    verify_inline_debug_object(argv[38], ARCH_X86);
    verify_inline_debug_object(argv[39], ARCH_X64);
    verify_inline_dynamic_scalar_parameter(
        argv[24], ARCH_X86, "debug_float_parameter_inline",
        "debug_inline_nonconstant_float_entry",
        "debug_inline_nonconstant_float_source");
    verify_inline_dynamic_scalar_parameter(
        argv[25], ARCH_X64, "debug_float_parameter_inline",
        "debug_inline_nonconstant_float_entry",
        "debug_inline_nonconstant_float_source");
    verify_inline_dynamic_scalar_parameter(
        argv[24], ARCH_X86, "debug_double_parameter_inline",
        "debug_inline_nonconstant_double_entry",
        "debug_inline_nonconstant_double_source");
    verify_inline_dynamic_scalar_parameter(
        argv[25], ARCH_X64, "debug_double_parameter_inline",
        "debug_inline_nonconstant_double_entry",
        "debug_inline_nonconstant_double_source");
    verify_inline_dynamic_scalar_parameter(
        argv[38], ARCH_X86, "debug_float_parameter_inline",
        "debug_inline_nonconstant_float_entry",
        "debug_inline_nonconstant_float_source");
    verify_inline_dynamic_scalar_parameter(
        argv[39], ARCH_X64, "debug_float_parameter_inline",
        "debug_inline_nonconstant_float_entry",
        "debug_inline_nonconstant_float_source");
    verify_inline_dynamic_scalar_parameter(
        argv[38], ARCH_X86, "debug_double_parameter_inline",
        "debug_inline_nonconstant_double_entry",
        "debug_inline_nonconstant_double_source");
    verify_inline_dynamic_scalar_parameter(
        argv[39], ARCH_X64, "debug_double_parameter_inline",
        "debug_inline_nonconstant_double_entry",
        "debug_inline_nonconstant_double_source");
    verify_inline_dynamic_scalar_parameter(
        argv[24], ARCH_X86, "debug_float_parameter_inline",
        "debug_inline_nonconstant_float_identity_cast_entry",
        "debug_inline_nonconstant_float_cast_source");
    verify_inline_dynamic_scalar_parameter(
        argv[25], ARCH_X64, "debug_float_parameter_inline",
        "debug_inline_nonconstant_float_identity_cast_entry",
        "debug_inline_nonconstant_float_cast_source");
    verify_inline_dynamic_scalar_parameter(
        argv[24], ARCH_X86, "debug_double_parameter_inline",
        "debug_inline_nonconstant_double_identity_cast_entry",
        "debug_inline_nonconstant_double_cast_source");
    verify_inline_dynamic_scalar_parameter(
        argv[25], ARCH_X64, "debug_double_parameter_inline",
        "debug_inline_nonconstant_double_identity_cast_entry",
        "debug_inline_nonconstant_double_cast_source");
    verify_inline_dynamic_scalar_parameter(
        argv[38], ARCH_X86, "debug_float_parameter_inline",
        "debug_inline_nonconstant_float_identity_cast_entry",
        "debug_inline_nonconstant_float_cast_source");
    verify_inline_dynamic_scalar_parameter(
        argv[39], ARCH_X64, "debug_float_parameter_inline",
        "debug_inline_nonconstant_float_identity_cast_entry",
        "debug_inline_nonconstant_float_cast_source");
    verify_inline_dynamic_scalar_parameter(
        argv[38], ARCH_X86, "debug_double_parameter_inline",
        "debug_inline_nonconstant_double_identity_cast_entry",
        "debug_inline_nonconstant_double_cast_source");
    verify_inline_dynamic_scalar_parameter(
        argv[39], ARCH_X64, "debug_double_parameter_inline",
        "debug_inline_nonconstant_double_identity_cast_entry",
        "debug_inline_nonconstant_double_cast_source");
    verify_inline_integer_expression_cases(argv[24], ARCH_X86);
    verify_inline_integer_expression_cases(argv[25], ARCH_X64);
    verify_inline_integer_expression_cases(argv[38], ARCH_X86);
    verify_inline_integer_expression_cases(argv[39], ARCH_X64);
    verify_cxx_inline_nullptr_parameter(argv[40], ARCH_X86);
    verify_cxx_inline_nullptr_parameter(argv[41], ARCH_X64);
    verify_cxx_inline_nullptr_parameter(argv[42], ARCH_X86);
    verify_cxx_inline_nullptr_parameter(argv[43], ARCH_X64);
    verify_cxx_inline_adjusted_base_parameter(argv[40], ARCH_X86);
    verify_cxx_inline_adjusted_base_parameter(argv[41], ARCH_X64);
    verify_cxx_inline_adjusted_base_parameter(argv[42], ARCH_X86);
    verify_cxx_inline_adjusted_base_parameter(argv[43], ARCH_X64);
    verify_cxx_inline_automatic_pointer_value(
        argv[40], ARCH_X86, "debug_inline_automatic_member_address_entry",
        "debug_inline_automatic_member_source", 4, false);
    verify_cxx_inline_automatic_pointer_value(
        argv[41], ARCH_X64, "debug_inline_automatic_member_address_entry",
        "debug_inline_automatic_member_source", 4, false);
    verify_cxx_inline_automatic_pointer_value(
        argv[42], ARCH_X86, "debug_inline_automatic_member_address_entry",
        "debug_inline_automatic_member_source", 4, false);
    verify_cxx_inline_automatic_pointer_value(
        argv[43], ARCH_X64, "debug_inline_automatic_member_address_entry",
        "debug_inline_automatic_member_source", 4, false);
    verify_cxx_inline_automatic_pointer_value(
        argv[40], ARCH_X86,
        "debug_inline_automatic_array_element_address_entry",
        "debug_inline_automatic_array_source", 16, false);
    verify_cxx_inline_automatic_pointer_value(
        argv[41], ARCH_X64,
        "debug_inline_automatic_array_element_address_entry",
        "debug_inline_automatic_array_source", 16, false);
    verify_cxx_inline_automatic_pointer_value(
        argv[42], ARCH_X86,
        "debug_inline_automatic_array_element_address_entry",
        "debug_inline_automatic_array_source", 16, false);
    verify_cxx_inline_automatic_pointer_value(
        argv[43], ARCH_X64,
        "debug_inline_automatic_array_element_address_entry",
        "debug_inline_automatic_array_source", 16, false);
    verify_cxx_inline_automatic_pointer_value(
        argv[40], ARCH_X86, "debug_inline_automatic_pointer_add_entry",
        "debug_inline_automatic_pointer_source", 8, true);
    verify_cxx_inline_automatic_pointer_value(
        argv[41], ARCH_X64, "debug_inline_automatic_pointer_add_entry",
        "debug_inline_automatic_pointer_source", 8, true);
    verify_cxx_inline_automatic_pointer_value(
        argv[42], ARCH_X86, "debug_inline_automatic_pointer_add_entry",
        "debug_inline_automatic_pointer_source", 8, true);
    verify_cxx_inline_automatic_pointer_value(
        argv[43], ARCH_X64, "debug_inline_automatic_pointer_add_entry",
        "debug_inline_automatic_pointer_source", 8, true);
    verify_cxx_inline_automatic_pointer_value(
        argv[40], ARCH_X86, "debug_inline_automatic_pointer_sub_entry",
        "debug_inline_automatic_pointer_sub_source", -4, true);
    verify_cxx_inline_automatic_pointer_value(
        argv[41], ARCH_X64, "debug_inline_automatic_pointer_sub_entry",
        "debug_inline_automatic_pointer_sub_source", -4, true);
    verify_cxx_inline_automatic_pointer_value(
        argv[42], ARCH_X86, "debug_inline_automatic_pointer_sub_entry",
        "debug_inline_automatic_pointer_sub_source", -4, true);
    verify_cxx_inline_automatic_pointer_value(
        argv[43], ARCH_X64, "debug_inline_automatic_pointer_sub_entry",
        "debug_inline_automatic_pointer_sub_source", -4, true);
    verify_cxx_inline_automatic_pointer_value(
        argv[40], ARCH_X86, "debug_inline_automatic_pointer_chain_entry",
        "debug_inline_automatic_pointer_chain_source", 4, true);
    verify_cxx_inline_automatic_pointer_value(
        argv[41], ARCH_X64, "debug_inline_automatic_pointer_chain_entry",
        "debug_inline_automatic_pointer_chain_source", 4, true);
    verify_cxx_inline_automatic_pointer_value(
        argv[42], ARCH_X86, "debug_inline_automatic_pointer_chain_entry",
        "debug_inline_automatic_pointer_chain_source", 4, true);
    verify_cxx_inline_automatic_pointer_value(
        argv[43], ARCH_X64, "debug_inline_automatic_pointer_chain_entry",
        "debug_inline_automatic_pointer_chain_source", 4, true);
    verify_cxx_inline_automatic_pointer_index(
        argv[40], ARCH_X86, "debug_inline_automatic_pointer_index_entry",
        "debug_inline_automatic_pointer_index_source",
        "debug_inline_automatic_index_source", 0, true, 4u, false, 4u,
        false);
    verify_cxx_inline_automatic_pointer_index(
        argv[41], ARCH_X64, "debug_inline_automatic_pointer_index_entry",
        "debug_inline_automatic_pointer_index_source",
        "debug_inline_automatic_index_source", 0, true, 4u, false, 4u,
        false);
    verify_cxx_inline_automatic_pointer_index(
        argv[42], ARCH_X86, "debug_inline_automatic_pointer_index_entry",
        "debug_inline_automatic_pointer_index_source",
        "debug_inline_automatic_index_source", 0, true, 4u, false, 4u,
        false);
    verify_cxx_inline_automatic_pointer_index(
        argv[43], ARCH_X64, "debug_inline_automatic_pointer_index_entry",
        "debug_inline_automatic_pointer_index_source",
        "debug_inline_automatic_index_source", 0, true, 4u, false, 4u,
        false);
    verify_cxx_inline_automatic_pointer_divided_index(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_divided_index_entry",
        "debug_inline_automatic_pointer_divided_source",
        "debug_inline_automatic_divided_index", 2);
    verify_cxx_inline_automatic_pointer_divided_index(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_divided_index_entry",
        "debug_inline_automatic_pointer_divided_source",
        "debug_inline_automatic_divided_index", 2);
    verify_cxx_inline_automatic_pointer_divided_index(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_divided_index_entry",
        "debug_inline_automatic_pointer_divided_source",
        "debug_inline_automatic_divided_index", 2);
    verify_cxx_inline_automatic_pointer_divided_index(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_divided_index_entry",
        "debug_inline_automatic_pointer_divided_source",
        "debug_inline_automatic_divided_index", 2);
    verify_cxx_inline_automatic_pointer_divided_index(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_negative_divisor_index_entry",
        "debug_inline_automatic_pointer_negative_divisor_source",
        "debug_inline_automatic_negative_divisor_index", -2);
    verify_cxx_inline_automatic_pointer_divided_index(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_negative_divisor_index_entry",
        "debug_inline_automatic_pointer_negative_divisor_source",
        "debug_inline_automatic_negative_divisor_index", -2);
    verify_cxx_inline_automatic_pointer_divided_index(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_negative_divisor_index_entry",
        "debug_inline_automatic_pointer_negative_divisor_source",
        "debug_inline_automatic_negative_divisor_index", -2);
    verify_cxx_inline_automatic_pointer_divided_index(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_negative_divisor_index_entry",
        "debug_inline_automatic_pointer_negative_divisor_source",
        "debug_inline_automatic_negative_divisor_index", -2);
    verify_cxx_inline_automatic_pointer_divided_index(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_odd_divisor_index_entry",
        "debug_inline_automatic_pointer_odd_divisor_source",
        "debug_inline_automatic_odd_divisor_index", 3);
    verify_cxx_inline_automatic_pointer_divided_index(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_odd_divisor_index_entry",
        "debug_inline_automatic_pointer_odd_divisor_source",
        "debug_inline_automatic_odd_divisor_index", 3);
    verify_cxx_inline_automatic_pointer_divided_index(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_odd_divisor_index_entry",
        "debug_inline_automatic_pointer_odd_divisor_source",
        "debug_inline_automatic_odd_divisor_index", 3);
    verify_cxx_inline_automatic_pointer_divided_index(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_odd_divisor_index_entry",
        "debug_inline_automatic_pointer_odd_divisor_source",
        "debug_inline_automatic_odd_divisor_index", 3);
    verify_cxx_inline_automatic_pointer_divided_index(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_negative_odd_divisor_index_entry",
        "debug_inline_automatic_pointer_negative_odd_divisor_source",
        "debug_inline_automatic_negative_odd_divisor_index", -3);
    verify_cxx_inline_automatic_pointer_divided_index(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_negative_odd_divisor_index_entry",
        "debug_inline_automatic_pointer_negative_odd_divisor_source",
        "debug_inline_automatic_negative_odd_divisor_index", -3);
    verify_cxx_inline_automatic_pointer_divided_index(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_negative_odd_divisor_index_entry",
        "debug_inline_automatic_pointer_negative_odd_divisor_source",
        "debug_inline_automatic_negative_odd_divisor_index", -3);
    verify_cxx_inline_automatic_pointer_divided_index(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_negative_odd_divisor_index_entry",
        "debug_inline_automatic_pointer_negative_odd_divisor_source",
        "debug_inline_automatic_negative_odd_divisor_index", -3);
    verify_cxx_inline_automatic_pointer_index(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_signed_to_unsigned_index_entry",
        "debug_inline_automatic_pointer_signed_to_unsigned_source",
        "debug_inline_automatic_signed_to_unsigned_index", 0, true, 4u,
        true, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_signed_to_unsigned_index_entry",
        "debug_inline_automatic_pointer_signed_to_unsigned_source",
        "debug_inline_automatic_signed_to_unsigned_index", 0, true, 4u,
        true, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_signed_to_unsigned_index_entry",
        "debug_inline_automatic_pointer_signed_to_unsigned_source",
        "debug_inline_automatic_signed_to_unsigned_index", 0, true, 4u,
        true, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_signed_to_unsigned_index_entry",
        "debug_inline_automatic_pointer_signed_to_unsigned_source",
        "debug_inline_automatic_signed_to_unsigned_index", 0, true, 4u,
        true, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_signed_narrow_index_entry",
        "debug_inline_automatic_pointer_signed_narrow_source",
        "debug_inline_automatic_signed_narrow_index", 0, true, 2u, false,
        4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_signed_narrow_index_entry",
        "debug_inline_automatic_pointer_signed_narrow_source",
        "debug_inline_automatic_signed_narrow_index", 0, true, 2u, false,
        4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_signed_narrow_index_entry",
        "debug_inline_automatic_pointer_signed_narrow_source",
        "debug_inline_automatic_signed_narrow_index", 0, true, 2u, false,
        4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_signed_narrow_index_entry",
        "debug_inline_automatic_pointer_signed_narrow_source",
        "debug_inline_automatic_signed_narrow_index", 0, true, 2u, false,
        4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_signed_narrow_widen_index_entry",
        "debug_inline_automatic_pointer_signed_narrow_widen_source",
        "debug_inline_automatic_signed_narrow_widen_index", 0, true, 2u,
        false, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_signed_narrow_widen_index_entry",
        "debug_inline_automatic_pointer_signed_narrow_widen_source",
        "debug_inline_automatic_signed_narrow_widen_index", 0, true, 2u,
        false, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_signed_narrow_widen_index_entry",
        "debug_inline_automatic_pointer_signed_narrow_widen_source",
        "debug_inline_automatic_signed_narrow_widen_index", 0, true, 2u,
        false, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_signed_narrow_widen_index_entry",
        "debug_inline_automatic_pointer_signed_narrow_widen_source",
        "debug_inline_automatic_signed_narrow_widen_index", 0, true, 2u,
        false, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_unsigned_narrow_index_entry",
        "debug_inline_automatic_pointer_unsigned_narrow_source",
        "debug_inline_automatic_unsigned_narrow_index", 0, true, 2u, true,
        4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_unsigned_narrow_index_entry",
        "debug_inline_automatic_pointer_unsigned_narrow_source",
        "debug_inline_automatic_unsigned_narrow_index", 0, true, 2u, true,
        4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_unsigned_narrow_index_entry",
        "debug_inline_automatic_pointer_unsigned_narrow_source",
        "debug_inline_automatic_unsigned_narrow_index", 0, true, 2u, true,
        4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_unsigned_narrow_index_entry",
        "debug_inline_automatic_pointer_unsigned_narrow_source",
        "debug_inline_automatic_unsigned_narrow_index", 0, true, 2u, true,
        4u, false);
    verify_cxx_inline_automatic_pointer_indices(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_mixed_width_index_entry",
        "debug_inline_automatic_pointer_mixed_width_source",
        mixed_width_x64_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_mixed_width_index_entry",
        "debug_inline_automatic_pointer_mixed_width_source",
        mixed_width_x64_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_signed_to_narrow_unsigned_index_entry",
        "debug_inline_automatic_pointer_signed_to_narrow_unsigned_source",
        signed_to_narrow_unsigned_index, 1u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_signed_to_narrow_unsigned_index_entry",
        "debug_inline_automatic_pointer_signed_to_narrow_unsigned_source",
        signed_to_narrow_unsigned_index, 1u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_signed_to_narrow_unsigned_index_entry",
        "debug_inline_automatic_pointer_signed_to_narrow_unsigned_source",
        signed_to_narrow_unsigned_index, 1u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_signed_to_narrow_unsigned_index_entry",
        "debug_inline_automatic_pointer_signed_to_narrow_unsigned_source",
        signed_to_narrow_unsigned_index, 1u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_signed_narrow_unsigned_index_entry",
        "debug_inline_automatic_pointer_signed_narrow_unsigned_source",
        signed_narrow_unsigned_widen_index, 1u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_signed_narrow_unsigned_index_entry",
        "debug_inline_automatic_pointer_signed_narrow_unsigned_source",
        signed_narrow_unsigned_widen_index, 1u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_signed_narrow_unsigned_index_entry",
        "debug_inline_automatic_pointer_signed_narrow_unsigned_source",
        signed_narrow_unsigned_widen_index, 1u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_signed_narrow_unsigned_index_entry",
        "debug_inline_automatic_pointer_signed_narrow_unsigned_source",
        signed_narrow_unsigned_widen_index, 1u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_signed_narrow_unsigned_multistage_entry",
        "debug_inline_automatic_pointer_signed_narrow_unsigned_multistage_source",
        signed_narrow_unsigned_multistage_index, 1u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_signed_narrow_unsigned_multistage_entry",
        "debug_inline_automatic_pointer_signed_narrow_unsigned_multistage_source",
        signed_narrow_unsigned_multistage_index, 1u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_signed_narrow_unsigned_multistage_entry",
        "debug_inline_automatic_pointer_signed_narrow_unsigned_multistage_source",
        signed_narrow_unsigned_multistage_index, 1u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_signed_narrow_unsigned_multistage_entry",
        "debug_inline_automatic_pointer_signed_narrow_unsigned_multistage_source",
        signed_narrow_unsigned_multistage_index, 1u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_signed_narrow_unsigned_signedwide_entry",
        "debug_inline_automatic_pointer_signed_narrow_unsigned_signedwide_source",
        signed_narrow_unsigned_signedwide_index, 1u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_signed_narrow_unsigned_signedwide_entry",
        "debug_inline_automatic_pointer_signed_narrow_unsigned_signedwide_source",
        signed_narrow_unsigned_signedwide_index, 1u, 0, true);
    verify_cxx_inline_automatic_pointer_indices_impl(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_multiple_mask_index_entry",
        "debug_inline_automatic_pointer_multiple_mask_source",
        multiple_mask_x64_index, 1u, 0, true,
        multiple_mask_x64_steps,
        sizeof(multiple_mask_x64_steps) /
            sizeof(multiple_mask_x64_steps[0]), 0, 0);
    verify_cxx_inline_automatic_pointer_indices_impl(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_multiple_mask_index_entry",
        "debug_inline_automatic_pointer_multiple_mask_source",
        multiple_mask_x64_index, 1u, 0, true,
        multiple_mask_x64_steps,
        sizeof(multiple_mask_x64_steps) /
            sizeof(multiple_mask_x64_steps[0]), 0, 0);
    verify_cxx_inline_automatic_pointer_index(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_unsigned_narrow_signed_index_entry",
        "debug_inline_automatic_pointer_unsigned_narrow_signed_source",
        "debug_inline_automatic_unsigned_narrow_signed_index", 0, true,
        2u, true, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_unsigned_narrow_signed_index_entry",
        "debug_inline_automatic_pointer_unsigned_narrow_signed_source",
        "debug_inline_automatic_unsigned_narrow_signed_index", 0, true,
        2u, true, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_unsigned_narrow_signed_index_entry",
        "debug_inline_automatic_pointer_unsigned_narrow_signed_source",
        "debug_inline_automatic_unsigned_narrow_signed_index", 0, true,
        2u, true, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_unsigned_narrow_signed_index_entry",
        "debug_inline_automatic_pointer_unsigned_narrow_signed_source",
        "debug_inline_automatic_unsigned_narrow_signed_index", 0, true,
        2u, true, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_multistage_signed_widen_index_entry",
        "debug_inline_automatic_pointer_multistage_signed_widen_source",
        "debug_inline_automatic_multistage_signed_widen_index", 0, true,
        2u, false, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_multistage_signed_widen_index_entry",
        "debug_inline_automatic_pointer_multistage_signed_widen_source",
        "debug_inline_automatic_multistage_signed_widen_index", 0, true,
        2u, false, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_multistage_signed_widen_index_entry",
        "debug_inline_automatic_pointer_multistage_signed_widen_source",
        "debug_inline_automatic_multistage_signed_widen_index", 0, true,
        2u, false, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_multistage_signed_widen_index_entry",
        "debug_inline_automatic_pointer_multistage_signed_widen_source",
        "debug_inline_automatic_multistage_signed_widen_index", 0, true,
        2u, false, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_multistage_narrow_index_entry",
        "debug_inline_automatic_pointer_multistage_narrow_source",
        "debug_inline_automatic_multistage_narrow_index", 0, true,
        1u, false, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_multistage_narrow_index_entry",
        "debug_inline_automatic_pointer_multistage_narrow_source",
        "debug_inline_automatic_multistage_narrow_index", 0, true,
        1u, false, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_multistage_narrow_index_entry",
        "debug_inline_automatic_pointer_multistage_narrow_source",
        "debug_inline_automatic_multistage_narrow_index", 0, true,
        1u, false, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_multistage_narrow_index_entry",
        "debug_inline_automatic_pointer_multistage_narrow_source",
        "debug_inline_automatic_multistage_narrow_index", 0, true,
        1u, false, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_index_sub_entry",
        "debug_inline_automatic_pointer_index_sub_source",
        "debug_inline_automatic_index_sub_source", 0, true, 4u, true, 4u,
        true);
    verify_cxx_inline_automatic_pointer_index(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_index_sub_entry",
        "debug_inline_automatic_pointer_index_sub_source",
        "debug_inline_automatic_index_sub_source", 0, true, 4u, true, 4u,
        true);
    verify_cxx_inline_automatic_pointer_index(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_index_sub_entry",
        "debug_inline_automatic_pointer_index_sub_source",
        "debug_inline_automatic_index_sub_source", 0, true, 4u, true, 4u,
        true);
    verify_cxx_inline_automatic_pointer_index(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_index_sub_entry",
        "debug_inline_automatic_pointer_index_sub_source",
        "debug_inline_automatic_index_sub_source", 0, true, 4u, true, 4u,
        true);
    verify_cxx_inline_automatic_pointer_index(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_narrow_index_entry",
        "debug_inline_automatic_pointer_narrow_index_source",
        "debug_inline_automatic_narrow_index_source", 0, true, 1u, false,
        4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_narrow_index_entry",
        "debug_inline_automatic_pointer_narrow_index_source",
        "debug_inline_automatic_narrow_index_source", 0, true, 1u, false,
        4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_narrow_index_entry",
        "debug_inline_automatic_pointer_narrow_index_source",
        "debug_inline_automatic_narrow_index_source", 0, true, 1u, false,
        4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_narrow_index_entry",
        "debug_inline_automatic_pointer_narrow_index_source",
        "debug_inline_automatic_narrow_index_source", 0, true, 1u, false,
        4u, false);
    verify_cxx_inline_automatic_pointer_indices(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_compound_index_entry",
        "debug_inline_automatic_pointer_compound_source",
        compound_pointer_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_compound_index_entry",
        "debug_inline_automatic_pointer_compound_source",
        compound_pointer_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_compound_index_entry",
        "debug_inline_automatic_pointer_compound_source",
        compound_pointer_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_compound_index_entry",
        "debug_inline_automatic_pointer_compound_source",
        compound_pointer_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_index(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_parameter_index_entry",
        "debug_inline_automatic_pointer_parameter_source",
        "debug_inline_automatic_parameter_index_source", 0, true, 4u,
        false, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_parameter_index_entry",
        "debug_inline_automatic_pointer_parameter_source",
        "debug_inline_automatic_parameter_index_source", 0, true, 4u,
        false, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_parameter_index_entry",
        "debug_inline_automatic_pointer_parameter_source",
        "debug_inline_automatic_parameter_index_source", 0, true, 4u,
        false, 4u, false);
    verify_cxx_inline_automatic_pointer_index(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_parameter_index_entry",
        "debug_inline_automatic_pointer_parameter_source",
        "debug_inline_automatic_parameter_index_source", 0, true, 4u,
        false, 4u, false);
    verify_cxx_inline_automatic_pointer_indices(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_computed_index_entry",
        "debug_inline_automatic_pointer_computed_source",
        computed_pointer_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_computed_index_entry",
        "debug_inline_automatic_pointer_computed_source",
        computed_pointer_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_computed_index_entry",
        "debug_inline_automatic_pointer_computed_source",
        computed_pointer_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_computed_index_entry",
        "debug_inline_automatic_pointer_computed_source",
        computed_pointer_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[40], ARCH_X86,
        "debug_inline_automatic_nested_array_index_entry",
        "debug_inline_automatic_nested_matrix",
        nested_array_pointer_indices, 2u, 0, false);
    verify_cxx_inline_automatic_pointer_indices(
        argv[41], ARCH_X64,
        "debug_inline_automatic_nested_array_index_entry",
        "debug_inline_automatic_nested_matrix",
        nested_array_pointer_indices, 2u, 0, false);
    verify_cxx_inline_automatic_pointer_indices(
        argv[42], ARCH_X86,
        "debug_inline_automatic_nested_array_index_entry",
        "debug_inline_automatic_nested_matrix",
        nested_array_pointer_indices, 2u, 0, false);
    verify_cxx_inline_automatic_pointer_indices(
        argv[43], ARCH_X64,
        "debug_inline_automatic_nested_array_index_entry",
        "debug_inline_automatic_nested_matrix",
        nested_array_pointer_indices, 2u, 0, false);
    verify_cxx_inline_automatic_pointer_indices(
        argv[40], ARCH_X86,
        "debug_inline_automatic_nested_array_constant_row_entry",
        "debug_inline_automatic_nested_constant_row_matrix",
        nested_array_constant_row_index, 1u, 20, false);
    verify_cxx_inline_automatic_pointer_indices(
        argv[41], ARCH_X64,
        "debug_inline_automatic_nested_array_constant_row_entry",
        "debug_inline_automatic_nested_constant_row_matrix",
        nested_array_constant_row_index, 1u, 20, false);
    verify_cxx_inline_automatic_pointer_indices(
        argv[42], ARCH_X86,
        "debug_inline_automatic_nested_array_constant_row_entry",
        "debug_inline_automatic_nested_constant_row_matrix",
        nested_array_constant_row_index, 1u, 20, false);
    verify_cxx_inline_automatic_pointer_indices(
        argv[43], ARCH_X64,
        "debug_inline_automatic_nested_array_constant_row_entry",
        "debug_inline_automatic_nested_constant_row_matrix",
        nested_array_constant_row_index, 1u, 20, false);
    verify_cxx_inline_automatic_pointer_indices(
        argv[40], ARCH_X86,
        "debug_inline_automatic_nested_array_constant_column_entry",
        "debug_inline_automatic_nested_constant_column_matrix",
        nested_array_constant_column_index, 1u, 8, false);
    verify_cxx_inline_automatic_pointer_indices(
        argv[41], ARCH_X64,
        "debug_inline_automatic_nested_array_constant_column_entry",
        "debug_inline_automatic_nested_constant_column_matrix",
        nested_array_constant_column_index, 1u, 8, false);
    verify_cxx_inline_automatic_pointer_indices(
        argv[42], ARCH_X86,
        "debug_inline_automatic_nested_array_constant_column_entry",
        "debug_inline_automatic_nested_constant_column_matrix",
        nested_array_constant_column_index, 1u, 8, false);
    verify_cxx_inline_automatic_pointer_indices(
        argv[43], ARCH_X64,
        "debug_inline_automatic_nested_array_constant_column_entry",
        "debug_inline_automatic_nested_constant_column_matrix",
        nested_array_constant_column_index, 1u, 8, false);
    verify_cxx_inline_automatic_pointer_indices(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_nested_array_index_entry",
        "debug_inline_automatic_pointer_nested_matrix",
        pointer_nested_array_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_nested_array_index_entry",
        "debug_inline_automatic_pointer_nested_matrix",
        pointer_nested_array_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_nested_array_index_entry",
        "debug_inline_automatic_pointer_nested_matrix",
        pointer_nested_array_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_nested_array_index_entry",
        "debug_inline_automatic_pointer_nested_matrix",
        pointer_nested_array_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices_with_adjustment(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_nested_array_constant_row_entry",
        "debug_inline_automatic_pointer_constant_row_matrix",
        pointer_nested_constant_row_index, 1u, 0, true, 20);
    verify_cxx_inline_automatic_pointer_indices_with_adjustment(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_nested_array_constant_row_entry",
        "debug_inline_automatic_pointer_constant_row_matrix",
        pointer_nested_constant_row_index, 1u, 0, true, 20);
    verify_cxx_inline_automatic_pointer_indices_with_adjustment(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_nested_array_constant_row_entry",
        "debug_inline_automatic_pointer_constant_row_matrix",
        pointer_nested_constant_row_index, 1u, 0, true, 20);
    verify_cxx_inline_automatic_pointer_indices_with_adjustment(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_nested_array_constant_row_entry",
        "debug_inline_automatic_pointer_constant_row_matrix",
        pointer_nested_constant_row_index, 1u, 0, true, 20);
    verify_cxx_inline_automatic_pointer_indices_with_adjustment(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_nested_array_constant_column_entry",
        "debug_inline_automatic_pointer_constant_column_matrix",
        pointer_nested_constant_column_index, 1u, 0, true, 8);
    verify_cxx_inline_automatic_pointer_indices_with_adjustment(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_nested_array_constant_column_entry",
        "debug_inline_automatic_pointer_constant_column_matrix",
        pointer_nested_constant_column_index, 1u, 0, true, 8);
    verify_cxx_inline_automatic_pointer_indices_with_adjustment(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_nested_array_constant_column_entry",
        "debug_inline_automatic_pointer_constant_column_matrix",
        pointer_nested_constant_column_index, 1u, 0, true, 8);
    verify_cxx_inline_automatic_pointer_indices_with_adjustment(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_nested_array_constant_column_entry",
        "debug_inline_automatic_pointer_constant_column_matrix",
        pointer_nested_constant_column_index, 1u, 0, true, 8);
    verify_cxx_inline_automatic_pointer_indices(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_unsigned_computed_index_entry",
        "debug_inline_automatic_pointer_unsigned_computed_source",
        unsigned_computed_x86_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_unsigned_computed_index_entry",
        "debug_inline_automatic_pointer_unsigned_computed_source",
        unsigned_computed_x64_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_unsigned_computed_index_entry",
        "debug_inline_automatic_pointer_unsigned_computed_source",
        unsigned_computed_x86_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_unsigned_computed_index_entry",
        "debug_inline_automatic_pointer_unsigned_computed_source",
        unsigned_computed_x64_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_narrow_unsigned_computed_index_entry",
        "debug_inline_automatic_pointer_narrow_unsigned_computed_source",
        narrow_unsigned_computed_x86_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_narrow_unsigned_computed_index_entry",
        "debug_inline_automatic_pointer_narrow_unsigned_computed_source",
        narrow_unsigned_computed_x64_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_narrow_unsigned_computed_index_entry",
        "debug_inline_automatic_pointer_narrow_unsigned_computed_source",
        narrow_unsigned_computed_x86_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_narrow_unsigned_computed_index_entry",
        "debug_inline_automatic_pointer_narrow_unsigned_computed_source",
        narrow_unsigned_computed_x64_indices, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_narrow_unsigned_constant_index_entry",
        "debug_inline_automatic_pointer_narrow_unsigned_constant_source",
        narrow_unsigned_constant_index_x64_terms, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_narrow_unsigned_constant_index_entry",
        "debug_inline_automatic_pointer_narrow_unsigned_constant_source",
        narrow_unsigned_constant_index_x64_terms, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_large_unsigned_constant_index_entry",
        "debug_inline_automatic_pointer_large_unsigned_constant_source",
        large_unsigned_constant_index_x64_terms, 1u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_large_unsigned_constant_index_entry",
        "debug_inline_automatic_pointer_large_unsigned_constant_source",
        large_unsigned_constant_index_x64_terms, 1u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_many_unsigned_index_entry",
        "debug_inline_automatic_pointer_many_unsigned_source",
        many_unsigned_index_x86_terms, 17u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_many_unsigned_index_entry",
        "debug_inline_automatic_pointer_many_unsigned_source",
        many_unsigned_index_x64_terms, 17u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_many_unsigned_index_entry",
        "debug_inline_automatic_pointer_many_unsigned_source",
        many_unsigned_index_x86_terms, 17u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_many_unsigned_index_entry",
        "debug_inline_automatic_pointer_many_unsigned_source",
        many_unsigned_index_x64_terms, 17u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_scaled_unsigned_index_entry",
        "debug_inline_automatic_pointer_scaled_unsigned_source",
        scaled_unsigned_index_x86_terms, 3u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_scaled_unsigned_index_entry",
        "debug_inline_automatic_pointer_scaled_unsigned_source",
        scaled_unsigned_index_x64_terms, 3u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_scaled_unsigned_index_entry",
        "debug_inline_automatic_pointer_scaled_unsigned_source",
        scaled_unsigned_index_x86_terms, 3u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_scaled_unsigned_index_entry",
        "debug_inline_automatic_pointer_scaled_unsigned_source",
        scaled_unsigned_index_x64_terms, 3u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_scaled_signed_index_entry",
        "debug_inline_automatic_pointer_scaled_signed_source",
        scaled_signed_index_terms, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_scaled_signed_index_entry",
        "debug_inline_automatic_pointer_scaled_signed_source",
        scaled_signed_index_terms, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_scaled_signed_index_entry",
        "debug_inline_automatic_pointer_scaled_signed_source",
        scaled_signed_index_terms, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_scaled_signed_index_entry",
        "debug_inline_automatic_pointer_scaled_signed_source",
        scaled_signed_index_terms, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[40], ARCH_X86,
        "debug_inline_automatic_pointer_neg_scaled_signed_index_entry",
        "debug_inline_automatic_pointer_neg_scaled_signed_source",
        neg_scaled_signed_index_terms, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[41], ARCH_X64,
        "debug_inline_automatic_pointer_neg_scaled_signed_index_entry",
        "debug_inline_automatic_pointer_neg_scaled_signed_source",
        neg_scaled_signed_index_terms, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[42], ARCH_X86,
        "debug_inline_automatic_pointer_neg_scaled_signed_index_entry",
        "debug_inline_automatic_pointer_neg_scaled_signed_source",
        neg_scaled_signed_index_terms, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_indices(
        argv[43], ARCH_X64,
        "debug_inline_automatic_pointer_neg_scaled_signed_index_entry",
        "debug_inline_automatic_pointer_neg_scaled_signed_source",
        neg_scaled_signed_index_terms, 2u, 0, true);
    verify_cxx_inline_automatic_pointer_index(
        argv[40], ARCH_X86, "debug_inline_automatic_array_index_entry",
        "debug_inline_automatic_array_index_source",
        "debug_inline_automatic_array_index", 8, false, 4u, false, 4u,
        false);
    verify_cxx_inline_automatic_pointer_index(
        argv[41], ARCH_X64, "debug_inline_automatic_array_index_entry",
        "debug_inline_automatic_array_index_source",
        "debug_inline_automatic_array_index", 8, false, 4u, false, 4u,
        false);
    verify_cxx_inline_automatic_pointer_index(
        argv[42], ARCH_X86, "debug_inline_automatic_array_index_entry",
        "debug_inline_automatic_array_index_source",
        "debug_inline_automatic_array_index", 8, false, 4u, false, 4u,
        false);
    verify_cxx_inline_automatic_pointer_index(
        argv[43], ARCH_X64, "debug_inline_automatic_array_index_entry",
        "debug_inline_automatic_array_index_source",
        "debug_inline_automatic_array_index", 8, false, 4u, false, 4u,
        false);
    verify_cxx_inline_constexpr_parameter(
        argv[40], ARCH_X86, "debug_inline_constexpr_identifier_parameter",
        "debug_inline_constexpr_identifier_entry", 41);
    verify_cxx_inline_constexpr_parameter(
        argv[41], ARCH_X64, "debug_inline_constexpr_identifier_parameter",
        "debug_inline_constexpr_identifier_entry", 41);
    verify_cxx_inline_constexpr_parameter(
        argv[42], ARCH_X86, "debug_inline_constexpr_call_parameter",
        "debug_inline_constexpr_call_entry", 23);
    verify_cxx_inline_constexpr_parameter(
        argv[43], ARCH_X64, "debug_inline_constexpr_call_parameter",
        "debug_inline_constexpr_call_entry", 23);
    verify_cxx_inline_constexpr_parameter(
        argv[40], ARCH_X86, "debug_inline_constexpr_enum_parameter",
        "debug_inline_constexpr_enum_entry", -37);
    verify_cxx_inline_constexpr_parameter(
        argv[41], ARCH_X64, "debug_inline_constexpr_enum_parameter",
        "debug_inline_constexpr_enum_entry", -37);
    verify_cxx_inline_constexpr_parameter(
        argv[42], ARCH_X86, "debug_inline_constexpr_enum_parameter",
        "debug_inline_constexpr_enum_entry", -37);
    verify_cxx_inline_constexpr_parameter(
        argv[43], ARCH_X64, "debug_inline_constexpr_enum_parameter",
        "debug_inline_constexpr_enum_entry", -37);
    verify_class_template_parameter_dies(argv[26], ARCH_X86);
    verify_class_template_parameter_dies(argv[27], ARCH_X64);
    verify_class_template_template_parameter_die(argv[26], ARCH_X86);
    verify_class_template_template_parameter_die(argv[27], ARCH_X64);
    verify_class_template_template_parameter_pack_dies(
        argv[26], ARCH_X86);
    verify_class_template_template_parameter_pack_dies(
        argv[27], ARCH_X64);
    verify_class_template_parameter_pack_dies(argv[26], ARCH_X86);
    verify_class_template_parameter_pack_dies(argv[27], ARCH_X64);
    verify_class_non_type_template_parameter_pack_dies(
        argv[26], ARCH_X86, "DebugTemplateSignedValuePack.", "int", false,
        0u);
    verify_class_non_type_template_parameter_pack_dies(
        argv[27], ARCH_X64, "DebugTemplateSignedValuePack.", "int", false,
        0u);
    verify_class_non_type_template_parameter_pack_dies(
        argv[26], ARCH_X86, "DebugTemplateUnsignedValuePack.",
        "unsigned int", true, UINT32_C(0xf0000000));
    verify_class_non_type_template_parameter_pack_dies(
        argv[27], ARCH_X64, "DebugTemplateUnsignedValuePack.",
        "unsigned int", true, UINT32_C(0xf0000000));
    verify_class_dependent_non_type_template_parameter_pack_dies(
        argv[26], ARCH_X86);
    verify_class_dependent_non_type_template_parameter_pack_dies(
        argv[27], ARCH_X64);
    verify_class_nested_dependent_non_type_pack_die(
        argv[26], ARCH_X86, "DebugTemplateNestedValuePack.",
        "DebugTemplateNestedValueOwner");
    verify_class_nested_dependent_non_type_pack_die(
        argv[27], ARCH_X64, "DebugTemplateNestedValuePack.",
        "DebugTemplateNestedValueOwner");
    verify_class_nested_dependent_non_type_pack_die(
        argv[26], ARCH_X86, "DebugTemplateCompoundNestedValuePack.",
        "DebugTemplateCompoundValueOwner");
    verify_class_nested_dependent_non_type_pack_die(
        argv[27], ARCH_X64, "DebugTemplateCompoundNestedValuePack.",
        "DebugTemplateCompoundValueOwner");
    verify_zero_argument_non_type_template_pack_die(
        argv[26], ARCH_X86, "DebugTemplateEmptyNonTypePack.");
    verify_zero_argument_non_type_template_pack_die(
        argv[27], ARCH_X64, "DebugTemplateEmptyNonTypePack.");
    verify_function_template_parameter_dies(argv[26], ARCH_X86);
    verify_function_template_parameter_dies(argv[27], ARCH_X64);
    verify_dependent_compound_function_type(
        argv[26], ARCH_X86, "debug_template_dependent_pointer_type", 6u,
        false);
    verify_dependent_compound_function_type(
        argv[27], ARCH_X64, "debug_template_dependent_pointer_type", 6u,
        false);
    verify_dependent_compound_function_type(
        argv[26], ARCH_X86, "debug_template_dependent_reference_type", 37u,
        false);
    verify_dependent_compound_function_type(
        argv[27], ARCH_X64, "debug_template_dependent_reference_type", 37u,
        false);
    verify_dependent_compound_function_type(
        argv[26], ARCH_X86, "debug_template_dependent_member_pointer_type",
        57u, true);
    verify_dependent_compound_function_type(
        argv[27], ARCH_X64, "debug_template_dependent_member_pointer_type",
        57u, true);
    verify_dependent_function_parameter_pack(
        argv[26], ARCH_X86, "debug_template_dependent_pointer_pack", 6u,
        false, false);
    verify_dependent_function_parameter_pack(
        argv[27], ARCH_X64, "debug_template_dependent_pointer_pack", 6u,
        false, false);
    verify_dependent_function_parameter_pack(
        argv[26], ARCH_X86, "debug_template_dependent_reference_pack", 37u,
        false, false);
    verify_dependent_function_parameter_pack(
        argv[27], ARCH_X64, "debug_template_dependent_reference_pack", 37u,
        false, false);
    verify_dependent_function_parameter_pack(
        argv[26], ARCH_X86,
        "debug_template_dependent_const_reference_pack", 37u,
        false, true);
    verify_dependent_function_parameter_pack(
        argv[27], ARCH_X64,
        "debug_template_dependent_const_reference_pack", 37u,
        false, true);
    verify_dependent_function_parameter_pack(
        argv[26], ARCH_X86,
        "debug_template_dependent_member_pointer_pack", 57u, false, false);
    verify_dependent_function_parameter_pack(
        argv[27], ARCH_X64,
        "debug_template_dependent_member_pointer_pack", 57u, false, false);
    verify_dependent_function_parameter_pack(
        argv[26], ARCH_X86, "debug_template_dependent_value_pack", 5u,
        false, false);
    verify_dependent_function_parameter_pack(
        argv[27], ARCH_X64, "debug_template_dependent_value_pack", 5u,
        false, false);
    verify_dependent_function_parameter_pack(
        argv[26], ARCH_X86,
        "debug_template_dependent_array_reference_pack", 37u, true, false);
    verify_dependent_function_parameter_pack(
        argv[27], ARCH_X64,
        "debug_template_dependent_array_reference_pack", 37u, true, false);
    verify_dependent_function_parameter_pack(
        argv[26], ARCH_X86,
        "debug_template_dependent_array_pointer_pack", 6u, false, false);
    verify_dependent_function_parameter_pack(
        argv[27], ARCH_X64,
        "debug_template_dependent_array_pointer_pack", 6u, false, false);
    verify_function_template_template_parameter_dies(argv[26], ARCH_X86);
    verify_function_template_template_parameter_dies(argv[27], ARCH_X64);
    verify_function_template_nontype_signature_die(argv[26], ARCH_X86);
    verify_function_template_nontype_signature_die(argv[27], ARCH_X64);
    verify_function_template_type_pack_dies(argv[26], ARCH_X86);
    verify_function_template_type_pack_dies(argv[27], ARCH_X64);
    verify_function_template_signed_pack_dies(argv[26], ARCH_X86);
    verify_function_template_signed_pack_dies(argv[27], ARCH_X64);
    verify_function_template_unsigned_pack_die(argv[26], ARCH_X86);
    verify_function_template_unsigned_pack_die(argv[27], ARCH_X64);
    verify_function_template_dependent_value_pack_dies(
        argv[26], ARCH_X86);
    verify_function_template_dependent_value_pack_dies(
        argv[27], ARCH_X64);
    verify_function_template_nested_dependent_value_pack_die(
        argv[26], ARCH_X86,
        "debug_template_function_nested_value_pack",
        "DebugTemplateNestedValueOwner");
    verify_function_template_nested_dependent_value_pack_die(
        argv[27], ARCH_X64,
        "debug_template_function_nested_value_pack",
        "DebugTemplateNestedValueOwner");
    verify_function_template_nested_dependent_value_pack_die(
        argv[26], ARCH_X86,
        "debug_template_function_compound_nested_value_pack",
        "DebugTemplateCompoundValueOwner");
    verify_function_template_nested_dependent_value_pack_die(
        argv[27], ARCH_X64,
        "debug_template_function_compound_nested_value_pack",
        "DebugTemplateCompoundValueOwner");
    verify_class_nested_dependent_scalar_value_die(argv[26], ARCH_X86);
    verify_class_nested_dependent_scalar_value_die(argv[27], ARCH_X64);
    verify_function_template_nested_dependent_scalar_value_die(
        argv[26], ARCH_X86);
    verify_function_template_nested_dependent_scalar_value_die(
        argv[27], ARCH_X64);
    verify_enum_template_value_dies(argv[26], ARCH_X86);
    verify_enum_template_value_dies(argv[27], ARCH_X64);
    verify_verified_fallback_debug_object(argv[28], ARCH_X86);
    verify_verified_fallback_debug_object(argv[29], ARCH_X64);
    verify_large_file_table(argv[34], ARCH_X86, "debug_info_file_259.c");
    verify_large_file_table(argv[35], ARCH_X64, "debug_info_file_259.c");
    verify_static_reference_debug_object(argv[36], ARCH_X86);
    verify_static_reference_debug_object(argv[37], ARCH_X64);
    return 0;
}
