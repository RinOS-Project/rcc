#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#if defined(_WIN64)
#define RINOS_ABI __attribute__((sysv_abi))
#else
#define RINOS_ABI
#endif

typedef unsigned long long RINOS_ABI (*Bswap64Function)(unsigned long long);
typedef unsigned short RINOS_ABI (*Bswap16Function)(unsigned short);
typedef unsigned int RINOS_ABI (*Bswap32Function)(unsigned int);
typedef void* RINOS_ABI (*AssumeAlignedFunction)(void*);

static void* map_text(const ObjSection* text, size_t* mapped_size)
{
    size_t page;
    size_t size;
    void* memory;
#ifdef _WIN32
    SYSTEM_INFO system_info;
    DWORD previous;
    GetSystemInfo(&system_info);
    page = (size_t)system_info.dwPageSize;
#else
    long host_page = sysconf(_SC_PAGESIZE);
    page = host_page > 0 ? (size_t)host_page : 0u;
#endif
    assert(text != NULL && text->data != NULL && text->size != 0u);
    assert(page != 0u && text->size <= SIZE_MAX - page + 1u);
    size = ((size_t)text->size + page - 1u) & ~(page - 1u);
#ifdef _WIN32
    memory = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT,
                          PAGE_READWRITE);
#else
    memory = mmap(NULL, size, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) memory = NULL;
#endif
    assert(memory != NULL);
    memcpy(memory, text->data, (size_t)text->size);
#ifdef _WIN32
    assert(VirtualProtect(memory, size, PAGE_EXECUTE_READ, &previous));
#else
    assert(mprotect(memory, size, PROT_READ | PROT_EXEC) == 0);
#endif
    *mapped_size = size;
    return memory;
}

static void unmap_text(void* memory, size_t size)
{
#ifdef _WIN32
    (void)size;
    assert(VirtualFree(memory, 0, MEM_RELEASE));
#else
    assert(munmap(memory, size) == 0);
#endif
}

int main(int argc, char** argv)
{
    ObjectFile* object;
    ObjSection* text;
    ObjSymbol* symbol;
    ObjSymbol* symbol16;
    ObjSymbol* symbol32;
    ObjSymbol* assume_symbol;
    ObjSymbol* assume_offset_symbol;
    Bswap64Function function;
    Bswap16Function function16;
    Bswap32Function function32;
    AssumeAlignedFunction assume_function;
    AssumeAlignedFunction assume_offset_function;
    void* memory;
    void* address;
    size_t mapped_size;
    assert(argc == 2);
    object = objfile_read(argv[1]);
    assert(object != NULL && object->arch == ARCH_X64);
    text = objfile_get_section(object, ".text");
    symbol = objfile_find_symbol(object, "verified_builtin_bswap64");
    symbol16 = objfile_find_symbol(object, "verified_builtin_bswap16");
    symbol32 = objfile_find_symbol(object, "verified_builtin_bswap32");
    assume_symbol = objfile_find_symbol(
        object, "verified_builtin_assume_aligned");
    assume_offset_symbol = objfile_find_symbol(
        object, "verified_builtin_assume_aligned_offset");
    assert(text != NULL && symbol != NULL && symbol->section == 0 &&
           symbol16 != NULL && symbol16->section == 0 &&
           symbol32 != NULL && symbol32->section == 0 &&
           assume_symbol != NULL && assume_symbol->section == 0 &&
           assume_offset_symbol != NULL &&
           assume_offset_symbol->section == 0);
    memory = map_text(text, &mapped_size);
    address = (uint8_t*)memory + symbol->value;
    memcpy(&function, &address, sizeof(function));
    assert(function(UINT64_C(0x0102030405060708)) ==
           UINT64_C(0x0807060504030201));
    address = (uint8_t*)memory + symbol16->value;
    memcpy(&function16, &address, sizeof(function16));
    assert(function16((unsigned short)0x1234u) == (unsigned short)0x3412u);
    address = (uint8_t*)memory + symbol32->value;
    memcpy(&function32, &address, sizeof(function32));
    assert(function32(0x12345678u) == 0x78563412u);
    address = (uint8_t*)memory + assume_symbol->value;
    memcpy(&assume_function, &address, sizeof(assume_function));
    address = (uint8_t*)memory + assume_offset_symbol->value;
    memcpy(&assume_offset_function, &address,
           sizeof(assume_offset_function));
    assert(assume_function((void*)(uintptr_t)0x12345000u) ==
           (void*)(uintptr_t)0x12345000u);
    assert(assume_offset_function((void*)(uintptr_t)0x12345004u) ==
           (void*)(uintptr_t)0x12345004u);
    unmap_text(memory, mapped_size);
    objfile_free(object);
    puts("Verified backend bswap64 execution passed");
    return 0;
}
