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

    for (uint64_t die = 11u; die + 1u + 4u + address_size + 4u + 1u +
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
            info->data, die + 1u + 4u + address_size + 4u + 1u + 4u +
                       4u + 1u);
        assert(linkage_name_offset < strings->size);
        assert(strcmp((const char*)strings->data + linkage_name_offset,
                      "read") != 0);
        assert(strings->data[linkage_name_offset] == '_');
        cursor = die + 1u + 4u + address_size + 4u + 1u + 4u + 4u +
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

    for (uint64_t die = 11u; die + 1u + 4u + address_size + 4u + 1u +
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
        cursor = die + 1u + 4u + address_size + 4u + 1u + 4u + 4u + 1u;
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

        cursor = die + 1u + 4u + address_size + 4u + 1u + 4u + 4u + 1u +
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
        line_offset = file_offset + 1u;
        column_offset = line_offset + 4u;
        child_offset = column_offset + 4u;
        range_end = child_offset + 5u;
        if (range_end > info->size ||
            (!ranged && read_u32(info->data, range_offset) == 0u) ||
            info->data[file_offset] == 0u ||
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
        child_offset = range_offset + 4u + 1u + 4u + 4u;
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
            abbreviation != 39u) continue;
        name_offset = read_u32(info->data, die + 1u);
        if (name_offset >= strings->size ||
            strcmp((const char*)strings->data + name_offset,
                   variable_name) != 0) continue;
        has_expression = abbreviation == 3u || abbreviation == 4u ||
            abbreviation == 27u;
        *is_location_list = abbreviation == 39u;
        *has_location = has_expression || *is_location_list;
        *frame_offset = 0;
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
        assert(expression_size >= 2u &&
               expression_size <= info->size - expression_cursor);
        expression_end = expression_cursor + expression_size;
        assert(info->data[expression_cursor] == frame_register);
        expression_cursor++;
        *frame_offset = read_sleb(
            info->data, expression_end,
            &expression_cursor);
        assert(expression_cursor == expression_end ||
               (expression_cursor + 1u == expression_end &&
                info->data[expression_cursor] == 0x06u) ||
               strcmp(variable_name, "aligned") == 0);
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
    assert(info != NULL && strings != NULL && frame != NULL &&
           locations != NULL);
    verify_multiple_return_frame_fde(frame, architecture);
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
    {
        int64_t loop_start;
        int64_t loop_end;
        int64_t outer_start;
        int64_t outer_end;
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
    ObjectFile* object = objfile_read(path);
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    ObjSection* line;
    uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
    uint64_t outer_die = UINT64_MAX;
    uint64_t inner_die = UINT64_MAX;
    uint64_t outer_start = 0u;
    uint64_t outer_end = 0u;
    uint64_t inner_start = 0u;
    uint64_t inner_end = 0u;

    assert(object != NULL && object->arch == architecture);
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    line = objfile_get_section(object, ".debug_line");
    assert(info != NULL && abbrev != NULL && strings != NULL && line != NULL);
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
            strcmp(inline_name, "debug_nested_inline") != 0) {
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
        assert(parameter_type < info->size &&
               info->data[parameter_type] == 5u);
        assert(parameter_file > 0u &&
               read_u32(info->data, parameter_die + 13u) > 0u &&
               read_u32(info->data, parameter_die + 17u) > 0u);
        assert(line_table_file_matches(line, parameter_file,
                                       "tests/debug_info_inline.h"));
        assert(info->data[parameter_die + 21u] == 0u);

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
            strcmp(inline_name, "debug_declared_inline") == 0
                ? "tests/debug_info.c"
                : "tests/debug_info_inline.h"));
        assert(line_table_file_matches(line, origin_file,
                                       "tests/debug_info_inline.h"));

        for (const ObjReloc* relocation = info->relocs; relocation;
             relocation = relocation->next) {
            bool root_call;
            if (relocation->offset != low_pc_offset) continue;
            assert(relocation->symbol_name != NULL);
            root_call = strcmp(relocation->symbol_name,
                               "debug_inline_entry") == 0;
            assert(root_call ||
                   (strcmp(relocation->symbol_name,
                           "debug_declared_inline") == 0 &&
                    strcmp(inline_name,
                           "debug_nested_inline") == 0));
            assert(relocation->addend > 0);
            has_call_relocation = true;
            if (root_call &&
                strcmp(inline_name, "debug_declared_inline") == 0 &&
                outer_die == UINT64_MAX) {
                outer_die = die;
                outer_start = (uint64_t)relocation->addend;
                outer_end = outer_start + call_length;
            } else if (root_call &&
                       strcmp(inline_name, "debug_nested_inline") == 0 &&
                       inner_die == UINT64_MAX) {
                inner_die = die;
                inner_start = (uint64_t)relocation->addend;
                inner_end = inner_start + call_length;
            }
            break;
        }
        assert(has_call_relocation);
    }
    assert(outer_die != UINT64_MAX && inner_die != UINT64_MAX);
    assert(outer_start <= inner_start && inner_end <= outer_end &&
           inner_end < outer_end);
    assert(inner_die == outer_die + 1u + 4u + address_size + 4u + 12u);
    assert(info->data[inner_die + 1u + 4u + address_size + 4u + 12u] == 0u);
    assert(info->data[inner_die + 1u + 4u + address_size + 4u + 13u] == 0u);
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
                                             uint16_t architecture)
{
    uint32_t cie_length = read_u32(frame->data, 0u);
    uint64_t fde_offset = 4u + cie_length;
    uint32_t fde_length = read_u32(frame->data, fde_offset);
    uint64_t instruction_offset = fde_offset + 8u +
        (architecture == ARCH_X64 ? 16u : 8u);
    uint64_t instruction_end = fde_offset + 4u + fde_length;
    uint8_t frame_register = architecture == ARCH_X64 ? 6u : 5u;
    uint8_t stack_register = architecture == ARCH_X64 ? 7u : 4u;
    uint8_t pointer_size = architecture == ARCH_X64 ? 8u : 4u;
    assert(count_epilogue_cfa_restores(
               frame, instruction_offset, instruction_end, stack_register,
               frame_register, pointer_size) == 3u);
    assert(count_frame_body_restores(
               frame, instruction_offset, instruction_end, frame_register,
               pointer_size) == 2u);
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

static uint8_t read_prototyped_attribute(const ObjSection* info,
                                         const ObjSection* strings,
                                         const char* function_name,
                                         uint64_t address_size)
{
    uint64_t function_offset = find_function_die(
        info, strings, function_name, address_size);
    uint64_t offset;
    assert(function_offset != UINT64_MAX);
    offset = function_offset + 1u + 4u + address_size + 4u + 1u + 4u +
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
        verify_multiple_return_frame_fde(frame, architecture);
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
    assert(file != NULL);
    assert(fread(&header, sizeof(header), 1u, file) == 1u);
    assert(header.magic == RIN_IMAGE_MAGIC &&
           header.version == RIN_IMAGE_VERSION_3 &&
           header.architecture == architecture);
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
    }
    free(sections);
    assert(fclose(file) == 0);
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
    verify_multiple_return_frame_fde(frame, architecture);
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
    {
        int64_t loop_start;
        int64_t loop_end;
        int64_t outer_start;
        int64_t outer_end;
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

int main(int argc, char** argv)
{
    assert(argc == 26);
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
    verify_static_member_containing_type(argv[14], ARCH_X86);
    verify_static_member_containing_type(argv[15], ARCH_X64);
    verify_cxx_member_accessibility(argv[14], ARCH_X86);
    verify_cxx_member_accessibility(argv[15], ARCH_X64);
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
    return 0;
}
