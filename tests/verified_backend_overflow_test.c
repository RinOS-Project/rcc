#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
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

typedef int RINOS_ABI (*SignedOverflowFunction)(int, int, int*);
typedef int RINOS_ABI (*UnsignedOverflowFunction)(unsigned int,
                                                   unsigned int,
                                                   unsigned int*);

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
    ObjSymbol* add_signed_symbol;
    ObjSymbol* add_unsigned_symbol;
    ObjSymbol* sub_signed_symbol;
    ObjSymbol* sub_unsigned_symbol;
    SignedOverflowFunction add_signed;
    UnsignedOverflowFunction add_unsigned;
    SignedOverflowFunction sub_signed;
    UnsignedOverflowFunction sub_unsigned;
    void* memory;
    size_t mapped_size;
    void* address;
    int signed_result;
    unsigned int unsigned_result;

    assert(argc == 2);
    object = objfile_read(argv[1]);
    assert(object != NULL && object->arch == ARCH_X64);
    text = objfile_get_section(object, ".text");
    add_signed_symbol = objfile_find_symbol(object, "verified_add_signed");
    add_unsigned_symbol = objfile_find_symbol(
        object, "verified_add_unsigned");
    sub_signed_symbol = objfile_find_symbol(object, "verified_sub_signed");
    sub_unsigned_symbol = objfile_find_symbol(
        object, "verified_sub_unsigned");
    assert(text != NULL && add_signed_symbol != NULL &&
           add_unsigned_symbol != NULL && sub_signed_symbol != NULL &&
           sub_unsigned_symbol != NULL);
    memory = map_text(text, &mapped_size);

    address = (uint8_t*)memory + add_signed_symbol->value;
    memcpy(&add_signed, &address, sizeof(add_signed));
    address = (uint8_t*)memory + add_unsigned_symbol->value;
    memcpy(&add_unsigned, &address, sizeof(add_unsigned));
    address = (uint8_t*)memory + sub_signed_symbol->value;
    memcpy(&sub_signed, &address, sizeof(sub_signed));
    address = (uint8_t*)memory + sub_unsigned_symbol->value;
    memcpy(&sub_unsigned, &address, sizeof(sub_unsigned));

    signed_result = 0;
    assert(add_signed(10, 20, &signed_result) == 0);
    assert(signed_result == 30);
    assert(add_signed(INT32_MAX, 1, &signed_result) == 1);
    assert(signed_result == INT32_MIN);
    unsigned_result = 0u;
    assert(add_unsigned(UINT32_MAX, 1u, &unsigned_result) == 1);
    assert(unsigned_result == 0u);
    assert(sub_signed(30, 10, &signed_result) == 0);
    assert(signed_result == 20);
    assert(sub_signed(INT32_MIN, 1, &signed_result) == 1);
    assert(signed_result == INT32_MAX);
    assert(sub_unsigned(0u, 1u, &unsigned_result) == 1);
    assert(unsigned_result == UINT32_MAX);

    unmap_text(memory, mapped_size);
    objfile_free(object);
    puts("Verified backend checked add/sub execution passed");
    return 0;
}
