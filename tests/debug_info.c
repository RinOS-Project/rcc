typedef unsigned int DebugInfoWord;

int debug_global_data = 7;
extern int debug_global_data;
static int debug_file_static;
const int debug_const_data = 4;
volatile int debug_volatile_data;
int * restrict debug_restrict_data;
_Atomic int debug_atomic_data;
_Thread_local int debug_tls_data = 11;

struct debug_aggregate {
    int first;
    int second;
};

struct debug_aggregate debug_aggregate_data = {1, 2};
int debug_array_data[2] = {8, 9};
struct debug_recursive {
    int value;
    struct debug_recursive* next;
};
struct debug_recursive* debug_recursive_root;
struct debug_bits {
    unsigned first : 3;
    unsigned second : 5;
};
struct debug_bits debug_bits_data;
enum debug_enum {
    DEBUG_ENUM_NEGATIVE = -2,
    DEBUG_ENUM_POSITIVE = 6
};
enum debug_enum debug_enum_data = DEBUG_ENUM_POSITIVE;
int* debug_inline_global_pointer_source;
int debug_inline_addressed_pointer_source;
int debug_inline_addressed_array_source[4];
int debug_inline_addressed_matrix_source[2][3];
struct debug_inline_addressed_member_container {
    int prefix;
    int target;
};
struct debug_inline_addressed_member_container
    debug_inline_addressed_member_source;
struct debug_inline_addressed_nested_container {
    int prefix;
    int values[3];
};
struct debug_inline_addressed_nested_container
    debug_inline_addressed_nested_source;
const int debug_inline_readonly_addressed_pointer_source = 31;
int** debug_inline_escaped_pointer;
int* debug_inline_escaped_object_address;
int* debug_inline_nonconstant_integer_escape;
unsigned int* debug_inline_nonconstant_unsigned_integer_escape;
unsigned int* debug_inline_nonconstant_unsigned_high_divisor_escape;
unsigned long long* debug_inline_nonconstant_unsigned_long_long_escape;
unsigned long long*
    debug_inline_nonconstant_unsigned_long_long_rhs_escape;
unsigned long long* debug_inline_nonconstant_unsigned_long_long_high_divisor_escape;
long long* debug_inline_nonconstant_signed_long_long_escape;
long long* debug_inline_nonconstant_signed_long_long_rhs_escape;

static void debug_inline_object_address_sink(int* value)
{
    debug_inline_escaped_object_address = value;
}

static int debug_inline_addressed_function_target(void)
{
    return 9;
}
static int debug_aggregate_sum(void)
{
    struct debug_aggregate local = {3, 4};
    if (local.first == 0) {
        return local.second;
    }
    return local.first + local.second;
}

static int debug_line_helper(void)
{
    return 3;
}

#include "debug_info_inline.h"

int debug_line_entry(void)
{
    static int debug_line_static;
    return debug_line_helper() + debug_declared_inline(1) +
           debug_global_data + debug_file_static + debug_line_static;
}

int debug_inline_entry(int value)
{
    return debug_declared_inline(value);
}

