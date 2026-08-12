#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#if !defined(_WIN32) && \
    (defined(__x86_64__) || defined(__i386__))
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

static uint64_t function_extent(ObjectFile* object, const char* name)
{
    ObjSection* code = code_section(object);
    ObjSymbol* function = function_symbol(object, name);
    uint64_t end = code->size;
    for (ObjSymbol* symbol = object->symbols; symbol; symbol = symbol->next) {
        if (symbol->binding == BIND_CODE &&
            symbol->section == function->section &&
            symbol->value > function->value && symbol->value < end) {
            end = symbol->value;
        }
    }
    assert(end >= function->value);
    return end - function->value;
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
    assert(function_extent(optimized, "folded_unsigned_wrap") <
           function_extent(unoptimized, "folded_unsigned_wrap"));
    assert(function_extent(optimized, "folded_unsigned_divmod") <
           function_extent(unoptimized, "folded_unsigned_divmod"));
    assert(function_extent(optimized, "folded_unsigned_shift") <
           function_extent(unoptimized, "folded_unsigned_shift"));
    assert(function_extent(optimized, "folded_unsigned_32") <
           function_extent(unoptimized, "folded_unsigned_32"));
    assert(function_extent(optimized, "folded_unsigned_narrow") <
           function_extent(unoptimized, "folded_unsigned_narrow"));
    assert(function_extent(optimized, "folded_unsigned_unary") <
           function_extent(unoptimized, "folded_unsigned_unary"));
    assert(function_extent(optimized, "folded_mixed_unsigned_comparison") <
           function_extent(unoptimized, "folded_mixed_unsigned_comparison"));
    assert(function_extent(optimized, "removed_after_return") <
           function_extent(unoptimized, "removed_after_return"));
    assert(function_extent(optimized, "removed_after_goto") <
           function_extent(unoptimized, "removed_after_goto"));
    assert(function_extent(optimized, "removed_after_break") <
           function_extent(unoptimized, "removed_after_break"));
    assert(function_extent(optimized, "removed_after_continue") <
           function_extent(unoptimized, "removed_after_continue"));
    objfile_free(unoptimized);
    objfile_free(optimized);
}

