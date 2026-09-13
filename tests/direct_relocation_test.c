#include "objfile.h"
#include "rin_formats_v3.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ObjSymbol* required_symbol(ObjectFile* object, const char* name)
{
    ObjSymbol* symbol = objfile_find_symbol(object, name);
    assert(symbol != NULL);
    assert(symbol->section >= 0);
    return symbol;
}

static uint64_t required_bytes(const uint8_t* data, uint64_t size,
                               const char* value)
{
    size_t value_size = strlen(value) + 1u;
    assert(value_size <= size);
    for (uint64_t offset = 0u; offset + value_size <= size; ++offset) {
        if (memcmp(data + offset, value, value_size) == 0) return offset;
    }
    assert(!"required byte sequence was not found");
    return 0u;
}

static int contains_bytes(const uint8_t* data, uint64_t size,
                          const uint8_t* pattern, size_t pattern_size)
{
    if (!data || !pattern || pattern_size > size) return 0;
    for (uint64_t offset = 0u; offset + pattern_size <= size; ++offset) {
        if (memcmp(data + offset, pattern, pattern_size) == 0) return 1;
    }
    return 0;
}

static uint64_t read_little_endian(const uint8_t* data, size_t size)
{
    uint64_t value = 0u;
    assert(size <= sizeof(value));
    for (size_t byte = 0u; byte < size; ++byte) {
        value |= (uint64_t)data[byte] << (byte * 8u);
    }
    return value;
}

static ObjSection* required_section(ObjectFile* object, SectionType type)
{
    for (ObjSection* section = object->sections; section;
         section = section->next) {
        if (section->type == type) return section;
    }
    assert(!"required object section was not found");
    return NULL;
}

static uint64_t symbol_extent(ObjectFile* object, ObjSymbol* symbol,
                              uint64_t section_size)
{
    uint64_t end = section_size;
    assert(symbol->value < section_size);
    for (ObjSymbol* candidate = object->symbols; candidate;
         candidate = candidate->next) {
        if (candidate->section == symbol->section &&
            candidate->value > symbol->value && candidate->value < end) {
            end = candidate->value;
        }
    }
    return end - symbol->value;
}