int debug_inline_nonconstant_integer_entry(int value)
{
    int debug_inline_nonconstant_integer_source = value;
    unsigned int debug_inline_nonconstant_unsigned_integer_source =
        (unsigned int)value;
    unsigned int debug_inline_nonconstant_unsigned_high_divisor_source =
        (debug_inline_nonconstant_unsigned_integer_source >> 2u) |
        0x80000000u;
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_source =
            (unsigned long long)(unsigned int)value;
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_high_divisor_source =
            (debug_inline_nonconstant_unsigned_long_long_source >> 2u) |
                0x8000000000000000ull;
    int added_value;
    int multiplied_value;
    int divided_value;
    int remainder_value;
    int negated_value;
    int bitnot_value;
    int bitand_value;
    int bitor_value;
    int bitxor_value;
    int lshift_value;
    int rshift_value;
    unsigned int unsigned_divided_value;
    unsigned int unsigned_remainder_value;
    unsigned int unsigned_high_divided_value;
    unsigned int unsigned_high_remainder_value;
    unsigned long long unsigned_long_long_divided_value;
    unsigned long long unsigned_long_long_remainder_value;
    unsigned long long unsigned_long_long_high_divided_value;
    unsigned long long unsigned_long_long_high_remainder_value;
    unsigned long long unsigned_long_long_identity_value;
    unsigned long long unsigned_long_long_added_value;
    unsigned long long unsigned_long_long_multiplied_value;
    unsigned long long unsigned_long_long_bitand_value;
    unsigned long long unsigned_long_long_bitor_value;
    unsigned long long unsigned_long_long_bitxor_value;
    unsigned long long unsigned_long_long_sub_value;
    unsigned long long unsigned_long_long_bitnot_value;
    unsigned long long unsigned_long_long_negate_value;
    debug_inline_nonconstant_integer_escape =
        &debug_inline_nonconstant_integer_source;
    debug_inline_nonconstant_unsigned_integer_escape =
        &debug_inline_nonconstant_unsigned_integer_source;
    debug_inline_nonconstant_unsigned_high_divisor_escape =
        &debug_inline_nonconstant_unsigned_high_divisor_source;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_source;
    debug_inline_nonconstant_unsigned_long_long_high_divisor_escape =
        &debug_inline_nonconstant_unsigned_long_long_high_divisor_source;
    added_value = debug_nonconstant_integer_parameter_inline(
        debug_inline_nonconstant_integer_source + 5);
    multiplied_value = debug_nonconstant_integer_multiply_parameter_inline(
        debug_inline_nonconstant_integer_source * 3);
    divided_value = debug_nonconstant_integer_divide_parameter_inline(
        debug_inline_nonconstant_integer_source / 3);
    remainder_value = debug_nonconstant_integer_remainder_parameter_inline(
        debug_inline_nonconstant_integer_source % 5);
    negated_value = debug_nonconstant_integer_negate_parameter_inline(
        -debug_inline_nonconstant_integer_source);
    bitnot_value = debug_nonconstant_integer_bitnot_parameter_inline(
        ~debug_inline_nonconstant_integer_source);
    bitand_value = debug_nonconstant_integer_bitand_parameter_inline(
        debug_inline_nonconstant_integer_source & 7);
    bitor_value = debug_nonconstant_integer_bitor_parameter_inline(
        debug_inline_nonconstant_integer_source | 8);
    bitxor_value = debug_nonconstant_integer_bitxor_parameter_inline(
        debug_inline_nonconstant_integer_source ^ 15);
    lshift_value = debug_nonconstant_integer_lshift_parameter_inline(
        debug_inline_nonconstant_integer_source << 2);
    rshift_value = debug_nonconstant_integer_rshift_parameter_inline(
        debug_inline_nonconstant_integer_source >> 1);
    rshift_value += (int)
        debug_nonconstant_unsigned_integer_rshift_parameter_inline(
            debug_inline_nonconstant_unsigned_integer_source >> 1u);
    unsigned_divided_value =
        debug_nonconstant_unsigned_integer_divide_parameter_inline(
            debug_inline_nonconstant_unsigned_integer_source / 3u);
    unsigned_remainder_value =
        debug_nonconstant_unsigned_integer_remainder_parameter_inline(
            debug_inline_nonconstant_unsigned_integer_source % 5u);
    unsigned_high_divided_value =
        debug_nonconstant_unsigned_integer_high_divide_inline(
            debug_inline_nonconstant_unsigned_integer_source /
                debug_inline_nonconstant_unsigned_high_divisor_source);
    unsigned_high_remainder_value =
        debug_nonconstant_unsigned_integer_high_remainder_inline(
            debug_inline_nonconstant_unsigned_integer_source %
                debug_inline_nonconstant_unsigned_high_divisor_source);
    unsigned_long_long_divided_value =
        debug_nonconstant_unsigned_long_long_divide_inline(
            debug_inline_nonconstant_unsigned_long_long_source / 3ull);
    unsigned_long_long_remainder_value =
        debug_nonconstant_unsigned_long_long_remainder_inline(
            debug_inline_nonconstant_unsigned_long_long_source % 5ull);
    unsigned_long_long_high_divided_value =
        debug_nonconstant_unsigned_long_long_high_divide_inline(
            debug_inline_nonconstant_unsigned_long_long_source /
                debug_inline_nonconstant_unsigned_long_long_high_divisor_source);
    unsigned_long_long_high_remainder_value =
        debug_nonconstant_unsigned_long_long_high_remainder_inline(
            debug_inline_nonconstant_unsigned_long_long_source %
                debug_inline_nonconstant_unsigned_long_long_high_divisor_source);
    unsigned_long_long_identity_value =
        debug_nonconstant_unsigned_long_long_identity_inline(
            debug_inline_nonconstant_unsigned_long_long_source);
    unsigned_long_long_added_value =
        debug_nonconstant_unsigned_long_long_add_inline(
            debug_inline_nonconstant_unsigned_long_long_source +
                0x100000005ull);
    unsigned_long_long_multiplied_value =
        debug_nonconstant_unsigned_long_long_multiply_inline(
            debug_inline_nonconstant_unsigned_long_long_source *
                0x0f0ff0f00ff00ff0ull);
    unsigned_long_long_bitand_value =
        debug_nonconstant_unsigned_long_long_bitand_inline(
            debug_inline_nonconstant_unsigned_long_long_source &
                0x0f0ff0f00ff00ff0ull);
    unsigned_long_long_bitor_value =
        debug_nonconstant_unsigned_long_long_bitor_inline(
            debug_inline_nonconstant_unsigned_long_long_source |
                0x4000000100000001ull);
    unsigned_long_long_bitxor_value =
        debug_nonconstant_unsigned_long_long_bitxor_inline(
            debug_inline_nonconstant_unsigned_long_long_source ^
                0x7fffffffffffffffull);
    unsigned_long_long_sub_value =
        debug_nonconstant_unsigned_long_long_sub_inline(
            debug_inline_nonconstant_unsigned_long_long_source -
                0x100000005ull);
    unsigned_long_long_bitnot_value =
        debug_nonconstant_unsigned_long_long_bitnot_inline(
            ~debug_inline_nonconstant_unsigned_long_long_source);
    unsigned_long_long_negate_value =
        debug_nonconstant_unsigned_long_long_negate_inline(
            -debug_inline_nonconstant_unsigned_long_long_source);
    return added_value + multiplied_value + divided_value + remainder_value +
        negated_value +
        bitnot_value +
        bitand_value +
        bitor_value + bitxor_value + lshift_value + rshift_value +
        (int)unsigned_divided_value + (int)unsigned_remainder_value +
        (int)unsigned_high_divided_value +
        (int)unsigned_high_remainder_value +
        (int)unsigned_long_long_divided_value +
        (int)unsigned_long_long_remainder_value +
        (int)unsigned_long_long_high_divided_value +
        (int)unsigned_long_long_high_remainder_value +
        (int)unsigned_long_long_identity_value +
        (int)unsigned_long_long_added_value +
        (int)unsigned_long_long_multiplied_value +
        (int)unsigned_long_long_bitand_value +
        (int)unsigned_long_long_bitor_value +
        (int)unsigned_long_long_bitxor_value +
        (int)unsigned_long_long_sub_value +
        (int)unsigned_long_long_bitnot_value +
        (int)unsigned_long_long_negate_value +
        debug_nonconstant_integer_subtract_parameter_inline(
            debug_inline_nonconstant_integer_source - 3);
}

