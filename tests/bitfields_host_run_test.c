#include "objfile.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

extern int bitfield_static(void);
extern int bitfield_runtime(void);
extern int bitfield_address_is_rejected(void);

static void require_function(ObjectFile* object, const char* name) {
    ObjSymbol* symbol = objfile_find_symbol(object, name);
    assert(symbol != NULL);
    assert(symbol->section >= 0);
    assert(symbol->binding == BIND_CODE);
}

static int inspect_i686(const char* path) {
    ObjectFile* object = objfile_read(path);
    ObjSection* code;
    ObjSection* data;

    assert(object != NULL);
    assert(object->arch == ARCH_X86);
    code = objfile_get_section(object, ".text");
    data = objfile_get_section(object, ".data");
    assert(code != NULL && code->size > 0u);
    assert(data != NULL && data->size >= 8u);
    require_function(object, "bitfield_static");
    require_function(object, "bitfield_runtime");
    require_function(object, "bitfield_address_is_rejected");
    objfile_free(object);
    puts("i686 bit-field object layout and symbol inspection passed");
    return 0;
}

int main(int argc, char** argv) {
    if (argc == 3 && strcmp(argv[1], "--inspect") == 0) {
        return inspect_i686(argv[2]);
    }
    assert(argc == 1);
    assert(bitfield_static() == 0);
    assert(bitfield_runtime() == 0);
    assert(bitfield_address_is_rejected() == 0);
    puts("x86_64 bit-field execution passed");
    return 0;
}
