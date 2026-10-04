#include "objfile.h"

#include <assert.h>
#include <string.h>

#if !defined(_WIN32) && (defined(__x86_64__) || defined(__i386__))
#include <sys/mman.h>
#include <unistd.h>
#endif

static ObjSection* code_section(ObjectFile* object)
{
    for (ObjSection* section = object->sections; section;
         section = section->next) {
        if (section->type == SECT_CODE) return section;
    }
    return NULL;
}

static bool is_internal_label(const char* name)
{
    return name && strstr(name, "__rcc_label_") != NULL;
}

static uint64_t function_extent(ObjectFile* object, const char* name)
{
    ObjSection* code = code_section(object);
    ObjSymbol* function = objfile_find_symbol(object, name);
    uint64_t end;
    assert(code != NULL && function != NULL);
    end = code->size;
    for (ObjSymbol* symbol = object->symbols; symbol;
         symbol = symbol->next) {
        if (symbol->binding == BIND_CODE &&
            !is_internal_label(symbol->name) &&
            symbol->section == function->section &&
            symbol->value > function->value && symbol->value < end) {
            end = symbol->value;
        }
    }
    assert(end >= function->value);
    return end - function->value;
}

static void verify_pair(const char* unoptimized_path,
                        const char* optimized_path,
                        uint16_t architecture)
{
    ObjectFile* unoptimized = objfile_read(unoptimized_path);
    ObjectFile* optimized = objfile_read(optimized_path);
    assert(unoptimized != NULL && optimized != NULL);
    assert(unoptimized->arch == architecture && optimized->arch == architecture);
    assert(code_section(optimized)->size < code_section(unoptimized)->size);
    assert(function_extent(optimized, "loop_mutates_condition") ==
           function_extent(unoptimized, "loop_mutates_condition"));
    assert(function_extent(optimized, "loop_constant_one") <
           function_extent(unoptimized, "loop_constant_one"));
    assert(function_extent(optimized, "loop_constant_one_le") <
           function_extent(unoptimized, "loop_constant_one_le"));

#if !defined(_WIN32) && (defined(__x86_64__) || defined(__i386__))
#if defined(__i386__)
    if (architecture == ARCH_X86)
#else
    if (architecture == ARCH_X64)
#endif
    {
        ObjSection* code = code_section(optimized);
        ObjSymbol* while_symbol = objfile_find_symbol(
            optimized, "loop_invariant_while_zero");
        ObjSymbol* for_symbol = objfile_find_symbol(
            optimized, "loop_invariant_for_zero");
        ObjSymbol* mutate_symbol = objfile_find_symbol(
            optimized, "loop_mutates_condition");
        ObjSymbol* one_symbol = objfile_find_symbol(
            optimized, "loop_constant_one");
        ObjSymbol* one_le_symbol = objfile_find_symbol(
            optimized, "loop_constant_one_le");
        long page_size = sysconf(_SC_PAGESIZE);
        size_t mapping_size;
        uint8_t* mapping;
        int (*while_function)(void);
        int (*for_function)(void);
        int (*mutate_function)(int);
        int (*one_function)(void);
        int (*one_le_function)(void);
        void* address;
        assert(code != NULL && while_symbol != NULL && for_symbol != NULL &&
               mutate_symbol != NULL && one_symbol != NULL &&
               one_le_symbol != NULL && page_size > 0);
        mapping_size = (((size_t)code->size + (size_t)page_size - 1u) /
                        (size_t)page_size) * (size_t)page_size;
        mapping = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(mapping != MAP_FAILED);
        memcpy(mapping, code->data, (size_t)code->size);
        assert(mprotect(mapping, mapping_size, PROT_READ | PROT_EXEC) == 0);
        address = mapping + while_symbol->value;
        memcpy(&while_function, &address, sizeof(while_function));
        address = mapping + for_symbol->value;
        memcpy(&for_function, &address, sizeof(for_function));
        address = mapping + mutate_symbol->value;
        memcpy(&mutate_function, &address, sizeof(mutate_function));
        address = mapping + one_symbol->value;
        memcpy(&one_function, &address, sizeof(one_function));
        address = mapping + one_le_symbol->value;
        memcpy(&one_le_function, &address, sizeof(one_le_function));
        assert(while_function() == 7);
        assert(for_function() == 11);
        assert(mutate_function(0) == 0);
        assert(mutate_function(3) == 0);
        assert(one_function() == 17);
        assert(one_le_function() == 19);
        munmap(mapping, mapping_size);
    }
#endif
    objfile_free(unoptimized);
    objfile_free(optimized);
}

int main(int argc, char** argv)
{
    assert(argc == 5);
    verify_pair(argv[1], argv[2], ARCH_X86);
    verify_pair(argv[3], argv[4], ARCH_X64);
    return 0;
}