int debug_inline_nonconstant_unsigned_long_long_add_two_variables_entry(
    unsigned int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_add_left_source =
            (unsigned long long)value;
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_rhs_source =
            ~debug_inline_nonconstant_unsigned_long_long_add_left_source;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_add_left_source;
    debug_inline_nonconstant_unsigned_long_long_rhs_escape =
        &debug_inline_nonconstant_unsigned_long_long_rhs_source;
    return (int)debug_nonconstant_unsigned_long_long_add_two_variables_inline(
        debug_inline_nonconstant_unsigned_long_long_add_left_source +
            debug_inline_nonconstant_unsigned_long_long_rhs_source);
}

int debug_inline_nonconstant_unsigned_long_long_sub_two_variables_entry(
    unsigned int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_sub_left_source =
            (unsigned long long)value;
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_sub_rhs_source =
            ~debug_inline_nonconstant_unsigned_long_long_sub_left_source;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_sub_left_source;
    debug_inline_nonconstant_unsigned_long_long_rhs_escape =
        &debug_inline_nonconstant_unsigned_long_long_sub_rhs_source;
    return (int)debug_nonconstant_unsigned_long_long_sub_two_variables_inline(
        debug_inline_nonconstant_unsigned_long_long_sub_left_source -
            debug_inline_nonconstant_unsigned_long_long_sub_rhs_source);
}

