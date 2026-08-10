#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#if defined(__x86_64__) && !defined(_WIN32)
#include <sys/mman.h>
#include <unistd.h>
#endif

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

static void verify_smaller(const char* unoptimized_path,
                           const char* optimized_path,
                           uint16_t architecture)
{
    ObjectFile* unoptimized = objfile_read(unoptimized_path);
    ObjectFile* optimized = objfile_read(optimized_path);
    ObjSection* unoptimized_code;
    ObjSection* optimized_code;
    assert(unoptimized != NULL && optimized != NULL);
    assert(unoptimized->arch == architecture && optimized->arch == architecture);
    unoptimized_code = code_section(unoptimized);
    optimized_code = code_section(optimized);
    assert(optimized_code->size < unoptimized_code->size);
    objfile_free(unoptimized);
    objfile_free(optimized);
}

int main(int argc, char** argv)
{
    assert(argc == 5);
    verify_smaller(argv[1], argv[2], ARCH_X86);
    verify_smaller(argv[3], argv[4], ARCH_X64);
#if defined(__x86_64__) && !defined(_WIN32)
    {
        ObjectFile* object = objfile_read(argv[4]);
        ObjSection* code = code_section(object);
        ObjSymbol* arithmetic_symbol = function_symbol(
            object, "folded_arithmetic");
        ObjSymbol* choice_symbol = function_symbol(object, "folded_choice");
        ObjSymbol* short_circuit_symbol = function_symbol(
            object, "folded_short_circuit");
        long page_size = sysconf(_SC_PAGESIZE);
        size_t mapping_size;
        uint8_t* mapping;
        int (*folded_arithmetic)(void);
        int (*folded_choice)(int);
        int (*folded_short_circuit)(int*);
        void* address;
        int value = 3;
        assert(page_size > 0);
        mapping_size = (((size_t)code->size + (size_t)page_size - 1u) /
                        (size_t)page_size) * (size_t)page_size;
        mapping = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(mapping != MAP_FAILED);
        memcpy(mapping, code->data, (size_t)code->size);
        assert(mprotect(mapping, mapping_size, PROT_READ | PROT_EXEC) == 0);

        address = mapping + arithmetic_symbol->value;
        memcpy(&folded_arithmetic, &address, sizeof(folded_arithmetic));
        address = mapping + choice_symbol->value;
        memcpy(&folded_choice, &address, sizeof(folded_choice));
        address = mapping + short_circuit_symbol->value;
        memcpy(&folded_short_circuit, &address,
               sizeof(folded_short_circuit));
        assert(folded_arithmetic() == 19);
        assert(folded_choice(7) == 42);
        assert(folded_short_circuit(&value) == 1);
        assert(value == 3);

        assert(munmap(mapping, mapping_size) == 0);
        objfile_free(object);
    }
#endif
    return 0;
}
