#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if defined(__x86_64__)
#include <sys/mman.h>
#include <unistd.h>
#endif

static ObjSymbol* function_symbol(ObjectFile* object, const char* name)
{
    ObjSymbol* symbol = objfile_find_symbol(object, name);
    assert(symbol != NULL);
    assert(symbol->section >= 0);
    assert(symbol->binding == BIND_CODE);
    return symbol;
}

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
    long page_size;
    size_t mapping_size;
    uint8_t* mapping;
    int* (*pointer_add)(int*, int);
    int* (*integer_add)(int, int*);
    long (*pointer_distance)(int*, int*);
    int* (*pointer_update)(int*);
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

    assert(pointer_add(values, 2) == values + 2);
    assert(integer_add(3, values) == values + 3);
    assert(pointer_distance(values, values + 3) == 3);
    assert(pointer_update(values) == values + 1);

    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
#else
    (void)argv;
    puts("pointer arithmetic execution test skipped on non-x86_64 host");
#endif
    return 0;
}