int debug_inline_nonconstant_unsigned_long_long_and_two_variables_entry(
    unsigned int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_and_left_source =
            (unsigned long long)value;
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_and_rhs_source =
            ~debug_inline_nonconstant_unsigned_long_long_and_left_source;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_and_left_source;
    debug_inline_nonconstant_unsigned_long_long_rhs_escape =
        &debug_inline_nonconstant_unsigned_long_long_and_rhs_source;
    return (int)debug_nonconstant_unsigned_long_long_and_two_variables_inline(
        debug_inline_nonconstant_unsigned_long_long_and_left_source &
            debug_inline_nonconstant_unsigned_long_long_and_rhs_source);
}

int debug_inline_nonconstant_unsigned_long_long_or_two_variables_entry(
    unsigned int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_or_left_source =
            (unsigned long long)value;
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_or_rhs_source =
            ~debug_inline_nonconstant_unsigned_long_long_or_left_source;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_or_left_source;
    debug_inline_nonconstant_unsigned_long_long_rhs_escape =
        &debug_inline_nonconstant_unsigned_long_long_or_rhs_source;
    return (int)debug_nonconstant_unsigned_long_long_or_two_variables_inline(
        debug_inline_nonconstant_unsigned_long_long_or_left_source |
            debug_inline_nonconstant_unsigned_long_long_or_rhs_source);
}

int debug_inline_nonconstant_unsigned_long_long_xor_two_variables_entry(
    unsigned int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_xor_left_source =
            (unsigned long long)value;
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_xor_rhs_source =
            ~debug_inline_nonconstant_unsigned_long_long_xor_left_source;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_xor_left_source;
    debug_inline_nonconstant_unsigned_long_long_rhs_escape =
        &debug_inline_nonconstant_unsigned_long_long_xor_rhs_source;
    return (int)debug_nonconstant_unsigned_long_long_xor_two_variables_inline(
        debug_inline_nonconstant_unsigned_long_long_xor_left_source ^
            debug_inline_nonconstant_unsigned_long_long_xor_rhs_source);
}

int debug_inline_nonconstant_unsigned_long_long_mul_two_variables_entry(
    unsigned int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_mul_left_source =
            ((unsigned long long)value << 32u) | 0xfffffff1ull;
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_mul_rhs_source =
            ((unsigned long long)value << 16u) | 0x12345ull;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_mul_left_source;
    debug_inline_nonconstant_unsigned_long_long_rhs_escape =
        &debug_inline_nonconstant_unsigned_long_long_mul_rhs_source;
    return (int)debug_nonconstant_unsigned_long_long_mul_two_variables_inline(
        debug_inline_nonconstant_unsigned_long_long_mul_left_source *
            debug_inline_nonconstant_unsigned_long_long_mul_rhs_source);
}

int debug_inline_nonconstant_signed_long_long_add_two_variables_entry(
    int value)
{
    long long debug_inline_nonconstant_signed_long_long_add_left_source =
        (long long)value;
    long long debug_inline_nonconstant_signed_long_long_add_rhs_source =
        -(long long)value;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_add_left_source;
    debug_inline_nonconstant_signed_long_long_rhs_escape =
        &debug_inline_nonconstant_signed_long_long_add_rhs_source;
    return (int)debug_nonconstant_signed_long_long_add_two_variables_inline(
        debug_inline_nonconstant_signed_long_long_add_left_source +
            debug_inline_nonconstant_signed_long_long_add_rhs_source);
}

int debug_inline_nonconstant_signed_long_long_sub_two_variables_entry(
    int value)
{
    long long debug_inline_nonconstant_signed_long_long_sub_left_source =
        (long long)value;
    long long debug_inline_nonconstant_signed_long_long_sub_rhs_source =
        (long long)value - 7ll;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_sub_left_source;
    debug_inline_nonconstant_signed_long_long_rhs_escape =
        &debug_inline_nonconstant_signed_long_long_sub_rhs_source;
    return (int)debug_nonconstant_signed_long_long_sub_two_variables_inline(
        debug_inline_nonconstant_signed_long_long_sub_left_source -
            debug_inline_nonconstant_signed_long_long_sub_rhs_source);
}

