#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef int (*signed_shift_function)(int, uint64_t);
typedef uint64_t (*width_shift_function)(uint32_t, uint64_t);
typedef int (*short_shift_function)(unsigned short, uint64_t);
typedef uint32_t (*unsigned_shift_function)(uint32_t, uint64_t);
typedef int (*void_function)(void);
typedef int (*short_function)(unsigned short);
typedef int (*signed_char_function)(signed char);

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
    signed_shift_function signed_comparison;
    width_shift_function unsigned_width;
    short_shift_function unsigned_short_shift;
    unsigned_shift_function unsigned_right;
    void_function type_signed;
    void_function type_unsigned;
    short_function type_short;
    signed_char_function type_signed_char;
    short_function type_unsigned_short;
    short_function bitnot_short;

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

    LOAD_FUNCTION(signed_comparison, object, mapping,
                  "shift_signed_comparison");
    LOAD_FUNCTION(unsigned_width, object, mapping,
                  "shift_unsigned_int_width");
    LOAD_FUNCTION(unsigned_short_shift, object, mapping,
                  "shift_unsigned_short");
    LOAD_FUNCTION(unsigned_right, object, mapping, "shift_unsigned_right");
    LOAD_FUNCTION(type_signed, object, mapping, "shift_type_signed");
    LOAD_FUNCTION(type_unsigned, object, mapping, "shift_type_unsigned");
    LOAD_FUNCTION(type_short, object, mapping, "shift_type_short");
    LOAD_FUNCTION(type_signed_char, object, mapping,
                  "unary_type_signed_char");
    LOAD_FUNCTION(type_unsigned_short, object, mapping,
                  "unary_type_unsigned_short");
    LOAD_FUNCTION(bitnot_short, object, mapping, "bitnot_unsigned_short");

    assert(signed_comparison(-8, UINT64_C(2)) == 1);
    assert(signed_comparison(8, UINT64_C(2)) == 0);
    assert(unsigned_width(UINT32_C(0x80000000), UINT64_C(1)) == 0u);
    assert(unsigned_width(UINT32_C(3), UINT64_C(4)) == UINT64_C(48));
    assert(unsigned_short_shift(UINT16_C(0x8000), UINT64_C(1)) == 65536);
    assert(unsigned_right(UINT32_C(0x80000000), UINT64_C(31)) == 1u);
    assert(type_signed() == 1);
    assert(type_unsigned() == 1);
    assert(type_short(3u) == 1);
    assert(type_signed_char(-3) == 1);
    assert(type_unsigned_short(3u) == 1);
    assert(bitnot_short(UINT16_MAX) == -65536);

    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
    puts("C17 integer promotion execution test passed");
    return 0;
}
