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
typedef int (*wide_binary_function)(uint64_t, uint64_t);

typedef struct RinSliceV1 {
    uint64_t address;
    uint64_t size;
} RinSliceV1;

typedef RinSliceV1 (*reference_copy_function)(const RinSliceV1*);

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
    typedef int (*unary_function)(int);
    unary_function lowered_constructor_init;
    reference_copy_function copy_reference;
    wide_binary_function reference_call;
    wide_binary_function reference_overload;

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
    LOAD_FUNCTION(lowered_constructor_init, object, mapping,
                  "cxx_lowered_constructor_init");
    LOAD_FUNCTION(copy_reference, object, mapping,
                  "_ZN3rin14copy_referenceERK10RinSliceV1");
    LOAD_FUNCTION(reference_call, object, mapping, "cxx_reference_call");
    LOAD_FUNCTION(reference_overload, object, mapping,
                  "cxx_reference_overload");
    assert(direct_value_init() == 1);
    assert(local_value_init() == 1);
    assert(scalar_value_init() == 1);
    assert(class_aggregate_init(4, 7) == 4774);
    assert(lowered_constructor_init(9) == 90);
    {
        RinSliceV1 input = {UINT64_C(0x12345678), UINT64_C(0x87654321)};
        RinSliceV1 copied = copy_reference(&input);
        assert(copied.address == input.address);
        assert(copied.size == input.size);
    }
    assert(reference_call(UINT64_C(0x1122334455667788),
                          UINT64_C(0x8877665544332211)) == 1);
    assert(reference_overload(UINT64_C(0x1020304050607080),
                              UINT64_C(0x8070605040302010)) == 1);

    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
    puts("C++20 value-initialization execution test passed");
    return 0;
}