int debug_inline_nonconstant_signed_long_long_and_two_variables_entry(
    int value)
{
    long long debug_inline_nonconstant_signed_long_long_and_left_source =
        (long long)value;
    long long debug_inline_nonconstant_signed_long_long_and_rhs_source =
        ~debug_inline_nonconstant_signed_long_long_and_left_source;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_and_left_source;
    debug_inline_nonconstant_signed_long_long_rhs_escape =
        &debug_inline_nonconstant_signed_long_long_and_rhs_source;
    return (int)debug_nonconstant_signed_long_long_and_two_variables_inline(
        debug_inline_nonconstant_signed_long_long_and_left_source &
            debug_inline_nonconstant_signed_long_long_and_rhs_source);
}

int debug_inline_nonconstant_signed_long_long_or_two_variables_entry(
    int value)
{
    long long debug_inline_nonconstant_signed_long_long_or_left_source =
        (long long)value;
    long long debug_inline_nonconstant_signed_long_long_or_rhs_source =
        ~debug_inline_nonconstant_signed_long_long_or_left_source;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_or_left_source;
    debug_inline_nonconstant_signed_long_long_rhs_escape =
        &debug_inline_nonconstant_signed_long_long_or_rhs_source;
    return (int)debug_nonconstant_signed_long_long_or_two_variables_inline(
        debug_inline_nonconstant_signed_long_long_or_left_source |
            debug_inline_nonconstant_signed_long_long_or_rhs_source);
}

int debug_inline_nonconstant_signed_long_long_xor_two_variables_entry(
    int value)
{
    long long debug_inline_nonconstant_signed_long_long_xor_left_source =
        (long long)value;
    long long debug_inline_nonconstant_signed_long_long_xor_rhs_source =
        ~debug_inline_nonconstant_signed_long_long_xor_left_source;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_xor_left_source;
    debug_inline_nonconstant_signed_long_long_rhs_escape =
        &debug_inline_nonconstant_signed_long_long_xor_rhs_source;
    return (int)debug_nonconstant_signed_long_long_xor_two_variables_inline(
        debug_inline_nonconstant_signed_long_long_xor_left_source ^
            debug_inline_nonconstant_signed_long_long_xor_rhs_source);
}

int debug_inline_nonconstant_signed_long_long_mul_two_variables_entry(
    int value)
{
    long long debug_inline_nonconstant_signed_long_long_mul_left_source =
        (long long)value;
    long long debug_inline_nonconstant_signed_long_long_mul_rhs_source =
        (long long)value;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_mul_left_source;
    debug_inline_nonconstant_signed_long_long_rhs_escape =
        &debug_inline_nonconstant_signed_long_long_mul_rhs_source;
    return (int)debug_nonconstant_signed_long_long_mul_two_variables_inline(
        debug_inline_nonconstant_signed_long_long_mul_left_source *
            debug_inline_nonconstant_signed_long_long_mul_rhs_source);
}

int debug_inline_nonconstant_signed_long_long_add_constant_entry(int value)
{
    long long debug_inline_nonconstant_signed_long_long_add_constant_source =
        (long long)value;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_add_constant_source;
    return (int)debug_nonconstant_signed_long_long_add_constant_inline(
        debug_inline_nonconstant_signed_long_long_add_constant_source +
            (-7ll));
}

int debug_inline_nonconstant_signed_long_long_sub_constant_entry(int value)
{
    long long debug_inline_nonconstant_signed_long_long_sub_constant_source =
        (long long)value;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_sub_constant_source;
    return (int)debug_nonconstant_signed_long_long_sub_constant_inline(
        debug_inline_nonconstant_signed_long_long_sub_constant_source - 7ll);
}

int debug_inline_nonconstant_signed_long_long_and_constant_entry(int value)
{
    long long debug_inline_nonconstant_signed_long_long_and_constant_source =
        (long long)value;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_and_constant_source;
    return (int)debug_nonconstant_signed_long_long_and_constant_inline(
        debug_inline_nonconstant_signed_long_long_and_constant_source &
            (-0x0f0ff0f00ff00ff0ll));
}