static void verify_artifact(const char* object_path, const char* image_path,
                            uint16_t expected_architecture,
                            uint32_t expected_magic)
{
    ObjectFile* object = objfile_read(object_path);
    ObjSymbol* first;
    ObjSymbol* second;
    ObjSymbol* zero;
    ObjSymbol* target;
    ObjSymbol* static_literal;
    ObjSymbol* static_suffix;
    ObjSymbol* static_zero;
    ObjSymbol* static_values;
    ObjSymbol* static_second;
    ObjSymbol* static_target;
    ObjSymbol* static_null;
    ObjSymbol* static_array;
    ObjSymbol* static_fixed;
    ObjSymbol* static_constant;
    ObjSymbol* static_logic;
    ObjSymbol* static_bits;
    ObjSymbol* static_choice;
    ObjSymbol* static_bool;
    ObjSymbol* static_unary;
    ObjSymbol* aggregate_target;
    ObjSymbol* aggregate_scalar;
    ObjSymbol* aggregate_braced_string;
    ObjSymbol* aggregate_numbers;
    ObjSymbol* aggregate_pointers;
    ObjSymbol* aggregate_record;
    ObjSymbol* aggregate_nested;
    ObjSymbol* aggregate_path;
    ObjSymbol* aggregate_union;
    ObjSymbol* pointer_add;
    ObjSymbol* integer_add;
    ObjSymbol* pointer_distance;
    ObjSymbol* pointer_update;
    ObjSymbol* local_array_value;
    ObjSymbol* large_local_array_value;
    ObjSection* object_code = NULL;
    ObjSection* object_rodata = NULL;
    ObjSection* object_data = NULL;
    RinHeaderV3 header;
    RinDriverHeaderV3 driver_header;
    RinSectionV3* sections;
    RinSectionV3* code = NULL;
    RinSectionV3* rodata = NULL;
    RinSectionV3* data = NULL;
    RinSectionV3* bss = NULL;
    RinSectionV3* relocation_section = NULL;
    RinRelocationV3* relocations;
    FILE* image;
    size_t relocation_count;
    uint32_t section_count;
    uint64_t section_table_offset;
    uint64_t preferred_base = 0u;
    int saw_data_symbol = 0;
    int saw_rodata_symbol = 0;
    int saw_bss_symbol = 0;
    int saw_code_symbol = 0;
    int saw_static_literal = 0;
    int saw_static_suffix = 0;
    int saw_static_zero = 0;
    int saw_static_second = 0;
    int saw_static_target = 0;
    int saw_aggregate_pointer_first = 0;
    int saw_aggregate_pointer_third = 0;
    int saw_aggregate_record_pointer = 0;
    int saw_aggregate_nested_pointer = 0;
    int saw_aggregate_path_pointer = 0;
    size_t data_source_relocations = 0u;
    size_t pointer_width = expected_architecture == RIN_ARCH_X86_64 ? 8u : 4u;
    uint64_t static_literal_offset;
    uint64_t image_rodata_rva;

    assert(object != NULL);
    first = required_symbol(object, "first_value");
    second = required_symbol(object, "second_value");
    zero = required_symbol(object, "zero_value");
    target = required_symbol(object, "target");
    static_literal = required_symbol(object, "static_literal");
    static_suffix = required_symbol(object, "static_suffix");
    static_zero = required_symbol(object, "static_zero");
    static_values = required_symbol(object, "static_values");
    static_second = required_symbol(object, "static_second");
    static_target = required_symbol(object, "static_target");
    static_null = required_symbol(object, "static_null");
    static_array = required_symbol(object, "static_array");
    static_fixed = required_symbol(object, "static_fixed");
    static_constant = required_symbol(object, "static_constant");
    static_logic = required_symbol(object, "static_logic");
    static_bits = required_symbol(object, "static_bits");
    static_choice = required_symbol(object, "static_choice");
    static_bool = required_symbol(object, "static_bool");
    static_unary = required_symbol(object, "static_unary");
    aggregate_target = required_symbol(object, "aggregate_target");
    aggregate_scalar = required_symbol(object, "aggregate_scalar");
    aggregate_braced_string = required_symbol(
        object, "aggregate_braced_string");
    aggregate_numbers = required_symbol(object, "aggregate_numbers");
    aggregate_pointers = required_symbol(object, "aggregate_pointers");
    aggregate_record = required_symbol(object, "aggregate_record");
    aggregate_nested = required_symbol(object, "aggregate_nested");
    aggregate_path = required_symbol(object, "aggregate_path");
    aggregate_union = required_symbol(object, "aggregate_union");
    pointer_add = required_symbol(object, "pointer_add");
    integer_add = required_symbol(object, "integer_add");
    pointer_distance = required_symbol(object, "pointer_distance");
    pointer_update = required_symbol(object, "pointer_update");
    local_array_value = required_symbol(object, "local_array_value");
    large_local_array_value = required_symbol(object,
                                               "large_local_array_value");
    assert(first->section == second->section);
    assert(first->value != second->value);
    assert(zero->binding == BIND_BSS);
    assert(zero->section != first->section);
    for (ObjSection* section = object->sections; section;
         section = section->next) {
        if (section->type == SECT_RODATA) object_rodata = section;
        if (section->type == SECT_DATA) object_data = section;
        if (section->type == SECT_CODE) object_code = section;
    }
    assert(object_code != NULL && object_rodata != NULL && object_data != NULL);
    assert(object_rodata->flags == SECT_FLAG_ALLOC);
    assert(static_array->value + 9u <= object_data->size);
    assert(memcmp(object_data->data + static_array->value,
                  "ArrayRin", 9u) == 0);
    assert(static_fixed->value + 12u <= object_data->size);
    assert(memcmp(object_data->data + static_fixed->value, "Fixed", 6u) == 0);
    for (uint64_t byte = 6u; byte < 12u; ++byte) {
        assert(object_data->data[static_fixed->value + byte] == 0u);
    }
    assert(static_constant->value + 4u <= object_data->size);
    assert(object_data->data[static_constant->value] == 30u);
    assert(object_data->data[static_constant->value + 1u] == 0u);
    assert(static_logic->value + 4u <= object_data->size);
    assert(read_little_endian(object_data->data + static_logic->value, 4u) ==
           1u);
    assert(static_bits->value + 4u <= object_data->size);
    assert(read_little_endian(object_data->data + static_bits->value, 4u) ==
           55u);
    assert(static_choice->value + 4u <= object_data->size);
    assert(read_little_endian(object_data->data + static_choice->value, 4u) ==
           4u);
    assert(static_bool->value + 1u <= object_data->size);
    assert(object_data->data[static_bool->value] == 1u);
    assert(static_unary->value + 4u <= object_data->size);
    assert(read_little_endian(object_data->data + static_unary->value, 4u) ==
           UINT32_C(0xfffffffc));
    assert(aggregate_scalar->value + 4u <= object_data->size);
    assert(read_little_endian(object_data->data + aggregate_scalar->value,
                              4u) == 13u);
    assert(aggregate_braced_string->value + 6u <= object_data->size);
    assert(memcmp(object_data->data + aggregate_braced_string->value,
                  "Brace", 6u) == 0);
    assert(aggregate_numbers->value + 20u <= object_data->size);
    assert(read_little_endian(object_data->data + aggregate_numbers->value,
                              4u) == 1u);
    assert(read_little_endian(object_data->data + aggregate_numbers->value +
                                  4u,
                              4u) == 0u);
    assert(read_little_endian(object_data->data + aggregate_numbers->value +
                                  12u,
                              4u) == 7u);
    assert(read_little_endian(object_data->data + aggregate_numbers->value +
                                  16u,
                              4u) == 9u);
    assert(aggregate_record->value + 8u + pointer_width <=
           object_data->size);
    assert(read_little_endian(object_data->data + aggregate_record->value,
                              4u) == 12u);
    assert(object_data->data[aggregate_record->value + 4u] == 'R');
    assert(aggregate_nested->value + 24u + pointer_width <=
           object_data->size);
    assert(read_little_endian(object_data->data + aggregate_nested->value,
                              4u) == 1u);
    assert(read_little_endian(object_data->data + aggregate_nested->value +
                                  12u,
                              4u) == 4u);
    assert(read_little_endian(object_data->data + aggregate_nested->value +
                                  16u,
                              4u) == 5u);
    assert(object_data->data[aggregate_nested->value + 20u] == 'N');
    assert(aggregate_path->value + 24u + pointer_width <=
           object_data->size);
    assert(read_little_endian(object_data->data + aggregate_path->value + 8u,
                              4u) == 17u);
    assert(aggregate_union->value + 4u <= object_data->size);
    assert(read_little_endian(object_data->data + aggregate_union->value,
                              4u) == 6u);
    static_literal_offset = required_bytes(object_rodata->data,
                                           object_rodata->size,
                                           "StaticRinOS");
    (void)required_bytes(object_rodata->data, object_rodata->size, "RinOS");
    {
        uint16_t object_architecture = expected_architecture == RIN_ARCH_X86_64
            ? ARCH_X64 : ARCH_X86;
        RelocType expected_type = object_architecture == ARCH_X64
            ? RELOC_ABS64 : RELOC_ABS32U;
        size_t relocation_width = object_architecture == ARCH_X64 ? 8u : 4u;
        for (ObjReloc* relocation = object_data->relocs; relocation;
             relocation = relocation->next) {
            assert(relocation->section == static_literal->section);
            assert(relocation->type == expected_type);
            assert(relocation->offset + relocation_width <= object_data->size);
            if (relocation->offset == static_literal->value) {
                assert(strstr(relocation->symbol_name,
                              "__rcc_rodata_base") != NULL);
                assert(relocation->addend == (int64_t)static_literal_offset);
                saw_static_literal = 1;
            } else if (relocation->offset == static_suffix->value) {
                assert(strstr(relocation->symbol_name,
                              "__rcc_rodata_base") != NULL);
                assert(relocation->addend ==
                       (int64_t)static_literal_offset + 6);
                saw_static_suffix = 1;
            } else if (relocation->offset == static_zero->value) {
                assert(strcmp(relocation->symbol_name, "zero_value") == 0);
                assert(relocation->addend == 0);
                saw_static_zero = 1;
            } else if (relocation->offset == static_second->value) {
                assert(strcmp(relocation->symbol_name, "static_values") == 0);
                assert(relocation->addend == 4);
                saw_static_second = 1;
            } else if (relocation->offset == static_target->value) {
                assert(strcmp(relocation->symbol_name, "target") == 0);
                assert(relocation->addend == 0);
                saw_static_target = 1;
            } else if (relocation->offset == aggregate_pointers->value) {
                assert(strcmp(relocation->symbol_name,
                              "aggregate_target") == 0);
                assert(relocation->addend == 0);
                saw_aggregate_pointer_first = 1;
            } else if (relocation->offset == aggregate_pointers->value +
                                                 2u * pointer_width) {
                assert(strcmp(relocation->symbol_name,
                              "aggregate_target") == 0);
                assert(relocation->addend == 0);
                saw_aggregate_pointer_third = 1;
            } else if (relocation->offset == aggregate_record->value + 8u) {
                assert(strcmp(relocation->symbol_name,
                              "aggregate_target") == 0);
                assert(relocation->addend == 0);
                saw_aggregate_record_pointer = 1;
            } else if (relocation->offset == aggregate_nested->value + 24u) {
                assert(strcmp(relocation->symbol_name,
                              "aggregate_target") == 0);
                assert(relocation->addend == 0);
                saw_aggregate_nested_pointer = 1;
            } else if (relocation->offset == aggregate_path->value + 24u) {
                assert(strcmp(relocation->symbol_name,
                              "aggregate_target") == 0);
                assert(relocation->addend == 0);
                saw_aggregate_path_pointer = 1;
            }
        }
        assert(saw_static_literal && saw_static_suffix && saw_static_zero &&
               saw_static_second && saw_static_target);
        assert(saw_aggregate_pointer_first && saw_aggregate_pointer_third &&
               saw_aggregate_record_pointer &&
               saw_aggregate_nested_pointer && saw_aggregate_path_pointer);
    }
    if (expected_architecture == RIN_ARCH_X86_64) {
        static const uint8_t large_stack_frame[] = {
            0x55, 0x48, 0x89, 0xe5, 0x48, 0x81, 0xec,
            0x40, 0x01, 0x00, 0x00,
        };
        static const uint8_t signed_char_load[] = {0x48, 0x0f, 0xbe};
        static const uint8_t scale_left[] = {
            0x48, 0xc7, 0xc2, 0x04, 0x00, 0x00, 0x00,
            0x48, 0x0f, 0xaf, 0xc2,
        };
        static const uint8_t scale_right[] = {
            0x48, 0xc7, 0xc2, 0x04, 0x00, 0x00, 0x00,
            0x48, 0x0f, 0xaf, 0xca,
        };
        static const uint8_t pointer_difference[] = {
            0x48, 0xc7, 0xc1, 0x04, 0x00, 0x00, 0x00,
            0x48, 0x99, 0x48, 0xf7, 0xf9,
        };
        static const uint8_t post_increment[] = {0x48, 0x83, 0xc2, 0x04};
        static const uint8_t pre_increment[] = {0x48, 0x83, 0xc0, 0x04};
        assert(contains_bytes(
            object_code->data + large_local_array_value->value,
            symbol_extent(object, large_local_array_value, object_code->size),
            large_stack_frame, sizeof(large_stack_frame)));
        assert(contains_bytes(
            object_code->data + local_array_value->value,
            symbol_extent(object, local_array_value, object_code->size),
            signed_char_load, sizeof(signed_char_load)));
        assert(contains_bytes(object_code->data + integer_add->value,
                              symbol_extent(object, integer_add,
                                            object_code->size),
                              scale_left, sizeof(scale_left)));
        assert(contains_bytes(object_code->data + pointer_add->value,
                              symbol_extent(object, pointer_add,
                                            object_code->size),
                              scale_right, sizeof(scale_right)));
        assert(contains_bytes(object_code->data + pointer_distance->value,
                              symbol_extent(object, pointer_distance,
                                            object_code->size),
                              pointer_difference,
                              sizeof(pointer_difference)));
        assert(contains_bytes(object_code->data + pointer_update->value,
                              symbol_extent(object, pointer_update,
                                            object_code->size),
                              scale_left, sizeof(scale_left)));
        assert(contains_bytes(object_code->data + pointer_update->value,
                              symbol_extent(object, pointer_update,
                                            object_code->size),
                              post_increment, sizeof(post_increment)));
        assert(contains_bytes(object_code->data + pointer_update->value,
                              symbol_extent(object, pointer_update,
                                            object_code->size),
                              pre_increment, sizeof(pre_increment)));
    } else {
        static const uint8_t large_stack_frame[] = {
            0x55, 0x89, 0xe5, 0x81, 0xec, 0x40, 0x01, 0x00, 0x00,
        };
        static const uint8_t signed_char_load[] = {0x0f, 0xbe};
        static const uint8_t scale_left[] = {
            0xba, 0x04, 0x00, 0x00, 0x00, 0x0f, 0xaf, 0xc2,
        };
        static const uint8_t scale_right[] = {
            0xba, 0x04, 0x00, 0x00, 0x00, 0x0f, 0xaf, 0xca,
        };
        static const uint8_t pointer_difference[] = {
            0xb9, 0x04, 0x00, 0x00, 0x00, 0x99, 0xf7, 0xf9,
        };
        static const uint8_t post_increment[] = {0x83, 0xc2, 0x04};
        static const uint8_t pre_increment[] = {0x83, 0xc0, 0x04};
        assert(contains_bytes(
            object_code->data + large_local_array_value->value,
            symbol_extent(object, large_local_array_value, object_code->size),
            large_stack_frame, sizeof(large_stack_frame)));
        assert(contains_bytes(
            object_code->data + local_array_value->value,
            symbol_extent(object, local_array_value, object_code->size),
            signed_char_load, sizeof(signed_char_load)));
        assert(contains_bytes(object_code->data + integer_add->value,
                              symbol_extent(object, integer_add,
                                            object_code->size),
                              scale_left, sizeof(scale_left)));
        assert(contains_bytes(object_code->data + pointer_add->value,
                              symbol_extent(object, pointer_add,
                                            object_code->size),
                              scale_right, sizeof(scale_right)));
        assert(contains_bytes(object_code->data + pointer_distance->value,
                              symbol_extent(object, pointer_distance,
                                            object_code->size),
                              pointer_difference,
                              sizeof(pointer_difference)));
        assert(contains_bytes(object_code->data + pointer_update->value,
                              symbol_extent(object, pointer_update,
                                            object_code->size),
                              scale_left, sizeof(scale_left)));
        assert(contains_bytes(object_code->data + pointer_update->value,
                              symbol_extent(object, pointer_update,
                                            object_code->size),
                              post_increment, sizeof(post_increment)));
        assert(contains_bytes(object_code->data + pointer_update->value,
                              symbol_extent(object, pointer_update,
                                            object_code->size),
                              pre_increment, sizeof(pre_increment)));
    }

    saw_static_literal = 0;
    saw_static_suffix = 0;
    saw_static_zero = 0;
    saw_static_second = 0;
    saw_static_target = 0;
    saw_aggregate_pointer_first = 0;
    saw_aggregate_pointer_third = 0;
    saw_aggregate_record_pointer = 0;
    saw_aggregate_nested_pointer = 0;
    saw_aggregate_path_pointer = 0;

    image = fopen(image_path, "rb");
    assert(image != NULL);
    if (expected_magic == RIN_IMAGE_MAGIC) {
        assert(fread(&header, sizeof(header), 1u, image) == 1u);
        assert(header.magic == expected_magic);
        assert(header.version == RIN_IMAGE_VERSION_3);
        assert(header.architecture == expected_architecture);
        section_count = header.section_count;
        section_table_offset = header.section_table_offset;
        preferred_base = header.preferred_base;
    } else {
        assert(expected_magic == RIN_DRIVER_IMAGE_MAGIC);
        assert(fread(&driver_header, sizeof(driver_header), 1u, image) == 1u);
        assert(driver_header.magic == expected_magic);
        assert(driver_header.version == RIN_DRIVER_IMAGE_VERSION_3);
        assert(driver_header.architecture == expected_architecture);
        section_count = driver_header.section_count;
        section_table_offset = driver_header.section_table_offset;
    }
    sections = calloc(section_count, sizeof(*sections));
    assert(sections != NULL);
    assert(fseek(image, (long)section_table_offset, SEEK_SET) == 0);
    assert(fread(sections, sizeof(*sections), section_count, image) ==
           section_count);
    for (uint32_t index = 0u; index < section_count; ++index) {
        if (sections[index].type == RIN_IMAGE_SECTION_CODE) code = &sections[index];
        if (sections[index].type == RIN_IMAGE_SECTION_RODATA) rodata = &sections[index];
        if (sections[index].type == RIN_IMAGE_SECTION_DATA) data = &sections[index];
        if (sections[index].type == RIN_IMAGE_SECTION_BSS) bss = &sections[index];
        if (sections[index].type == RIN_IMAGE_SECTION_RELOCATIONS) {
            relocation_section = &sections[index];
        }
    }
    assert(code != NULL && data != NULL && bss != NULL &&
           relocation_section != NULL);
    if (rodata != NULL) {
        assert(rodata->flags == RIN_IMAGE_SECTION_READ);
        assert(rodata->file_size == object_rodata->size &&
               rodata->memory_size == object_rodata->size);
        assert(rodata->virtual_address != data->virtual_address);
        image_rodata_rva = rodata->virtual_address;
    } else {
        uint64_t alignment = preferred_base == 0u
            ? 16u : object_rodata->align;
        image_rodata_rva = (object_code->size + alignment - 1u) &
                           ~(alignment - 1u);
        assert(expected_magic == RIN_IMAGE_MAGIC);
        assert(image_rodata_rva + object_rodata->size <= code->file_size);
    }
    if (expected_magic == RIN_IMAGE_MAGIC) {
        assert(rodata != NULL);
        assert(code->virtual_address == 0u &&
               code->file_size == object_code->size &&
               code->memory_size <= rodata->virtual_address &&
               (preferred_base == 0u
                    ? code->memory_size == rodata->virtual_address
                    : code->memory_size == object_code->size) &&
               rodata->virtual_address >= code->memory_size &&
               (rodata->virtual_address & (rodata->alignment - 1u)) == 0u &&
               rodata->file_offset >= code->file_offset + code->file_size &&
               (rodata->file_offset & (rodata->alignment - 1u)) == 0u &&
               data->virtual_address >= rodata->virtual_address +
                   rodata->memory_size &&
               (data->virtual_address & (data->alignment - 1u)) == 0u &&
               data->file_offset >= rodata->file_offset + rodata->file_size &&
               data->file_size == data->memory_size &&
               bss->virtual_address == data->virtual_address +
                   data->memory_size &&
               (bss->virtual_address & 4095u) == 0u &&
               header.image_size == bss->virtual_address +
                   bss->memory_size);
    }
    assert(bss->file_offset == 0u && bss->file_size == 0u);
    assert(bss->memory_size >= zero->value + 4u);
    assert(relocation_section->file_size % sizeof(RinRelocationV3) == 0u);
    relocation_count = (size_t)(relocation_section->file_size /
                                sizeof(RinRelocationV3));
    assert(relocation_count >= 2u);
    relocations = calloc(relocation_count, sizeof(*relocations));
    assert(relocations != NULL);
    assert(fseek(image, (long)relocation_section->file_offset, SEEK_SET) == 0);
    assert(fread(relocations, sizeof(*relocations), relocation_count, image) ==
           relocation_count);

    for (size_t index = 0u; index < relocation_count; ++index) {
        uint64_t value = 0u;
        RinSectionV3* source = NULL;
        uint64_t source_offset;
        size_t width = expected_architecture == RIN_ARCH_X86_64 ? 8u : 4u;
        uint16_t expected_type = expected_architecture == RIN_ARCH_X86_64
            ? RIN_IMAGE_RELOCATION_ABS64 : RIN_IMAGE_RELOCATION_ABS32U;
        assert(relocations[index].type == expected_type);
        if (relocations[index].virtual_address >= code->virtual_address &&
            relocations[index].virtual_address + width <=
                code->virtual_address + code->file_size) {
            source = code;
        } else if (rodata != NULL &&
                   relocations[index].virtual_address >=
                       rodata->virtual_address &&
                   relocations[index].virtual_address + width <=
                       rodata->virtual_address + rodata->file_size) {
            source = rodata;
        } else if (relocations[index].virtual_address >=
                       data->virtual_address &&
                   relocations[index].virtual_address + width <=
                       data->virtual_address + data->file_size) {
            source = data;
        }
        assert(source != NULL);
        source_offset = relocations[index].virtual_address -
                        source->virtual_address;
        assert(fseek(image, (long)(source->file_offset + source_offset),
                     SEEK_SET) == 0);
        assert(fread(&value, width, 1u, image) == 1u);
        if (value == preferred_base + data->virtual_address + second->value) {
            saw_data_symbol = 1;
        }
        if (value == preferred_base + image_rodata_rva) {
            saw_rodata_symbol = 1;
        }
        if (value == preferred_base + bss->virtual_address + zero->value) {
            saw_bss_symbol = 1;
        }
        if (value == preferred_base + code->virtual_address + target->value) {
            saw_code_symbol = 1;
        }
        if (source == data) {
            assert(source_offset != static_null->value);
            ++data_source_relocations;
            if (source_offset == static_literal->value &&
                value == preferred_base + image_rodata_rva +
                         static_literal_offset) {
                saw_static_literal = 1;
            }
            if (source_offset == static_suffix->value &&
                value == preferred_base + image_rodata_rva +
                         static_literal_offset + 6u) {
                saw_static_suffix = 1;
            }
            if (source_offset == static_zero->value &&
                value == preferred_base + bss->virtual_address + zero->value) {
                saw_static_zero = 1;
            }
            if (source_offset == static_second->value &&
                value == preferred_base + bss->virtual_address +
                         static_values->value + 4u) {
                saw_static_second = 1;
            }
            if (source_offset == static_target->value &&
                value == preferred_base + code->virtual_address +
                         target->value) {
                saw_static_target = 1;
            }
            if (source_offset == aggregate_pointers->value &&
                value == preferred_base + data->virtual_address +
                         aggregate_target->value) {
                saw_aggregate_pointer_first = 1;
            }
            if (source_offset == aggregate_pointers->value +
                                     2u * pointer_width &&
                value == preferred_base + data->virtual_address +
                         aggregate_target->value) {
                saw_aggregate_pointer_third = 1;
            }
            if (source_offset == aggregate_record->value + 8u &&
                value == preferred_base + data->virtual_address +
                         aggregate_target->value) {
                saw_aggregate_record_pointer = 1;
            }
            if (source_offset == aggregate_nested->value + 24u &&
                value == preferred_base + data->virtual_address +
                         aggregate_target->value) {
                saw_aggregate_nested_pointer = 1;
            }
            if (source_offset == aggregate_path->value + 24u &&
                value == preferred_base + data->virtual_address +
                         aggregate_target->value) {
                saw_aggregate_path_pointer = 1;
            }
        }
    }
    assert(saw_data_symbol);
    assert(saw_rodata_symbol);
    assert(saw_bss_symbol);
    assert(saw_code_symbol);
    assert(data_source_relocations >= 10u);
    assert(saw_static_literal);
    assert(saw_static_suffix);
    assert(saw_static_zero);
    assert(saw_static_second);
    assert(saw_static_target);
    assert(saw_aggregate_pointer_first);
    assert(saw_aggregate_pointer_third);
    assert(saw_aggregate_record_pointer);
    assert(saw_aggregate_nested_pointer);
    assert(saw_aggregate_path_pointer);
    {
        uint8_t initialized[12];
        uint64_t null_value = 0u;
        assert(fseek(image, (long)(data->file_offset + static_null->value),
                     SEEK_SET) == 0);
        assert(fread(&null_value, pointer_width, 1u, image) == 1u);
        assert(null_value == 0u);
        assert(fseek(image, (long)(data->file_offset + static_array->value),
                     SEEK_SET) == 0);
        assert(fread(initialized, 1u, 9u, image) == 9u);
        assert(memcmp(initialized, "ArrayRin", 9u) == 0);
        assert(fseek(image, (long)(data->file_offset + static_fixed->value),
                     SEEK_SET) == 0);
        assert(fread(initialized, 1u, sizeof(initialized), image) ==
               sizeof(initialized));
        assert(memcmp(initialized, "Fixed", 6u) == 0);
        for (size_t byte = 6u; byte < sizeof(initialized); ++byte) {
            assert(initialized[byte] == 0u);
        }
        assert(fseek(image,
                     (long)(data->file_offset + aggregate_pointers->value +
                            pointer_width),
                     SEEK_SET) == 0);
        null_value = 0u;
        assert(fread(&null_value, pointer_width, 1u, image) == 1u);
        assert(null_value == 0u);
    }

    free(relocations);
    free(sections);
    assert(fclose(image) == 0);
    objfile_free(object);
}

