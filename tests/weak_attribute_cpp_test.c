#include <assert.h>
#include <stdio.h>

#include "objfile.h"

static void check_cpp_object(const char* path) {
    ObjectFile* object = objfile_read(path);
    ObjSymbol* symbol;
    assert(object != NULL);
    symbol = objfile_find_symbol(object, "rcc_weak_attribute_cpp_function");
    assert(symbol != NULL);
    assert(symbol->type == SYM_WEAK);
    assert(symbol->binding == BIND_CODE);
    assert(symbol->section == 0);
    symbol = objfile_find_symbol(object, "rcc_weak_attribute_cpp_gnu_function");
    assert(symbol != NULL);
    assert(symbol->type == SYM_WEAK);
    assert(symbol->binding == BIND_CODE);
    assert(symbol->section == 0);
    symbol = objfile_find_symbol(object, "rcc_weak_attribute_cpp_data");
    assert(symbol != NULL);
    assert(symbol->type == SYM_WEAK);
    assert(symbol->binding == BIND_DATA);
    assert(symbol->section == 1);
    objfile_free(object);
}

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    check_cpp_object(argv[1]);
    check_cpp_object(argv[2]);
    puts("C++ source-level weak declaration tests completed");
    return 0;
}
