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

typedef int (RCC_SYSV_ABI *int_unary_function)(int);
typedef int (RCC_SYSV_ABI *int_void_function)(void);

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
    int_unary_function forward;
    int_unary_function backward;
    int_void_function constant_if;
    int_void_function constant_while;
    int_unary_function into_switch;
    int_unary_function reused_a;
    int_unary_function reused_b;

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
            "goto_forward", "goto_backward", "goto_into_constant_if",
            "goto_into_constant_while", "goto_into_switch",
            "goto_reused_label_a", "goto_reused_label_b",
        };
        size_t index;
        for (index = 0u; index < sizeof(names) / sizeof(names[0]); ++index) {
            (void)required_function(object, names[index]);
        }
        objfile_free(object);
        puts("i686 control-flow object and symbol inspection passed");
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
