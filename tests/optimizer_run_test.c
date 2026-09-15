#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#if !defined(_WIN32) && \
    (defined(__x86_64__) || defined(__i386__))
#include <sys/mman.h>
#include <unistd.h>
#endif

static bool is_internal_label(const char* name)
{
    return name && strstr(name, "__rcc_label_") != NULL;
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

static uint64_t function_extent(ObjectFile* object, const char* name)
{
    ObjSection* code = code_section(object);
    ObjSymbol* function = function_symbol(object, name);
    uint64_t end = code->size;
    for (ObjSymbol* symbol = object->symbols; symbol; symbol = symbol->next) {
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
    assert(function_extent(optimized, "removed_pure_expression") <
           function_extent(unoptimized, "removed_pure_expression"));
    assert(function_extent(optimized, "preserved_assignment_expression") ==
           function_extent(unoptimized, "preserved_assignment_expression"));
    assert(function_extent(optimized, "preserved_volatile_read") ==
           function_extent(unoptimized, "preserved_volatile_read"));
    assert(function_extent(optimized, "preserved_postfix_volatile_read") ==
           function_extent(unoptimized,
                           "preserved_postfix_volatile_read"));
    assert(function_extent(optimized, "preserved_volatile_pointer_read") ==
           function_extent(unoptimized,
                           "preserved_volatile_pointer_read"));
    assert(function_extent(optimized, "preserved_call_expression") ==
           function_extent(unoptimized, "preserved_call_expression"));
    assert(function_extent(optimized, "propagated_local_arithmetic") <
           function_extent(unoptimized, "propagated_local_arithmetic"));
    assert(function_extent(optimized, "propagated_local_assignment") <
           function_extent(unoptimized, "propagated_local_assignment"));
    assert(function_extent(optimized, "propagated_unsigned_narrow") <
           function_extent(unoptimized, "propagated_unsigned_narrow"));
    assert(function_extent(optimized, "propagated_local_branch") <
           function_extent(unoptimized, "propagated_local_branch"));
    assert(function_extent(optimized, "propagated_compound_assignment") <
           function_extent(unoptimized, "propagated_compound_assignment"));
    assert(function_extent(optimized, "propagated_increment") <
           function_extent(unoptimized, "propagated_increment"));
    assert(function_extent(optimized, "eliminated_dead_stores") <
           function_extent(unoptimized, "eliminated_dead_stores"));
    assert(function_extent(optimized, "eliminated_overwritten_store") <
           function_extent(unoptimized, "eliminated_overwritten_store"));
    assert(function_extent(optimized, "preserved_dead_volatile_store") ==
           function_extent(unoptimized, "preserved_dead_volatile_store"));
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
        ObjSymbol* removed_pure_expression_symbol = function_symbol(
            object, "removed_pure_expression");
        ObjSymbol* preserved_assignment_expression_symbol = function_symbol(
            object, "preserved_assignment_expression");
        ObjSymbol* preserved_volatile_read_symbol = function_symbol(
            object, "preserved_volatile_read");
        ObjSymbol* preserved_postfix_volatile_read_symbol = function_symbol(
            object, "preserved_postfix_volatile_read");
        ObjSymbol* preserved_volatile_pointer_read_symbol = function_symbol(
            object, "preserved_volatile_pointer_read");
        ObjSymbol* qualified_pointer_levels_symbol = function_symbol(
            object, "qualified_pointer_levels");
        ObjSymbol* propagated_local_arithmetic_symbol = function_symbol(
            object, "propagated_local_arithmetic");
        ObjSymbol* propagated_local_assignment_symbol = function_symbol(
            object, "propagated_local_assignment");
        ObjSymbol* propagated_unsigned_narrow_symbol = function_symbol(
            object, "propagated_unsigned_narrow");
        ObjSymbol* propagated_local_branch_symbol = function_symbol(
            object, "propagated_local_branch");
        ObjSymbol* preserved_call_barrier_symbol = function_symbol(
            object, "preserved_call_barrier");
        ObjSymbol* preserved_address_alias_symbol = function_symbol(
            object, "preserved_address_alias");
        ObjSymbol* preserved_conditional_state_symbol = function_symbol(
            object, "preserved_conditional_state");
        ObjSymbol* preserved_do_state_symbol = function_symbol(
            object, "preserved_do_state");
        ObjSymbol* preserved_while_state_symbol = function_symbol(
            object, "preserved_while_state");
        ObjSymbol* propagated_compound_assignment_symbol = function_symbol(
            object, "propagated_compound_assignment");
        ObjSymbol* propagated_increment_symbol = function_symbol(
            object, "propagated_increment");
        ObjSymbol* eliminated_dead_stores_symbol = function_symbol(
            object, "eliminated_dead_stores");
        ObjSymbol* eliminated_overwritten_store_symbol = function_symbol(
            object, "eliminated_overwritten_store");
        ObjSymbol* preserved_dead_store_effect_symbol = function_symbol(
            object, "preserved_dead_store_effect");
        ObjSymbol* preserved_dead_store_escape_symbol = function_symbol(
            object, "preserved_dead_store_escape");
        ObjSymbol* preserved_dead_volatile_store_symbol = function_symbol(
            object, "preserved_dead_volatile_store");
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
        int (*removed_pure_expression)(int);
        int (*preserved_assignment_expression)(int*);
        int (*preserved_volatile_read)(volatile int*);
        int (*preserved_postfix_volatile_read)(volatile int*);
        int (*preserved_volatile_pointer_read)(int* volatile);
        int (*qualified_pointer_levels)(int*, int*);
        int (*propagated_local_arithmetic)(void);
        int (*propagated_local_assignment)(void);
        uint32_t (*propagated_unsigned_narrow)(void);
        int (*propagated_local_branch)(int*);
        int (*preserved_call_barrier)(void);
        int (*preserved_address_alias)(void);
        int (*preserved_conditional_state)(int);
        int (*preserved_do_state)(void);
        int (*preserved_while_state)(void);
        int (*propagated_compound_assignment)(void);
        int (*propagated_increment)(void);
        int (*eliminated_dead_stores)(void);
        int (*eliminated_overwritten_store)(int);
        int (*preserved_dead_store_effect)(int*);
        int (*preserved_dead_store_escape)(void);
        int (*preserved_dead_volatile_store)(void);
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
        address = mapping + removed_pure_expression_symbol->value;
        memcpy(&removed_pure_expression, &address,
               sizeof(removed_pure_expression));
        address = mapping + preserved_assignment_expression_symbol->value;
        memcpy(&preserved_assignment_expression, &address,
               sizeof(preserved_assignment_expression));
        address = mapping + preserved_volatile_read_symbol->value;
        memcpy(&preserved_volatile_read, &address,
               sizeof(preserved_volatile_read));
        address = mapping + preserved_postfix_volatile_read_symbol->value;
        memcpy(&preserved_postfix_volatile_read, &address,
               sizeof(preserved_postfix_volatile_read));
        address = mapping + preserved_volatile_pointer_read_symbol->value;
        memcpy(&preserved_volatile_pointer_read, &address,
               sizeof(preserved_volatile_pointer_read));
        address = mapping + qualified_pointer_levels_symbol->value;
        memcpy(&qualified_pointer_levels, &address,
               sizeof(qualified_pointer_levels));
        address = mapping + propagated_local_arithmetic_symbol->value;
        memcpy(&propagated_local_arithmetic, &address,
               sizeof(propagated_local_arithmetic));
        address = mapping + propagated_local_assignment_symbol->value;
        memcpy(&propagated_local_assignment, &address,
               sizeof(propagated_local_assignment));
        address = mapping + propagated_unsigned_narrow_symbol->value;
        memcpy(&propagated_unsigned_narrow, &address,
               sizeof(propagated_unsigned_narrow));
        address = mapping + propagated_local_branch_symbol->value;
        memcpy(&propagated_local_branch, &address,
               sizeof(propagated_local_branch));
        address = mapping + preserved_call_barrier_symbol->value;
        memcpy(&preserved_call_barrier, &address,
               sizeof(preserved_call_barrier));
        address = mapping + preserved_address_alias_symbol->value;
        memcpy(&preserved_address_alias, &address,
               sizeof(preserved_address_alias));
        address = mapping + preserved_conditional_state_symbol->value;
        memcpy(&preserved_conditional_state, &address,
               sizeof(preserved_conditional_state));
        address = mapping + preserved_do_state_symbol->value;
        memcpy(&preserved_do_state, &address,
               sizeof(preserved_do_state));
        address = mapping + preserved_while_state_symbol->value;
        memcpy(&preserved_while_state, &address,
               sizeof(preserved_while_state));
        address = mapping + propagated_compound_assignment_symbol->value;
        memcpy(&propagated_compound_assignment, &address,
               sizeof(propagated_compound_assignment));
        address = mapping + propagated_increment_symbol->value;
        memcpy(&propagated_increment, &address,
               sizeof(propagated_increment));
        address = mapping + eliminated_dead_stores_symbol->value;
        memcpy(&eliminated_dead_stores, &address,
               sizeof(eliminated_dead_stores));
        address = mapping + eliminated_overwritten_store_symbol->value;
        memcpy(&eliminated_overwritten_store, &address,
               sizeof(eliminated_overwritten_store));
        address = mapping + preserved_dead_store_effect_symbol->value;
        memcpy(&preserved_dead_store_effect, &address,
               sizeof(preserved_dead_store_effect));
        address = mapping + preserved_dead_store_escape_symbol->value;
        memcpy(&preserved_dead_store_escape, &address,
               sizeof(preserved_dead_store_escape));
        address = mapping + preserved_dead_volatile_store_symbol->value;
        memcpy(&preserved_dead_volatile_store, &address,
               sizeof(preserved_dead_volatile_store));
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
        assert(removed_pure_expression(8) == 9);
        value = 10;
        assert(preserved_assignment_expression(&value) == 12);
        assert(value == 12);
        assert(preserved_volatile_read(&value) == 31);
        assert(preserved_postfix_volatile_read(&value) == 37);
        assert(preserved_volatile_pointer_read(&value) == 12);
        {
            int left = 4;
            int right = 7;
            assert(qualified_pointer_levels(&left, &right) == 20);
            assert(left == 6 && right == 7);
        }
        assert(propagated_local_arithmetic() == 36);
        assert(propagated_local_assignment() == 42);
        assert(propagated_unsigned_narrow() == UINT32_C(5));
        assert(propagated_local_branch(&value) == 41);
        assert(value == 12);
        assert(preserved_call_barrier() == 23);
        assert(preserved_address_alias() == 29);
        assert(preserved_conditional_state(0) == 1);
        assert(preserved_conditional_state(1) == 2);
        assert(preserved_do_state() == 3);
        assert(preserved_while_state() == 3);
        assert(propagated_compound_assignment() == 8);
        assert(propagated_increment() == 21);
        assert(eliminated_dead_stores() == 5);
        assert(eliminated_overwritten_store(18) == 19);
        value = 0;
        assert(preserved_dead_store_effect(&value) == 1);
        assert(value == 1);
        assert(preserved_dead_store_escape() == 7);
        assert(preserved_dead_volatile_store() == 1);
        value = 3;
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
