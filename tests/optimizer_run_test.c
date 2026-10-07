#include "objfile.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

struct OptimizerPair {
    int first;
    int second;
};

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

static bool function_contains_byte(ObjectFile* object, const char* name,
                                   uint8_t value)
{
    ObjSection* code = code_section(object);
    ObjSymbol* function = function_symbol(object, name);
    uint64_t extent = function_extent(object, name);
    for (uint64_t offset = 0u; offset < extent; ++offset) {
        if (code->data[function->value + offset] == value) return true;
    }
    return false;
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
    assert(function_extent(optimized, "folded_float_arithmetic") <
           function_extent(unoptimized, "folded_float_arithmetic"));
    assert(function_extent(optimized, "folded_float_unary") <
           function_extent(unoptimized, "folded_float_unary"));
    assert(function_extent(optimized, "folded_float_cast_from_int") <
           function_extent(unoptimized, "folded_float_cast_from_int"));
    assert(function_extent(optimized, "folded_float_narrow_cast") <
           function_extent(unoptimized, "folded_float_narrow_cast"));
    assert(function_extent(optimized, "folded_float_to_signed") <
           function_extent(unoptimized, "folded_float_to_signed"));
    assert(function_extent(optimized, "folded_float_to_unsigned") <
           function_extent(unoptimized, "folded_float_to_unsigned"));
    assert(function_extent(optimized,
                           "folded_float_to_unsigned_negative_fraction") <
           function_extent(unoptimized,
                           "folded_float_to_unsigned_negative_fraction"));
    assert(function_extent(optimized, "retained_float_to_int_out_of_range") ==
           function_extent(unoptimized, "retained_float_to_int_out_of_range"));
    assert(function_extent(optimized, "folded_float_compare") <
           function_extent(unoptimized, "folded_float_compare"));
    assert(function_extent(optimized, "folded_float_branch") <
           function_extent(unoptimized, "folded_float_branch"));
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
    assert(function_contains_byte(unoptimized, "inlined_constant_call",
                                  0xe8u));
    assert(!function_contains_byte(optimized, "inlined_constant_call",
                                   0xe8u));
    assert(function_contains_byte(unoptimized, "inlined_argument_call",
                                  0xe8u));
    assert(!function_contains_byte(optimized, "inlined_argument_call",
                                   0xe8u));
    assert(function_contains_byte(unoptimized,
                                  "inlined_local_temporary_call", 0xe8u));
    assert(!function_contains_byte(optimized,
                                   "inlined_local_temporary_call", 0xe8u));
    assert(function_contains_byte(
        unoptimized, "inlined_two_local_temporaries_call", 0xe8u));
    assert(!function_contains_byte(
        optimized, "inlined_two_local_temporaries_call", 0xe8u));
    assert(function_contains_byte(
        unoptimized, "inlined_local_mutations_call", 0xe8u));
    assert(!function_contains_byte(
        optimized, "inlined_local_mutations_call", 0xe8u));
    assert(function_contains_byte(
        unoptimized, "inlined_local_snapshot_before_mutation_call", 0xe8u));
    assert(!function_contains_byte(
        optimized, "inlined_local_snapshot_before_mutation_call", 0xe8u));
    assert(function_contains_byte(
        unoptimized, "preserved_local_mutation_side_effect_call", 0xe8u));
    assert(function_contains_byte(
        optimized, "preserved_local_mutation_side_effect_call", 0xe8u));
    assert(function_contains_byte(
        unoptimized, "preserved_local_side_effect_call", 0xe8u));
    assert(function_contains_byte(
        optimized, "preserved_local_side_effect_call", 0xe8u));
    assert(function_contains_byte(unoptimized, "inlined_forward_chain",
                                  0xe8u));
    assert(!function_contains_byte(optimized, "inlined_forward_chain",
                                   0xe8u));
    assert(function_contains_byte(unoptimized,
                                  "inlined_repeated_argument_call", 0xe8u));
    assert(!function_contains_byte(optimized,
                                   "inlined_repeated_argument_call", 0xe8u));
    assert(function_contains_byte(
        unoptimized, "inlined_repeated_complex_argument_call", 0xe8u));
    assert(!function_contains_byte(
        optimized, "inlined_repeated_complex_argument_call", 0xe8u));
    assert(function_contains_byte(
        unoptimized, "inlined_conditional_cast_call", 0xe8u));
    assert(!function_contains_byte(
        optimized, "inlined_conditional_cast_call", 0xe8u));
    assert(function_contains_byte(
        unoptimized, "preserved_large_inline_call", 0xe8u));
    assert(function_contains_byte(
        optimized, "preserved_large_inline_call", 0xe8u));
    assert(function_contains_byte(unoptimized, "inlined_two_argument_call",
                                  0xe8u));
    assert(!function_contains_byte(optimized, "inlined_two_argument_call",
                                   0xe8u));
    assert(function_contains_byte(unoptimized,
                                  "inlined_three_argument_call", 0xe8u));
    assert(!function_contains_byte(optimized,
                                   "inlined_three_argument_call", 0xe8u));
    assert(function_contains_byte(unoptimized, "inlined_pointer_call",
                                  0xe8u));
    assert(!function_contains_byte(optimized, "inlined_pointer_call",
                                   0xe8u));
    assert(function_contains_byte(unoptimized,
                                  "inlined_pointer_offset_call", 0xe8u));
    assert(!function_contains_byte(optimized,
                                   "inlined_pointer_offset_call", 0xe8u));
    assert(function_contains_byte(unoptimized,
                                  "inlined_pointer_read_call", 0xe8u));
    assert(!function_contains_byte(optimized,
                                   "inlined_pointer_read_call", 0xe8u));
    assert(function_contains_byte(
        unoptimized, "preserved_pointer_side_effect_call", 0xe8u));
    assert(function_contains_byte(
        optimized, "preserved_pointer_side_effect_call", 0xe8u));
    assert(function_contains_byte(unoptimized,
                                  "inlined_pointee_size_call", 0xe8u));
    assert(!function_contains_byte(optimized,
                                   "inlined_pointee_size_call", 0xe8u));
    assert(function_contains_byte(unoptimized,
                                  "inlined_character_call", 0xe8u));
    assert(!function_contains_byte(optimized,
                                   "inlined_character_call", 0xe8u));
    assert(function_contains_byte(unoptimized,
                                  "inlined_string_constant_call", 0xe8u));
    assert(!function_contains_byte(optimized,
                                   "inlined_string_constant_call", 0xe8u));
    assert(function_contains_byte(unoptimized,
                                  "inlined_comma_call", 0xe8u));
    assert(!function_contains_byte(optimized,
                                   "inlined_comma_call", 0xe8u));
    {
        ObjSymbol* unoptimized_noexcept = objfile_find_symbol(
            unoptimized, "inlined_noexcept_call");
        ObjSymbol* optimized_noexcept = objfile_find_symbol(
            optimized, "inlined_noexcept_call");
        assert((unoptimized_noexcept != NULL) ==
               (optimized_noexcept != NULL));
        if (unoptimized_noexcept) {
            assert(function_contains_byte(
                unoptimized, "inlined_noexcept_call", 0xe8u));
            assert(!function_contains_byte(
                optimized, "inlined_noexcept_call", 0xe8u));
        }
    }
    assert(function_contains_byte(unoptimized,
                                  "inlined_pointer_index_call", 0xe8u));
    assert(!function_contains_byte(optimized,
                                   "inlined_pointer_index_call", 0xe8u));
    assert(function_contains_byte(unoptimized,
                                  "inlined_pointer_member_call", 0xe8u));
    assert(!function_contains_byte(optimized,
                                   "inlined_pointer_member_call", 0xe8u));
    assert(function_contains_byte(
        unoptimized, "inlined_pointer_member_deref_call", 0xe8u));
    assert(!function_contains_byte(
        optimized, "inlined_pointer_member_deref_call", 0xe8u));
    assert(function_contains_byte(unoptimized,
                                  "inlined_constant_double_call", 0xe8u));
    assert(!function_contains_byte(optimized,
                                   "inlined_constant_double_call", 0xe8u));
    assert(function_contains_byte(unoptimized, "inlined_double_call", 0xe8u));
    assert(!function_contains_byte(optimized, "inlined_double_call", 0xe8u));
    assert(function_contains_byte(unoptimized, "inlined_float_call", 0xe8u));
    assert(!function_contains_byte(optimized, "inlined_float_call", 0xe8u));
    assert(function_extent(optimized, "algebraic_integer_identities") <
           function_extent(unoptimized, "algebraic_integer_identities"));
    assert(function_extent(optimized, "algebraic_integer_mod_one") <
           function_extent(unoptimized, "algebraic_integer_mod_one"));
    assert(function_extent(optimized, "algebraic_integer_div_neg_one") <
           function_extent(unoptimized, "algebraic_integer_div_neg_one"));
    assert(function_extent(optimized, "algebraic_integer_mul_neg_one") <
           function_extent(unoptimized, "algebraic_integer_mul_neg_one"));
    assert(function_extent(optimized, "algebraic_integer_mod_neg_one") <
           function_extent(unoptimized, "algebraic_integer_mod_neg_one"));
    assert(function_extent(optimized, "algebraic_integer_zero") <
           function_extent(unoptimized, "algebraic_integer_zero"));
    assert(function_extent(optimized, "strength_reduce_unsigned_right") <
           function_extent(unoptimized, "strength_reduce_unsigned_right"));
    assert(function_extent(optimized, "strength_reduce_unsigned_left") <
           function_extent(unoptimized, "strength_reduce_unsigned_left"));
    assert(function_contains_byte(unoptimized,
                                  "strength_reduce_unsigned_three", 0xafu));
    assert(!function_contains_byte(optimized,
                                   "strength_reduce_unsigned_three", 0xafu));
    assert(function_contains_byte(unoptimized,
                                  "strength_reduce_unsigned_five", 0xafu));
    assert(!function_contains_byte(optimized,
                                   "strength_reduce_unsigned_five", 0xafu));
    assert(function_contains_byte(unoptimized,
                                  "strength_reduce_unsigned_six", 0xafu));
    assert(!function_contains_byte(optimized,
                                   "strength_reduce_unsigned_six", 0xafu));
    assert(function_contains_byte(unoptimized,
                                  "strength_reduce_unsigned_seven", 0xafu));
    assert(!function_contains_byte(optimized,
                                   "strength_reduce_unsigned_seven", 0xafu));
    assert(function_contains_byte(unoptimized,
                                  "strength_reduce_unsigned_nine", 0xafu));
    assert(!function_contains_byte(optimized,
                                   "strength_reduce_unsigned_nine", 0xafu));
    assert(function_contains_byte(unoptimized,
                                  "strength_reduce_unsigned_ten", 0xafu));
    assert(!function_contains_byte(optimized,
                                   "strength_reduce_unsigned_ten", 0xafu));
    assert(function_contains_byte(unoptimized,
                                  "strength_reduce_unsigned_eleven", 0xafu));
    assert(!function_contains_byte(optimized,
                                   "strength_reduce_unsigned_eleven", 0xafu));
    assert(function_contains_byte(unoptimized,
                                  "strength_reduce_unsigned_twelve", 0xafu));
    assert(!function_contains_byte(optimized,
                                   "strength_reduce_unsigned_twelve", 0xafu));
    assert(function_contains_byte(unoptimized,
                                  "strength_reduce_unsigned_thirteen", 0xafu));
    assert(!function_contains_byte(optimized,
                                   "strength_reduce_unsigned_thirteen", 0xafu));
    assert(function_contains_byte(unoptimized,
                                  "strength_reduce_unsigned_fourteen", 0xafu));
    assert(!function_contains_byte(optimized,
                                   "strength_reduce_unsigned_fourteen", 0xafu));
    assert(function_contains_byte(unoptimized,
                                  "strength_reduce_unsigned_fifteen", 0xafu));
    assert(!function_contains_byte(optimized,
                                   "strength_reduce_unsigned_fifteen", 0xafu));
    assert(function_contains_byte(unoptimized,
                                  "strength_reduce_unsigned_seventeen", 0xafu));
    assert(!function_contains_byte(optimized,
                                   "strength_reduce_unsigned_seventeen", 0xafu));
    assert(function_contains_byte(unoptimized,
                                  "strength_reduce_unsigned_thirty_one", 0xafu));
    assert(!function_contains_byte(optimized,
                                   "strength_reduce_unsigned_thirty_one", 0xafu));
    assert(function_contains_byte(unoptimized,
                                  "strength_reduce_unsigned_thirty_three", 0xafu));
    assert(!function_contains_byte(optimized,
                                   "strength_reduce_unsigned_thirty_three", 0xafu));
    assert(function_contains_byte(unoptimized,
                                  "strength_reduce_unsigned_forty_seven", 0xafu));
    assert(!function_contains_byte(optimized,
                                   "strength_reduce_unsigned_forty_seven", 0xafu));
    assert(function_contains_byte(unoptimized,
                                  "strength_reduce_unsigned_sixty_three", 0xafu));
    assert(!function_contains_byte(optimized,
                                   "strength_reduce_unsigned_sixty_three", 0xafu));
    assert(function_contains_byte(unoptimized,
                                  "strength_reduce_unsigned_sixty_five", 0xafu));
    assert(!function_contains_byte(optimized,
                                   "strength_reduce_unsigned_sixty_five", 0xafu));
    assert(function_contains_byte(unoptimized,
                                  "strength_reduce_unsigned_ninety_five", 0xafu));
    assert(!function_contains_byte(optimized,
                                   "strength_reduce_unsigned_ninety_five", 0xafu));
    assert(function_contains_byte(unoptimized,
                                  "strength_reduce_unsigned_one_hundred_twenty_seven",
                                  0xafu));
    assert(!function_contains_byte(optimized,
                                   "strength_reduce_unsigned_one_hundred_twenty_seven",
                                   0xafu));
    assert(function_extent(optimized, "strength_reduce_unsigned_div") <
           function_extent(unoptimized, "strength_reduce_unsigned_div"));
    assert(function_extent(optimized, "strength_reduce_unsigned_mod") <
           function_extent(unoptimized, "strength_reduce_unsigned_mod"));
    assert(function_contains_byte(
        unoptimized, "strength_reduce_signed_div_two", 0xf7u));
    assert(!function_contains_byte(
        optimized, "strength_reduce_signed_div_two", 0xf7u));
    assert(function_contains_byte(
        unoptimized, "strength_reduce_signed_div_eight", 0xf7u));
    assert(!function_contains_byte(
        optimized, "strength_reduce_signed_div_eight", 0xf7u));
    assert(function_contains_byte(
        unoptimized, "strength_reduce_signed_mod_two", 0xf7u));
    assert(!function_contains_byte(
        optimized, "strength_reduce_signed_mod_two", 0xf7u));
    assert(function_contains_byte(
        unoptimized, "strength_reduce_signed_mod_eight", 0xf7u));
    assert(!function_contains_byte(
        optimized, "strength_reduce_signed_mod_eight", 0xf7u));
    assert(function_contains_byte(
        unoptimized, "preserved_signed_div_negative_power", 0xf7u));
    assert(function_contains_byte(
        optimized, "preserved_signed_div_negative_power", 0xf7u));
    assert(function_extent(optimized, "preserved_algebraic_side_effect") <
           function_extent(unoptimized, "preserved_algebraic_side_effect"));
    assert(function_extent(optimized,
                          "preserved_algebraic_mod_one_side_effect") <
           function_extent(unoptimized,
                           "preserved_algebraic_mod_one_side_effect"));
    assert(function_extent(optimized,
                           "preserved_algebraic_mod_neg_one_side_effect") <
           function_extent(unoptimized,
                           "preserved_algebraic_mod_neg_one_side_effect"));
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
        ObjSymbol* float_arithmetic_symbol = function_symbol(
            object, "folded_float_arithmetic");
        ObjSymbol* float_unary_symbol = function_symbol(
            object, "folded_float_unary");
        ObjSymbol* float_cast_symbol = function_symbol(
            object, "folded_float_cast_from_int");
        ObjSymbol* float_narrow_cast_symbol = function_symbol(
            object, "folded_float_narrow_cast");
        ObjSymbol* float_to_signed_symbol = function_symbol(
            object, "folded_float_to_signed");
        ObjSymbol* float_to_unsigned_symbol = function_symbol(
            object, "folded_float_to_unsigned");
        ObjSymbol* float_to_unsigned_negative_symbol = function_symbol(
            object, "folded_float_to_unsigned_negative_fraction");
        ObjSymbol* float_compare_symbol = function_symbol(
            object, "folded_float_compare");
        ObjSymbol* float_branch_symbol = function_symbol(
            object, "folded_float_branch");
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
        ObjSymbol* algebraic_integer_identities_symbol = function_symbol(
            object, "algebraic_integer_identities");
        ObjSymbol* algebraic_integer_mod_one_symbol = function_symbol(
            object, "algebraic_integer_mod_one");
        ObjSymbol* algebraic_integer_div_neg_one_symbol = function_symbol(
            object, "algebraic_integer_div_neg_one");
        ObjSymbol* algebraic_integer_mul_neg_one_symbol = function_symbol(
            object, "algebraic_integer_mul_neg_one");
        ObjSymbol* algebraic_integer_mod_neg_one_symbol = function_symbol(
            object, "algebraic_integer_mod_neg_one");
        ObjSymbol* algebraic_integer_zero_symbol = function_symbol(
            object, "algebraic_integer_zero");
        ObjSymbol* strength_reduce_unsigned_right_symbol = function_symbol(
            object, "strength_reduce_unsigned_right");
        ObjSymbol* strength_reduce_unsigned_left_symbol = function_symbol(
            object, "strength_reduce_unsigned_left");
        ObjSymbol* strength_reduce_unsigned_div_symbol = function_symbol(
            object, "strength_reduce_unsigned_div");
        ObjSymbol* strength_reduce_unsigned_mod_symbol = function_symbol(
            object, "strength_reduce_unsigned_mod");
        ObjSymbol* strength_reduce_signed_div_two_symbol = function_symbol(
            object, "strength_reduce_signed_div_two");
        ObjSymbol* strength_reduce_signed_div_eight_symbol = function_symbol(
            object, "strength_reduce_signed_div_eight");
        ObjSymbol* strength_reduce_signed_mod_two_symbol = function_symbol(
            object, "strength_reduce_signed_mod_two");
        ObjSymbol* strength_reduce_signed_mod_eight_symbol = function_symbol(
            object, "strength_reduce_signed_mod_eight");
        ObjSymbol* inlined_argument_call_symbol = function_symbol(
            object, "inlined_argument_call");
        ObjSymbol* inlined_local_temporary_call_symbol = function_symbol(
            object, "inlined_local_temporary_call");
        ObjSymbol* inlined_two_local_temporaries_call_symbol = function_symbol(
            object, "inlined_two_local_temporaries_call");
        ObjSymbol* preserved_local_side_effect_call_symbol = function_symbol(
            object, "preserved_local_side_effect_call");
        ObjSymbol* inlined_repeated_argument_call_symbol = function_symbol(
            object, "inlined_repeated_argument_call");
        ObjSymbol* inlined_repeated_complex_argument_call_symbol =
            function_symbol(object, "inlined_repeated_complex_argument_call");
        ObjSymbol* inlined_forward_chain_symbol = function_symbol(
            object, "inlined_forward_chain");
        ObjSymbol* preserved_algebraic_side_effect_symbol = function_symbol(
            object, "preserved_algebraic_side_effect");
        ObjSymbol* preserved_algebraic_mod_one_side_effect_symbol =
            function_symbol(object, "preserved_algebraic_mod_one_side_effect");
        ObjSymbol* preserved_algebraic_mod_neg_one_side_effect_symbol =
            function_symbol(object,
                            "preserved_algebraic_mod_neg_one_side_effect");
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
        double (*folded_float_arithmetic)(void);
        float (*folded_float_unary)(void);
        double (*folded_float_cast_from_int)(void);
        float (*folded_float_narrow_cast)(void);
        int (*folded_float_to_signed)(void);
        uint32_t (*folded_float_to_unsigned)(void);
        uint32_t (*folded_float_to_unsigned_negative_fraction)(void);
        int (*folded_float_compare)(void);
        int (*folded_float_branch)(void);
        int (*folded_choice)(int);
        int (*folded_short_circuit)(int*);
        uint64_t (*folded_unsigned_wrap)(void);
        uint64_t (*folded_unsigned_divmod)(void);
        uint64_t (*folded_unsigned_shift)(void);
        uint32_t (*folded_unsigned_32)(void);
        uint32_t (*folded_unsigned_narrow)(void);
        uint64_t (*folded_unsigned_unary)(void);
        int (*folded_mixed_unsigned_comparison)(void);
        int (*algebraic_integer_identities)(int);
        int (*algebraic_integer_mod_one)(int);
        int (*algebraic_integer_div_neg_one)(int);
        int (*algebraic_integer_mul_neg_one)(int);
        int (*algebraic_integer_mod_neg_one)(int);
        int (*algebraic_integer_zero)(int);
        uint32_t (*strength_reduce_unsigned_right)(uint32_t);
        uint32_t (*strength_reduce_unsigned_left)(uint32_t);
        uint32_t (*strength_reduce_unsigned_three)(uint32_t);
        uint32_t (*strength_reduce_unsigned_five)(uint32_t);
        uint32_t (*strength_reduce_unsigned_six)(uint32_t);
        uint32_t (*strength_reduce_unsigned_seven)(uint32_t);
        uint32_t (*strength_reduce_unsigned_nine)(uint32_t);
        uint32_t (*strength_reduce_unsigned_ten)(uint32_t);
        uint32_t (*strength_reduce_unsigned_eleven)(uint32_t);
        uint32_t (*strength_reduce_unsigned_twelve)(uint32_t);
        uint32_t (*strength_reduce_unsigned_thirteen)(uint32_t);
        uint32_t (*strength_reduce_unsigned_fourteen)(uint32_t);
        uint32_t (*strength_reduce_unsigned_fifteen)(uint32_t);
        uint32_t (*strength_reduce_unsigned_seventeen)(uint32_t);
        uint32_t (*strength_reduce_unsigned_thirty_one)(uint32_t);
        uint32_t (*strength_reduce_unsigned_thirty_three)(uint32_t);
        uint32_t (*strength_reduce_unsigned_forty_seven)(uint32_t);
        uint32_t (*strength_reduce_unsigned_sixty_three)(uint32_t);
        uint32_t (*strength_reduce_unsigned_sixty_five)(uint32_t);
        uint32_t (*strength_reduce_unsigned_ninety_five)(uint32_t);
        uint32_t (*strength_reduce_unsigned_one_hundred_twenty_seven)(
            uint32_t);
        uint32_t (*strength_reduce_unsigned_div)(uint32_t);
        uint32_t (*strength_reduce_unsigned_mod)(uint32_t);
        int (*strength_reduce_signed_div_two)(int);
        int (*strength_reduce_signed_div_eight)(int);
        int (*strength_reduce_signed_mod_two)(int);
        int (*strength_reduce_signed_mod_eight)(int);
        int (*inlined_argument_call)(int);
        int (*inlined_local_temporary_call)(int);
        int (*inlined_two_local_temporaries_call)(int, int);
        int (*inlined_local_mutations_call)(int);
        int (*inlined_local_snapshot_before_mutation_call)(int);
        int (*preserved_local_mutation_side_effect_call)(int*);
        int (*preserved_local_side_effect_call)(volatile int*);
        int (*inlined_repeated_argument_call)(int);
        int (*inlined_repeated_complex_argument_call)(int);
        int (*inlined_conditional_cast_call)(int);
        int (*preserved_large_inline_call)(int);
        int (*inlined_forward_chain)(int);
        int (*inlined_two_argument_call)(int, int);
        uint32_t (*inlined_three_argument_call)(uint32_t, uint32_t,
                                                uint32_t);
        int (*inlined_pointer_call)(int*);
        int (*inlined_pointer_offset_call)(int*);
        int (*inlined_pointer_read_call)(int*);
        int (*preserved_pointer_side_effect_call)(int*);
        int (*inlined_pointee_size_call)(int*);
        int (*inlined_character_call)(int);
        const char* (*inlined_string_constant_call)(void);
        int (*inlined_comma_call)(int);
        int (*inlined_noexcept_call)(int*);
        int (*inlined_pointer_index_call)(int*);
        int (*inlined_pointer_member_call)(struct OptimizerPair*);
        int (*inlined_pointer_member_deref_call)(struct OptimizerPair*);
        double (*inlined_constant_double_call)(void);
        double (*inlined_double_call)(double);
        float (*inlined_float_call)(float, float);
        int (*preserved_algebraic_side_effect)(int*);
        int (*preserved_algebraic_mod_one_side_effect)(int*);
        int (*preserved_algebraic_mod_neg_one_side_effect)(int*);
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
        address = mapping + float_arithmetic_symbol->value;
        memcpy(&folded_float_arithmetic, &address,
               sizeof(folded_float_arithmetic));
        address = mapping + float_unary_symbol->value;
        memcpy(&folded_float_unary, &address, sizeof(folded_float_unary));
        address = mapping + float_cast_symbol->value;
        memcpy(&folded_float_cast_from_int, &address,
               sizeof(folded_float_cast_from_int));
        address = mapping + float_narrow_cast_symbol->value;
        memcpy(&folded_float_narrow_cast, &address,
               sizeof(folded_float_narrow_cast));
        address = mapping + float_to_signed_symbol->value;
        memcpy(&folded_float_to_signed, &address,
               sizeof(folded_float_to_signed));
        address = mapping + float_to_unsigned_symbol->value;
        memcpy(&folded_float_to_unsigned, &address,
               sizeof(folded_float_to_unsigned));
        address = mapping + float_to_unsigned_negative_symbol->value;
        memcpy(&folded_float_to_unsigned_negative_fraction, &address,
               sizeof(folded_float_to_unsigned_negative_fraction));
        address = mapping + float_compare_symbol->value;
        memcpy(&folded_float_compare, &address, sizeof(folded_float_compare));
        address = mapping + float_branch_symbol->value;
        memcpy(&folded_float_branch, &address, sizeof(folded_float_branch));
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
        address = mapping + function_symbol(
            object, "strength_reduce_unsigned_three")->value;
        memcpy(&strength_reduce_unsigned_three, &address,
               sizeof(strength_reduce_unsigned_three));
        address = mapping + function_symbol(
            object, "strength_reduce_unsigned_five")->value;
        memcpy(&strength_reduce_unsigned_five, &address,
               sizeof(strength_reduce_unsigned_five));
        address = mapping + function_symbol(
            object, "strength_reduce_unsigned_six")->value;
        memcpy(&strength_reduce_unsigned_six, &address,
               sizeof(strength_reduce_unsigned_six));
        address = mapping + function_symbol(
            object, "strength_reduce_unsigned_seven")->value;
        memcpy(&strength_reduce_unsigned_seven, &address,
               sizeof(strength_reduce_unsigned_seven));
        address = mapping + function_symbol(
            object, "strength_reduce_unsigned_nine")->value;
        memcpy(&strength_reduce_unsigned_nine, &address,
               sizeof(strength_reduce_unsigned_nine));
        address = mapping + function_symbol(
            object, "strength_reduce_unsigned_ten")->value;
        memcpy(&strength_reduce_unsigned_ten, &address,
               sizeof(strength_reduce_unsigned_ten));
        address = mapping + function_symbol(
            object, "strength_reduce_unsigned_eleven")->value;
        memcpy(&strength_reduce_unsigned_eleven, &address,
               sizeof(strength_reduce_unsigned_eleven));
        address = mapping + function_symbol(
            object, "strength_reduce_unsigned_twelve")->value;
        memcpy(&strength_reduce_unsigned_twelve, &address,
               sizeof(strength_reduce_unsigned_twelve));
        address = mapping + function_symbol(
            object, "strength_reduce_unsigned_thirteen")->value;
        memcpy(&strength_reduce_unsigned_thirteen, &address,
               sizeof(strength_reduce_unsigned_thirteen));
        address = mapping + function_symbol(
            object, "strength_reduce_unsigned_fourteen")->value;
        memcpy(&strength_reduce_unsigned_fourteen, &address,
               sizeof(strength_reduce_unsigned_fourteen));
        address = mapping + function_symbol(
            object, "strength_reduce_unsigned_fifteen")->value;
        memcpy(&strength_reduce_unsigned_fifteen, &address,
               sizeof(strength_reduce_unsigned_fifteen));
        address = mapping + function_symbol(
            object, "strength_reduce_unsigned_seventeen")->value;
        memcpy(&strength_reduce_unsigned_seventeen, &address,
               sizeof(strength_reduce_unsigned_seventeen));
        address = mapping + function_symbol(
            object, "strength_reduce_unsigned_thirty_one")->value;
        memcpy(&strength_reduce_unsigned_thirty_one, &address,
               sizeof(strength_reduce_unsigned_thirty_one));
        address = mapping + function_symbol(
            object, "strength_reduce_unsigned_thirty_three")->value;
        memcpy(&strength_reduce_unsigned_thirty_three, &address,
               sizeof(strength_reduce_unsigned_thirty_three));
        address = mapping + function_symbol(
            object, "strength_reduce_unsigned_forty_seven")->value;
        memcpy(&strength_reduce_unsigned_forty_seven, &address,
               sizeof(strength_reduce_unsigned_forty_seven));
        address = mapping + function_symbol(
            object, "strength_reduce_unsigned_sixty_three")->value;
        memcpy(&strength_reduce_unsigned_sixty_three, &address,
               sizeof(strength_reduce_unsigned_sixty_three));
        address = mapping + function_symbol(
            object, "strength_reduce_unsigned_sixty_five")->value;
        memcpy(&strength_reduce_unsigned_sixty_five, &address,
               sizeof(strength_reduce_unsigned_sixty_five));
        address = mapping + function_symbol(
            object, "strength_reduce_unsigned_ninety_five")->value;
        memcpy(&strength_reduce_unsigned_ninety_five, &address,
               sizeof(strength_reduce_unsigned_ninety_five));
        address = mapping + function_symbol(
            object, "strength_reduce_unsigned_one_hundred_twenty_seven")->value;
        memcpy(&strength_reduce_unsigned_one_hundred_twenty_seven, &address,
               sizeof(strength_reduce_unsigned_one_hundred_twenty_seven));
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
        address = mapping + algebraic_integer_identities_symbol->value;
        memcpy(&algebraic_integer_identities, &address,
               sizeof(algebraic_integer_identities));
        address = mapping + algebraic_integer_mod_one_symbol->value;
        memcpy(&algebraic_integer_mod_one, &address,
               sizeof(algebraic_integer_mod_one));
        address = mapping + algebraic_integer_div_neg_one_symbol->value;
        memcpy(&algebraic_integer_div_neg_one, &address,
               sizeof(algebraic_integer_div_neg_one));
        address = mapping + algebraic_integer_mul_neg_one_symbol->value;
        memcpy(&algebraic_integer_mul_neg_one, &address,
               sizeof(algebraic_integer_mul_neg_one));
        address = mapping + algebraic_integer_mod_neg_one_symbol->value;
        memcpy(&algebraic_integer_mod_neg_one, &address,
               sizeof(algebraic_integer_mod_neg_one));
        address = mapping + algebraic_integer_zero_symbol->value;
        memcpy(&algebraic_integer_zero, &address,
               sizeof(algebraic_integer_zero));
        address = mapping + strength_reduce_unsigned_right_symbol->value;
        memcpy(&strength_reduce_unsigned_right, &address,
               sizeof(strength_reduce_unsigned_right));
        address = mapping + strength_reduce_unsigned_left_symbol->value;
        memcpy(&strength_reduce_unsigned_left, &address,
               sizeof(strength_reduce_unsigned_left));
        address = mapping + strength_reduce_unsigned_div_symbol->value;
        memcpy(&strength_reduce_unsigned_div, &address,
               sizeof(strength_reduce_unsigned_div));
        address = mapping + strength_reduce_unsigned_mod_symbol->value;
        memcpy(&strength_reduce_unsigned_mod, &address,
               sizeof(strength_reduce_unsigned_mod));
        address = mapping + strength_reduce_signed_div_two_symbol->value;
        memcpy(&strength_reduce_signed_div_two, &address,
               sizeof(strength_reduce_signed_div_two));
        address = mapping + strength_reduce_signed_div_eight_symbol->value;
        memcpy(&strength_reduce_signed_div_eight, &address,
               sizeof(strength_reduce_signed_div_eight));
        address = mapping + strength_reduce_signed_mod_two_symbol->value;
        memcpy(&strength_reduce_signed_mod_two, &address,
               sizeof(strength_reduce_signed_mod_two));
        address = mapping + strength_reduce_signed_mod_eight_symbol->value;
        memcpy(&strength_reduce_signed_mod_eight, &address,
               sizeof(strength_reduce_signed_mod_eight));
        address = mapping + inlined_argument_call_symbol->value;
        memcpy(&inlined_argument_call, &address,
               sizeof(inlined_argument_call));
        address = mapping + inlined_local_temporary_call_symbol->value;
        memcpy(&inlined_local_temporary_call, &address,
               sizeof(inlined_local_temporary_call));
        address = mapping + inlined_two_local_temporaries_call_symbol->value;
        memcpy(&inlined_two_local_temporaries_call, &address,
               sizeof(inlined_two_local_temporaries_call));
        {
            ObjSymbol* symbol = function_symbol(
                object, "inlined_local_mutations_call");
            address = mapping + symbol->value;
            memcpy(&inlined_local_mutations_call, &address,
                   sizeof(inlined_local_mutations_call));
        }
        {
            ObjSymbol* symbol = function_symbol(
                object, "inlined_local_snapshot_before_mutation_call");
            address = mapping + symbol->value;
            memcpy(&inlined_local_snapshot_before_mutation_call, &address,
                   sizeof(inlined_local_snapshot_before_mutation_call));
        }
        {
            ObjSymbol* symbol = function_symbol(
                object, "preserved_local_mutation_side_effect_call");
            address = mapping + symbol->value;
            memcpy(&preserved_local_mutation_side_effect_call, &address,
                   sizeof(preserved_local_mutation_side_effect_call));
        }
        address = mapping + preserved_local_side_effect_call_symbol->value;
        memcpy(&preserved_local_side_effect_call, &address,
               sizeof(preserved_local_side_effect_call));
        address = mapping + inlined_repeated_argument_call_symbol->value;
        memcpy(&inlined_repeated_argument_call, &address,
               sizeof(inlined_repeated_argument_call));
        address = mapping + inlined_repeated_complex_argument_call_symbol->value;
        memcpy(&inlined_repeated_complex_argument_call, &address,
               sizeof(inlined_repeated_complex_argument_call));
        {
            ObjSymbol* inlined_conditional_cast_call_symbol = function_symbol(
                object, "inlined_conditional_cast_call");
            address = mapping + inlined_conditional_cast_call_symbol->value;
            memcpy(&inlined_conditional_cast_call, &address,
                   sizeof(inlined_conditional_cast_call));
        }
        {
            ObjSymbol* preserved_large_inline_call_symbol = function_symbol(
                object, "preserved_large_inline_call");
            address = mapping + preserved_large_inline_call_symbol->value;
            memcpy(&preserved_large_inline_call, &address,
                   sizeof(preserved_large_inline_call));
        }
        address = mapping + inlined_forward_chain_symbol->value;
        memcpy(&inlined_forward_chain, &address,
               sizeof(inlined_forward_chain));
        {
            ObjSymbol* inlined_two_argument_call_symbol = function_symbol(
                object, "inlined_two_argument_call");
            address = mapping + inlined_two_argument_call_symbol->value;
            memcpy(&inlined_two_argument_call, &address,
                   sizeof(inlined_two_argument_call));
        }
        {
            ObjSymbol* inlined_three_argument_call_symbol = function_symbol(
                object, "inlined_three_argument_call");
            address = mapping + inlined_three_argument_call_symbol->value;
            memcpy(&inlined_three_argument_call, &address,
                   sizeof(inlined_three_argument_call));
        }
        {
            ObjSymbol* symbol = function_symbol(object, "inlined_pointer_call");
            address = mapping + symbol->value;
            memcpy(&inlined_pointer_call, &address,
                   sizeof(inlined_pointer_call));
        }
        {
            ObjSymbol* symbol = function_symbol(
                object, "inlined_pointer_offset_call");
            address = mapping + symbol->value;
            memcpy(&inlined_pointer_offset_call, &address,
                   sizeof(inlined_pointer_offset_call));
        }
        {
            ObjSymbol* symbol = function_symbol(
                object, "inlined_pointer_read_call");
            address = mapping + symbol->value;
            memcpy(&inlined_pointer_read_call, &address,
                   sizeof(inlined_pointer_read_call));
        }
        {
            ObjSymbol* symbol = function_symbol(
                object, "preserved_pointer_side_effect_call");
            address = mapping + symbol->value;
            memcpy(&preserved_pointer_side_effect_call, &address,
                   sizeof(preserved_pointer_side_effect_call));
        }
        {
            ObjSymbol* symbol = function_symbol(
                object, "inlined_pointee_size_call");
            address = mapping + symbol->value;
            memcpy(&inlined_pointee_size_call, &address,
                   sizeof(inlined_pointee_size_call));
        }
        {
            ObjSymbol* symbol = function_symbol(
                object, "inlined_character_call");
            address = mapping + symbol->value;
            memcpy(&inlined_character_call, &address,
                   sizeof(inlined_character_call));
        }
        {
            ObjSymbol* symbol = function_symbol(
                object, "inlined_string_constant_call");
            address = mapping + symbol->value;
            memcpy(&inlined_string_constant_call, &address,
                   sizeof(inlined_string_constant_call));
        }
        {
            ObjSymbol* symbol = function_symbol(object, "inlined_comma_call");
            address = mapping + symbol->value;
            memcpy(&inlined_comma_call, &address,
                   sizeof(inlined_comma_call));
        }
        {
            ObjSymbol* symbol = objfile_find_symbol(
                object, "inlined_noexcept_call");
            if (symbol) {
                address = mapping + symbol->value;
                memcpy(&inlined_noexcept_call, &address,
                       sizeof(inlined_noexcept_call));
            }
        }
        {
            ObjSymbol* symbol = function_symbol(
                object, "inlined_pointer_index_call");
            address = mapping + symbol->value;
            memcpy(&inlined_pointer_index_call, &address,
                   sizeof(inlined_pointer_index_call));
        }
        {
            ObjSymbol* symbol = function_symbol(
                object, "inlined_pointer_member_call");
            address = mapping + symbol->value;
            memcpy(&inlined_pointer_member_call, &address,
                   sizeof(inlined_pointer_member_call));
        }
        {
            ObjSymbol* symbol = function_symbol(
                object, "inlined_pointer_member_deref_call");
            address = mapping + symbol->value;
            memcpy(&inlined_pointer_member_deref_call, &address,
                   sizeof(inlined_pointer_member_deref_call));
        }
        {
            ObjSymbol* symbol = function_symbol(
                object, "inlined_constant_double_call");
            address = mapping + symbol->value;
            memcpy(&inlined_constant_double_call, &address,
                   sizeof(inlined_constant_double_call));
        }
        {
            ObjSymbol* symbol = function_symbol(object, "inlined_double_call");
            address = mapping + symbol->value;
            memcpy(&inlined_double_call, &address,
                   sizeof(inlined_double_call));
        }
        {
            ObjSymbol* symbol = function_symbol(object, "inlined_float_call");
            address = mapping + symbol->value;
            memcpy(&inlined_float_call, &address,
                   sizeof(inlined_float_call));
        }
        address = mapping + preserved_algebraic_side_effect_symbol->value;
        memcpy(&preserved_algebraic_side_effect, &address,
               sizeof(preserved_algebraic_side_effect));
        address = mapping + preserved_algebraic_mod_one_side_effect_symbol->value;
        memcpy(&preserved_algebraic_mod_one_side_effect, &address,
               sizeof(preserved_algebraic_mod_one_side_effect));
        address = mapping +
                  preserved_algebraic_mod_neg_one_side_effect_symbol->value;
        memcpy(&preserved_algebraic_mod_neg_one_side_effect, &address,
               sizeof(preserved_algebraic_mod_neg_one_side_effect));
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
        assert(folded_float_arithmetic() > 5.999 &&
               folded_float_arithmetic() < 6.001);
        assert(folded_float_unary() > 2.499f &&
               folded_float_unary() < 2.501f);
        assert(folded_float_cast_from_int() > 6.999 &&
               folded_float_cast_from_int() < 7.001);
        assert(folded_float_narrow_cast() > 16777215.5f &&
               folded_float_narrow_cast() < 16777216.5f);
        assert(folded_float_to_signed() == -3);
        assert(folded_float_to_unsigned() == 3u);
        assert(folded_float_to_unsigned_negative_fraction() == 0u);
        assert(folded_float_compare() == 1);
        assert(folded_float_branch() == 17);
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
        assert(algebraic_integer_identities(-17) == -17);
        assert(algebraic_integer_mod_one(-17) == 0);
        assert(algebraic_integer_div_neg_one(-17) == 17);
        assert(algebraic_integer_mul_neg_one(-17) == 17);
        assert(algebraic_integer_mod_neg_one(-17) == 0);
        assert(algebraic_integer_zero(123) == 7);
        assert(strength_reduce_unsigned_right(123u) == 984u);
        assert(strength_reduce_unsigned_left(123u) == 1968u);
        assert(strength_reduce_unsigned_three(123u) == 369u);
        assert(strength_reduce_unsigned_five(123u) == 615u);
        assert(strength_reduce_unsigned_six(123u) == 738u);
        assert(strength_reduce_unsigned_seven(123u) == 861u);
        assert(strength_reduce_unsigned_nine(123u) == 1107u);
        assert(strength_reduce_unsigned_ten(123u) == 1230u);
        assert(strength_reduce_unsigned_eleven(123u) == 1353u);
        assert(strength_reduce_unsigned_twelve(123u) == 1476u);
        assert(strength_reduce_unsigned_thirteen(123u) == 1599u);
        assert(strength_reduce_unsigned_fourteen(123u) == 1722u);
        assert(strength_reduce_unsigned_fifteen(123u) == 1845u);
        assert(strength_reduce_unsigned_seventeen(123u) == 2091u);
        assert(strength_reduce_unsigned_thirty_one(123u) == 3813u);
        assert(strength_reduce_unsigned_thirty_three(123u) == 4059u);
        assert(strength_reduce_unsigned_forty_seven(123u) == 5781u);
        assert(strength_reduce_unsigned_sixty_three(123u) == 7749u);
        assert(strength_reduce_unsigned_sixty_five(123u) == 7995u);
        assert(strength_reduce_unsigned_ninety_five(123u) == 11685u);
        assert(strength_reduce_unsigned_one_hundred_twenty_seven(123u) ==
               15621u);
        assert(strength_reduce_unsigned_div(123u) == 15u);
        assert(strength_reduce_unsigned_mod(123u) == 3u);
        assert(strength_reduce_signed_div_two(-17) == -8);
        assert(strength_reduce_signed_div_two(17) == 8);
        assert(strength_reduce_signed_div_eight(-17) == -2);
        assert(strength_reduce_signed_div_eight(17) == 2);
        assert(strength_reduce_signed_div_eight(INT32_MIN) == -268435456);
        assert(strength_reduce_signed_mod_two(-17) == -1);
        assert(strength_reduce_signed_mod_two(17) == 1);
        assert(strength_reduce_signed_mod_eight(-17) == -1);
        assert(strength_reduce_signed_mod_eight(17) == 1);
        assert(strength_reduce_signed_mod_eight(INT32_MIN) == 0);
        assert(inlined_argument_call(-8) == -7);
        assert(inlined_local_temporary_call(-8) == -10);
        assert(inlined_two_local_temporaries_call(-8, 13) == 11);
        assert(inlined_local_mutations_call(-8) == -11);
        assert(inlined_local_mutations_call(5) == 15);
        assert(inlined_local_snapshot_before_mutation_call(3) == 20);
        assert(inlined_local_snapshot_before_mutation_call(-4) == -22);
        {
            int mutation_value = 4;
            assert(preserved_local_mutation_side_effect_call(
                       &mutation_value) == 8);
            assert(mutation_value == 5);
        }
        {
            volatile int local_side_effect_value = 10;
            assert(preserved_local_side_effect_call(
                       &local_side_effect_value) == 11);
            assert(local_side_effect_value == 10);
        }
        assert(inlined_repeated_argument_call(-8) == -16);
        assert(inlined_repeated_complex_argument_call(-8) == -46);
        assert(inlined_conditional_cast_call(-8) == 8);
        assert(inlined_conditional_cast_call(8) == 12);
        assert(preserved_large_inline_call(2) == 32);
        assert(inlined_forward_chain(-8) == 1);
        assert(inlined_two_argument_call(-8, 13) == 5);
        assert(inlined_three_argument_call(UINT32_C(0x55),
                                           UINT32_C(0x0f),
                                           UINT32_C(3)) == UINT32_C(93));
        {
            int values[] = {17, 29};
            assert(inlined_pointer_call(&values[0]) == 17);
            assert(inlined_pointer_offset_call(&values[0]) == 29);
            assert(inlined_pointer_read_call(&values[1]) == 29);
            assert(inlined_pointer_index_call(&values[0]) == 29);
        }
        {
            int side_effect_value = 10;
            assert(preserved_pointer_side_effect_call(&side_effect_value) ==
                   11);
            assert(side_effect_value == 11);
        }
        assert(inlined_pointee_size_call(&value) == 4);
        assert(inlined_character_call(3) == 68);
        assert(inlined_comma_call(3) == 4);
        if (objfile_find_symbol(object, "inlined_noexcept_call")) {
            int noexcept_value = 3;
            assert(inlined_noexcept_call(&noexcept_value) == 1);
            assert(noexcept_value == 3);
        }
        {
            struct OptimizerPair pair = {17, 29};
            assert(inlined_pointer_member_call(&pair) == 29);
            assert(inlined_pointer_member_deref_call(&pair) == 29);
        }
        assert(inlined_constant_double_call() == 2.5);
        assert(inlined_double_call(2.5) == 4.0);
        assert(inlined_float_call(1.25f, 2.5f) == 6.25f);
        value = 10;
        assert(preserved_algebraic_side_effect(&value) == 0);
        assert(value == 11);
        value = 10;
        assert(preserved_algebraic_mod_one_side_effect(&value) == 0);
        assert(value == 11);
        value = 10;
        assert(preserved_algebraic_mod_neg_one_side_effect(&value) == 0);
        assert(value == 11);
        value = 3;
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
