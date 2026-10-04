#include "objfile.h"

#include <assert.h>
#include <stdio.h>

static void verify(ObjectFile* object, uint16_t architecture)
{
    ObjSection* section = object->sections;
    ObjSymbol* entry;

    assert(object->arch == architecture);
    while (section && section->type != SECT_CODE) section = section->next;
    assert(section != NULL && section->size > 0u);
    entry = objfile_find_symbol(object, "main");
    assert(entry != NULL && entry->section >= 0);
    assert(entry->binding == BIND_CODE);
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
    puts("i686/x64 C++ _Pragma pack object inspection passed");
    return 0;
}
