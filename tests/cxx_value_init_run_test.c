/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "objfile.h"

typedef int (*nullary_function)(void);
typedef void* (*nullary_pointer_function)(void);
typedef int (*nullable_pointer_function)(void*);
typedef void* (*int_nullable_pointer_function)(int, void*);
typedef int (*binary_function)(int, int);
typedef int (*wide_binary_function)(uint64_t, uint64_t);
typedef uint64_t (*int_wide_function)(int, uint64_t);
typedef uint64_t (*wide_unary_function)(uint64_t);
typedef int (*int_pointer_function)(const int*);
typedef int (*mutable_int_pointer_function)(int*);
typedef int (*mutable_int_pointer_binary_function)(int*, int);
typedef int (*mutable_int_pointer_pair_function)(int*, int*);

typedef struct RinSliceV1 {
    uint64_t address;
    uint64_t size;
} RinSliceV1;

typedef struct CxxVersioned {
    uint32_t struct_size;
    uint32_t version;
    uint64_t payload;
} CxxVersioned;

typedef RinSliceV1 (*reference_copy_function)(const RinSliceV1*);

static ObjSection* code_section(ObjectFile* object)
{
    ObjSection* section = object->sections;
    while (section && section->type != SECT_CODE) section = section->next;
    assert(section != NULL);
    return section;
}

#define LOAD_FUNCTION(target, object, mapping, symbol_name)                 \
    do {                                                                    \
        ObjSymbol* symbol = objfile_find_symbol((object), (symbol_name));    \
        void* address;                                                      \
        assert(symbol != NULL && symbol->section >= 0);                     \
        assert(symbol->binding == BIND_CODE);                               \
        address = (mapping) + symbol->value;                                \
        memcpy(&(target), &address, sizeof(target));                        \
    } while (0)

