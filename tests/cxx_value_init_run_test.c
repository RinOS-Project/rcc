/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "objfile.h"

typedef int (*nullary_function)(void);
typedef int (*binary_function)(int, int);

static ObjSection* code_section(ObjectFile* object)
{
    ObjSection* section = object->sections;
    while (section && section->type != SECT_CODE) section = section->next;
    assert(section != NULL);
    return section;
}

#define LOAD_FUNCTION(target, object, mapping, symbol_name)                 \
    do {                                                                    \
        ObjSymbol* symbol = objfile_find_symbol((object), (symbol_name));    \
        void* address;                                                      \
        assert(symbol != NULL && symbol->section >= 0);                     \
        assert(symbol->binding == BIND_CODE);                               \
        address = (mapping) + symbol->value;                                \
        memcpy(&(target), &address, sizeof(target));                        \
    } while (0)

int main(int argc, char** argv)
{
    ObjectFile* object;
    ObjSection* code;
    uint8_t* mapping;
    long page_size;
    size_t mapping_size;
    nullary_function direct_value_init;
    nullary_function local_value_init;
    nullary_function scalar_value_init;
    binary_function class_aggregate_init;

    assert(argc == 2);
    object = objfile_read(argv[1]);
    assert(object != NULL);
#if defined(__i386__)
    assert(object->arch == ARCH_X86);
#else
    assert(object->arch == ARCH_X64);
#endif
    code = code_section(object);
    page_size = sysconf(_SC_PAGESIZE);
    assert(page_size > 0);
    mapping_size = ((size_t)code->size + (size_t)page_size - 1u) /
                   (size_t)page_size * (size_t)page_size;
    mapping = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(mapping != MAP_FAILED);
    memcpy(mapping, code->data, (size_t)code->size);
    assert(mprotect(mapping, mapping_size, PROT_READ | PROT_EXEC) == 0);

    LOAD_FUNCTION(direct_value_init, object, mapping,
                  "cxx_direct_value_init");
    LOAD_FUNCTION(local_value_init, object, mapping,
                  "cxx_local_value_init");
    LOAD_FUNCTION(scalar_value_init, object, mapping,
                  "cxx_scalar_value_init");
    LOAD_FUNCTION(class_aggregate_init, object, mapping,
                  "cxx_class_aggregate_init");
    assert(direct_value_init() == 1);
    assert(local_value_init() == 1);
    assert(scalar_value_init() == 1);
    assert(class_aggregate_init(4, 7) == 4774);

    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
    puts("C++20 value-initialization execution test passed");
    return 0;
}