int main(int argc, char** argv)
{
    assert(argc == 5);
    verify_smaller(argv[1], argv[2], ARCH_X86);
    verify_smaller(argv[3], argv[4], ARCH_X64);
#if !defined(_WIN32) && \
    (defined(__x86_64__) || defined(__i386__))
    {
#if defined(__i386__)
        const char* execution_path = argv[2];
        const uint16_t execution_architecture = ARCH_X86;
#else
        const char* execution_path = argv[4];
        const uint16_t execution_architecture = ARCH_X64;
#endif
        ObjectFile* object = objfile_read(execution_path);
        assert(object != NULL && object->arch == execution_architecture);
        ObjSection* code = code_section(object);
        ObjSymbol* arithmetic_symbol = function_symbol(
            object, "folded_arithmetic");
        ObjSymbol* choice_symbol = function_symbol(object, "folded_choice");
        ObjSymbol* short_circuit_symbol = function_symbol(
            object, "folded_short_circuit");
        ObjSymbol* unsigned_wrap_symbol = function_symbol(
            object, "folded_unsigned_wrap");
        ObjSymbol* unsigned_divmod_symbol = function_symbol(
            object, "folded_unsigned_divmod");
        ObjSymbol* unsigned_shift_symbol = function_symbol(
            object, "folded_unsigned_shift");
        ObjSymbol* unsigned_32_symbol = function_symbol(
            object, "folded_unsigned_32");
        ObjSymbol* unsigned_narrow_symbol = function_symbol(
            object, "folded_unsigned_narrow");
        ObjSymbol* unsigned_unary_symbol = function_symbol(
            object, "folded_unsigned_unary");
        ObjSymbol* mixed_unsigned_comparison_symbol = function_symbol(
            object, "folded_mixed_unsigned_comparison");
        ObjSymbol* removed_after_return_symbol = function_symbol(
            object, "removed_after_return");
        ObjSymbol* removed_after_goto_symbol = function_symbol(
            object, "removed_after_goto");
        ObjSymbol* preserved_nested_label_symbol = function_symbol(
            object, "preserved_nested_label");
        ObjSymbol* removed_after_break_symbol = function_symbol(
            object, "removed_after_break");
        ObjSymbol* removed_after_continue_symbol = function_symbol(
            object, "removed_after_continue");
        ObjSymbol* preserved_case_after_break_symbol = function_symbol(
            object, "preserved_case_after_break");
        ObjSymbol* branch_symbol = function_symbol(object, "folded_branch");
        ObjSymbol* loop_symbol = function_symbol(object, "removed_loop");
        ObjSymbol* for_symbol = function_symbol(object, "removed_for_loop");
        ObjSymbol* case_loop_symbol = function_symbol(
            object, "preserved_case_loop");
        ObjSymbol* case_for_symbol = function_symbol(
            object, "preserved_case_for");
        long page_size = sysconf(_SC_PAGESIZE);
        size_t mapping_size;
        uint8_t* mapping;
        int (*folded_arithmetic)(void);
        int (*folded_choice)(int);
        int (*folded_short_circuit)(int*);
        uint64_t (*folded_unsigned_wrap)(void);
        uint64_t (*folded_unsigned_divmod)(void);
        uint64_t (*folded_unsigned_shift)(void);
        uint32_t (*folded_unsigned_32)(void);
        uint32_t (*folded_unsigned_narrow)(void);
        uint64_t (*folded_unsigned_unary)(void);
        int (*folded_mixed_unsigned_comparison)(void);
        int (*removed_after_return)(int*);
        int (*removed_after_goto)(int*);
        int (*preserved_nested_label)(int);
        int (*removed_after_break)(int*);
        int (*removed_after_continue)(int*);
        int (*preserved_case_after_break)(int);
        int (*folded_branch)(int*);
        int (*removed_loop)(int*);
        int (*removed_for_loop)(int*);
        int (*preserved_case_loop)(int);
        int (*preserved_case_for)(int);
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
        address = mapping + unsigned_wrap_symbol->value;
        memcpy(&folded_unsigned_wrap, &address,
               sizeof(folded_unsigned_wrap));
        address = mapping + unsigned_divmod_symbol->value;
        memcpy(&folded_unsigned_divmod, &address,
               sizeof(folded_unsigned_divmod));
        address = mapping + unsigned_shift_symbol->value;
        memcpy(&folded_unsigned_shift, &address,
               sizeof(folded_unsigned_shift));
        address = mapping + unsigned_32_symbol->value;
        memcpy(&folded_unsigned_32, &address,
               sizeof(folded_unsigned_32));
        address = mapping + unsigned_narrow_symbol->value;
        memcpy(&folded_unsigned_narrow, &address,
               sizeof(folded_unsigned_narrow));
        address = mapping + unsigned_unary_symbol->value;
        memcpy(&folded_unsigned_unary, &address,
               sizeof(folded_unsigned_unary));
        address = mapping + mixed_unsigned_comparison_symbol->value;
        memcpy(&folded_mixed_unsigned_comparison, &address,
               sizeof(folded_mixed_unsigned_comparison));
        address = mapping + removed_after_return_symbol->value;
        memcpy(&removed_after_return, &address,
               sizeof(removed_after_return));
        address = mapping + removed_after_goto_symbol->value;
        memcpy(&removed_after_goto, &address, sizeof(removed_after_goto));
        address = mapping + preserved_nested_label_symbol->value;
        memcpy(&preserved_nested_label, &address,
               sizeof(preserved_nested_label));
        address = mapping + removed_after_break_symbol->value;
        memcpy(&removed_after_break, &address, sizeof(removed_after_break));
        address = mapping + removed_after_continue_symbol->value;
        memcpy(&removed_after_continue, &address,
               sizeof(removed_after_continue));
        address = mapping + preserved_case_after_break_symbol->value;
        memcpy(&preserved_case_after_break, &address,
               sizeof(preserved_case_after_break));
        address = mapping + branch_symbol->value;
        memcpy(&folded_branch, &address, sizeof(folded_branch));
        address = mapping + loop_symbol->value;
        memcpy(&removed_loop, &address, sizeof(removed_loop));
        address = mapping + for_symbol->value;
        memcpy(&removed_for_loop, &address, sizeof(removed_for_loop));
        address = mapping + case_loop_symbol->value;
        memcpy(&preserved_case_loop, &address, sizeof(preserved_case_loop));
        address = mapping + case_for_symbol->value;
        memcpy(&preserved_case_for, &address, sizeof(preserved_case_for));
        assert(folded_arithmetic() == 19);
        assert(folded_choice(7) == 42);
        assert(folded_short_circuit(&value) == 1);
        assert(value == 3);
        assert(folded_unsigned_wrap() == UINT64_C(3));
        assert(folded_unsigned_divmod() ==
               UINT64_C(0x100000000000000e));
        assert(folded_unsigned_shift() ==
               UINT64_C(0x8000000000000001));
        assert(folded_unsigned_32() == UINT32_C(1));
        assert(folded_unsigned_narrow() == UINT32_C(5));
        assert(folded_unsigned_unary() == UINT64_C(0));
        assert(folded_mixed_unsigned_comparison() == 0);
        assert(removed_after_return(&value) == 7);
        assert(value == 3);
        assert(removed_after_goto(&value) == 3);
        assert(value == 3);
        assert(preserved_nested_label(0) == 23);
        assert(preserved_nested_label(1) == 23);
        value = 0;
        assert(removed_after_break(&value) == 1);
        assert(value == 1);
        value = 0;
        assert(removed_after_continue(&value) == 3);
        assert(value == 3);
        assert(preserved_case_after_break(0) == 0);
        assert(preserved_case_after_break(3) == 33);
        assert(folded_branch(&value) == 5);
        assert(value == 3);
        assert(removed_loop(&value) == 3);
        assert(value == 3);
        assert(removed_for_loop(&value) == 5);
        assert(value == 5);
        assert(preserved_case_loop(0) == 0);
        assert(preserved_case_loop(1) == 17);
        assert(preserved_case_for(0) == 0);
        assert(preserved_case_for(2) == 29);

        assert(munmap(mapping, mapping_size) == 0);
        objfile_free(object);
    }
#endif
    return 0;
}
