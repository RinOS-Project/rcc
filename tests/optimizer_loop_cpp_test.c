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

static uint64_t function_extent(ObjectFile* object, const char* name)
{
    ObjSection* code = code_section(object);
    ObjSymbol* function = objfile_find_symbol(object, name);
    uint64_t end;
    assert(code != NULL && function != NULL);
    end = code->size;
    for (ObjSymbol* symbol = object->symbols; symbol;
         symbol = symbol->next) {
        if (symbol->binding == BIND_CODE && symbol->section == function->section &&
            symbol->value > function->value && symbol->value < end &&
            strstr(symbol->name, "__rcc_label_") == NULL) {
            end = symbol->value;
        }
    }
    assert(end >= function->value);
    return end - function->value;
}

static void verify_pair(const char* unoptimized_path,
                        const char* optimized_path, uint16_t architecture)
{
    static const char* const names[] = {
        "cxx_loop_for_two", "cxx_loop_while_two", "cxx_loop_do_two",
        "cxx_switch_constant_direct", "cxx_switch_constant_fallthrough",
        "cxx_switch_constant_no_match", "cxx_switch_constant_sizeof",
        "cxx_switch_constant_alignof", "cxx_if_constant_sizeof",
        "cxx_while_constant_sizeof", "cxx_for_constant_alignof"
    };
    static const int expected[] = {
        146, 158, 166, 22, 8, 17, 71, 77, 83, 101, 107
    };
    (void)expected;
    ObjectFile* unoptimized = objfile_read(unoptimized_path);
    ObjectFile* optimized = objfile_read(optimized_path);
    assert(unoptimized != NULL && optimized != NULL);
    assert(unoptimized->arch == architecture && optimized->arch == architecture);
    for (size_t index = 0u; index < sizeof(names) / sizeof(names[0]); ++index) {
        assert(function_extent(optimized, names[index]) > 0u);
        assert(function_extent(optimized, names[index]) !=
               function_extent(unoptimized, names[index]));
    }

#if !defined(_WIN32) && (defined(__x86_64__) || defined(__i386__))
#if defined(__i386__)
    if (architecture == ARCH_X86)
#else
    if (architecture == ARCH_X64)
#endif
    {
        ObjSection* code = code_section(optimized);
        long page_size = sysconf(_SC_PAGESIZE);
        size_t mapping_size;
        uint8_t* mapping;
        assert(code != NULL && page_size > 0);
        mapping_size = (((size_t)code->size + (size_t)page_size - 1u) /
                        (size_t)page_size) * (size_t)page_size;
        mapping = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        assert(mapping != MAP_FAILED);
        memcpy(mapping, code->data, (size_t)code->size);
        assert(mprotect(mapping, mapping_size, PROT_READ | PROT_EXEC) == 0);
        for (size_t index = 0u; index < sizeof(names) / sizeof(names[0]);
             ++index) {
            ObjSymbol* symbol = objfile_find_symbol(optimized, names[index]);
            int (*function)(void);
            void* address;
            assert(symbol != NULL);
            address = mapping + symbol->value;
            memcpy(&function, &address, sizeof(function));
            assert(function() == expected[index]);
        }
        assert(munmap(mapping, mapping_size) == 0);
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
