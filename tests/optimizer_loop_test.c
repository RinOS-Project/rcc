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
    assert(function_extent(optimized, "loop_constant_two") <
           function_extent(unoptimized, "loop_constant_two"));
    assert(function_extent(optimized, "loop_compound_increment") <
           function_extent(unoptimized, "loop_compound_increment"));
    assert(function_extent(optimized, "loop_assignment_increment") <
           function_extent(unoptimized, "loop_assignment_increment"));
    assert(function_extent(optimized, "loop_volatile_increment") ==
           function_extent(unoptimized, "loop_volatile_increment"));
    assert(function_extent(optimized, "loop_descending_two") <
           function_extent(unoptimized, "loop_descending_two"));
    assert(function_extent(optimized, "loop_descending_two_unsigned") <
           function_extent(unoptimized, "loop_descending_two_unsigned"));
    /* Unrolling three iterations can increase code size on this backend;
     * semantic execution below is the regression check for that case. */
    assert(function_extent(optimized, "loop_constant_three_le") > 0);
    assert(function_extent(optimized, "loop_constant_zero") <
           function_extent(unoptimized, "loop_constant_zero"));
    assert(function_extent(optimized, "loop_constant_zero_le") <
           function_extent(unoptimized, "loop_constant_zero_le"));
    assert(function_extent(optimized, "loop_constant_zero_unsigned") <
           function_extent(unoptimized, "loop_constant_zero_unsigned"));
    assert(function_extent(optimized, "do_constant_zero") <
           function_extent(unoptimized, "do_constant_zero"));

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
        ObjSymbol* two_symbol = objfile_find_symbol(
            optimized, "loop_constant_two");
        ObjSymbol* compound_symbol = objfile_find_symbol(
            optimized, "loop_compound_increment");
        ObjSymbol* assignment_symbol = objfile_find_symbol(
            optimized, "loop_assignment_increment");
        ObjSymbol* volatile_symbol = objfile_find_symbol(
            optimized, "loop_volatile_increment");
        ObjSymbol* descending_two_symbol = objfile_find_symbol(
            optimized, "loop_descending_two");
        ObjSymbol* descending_two_unsigned_symbol = objfile_find_symbol(
            optimized, "loop_descending_two_unsigned");
        ObjSymbol* three_le_symbol = objfile_find_symbol(
            optimized, "loop_constant_three_le");
        ObjSymbol* zero_symbol = objfile_find_symbol(
            optimized, "loop_constant_zero");
        ObjSymbol* zero_le_symbol = objfile_find_symbol(
            optimized, "loop_constant_zero_le");
        ObjSymbol* zero_unsigned_symbol = objfile_find_symbol(
            optimized, "loop_constant_zero_unsigned");
        ObjSymbol* do_zero_symbol = objfile_find_symbol(
            optimized, "do_constant_zero");
        ObjSymbol* do_zero_continue_symbol = objfile_find_symbol(
            optimized, "do_constant_zero_continue");
        long page_size = sysconf(_SC_PAGESIZE);
        size_t mapping_size;
        uint8_t* mapping;
        int (*while_function)(void);
        int (*for_function)(void);
        int (*mutate_function)(int);
        int (*one_function)(void);
        int (*one_le_function)(void);
        int (*two_function)(void);
        int (*compound_function)(void);
        int (*assignment_function)(void);
        int (*volatile_function)(void);
        int (*descending_two_function)(void);
        int (*descending_two_unsigned_function)(void);
        int (*three_le_function)(void);
        int (*zero_function)(void);
        int (*zero_le_function)(void);
        int (*zero_unsigned_function)(void);
        int (*do_zero_function)(void);
        int (*do_zero_continue_function)(void);
        void* address;
        assert(code != NULL && while_symbol != NULL && for_symbol != NULL &&
               mutate_symbol != NULL && one_symbol != NULL &&
               one_le_symbol != NULL && two_symbol != NULL &&
               compound_symbol != NULL && assignment_symbol != NULL &&
               volatile_symbol != NULL &&
               descending_two_symbol != NULL &&
               descending_two_unsigned_symbol != NULL &&
               three_le_symbol != NULL && zero_symbol != NULL &&
               zero_le_symbol != NULL && zero_unsigned_symbol != NULL &&
               do_zero_symbol != NULL && do_zero_continue_symbol != NULL &&
               page_size > 0);
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
        address = mapping + two_symbol->value;
        memcpy(&two_function, &address, sizeof(two_function));
        address = mapping + compound_symbol->value;
        memcpy(&compound_function, &address, sizeof(compound_function));
        address = mapping + assignment_symbol->value;
        memcpy(&assignment_function, &address, sizeof(assignment_function));
        address = mapping + volatile_symbol->value;
        memcpy(&volatile_function, &address, sizeof(volatile_function));
        address = mapping + descending_two_symbol->value;
        memcpy(&descending_two_function, &address,
               sizeof(descending_two_function));
        address = mapping + descending_two_unsigned_symbol->value;
        memcpy(&descending_two_unsigned_function, &address,
               sizeof(descending_two_unsigned_function));
        address = mapping + three_le_symbol->value;
        memcpy(&three_le_function, &address, sizeof(three_le_function));
        address = mapping + zero_symbol->value;
        memcpy(&zero_function, &address, sizeof(zero_function));
        address = mapping + zero_le_symbol->value;
        memcpy(&zero_le_function, &address, sizeof(zero_le_function));
        address = mapping + zero_unsigned_symbol->value;
        memcpy(&zero_unsigned_function, &address,
               sizeof(zero_unsigned_function));
        address = mapping + do_zero_symbol->value;
        memcpy(&do_zero_function, &address, sizeof(do_zero_function));
        address = mapping + do_zero_continue_symbol->value;
        memcpy(&do_zero_continue_function, &address,
               sizeof(do_zero_continue_function));
        assert(while_function() == 7);
        assert(for_function() == 11);
        assert(mutate_function(0) == 0);
        assert(mutate_function(3) == 0);
        assert(one_function() == 17);
        assert(one_le_function() == 19);
        assert(two_function() == 26);
        assert(compound_function() == 46);
        assert(assignment_function() == 58);
        assert(volatile_function() == 62);
        assert(descending_two_function() == 74);
        assert(descending_two_unsigned_function() == 82);
        assert(three_le_function() == 21);
        assert(zero_function() == 5);
        assert(zero_le_function() == 7);
        assert(zero_unsigned_function() == 11);
        assert(do_zero_function() == 37);
        assert(do_zero_continue_function() == 41);
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
