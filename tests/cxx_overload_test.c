/* SPDX-License-Identifier: Apache-2.0 */
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
    assert(relocation_count(text, "_Z12choose_floatf") == 1u);
    assert(relocation_count(text, "_Z12choose_floatd") == 0u);
    assert(relocation_count(text, "_Z12pointer_kindPv") == 1u);
    assert(relocation_count(text, "_Z12pointer_kindPKv") == 1u);
    assert(relocation_count(text, "_Z13pointer_truthb") == 1u);
    assert(relocation_count(text, "_Z10null_truthb") == 1u);
    assert(relocation_count(text, "_Z7orderedil") == 1u);
    assert(relocation_count(text, "_Z7orderedli") == 1u);
    assert(relocation_count(text, "_Z12null_pointerPv") == 2u);
    assert(relocation_count(text, "_Z12null_pointeri") == 0u);
    assert(relocation_count(text, "_Z16default_overloadii") == 1u);
    assert(relocation_count(text, "_Z16default_overloadl") == 0u);
    objfile_free(object);
}

static void verify_derived_base_references(const char* path,
                                           uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSection* text;
    assert(object != NULL && object->arch == architecture);
    text = objfile_get_section(object, ".text");
    assert(text != NULL);
    assert(relocation_count(text, "_Z13take_ref_baseR7RefBase") == 1u);
    assert(relocation_count(text, "_Z16take_ref_derivedR10RefDerived") == 1u);
    assert(relocation_count(text, "_Z19take_const_ref_baseRK7RefBase") == 2u);
    objfile_free(object);
}

int main(int argc, char** argv)
{
    assert(argc == 5);
    verify_overloads(argv[1], ARCH_X86);
    verify_overloads(argv[2], ARCH_X64);
    verify_derived_base_references(argv[3], ARCH_X86);
    verify_derived_base_references(argv[4], ARCH_X64);
    return 0;
}
