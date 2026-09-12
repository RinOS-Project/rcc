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

static ObjSection* code_section(ObjectFile* object)
{
    for (ObjSection* section = object->sections; section;
         section = section->next) {
        if (section->type == SECT_CODE) return section;
    }
    assert(!"code section was not found");
    return NULL;
}

static ObjSymbol* function_symbol(ObjectFile* object, const char* name)
{
    ObjSymbol* symbol = objfile_find_symbol(object, name);
    assert(symbol != NULL && symbol->section >= 0);
    assert(symbol->binding == BIND_CODE);
    return symbol;
}

static ObjectFile* verify_object(const char* path, uint16_t architecture)
{
    ObjectFile* object = objfile_read(path);
    ObjSymbol* global;
    ObjSection* owner;
    int32_t value;
    assert(object != NULL && object->arch == architecture);
    global = objfile_find_symbol(object, "generic_global");
    assert(global != NULL && global->section >= 0);
    owner = section_at(object, global->section);
    assert(owner != NULL && owner->type == SECT_DATA);
    assert(global->value <= owner->size);
    assert(sizeof(value) <= owner->size - global->value);
    memcpy(&value, owner->data + (size_t)global->value, sizeof(value));
    assert(value == 22);
    return object;
}

int main(int argc, char** argv)
{
    ObjectFile* x86;
    ObjectFile* x64;
    assert(argc == 3);
    x86 = verify_object(argv[1], ARCH_X86);
    x64 = verify_object(argv[2], ARCH_X64);
    objfile_free(x86);

#if defined(__x86_64__) && !defined(_WIN32)
    {
        ObjSection* code = code_section(x64);
        const char* names[] = {
            "generic_int", "generic_string", "generic_array",
            "generic_side_effect", "generic_lvalue", "generic_function"
        };
        int expected[] = {5, 13, 14, 7, 9, 16};
        long page_size = sysconf(_SC_PAGESIZE);
        size_t mapping_size;
        uint8_t* mapping;
        assert(page_size > 0);
        mapping_size = (((size_t)code->size + (size_t)page_size - 1u) /
                        (size_t)page_size) * (size_t)page_size;
        mapping = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(mapping != MAP_FAILED);
        memcpy(mapping, code->data, (size_t)code->size);
        assert(mprotect(mapping, mapping_size, PROT_READ | PROT_EXEC) == 0);

        for (size_t index = 0; index < sizeof(names) / sizeof(names[0]);
             ++index) {
            ObjSymbol* symbol = function_symbol(x64, names[index]);
            int (*function)(void);
            void* address = mapping + symbol->value;
            memcpy(&function, &address, sizeof(function));
            assert(function() == expected[index]);
        }
        {
            ObjSymbol* symbol = function_symbol(x64, "generic_pointer");
            int (*function)(int*);
            void* address = mapping + symbol->value;
            memcpy(&function, &address, sizeof(function));
            assert(function(NULL) == 8);
        }
        {
            ObjSymbol* symbol = function_symbol(x64, "generic_default");
            int (*function)(char**);
            void* address = mapping + symbol->value;
            memcpy(&function, &address, sizeof(function));
            assert(function(NULL) == 12);
        }
        assert(munmap(mapping, mapping_size) == 0);
    }
#endif
    objfile_free(x64);
    return 0;
}
