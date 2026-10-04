#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#if defined(_WIN32) && defined(__x86_64__)
#define RCC_SYSV_ABI __attribute__((sysv_abi))
#else
#define RCC_SYSV_ABI
#endif

#if defined(_WIN32)
static long rcc_sysconf(int name) {
    SYSTEM_INFO system_info;
    (void)name;
    GetSystemInfo(&system_info);
    return (long)system_info.dwPageSize;
}

static void* rcc_mmap(void* address, size_t length, int protection, int flags,
                      int descriptor, long offset) {
    (void)address;
    (void)protection;
    (void)flags;
    (void)descriptor;
    (void)offset;
    return VirtualAlloc(NULL, length, MEM_RESERVE | MEM_COMMIT,
                        PAGE_READWRITE);
}

static int rcc_mprotect(void* address, size_t length, int protection) {
    DWORD old_protection;
    (void)length;
    (void)protection;
    return VirtualProtect(address, length, PAGE_EXECUTE_READ,
                          &old_protection) ? 0 : -1;
}

static int rcc_munmap(void* address, size_t length) {
    (void)length;
    return VirtualFree(address, 0, MEM_RELEASE) ? 0 : -1;
}

#define sysconf rcc_sysconf
#define mmap rcc_mmap
#define mprotect rcc_mprotect
#define munmap rcc_munmap
#define MAP_FAILED ((void*)-1)
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_PRIVATE 2
#define MAP_ANONYMOUS 0x20
#define _SC_PAGESIZE 30
#endif

typedef uint32_t (RCC_SYSV_ABI *unsigned_compound_function)(uint32_t*, uint32_t);
typedef int32_t (RCC_SYSV_ABI *signed_compound_function)(int32_t*, int32_t);
typedef uint32_t (RCC_SYSV_ABI *wide_rhs_compound_function)(uint32_t*, uint64_t);
typedef int (RCC_SYSV_ABI *uchar_compound_function)(unsigned char*, unsigned);
typedef int (RCC_SYSV_ABI *schar_compound_function)(signed char*, int);
typedef int (RCC_SYSV_ABI *ushort_compound_function)(unsigned short*, unsigned);
typedef uint32_t (RCC_SYSV_ABI *once_compound_function)(uint32_t*, uint32_t, int*);
typedef uint32_t (RCC_SYSV_ABI *unsigned_binary_function)(uint32_t, uint32_t);

static ObjSymbol* required_function(ObjectFile* object, const char* name) {
    ObjSymbol* symbol = objfile_find_symbol(object, name);
    assert(symbol != NULL && symbol->section >= 0);
    assert(symbol->binding == BIND_CODE);
    return symbol;
}

static ObjSection* code_section(ObjectFile* object)
{
    ObjSection* section = object->sections;
    while (section && section->type != SECT_CODE) section = section->next;
    assert(section != NULL);
    return section;
}

#define LOAD_FUNCTION(target, object, mapping, symbol_name)                 \
    do {                                                                    \
        ObjSymbol* symbol = required_function((object), (symbol_name));     \
        void* address;                                                       \
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

    assert(argc == 2 || (argc == 3 && strcmp(argv[1], "--inspect") == 0));
    object = objfile_read(argc == 3 ? argv[2] : argv[1]);
    assert(object != NULL);
#if defined(_WIN32) && defined(__x86_64__)
    assert(argc == 3 ? object->arch == ARCH_X86 : object->arch == ARCH_X64);
#elif defined(__i386__)
    assert(argc == 2 && object->arch == ARCH_X86);
#else
    assert(argc == 2 && object->arch == ARCH_X64);
#endif
    code = code_section(object);
    assert(code->size > 0u);
    if (argc == 3) {
        static const char* const names[] = {
            "compound_unsigned_multiply",
            "compound_unsigned_divide",
            "compound_unsigned_modulo",
            "compound_unsigned_divide_wide",
            "compound_unsigned_modulo_wide",
            "compound_unsigned_and",
            "compound_unsigned_or",
            "compound_unsigned_xor",
            "compound_unsigned_shift_left",
            "compound_unsigned_shift_right",
            "compound_signed_divide",
            "compound_signed_modulo",
            "compound_signed_shift_right",
            "compound_unsigned_char",
            "compound_signed_char",
            "compound_unsigned_short",
            "compound_lvalue_once",
            "ordinary_unsigned_divide",
            "ordinary_unsigned_modulo",
        };
        size_t index;
        for (index = 0u; index < sizeof(names) / sizeof(names[0]); ++index) {
            (void)required_function(object, names[index]);
        }
        objfile_free(object);
        puts("i686 compound assignment object and symbol inspection passed");
        return 0;
    }
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
