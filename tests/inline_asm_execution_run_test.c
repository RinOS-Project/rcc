#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef int (*binary_function)(int, int);
typedef int (*unary_function)(int);

static ObjSection* code_section(ObjectFile* object)
{
    ObjSection* section = object->sections;
    while (section && section->type != SECT_CODE) section = section->next;
    assert(section != NULL);
    return section;
}

#define LOAD_FUNCTION(target, object, mapping, symbol_name)              \
    do {                                                                 \
        ObjSymbol* symbol = objfile_find_symbol((object), (symbol_name)); \
        void* address;                                                   \
        assert(symbol != NULL && symbol->section >= 0);                  \
        assert(symbol->binding == BIND_CODE);                            \
        address = (mapping) + symbol->value;                             \
        memcpy(&(target), &address, sizeof(target));                     \
    } while (0)

int main(int argc, char** argv)
{
    ObjectFile* object;
    ObjSection* code;
    uint8_t* mapping;
    long page_size;
    size_t mapping_size;
    binary_function roundtrip;
    unary_function read_write;

    assert(argc == 2);
    object = objfile_read(argv[1]);
    assert(object != NULL);
#if defined(__i386__)
    assert(object->arch == ARCH_X86);
#else
    assert(object->arch == ARCH_X64);
#endif
    code = code_section(object);
    page_size = sysconf(_SC_PAGESIZE);
    assert(page_size > 0);
    mapping_size = ((size_t)code->size + (size_t)page_size - 1u) /
                   (size_t)page_size * (size_t)page_size;
    mapping = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(mapping != MAP_FAILED);
    memcpy(mapping, code->data, (size_t)code->size);
    assert(mprotect(mapping, mapping_size, PROT_READ | PROT_EXEC) == 0);

    LOAD_FUNCTION(roundtrip, object, mapping, "asm_fixed_register_roundtrip");
    LOAD_FUNCTION(read_write, object, mapping, "asm_read_write_accumulator");
    assert(roundtrip(37, 91) == 37);
    assert(read_write(53) == 53);
    LOAD_FUNCTION(read_write, object, mapping, "asm_callee_saved_clobber");
    assert(read_write(71) == 71);
#if defined(__i386__)
    {
        int found = 0;
        for (uint64_t index = 0; index + 1u < code->size; ++index) {
            if (code->data[index] == UINT8_C(0xcd) &&
                code->data[index + 1u] == UINT8_C(0x80)) {
                found = 1;
                break;
            }
        }
        assert(found);
    }
#else
    {
        int found = 0;
        for (uint64_t index = 0; index + 1u < code->size; ++index) {
            if (code->data[index] == UINT8_C(0x0f) &&
                code->data[index + 1u] == UINT8_C(0x05)) {
                found = 1;
                break;
            }
        }
        assert(found);
    }
#endif

    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
    puts("fixed-register inline asm execution test passed");
    return 0;
}
