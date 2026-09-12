#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef int (*unary_function)(int);
typedef int (*binary_function)(int, int);
typedef int (*ternary_function)(int, int, int);
typedef int (*pointer_function)(int*);

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
    unary_function compound_member;
    ternary_function compound_argument;
    binary_function compound_array;
    unary_function compound_scalar;
    pointer_function compound_side_effect;
    int counter = 4;

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

    LOAD_FUNCTION(compound_member, object, mapping, "compound_member");
    LOAD_FUNCTION(compound_argument, object, mapping, "compound_argument");
    LOAD_FUNCTION(compound_array, object, mapping, "compound_array");
    LOAD_FUNCTION(compound_scalar, object, mapping, "compound_scalar");
    LOAD_FUNCTION(compound_side_effect, object, mapping,
                  "compound_side_effect");

    assert(compound_member(7) == 7);
    assert(compound_argument(4, 5, 6) == 456);
    assert(compound_array(3, 8) == 8);
    assert(compound_scalar(9) == 11);
    assert(compound_side_effect(&counter) == 423);
    assert(counter == 5);

    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
    puts("C17 automatic compound literal test passed");
    return 0;
}