int debug_inline_nonconstant_signed_long_long_or_constant_entry(int value)
{
    long long debug_inline_nonconstant_signed_long_long_or_constant_source =
        (long long)value;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_or_constant_source;
    return (int)debug_nonconstant_signed_long_long_or_constant_inline(
        debug_inline_nonconstant_signed_long_long_or_constant_source |
            0x4000000100000001ll);
}

int debug_inline_nonconstant_signed_long_long_xor_constant_entry(int value)
{
    long long debug_inline_nonconstant_signed_long_long_xor_constant_source =
        (long long)value;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_xor_constant_source;
    return (int)debug_nonconstant_signed_long_long_xor_constant_inline(
        debug_inline_nonconstant_signed_long_long_xor_constant_source ^
            (-0x100000001ll));
}

int debug_inline_nonconstant_signed_long_long_bitnot_entry(int value)
{
    long long debug_inline_nonconstant_signed_long_long_bitnot_source =
        (long long)value;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_bitnot_source;
    return (int)debug_nonconstant_signed_long_long_bitnot_inline(
        ~debug_inline_nonconstant_signed_long_long_bitnot_source);
}

int debug_inline_nonconstant_signed_long_long_negate_entry(int value)
{
    long long debug_inline_nonconstant_signed_long_long_negate_source =
        (long long)value;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_negate_source;
    return (int)debug_nonconstant_signed_long_long_negate_inline(
        -debug_inline_nonconstant_signed_long_long_negate_source);
}

int debug_inline_nonconstant_signed_long_long_mul_constant_entry(int value)
{
    long long debug_inline_nonconstant_signed_long_long_mul_constant_source =
        (long long)value;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_mul_constant_source;
    return (int)debug_nonconstant_signed_long_long_mul_constant_inline(
        debug_inline_nonconstant_signed_long_long_mul_constant_source *
            (-3ll));
}

int debug_inline_nonconstant_unsigned_long_long_lshift_entry(int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_lshift_source =
            (unsigned long long)(unsigned int)value;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_lshift_source;
    return (int)debug_nonconstant_unsigned_long_long_lshift_inline(
        debug_inline_nonconstant_unsigned_long_long_lshift_source << 1u);
}

int debug_inline_nonconstant_unsigned_long_long_rshift_entry(int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_rshift_source =
            (unsigned long long)(unsigned int)value;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_rshift_source;
    return (int)debug_nonconstant_unsigned_long_long_rshift_inline(
        debug_inline_nonconstant_unsigned_long_long_rshift_source >> 33u);
}

int debug_inline_nonconstant_signed_long_long_lshift_entry(int value)
{
    long long debug_inline_nonconstant_signed_long_long_lshift_source =
        (long long)value;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_lshift_source;
    return (int)debug_nonconstant_signed_long_long_lshift_inline(
        debug_inline_nonconstant_signed_long_long_lshift_source << 1u);
}

int debug_inline_nonconstant_signed_long_long_rshift_entry(int value)
{
    long long debug_inline_nonconstant_signed_long_long_rshift_source =
        (long long)value;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_rshift_source;
    return (int)debug_nonconstant_signed_long_long_rshift_inline(
        debug_inline_nonconstant_signed_long_long_rshift_source >> 33u);
}

int debug_inline_local_entry(int value)
{
    return debug_local_inline(value);
}

int debug_inline_constant_entry(void)
{
    return debug_constant_inline(23);
}

unsigned long long debug_inline_unsigned_constant_entry(void)
{
    return debug_unsigned_constant_inline(18446744073709551615ull);
}

int debug_inline_converted_signed_entry(void)
{
    return debug_converted_signed_inline((short)23);
}

int debug_inline_unsigned_to_signed_entry(void)
{
    return debug_converted_signed_inline(-1u);
}

unsigned int debug_inline_converted_unsigned_entry(void)
{
    return debug_converted_unsigned_inline(-1);
}

float debug_inline_float_constant_entry(void)
{
    return debug_float_parameter_inline(1.5f);
}

double debug_inline_double_constant_entry(void)
{
    return debug_double_parameter_inline(2.25);
}

double debug_inline_float_to_double_constant_entry(void)
{
    return debug_float_to_double_parameter_inline(1.1f);
}

float debug_inline_double_to_float_constant_entry(void)
{
    return debug_double_to_float_parameter_inline(1.1);
}

double debug_inline_float_expression_constant_entry(void)
{
    return debug_float_expression_parameter_inline(1.1f + 2.2f);
}

