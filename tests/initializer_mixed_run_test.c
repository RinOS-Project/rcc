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
    static const int32_t expected_struct[4] = {0, 5, 7, 0};
    static const int32_t expected_nested_struct[4] = {0, 5, 7, 8};
    static const int32_t expected_designator_before_aggregate[5] = {
        1, 2, 3, 4, 5};
    static const int32_t expected_array[4] = {1, 2, 3, 4};
    const uint8_t* rows = global_bytes(object, "string_rows",
                                       sizeof(expected_rows));
    const uint8_t* mixed_rows = global_bytes(object, "mixed_string_rows",
                                              sizeof(expected_mixed_rows));
    const uint8_t* inferred_rows = global_bytes(object, "inferred_string_rows",
                                                sizeof(expected_rows));
    const uint8_t* aggregate = global_bytes(object, "mixed_designators",
                                             sizeof(expected_struct));
    const uint8_t* nested_aggregate = global_bytes(
        object, "mixed_nested_designators", sizeof(expected_nested_struct));
    const uint8_t* aggregate_followup = global_bytes(
        object, "mixed_designator_before_aggregate",
        sizeof(expected_designator_before_aggregate));
    const uint8_t* array = global_bytes(object, "mixed_array",
                                         sizeof(expected_array));
    assert(memcmp(rows, expected_rows, sizeof(expected_rows)) == 0);
    assert(memcmp(mixed_rows, expected_mixed_rows,
                  sizeof(expected_mixed_rows)) == 0);
    assert(memcmp(inferred_rows, expected_rows, sizeof(expected_rows)) == 0);
    assert(memcmp(aggregate, expected_struct, sizeof(expected_struct)) == 0);
    assert(memcmp(nested_aggregate, expected_nested_struct,
                  sizeof(expected_nested_struct)) == 0);
    assert(memcmp(aggregate_followup, expected_designator_before_aggregate,
                  sizeof(expected_designator_before_aggregate)) == 0);
    assert(memcmp(array, expected_array, sizeof(expected_array)) == 0);
}

#if defined(__x86_64__) && !defined(_WIN32)
static void run_local_initializer(ObjectFile* object)
{
    ObjSection* code = NULL;
    for (ObjSection* section = object->sections; section;
         section = section->next) {
        if (section->type == SECT_CODE) {
            code = section;
            break;
        }
    }
    assert(code != NULL);
    {
        ObjSymbol* symbol = objfile_find_symbol(object,
                                                "mixed_initializer_local");
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
        assert(result == 'x' + 'y' + 'z' + 0 + 0 + 9 + 11 +
                         12 + 13 + 0 + 14 + 15 + 16 +
                         17 + 18 + 19 + 20 +
                         21 + 22 + 23 + 24 + 25 + 1);
        assert(munmap(mapping, mapping_size) == 0);
    }
}
#endif

int main(int argc, char** argv)
{
    ObjectFile* x86;
    ObjectFile* x64;
    ObjectFile* verified_x86;
    ObjectFile* verified_x64;
    assert(argc == 5);
    x86 = objfile_read(argv[1]);
    x64 = objfile_read(argv[2]);
    verified_x86 = objfile_read(argv[3]);
    verified_x64 = objfile_read(argv[4]);
    assert(x86 != NULL && x86->arch == ARCH_X86);
    assert(x64 != NULL && x64->arch == ARCH_X64);
    assert(verified_x86 != NULL && verified_x86->arch == ARCH_X86);
    assert(verified_x64 != NULL && verified_x64->arch == ARCH_X64);
    verify_globals(x86);
    verify_globals(x64);
    verify_globals(verified_x86);
    verify_globals(verified_x64);

#if defined(__x86_64__) && !defined(_WIN32)
    run_local_initializer(x64);
    run_local_initializer(verified_x64);
#endif
    objfile_free(verified_x86);
    objfile_free(verified_x64);
    objfile_free(x86);
    objfile_free(x64);
    return 0;
}
