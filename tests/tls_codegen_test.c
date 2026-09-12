/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "objfile.h"
#include "rin_formats_v3.h"

static uint8_t* read_file(const char* path, size_t* size)
{
    FILE* file = fopen(path, "rb");
    long length;
    uint8_t* bytes;
    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    length = ftell(file);
    assert(length > 0);
    assert(fseek(file, 0, SEEK_SET) == 0);
    bytes = malloc((size_t)length);
    assert(bytes != NULL);
    assert(fread(bytes, 1u, (size_t)length, file) == (size_t)length);
    assert(fclose(file) == 0);
    *size = (size_t)length;
    return bytes;
}

static uint32_t read_u32(const uint8_t* bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static int section_index(ObjectFile* object, ObjSection* target)
{
    int index = 0;
    for (ObjSection* section = object->sections; section;
         section = section->next, ++index) {
        if (section == target) return index;
    }
    return -1;
}

static int bytes_are_zero(const uint8_t* bytes, uint64_t size)
{
    for (uint64_t index = 0u; index < size; ++index) {
        if (bytes[index] != 0u) return 0;
    }
    return 1;
}

static void verify_object(const char* path, uint16_t arch, int cxx)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* tls;
    ObjSection* text;
    ObjSymbol* initialized;
    uint64_t pointer_size = arch == ARCH_X64 ? 8u : 4u;
    unsigned relocation_count = 0u;
    assert(object != NULL && object->arch == arch);
    tls = objfile_get_section(object, ".tls");
    text = objfile_get_section(object, ".text");
    assert(tls != NULL && text != NULL);
    assert(tls->type == SECT_TLS && tls->size == tls->memory_size);
    initialized = objfile_find_symbol(
        object, cxx ? "_Z15tls_cpp_counter" : "tls_counter");
    assert(initialized != NULL && initialized->binding == BIND_TLS &&
           initialized->section >= 0 && initialized->value + 4u <= tls->size);
    assert(read_u32(tls->data + initialized->value) == (cxx ? 11u : 7u));
    if (cxx) {
        ObjSection* data = objfile_get_section(object, ".data");
        ObjSymbol* tls_pointer = objfile_find_symbol(
            object, "_Z20tls_cpp_null_pointer");
        ObjSymbol* global_pointer = objfile_find_symbol(
            object, "_Z16cpp_null_pointer");
        assert(data != NULL);
        assert(tls_pointer != NULL && tls_pointer->binding == BIND_TLS &&
               tls_pointer->section == section_index(object, tls) &&
               tls_pointer->value + pointer_size <= tls->size &&
               bytes_are_zero(tls->data + tls_pointer->value, pointer_size));
        assert(global_pointer != NULL &&
               global_pointer->section == section_index(object, data) &&
               global_pointer->value + pointer_size <= data->size &&
               bytes_are_zero(data->data + global_pointer->value,
                              pointer_size));
    } else {
        ObjSymbol* zero = NULL;
        for (ObjSymbol* symbol = object->symbols; symbol;
             symbol = symbol->next) {
            if (symbol != initialized && symbol->binding == BIND_TLS &&
                symbol->section >= 0) {
                zero = symbol;
                break;
            }
        }
        assert(zero != NULL && zero->value + 4u <= tls->size);
        assert(read_u32(tls->data + zero->value) == 0u);
    }
    for (ObjReloc* relocation = text->relocs; relocation;
         relocation = relocation->next) {
        if (relocation->type != RELOC_TLSOFF32S) continue;
        assert(relocation->offset + 4u <= text->size);
        assert(read_u32(text->data + relocation->offset) == 0u);
        ++relocation_count;
    }
    assert(relocation_count >= 2u);
    objfile_free(object);
}