int main(int argc, char** argv)
{
    ObjectFile* object;
    ObjSection* code;
    uint8_t* mapping;
    long page_size;
    size_t mapping_size;
    nullary_function direct_value_init;
    nullary_pointer_function nullptr_return;
    nullary_function nullptr_context;
    nullable_pointer_function nullptr_comparisons;
    nullable_pointer_function nullptr_assignment;
    nullable_pointer_function nullptr_auto;
    nullary_function nullptr_size;
    int_nullable_pointer_function nullptr_conditional;
    nullary_function local_value_init;
    nullary_function scalar_value_init;
    nullary_function versioned_template_value;
    binary_function auto_function_call;
    binary_function class_aggregate_init;
    typedef int (*unary_function)(int);
    unary_function lowered_constructor_init;
    unary_function inline_accessor;
    unary_function delegated_status_bool;
    unary_function temporary_accessor;
    binary_function template_outcome_accessor;
    binary_function delegated_outcome_bool;
    int_wide_function template_outcome_wide_value;
    int_pointer_function pointer_accessor;
    reference_copy_function copy_reference;
    wide_binary_function reference_call;
    wide_binary_function reference_overload;
    mutable_int_pointer_function cleanup_block;
    mutable_int_pointer_function cleanup_return;
    mutable_int_pointer_function cleanup_wide;
    mutable_int_pointer_function cleanup_release;
    mutable_int_pointer_function cleanup_wide_release;
    mutable_int_pointer_function cleanup_get;
    mutable_int_pointer_function cleanup_close_call;
    mutable_int_pointer_function cleanup_close_failure;
    nullary_function cleanup_close_invalid;
    mutable_int_pointer_function cleanup_reset_call;
    mutable_int_pointer_function cleanup_reset_failure;
    mutable_int_pointer_function cleanup_wide_reset_call;
    mutable_int_pointer_function cleanup_wide_close_call;
    wide_unary_function cleanup_wide_get;
    mutable_int_pointer_function cleanup_break;
    mutable_int_pointer_function cleanup_continue;
    mutable_int_pointer_function cleanup_for_break;
    mutable_int_pointer_function cleanup_for_continue;
    mutable_int_pointer_function cleanup_zero_for;
    mutable_int_pointer_binary_function cleanup_switch;
    mutable_int_pointer_function cleanup_goto_exit;
    mutable_int_pointer_function cleanup_goto_backward;
    mutable_int_pointer_function cleanup_goto_same_scope;
    mutable_int_pointer_function cleanup_goto_for_init;
    mutable_int_pointer_function cleanup_contextual_bool;
    mutable_int_pointer_function cleanup_wide_contextual_bool;
    mutable_int_pointer_function cleanup_move;
    mutable_int_pointer_function cleanup_wide_move;
    mutable_int_pointer_pair_function cleanup_move_assignment;
    mutable_int_pointer_function cleanup_move_self_assignment;
    mutable_int_pointer_pair_function cleanup_wide_move_assignment;
    mutable_int_pointer_function cleanup_contextual_control;

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

    LOAD_FUNCTION(direct_value_init, object, mapping,
                  "cxx_direct_value_init");
    LOAD_FUNCTION(nullptr_return, object, mapping, "cxx_nullptr_return");
    LOAD_FUNCTION(nullptr_context, object, mapping, "cxx_nullptr_context");
    LOAD_FUNCTION(nullptr_comparisons, object, mapping,
                  "cxx_nullptr_comparisons");
    LOAD_FUNCTION(nullptr_assignment, object, mapping,
                  "cxx_nullptr_assignment");
    LOAD_FUNCTION(nullptr_auto, object, mapping, "cxx_nullptr_auto");
    LOAD_FUNCTION(nullptr_size, object, mapping, "cxx_nullptr_size");
    LOAD_FUNCTION(nullptr_conditional, object, mapping,
                  "cxx_nullptr_conditional");
    LOAD_FUNCTION(local_value_init, object, mapping,
                  "cxx_local_value_init");
    LOAD_FUNCTION(scalar_value_init, object, mapping,
                  "cxx_scalar_value_init");
    LOAD_FUNCTION(versioned_template_value, object, mapping,
                  "cxx_versioned_template_value");
    LOAD_FUNCTION(auto_function_call, object, mapping,
                  "cxx_auto_function_call");
    LOAD_FUNCTION(class_aggregate_init, object, mapping,
                  "cxx_class_aggregate_init");
    LOAD_FUNCTION(lowered_constructor_init, object, mapping,
                  "cxx_lowered_constructor_init");
    LOAD_FUNCTION(inline_accessor, object, mapping, "cxx_inline_accessor");
    LOAD_FUNCTION(delegated_status_bool, object, mapping,
                  "cxx_delegated_status_bool");
    LOAD_FUNCTION(temporary_accessor, object, mapping,
                  "cxx_temporary_accessor");
    LOAD_FUNCTION(pointer_accessor, object, mapping, "cxx_pointer_accessor");
    LOAD_FUNCTION(template_outcome_accessor, object, mapping,
                  "cxx_template_outcome_accessor");
    LOAD_FUNCTION(delegated_outcome_bool, object, mapping,
                  "cxx_delegated_outcome_bool");
    LOAD_FUNCTION(template_outcome_wide_value, object, mapping,
                  "cxx_template_outcome_wide_value");
    LOAD_FUNCTION(copy_reference, object, mapping,
                  "_ZN3rin14copy_referenceERK10RinSliceV1");
    LOAD_FUNCTION(reference_call, object, mapping, "cxx_reference_call");
    LOAD_FUNCTION(reference_overload, object, mapping,
                  "cxx_reference_overload");
    LOAD_FUNCTION(cleanup_block, object, mapping, "cxx_cleanup_block");
    LOAD_FUNCTION(cleanup_return, object, mapping, "cxx_cleanup_return");
    LOAD_FUNCTION(cleanup_wide, object, mapping, "cxx_cleanup_wide");
    LOAD_FUNCTION(cleanup_release, object, mapping, "cxx_cleanup_release");
    LOAD_FUNCTION(cleanup_wide_release, object, mapping,
                  "cxx_cleanup_wide_release");
    LOAD_FUNCTION(cleanup_get, object, mapping, "cxx_cleanup_get");
    LOAD_FUNCTION(cleanup_close_call, object, mapping,
                  "cxx_cleanup_close_call");
    LOAD_FUNCTION(cleanup_close_failure, object, mapping,
                  "cxx_cleanup_close_failure");
    LOAD_FUNCTION(cleanup_close_invalid, object, mapping,
                  "cxx_cleanup_close_invalid");
    LOAD_FUNCTION(cleanup_reset_call, object, mapping,
                  "cxx_cleanup_reset_call");
    LOAD_FUNCTION(cleanup_reset_failure, object, mapping,
                  "cxx_cleanup_reset_failure");
    LOAD_FUNCTION(cleanup_wide_reset_call, object, mapping,
                  "cxx_cleanup_wide_reset_call");
    LOAD_FUNCTION(cleanup_wide_close_call, object, mapping,
                  "cxx_cleanup_wide_close_call");
    LOAD_FUNCTION(cleanup_wide_get, object, mapping,
                  "cxx_cleanup_wide_get");
    LOAD_FUNCTION(cleanup_break, object, mapping, "cxx_cleanup_break");
    LOAD_FUNCTION(cleanup_continue, object, mapping, "cxx_cleanup_continue");
    LOAD_FUNCTION(cleanup_for_break, object, mapping,
                  "cxx_cleanup_for_break");
    LOAD_FUNCTION(cleanup_for_continue, object, mapping,
                  "cxx_cleanup_for_continue");
    LOAD_FUNCTION(cleanup_zero_for, object, mapping,
                  "cxx_cleanup_zero_for");
    LOAD_FUNCTION(cleanup_switch, object, mapping, "cxx_cleanup_switch");
    LOAD_FUNCTION(cleanup_goto_exit, object, mapping,
                  "cxx_cleanup_goto_exit");
    LOAD_FUNCTION(cleanup_goto_backward, object, mapping,
                  "cxx_cleanup_goto_backward");
    LOAD_FUNCTION(cleanup_goto_same_scope, object, mapping,
                  "cxx_cleanup_goto_same_scope");
    LOAD_FUNCTION(cleanup_goto_for_init, object, mapping,
                  "cxx_cleanup_goto_for_init");
    LOAD_FUNCTION(cleanup_contextual_bool, object, mapping,
                  "cxx_cleanup_contextual_bool");
    LOAD_FUNCTION(cleanup_wide_contextual_bool, object, mapping,
                  "cxx_cleanup_wide_contextual_bool");
    LOAD_FUNCTION(cleanup_move, object, mapping, "cxx_cleanup_move");
    LOAD_FUNCTION(cleanup_wide_move, object, mapping,
                  "cxx_cleanup_wide_move");
    LOAD_FUNCTION(cleanup_move_assignment, object, mapping,
                  "cxx_cleanup_move_assignment");
    LOAD_FUNCTION(cleanup_move_self_assignment, object, mapping,
                  "cxx_cleanup_move_self_assignment");
    LOAD_FUNCTION(cleanup_wide_move_assignment, object, mapping,
                  "cxx_cleanup_wide_move_assignment");
    LOAD_FUNCTION(cleanup_contextual_control, object, mapping,
                  "cxx_cleanup_contextual_control");
    assert(direct_value_init() == 1);
    assert(nullptr_return() == NULL);
    assert(nullptr_context() == 1);
    assert(nullptr_comparisons(NULL) == 1001);
    assert(nullptr_comparisons(mapping) == 1011);
    assert(nullptr_assignment(mapping) == 1);
    assert(nullptr_auto(mapping) == 1111);
    assert(nullptr_size() == (int)sizeof(void*));
    assert(nullptr_conditional(0, mapping) == NULL);
    assert(nullptr_conditional(1, mapping) == mapping);
    assert(local_value_init() == 1);
    assert(scalar_value_init() == 1);
    assert(versioned_template_value() ==
           (int)(sizeof(CxxVersioned) * 100u + 7u));
    assert(auto_function_call(31, 47) == 3147);
    assert(class_aggregate_init(4, 7) == 4774);
    assert(lowered_constructor_init(9) == 90);
    assert(inline_accessor(7) == 70);
    assert(inline_accessor(0) == 1);
    assert(delegated_status_bool(7) == 10);
    assert(delegated_status_bool(0) == 101);
    assert(temporary_accessor(-11) == -11);
    {
        int code = -17;
        assert(pointer_accessor(&code) == code);
    }
    assert(template_outcome_accessor(7, 99) == 7990);
    assert(template_outcome_accessor(0, 41) == 411);
    assert(delegated_outcome_bool(-1, 7) == 101);
    assert(delegated_outcome_bool(0, 0) == 1000);
    assert(delegated_outcome_bool(0, 7) == 1010);
    assert(template_outcome_wide_value(
               -5, UINT64_C(0x8877665544332211)) ==
           UINT64_C(0x8877665544332211));
    {
        RinSliceV1 input = {UINT64_C(0x12345678), UINT64_C(0x87654321)};
        RinSliceV1 copied = copy_reference(&input);
        assert(copied.address == input.address);
        assert(copied.size == input.size);
    }
    assert(reference_call(UINT64_C(0x1122334455667788),
                          UINT64_C(0x8877665544332211)) == 1);
    assert(reference_overload(UINT64_C(0x1020304050607080),
                              UINT64_C(0x8070605040302010)) == 1);
    {
        int value = 10;
        assert(cleanup_block(&value) == 11);
        assert(value == 11);
    }
    {
        int value = 20;
        assert(cleanup_return(&value) == 20);
        assert(value == 21);
    }
    assert(cleanup_return(NULL) == 42);
    {
        int value = 30;
        assert(cleanup_wide(&value) == 30);
        assert(value == 32);
    }
    {
        int value = 40;
        assert(cleanup_release(&value) == 1);
        assert(value == 40);
        assert(cleanup_wide_release(&value) == 1);
        assert(value == 40);
    }
    {
        int value = 50;
        assert(cleanup_get(&value) == 1);
        assert(value == 51);
    }
    {
        int value = 10;
        int failed = -10;
        int wide = 20;
        assert(cleanup_close_call(&value) == 11);
        assert(value == 11);
        assert(cleanup_close_failure(&failed) == 111);
        assert(failed == -9);
        assert(cleanup_close_invalid() == 11);
        value = 30;
        failed = -10;
        wide = 40;
        assert(cleanup_reset_call(&value) == 11);
        assert(value == 31);
        assert(cleanup_reset_failure(&failed) == 111);
        assert(failed == -9);
        assert(cleanup_wide_reset_call(&wide) == 11);
        assert(wide == 42);
        assert(cleanup_wide_close_call(&wide) == 11);
        assert(wide == 44);
    }
    assert(cleanup_wide_get(UINT64_C(0xfedcba9876543210)) ==
           UINT64_C(0xfedcba9876543210));
    {
        int value = 60;
        assert(cleanup_break(&value) == 61);
        assert(value == 61);
        assert(cleanup_continue(&value) == 63);
        assert(value == 63);
        assert(cleanup_for_break(&value) == 64);
        assert(value == 64);
        assert(cleanup_for_continue(&value) == 67);
        assert(value == 67);
        {
            int zero_trip_value = 5;
            assert(cleanup_zero_for(&zero_trip_value) == 6);
            assert(zero_trip_value == 6);
        }
        assert(cleanup_switch(&value, 0) == 68);
        assert(value == 69);
        assert(cleanup_switch(&value, 1) == 71);
        assert(value == 72);
        assert(cleanup_switch(&value, 2) == 73);
        assert(value == 74);
        assert(cleanup_switch(&value, 9) == 74);
        assert(value == 75);
        assert(cleanup_goto_exit(&value) == 76);
        assert(value == 76);
        assert(cleanup_goto_backward(&value) == 78);
        assert(value == 78);
        assert(cleanup_goto_same_scope(&value) == 78);
        assert(value == 79);
        assert(cleanup_goto_for_init(&value) == 80);
        assert(value == 80);
        assert(cleanup_contextual_bool(&value) == 1101);
        assert(value == 80);
        assert(cleanup_wide_contextual_bool(&value) == 11);
        assert(value == 80);
        assert(cleanup_move(&value) == 11);
        assert(value == 81);
        assert(cleanup_wide_move(&value) == 11);
        assert(value == 83);
        {
            int old_value = 10;
            int new_value = 20;
            assert(cleanup_move_assignment(&old_value, &new_value) == 111);
            assert(old_value == 11);
            assert(new_value == 20);
            assert(cleanup_move_self_assignment(&old_value) == 1);
            assert(old_value == 11);
            assert(cleanup_wide_move_assignment(&old_value, &new_value) ==
                   101);
            assert(old_value == 13);
            assert(new_value == 20);
        }
        assert(cleanup_contextual_control(&value) == 11111);
        assert(value == 83);
    }

    assert(munmap(mapping, mapping_size) == 0);
    objfile_free(object);
    puts("C++20 value-initialization execution test passed");
    return 0;
}
