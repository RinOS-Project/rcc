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

static uint32_t read_u32(const uint8_t* data, uint64_t offset)
{
    return (uint32_t)data[offset] |
           ((uint32_t)data[offset + 1u] << 8) |
           ((uint32_t)data[offset + 2u] << 16) |
           ((uint32_t)data[offset + 3u] << 24);
}

static void verify_debug_object(const char* path, uint16_t architecture,
                                uint16_t language, const char* source_file,
                                const char* function_name)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* line;
    ObjSection* info;
    ObjSection* abbrev;
    ObjSection* strings;
    assert(object != NULL);
    assert(object->arch == architecture);
    line = objfile_get_section(object, ".debug_line");
    assert(line != NULL);
    assert(line->type == SECT_DEBUG_LINE);
    assert(line->flags == 0u && line->size == line->memory_size);
    assert(line->size > 16u);
    assert(line->data[4] == 4u && line->data[5] == 0u);
    assert(line->relocs != NULL);
    assert(contains_bytes(line->data, line->size, source_file));
    assert(contains_byte_pair(line->data, line->size, 5u, 1u));
    info = objfile_get_section(object, ".debug_info");
    abbrev = objfile_get_section(object, ".debug_abbrev");
    strings = objfile_get_section(object, ".debug_str");
    assert(info != NULL && info->type == SECT_DEBUG_INFO);
    assert(abbrev != NULL && abbrev->type == SECT_DEBUG_ABBREV);
    assert(strings != NULL && strings->type == SECT_DEBUG_STR);
    assert(info->relocs != NULL && info->size > 16u);
    assert(info->data[4] == 4u && info->data[5] == 0u);
    assert(info->data[16] == (uint8_t)language);
    assert(info->data[17] == (uint8_t)(language >> 8));
    {
        uint64_t address_size = architecture == ARCH_X64 ? 8u : 4u;
        uint64_t function_offset = 26u;
        uint64_t column_offset = function_offset + 1u + 4u + address_size +
                                  4u + 1u + 4u;
        assert(info->data[function_offset] == 2u);
        assert(info->size >= column_offset + 4u);
        assert(read_u32(info->data, column_offset) == 1u);
    }
    assert(abbrev->size > 8u && strings->size > 1u && strings->data[0] == 0u);
    assert(contains_bytes(strings->data, strings->size, function_name));
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
    objfile_free(object);
}

int main(int argc, char** argv)
{
    assert(argc == 5);
    verify_debug_object(argv[1], ARCH_X86, 0x000cu,
                        "tests/debug_info.c", "debug_line_entry");
    verify_debug_object(argv[2], ARCH_X64, 0x000cu,
                        "tests/debug_info.c", "debug_line_entry");
    verify_debug_object(argv[3], ARCH_X64, 0x0021u,
                        "tests/hello.cpp", "main");
    verify_without_debug(argv[4]);
    return 0;
}
