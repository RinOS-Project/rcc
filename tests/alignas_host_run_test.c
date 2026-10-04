#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static ObjSection* section_at(ObjectFile* object, int index)
{
    ObjSection* section = object->sections;
    while (section && index-- > 0) section = section->next;
    return section;
}

static void verify_alignment(ObjectFile* object, uint16_t architecture)
{
    ObjSection* data = objfile_get_section(object, ".data");
    ObjSection* bss = objfile_get_section(object, ".bss");
    ObjSymbol* aligned_global = objfile_find_symbol(object, "aligned_global");
    ObjSymbol* aligned_type_global =
        objfile_find_symbol(object, "aligned_type_global");
    ObjSymbol* entry = objfile_find_symbol(object, "main");
    ObjSection* global_section;
    ObjSection* type_section;

    assert(object->arch == architecture);
    assert((data != NULL && data->type == SECT_DATA) ||
           (bss != NULL && bss->type == SECT_BSS));
    assert(aligned_global != NULL && aligned_global->section >= 0);
    assert(aligned_type_global != NULL && aligned_type_global->section >= 0);
    assert(entry != NULL && entry->section >= 0);
    global_section = section_at(object, aligned_global->section);
    type_section = section_at(object, aligned_type_global->section);
    assert(global_section != NULL && type_section == global_section);
    assert(global_section->type == SECT_DATA || global_section->type == SECT_BSS);
    assert(global_section->align >= 16u);
    assert(aligned_global->value % 16u == 0u);
    assert(aligned_type_global->value % 8u == 0u);
}

int main(int argc, char** argv)
{
    ObjectFile* x86;
    ObjectFile* x64;

    assert(argc == 3);
    x86 = objfile_read(argv[1]);
    x64 = objfile_read(argv[2]);
    assert(x86 != NULL && x64 != NULL);
    verify_alignment(x86, ARCH_X86);
    verify_alignment(x64, ARCH_X64);
    objfile_free(x86);
    objfile_free(x64);
    puts("i686/x64 _Alignas object alignment inspection passed");
    return 0;
}
