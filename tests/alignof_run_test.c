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

static void verify_global(ObjectFile* object, uint16_t architecture)
{
    ObjSymbol* symbol = objfile_find_symbol(object, "alignof_global");
    ObjSection* section;
    int32_t value;
    assert(object->arch == architecture);
    assert(symbol != NULL && symbol->section >= 0);
    section = section_at(object, symbol->section);
    assert(section != NULL && section->type == SECT_DATA);
    assert(symbol->value <= section->size);
    assert(sizeof(value) <= section->size - symbol->value);
    memcpy(&value, section->data + (size_t)symbol->value, sizeof(value));
    assert(value == 8);
}

int main(int argc, char** argv)
{
    ObjectFile* x86;
    ObjectFile* x64;
    assert(argc == 3);
    x86 = objfile_read(argv[1]);
    x64 = objfile_read(argv[2]);
    assert(x86 != NULL && x64 != NULL);
    verify_global(x86, ARCH_X86);
    verify_global(x64, ARCH_X64);
    objfile_free(x86);

#if defined(__x86_64__) && !defined(_WIN32)
    {
        ObjSymbol* symbol = objfile_find_symbol(x64, "alignof_value");
        ObjSection* code = section_at(x64, symbol ? symbol->section : -1);
        long page_size = sysconf(_SC_PAGESIZE);
        size_t mapping_size;
        uint8_t* mapping;
        int (*function)(void);
        void* address;
        assert(symbol != NULL && code != NULL && code->type == SECT_CODE);
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
        assert(function() == 12);
        assert(munmap(mapping, mapping_size) == 0);
    }
#endif
    objfile_free(x64);
    return 0;
}
