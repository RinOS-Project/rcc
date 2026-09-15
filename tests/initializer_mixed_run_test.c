#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
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

static void verify_globals(ObjectFile* object)
{
    static const uint8_t expected_rows[8] = {'a', 0, 0, 0,
                                             'b', 'c', 0, 0};
    static const uint8_t expected_mixed_rows[8] = {'a', 0, 0, 0,
                                                   'b', 'c', 0, 0};
    static const int32_t expected_struct[4] = {0, 5, 0, 7};
    static const int32_t expected_array[4] = {1, 2, 3, 4};
    const uint8_t* rows = global_bytes(object, "string_rows",
                                       sizeof(expected_rows));
    const uint8_t* mixed_rows = global_bytes(object, "mixed_string_rows",
                                              sizeof(expected_mixed_rows));
    const uint8_t* inferred_rows = global_bytes(object, "inferred_string_rows",
                                                sizeof(expected_rows));
    const uint8_t* aggregate = global_bytes(object, "mixed_designators",
                                             sizeof(expected_struct));
    const uint8_t* array = global_bytes(object, "mixed_array",
                                         sizeof(expected_array));
    assert(memcmp(rows, expected_rows, sizeof(expected_rows)) == 0);
    assert(memcmp(mixed_rows, expected_mixed_rows,
                  sizeof(expected_mixed_rows)) == 0);
    assert(memcmp(inferred_rows, expected_rows, sizeof(expected_rows)) == 0);
    assert(memcmp(aggregate, expected_struct, sizeof(expected_struct)) == 0);
    assert(memcmp(array, expected_array, sizeof(expected_array)) == 0);
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
        ObjSymbol* symbol = objfile_find_symbol(x64, "mixed_initializer_local");
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
        int result = function();
        assert(result == 'x' + 'y' + 'z' + 0 + 0 + 9 + 11 + 1);
        assert(munmap(mapping, mapping_size) == 0);
    }
#endif
    objfile_free(x64);
    return 0;
}
