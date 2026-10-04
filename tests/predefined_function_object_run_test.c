#include "objfile.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static ObjSection* code_section(ObjectFile* object)
{
    ObjSection* section = object->sections;
    while (section && section->type != SECT_CODE) section = section->next;
    return section;
}

static void verify(ObjectFile* object, uint16_t architecture, int cxx)
{
    static const char* const c_names[] = {
        "probe_c_predefined_function", "probe_c_predefined_location",
    };
    static const char* const cxx_names[] = {
        "probe_cxx_predefined_function",
        "probe_cxx_predefined_location",
        "probe_cxx_nested_predefined_function",
    };
    const char* const* names = cxx ? cxx_names : c_names;
    size_t count = cxx ? sizeof(cxx_names) / sizeof(cxx_names[0])
                       : sizeof(c_names) / sizeof(c_names[0]);
    size_t index;

    assert(object->arch == architecture);
    assert(code_section(object) != NULL);
    for (index = 0; index < count; ++index) {
        ObjSymbol* symbol = objfile_find_symbol(object, names[index]);
        assert(symbol != NULL && symbol->section >= 0);
        assert(symbol->binding == BIND_CODE);
    }
}

int main(int argc, char** argv)
{
    int cxx = argc == 4 && strcmp(argv[1], "--cxx") == 0;
    ObjectFile* x86;
    ObjectFile* x64;

    assert(argc == 3 || cxx);
    x86 = objfile_read(cxx ? argv[2] : argv[1]);
    x64 = objfile_read(cxx ? argv[3] : argv[2]);
    assert(x86 != NULL && x64 != NULL);
    verify(x86, ARCH_X86, cxx);
    verify(x64, ARCH_X64, cxx);
    objfile_free(x86);
    objfile_free(x64);
    puts(cxx ? "i686/x64 C++ predefined identifier object inspection passed"
             : "i686/x64 C predefined identifier object inspection passed");
    return 0;
}