static void verify_external_data_relocation(const char* reference_path,
                                            const char* definition_path,
                                            const char* image_path,
                                            uint16_t expected_architecture)
{
    ObjectFile* reference = objfile_read(reference_path);
    ObjectFile* definition = objfile_read(definition_path);
    ObjSymbol* pointer;
    ObjSymbol* undefined_data;
    ObjSymbol* defined_data;
    ObjSection* reference_data;
    ObjSection* definition_data;
    RinHeaderV3 header;
    RinSectionV3* sections;
    RinSectionV3* data = NULL;
    RinSectionV3* relocation_section = NULL;
    RinRelocationV3* relocations;
    FILE* image;
    uint64_t definition_offset;
    uint64_t expected_value;
    uint64_t actual_value = 0u;
    size_t width = expected_architecture == RIN_ARCH_X86_64 ? 8u : 4u;
    RelocType object_type = expected_architecture == RIN_ARCH_X86_64
        ? RELOC_ABS64 : RELOC_ABS32U;
    uint16_t image_type = expected_architecture == RIN_ARCH_X86_64
        ? RIN_IMAGE_RELOCATION_ABS64 : RIN_IMAGE_RELOCATION_ABS32U;
    int saw_object_relocation = 0;
    int saw_image_relocation = 0;

    assert(reference != NULL && definition != NULL);
    pointer = required_symbol(reference, "external_pointer");
    undefined_data = objfile_find_symbol(reference, "external_data");
    defined_data = required_symbol(definition, "external_data");
    assert(undefined_data != NULL && undefined_data->section < 0);
    reference_data = required_section(reference, SECT_DATA);
    definition_data = required_section(definition, SECT_DATA);
    for (ObjReloc* relocation = reference_data->relocs; relocation;
         relocation = relocation->next) {
        if (relocation->offset == pointer->value &&
            strcmp(relocation->symbol_name, "external_data") == 0) {
            assert(relocation->type == object_type);
            assert(relocation->addend == 0);
            saw_object_relocation = 1;
        }
    }
    assert(saw_object_relocation);

    image = fopen(image_path, "rb");
    assert(image != NULL);
    assert(fread(&header, sizeof(header), 1u, image) == 1u);
    assert(header.magic == RIN_IMAGE_MAGIC);
    assert(header.version == RIN_IMAGE_VERSION_3);
    assert(header.architecture == expected_architecture);
    sections = calloc(header.section_count, sizeof(*sections));
    assert(sections != NULL);
    assert(fseek(image, (long)header.section_table_offset, SEEK_SET) == 0);
    assert(fread(sections, sizeof(*sections), header.section_count, image) ==
           header.section_count);
    for (uint32_t index = 0u; index < header.section_count; ++index) {
        if (sections[index].type == RIN_IMAGE_SECTION_DATA) data = &sections[index];
        if (sections[index].type == RIN_IMAGE_SECTION_RELOCATIONS) {
            relocation_section = &sections[index];
        }
    }
    assert(data != NULL && relocation_section != NULL);
    assert(pointer->value + width <= data->file_size);
    relocations = calloc(
        (size_t)(relocation_section->file_size / sizeof(*relocations)),
        sizeof(*relocations));
    assert(relocations != NULL);
    assert(fseek(image, (long)relocation_section->file_offset, SEEK_SET) == 0);
    assert(fread(relocations, sizeof(*relocations),
                 (size_t)(relocation_section->file_size /
                          sizeof(*relocations)), image) ==
           (size_t)(relocation_section->file_size / sizeof(*relocations)));
    for (size_t index = 0u;
         index < relocation_section->file_size / sizeof(*relocations);
         ++index) {
        if (relocations[index].virtual_address ==
            data->virtual_address + pointer->value) {
            assert(relocations[index].type == image_type);
            saw_image_relocation = 1;
        }
    }
    assert(saw_image_relocation);
    assert(fseek(image, (long)(data->file_offset + pointer->value),
                 SEEK_SET) == 0);
    assert(fread(&actual_value, width, 1u, image) == 1u);

    definition_offset = reference_data->memory_size;
    if (definition_data->align > 1u) {
        uint64_t mask = definition_data->align - 1u;
        definition_offset = (definition_offset + mask) & ~mask;
    }
    expected_value = header.preferred_base + data->virtual_address +
                     definition_offset + defined_data->value;
    assert(actual_value == expected_value);

    free(relocations);
    free(sections);
    assert(fclose(image) == 0);
    objfile_free(reference);
    objfile_free(definition);
}

int main(int argc, char** argv)
{
    assert(argc == 19);
    verify_artifact(argv[1], argv[2], RIN_ARCH_X86, RIN_IMAGE_MAGIC);
    verify_artifact(argv[3], argv[4], RIN_ARCH_X86_64, RIN_IMAGE_MAGIC);
    verify_artifact(argv[5], argv[6], RIN_ARCH_X86,
                    RIN_DRIVER_IMAGE_MAGIC);
    verify_artifact(argv[7], argv[8], RIN_ARCH_X86_64,
                    RIN_DRIVER_IMAGE_MAGIC);
    verify_artifact(argv[9], argv[10], RIN_ARCH_X86, RIN_IMAGE_MAGIC);
    verify_artifact(argv[11], argv[12], RIN_ARCH_X86_64, RIN_IMAGE_MAGIC);
    verify_external_data_relocation(argv[13], argv[14], argv[15],
                                    RIN_ARCH_X86);
    verify_external_data_relocation(argv[16], argv[17], argv[18],
                                    RIN_ARCH_X86_64);
    return 0;
}
