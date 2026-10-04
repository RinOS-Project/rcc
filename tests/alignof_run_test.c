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
static long rcc_sysconf(int name)
{
    SYSTEM_INFO system_info;
    (void)name;
    GetSystemInfo(&system_info);
    return (long)system_info.dwPageSize;
}

static void* rcc_mmap(void* address, size_t length, int protection, int flags,
                      int descriptor, long offset)
{
    (void)address;
    (void)protection;
    (void)flags;
    (void)descriptor;
    (void)offset;
    return VirtualAlloc(NULL, length, MEM_RESERVE | MEM_COMMIT,
                        PAGE_READWRITE);
}

static int rcc_mprotect(void* address, size_t length, int protection)
{
    DWORD old_protection;
    (void)protection;
    return VirtualProtect(address, length, PAGE_EXECUTE_READ,
                          &old_protection) ? 0 : -1;
}

static int rcc_munmap(void* address, size_t length)
{
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

typedef int (RCC_SYSV_ABI *alignof_function)(void);

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

static ObjSection* section_at(ObjectFile* object, int index)
{
    ObjSection* section = object->sections;
    while (section && index-- > 0) section = section->next;
    return section;
}

static void verify_global(ObjectFile* object, uint16_t architecture)
{
    ObjSymbol* symbol = objfile_find_symbol(object, "alignof_global");
    ObjSection* section;
    int32_t value;
    assert(object->arch == architecture);
    assert(symbol != NULL && symbol->section >= 0);
    section = section_at(object, symbol->section);
    assert(section != NULL && section->type == SECT_DATA);
    assert(symbol->value <= section->size);
    assert(sizeof(value) <= section->size - symbol->value);
    memcpy(&value, section->data + (size_t)symbol->value, sizeof(value));
    assert(value == 8);
}

int main(int argc, char** argv)
{
    ObjectFile* x86;
    ObjectFile* x64;
    assert(argc == 3 || (argc == 4 && strcmp(argv[1], "--inspect") == 0));
    x86 = objfile_read(argc == 4 ? argv[2] : argv[1]);
    x64 = objfile_read(argc == 4 ? argv[3] : argv[2]);
    assert(x86 != NULL && x64 != NULL);
    verify_global(x86, ARCH_X86);
    verify_global(x64, ARCH_X64);

#if defined(_WIN32) && defined(__x86_64__)
    if (argc == 4) {
        ObjSection* code = code_section(x86);
        assert(code->size > 0u);
        (void)required_function(x86, "alignof_value");
        objfile_free(x86);
        objfile_free(x64);
        puts("i686 _Alignof object and symbol inspection passed");
        return 0;
    }
#elif defined(__i386__)
    assert(argc == 3);
#else
    assert(argc == 3);
#endif
    objfile_free(x86);

#if defined(__x86_64__)
    {
        ObjSymbol* symbol = objfile_find_symbol(x64, "alignof_value");
        ObjSection* code = section_at(x64, symbol ? symbol->section : -1);
        long page_size = sysconf(_SC_PAGESIZE);
        size_t mapping_size;
        uint8_t* mapping;
        alignof_function function;
        void* address;
        assert(symbol != NULL && code != NULL && code->type == SECT_CODE);
        assert(page_size > 0);
        mapping_size = (((size_t)code->size + (size_t)page_size - 1u) /
                        (size_t)page_size) * (size_t)page_size;
        mapping = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(mapping != MAP_FAILED);
        memcpy(mapping, code->data, (size_t)code->size);
        assert(mprotect(mapping, mapping_size, PROT_READ | PROT_EXEC) == 0);
        address = mapping + symbol->value;
        memcpy(&function, &address, sizeof(function));
        assert(function() == 12);
        assert(munmap(mapping, mapping_size) == 0);
    }
#endif
    objfile_free(x64);
    return 0;
}
