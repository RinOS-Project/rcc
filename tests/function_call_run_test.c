#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef unsigned (*unsigned_function)(unsigned);
typedef int (*int_function)(int);
typedef int (*int_u64_function)(uint64_t);
typedef int (*void_function)(void);

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
    unsigned_function call_fixed_u8;
    int_function call_fixed_s8;
    unsigned_function call_fixed_u16;
    int_u64_function call_fixed_bool;
    int_u64_function call_variadic;
    void_function call_without_prototype;
    void_function call_promoted_redeclaration;

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

    LOAD_FUNCTION(call_fixed_u8, object, mapping, "call_fixed_u8");
    LOAD_FUNCTION(call_fixed_s8, object, mapping, "call_fixed_s8");
    LOAD_FUNCTION(call_fixed_u16, object, mapping, "call_fixed_u16");
    LOAD_FUNCTION(call_fixed_bool, object, mapping, "call_fixed_bool");
    LOAD_FUNCTION(call_variadic, object, mapping, "call_variadic");
    LOAD_FUNCTION(call_without_prototype, object, mapping,
                  "call_without_prototype");
    LOAD_FUNCTION(call_promoted_redeclaration, object, mapping,
                  "call_promoted_redeclaration");

    assert(call_fixed_u8(UINT32_C(0x1234)) == UINT8_C(0x34));
    assert(call_fixed_s8(255) == -1);
    assert(call_fixed_u16(UINT32_C(0x12345)) == UINT16_C(0x2345));
    assert(call_fixed_bool(UINT64_C(0x100000000)) == 1);
    assert(call_variadic(UINT64_C(0x100000000)) == 77);
    assert(call_without_prototype() == 91);
    assert(call_promoted_redeclaration() == 123);

    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
    puts("C17 function call execution test passed");
    return 0;
}
