#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "objfile.h"

static void check_symbol(ObjectFile* object, const char* name,
                         SymbolType type, SymbolBinding binding,
                         int section) {
    ObjSymbol* symbol = objfile_find_symbol(object, name);
    assert(symbol != NULL);
    assert(symbol->type == type);
    assert(symbol->binding == binding);
    assert(symbol->section == section);
}

int main(int argc, char** argv) {
    ObjectFile* object;
    if (argc != 3) return 2;
    object = objfile_read(argv[1]);
    assert(object != NULL);
    check_symbol(object, "rcc_weak_attribute_function", SYM_WEAK,
                 BIND_CODE, 0);
    check_symbol(object, "rcc_weak_attribute_data", SYM_WEAK,
                 BIND_DATA, 1);
    check_symbol(object, "rcc_weak_attribute_import", SYM_WEAK,
                 BIND_DATA, -1);
    check_symbol(object, "rcc_weak_attribute_read", SYM_GLOBAL,
                 BIND_CODE, 0);
    assert(strcmp(object->filename, argv[1]) == 0);
    objfile_free(object);

    object = objfile_read(argv[2]);
    assert(object != NULL);
    check_symbol(object, "rcc_weak_attribute_function", SYM_WEAK,
                 BIND_CODE, 0);
    check_symbol(object, "rcc_weak_attribute_data", SYM_WEAK,
                 BIND_DATA, 1);
    check_symbol(object, "rcc_weak_attribute_import", SYM_WEAK,
                 BIND_DATA, -1);
    objfile_free(object);
    puts("Source-level weak declaration tests completed");
    return 0;
}
