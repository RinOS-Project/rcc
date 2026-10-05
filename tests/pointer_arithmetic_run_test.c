#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(__x86_64__) && defined(_WIN32)
#include <windows.h>
#elif defined(__x86_64__)
#include <sys/mman.h>
#include <unistd.h>
#endif

#if defined(__x86_64__) && defined(_WIN32)
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

#if defined(_WIN32) && defined(__x86_64__)
#define RCC_SYSV_ABI __attribute__((sysv_abi))
#else
#define RCC_SYSV_ABI
#endif

static ObjSymbol* function_symbol(ObjectFile* object, const char* name)
{
    ObjSymbol* symbol = objfile_find_symbol(object, name);
    assert(symbol != NULL);
    assert(symbol->section >= 0);
    assert(symbol->binding == BIND_CODE);
    return symbol;
}

struct LocalAggregate {
    int first;
    int second;
    char third;
};

int main(int argc, char** argv)
{
    assert(argc == 2);
#if defined(__x86_64__)
    ObjectFile* object = objfile_read(argv[1]);
    ObjSection* code;
    ObjSymbol* add_symbol;
    ObjSymbol* reverse_add_symbol;
    ObjSymbol* distance_symbol;
    ObjSymbol* update_symbol;
    ObjSymbol* local_array_symbol;
    ObjSymbol* large_array_symbol;
    ObjSymbol* aggregate_symbol;
    ObjSymbol* aggregate_initializer_symbol;
    long page_size;
    size_t mapping_size;
    uint8_t* mapping;
    int* (RCC_SYSV_ABI *pointer_add)(int*, int);
    int* (RCC_SYSV_ABI *integer_add)(int, int*);
    long (RCC_SYSV_ABI *pointer_distance)(int*, int*);
    int* (RCC_SYSV_ABI *pointer_update)(int*);
    int (RCC_SYSV_ABI *local_array_value)(void);
    int (RCC_SYSV_ABI *large_local_array_value)(void);
    int (RCC_SYSV_ABI *aggregate_parameter_value)(struct LocalAggregate);
    int (RCC_SYSV_ABI *local_aggregate_initializer_value)(void);
    int values[4] = {1, 2, 3, 4};
    void* address;

    assert(object != NULL);
    assert(object->arch == ARCH_X64);
    code = objfile_get_section(object, ".text");
    assert(code != NULL && code->size > 0u);
    add_symbol = function_symbol(object, "pointer_add");
    reverse_add_symbol = function_symbol(object, "integer_add");
    distance_symbol = function_symbol(object, "pointer_distance");
    update_symbol = function_symbol(object, "pointer_update");
    local_array_symbol = function_symbol(object, "local_array_value");
    large_array_symbol = function_symbol(object, "large_local_array_value");
    aggregate_symbol = function_symbol(object, "aggregate_parameter_value");
    aggregate_initializer_symbol = function_symbol(
        object, "local_aggregate_initializer_value");
    page_size = sysconf(_SC_PAGESIZE);
    assert(page_size > 0);
    mapping_size = (((size_t)code->size + (size_t)page_size - 1u) /
                    (size_t)page_size) * (size_t)page_size;
    mapping = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(mapping != MAP_FAILED);
    memcpy(mapping, code->data, (size_t)code->size);
    assert(mprotect(mapping, mapping_size, PROT_READ | PROT_EXEC) == 0);

    address = mapping + add_symbol->value;
    memcpy(&pointer_add, &address, sizeof(pointer_add));
    address = mapping + reverse_add_symbol->value;
    memcpy(&integer_add, &address, sizeof(integer_add));
    address = mapping + distance_symbol->value;
    memcpy(&pointer_distance, &address, sizeof(pointer_distance));
    address = mapping + update_symbol->value;
    memcpy(&pointer_update, &address, sizeof(pointer_update));
    address = mapping + local_array_symbol->value;
    memcpy(&local_array_value, &address, sizeof(local_array_value));
    address = mapping + large_array_symbol->value;
    memcpy(&large_local_array_value, &address,
           sizeof(large_local_array_value));
    address = mapping + aggregate_symbol->value;
    memcpy(&aggregate_parameter_value, &address,
           sizeof(aggregate_parameter_value));
    address = mapping + aggregate_initializer_symbol->value;
    memcpy(&local_aggregate_initializer_value, &address,
           sizeof(local_aggregate_initializer_value));

    assert(pointer_add(values, 2) == values + 2);
    assert(integer_add(3, values) == values + 3);
    assert(pointer_distance(values, values + 3) == 3);
    assert(pointer_update(values) == values + 1);
    assert(local_array_value() == 'R');
    assert(large_local_array_value() == 'L');
    {
        struct LocalAggregate value = {10, 20, 3};
        assert(aggregate_parameter_value(value) == 33);
    }
    assert(local_aggregate_initializer_value() == 264);

    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
#else
    (void)argv;
    puts("pointer arithmetic execution test skipped on non-x86_64 host");
#endif
    return 0;
}
