#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef uint64_t (*literal_function)(void);

static ObjSection* code_section(ObjectFile* object)
{
    ObjSection* section = object->sections;
    while (section && section->type != SECT_CODE) section = section->next;
    assert(section != NULL);
    return section;
}

static literal_function load_function(ObjectFile* object, uint8_t* mapping,
                                      const char* name)
{
    ObjSymbol* symbol = objfile_find_symbol(object, name);
    literal_function function;
    void* address;
    assert(symbol != NULL && symbol->section >= 0);
    assert(symbol->binding == BIND_CODE);
    address = mapping + symbol->value;
    memcpy(&function, &address, sizeof(function));
    return function;
}

int main(int argc, char** argv)
{
    static const char* names[] = {
        "literal_maximum_ull",
        "literal_large_hex",
        "literal_decimal_boundary",
        "literal_unsigned_int",
        "literal_mixed_suffix"
    };
    static const uint64_t expected[] = {
        UINT64_MAX,
        UINT64_C(0xfedcba9876543210),
        UINT64_C(2147483648),
        UINT64_C(0xffffffff),
        UINT64_C(0x89abcdef01234567)
    };
    ObjectFile* object;
    ObjSection* code;
    uint8_t* mapping;
    long page_size;
    size_t mapping_size;

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

    for (size_t index = 0u; index < sizeof(names) / sizeof(names[0]); ++index) {
        literal_function function = load_function(object, mapping, names[index]);
        assert(function() == expected[index]);
    }

    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
    puts("C17 integer literal execution test passed");
    return 0;
}