int debug_inline_null_pointer_entry(int* other)
{
    return debug_null_pointer_parameter_inline((int*)0, other);
}

int debug_inline_enum_constant_entry(void)
{
    return debug_enum_parameter_inline(DEBUG_ENUM_NEGATIVE);
}

int debug_inline_global_pointer_entry(void)
{
    return debug_global_pointer_parameter_inline(
        debug_inline_global_pointer_source);
}

int debug_inline_address_constant_entry(void)
{
    return debug_global_pointer_parameter_inline(
        &debug_inline_addressed_pointer_source);
}

int debug_inline_static_address_constant_entry(void)
{
    static int debug_inline_addressed_static_source;
    return debug_global_pointer_parameter_inline(
        &debug_inline_addressed_static_source);
}

int debug_inline_readonly_address_constant_entry(void)
{
    return debug_const_global_pointer_parameter_inline(
        &debug_inline_readonly_addressed_pointer_source);
}

int debug_inline_cast_address_constant_entry(void)
{
    return debug_void_pointer_parameter_inline(
        (const void*)&debug_inline_addressed_pointer_source);
}

int debug_inline_array_element_address_constant_entry(void)
{
    return debug_global_pointer_parameter_inline(
        &debug_inline_addressed_array_source[2]);
}

int debug_inline_array_computed_index_entry(void)
{
    return debug_global_pointer_parameter_inline(
        &debug_inline_addressed_array_source[1 + 1]);
}

int debug_inline_array_unsigned_wrap_index_entry(void)
{
    return debug_global_pointer_parameter_inline(
        &debug_inline_addressed_array_source[
            ((unsigned long long)0 - 1ULL) + 1ULL]);
}

int debug_inline_array_computed_index_out_of_bounds_entry(void)
{
    return debug_global_pointer_parameter_inline(
        &debug_inline_addressed_array_source[2 + 3]);
}

int debug_inline_array_plus_constant_entry(void)
{
    return debug_global_pointer_parameter_inline(
        debug_inline_addressed_array_source + (1 + 2));
}

int debug_inline_constant_plus_array_entry(void)
{
    return debug_global_pointer_parameter_inline(
        1 + debug_inline_addressed_array_source);
}

int debug_inline_array_one_past_entry(void)
{
    return debug_global_pointer_parameter_inline(
        debug_inline_addressed_array_source + 4);
}

int debug_inline_array_cast_constant_entry(void)
{
    return debug_global_pointer_parameter_inline(
        debug_inline_addressed_array_source + (unsigned char)260);
}

int debug_inline_array_unsigned_modulo_entry(void)
{
    return debug_global_pointer_parameter_inline(
        debug_inline_addressed_array_source + (-2 % 3u));
}

int debug_inline_array_interior_minus_entry(void)
{
    return debug_global_pointer_parameter_inline(
        &debug_inline_addressed_array_source[2] - 1);
}

int debug_inline_array_interior_plus_entry(void)
{
    return debug_global_pointer_parameter_inline(
        &debug_inline_addressed_array_source[1] + 2);
}

int debug_inline_array_before_begin_entry(void)
{
    return debug_global_pointer_parameter_inline(
        &debug_inline_addressed_array_source[0] - 1);
}

int debug_inline_nested_array_interior_minus_entry(void)
{
    return debug_global_pointer_parameter_inline(
        &debug_inline_addressed_nested_source.values[2] - 1);
}

int debug_inline_nested_array_before_begin_entry(void)
{
    return debug_global_pointer_parameter_inline(
        &debug_inline_addressed_nested_source.values[0] - 1);
}

int debug_inline_object_one_past_entry(void)
{
    return debug_global_pointer_parameter_inline(
        &debug_inline_addressed_pointer_source + 1);
}

int debug_inline_array_out_of_bounds_entry(void)
{
    return debug_global_pointer_parameter_inline(
        debug_inline_addressed_array_source + (unsigned char)261);
}

int debug_inline_member_address_constant_entry(void)
{
    return debug_global_pointer_parameter_inline(
        &debug_inline_addressed_member_source.target);
}

int debug_inline_nested_array_address_constant_entry(void)
{
    return debug_global_pointer_parameter_inline(
        &debug_inline_addressed_nested_source.values[1]);
}

