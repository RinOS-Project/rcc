#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef int (*int_unary_function)(int);
typedef int (*int_void_function)(void);

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
    int_unary_function forward;
    int_unary_function backward;
    int_void_function constant_if;
    int_void_function constant_while;
    int_unary_function into_switch;
    int_unary_function reused_a;
    int_unary_function reused_b;

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

    LOAD_FUNCTION(forward, object, mapping, "goto_forward");
    LOAD_FUNCTION(backward, object, mapping, "goto_backward");
    LOAD_FUNCTION(constant_if, object, mapping, "goto_into_constant_if");
    LOAD_FUNCTION(constant_while, object, mapping,
                  "goto_into_constant_while");
    LOAD_FUNCTION(into_switch, object, mapping, "goto_into_switch");
    LOAD_FUNCTION(reused_a, object, mapping, "goto_reused_label_a");
    LOAD_FUNCTION(reused_b, object, mapping, "goto_reused_label_b");

    assert(forward(17) == 17);
    assert(forward(-4) == -4);
    assert(backward(0) == 0);
    assert(backward(6) == 15);
    assert(constant_if() == 42);
    assert(constant_while() == 77);
    assert(into_switch(1) == 7);
    assert(into_switch(9) == 7);
    assert(reused_a(10) == 11);
    assert(reused_b(10) == 12);

    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
    puts("C17 goto and label execution test passed");
    return 0;
}
