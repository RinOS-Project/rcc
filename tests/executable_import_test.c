/* SPDX-License-Identifier: MIT */
#include "rin_formats_v3.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void verify_image(const char* path, uint16_t architecture)
{
    FILE* file = fopen(path, "rb");
    RinHeaderV3 header;
    RinSectionV3* sections;
    RinDependencyV3 dependency;
    char* strings;
    const RinSectionV3* imports_section = NULL;
    const RinSectionV3* data_section = NULL;
    const RinSectionV3* code_section = NULL;
    RinImportV3 imported;
    uint8_t* code;
    size_t thunk_count = 0u;
    uint64_t pointer_width = architecture == RIN_ARCH_X86 ? 4u : 8u;

    assert(file != NULL);
    assert(fread(&header, sizeof(header), 1u, file) == 1u);
    assert(header.magic == RIN_IMAGE_MAGIC);
    assert(header.version == RIN_IMAGE_VERSION_3);
    assert(header.architecture == architecture);
    assert((header.flags & RIN_IMAGE_LIBRARY) == 0u);
    assert(header.dependency_count == 1u);
    assert(header.section_count >= 3u);

    sections = calloc(header.section_count, sizeof(*sections));
    strings = calloc((size_t)header.string_table_size, 1u);
    assert(sections != NULL && strings != NULL);
    assert(fseek(file, (long)header.section_table_offset, SEEK_SET) == 0);
    assert(fread(sections, sizeof(*sections), header.section_count, file) ==
           header.section_count);
    assert(fseek(file, (long)header.dependency_table_offset, SEEK_SET) == 0);
    assert(fread(&dependency, sizeof(dependency), 1u, file) == 1u);
    assert(fseek(file, (long)header.string_table_offset, SEEK_SET) == 0);
    assert(fread(strings, 1u, (size_t)header.string_table_size, file) ==
           header.string_table_size);
    assert(dependency.name_offset < header.string_table_size);
    assert(strcmp(strings + dependency.name_offset, "rincrt.rll") == 0);

    for (uint32_t index = 0u; index < header.section_count; ++index) {
        if (sections[index].type == RIN_IMAGE_SECTION_IMPORTS) {
            imports_section = &sections[index];
        } else if (sections[index].type == RIN_IMAGE_SECTION_DATA) {
            data_section = &sections[index];
        } else if (sections[index].type == RIN_IMAGE_SECTION_CODE) {
            code_section = &sections[index];
        }
    }
    assert(imports_section != NULL && data_section != NULL &&
           code_section != NULL);
    assert(imports_section->file_size == sizeof(RinImportV3));
    assert(fseek(file, (long)imports_section->file_offset, SEEK_SET) == 0);
    assert(fread(&imported, sizeof(imported), 1u, file) == 1u);
    assert(imported.name_offset < header.string_table_size);
    assert(strcmp(strings + imported.name_offset, "imported_function") == 0);
    assert(imported.dependency_index == 0u);
    assert(imported.kind == RIN_SYMBOL_FUNCTION);
    assert((imported.target_rva & (pointer_width - 1u)) == 0u);
    assert(imported.target_rva >= data_section->virtual_address);
    assert(imported.target_rva + pointer_width <=
           data_section->virtual_address + data_section->memory_size);

    assert(code_section->file_size >= 6u &&
           code_section->file_size <= SIZE_MAX);
    code = calloc((size_t)code_section->file_size, 1u);
    assert(code != NULL);
    assert(fseek(file, (long)code_section->file_offset, SEEK_SET) == 0);
    assert(fread(code, 1u, (size_t)code_section->file_size, file) ==
           code_section->file_size);
    for (size_t offset = 0u; offset + 6u <= code_section->file_size;
         ++offset) {
        if (code[offset] != 0xffu || code[offset + 1u] != 0x25u) continue;
        if (architecture == RIN_ARCH_X86) {
            uint32_t target;
            memcpy(&target, code + offset + 2u, sizeof(target));
            if (target == header.preferred_base + imported.target_rva) {
                ++thunk_count;
            }
        } else {
            int32_t displacement;
            uint64_t thunk_rva = code_section->virtual_address + offset;
            memcpy(&displacement, code + offset + 2u, sizeof(displacement));
            if ((int64_t)displacement ==
                (int64_t)imported.target_rva - (int64_t)(thunk_rva + 6u)) {
                ++thunk_count;
            }
        }
    }
    assert(thunk_count == 1u);

    free(code);
    free(strings);
    free(sections);
    assert(fclose(file) == 0);
}

int main(int argc, char** argv)
{
    assert(argc == 3);
    verify_image(argv[1], RIN_ARCH_X86);
    verify_image(argv[2], RIN_ARCH_X86_64);
    puts("RIN v3 executable typed-import test passed");
    return 0;
}
