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

static void verify_debug_object(const char* path, uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* line;
    assert(object != NULL);
    assert(object->arch == architecture);
    line = objfile_get_section(object, ".debug_line");
    assert(line != NULL);
    assert(line->type == SECT_DEBUG_LINE);
    assert(line->flags == 0u && line->size == line->memory_size);
    assert(line->size > 16u);
    assert(line->data[4] == 4u && line->data[5] == 0u);
    assert(line->relocs != NULL);
    assert(contains_bytes(line->data, line->size, "tests/debug_info.c"));
    objfile_free(object);
}

static void verify_without_debug(const char* path)
{
    ObjectFile* object = objfile_read(path);
    assert(object != NULL);
    assert(objfile_get_section(object, ".debug_line") == NULL);
    objfile_free(object);
}

int main(int argc, char** argv)
{
    assert(argc == 4);
    verify_debug_object(argv[1], ARCH_X86);
    verify_debug_object(argv[2], ARCH_X64);
    verify_without_debug(argv[3]);
    return 0;
}
