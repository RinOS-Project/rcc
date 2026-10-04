#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int contains_bytes(const ObjSection* section, const uint8_t* bytes,
                          size_t count)
{
    uint64_t offset;
    if (count > section->size) return 0;
    for (offset = 0; offset <= section->size - count; ++offset) {
        if (memcmp(section->data + offset, bytes, count) == 0) return 1;
    }
    return 0;
}

static void verify(ObjectFile* object, uint16_t architecture)
{
    ObjSymbol* entry = objfile_find_symbol(object, "main");
    ObjSection* section;
    static const uint8_t expected[] = {'A', 0, 'B', 0};
    int found = 0;

    assert(object->arch == architecture);
    assert(entry != NULL && entry->section >= 0);
    for (section = object->sections; section; section = section->next) {
        if ((section->type == SECT_RODATA || section->type == SECT_DATA) &&
            contains_bytes(section, expected, sizeof(expected))) {
            found = 1;
            break;
        }
    }
    assert(found);
}

int main(int argc, char** argv)
{
    ObjectFile* x86;
    ObjectFile* x64;

    assert(argc == 3);
    x86 = objfile_read(argv[1]);
    x64 = objfile_read(argv[2]);
    assert(x86 != NULL && x64 != NULL);
    verify(x86, ARCH_X86);
    verify(x64, ARCH_X64);
    objfile_free(x86);
    objfile_free(x64);
    puts("i686/x64 embedded-NUL string object inspection passed");
    return 0;
}
