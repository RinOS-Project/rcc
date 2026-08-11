#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef uint32_t (*unsigned_compound_function)(uint32_t*, uint32_t);
typedef int32_t (*signed_compound_function)(int32_t*, int32_t);
typedef uint32_t (*wide_rhs_compound_function)(uint32_t*, uint64_t);
typedef int (*uchar_compound_function)(unsigned char*, unsigned);
typedef int (*schar_compound_function)(signed char*, int);
typedef int (*ushort_compound_function)(unsigned short*, unsigned);
typedef uint32_t (*once_compound_function)(uint32_t*, uint32_t, int*);
typedef uint32_t (*unsigned_binary_function)(uint32_t, uint32_t);

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
    unsigned_compound_function unsigned_multiply;
    unsigned_compound_function unsigned_divide_assign;
    unsigned_compound_function unsigned_modulo_assign;
    wide_rhs_compound_function unsigned_divide_wide;
    wide_rhs_compound_function unsigned_modulo_wide;
    unsigned_compound_function unsigned_and;
    unsigned_compound_function unsigned_or;
    unsigned_compound_function unsigned_xor;
    unsigned_compound_function unsigned_shift_left;
    unsigned_compound_function unsigned_shift_right;
    signed_compound_function signed_divide;
    signed_compound_function signed_modulo;
    signed_compound_function signed_shift_right;
    uchar_compound_function uchar_compound;
    schar_compound_function schar_compound;
    ushort_compound_function ushort_compound;
    once_compound_function once_compound;
    unsigned_binary_function unsigned_divide;
    unsigned_binary_function unsigned_modulo;

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

    LOAD_FUNCTION(unsigned_multiply, object, mapping,
                  "compound_unsigned_multiply");
    LOAD_FUNCTION(unsigned_divide_assign, object, mapping,
                  "compound_unsigned_divide");
    LOAD_FUNCTION(unsigned_modulo_assign, object, mapping,
                  "compound_unsigned_modulo");
    LOAD_FUNCTION(unsigned_divide_wide, object, mapping,
                  "compound_unsigned_divide_wide");
    LOAD_FUNCTION(unsigned_modulo_wide, object, mapping,
                  "compound_unsigned_modulo_wide");
    LOAD_FUNCTION(unsigned_and, object, mapping, "compound_unsigned_and");
    LOAD_FUNCTION(unsigned_or, object, mapping, "compound_unsigned_or");
    LOAD_FUNCTION(unsigned_xor, object, mapping, "compound_unsigned_xor");
    LOAD_FUNCTION(unsigned_shift_left, object, mapping,
                  "compound_unsigned_shift_left");
    LOAD_FUNCTION(unsigned_shift_right, object, mapping,
                  "compound_unsigned_shift_right");
    LOAD_FUNCTION(signed_divide, object, mapping, "compound_signed_divide");
    LOAD_FUNCTION(signed_modulo, object, mapping, "compound_signed_modulo");
    LOAD_FUNCTION(signed_shift_right, object, mapping,
                  "compound_signed_shift_right");
    LOAD_FUNCTION(uchar_compound, object, mapping,
                  "compound_unsigned_char");
    LOAD_FUNCTION(schar_compound, object, mapping, "compound_signed_char");
    LOAD_FUNCTION(ushort_compound, object, mapping,
                  "compound_unsigned_short");
    LOAD_FUNCTION(once_compound, object, mapping, "compound_lvalue_once");
    LOAD_FUNCTION(unsigned_divide, object, mapping,
                  "ordinary_unsigned_divide");
    LOAD_FUNCTION(unsigned_modulo, object, mapping,
                  "ordinary_unsigned_modulo");

    {
        uint32_t value = UINT32_C(0x80000003);
        assert(unsigned_multiply(&value, 3u) == UINT32_C(0x80000009));
        value = UINT32_C(0xf0000001);
        assert(unsigned_divide_assign(&value, 3u) == UINT32_C(0x50000000));
        value = UINT32_C(0xf0000002);
        assert(unsigned_modulo_assign(&value, 7u) == 4u);
        value = 10u;
        assert(unsigned_divide_wide(&value, UINT64_C(0x100000000)) == 0u);
        value = 10u;
        assert(unsigned_modulo_wide(&value, UINT64_C(0x100000000)) == 10u);
        value = UINT32_C(0xf0f00ff0);
        assert(unsigned_and(&value, UINT32_C(0x0ff0ffff)) ==
               UINT32_C(0x00f00ff0));
        assert(unsigned_or(&value, UINT32_C(0x80000001)) ==
               UINT32_C(0x80f00ff1));
        assert(unsigned_xor(&value, UINT32_C(0x00ff00ff)) ==
               UINT32_C(0x800f0f0e));
        value = UINT32_C(0x40000001);
        assert(unsigned_shift_left(&value, 1u) == UINT32_C(0x80000002));
        assert(unsigned_shift_right(&value, 31u) == 1u);
    }
    {
        int32_t value = -21;
        assert(signed_divide(&value, 4) == -5);
        value = -21;
        assert(signed_modulo(&value, 4) == -1);
        value = -INT32_C(0x40000000);
        assert(signed_shift_right(&value, 30) == -1);
    }
    {
        unsigned char value = 250u;
        assert(uchar_compound(&value, 10u) == 4);
        assert(value == 4u);
    }
    {
        signed char value = 100;
        assert(schar_compound(&value, 2) == -56);
        assert(value == -56);
    }
    {
        unsigned short value = UINT16_C(0x8001);
        assert(ushort_compound(&value, 1u) == 2);
        assert(value == 2u);
    }
    {
        uint32_t value = UINT32_C(0xa5a5f00f);
        int calls = 0;
        assert(once_compound(&value, UINT32_C(0x00ff00ff), &calls) ==
               UINT32_C(0xa55af0f0));
        assert(value == UINT32_C(0xa55af0f0));
        assert(calls == 1);
    }
    assert(unsigned_divide(UINT32_C(0xf0000001), 3u) ==
           UINT32_C(0x50000000));
    assert(unsigned_modulo(UINT32_C(0xf0000002), 7u) == 4u);

    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
    puts("C17 compound assignment execution test passed");
    return 0;
}
