#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#if defined(__x86_64__) && defined(__GNUC__)
#define RCC_TEST_TARGET_ABI __attribute__((sysv_abi))
#else
#define RCC_TEST_TARGET_ABI
#endif
#else
#include <sys/mman.h>
#include <unistd.h>
#define RCC_TEST_TARGET_ABI
#endif

typedef int (RCC_TEST_TARGET_ABI *binary_function)(int, int);
typedef int (RCC_TEST_TARGET_ABI *unary_function)(int);
#if defined(__x86_64__)
typedef long long (RCC_TEST_TARGET_ABI *unary_i64_function)(long long);
#endif

static ObjSection* code_section(ObjectFile* object)
{
    ObjSection* section = object->sections;
    while (section && section->type != SECT_CODE) section = section->next;
    assert(section != NULL);
    return section;
}

#define LOAD_FUNCTION(target, object, mapping, symbol_name)              \
    do {                                                                 \
        ObjSymbol* symbol = objfile_find_symbol((object), (symbol_name)); \
        void* address;                                                   \
        assert(symbol != NULL && symbol->section >= 0);                  \
        assert(symbol->binding == BIND_CODE);                            \
        address = (mapping) + symbol->value;                             \
        memcpy(&(target), &address, sizeof(target));                     \
    } while (0)

int main(int argc, char** argv)
{
    ObjectFile* object;
    ObjSection* code;
    uint8_t* mapping;
    size_t page_size;
    size_t mapping_size;
    binary_function roundtrip;
    unary_function placeholder;
    unary_function generic_output;
    unary_function generic_read_write;
    unary_function general_input;
    unary_function general_output;
    unary_function general_read_write;
    unary_function read_write;

    assert(argc == 2);
    object = objfile_read(argv[1]);
    assert(object != NULL);
#if defined(__i386__)
    assert(object->arch == ARCH_X86);
#else
    assert(object->arch == ARCH_X64);
#endif
    code = code_section(object);
#if defined(_WIN32)
    {
        SYSTEM_INFO system_info;
        GetSystemInfo(&system_info);
        page_size = system_info.dwPageSize;
    }
#else
    {
        long host_page_size = sysconf(_SC_PAGESIZE);
        assert(host_page_size > 0);
        page_size = (size_t)host_page_size;
    }
#endif
    assert(page_size > 0);
    mapping_size = ((size_t)code->size + page_size - 1u) /
                   page_size * page_size;
#if defined(_WIN32)
    mapping = VirtualAlloc(NULL, mapping_size, MEM_COMMIT | MEM_RESERVE,
                           PAGE_READWRITE);
#else
    mapping = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(mapping != MAP_FAILED);
#endif
    assert(mapping != NULL);
    memcpy(mapping, code->data, (size_t)code->size);
#if defined(_WIN32)
    {
        DWORD old_protect;
        assert(VirtualProtect(mapping, mapping_size, PAGE_EXECUTE_READ,
                              &old_protect));
    }
#else
    assert(mprotect(mapping, mapping_size, PROT_READ | PROT_EXEC) == 0);
#endif

    LOAD_FUNCTION(roundtrip, object, mapping, "asm_fixed_register_roundtrip");
    LOAD_FUNCTION(placeholder, object, mapping, "asm_placeholder_move");
    LOAD_FUNCTION(generic_output, object, mapping, "asm_generic_output_move");
    LOAD_FUNCTION(generic_read_write, object, mapping, "asm_generic_read_write");
    LOAD_FUNCTION(general_input, object, mapping, "asm_general_input_move");
    LOAD_FUNCTION(general_output, object, mapping, "asm_general_output_move");
    LOAD_FUNCTION(general_read_write, object, mapping, "asm_general_read_write");
    LOAD_FUNCTION(read_write, object, mapping, "asm_read_write_accumulator");
    assert(roundtrip(37, 91) == 37);
    assert(placeholder(83) == 83);
    assert(generic_output(97) == 97);
    assert(generic_read_write(109) == 109);
    assert(general_input(127) == 127);
    assert(general_output(131) == 131);
    assert(general_read_write(137) == 137);
    assert(read_write(53) == 53);
    LOAD_FUNCTION(read_write, object, mapping, "asm_callee_saved_clobber");
    assert(read_write(71) == 71);
#if defined(__i386__)
    {
        int found = 0;
        for (uint64_t index = 0; index + 1u < code->size; ++index) {
            if (code->data[index] == UINT8_C(0xcd) &&
                code->data[index + 1u] == UINT8_C(0x80)) {
                found = 1;
                break;
            }
        }
        assert(found);
    }
#else
    {
        int found = 0;
        for (uint64_t index = 0; index + 1u < code->size; ++index) {
            if (code->data[index] == UINT8_C(0x0f) &&
                code->data[index + 1u] == UINT8_C(0x05)) {
                found = 1;
                break;
            }
        }
        assert(found);
    }
    {
        unary_i64_function q_modifier;
        LOAD_FUNCTION(q_modifier, object, mapping, "asm_x64_q_modifier");
        assert(q_modifier(0x1122334455667788LL) == 0x1122334455667788LL);
    }
#endif

#if defined(_WIN32)
    assert(VirtualFree(mapping, 0, MEM_RELEASE));
#else
    assert(munmap(mapping, mapping_size) == 0);
#endif
    objfile_free(object);
    puts("inline asm execution test passed");
    return 0;
}
