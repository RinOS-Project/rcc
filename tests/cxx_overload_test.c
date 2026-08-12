/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <string.h>

#include "objfile.h"

static unsigned relocation_count(ObjSection* text, const char* name)
{
    unsigned count = 0u;
    ObjReloc* relocation;
    for (relocation = text->relocs; relocation;
         relocation = relocation->next) {
        if (strcmp(relocation->symbol_name, name) == 0) ++count;
    }
    return count;
}

static void verify_overloads(const char* path, uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* text;
    assert(object != NULL && object->arch == architecture);
    text = objfile_get_section(object, ".text");
    assert(text != NULL);
    assert(relocation_count(text, "_Z6choosei") == 1u);
    assert(relocation_count(text, "_Z6choosel") == 1u);
    assert(relocation_count(text, "_Z12pointer_kindPv") == 1u);
    assert(relocation_count(text, "_Z12pointer_kindPKv") == 1u);
    assert(relocation_count(text, "_Z7orderedil") == 1u);
    assert(relocation_count(text, "_Z7orderedli") == 1u);
    assert(relocation_count(text, "_Z12null_pointerPv") == 1u);
    assert(relocation_count(text, "_Z12null_pointeri") == 0u);
    objfile_free(object);
}

int main(int argc, char** argv)
{
    assert(argc == 3);
    verify_overloads(argv[1], ARCH_X86);
    verify_overloads(argv[2], ARCH_X64);
    return 0;
}
