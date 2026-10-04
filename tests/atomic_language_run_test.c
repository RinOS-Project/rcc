#include "objfile.h"

#include <assert.h>
#include <stdint.h>
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

typedef int (RCC_SYSV_ABI *atomic_language_fn)(void);
typedef int (RCC_SYSV_ABI *atomic_language_deref_fn)(int*);

static ObjSymbol* required_function(ObjectFile* object, const char* name) {
    ObjSymbol* symbol = objfile_find_symbol(object, name);
    assert(symbol != NULL && symbol->section >= 0);
    assert(symbol->binding == BIND_CODE);
    return symbol;
}

int main(int argc, char** argv) {
    ObjectFile* object;
    ObjSection* code;
    ObjSymbol* symbol;
    long page_size;
    size_t mapping_size;
    uint8_t* mapping;
    void* address;
    atomic_language_fn surface;
    atomic_language_fn rmw;
    atomic_language_fn bitwise;
    atomic_language_fn arithmetic;
    atomic_language_deref_fn deref;
    int value = 0;
    int inspect_only;

    assert(argc == 2 || (argc == 3 && strcmp(argv[1], "--inspect") == 0));
    inspect_only = argc == 3;
    object = objfile_read(inspect_only ? argv[2] : argv[1]);
    assert(object != NULL);
#if defined(_WIN32) && defined(__x86_64__)
    assert(inspect_only ? object->arch == ARCH_X86 : object->arch == ARCH_X64);
#elif defined(__x86_64__)
    assert(object->arch == ARCH_X64);
#else
    assert(object->arch == ARCH_X86);
#endif
    code = objfile_get_section(object, ".text");
    assert(code != NULL && code->size > 0u && code->relocs == NULL);
    if (inspect_only) {
        (void)required_function(object, "atomic_language_surface");
        (void)required_function(object, "atomic_language_deref");
        (void)required_function(object, "atomic_language_rmw");
        (void)required_function(object, "atomic_language_bitwise");
        (void)required_function(object, "atomic_language_arithmetic");
        objfile_free(object);
        return 0;
    }
    page_size = sysconf(_SC_PAGESIZE);
    assert(page_size > 0);
    mapping_size = (((size_t)code->size + (size_t)page_size - 1u) /
                    (size_t)page_size) * (size_t)page_size;
    mapping = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(mapping != MAP_FAILED);
    memcpy(mapping, code->data, (size_t)code->size);
    assert(mprotect(mapping, mapping_size, PROT_READ | PROT_EXEC) == 0);
    symbol = required_function(object, "atomic_language_surface");
    address = mapping + symbol->value;
    memcpy(&surface, &address, sizeof(surface));
    symbol = required_function(object, "atomic_language_deref");
    address = mapping + symbol->value;
    memcpy(&deref, &address, sizeof(deref));
    symbol = required_function(object, "atomic_language_rmw");
    address = mapping + symbol->value;
    memcpy(&rmw, &address, sizeof(rmw));
    symbol = required_function(object, "atomic_language_bitwise");
    address = mapping + symbol->value;
    memcpy(&bitwise, &address, sizeof(bitwise));
    symbol = required_function(object, "atomic_language_arithmetic");
    address = mapping + symbol->value;
    memcpy(&arithmetic, &address, sizeof(arithmetic));
    assert(surface() == 0);
    assert(deref(&value) == 7 && value == 7);
    assert(rmw() == 0);
    assert(bitwise() == 0);
    assert(arithmetic() == 0);
    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
    return 0;
}
