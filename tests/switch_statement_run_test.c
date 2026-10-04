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

typedef uint32_t (RCC_SYSV_ABI *unsigned_identity_function)(uint32_t);
typedef int32_t (RCC_SYSV_ABI *signed_identity_function)(int32_t);
typedef int (RCC_SYSV_ABI *int_unary_function)(int);
typedef uint64_t (RCC_SYSV_ABI *unsigned_wide_function)(uint64_t);
typedef int64_t (RCC_SYSV_ABI *signed_wide_function)(int64_t);
typedef int (RCC_SYSV_ABI *once_function)(uint32_t*, int*);
typedef int (RCC_SYSV_ABI *nested_function)(int, int);
typedef int (RCC_SYSV_ABI *signed_char_function)(signed char);
typedef int (RCC_SYSV_ABI *unsigned_char_function)(unsigned char);

static ObjSection* code_section(ObjectFile* object)
{
    ObjSection* section = object->sections;
    while (section && section->type != SECT_CODE) section = section->next;
    assert(section != NULL);
    return section;
}

static ObjSymbol* required_function(ObjectFile* object, const char* name)
{
    ObjSymbol* symbol = objfile_find_symbol(object, name);
    assert(symbol != NULL && symbol->section >= 0);
    assert(symbol->binding == BIND_CODE);
    return symbol;
}

#define LOAD_FUNCTION(target, object, mapping, symbol_name)                 \
    do {                                                                    \
        ObjSymbol* symbol = required_function((object), (symbol_name));     \
        void* address;                                                      \
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
    unsigned_identity_function unsigned_identity;
    signed_identity_function signed_identity;
    int_unary_function basic;
    unsigned_wide_function unsigned_wide;
    signed_wide_function signed_wide;
    once_function evaluates_once;
    nested_function nested;
    int_unary_function inside_loop;
    signed_char_function signed_char_switch;
    unsigned_char_function unsigned_char_switch;

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
            "bare_unsigned_identity", "bare_signed_identity", "switch_basic",
            "switch_unsigned_wide", "switch_signed_wide",
            "switch_evaluates_once", "switch_nested", "switch_inside_loop",
            "switch_signed_char", "switch_unsigned_char",
        };
        size_t index;
        for (index = 0u; index < sizeof(names) / sizeof(names[0]); ++index) {
            (void)required_function(object, names[index]);
        }
        objfile_free(object);
        puts("i686 switch object and symbol inspection passed");
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

    LOAD_FUNCTION(unsigned_identity, object, mapping,
                  "bare_unsigned_identity");
    LOAD_FUNCTION(signed_identity, object, mapping, "bare_signed_identity");
    LOAD_FUNCTION(basic, object, mapping, "switch_basic");
    LOAD_FUNCTION(unsigned_wide, object, mapping, "switch_unsigned_wide");
    LOAD_FUNCTION(signed_wide, object, mapping, "switch_signed_wide");
    LOAD_FUNCTION(evaluates_once, object, mapping, "switch_evaluates_once");
    LOAD_FUNCTION(nested, object, mapping, "switch_nested");
    LOAD_FUNCTION(inside_loop, object, mapping, "switch_inside_loop");
    LOAD_FUNCTION(signed_char_switch, object, mapping, "switch_signed_char");
    LOAD_FUNCTION(unsigned_char_switch, object, mapping,
                  "switch_unsigned_char");

    assert(unsigned_identity(UINT32_C(0xf1234567)) ==
           UINT32_C(0xf1234567));
    assert(signed_identity(-1234567) == -1234567);

    assert(basic(-3) == 10);
    assert(basic(0) == 23);
    assert(basic(1) == 4);
    assert(basic(2) == 33);
    assert(basic(3) == 33);
    assert(basic(99) == 99);

    assert(unsigned_wide(UINT64_C(0x100000000)) == UINT64_C(11));
    assert(unsigned_wide(UINT64_C(0x8000000000000000)) == UINT64_C(22));
    assert(unsigned_wide(UINT64_MAX) == UINT64_C(33));
    assert(unsigned_wide(UINT64_C(9)) == UINT64_C(44));
    assert(signed_wide(INT64_C(-1)) == INT64_C(-11));
    assert(signed_wide(-INT64_C(0x100000000)) == INT64_C(-22));
    assert(signed_wide(INT64_C(0x100000000)) == INT64_C(33));
    assert(signed_wide(INT64_C(9)) == INT64_C(44));

    {
        uint32_t value = 7u;
        int calls = 0;
        assert(evaluates_once(&value, &calls) == 70);
        assert(calls == 1);
        value = 8u;
        assert(evaluates_once(&value, &calls) == 90);
        assert(calls == 2);
    }

    assert(nested(1, 2) == 112);
    assert(nested(1, 9) == 115);
    assert(nested(9, 2) == -1);
    assert(inside_loop(5) == 436);
    assert(signed_char_switch(-1) == 1);
    assert(signed_char_switch(127) == 2);
    assert(signed_char_switch(0) == 3);
    assert(unsigned_char_switch(255u) == 1);
    assert(unsigned_char_switch(127u) == 2);
    assert(unsigned_char_switch(0u) == 3);

    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
    puts("C17 switch statement execution test passed");
    return 0;
}
