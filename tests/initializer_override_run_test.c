#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#if defined(__x86_64__) && !defined(_WIN32)
#include <sys/mman.h>
#include <unistd.h>
#endif

static ObjSection* section_at(ObjectFile* object, int index)
{
    ObjSection* section = object->sections;
    while (section && index-- > 0) section = section->next;
    return section;
}

static const uint8_t* global_bytes(ObjectFile* object, const char* name,
                                   size_t size)
{
    ObjSymbol* symbol = objfile_find_symbol(object, name);
    ObjSection* owner;
    assert(symbol != NULL && symbol->section >= 0);
    owner = section_at(object, symbol->section);
    assert(owner != NULL && owner->type == SECT_DATA);
    assert(symbol->value <= owner->size);
    assert(size <= owner->size - symbol->value);
    return owner->data + (size_t)symbol->value;
}

static ObjSection* code_section(ObjectFile* object)
{
    for (ObjSection* section = object->sections; section;
         section = section->next) {
        if (section->type == SECT_CODE) return section;
    }
    assert(!"code section was not found");
    return NULL;
}

static void verify_globals(ObjectFile* object)
{
    static const int32_t expected_array[] = {9, 8, 0, 0};
    static const int32_t expected_pair[] = {7, 8};
    static const int32_t expected_nested[] = {4, 0, 6, 0};
    int32_t actual[4];
    const uint8_t* choice;

    memcpy(actual, global_bytes(object, "override_array", sizeof(actual)),
           sizeof(actual));
    assert(memcmp(actual, expected_array, sizeof(actual)) == 0);
    memcpy(actual, global_bytes(object, "override_pair",
                                sizeof(expected_pair)),
           sizeof(expected_pair));
    assert(memcmp(actual, expected_pair, sizeof(expected_pair)) == 0);
    choice = global_bytes(object, "override_choice", sizeof(int32_t));
    assert(choice[0] == (uint8_t)'R');
    memcpy(actual, global_bytes(object, "override_nested",
                                sizeof(expected_nested)),
           sizeof(expected_nested));
    assert(memcmp(actual, expected_nested, sizeof(expected_nested)) == 0);
}

int main(int argc, char** argv)
{
    ObjectFile* x86;
    ObjectFile* x64;
    assert(argc == 3);
    x86 = objfile_read(argv[1]);
    x64 = objfile_read(argv[2]);
    assert(x86 != NULL && x86->arch == ARCH_X86);
    assert(x64 != NULL && x64->arch == ARCH_X64);
    verify_globals(x86);
    verify_globals(x64);
    objfile_free(x86);

#if defined(__x86_64__) && !defined(_WIN32)
    {
        ObjSection* code = code_section(x64);
        ObjSymbol* symbol = objfile_find_symbol(
            x64, "local_initializer_override");
        long page_size = sysconf(_SC_PAGESIZE);
        size_t mapping_size;
        uint8_t* mapping;
        int (*function)(void);
        void* address;
        assert(symbol != NULL && symbol->section >= 0);
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
        assert(function() == 39);
        assert(munmap(mapping, mapping_size) == 0);
    }
#endif
    objfile_free(x64);
    return 0;
}
