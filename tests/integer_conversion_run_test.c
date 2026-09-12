#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef uint8_t (*u8_from_u64_function)(uint64_t);
typedef int8_t (*s8_from_u64_function)(uint64_t);
typedef uint16_t (*u16_from_u64_function)(uint64_t);
typedef _Bool (*bool_from_u64_function)(uint64_t);
typedef uint64_t (*u64_from_s8_function)(int8_t);
typedef unsigned (*assign_u8_function)(uint8_t*, unsigned);
typedef int (*assign_s8_function)(int8_t*, int);
typedef int (*assign_bool_function)(_Bool*, uint64_t);
typedef uint8_t (*u8_from_unsigned_function)(unsigned);
typedef uint64_t (*u64_from_int_function)(int);
typedef uint64_t (*u64_from_unsigned_function)(unsigned);
typedef int (*int_from_u64_function)(uint64_t);
typedef unsigned (*unsigned_from_unsigned_function)(unsigned);
typedef uint64_t (*u64_binary_function)(unsigned, unsigned);

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
    u8_from_u64_function cast_u8;
    s8_from_u64_function cast_s8;
    u16_from_u64_function cast_u16;
    bool_from_u64_function cast_bool;
    u64_from_s8_function cast_signed_char_to_u64;
    assign_u8_function assign_u8;
    assign_s8_function assign_s8;
    assign_bool_function assign_bool;
    u8_from_unsigned_function return_u8;
    bool_from_u64_function return_bool;
    u64_from_int_function return_widen_signed;
    u64_from_unsigned_function return_widen_unsigned;
    int_from_u64_function return_truncate_wide;
    u64_from_int_function call_widen_signed;
    u64_from_unsigned_function call_widen_unsigned;
    unsigned_from_unsigned_function call_truncate_u8;
    int_from_u64_function call_bool;
    int_from_u64_function initialize_bool;
    u64_binary_function unsigned_add_then_widen;
    u64_binary_function unsigned_multiply_then_widen;
    uint8_t u8_output = 0;
    int8_t s8_output = 0;
    _Bool bool_output = 0;

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

    LOAD_FUNCTION(cast_u8, object, mapping, "cast_u8");
    LOAD_FUNCTION(cast_s8, object, mapping, "cast_s8");
    LOAD_FUNCTION(cast_u16, object, mapping, "cast_u16");
    LOAD_FUNCTION(cast_bool, object, mapping, "cast_bool");
    LOAD_FUNCTION(cast_signed_char_to_u64, object, mapping,
                  "cast_signed_char_to_u64");
    LOAD_FUNCTION(assign_u8, object, mapping, "assign_u8");
    LOAD_FUNCTION(assign_s8, object, mapping, "assign_s8");
    LOAD_FUNCTION(assign_bool, object, mapping, "assign_bool");
    LOAD_FUNCTION(return_u8, object, mapping, "return_u8");
    LOAD_FUNCTION(return_bool, object, mapping, "return_bool");
    LOAD_FUNCTION(return_widen_signed, object, mapping,
                  "return_widen_signed");
    LOAD_FUNCTION(return_widen_unsigned, object, mapping,
                  "return_widen_unsigned");
    LOAD_FUNCTION(return_truncate_wide, object, mapping,
                  "return_truncate_wide");
    LOAD_FUNCTION(call_widen_signed, object, mapping, "call_widen_signed");
    LOAD_FUNCTION(call_widen_unsigned, object, mapping,
                  "call_widen_unsigned");
    LOAD_FUNCTION(call_truncate_u8, object, mapping, "call_truncate_u8");
    LOAD_FUNCTION(call_bool, object, mapping, "call_bool");
    LOAD_FUNCTION(initialize_bool, object, mapping, "initialize_bool");
    LOAD_FUNCTION(unsigned_add_then_widen, object, mapping,
                  "unsigned_add_then_widen");
    LOAD_FUNCTION(unsigned_multiply_then_widen, object, mapping,
                  "unsigned_multiply_then_widen");

    assert(cast_u8(UINT64_C(0x1234)) == UINT8_C(0x34));
    assert(cast_s8(UINT64_C(0xff)) == INT8_C(-1));
    assert(cast_u16(UINT64_C(0x123456)) == UINT16_C(0x3456));
    assert(cast_bool(0) == 0);
    assert(cast_bool(UINT64_C(0x100000000)) == 1);
    assert(cast_signed_char_to_u64(INT8_C(-1)) == UINT64_MAX);

    assert(assign_u8(&u8_output, UINT32_C(0x1234)) == UINT8_C(0x34));
    assert(u8_output == UINT8_C(0x34));
    assert(assign_s8(&s8_output, 255) == -1);
    assert(s8_output == -1);
    assert(assign_bool(&bool_output, UINT64_C(0x100000000)) == 1);
    assert(bool_output == 1);
    assert(assign_bool(&bool_output, 0) == 0);
    assert(bool_output == 0);

    assert(return_u8(UINT32_C(0x1234)) == UINT8_C(0x34));
    assert(return_bool(UINT64_C(0x100000000)) == 1);
    assert(return_widen_signed(-1) == UINT64_MAX);
    assert(return_widen_unsigned(UINT32_MAX) == UINT64_C(0xffffffff));
    assert(return_truncate_wide(UINT64_C(0x100000001)) == 1);

    assert(call_widen_signed(-1) == UINT64_MAX);
    assert(call_widen_unsigned(UINT32_MAX) == UINT64_C(0xffffffff));
    assert(call_truncate_u8(UINT32_C(0x1234)) == UINT8_C(0x34));
    assert(call_bool(UINT64_C(0x100000000)) == 1);
    assert(initialize_bool(UINT64_C(0x100000000)) == 1);

    assert(unsigned_add_then_widen(UINT32_MAX, 2u) == UINT64_C(1));
    assert(unsigned_multiply_then_widen(UINT32_C(0x80000001), 2u) ==
           UINT64_C(2));

    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
    puts("C17 integer conversion execution test passed");
    return 0;
}
