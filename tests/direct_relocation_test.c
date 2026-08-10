#include "objfile.h"
#include "rin_formats_v3.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static ObjSymbol* required_symbol(ObjectFile* object, const char* name)
{
    ObjSymbol* symbol = objfile_find_symbol(object, name);
    assert(symbol != NULL);
    assert(symbol->section >= 0);
    return symbol;
}

static void verify_artifact(const char* object_path, const char* image_path,
                            uint16_t expected_architecture,
                            uint32_t expected_magic)
{
    ObjectFile* object = objfile_read(object_path);
    ObjSymbol* first;
    ObjSymbol* second;
    ObjSymbol* target;
    RinHeaderV3 header;
    RinDriverHeaderV3 driver_header;
    RinSectionV3* sections;
    RinSectionV3* code = NULL;
    RinSectionV3* data = NULL;
    RinSectionV3* relocation_section = NULL;
    RinRelocationV3* relocations;
    FILE* image;
    size_t relocation_count;
    uint32_t section_count;
    uint64_t section_table_offset;
    int saw_data_symbol = 0;
    int saw_code_symbol = 0;

    assert(object != NULL);
    first = required_symbol(object, "first_value");
    second = required_symbol(object, "second_value");
    target = required_symbol(object, "target");
    assert(first->section == second->section);
    assert(first->value != second->value);

    image = fopen(image_path, "rb");
    assert(image != NULL);
    if (expected_magic == RIN_IMAGE_MAGIC) {
        assert(fread(&header, sizeof(header), 1u, image) == 1u);
        assert(header.magic == expected_magic);
        assert(header.version == RIN_IMAGE_VERSION_3);
        assert(header.architecture == expected_architecture);
        section_count = header.section_count;
        section_table_offset = header.section_table_offset;
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
        if (sections[index].type == RIN_IMAGE_SECTION_DATA) data = &sections[index];
        if (sections[index].type == RIN_IMAGE_SECTION_RELOCATIONS) {
            relocation_section = &sections[index];
        }
    }
    assert(code != NULL && data != NULL && relocation_section != NULL);
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
        size_t width = expected_architecture == RIN_ARCH_X86_64 ? 8u : 4u;
        uint16_t expected_type = expected_architecture == RIN_ARCH_X86_64
            ? RIN_IMAGE_RELOCATION_ABS64 : RIN_IMAGE_RELOCATION_ABS32U;
        assert(relocations[index].type == expected_type);
        assert(relocations[index].virtual_address + width <= code->file_size);
        assert(fseek(image, (long)(code->file_offset +
                                  relocations[index].virtual_address),
                     SEEK_SET) == 0);
        assert(fread(&value, width, 1u, image) == 1u);
        if (value == data->virtual_address + second->value) {
            saw_data_symbol = 1;
        }
        if (value == code->virtual_address + target->value) {
            saw_code_symbol = 1;
        }
    }
    assert(saw_data_symbol);
    assert(saw_code_symbol);

    free(relocations);
    free(sections);
    assert(fclose(image) == 0);
    objfile_free(object);
}

int main(int argc, char** argv)
{
    assert(argc == 9);
    verify_artifact(argv[1], argv[2], RIN_ARCH_X86, RIN_IMAGE_MAGIC);
    verify_artifact(argv[3], argv[4], RIN_ARCH_X86_64, RIN_IMAGE_MAGIC);
    verify_artifact(argv[5], argv[6], RIN_ARCH_X86,
                    RIN_DRIVER_IMAGE_MAGIC);
    verify_artifact(argv[7], argv[8], RIN_ARCH_X86_64,
                    RIN_DRIVER_IMAGE_MAGIC);
    return 0;
}
