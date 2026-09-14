#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#pragma pack(push, 1)
struct PackedArgument {
    unsigned char tag;
    long long value;
};

struct PackedReturn {
    unsigned char tag;
    long long value;
};
#pragma pack(pop)

typedef int (*packed_argument_function)(struct PackedArgument);
typedef struct PackedReturn (*packed_return_function)(int, long long);

static ObjSection* code_section(ObjectFile* object)
{
    ObjSection* section = object->sections;
    while (section && section->type != SECT_CODE) section = section->next;
    assert(section != NULL);
    return section;
}

#define LOAD_FUNCTION(target, object, mapping, symbol_name)                 \
    do {                                                                    \
        ObjSymbol* symbol = objfile_find_symbol((object), (symbol_name));   \
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
    packed_argument_function packed_argument;
    packed_return_function packed_return;
    struct PackedArgument argument = { 7u, 35ll };
    struct PackedReturn result;

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

    LOAD_FUNCTION(packed_argument, object, mapping, "packed_argument");
    LOAD_FUNCTION(packed_return, object, mapping, "packed_return");

    assert(packed_argument(argument) == 42);
    result = packed_return(9, 123456789ll);
    assert(result.tag == 9u && result.value == 123456789ll);

    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
    puts("SysV packed aggregate ABI test passed");
    return 0;
}
