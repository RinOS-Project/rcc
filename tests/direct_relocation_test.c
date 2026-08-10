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

static ObjSection* required_section(ObjectFile* object, SectionType type)
{
    for (ObjSection* section = object->sections; section;
         section = section->next) {
        if (section->type == type) return section;
    }
    assert(!"required object section was not found");
    return NULL;
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
    ObjSymbol* static_zero;
    ObjSymbol* static_values;
    ObjSymbol* static_second;
    ObjSymbol* static_target;
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
    int saw_static_zero = 0;
    int saw_static_second = 0;
    int saw_static_target = 0;
    size_t data_source_relocations = 0u;
    uint64_t static_literal_offset;

    assert(object != NULL);
    first = required_symbol(object, "first_value");
    second = required_symbol(object, "second_value");
    zero = required_symbol(object, "zero_value");
    target = required_symbol(object, "target");
    static_literal = required_symbol(object, "static_literal");
    static_zero = required_symbol(object, "static_zero");
    static_values = required_symbol(object, "static_values");
    static_second = required_symbol(object, "static_second");
    static_target = required_symbol(object, "static_target");
    assert(first->section == second->section);
    assert(first->value != second->value);
    assert(zero->binding == BIND_BSS);
    assert(zero->section != first->section);
    for (ObjSection* section = object->sections; section;
         section = section->next) {
        if (section->type == SECT_RODATA) object_rodata = section;
        if (section->type == SECT_DATA) object_data = section;
    }
    assert(object_rodata != NULL && object_data != NULL);
    assert(object_rodata->flags == SECT_FLAG_ALLOC);
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
            }
        }
        assert(saw_static_literal && saw_static_zero && saw_static_second &&
               saw_static_target);
    }

    saw_static_literal = 0;
    saw_static_zero = 0;
    saw_static_second = 0;
    saw_static_target = 0;

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
    assert(code != NULL && rodata != NULL && data != NULL && bss != NULL &&
           relocation_section != NULL);
    assert(rodata->flags == RIN_IMAGE_SECTION_READ);
    assert(rodata->file_size == object_rodata->size &&
           rodata->memory_size == object_rodata->size);
    assert(rodata->virtual_address != data->virtual_address);
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
        } else if (relocations[index].virtual_address >=
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
        if (value == preferred_base + rodata->virtual_address) {
            saw_rodata_symbol = 1;
        }
        if (value == preferred_base + bss->virtual_address + zero->value) {
            saw_bss_symbol = 1;
        }
        if (value == preferred_base + code->virtual_address + target->value) {
            saw_code_symbol = 1;
        }
        if (source == data) {
            ++data_source_relocations;
            if (source_offset == static_literal->value &&
                value == preferred_base + rodata->virtual_address +
                         static_literal_offset) {
                saw_static_literal = 1;
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
        }
    }
    assert(saw_data_symbol);
    assert(saw_rodata_symbol);
    assert(saw_bss_symbol);
    assert(saw_code_symbol);
    assert(data_source_relocations >= 4u);
    assert(saw_static_literal);
    assert(saw_static_zero);
    assert(saw_static_second);
    assert(saw_static_target);

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
