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

typedef unsigned long long RINOS_ABI (*ObjectSizeFunction)(void);
typedef unsigned long long RINOS_ABI (*ObjectSizeUnknownFunction)(
    const unsigned char*);

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
    ObjSymbol* array_symbol;
    ObjSymbol* offset_symbol;
    ObjSymbol* string_symbol;
    ObjSymbol* unknown_symbol;
    ObjSymbol* unknown_zero_symbol;
    ObjectSizeFunction array_function;
    ObjectSizeFunction offset_function;
    ObjectSizeFunction string_function;
    ObjectSizeUnknownFunction unknown_function;
    ObjectSizeUnknownFunction unknown_zero_function;
    void* memory;
    size_t mapped_size;
    void* address;

    assert(argc == 2);
    object = objfile_read(argv[1]);
    assert(object != NULL && object->arch == ARCH_X64);
    text = objfile_get_section(object, ".text");
    array_symbol = objfile_find_symbol(object, "verified_object_size_array");
    offset_symbol = objfile_find_symbol(
        object, "verified_object_size_array_offset");
    string_symbol = objfile_find_symbol(object, "verified_object_size_string");
    unknown_symbol = objfile_find_symbol(
        object, "verified_object_size_unknown");
    unknown_zero_symbol = objfile_find_symbol(
        object, "verified_object_size_unknown_zero");
    assert(text != NULL && array_symbol != NULL && offset_symbol != NULL &&
           string_symbol != NULL && unknown_symbol != NULL &&
           unknown_zero_symbol != NULL);
    memory = map_text(text, &mapped_size);

    address = (uint8_t*)memory + array_symbol->value;
    memcpy(&array_function, &address, sizeof(array_function));
    address = (uint8_t*)memory + offset_symbol->value;
    memcpy(&offset_function, &address, sizeof(offset_function));
    address = (uint8_t*)memory + string_symbol->value;
    memcpy(&string_function, &address, sizeof(string_function));
    address = (uint8_t*)memory + unknown_symbol->value;
    memcpy(&unknown_function, &address, sizeof(unknown_function));
    address = (uint8_t*)memory + unknown_zero_symbol->value;
    memcpy(&unknown_zero_function, &address, sizeof(unknown_zero_function));

    assert(array_function() == 16u);
    assert(offset_function() == 12u);
    assert(string_function() == 6u);
    assert(unknown_function(NULL) == UINT64_MAX);
    assert(unknown_zero_function(NULL) == 0u);

    unmap_text(memory, mapped_size);
    objfile_free(object);
    puts("Verified backend object-size execution passed");
    return 0;
}
