#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef int (*unsigned_compare_function)(unsigned, unsigned);
typedef int (*mixed_compare_function)(int, unsigned);
typedef int (*signed_wide_compare_function)(int64_t, unsigned);
typedef int (*unsigned_wide_compare_function)(uint64_t, int);
typedef int (*truth_function)(uint64_t);

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
    unsigned_compare_function unsigned_lt;
    unsigned_compare_function unsigned_gt;
    unsigned_compare_function unsigned_le;
    unsigned_compare_function unsigned_ge;
    mixed_compare_function mixed_int_unsigned_lt;
    mixed_compare_function mixed_int_unsigned_gt;
    signed_wide_compare_function signed_wide_unsigned_lt;
    unsigned_wide_compare_function unsigned_wide_signed_lt;
    truth_function truth_not;
    truth_function truth_and;
    truth_function truth_or;
    truth_function truth_conditional;
    truth_function truth_if;
    truth_function truth_while;
    truth_function truth_do_while;
    truth_function truth_for;
    uint64_t high_word = UINT64_C(0x100000000);

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

    LOAD_FUNCTION(unsigned_lt, object, mapping, "unsigned_lt");
    LOAD_FUNCTION(unsigned_gt, object, mapping, "unsigned_gt");
    LOAD_FUNCTION(unsigned_le, object, mapping, "unsigned_le");
    LOAD_FUNCTION(unsigned_ge, object, mapping, "unsigned_ge");
    LOAD_FUNCTION(mixed_int_unsigned_lt, object, mapping,
                  "mixed_int_unsigned_lt");
    LOAD_FUNCTION(mixed_int_unsigned_gt, object, mapping,
                  "mixed_int_unsigned_gt");
    LOAD_FUNCTION(signed_wide_unsigned_lt, object, mapping,
                  "signed_wide_unsigned_lt");
    LOAD_FUNCTION(unsigned_wide_signed_lt, object, mapping,
                  "unsigned_wide_signed_lt");
    LOAD_FUNCTION(truth_not, object, mapping, "truth_not");
    LOAD_FUNCTION(truth_and, object, mapping, "truth_and");
    LOAD_FUNCTION(truth_or, object, mapping, "truth_or");
    LOAD_FUNCTION(truth_conditional, object, mapping, "truth_conditional");
    LOAD_FUNCTION(truth_if, object, mapping, "truth_if");
    LOAD_FUNCTION(truth_while, object, mapping, "truth_while");
    LOAD_FUNCTION(truth_do_while, object, mapping, "truth_do_while");
    LOAD_FUNCTION(truth_for, object, mapping, "truth_for");

    assert(unsigned_lt(UINT32_MAX, 1u) == 0);
    assert(unsigned_gt(UINT32_MAX, 1u) == 1);
    assert(unsigned_le(UINT32_MAX, UINT32_MAX) == 1);
    assert(unsigned_ge(UINT32_MAX, 1u) == 1);
    assert(mixed_int_unsigned_lt(-1, 1u) == 0);
    assert(mixed_int_unsigned_gt(-1, 1u) == 1);
    assert(signed_wide_unsigned_lt(-1, UINT32_MAX) == 1);
    assert(unsigned_wide_signed_lt(0, -1) == 1);

    assert(truth_not(0) == 1);
    assert(truth_not(high_word) == 0);
    assert(truth_and(high_word) == 1);
    assert(truth_or(high_word) == 1);
    assert(truth_conditional(high_word) == 13);
    assert(truth_if(high_word) == 19);
    assert(truth_while(high_word) == 1);
    assert(truth_do_while(high_word) == 1);
    assert(truth_for(high_word) == 1);

    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
    puts("C17 scalar comparison and truth execution test passed");
    return 0;
}
