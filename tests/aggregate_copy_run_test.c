#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

struct Pair {
    int left;
    int right;
};

typedef int (*binary_function)(int, int);
typedef int (*pair_function)(struct Pair*);
typedef int (*ternary_function)(int, int, int);
typedef int (*unary_function)(int);
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
    binary_function copy_local;
    binary_function assign_chain;
    binary_function assign_odd;
    pair_function copy_pointer;
    ternary_function anonymous_members;
    unary_function conditional_argument;
    void_function comma_argument;
    struct Pair pair = { 4, 7 };

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

    LOAD_FUNCTION(copy_local, object, mapping, "copy_local");
    LOAD_FUNCTION(copy_pointer, object, mapping, "copy_pointer");
    LOAD_FUNCTION(assign_chain, object, mapping, "assign_chain");
    LOAD_FUNCTION(assign_odd, object, mapping, "assign_odd");
    LOAD_FUNCTION(anonymous_members, object, mapping, "anonymous_members");
    LOAD_FUNCTION(conditional_argument, object, mapping, "conditional_argument");
    LOAD_FUNCTION(comma_argument, object, mapping, "comma_argument");

    assert(copy_local(3, 8) == 38);
    assert(copy_pointer(&pair) == 47);
    assert(assign_chain(3, 8) == 3838);
    assert(assign_odd(7, 0xab) == 7171);
    assert(anonymous_members(1, 2, 3) == 123);
    assert(conditional_argument(0) == 47);
    assert(conditional_argument(1) == 38);
    assert(comma_argument() == 56);

    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
    puts("C17 aggregate copy and anonymous member test passed");
    return 0;
}