static void verify_image(const char* path, uint16_t architecture, int library)
{
    size_t size;
    uint8_t* bytes = read_file(path, &size);
    RinHeaderV3* header = (RinHeaderV3*)bytes;
    RinSectionV3* sections;
    RinSectionV3* code = NULL;
    RinSectionV3* data = NULL;
    RinSectionV3* tls = NULL;
    RinSectionV3* relocation_section = NULL;
    unsigned data_count = 0u;
    unsigned rodata_count = 0u;
    RinRelocationV3* relocations;
    uint64_t relocation_count;
    assert(size >= sizeof(*header));
    assert(header->magic == RIN_IMAGE_MAGIC &&
           header->version == RIN_IMAGE_VERSION_3 &&
           header->architecture == architecture &&
           header->abi_minor >= RIN_IMAGE_ABI_MINOR_TLSOFF32S &&
           (header->flags & RIN_IMAGE_USES_TLS) != 0u);
    assert(((header->flags & RIN_IMAGE_LIBRARY) != 0u) == (library != 0));
    assert(((header->flags & RIN_IMAGE_EXECUTABLE) != 0u) == (library == 0));
    if (library) assert(header->entry_rva == 0u);
    assert(header->section_table_offset +
               (uint64_t)header->section_count * sizeof(*sections) <= size);
    sections = (RinSectionV3*)(bytes + header->section_table_offset);
    for (uint32_t index = 0u; index < header->section_count; ++index) {
        if (sections[index].type == RIN_IMAGE_SECTION_CODE) code = &sections[index];
        if (sections[index].type == RIN_IMAGE_SECTION_DATA) {
            data = &sections[index];
            ++data_count;
        }
        if (sections[index].type == RIN_IMAGE_SECTION_RODATA) {
            ++rodata_count;
        }
        if (sections[index].type == RIN_IMAGE_SECTION_TLS) tls = &sections[index];
        if (sections[index].type == RIN_IMAGE_SECTION_RELOCATIONS) {
            relocation_section = &sections[index];
        }
    }
    assert(code != NULL && data != NULL && data_count == 1u && tls != NULL &&
           relocation_section != NULL);
    assert(rodata_count == 0u && code->virtual_address == 0u &&
           code->file_size == code->memory_size &&
           data->virtual_address == code->memory_size &&
           data->file_offset == code->file_offset + code->file_size &&
           data->file_size == data->memory_size &&
           header->image_size == data->virtual_address + data->memory_size);
    assert(tls->flags == RIN_IMAGE_SECTION_READ && tls->memory_size >= 8u &&
           tls->file_size == tls->memory_size &&
           tls->virtual_address >= data->virtual_address &&
           tls->virtual_address + tls->memory_size <=
               data->virtual_address + data->memory_size &&
           tls->file_offset + tls->file_size <= size);
    assert(read_u32(bytes + tls->file_offset) == 7u);
    assert(read_u32(bytes + tls->file_offset + 4u) == 0u);
    assert(read_u32(bytes + data->file_offset) == 3u);
    assert(relocation_section->file_offset + relocation_section->file_size <= size);
    assert(relocation_section->file_size % sizeof(*relocations) == 0u);
    relocations = (RinRelocationV3*)(bytes + relocation_section->file_offset);
    relocation_count = relocation_section->file_size / sizeof(*relocations);
    assert(relocation_count >= 4u);
    for (uint64_t index = 0u; index < relocation_count; ++index) {
        uint64_t relative;
        assert(relocations[index].type == RIN_IMAGE_RELOCATION_TLSOFF32S);
        assert(relocations[index].virtual_address >= code->virtual_address &&
               relocations[index].virtual_address + 4u <=
                   code->virtual_address + code->file_size);
        relative = relocations[index].virtual_address - code->virtual_address;
        assert(read_u32(bytes + code->file_offset + relative) < tls->memory_size);
    }
    free(bytes);
}

int main(int argc, char** argv)
{
    assert(argc == 13);
    verify_object(argv[1], ARCH_X86, 0);
    verify_image(argv[2], RIN_ARCH_X86, 0);
    verify_image(argv[3], RIN_ARCH_X86, 0);
    verify_object(argv[4], ARCH_X64, 0);
    verify_image(argv[5], RIN_ARCH_X86_64, 0);
    verify_image(argv[6], RIN_ARCH_X86_64, 0);
    verify_object(argv[7], ARCH_X86, 1);
    verify_object(argv[8], ARCH_X64, 1);
    verify_image(argv[9], RIN_ARCH_X86, 1);
    verify_image(argv[10], RIN_ARCH_X86, 1);
    verify_image(argv[11], RIN_ARCH_X86_64, 1);
    verify_image(argv[12], RIN_ARCH_X86_64, 1);
    return 0;
}
