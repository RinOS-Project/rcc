#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef int (*atomic_language_fn)(void);
typedef int (*atomic_language_deref_fn)(int*);

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
    atomic_language_deref_fn deref;
    int value = 0;

    assert(argc == 2);
    object = objfile_read(argv[1]);
    assert(object != NULL);
#if defined(__x86_64__)
    assert(object->arch == ARCH_X64);
#else
    assert(object->arch == ARCH_X86);
#endif
    code = objfile_get_section(object, ".text");
    assert(code != NULL && code->size > 0u && code->relocs == NULL);
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
    assert(surface() == 0);
    assert(deref(&value) == 7 && value == 7);
    assert(rmw() == 0);
    assert(bitwise() == 0);
    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
    return 0;
}
