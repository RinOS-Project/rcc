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

typedef int RINOS_ABI (*BitCount32Function)(unsigned int);
typedef int RINOS_ABI (*BitCount64Function)(unsigned long long);

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

static void* function_address(const ObjSection* text,
                              const ObjSymbol* symbol, void* memory)
{
    assert(symbol != NULL && symbol->section == 0 &&
           symbol->value <= text->size);
    return (uint8_t*)memory + symbol->value;
}

int main(int argc, char** argv)
{
    ObjectFile* object;
    ObjSection* text;
    ObjSymbol* clz32_symbol;
    ObjSymbol* ctz32_symbol;
    ObjSymbol* pop32_symbol;
    ObjSymbol* clz64_symbol;
    ObjSymbol* ctz64_symbol;
    ObjSymbol* pop64_symbol;
    BitCount32Function clz32;
    BitCount32Function ctz32;
    BitCount32Function pop32;
    BitCount64Function clz64;
    BitCount64Function ctz64;
    BitCount64Function pop64;
    void* memory;
    size_t mapped_size;
    void* address;
    assert(argc == 2);
    object = objfile_read(argv[1]);
    assert(object != NULL && object->arch == ARCH_X64);
    text = objfile_get_section(object, ".text");
    clz32_symbol = objfile_find_symbol(object, "verified_builtin_clz32");
    ctz32_symbol = objfile_find_symbol(object, "verified_builtin_ctz32");
    pop32_symbol = objfile_find_symbol(object, "verified_builtin_popcount32");
    clz64_symbol = objfile_find_symbol(object, "verified_builtin_clz64");
    ctz64_symbol = objfile_find_symbol(object, "verified_builtin_ctz64");
    pop64_symbol = objfile_find_symbol(object, "verified_builtin_popcount64");
    assert(text != NULL && clz32_symbol != NULL &&
           ctz32_symbol != NULL && pop32_symbol != NULL &&
           clz64_symbol != NULL && ctz64_symbol != NULL &&
           pop64_symbol != NULL);
    memory = map_text(text, &mapped_size);
    address = function_address(text, clz32_symbol, memory);
    memcpy(&clz32, &address, sizeof(clz32));
    address = function_address(text, ctz32_symbol, memory);
    memcpy(&ctz32, &address, sizeof(ctz32));
    address = function_address(text, pop32_symbol, memory);
    memcpy(&pop32, &address, sizeof(pop32));
    address = function_address(text, clz64_symbol, memory);
    memcpy(&clz64, &address, sizeof(clz64));
    address = function_address(text, ctz64_symbol, memory);
    memcpy(&ctz64, &address, sizeof(ctz64));
    address = function_address(text, pop64_symbol, memory);
    memcpy(&pop64, &address, sizeof(pop64));
    assert(clz32(0x00100000u) == 11);
    assert(clz32(0x80000000u) == 0);
    assert(ctz32(0x00001000u) == 12);
    assert(ctz32(0x80000000u) == 31);
    assert(pop32(0xf0f00f0fu) == 16);
    assert(pop32(0xffffffffu) == 32);
    assert(clz64(UINT64_C(1)) == 63);
    assert(clz64(UINT64_C(0x8000000000000000)) == 0);
    assert(ctz64(UINT64_C(1) << 40) == 40);
    assert(ctz64(UINT64_C(0x8000000000000000)) == 63);
    assert(pop64(UINT64_C(0xf00000000000000f)) == 8);
    assert(pop64(UINT64_MAX) == 64);
    unmap_text(memory, mapped_size);
    objfile_free(object);
    puts("Verified backend bit-count execution passed");
    return 0;
}