int debug_inline_matrix_subarray_interior_plus_entry(void)
{
    return debug_global_pointer_parameter_inline(
        &debug_inline_addressed_matrix_source[1][1] + 1);
}

int debug_inline_matrix_subarray_one_past_entry(void)
{
    return debug_global_pointer_parameter_inline(
        &debug_inline_addressed_matrix_source[1][2] + 1);
}

int debug_inline_matrix_subarray_cross_row_entry(void)
{
    return debug_global_pointer_parameter_inline(
        &debug_inline_addressed_matrix_source[1][2] + 2);
}

int debug_inline_function_address_constant_entry(void)
{
    return debug_function_pointer_parameter_inline(
        &debug_inline_addressed_function_target);
}

int debug_inline_function_designator_constant_entry(void)
{
    return debug_function_pointer_parameter_inline(
        debug_inline_addressed_function_target);
}

int debug_inline_static_pointer_entry(void)
{
    static int* debug_inline_static_pointer_source;
    return debug_global_pointer_parameter_inline(
        debug_inline_static_pointer_source);
}

void debug_inline_pointer_sink(int** value);
int debug_inline_automatic_pointer_entry(int* incoming)
{
    int* debug_inline_automatic_pointer_source = incoming;
    int result = debug_global_pointer_parameter_inline(
        debug_inline_automatic_pointer_source);
    debug_inline_pointer_sink(&debug_inline_automatic_pointer_source);
    return result;
}

int debug_inline_automatic_address_entry(void)
{
    int debug_inline_automatic_address_source = 43;
    int result = debug_global_pointer_parameter_inline(
        &debug_inline_automatic_address_source);
    debug_inline_object_address_sink(
        &debug_inline_automatic_address_source);
    return result;
}

int debug_inline_automatic_const_address_entry(void)
{
    int debug_inline_automatic_const_address_source = 47;
    int result = debug_const_global_pointer_parameter_inline(
        &debug_inline_automatic_const_address_source);
    debug_inline_object_address_sink(
        &debug_inline_automatic_const_address_source);
    return result;
}

int debug_inline_automatic_void_address_entry(void)
{
    int debug_inline_automatic_void_address_source = 53;
    int result = debug_void_pointer_parameter_inline(
        (const void*)&debug_inline_automatic_void_address_source);
    debug_inline_object_address_sink(
        &debug_inline_automatic_void_address_source);
    return result;
}

int debug_tls_static_local_read(void)
{
    static _Thread_local int debug_tls_static_local = 17;
    return debug_tls_static_local;
}

int debug_prototype_function(void)
{
    return 17;
}

int debug_no_prototype_function()
{
    return 19;
}

int debug_info_parameters(int left, int right)
{
    int sum = left + right;
    int* pointer = &sum;
    {
        int nested = *pointer;
        return nested + debug_aggregate_sum() +
               debug_aggregate_data.first + debug_array_data[0];
    }
}

int debug_for_initializer_scope(int limit)
{
    int outer_value = 0;
    for (int loop_index = 0; loop_index < limit; ++loop_index) {
        outer_value += loop_index;
    }
    return outer_value;
}

int debug_vla_location(int count)
{
    int debug_vla_values[count];
    debug_vla_values[0] = count;
    return debug_vla_values[0];
}

int debug_vla_matrix(int rows, int columns)
{
    int debug_vla_matrix_values[rows][columns];
    debug_vla_matrix_values[0][0] = rows + columns;
    return debug_vla_matrix_values[0][0];
}

int debug_vla_parameter(int rows, int columns,
                        int debug_vla_parameter_values[rows][columns])
{
    return debug_vla_parameter_values[0][0];
}

int debug_vla_pointer_parameter(
    int count, int (*debug_vla_pointer_values)[count])
{
    return (*debug_vla_pointer_values)[0];
}

int debug_vla_local_pointer(int count, int values[count])
{
    int (*debug_vla_local_pointer_values)[count] =
        (int (*)[count])values;
    return (*debug_vla_local_pointer_values)[0];
}

#line 100
int debug_statement_lines(int value)
{
    value = value + 1;
    if (value > 3) {
        value = value + 2;
    } else {
        value = value + 4;
    }
    return value;
}

void debug_inline_pointer_sink(int** value)
{
    debug_inline_escaped_pointer = value;
}
