typedef unsigned int DebugInfoWord;

int debug_global_data = 7;
extern int debug_global_data;
unsigned long long debug_inline_nonconstant_unsigned_long_long_runtime_divisor =
    0x8000000000000001ull;
unsigned long long debug_inline_nonconstant_unsigned_long_long_runtime_dividend =
    0xfedcba9876543217ull;
_Atomic unsigned long long
    debug_inline_nonconstant_unsigned_long_long_atomic_dividend =
        0xfedcba9876543217ull;
_Atomic unsigned long long
    debug_inline_nonconstant_unsigned_long_long_atomic_divisor =
        0x8000000000000001ull;
long long debug_inline_nonconstant_signed_long_long_runtime_global_dividend =
    -0x7123456789abcdefll;
long long debug_inline_nonconstant_signed_long_long_runtime_global_divisor =
    -0x100000001ll;
_Atomic long long debug_inline_nonconstant_signed_long_long_atomic_dividend =
    -0x7123456789abcdefll;
_Atomic long long debug_inline_nonconstant_signed_long_long_atomic_divisor =
    -0x100000001ll;
_Atomic long long
    debug_inline_nonconstant_signed_long_long_atomic_nested_add_lhs =
        0x1ffffffffll;
_Atomic long long
    debug_inline_nonconstant_signed_long_long_atomic_nested_add_rhs = 1ll;
_Atomic long long
    debug_inline_nonconstant_signed_long_long_atomic_nested_sub_lhs =
        0x100000000ll;
_Atomic long long
    debug_inline_nonconstant_signed_long_long_atomic_nested_sub_rhs =
        0x100000001ll;
_Atomic long long debug_inline_nonconstant_signed_long_long_atomic_bitwise_lhs =
    -0x7edcba9876543211ll;
_Atomic long long debug_inline_nonconstant_signed_long_long_atomic_bitwise_rhs =
    -0x0123456789abcdf0ll;
volatile long long
    debug_inline_nonconstant_signed_long_long_volatile_bitwise_lhs =
        -0x7edcba9876543211ll;
volatile long long
    debug_inline_nonconstant_signed_long_long_volatile_bitwise_rhs =
        -0x0123456789abcdf0ll;
volatile long long*
    debug_inline_nonconstant_signed_long_long_volatile_local_lhs_escape;
volatile long long*
    debug_inline_nonconstant_signed_long_long_volatile_local_rhs_escape;
volatile long long*
    debug_inline_nonconstant_signed_long_long_volatile_local_division_dividend_escape;
volatile long long*
    debug_inline_nonconstant_signed_long_long_volatile_local_division_divisor_escape;
volatile unsigned long long*
    debug_inline_nonconstant_unsigned_long_long_volatile_local_constant_escape;
volatile long long*
    debug_inline_nonconstant_signed_long_long_volatile_local_constant_escape;
_Thread_local _Atomic long long
    debug_inline_nonconstant_signed_long_long_tls_atomic_bitwise_lhs =
        -0x7edcba9876543211ll;
_Thread_local _Atomic long long
    debug_inline_nonconstant_signed_long_long_tls_atomic_bitwise_rhs =
        -0x0123456789abcdf0ll;
_Thread_local volatile long long
    debug_inline_nonconstant_signed_long_long_volatile_tls_bitwise_lhs =
        -0x7edcba9876543211ll;
_Thread_local volatile long long
    debug_inline_nonconstant_signed_long_long_volatile_tls_bitwise_rhs =
        -0x0123456789abcdf0ll;
_Atomic unsigned long long
    debug_inline_nonconstant_unsigned_long_long_atomic_mixed_bitwise_lhs =
        0xfedcba9876543210ull;
_Atomic unsigned long long
    debug_inline_nonconstant_unsigned_long_long_atomic_mixed_bitwise_rhs =
        0xfedcba9876543210ull;
_Atomic long long* debug_inline_nonconstant_signed_long_long_atomic_local_dividend_escape;
_Atomic long long* debug_inline_nonconstant_signed_long_long_atomic_local_divisor_escape;
_Atomic unsigned long long*
    debug_inline_nonconstant_unsigned_long_long_atomic_local_bitwise_escape;
long long* debug_inline_nonconstant_signed_long_long_runtime_local_escape;
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
int** debug_inline_indirect_pointer_slot_escape;
int*** debug_inline_indirect_pointer_chain_slot_escape;
int**** debug_inline_indirect_pointer_triple_chain_slot_escape;
int* debug_inline_indirect_global_pointer_source;
_Thread_local int* debug_inline_indirect_tls_pointer_source;
int* volatile* debug_inline_indirect_volatile_pointer_slot_escape;
volatile int** debug_inline_indirect_volatile_pointee_slot_escape;
long long** debug_inline_indirect_wide_pointer_slot_escape;
int* debug_inline_nonconstant_integer_escape;
long* debug_inline_nonconstant_long_escape;
unsigned long* debug_inline_nonconstant_unsigned_long_escape;
long long* debug_inline_nonconstant_integer_logical_not_wide_escape;
unsigned long long*
    debug_inline_nonconstant_unsigned_integer_logical_not_wide_escape;
unsigned int* debug_inline_nonconstant_unsigned_integer_escape;
unsigned int* debug_inline_nonconstant_unsigned_high_divisor_escape;
unsigned int* debug_inline_nonconstant_shift_count_escape;
float* debug_inline_nonconstant_float_escape;
double* debug_inline_nonconstant_double_escape;
unsigned long long* debug_inline_nonconstant_unsigned_long_long_escape;
unsigned long long*
    debug_inline_nonconstant_unsigned_long_long_runtime_escape;
unsigned long long*
    debug_inline_nonconstant_unsigned_long_long_runtime_local_divisor_escape;
unsigned long long*
    debug_inline_nonconstant_unsigned_long_long_wide_escape;
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

static inline long long debug_nonconstant_signed_long_long_parameter_inline(
    long long value)
{
    return value;
}

static inline unsigned long long
debug_nonconstant_unsigned_long_long_parameter_inline(
    unsigned long long value)
{
    return value;
}

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

long long debug_inline_nonconstant_signed_long_long_global_local_divide_entry(
    int value)
{
    long long debug_inline_nonconstant_signed_long_long_local_divisor =
        (long long)value | 1ll;
    debug_inline_nonconstant_signed_long_long_runtime_local_escape =
        &debug_inline_nonconstant_signed_long_long_local_divisor;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_runtime_global_dividend /
        debug_inline_nonconstant_signed_long_long_local_divisor);
}

long long debug_inline_nonconstant_signed_long_long_global_local_remainder_entry(
    int value)
{
    long long debug_inline_nonconstant_signed_long_long_local_divisor =
        (long long)value | 1ll;
    debug_inline_nonconstant_signed_long_long_runtime_local_escape =
        &debug_inline_nonconstant_signed_long_long_local_divisor;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_runtime_global_dividend %
        debug_inline_nonconstant_signed_long_long_local_divisor);
}

long long debug_inline_nonconstant_signed_long_long_local_global_divide_entry(
    int value)
{
    long long debug_inline_nonconstant_signed_long_long_local_dividend =
        (long long)value - 5ll;
    debug_inline_nonconstant_signed_long_long_runtime_local_escape =
        &debug_inline_nonconstant_signed_long_long_local_dividend;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_local_dividend /
        debug_inline_nonconstant_signed_long_long_runtime_global_divisor);
}

long long debug_inline_nonconstant_signed_long_long_local_global_remainder_entry(
    int value)
{
    long long debug_inline_nonconstant_signed_long_long_local_dividend =
        (long long)value - 5ll;
    debug_inline_nonconstant_signed_long_long_runtime_local_escape =
        &debug_inline_nonconstant_signed_long_long_local_dividend;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_local_dividend %
        debug_inline_nonconstant_signed_long_long_runtime_global_divisor);
}

long long debug_inline_nonconstant_signed_long_long_local_pair_divide_entry(
    int value)
{
    long long debug_inline_nonconstant_signed_long_long_local_pair_dividend =
        (long long)value - 5ll;
    long long debug_inline_nonconstant_signed_long_long_local_pair_divisor =
        (long long)(value | 1);
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_local_pair_dividend;
    debug_inline_nonconstant_signed_long_long_rhs_escape =
        &debug_inline_nonconstant_signed_long_long_local_pair_divisor;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_local_pair_dividend /
        debug_inline_nonconstant_signed_long_long_local_pair_divisor);
}

long long debug_inline_nonconstant_signed_long_long_local_pair_remainder_entry(
    int value)
{
    long long debug_inline_nonconstant_signed_long_long_local_pair_dividend =
        (long long)value - 5ll;
    long long debug_inline_nonconstant_signed_long_long_local_pair_divisor =
        (long long)(value | 1);
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_local_pair_dividend;
    debug_inline_nonconstant_signed_long_long_rhs_escape =
        &debug_inline_nonconstant_signed_long_long_local_pair_divisor;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_local_pair_dividend %
        debug_inline_nonconstant_signed_long_long_local_pair_divisor);
}

long long debug_inline_nonconstant_signed_long_long_negated_global_dividend_divide_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        -debug_inline_nonconstant_signed_long_long_runtime_global_dividend /
        debug_inline_nonconstant_signed_long_long_runtime_global_divisor);
}

long long debug_inline_nonconstant_signed_long_long_negated_global_dividend_remainder_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        -debug_inline_nonconstant_signed_long_long_runtime_global_dividend %
        debug_inline_nonconstant_signed_long_long_runtime_global_divisor);
}

long long debug_inline_nonconstant_signed_long_long_negated_global_divisor_divide_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_runtime_global_dividend /
        -debug_inline_nonconstant_signed_long_long_runtime_global_divisor);
}

long long debug_inline_nonconstant_signed_long_long_negated_global_divisor_remainder_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_runtime_global_dividend %
        -debug_inline_nonconstant_signed_long_long_runtime_global_divisor);
}

long long debug_inline_nonconstant_signed_long_long_cast_global_dividend_divide_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        (long long)debug_inline_nonconstant_signed_long_long_runtime_global_dividend /
        debug_inline_nonconstant_signed_long_long_runtime_global_divisor);
}

long long debug_inline_nonconstant_signed_long_long_cast_global_divisor_remainder_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_runtime_global_dividend %
        (long long)debug_inline_nonconstant_signed_long_long_runtime_global_divisor);
}

long long debug_inline_nonconstant_signed_long_long_positive_cast_global_dividend_divide_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        +(long long)debug_inline_nonconstant_signed_long_long_runtime_global_dividend /
        debug_inline_nonconstant_signed_long_long_runtime_global_divisor);
}

long long debug_inline_nonconstant_signed_long_long_double_negated_cast_global_dividend_divide_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        -(-(long long)debug_inline_nonconstant_signed_long_long_runtime_global_dividend) /
        debug_inline_nonconstant_signed_long_long_runtime_global_divisor);
}

long long debug_inline_nonconstant_signed_long_long_triple_negated_cast_global_divisor_remainder_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_runtime_global_dividend %
        -(-(-(long long)debug_inline_nonconstant_signed_long_long_runtime_global_divisor)));
}

long long debug_inline_nonconstant_signed_long_long_cast_local_dividend_divide_entry(
    int value)
{
    long long debug_inline_nonconstant_signed_long_long_cast_local_dividend =
        (long long)value - 5ll;
    long long debug_inline_nonconstant_signed_long_long_cast_local_divisor =
        (long long)(value | 1);
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_cast_local_dividend;
    debug_inline_nonconstant_signed_long_long_rhs_escape =
        &debug_inline_nonconstant_signed_long_long_cast_local_divisor;
    return debug_nonconstant_signed_long_long_parameter_inline(
        (long long)debug_inline_nonconstant_signed_long_long_cast_local_dividend /
        debug_inline_nonconstant_signed_long_long_cast_local_divisor);
}

long long debug_inline_nonconstant_signed_long_long_cast_local_divisor_remainder_entry(
    int value)
{
    long long debug_inline_nonconstant_signed_long_long_cast_local_dividend =
        (long long)value - 5ll;
    long long debug_inline_nonconstant_signed_long_long_cast_local_divisor =
        (long long)(value | 1);
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_cast_local_dividend;
    debug_inline_nonconstant_signed_long_long_rhs_escape =
        &debug_inline_nonconstant_signed_long_long_cast_local_divisor;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_cast_local_dividend %
        (long long)debug_inline_nonconstant_signed_long_long_cast_local_divisor);
}

long long debug_inline_nonconstant_signed_long_long_negated_local_dividend_divide_entry(
    int value)
{
    long long debug_inline_nonconstant_signed_long_long_negated_local_dividend =
        (long long)value - 5ll;
    long long debug_inline_nonconstant_signed_long_long_negated_local_divisor =
        (long long)(value | 1);
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_negated_local_dividend;
    debug_inline_nonconstant_signed_long_long_rhs_escape =
        &debug_inline_nonconstant_signed_long_long_negated_local_divisor;
    return debug_nonconstant_signed_long_long_parameter_inline(
        -debug_inline_nonconstant_signed_long_long_negated_local_dividend /
        debug_inline_nonconstant_signed_long_long_negated_local_divisor);
}

long long debug_inline_nonconstant_signed_long_long_negated_local_divisor_remainder_entry(
    int value)
{
    long long debug_inline_nonconstant_signed_long_long_negated_local_dividend =
        (long long)value - 5ll;
    long long debug_inline_nonconstant_signed_long_long_negated_local_divisor =
        (long long)(value | 1);
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_negated_local_dividend;
    debug_inline_nonconstant_signed_long_long_rhs_escape =
        &debug_inline_nonconstant_signed_long_long_negated_local_divisor;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_negated_local_dividend %
        -debug_inline_nonconstant_signed_long_long_negated_local_divisor);
}

int debug_inline_nonconstant_unsigned_long_long_runtime_divide_entry(
    int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_runtime_source =
            (unsigned int)value;
    debug_inline_nonconstant_unsigned_long_long_runtime_escape =
        &debug_inline_nonconstant_unsigned_long_long_runtime_source;
    return (int)debug_nonconstant_unsigned_long_long_high_divide_inline(
        debug_inline_nonconstant_unsigned_long_long_runtime_source /
        debug_inline_nonconstant_unsigned_long_long_runtime_divisor);
}

int debug_inline_nonconstant_unsigned_long_long_runtime_remainder_entry(
    int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_runtime_source =
            (unsigned int)value;
    debug_inline_nonconstant_unsigned_long_long_runtime_escape =
        &debug_inline_nonconstant_unsigned_long_long_runtime_source;
    return (int)debug_nonconstant_unsigned_long_long_high_remainder_inline(
        debug_inline_nonconstant_unsigned_long_long_runtime_source %
        debug_inline_nonconstant_unsigned_long_long_runtime_divisor);
}

int debug_inline_nonconstant_unsigned_long_long_global_runtime_divide_entry(
    int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_runtime_divide_local_divisor =
            (unsigned int)value | 1u;
    debug_inline_nonconstant_unsigned_long_long_runtime_local_divisor_escape =
        &debug_inline_nonconstant_unsigned_long_long_runtime_divide_local_divisor;
    return (int)debug_nonconstant_unsigned_long_long_high_divide_inline(
        debug_inline_nonconstant_unsigned_long_long_runtime_dividend /
        debug_inline_nonconstant_unsigned_long_long_runtime_divide_local_divisor);
}

int debug_inline_nonconstant_unsigned_long_long_global_runtime_remainder_entry(
    int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_runtime_remainder_local_divisor =
            (unsigned int)value | 1u;
    debug_inline_nonconstant_unsigned_long_long_runtime_local_divisor_escape =
        &debug_inline_nonconstant_unsigned_long_long_runtime_remainder_local_divisor;
    return (int)debug_nonconstant_unsigned_long_long_high_remainder_inline(
        debug_inline_nonconstant_unsigned_long_long_runtime_dividend %
        debug_inline_nonconstant_unsigned_long_long_runtime_remainder_local_divisor);
}

int debug_inline_nonconstant_unsigned_long_long_global_pair_divide_entry(void)
{
    return (int)debug_nonconstant_unsigned_long_long_high_divide_inline(
        (+(unsigned long long)
            debug_inline_nonconstant_unsigned_long_long_runtime_dividend) /
        debug_inline_nonconstant_unsigned_long_long_runtime_divisor);
}

int debug_inline_nonconstant_unsigned_long_long_global_pair_remainder_entry(void)
{
    return (int)debug_nonconstant_unsigned_long_long_high_remainder_inline(
        debug_inline_nonconstant_unsigned_long_long_runtime_dividend %
        (+(unsigned long long)
            debug_inline_nonconstant_unsigned_long_long_runtime_divisor));
}

int debug_inline_nonconstant_unsigned_long_long_atomic_divide_entry(void)
{
    return (int)debug_nonconstant_unsigned_long_long_high_divide_inline(
        debug_inline_nonconstant_unsigned_long_long_atomic_dividend / 7ull);
}

int debug_inline_nonconstant_unsigned_long_long_atomic_remainder_entry(void)
{
    return (int)debug_nonconstant_unsigned_long_long_high_remainder_inline(
        debug_inline_nonconstant_unsigned_long_long_atomic_dividend % 7ull);
}

int debug_inline_nonconstant_unsigned_long_long_atomic_runtime_divide_entry(
    void)
{
    return (int)debug_nonconstant_unsigned_long_long_high_divide_inline(
        debug_inline_nonconstant_unsigned_long_long_atomic_dividend /
        debug_inline_nonconstant_unsigned_long_long_atomic_divisor);
}

int debug_inline_nonconstant_unsigned_long_long_atomic_runtime_remainder_entry(
    void)
{
    return (int)debug_nonconstant_unsigned_long_long_high_remainder_inline(
        debug_inline_nonconstant_unsigned_long_long_atomic_dividend %
        debug_inline_nonconstant_unsigned_long_long_atomic_divisor);
}

long long debug_inline_nonconstant_signed_long_long_atomic_runtime_divide_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_atomic_dividend /
        debug_inline_nonconstant_signed_long_long_atomic_divisor);
}

long long debug_inline_nonconstant_signed_long_long_atomic_runtime_remainder_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_atomic_dividend %
        debug_inline_nonconstant_signed_long_long_atomic_divisor);
}

long long debug_inline_nonconstant_signed_long_long_atomic_nested_add_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_atomic_nested_add_lhs +
        (debug_inline_nonconstant_signed_long_long_atomic_nested_add_rhs *
         2ll));
}

long long debug_inline_nonconstant_signed_long_long_atomic_nested_sub_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_atomic_nested_sub_lhs -
        (debug_inline_nonconstant_signed_long_long_atomic_nested_sub_rhs *
         2ll));
}

long long debug_inline_nonconstant_signed_long_long_atomic_bitand_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_atomic_bitwise_lhs &
        debug_inline_nonconstant_signed_long_long_atomic_bitwise_rhs);
}

long long debug_inline_nonconstant_signed_long_long_atomic_bitor_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_atomic_bitwise_lhs |
        debug_inline_nonconstant_signed_long_long_atomic_bitwise_rhs);
}

long long debug_inline_nonconstant_signed_long_long_atomic_bitxor_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_atomic_bitwise_lhs ^
        debug_inline_nonconstant_signed_long_long_atomic_bitwise_rhs);
}

long long debug_inline_nonconstant_signed_long_long_volatile_bitand_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_volatile_bitwise_lhs &
        debug_inline_nonconstant_signed_long_long_volatile_bitwise_rhs);
}

long long debug_inline_nonconstant_signed_long_long_volatile_bitor_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_volatile_bitwise_lhs |
        debug_inline_nonconstant_signed_long_long_volatile_bitwise_rhs);
}

long long debug_inline_nonconstant_signed_long_long_volatile_bitxor_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_volatile_bitwise_lhs ^
        debug_inline_nonconstant_signed_long_long_volatile_bitwise_rhs);
}

long long debug_inline_nonconstant_signed_long_long_tls_atomic_bitand_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_tls_atomic_bitwise_lhs &
        debug_inline_nonconstant_signed_long_long_tls_atomic_bitwise_rhs);
}

long long debug_inline_nonconstant_signed_long_long_tls_atomic_bitor_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_tls_atomic_bitwise_lhs |
        debug_inline_nonconstant_signed_long_long_tls_atomic_bitwise_rhs);
}

long long debug_inline_nonconstant_signed_long_long_tls_atomic_bitxor_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_tls_atomic_bitwise_lhs ^
        debug_inline_nonconstant_signed_long_long_tls_atomic_bitwise_rhs);
}

long long debug_inline_nonconstant_signed_long_long_volatile_tls_bitand_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_volatile_tls_bitwise_lhs &
        debug_inline_nonconstant_signed_long_long_volatile_tls_bitwise_rhs);
}

long long debug_inline_nonconstant_signed_long_long_volatile_tls_bitor_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_volatile_tls_bitwise_lhs |
        debug_inline_nonconstant_signed_long_long_volatile_tls_bitwise_rhs);
}

long long debug_inline_nonconstant_signed_long_long_volatile_tls_bitxor_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_volatile_tls_bitwise_lhs ^
        debug_inline_nonconstant_signed_long_long_volatile_tls_bitwise_rhs);
}

unsigned long long
debug_inline_nonconstant_signed_long_long_atomic_bitand_unsigned_rhs_entry(
    void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_atomic_bitwise_lhs &
        debug_inline_nonconstant_unsigned_long_long_atomic_mixed_bitwise_rhs);
}

unsigned long long
debug_inline_nonconstant_signed_long_long_atomic_bitor_unsigned_rhs_entry(
    void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_atomic_bitwise_lhs |
        debug_inline_nonconstant_unsigned_long_long_atomic_mixed_bitwise_rhs);
}

unsigned long long
debug_inline_nonconstant_signed_long_long_atomic_bitxor_unsigned_rhs_entry(
    void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_atomic_bitwise_lhs ^
        debug_inline_nonconstant_unsigned_long_long_atomic_mixed_bitwise_rhs);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_atomic_bitand_signed_rhs_entry(
    void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_atomic_mixed_bitwise_lhs &
        debug_inline_nonconstant_signed_long_long_atomic_bitwise_lhs);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_atomic_bitor_signed_rhs_entry(
    void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_atomic_mixed_bitwise_lhs |
        debug_inline_nonconstant_signed_long_long_atomic_bitwise_lhs);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_atomic_bitxor_signed_rhs_entry(
    void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_atomic_mixed_bitwise_lhs ^
        debug_inline_nonconstant_signed_long_long_atomic_bitwise_lhs);
}

long long debug_inline_nonconstant_signed_long_long_atomic_local_pair_divide_entry(
    long long dividend, long long divisor)
{
    _Atomic long long debug_inline_nonconstant_signed_long_long_atomic_local_dividend =
        dividend;
    _Atomic long long debug_inline_nonconstant_signed_long_long_atomic_local_divisor =
        divisor;
    debug_inline_nonconstant_signed_long_long_atomic_local_dividend_escape =
        &debug_inline_nonconstant_signed_long_long_atomic_local_dividend;
    debug_inline_nonconstant_signed_long_long_atomic_local_divisor_escape =
        &debug_inline_nonconstant_signed_long_long_atomic_local_divisor;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_atomic_local_dividend /
        debug_inline_nonconstant_signed_long_long_atomic_local_divisor);
}

long long debug_inline_nonconstant_signed_long_long_atomic_local_pair_remainder_entry(
    long long dividend, long long divisor)
{
    _Atomic long long debug_inline_nonconstant_signed_long_long_atomic_local_dividend =
        dividend;
    _Atomic long long debug_inline_nonconstant_signed_long_long_atomic_local_divisor =
        divisor;
    debug_inline_nonconstant_signed_long_long_atomic_local_dividend_escape =
        &debug_inline_nonconstant_signed_long_long_atomic_local_dividend;
    debug_inline_nonconstant_signed_long_long_atomic_local_divisor_escape =
        &debug_inline_nonconstant_signed_long_long_atomic_local_divisor;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_atomic_local_dividend %
        debug_inline_nonconstant_signed_long_long_atomic_local_divisor);
}

long long debug_inline_nonconstant_signed_long_long_volatile_local_pair_divide_entry(
    long long dividend, long long divisor)
{
    volatile long long
        debug_inline_nonconstant_signed_long_long_volatile_local_division_dividend =
            dividend;
    volatile long long
        debug_inline_nonconstant_signed_long_long_volatile_local_division_divisor =
            divisor;
    debug_inline_nonconstant_signed_long_long_volatile_local_division_dividend_escape =
        &debug_inline_nonconstant_signed_long_long_volatile_local_division_dividend;
    debug_inline_nonconstant_signed_long_long_volatile_local_division_divisor_escape =
        &debug_inline_nonconstant_signed_long_long_volatile_local_division_divisor;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_volatile_local_division_dividend /
        debug_inline_nonconstant_signed_long_long_volatile_local_division_divisor);
}

long long debug_inline_nonconstant_signed_long_long_volatile_local_pair_remainder_entry(
    long long dividend, long long divisor)
{
    volatile long long
        debug_inline_nonconstant_signed_long_long_volatile_local_remainder_dividend =
            dividend;
    volatile long long
        debug_inline_nonconstant_signed_long_long_volatile_local_remainder_divisor =
            divisor;
    debug_inline_nonconstant_signed_long_long_volatile_local_division_dividend_escape =
        &debug_inline_nonconstant_signed_long_long_volatile_local_remainder_dividend;
    debug_inline_nonconstant_signed_long_long_volatile_local_division_divisor_escape =
        &debug_inline_nonconstant_signed_long_long_volatile_local_remainder_divisor;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_volatile_local_remainder_dividend %
        debug_inline_nonconstant_signed_long_long_volatile_local_remainder_divisor);
}

long long debug_inline_nonconstant_signed_long_long_atomic_local_bitand_entry(
    long long left_value, long long right_value)
{
    _Atomic long long debug_inline_nonconstant_signed_long_long_atomic_local_bitand_lhs =
        left_value;
    _Atomic long long debug_inline_nonconstant_signed_long_long_atomic_local_bitand_rhs =
        right_value;
    debug_inline_nonconstant_signed_long_long_atomic_local_dividend_escape =
        &debug_inline_nonconstant_signed_long_long_atomic_local_bitand_lhs;
    debug_inline_nonconstant_signed_long_long_atomic_local_divisor_escape =
        &debug_inline_nonconstant_signed_long_long_atomic_local_bitand_rhs;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_atomic_local_bitand_lhs &
        debug_inline_nonconstant_signed_long_long_atomic_local_bitand_rhs);
}

long long debug_inline_nonconstant_signed_long_long_atomic_local_bitor_entry(
    long long left_value, long long right_value)
{
    _Atomic long long debug_inline_nonconstant_signed_long_long_atomic_local_bitor_lhs =
        left_value;
    _Atomic long long debug_inline_nonconstant_signed_long_long_atomic_local_bitor_rhs =
        right_value;
    debug_inline_nonconstant_signed_long_long_atomic_local_dividend_escape =
        &debug_inline_nonconstant_signed_long_long_atomic_local_bitor_lhs;
    debug_inline_nonconstant_signed_long_long_atomic_local_divisor_escape =
        &debug_inline_nonconstant_signed_long_long_atomic_local_bitor_rhs;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_atomic_local_bitor_lhs |
        debug_inline_nonconstant_signed_long_long_atomic_local_bitor_rhs);
}

long long debug_inline_nonconstant_signed_long_long_atomic_local_bitxor_entry(
    long long left_value, long long right_value)
{
    _Atomic long long debug_inline_nonconstant_signed_long_long_atomic_local_bitxor_lhs =
        left_value;
    _Atomic long long debug_inline_nonconstant_signed_long_long_atomic_local_bitxor_rhs =
        right_value;
    debug_inline_nonconstant_signed_long_long_atomic_local_dividend_escape =
        &debug_inline_nonconstant_signed_long_long_atomic_local_bitxor_lhs;
    debug_inline_nonconstant_signed_long_long_atomic_local_divisor_escape =
        &debug_inline_nonconstant_signed_long_long_atomic_local_bitxor_rhs;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_atomic_local_bitxor_lhs ^
        debug_inline_nonconstant_signed_long_long_atomic_local_bitxor_rhs);
}

unsigned long long
debug_inline_nonconstant_signed_long_long_atomic_local_mixed_bitand_unsigned_rhs_entry(
    long long signed_value, unsigned long long unsigned_value)
{
    _Atomic long long
        debug_inline_atomic_local_mixed_signed_bitand_lhs = signed_value;
    _Atomic unsigned long long
        debug_inline_atomic_local_mixed_unsigned_bitand_rhs = unsigned_value;
    debug_inline_nonconstant_signed_long_long_atomic_local_dividend_escape =
        &debug_inline_atomic_local_mixed_signed_bitand_lhs;
    debug_inline_nonconstant_unsigned_long_long_atomic_local_bitwise_escape =
        &debug_inline_atomic_local_mixed_unsigned_bitand_rhs;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_atomic_local_mixed_signed_bitand_lhs &
        debug_inline_atomic_local_mixed_unsigned_bitand_rhs);
}

unsigned long long
debug_inline_nonconstant_signed_long_long_atomic_local_mixed_bitor_unsigned_rhs_entry(
    long long signed_value, unsigned long long unsigned_value)
{
    _Atomic long long
        debug_inline_atomic_local_mixed_signed_bitor_lhs = signed_value;
    _Atomic unsigned long long
        debug_inline_atomic_local_mixed_unsigned_bitor_rhs = unsigned_value;
    debug_inline_nonconstant_signed_long_long_atomic_local_dividend_escape =
        &debug_inline_atomic_local_mixed_signed_bitor_lhs;
    debug_inline_nonconstant_unsigned_long_long_atomic_local_bitwise_escape =
        &debug_inline_atomic_local_mixed_unsigned_bitor_rhs;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_atomic_local_mixed_signed_bitor_lhs |
        debug_inline_atomic_local_mixed_unsigned_bitor_rhs);
}

unsigned long long
debug_inline_nonconstant_signed_long_long_atomic_local_mixed_bitxor_unsigned_rhs_entry(
    long long signed_value, unsigned long long unsigned_value)
{
    _Atomic long long
        debug_inline_atomic_local_mixed_signed_bitxor_lhs = signed_value;
    _Atomic unsigned long long
        debug_inline_atomic_local_mixed_unsigned_bitxor_rhs = unsigned_value;
    debug_inline_nonconstant_signed_long_long_atomic_local_dividend_escape =
        &debug_inline_atomic_local_mixed_signed_bitxor_lhs;
    debug_inline_nonconstant_unsigned_long_long_atomic_local_bitwise_escape =
        &debug_inline_atomic_local_mixed_unsigned_bitxor_rhs;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_atomic_local_mixed_signed_bitxor_lhs ^
        debug_inline_atomic_local_mixed_unsigned_bitxor_rhs);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_atomic_local_mixed_bitand_signed_rhs_entry(
    unsigned long long unsigned_value, long long signed_value)
{
    _Atomic unsigned long long
        debug_inline_atomic_local_mixed_unsigned_bitand_lhs = unsigned_value;
    _Atomic long long
        debug_inline_atomic_local_mixed_signed_bitand_rhs = signed_value;
    debug_inline_nonconstant_unsigned_long_long_atomic_local_bitwise_escape =
        &debug_inline_atomic_local_mixed_unsigned_bitand_lhs;
    debug_inline_nonconstant_signed_long_long_atomic_local_divisor_escape =
        &debug_inline_atomic_local_mixed_signed_bitand_rhs;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_atomic_local_mixed_unsigned_bitand_lhs &
        debug_inline_atomic_local_mixed_signed_bitand_rhs);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_atomic_local_mixed_bitor_signed_rhs_entry(
    unsigned long long unsigned_value, long long signed_value)
{
    _Atomic unsigned long long
        debug_inline_atomic_local_mixed_unsigned_bitor_lhs = unsigned_value;
    _Atomic long long
        debug_inline_atomic_local_mixed_signed_bitor_rhs = signed_value;
    debug_inline_nonconstant_unsigned_long_long_atomic_local_bitwise_escape =
        &debug_inline_atomic_local_mixed_unsigned_bitor_lhs;
    debug_inline_nonconstant_signed_long_long_atomic_local_divisor_escape =
        &debug_inline_atomic_local_mixed_signed_bitor_rhs;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_atomic_local_mixed_unsigned_bitor_lhs |
        debug_inline_atomic_local_mixed_signed_bitor_rhs);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_atomic_local_mixed_bitxor_signed_rhs_entry(
    unsigned long long unsigned_value, long long signed_value)
{
    _Atomic unsigned long long
        debug_inline_atomic_local_mixed_unsigned_bitxor_lhs = unsigned_value;
    _Atomic long long
        debug_inline_atomic_local_mixed_signed_bitxor_rhs = signed_value;
    debug_inline_nonconstant_unsigned_long_long_atomic_local_bitwise_escape =
        &debug_inline_atomic_local_mixed_unsigned_bitxor_lhs;
    debug_inline_nonconstant_signed_long_long_atomic_local_divisor_escape =
        &debug_inline_atomic_local_mixed_signed_bitxor_rhs;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_atomic_local_mixed_unsigned_bitxor_lhs ^
        debug_inline_atomic_local_mixed_signed_bitxor_rhs);
}

long long debug_inline_nonconstant_signed_long_long_volatile_local_bitand_entry(
    long long left_value, long long right_value)
{
    volatile long long
        debug_inline_nonconstant_signed_long_long_volatile_local_bitand_lhs =
            left_value;
    volatile long long
        debug_inline_nonconstant_signed_long_long_volatile_local_bitand_rhs =
            right_value;
    debug_inline_nonconstant_signed_long_long_volatile_local_lhs_escape =
        &debug_inline_nonconstant_signed_long_long_volatile_local_bitand_lhs;
    debug_inline_nonconstant_signed_long_long_volatile_local_rhs_escape =
        &debug_inline_nonconstant_signed_long_long_volatile_local_bitand_rhs;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_volatile_local_bitand_lhs &
        debug_inline_nonconstant_signed_long_long_volatile_local_bitand_rhs);
}

long long debug_inline_nonconstant_signed_long_long_volatile_local_bitor_entry(
    long long left_value, long long right_value)
{
    volatile long long
        debug_inline_nonconstant_signed_long_long_volatile_local_bitor_lhs =
            left_value;
    volatile long long
        debug_inline_nonconstant_signed_long_long_volatile_local_bitor_rhs =
            right_value;
    debug_inline_nonconstant_signed_long_long_volatile_local_lhs_escape =
        &debug_inline_nonconstant_signed_long_long_volatile_local_bitor_lhs;
    debug_inline_nonconstant_signed_long_long_volatile_local_rhs_escape =
        &debug_inline_nonconstant_signed_long_long_volatile_local_bitor_rhs;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_volatile_local_bitor_lhs |
        debug_inline_nonconstant_signed_long_long_volatile_local_bitor_rhs);
}

long long debug_inline_nonconstant_signed_long_long_volatile_local_bitxor_entry(
    long long left_value, long long right_value)
{
    volatile long long
        debug_inline_nonconstant_signed_long_long_volatile_local_bitxor_lhs =
            left_value;
    volatile long long
        debug_inline_nonconstant_signed_long_long_volatile_local_bitxor_rhs =
            right_value;
    debug_inline_nonconstant_signed_long_long_volatile_local_lhs_escape =
        &debug_inline_nonconstant_signed_long_long_volatile_local_bitxor_lhs;
    debug_inline_nonconstant_signed_long_long_volatile_local_rhs_escape =
        &debug_inline_nonconstant_signed_long_long_volatile_local_bitxor_rhs;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_volatile_local_bitxor_lhs ^
        debug_inline_nonconstant_signed_long_long_volatile_local_bitxor_rhs);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_volatile_local_constant_divide_entry(
    unsigned long long dividend)
{
    volatile unsigned long long
        debug_inline_nonconstant_unsigned_long_long_volatile_local_constant_dividend =
            dividend;
    debug_inline_nonconstant_unsigned_long_long_volatile_local_constant_escape =
        &debug_inline_nonconstant_unsigned_long_long_volatile_local_constant_dividend;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_volatile_local_constant_dividend /
        7ull);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_volatile_local_constant_remainder_entry(
    unsigned long long dividend)
{
    volatile unsigned long long
        debug_inline_nonconstant_unsigned_long_long_volatile_local_remainder_dividend =
            dividend;
    debug_inline_nonconstant_unsigned_long_long_volatile_local_constant_escape =
        &debug_inline_nonconstant_unsigned_long_long_volatile_local_remainder_dividend;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_volatile_local_remainder_dividend %
        7ull);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_atomic_local_constant_divide_entry(
    unsigned long long dividend)
{
    _Atomic unsigned long long
        debug_inline_nonconstant_unsigned_long_long_atomic_local_constant_dividend =
            dividend;
    debug_inline_nonconstant_unsigned_long_long_atomic_local_bitwise_escape =
        &debug_inline_nonconstant_unsigned_long_long_atomic_local_constant_dividend;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_atomic_local_constant_dividend /
        7ull);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_atomic_local_constant_remainder_entry(
    unsigned long long dividend)
{
    _Atomic unsigned long long
        debug_inline_nonconstant_unsigned_long_long_atomic_local_constant_remainder_dividend =
            dividend;
    debug_inline_nonconstant_unsigned_long_long_atomic_local_bitwise_escape =
        &debug_inline_nonconstant_unsigned_long_long_atomic_local_constant_remainder_dividend;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_atomic_local_constant_remainder_dividend %
        7ull);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_atomic_local_high_constant_divide_entry(
    unsigned long long dividend)
{
    _Atomic unsigned long long
        debug_inline_nonconstant_unsigned_long_long_atomic_local_high_constant_dividend =
            dividend;
    debug_inline_nonconstant_unsigned_long_long_atomic_local_bitwise_escape =
        &debug_inline_nonconstant_unsigned_long_long_atomic_local_high_constant_dividend;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_atomic_local_high_constant_dividend /
        0x8000000000000001ull);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_atomic_local_high_constant_remainder_entry(
    unsigned long long dividend)
{
    _Atomic unsigned long long
        debug_inline_nonconstant_unsigned_long_long_atomic_local_high_constant_remainder_dividend =
            dividend;
    debug_inline_nonconstant_unsigned_long_long_atomic_local_bitwise_escape =
        &debug_inline_nonconstant_unsigned_long_long_atomic_local_high_constant_remainder_dividend;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        debug_inline_nonconstant_unsigned_long_long_atomic_local_high_constant_remainder_dividend %
        0x8000000000000001ull);
}

long long
debug_inline_nonconstant_signed_long_long_volatile_local_constant_divide_entry(
    long long dividend)
{
    volatile long long
        debug_inline_nonconstant_signed_long_long_volatile_local_constant_dividend =
            dividend;
    debug_inline_nonconstant_signed_long_long_volatile_local_constant_escape =
        &debug_inline_nonconstant_signed_long_long_volatile_local_constant_dividend;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_volatile_local_constant_dividend /
        -7ll);
}

long long
debug_inline_nonconstant_signed_long_long_volatile_local_constant_remainder_entry(
    long long dividend)
{
    volatile long long
        debug_inline_nonconstant_signed_long_long_volatile_local_constant_remainder_dividend =
            dividend;
    debug_inline_nonconstant_signed_long_long_volatile_local_constant_escape =
        &debug_inline_nonconstant_signed_long_long_volatile_local_constant_remainder_dividend;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_volatile_local_constant_remainder_dividend %
        -7ll);
}

long long
debug_inline_nonconstant_signed_long_long_atomic_local_constant_divide_entry(
    long long dividend)
{
    _Atomic long long
        debug_inline_nonconstant_signed_long_long_atomic_local_constant_dividend =
            dividend;
    debug_inline_nonconstant_signed_long_long_atomic_local_dividend_escape =
        &debug_inline_nonconstant_signed_long_long_atomic_local_constant_dividend;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_atomic_local_constant_dividend /
        -7ll);
}

long long
debug_inline_nonconstant_signed_long_long_atomic_local_constant_remainder_entry(
    long long dividend)
{
    _Atomic long long
        debug_inline_nonconstant_signed_long_long_atomic_local_constant_remainder_dividend =
            dividend;
    debug_inline_nonconstant_signed_long_long_atomic_local_dividend_escape =
        &debug_inline_nonconstant_signed_long_long_atomic_local_constant_remainder_dividend;
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_atomic_local_constant_remainder_dividend %
        -7ll);
}

long long debug_inline_nonconstant_signed_long_long_atomic_constant_divide_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_atomic_dividend / 7ll);
}

long long debug_inline_nonconstant_signed_long_long_atomic_constant_remainder_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        debug_inline_nonconstant_signed_long_long_atomic_dividend % 7ll);
}

long long debug_inline_nonconstant_signed_long_long_atomic_negated_divide_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        -debug_inline_nonconstant_signed_long_long_atomic_dividend / 7ll);
}

long long debug_inline_nonconstant_signed_long_long_atomic_negated_remainder_entry(
    void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        -debug_inline_nonconstant_signed_long_long_atomic_dividend % 7ll);
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

int debug_inline_indirect_pointer_entry(int value)
{
    int debug_inline_indirect_pointer_value = value;
    int* debug_inline_indirect_pointer_slot =
        &debug_inline_indirect_pointer_value;
    debug_inline_indirect_pointer_slot_escape =
        &debug_inline_indirect_pointer_slot;
    return debug_nonconstant_integer_parameter_inline(
        *debug_inline_indirect_pointer_slot);
}

int debug_inline_indirect_pointer_chain_entry(int value)
{
    int debug_inline_indirect_pointer_chain_value = value;
    int* debug_inline_indirect_pointer_chain_target =
        &debug_inline_indirect_pointer_chain_value;
    int** debug_inline_indirect_pointer_chain_slot =
        &debug_inline_indirect_pointer_chain_target;
    debug_inline_indirect_pointer_chain_slot_escape =
        &debug_inline_indirect_pointer_chain_slot;
    return debug_nonconstant_integer_parameter_inline(
        **debug_inline_indirect_pointer_chain_slot);
}

int debug_inline_indirect_pointer_triple_chain_entry(int value)
{
    int debug_inline_indirect_pointer_triple_chain_value = value;
    int* debug_inline_indirect_pointer_triple_chain_target1 =
        &debug_inline_indirect_pointer_triple_chain_value;
    int** debug_inline_indirect_pointer_triple_chain_target2 =
        &debug_inline_indirect_pointer_triple_chain_target1;
    int*** debug_inline_indirect_pointer_triple_chain_slot =
        &debug_inline_indirect_pointer_triple_chain_target2;
    debug_inline_indirect_pointer_triple_chain_slot_escape =
        &debug_inline_indirect_pointer_triple_chain_slot;
    return debug_nonconstant_integer_parameter_inline(
        ***debug_inline_indirect_pointer_triple_chain_slot);
}

int debug_inline_indirect_global_pointer_entry(int value)
{
    int debug_inline_indirect_global_pointer_value = value;
    debug_inline_indirect_global_pointer_source =
        &debug_inline_indirect_global_pointer_value;
    return debug_nonconstant_integer_parameter_inline(
        *debug_inline_indirect_global_pointer_source);
}

int debug_inline_indirect_tls_pointer_entry(int value)
{
    int debug_inline_indirect_tls_pointer_value = value;
    debug_inline_indirect_tls_pointer_source =
        &debug_inline_indirect_tls_pointer_value;
    return debug_nonconstant_integer_parameter_inline(
        *debug_inline_indirect_tls_pointer_source);
}

int debug_inline_indirect_static_pointer_entry(int value)
{
    int debug_inline_indirect_static_pointer_value = value;
    static int* debug_inline_indirect_static_pointer_slot;
    debug_inline_indirect_static_pointer_slot =
        &debug_inline_indirect_static_pointer_value;
    return debug_nonconstant_integer_parameter_inline(
        *debug_inline_indirect_static_pointer_slot);
}

int debug_inline_indirect_volatile_pointer_entry(int value)
{
    int debug_inline_indirect_volatile_pointer_value = value;
    int* volatile debug_inline_indirect_volatile_pointer_slot =
        &debug_inline_indirect_volatile_pointer_value;
    debug_inline_indirect_volatile_pointer_slot_escape =
        &debug_inline_indirect_volatile_pointer_slot;
    return debug_nonconstant_integer_parameter_inline(
        *debug_inline_indirect_volatile_pointer_slot);
}

int debug_inline_indirect_volatile_pointee_entry(int value)
{
    volatile int debug_inline_indirect_volatile_pointee_value = value;
    volatile int* debug_inline_indirect_volatile_pointee_slot =
        &debug_inline_indirect_volatile_pointee_value;
    debug_inline_indirect_volatile_pointee_slot_escape =
        &debug_inline_indirect_volatile_pointee_slot;
    return debug_nonconstant_integer_parameter_inline(
        *debug_inline_indirect_volatile_pointee_slot);
}

int debug_inline_indirect_atomic_pointer_entry(int value)
{
    int debug_inline_indirect_atomic_pointer_value = value;
    _Atomic(int*) debug_inline_indirect_atomic_pointer_slot;
    debug_inline_indirect_atomic_pointer_slot =
        &debug_inline_indirect_atomic_pointer_value;
    return debug_nonconstant_integer_parameter_inline(
        *debug_inline_indirect_atomic_pointer_slot);
}

int debug_inline_indirect_atomic_pointee_entry(int value)
{
    _Atomic(int) debug_inline_indirect_atomic_pointee_value = value;
    _Atomic(int)* debug_inline_indirect_atomic_pointee_slot =
        &debug_inline_indirect_atomic_pointee_value;
    return debug_nonconstant_integer_parameter_inline(
        *debug_inline_indirect_atomic_pointee_slot);
}

long long debug_inline_indirect_wide_pointee_entry(long long value)
{
    long long* debug_inline_indirect_wide_pointee_slot = &value;
    debug_inline_indirect_wide_pointer_slot_escape =
        &debug_inline_indirect_wide_pointee_slot;
    return debug_nonconstant_signed_long_long_parameter_inline(
        *debug_inline_indirect_wide_pointee_slot);
}

int debug_inline_nonconstant_integer_logical_not_entry(int value)
{
    int debug_inline_nonconstant_integer_logical_not_source = value;
    debug_inline_nonconstant_integer_escape =
        &debug_inline_nonconstant_integer_logical_not_source;
    return debug_nonconstant_integer_parameter_inline(
        !debug_inline_nonconstant_integer_logical_not_source);
}

int debug_inline_nonconstant_integer_logical_not_wide_entry(long long value)
{
    long long debug_inline_nonconstant_integer_logical_not_wide_source = value;
    debug_inline_nonconstant_integer_logical_not_wide_escape =
        &debug_inline_nonconstant_integer_logical_not_wide_source;
    return debug_nonconstant_integer_parameter_inline(
        !debug_inline_nonconstant_integer_logical_not_wide_source);
}

int debug_inline_nonconstant_unsigned_integer_logical_not_wide_shift_entry(
    unsigned long long value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_integer_logical_not_wide_shift_source =
            value;
    debug_inline_nonconstant_unsigned_integer_logical_not_wide_escape =
        &debug_inline_nonconstant_unsigned_integer_logical_not_wide_shift_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_nonconstant_unsigned_integer_logical_not_wide_shift_source >>
          33u));
}

int debug_inline_nonconstant_integer_logical_not_equality_entry(int value)
{
    int debug_inline_nonconstant_integer_logical_not_equality_source = value;
    debug_inline_nonconstant_integer_escape =
        &debug_inline_nonconstant_integer_logical_not_equality_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_nonconstant_integer_logical_not_equality_source == 0));
}

#define DEBUG_INLINE_SIGNED_COMPARE_ENTRY(entry_name, comparison) \
    int entry_name(int value) \
    { \
        int entry_name##_source = value; \
        debug_inline_nonconstant_integer_escape = &entry_name##_source; \
        return debug_nonconstant_integer_parameter_inline( \
            !(entry_name##_source comparison 0)); \
    }

DEBUG_INLINE_SIGNED_COMPARE_ENTRY(
    debug_inline_nonconstant_integer_compare_less_entry, <)
DEBUG_INLINE_SIGNED_COMPARE_ENTRY(
    debug_inline_nonconstant_integer_compare_greater_entry, >)
DEBUG_INLINE_SIGNED_COMPARE_ENTRY(
    debug_inline_nonconstant_integer_compare_less_equal_entry, <=)
DEBUG_INLINE_SIGNED_COMPARE_ENTRY(
    debug_inline_nonconstant_integer_compare_greater_equal_entry, >=)
DEBUG_INLINE_SIGNED_COMPARE_ENTRY(
    debug_inline_nonconstant_integer_compare_not_equal_entry, !=)

#undef DEBUG_INLINE_SIGNED_COMPARE_ENTRY

int debug_inline_nonconstant_unsigned_integer_compare_less_entry(
    unsigned int value)
{
    unsigned int
        debug_inline_nonconstant_unsigned_integer_compare_less_entry_source =
            value;
    debug_inline_nonconstant_unsigned_integer_escape =
        &debug_inline_nonconstant_unsigned_integer_compare_less_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_nonconstant_unsigned_integer_compare_less_entry_source <
          0x80000000u));
}

int debug_inline_nonconstant_integer_compare_mixed_entry(int value)
{
    int debug_inline_nonconstant_integer_compare_mixed_entry_source = value;
    debug_inline_nonconstant_integer_escape =
        &debug_inline_nonconstant_integer_compare_mixed_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !((unsigned int)
              debug_inline_nonconstant_integer_compare_mixed_entry_source <
          0x80000000u));
}

int debug_inline_nonconstant_integer_compare_implicit_entry(int value)
{
    int debug_inline_nonconstant_integer_compare_implicit_entry_source = value;
    debug_inline_nonconstant_integer_escape =
        &debug_inline_nonconstant_integer_compare_implicit_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_nonconstant_integer_compare_implicit_entry_source <
          0x80000000u));
}

int debug_inline_nonconstant_integer_compare_promoted_short_entry(int value)
{
    int debug_inline_nonconstant_integer_compare_promoted_short_entry_source =
        value;
    debug_inline_nonconstant_integer_escape =
        &debug_inline_nonconstant_integer_compare_promoted_short_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !((short)
              debug_inline_nonconstant_integer_compare_promoted_short_entry_source <
          0));
}

int debug_inline_nonconstant_integer_compare_promoted_unsigned_short_entry(
    int value)
{
    int
        debug_inline_nonconstant_integer_compare_promoted_unsigned_short_entry_source =
            value;
    debug_inline_nonconstant_integer_escape =
        &debug_inline_nonconstant_integer_compare_promoted_unsigned_short_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !((unsigned short)
              debug_inline_nonconstant_integer_compare_promoted_unsigned_short_entry_source <
          0x8000u));
}

int debug_inline_nonconstant_long_compare_unsigned_int_entry(long value)
{
    long debug_inline_nonconstant_long_compare_unsigned_int_entry_source =
        value;
    debug_inline_nonconstant_long_escape =
        &debug_inline_nonconstant_long_compare_unsigned_int_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_nonconstant_long_compare_unsigned_int_entry_source <
          0x80000000u));
}

int debug_inline_nonconstant_unsigned_long_compare_signed_int_entry(
    unsigned long value)
{
    unsigned long
        debug_inline_nonconstant_unsigned_long_compare_signed_int_entry_source =
            value;
    debug_inline_nonconstant_unsigned_long_escape =
        &debug_inline_nonconstant_unsigned_long_compare_signed_int_entry_source;
    return debug_nonconstant_integer_parameter_inline(
        !(debug_inline_nonconstant_unsigned_long_compare_signed_int_entry_source <
          -1));
}

int debug_inline_nonconstant_unsigned_long_long_wide_divmod_entry(int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_wide_source =
            (unsigned long long)(unsigned int)value;
    int divided = debug_nonconstant_unsigned_long_long_wide_divide_inline(
        debug_inline_nonconstant_unsigned_long_long_wide_source / 32769ull);
    int remainder =
        debug_nonconstant_unsigned_long_long_wide_remainder_inline(
            debug_inline_nonconstant_unsigned_long_long_wide_source %
                65536ull);
    int divided_wider = debug_nonconstant_unsigned_long_long_wider_divide_inline(
        debug_inline_nonconstant_unsigned_long_long_wide_source / 65537ull);
    int remainder_wider =
        debug_nonconstant_unsigned_long_long_wider_remainder_inline(
            debug_inline_nonconstant_unsigned_long_long_wide_source %
                131072ull);
    int divided_widest = debug_nonconstant_unsigned_long_long_widest_divide_inline(
        debug_inline_nonconstant_unsigned_long_long_wide_source /
            1073741825ull);
    int remainder_widest =
        debug_nonconstant_unsigned_long_long_widest_remainder_inline(
            debug_inline_nonconstant_unsigned_long_long_wide_source %
                2147483647ull);
    int divided_u32 =
        debug_nonconstant_unsigned_long_long_u32_divide_inline(
            debug_inline_nonconstant_unsigned_long_long_wide_source /
                2147483649ull);
    int remainder_u32 =
        debug_nonconstant_unsigned_long_long_u32_remainder_inline(
            debug_inline_nonconstant_unsigned_long_long_wide_source %
                4294967295ull);
    int divided_power =
        debug_nonconstant_unsigned_long_long_power_divide_inline(
            debug_inline_nonconstant_unsigned_long_long_wide_source /
                4294967296ull);
    int remainder_power =
        debug_nonconstant_unsigned_long_long_power_remainder_inline(
            debug_inline_nonconstant_unsigned_long_long_wide_source %
                4294967296ull);
    int divided_wide_power =
        debug_nonconstant_unsigned_long_long_wide_power_divide_inline(
            debug_inline_nonconstant_unsigned_long_long_wide_source /
                1099511627776ull);
    int remainder_wide_power =
        debug_nonconstant_unsigned_long_long_wide_power_remainder_inline(
            debug_inline_nonconstant_unsigned_long_long_wide_source %
                4611686018427387904ull);
    debug_inline_nonconstant_unsigned_long_long_wide_escape =
        &debug_inline_nonconstant_unsigned_long_long_wide_source;
    return divided + remainder + divided_wider + remainder_wider +
        divided_widest + remainder_widest + divided_u32 + remainder_u32 +
        divided_power + remainder_power + divided_wide_power +
        remainder_wide_power;
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

int debug_inline_nonconstant_unsigned_long_long_nested_mul_two_variables_entry(
    unsigned int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_nested_mul_left_source =
            ((unsigned long long)value << 32u) | 0xfffffff1ull;
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_nested_mul_rhs_source =
            ((unsigned long long)value << 16u) | 0x12345ull;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_nested_mul_left_source;
    debug_inline_nonconstant_unsigned_long_long_rhs_escape =
        &debug_inline_nonconstant_unsigned_long_long_nested_mul_rhs_source;
    return (int)debug_nonconstant_unsigned_long_long_mul_two_variables_inline(
        ((debug_inline_nonconstant_unsigned_long_long_nested_mul_left_source +
          0x100000001ull) *
         (debug_inline_nonconstant_unsigned_long_long_nested_mul_rhs_source -
          0x100000001ull)) *
        (debug_inline_nonconstant_unsigned_long_long_nested_mul_left_source +
         0x100000001ull));
}

int debug_inline_nonconstant_unsigned_long_long_nested_shift_add_entry(
    unsigned int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_nested_shift_source =
            ((unsigned long long)value << 32u) | 0x13579bdfu;
    unsigned int
        debug_inline_nonconstant_unsigned_long_long_nested_shift_count =
            value & 63u;
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_nested_shift_addend =
            ((unsigned long long)(value ^ 0xa5a5a5a5u) << 17u) | 0x2468u;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_nested_shift_source;
    debug_inline_nonconstant_unsigned_long_long_rhs_escape =
        &debug_inline_nonconstant_unsigned_long_long_nested_shift_addend;
    debug_inline_nonconstant_shift_count_escape =
        &debug_inline_nonconstant_unsigned_long_long_nested_shift_count;
    return (int)debug_nonconstant_unsigned_long_long_add_two_variables_inline(
        (debug_inline_nonconstant_unsigned_long_long_nested_shift_source <<
         debug_inline_nonconstant_unsigned_long_long_nested_shift_count) +
        debug_inline_nonconstant_unsigned_long_long_nested_shift_addend);
}

int debug_inline_nonconstant_unsigned_long_long_nested_rshift_add_entry(
    unsigned int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_nested_rshift_source =
            ((unsigned long long)(value ^ 0x5a5a5a5au) << 32u) | 0xfdb97531u;
    unsigned int
        debug_inline_nonconstant_unsigned_long_long_nested_rshift_count =
            value & 63u;
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_nested_rshift_addend =
            ((unsigned long long)value << 19u) | 0x1357u;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_nested_rshift_source;
    debug_inline_nonconstant_unsigned_long_long_rhs_escape =
        &debug_inline_nonconstant_unsigned_long_long_nested_rshift_addend;
    debug_inline_nonconstant_shift_count_escape =
        &debug_inline_nonconstant_unsigned_long_long_nested_rshift_count;
    return (int)debug_nonconstant_unsigned_long_long_add_two_variables_inline(
        (debug_inline_nonconstant_unsigned_long_long_nested_rshift_source >>
         debug_inline_nonconstant_unsigned_long_long_nested_rshift_count) +
        debug_inline_nonconstant_unsigned_long_long_nested_rshift_addend);
}

int debug_inline_nonconstant_unsigned_long_long_nested_shift_or_entry(
    unsigned int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_nested_shift_or_source =
            ((unsigned long long)(value + 0x31415927u) << 32u) | 0xdeadbeefu;
    unsigned int
        debug_inline_nonconstant_unsigned_long_long_nested_shift_or_count =
            value & 63u;
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_nested_shift_or_rhs =
            ((unsigned long long)(value ^ 0x27182818u) << 21u) | 0x1234u;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_nested_shift_or_source;
    debug_inline_nonconstant_unsigned_long_long_rhs_escape =
        &debug_inline_nonconstant_unsigned_long_long_nested_shift_or_rhs;
    debug_inline_nonconstant_shift_count_escape =
        &debug_inline_nonconstant_unsigned_long_long_nested_shift_or_count;
    return (int)debug_nonconstant_unsigned_long_long_or_two_variables_inline(
        (debug_inline_nonconstant_unsigned_long_long_nested_shift_or_source <<
         debug_inline_nonconstant_unsigned_long_long_nested_shift_or_count) |
        debug_inline_nonconstant_unsigned_long_long_nested_shift_or_rhs);
}

int debug_inline_nonconstant_unsigned_long_long_nested_shift_and_entry(
    unsigned int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_nested_shift_and_source =
            ((unsigned long long)(value + 0x10203040u) << 32u) | 0x89abcdefu;
    unsigned int
        debug_inline_nonconstant_unsigned_long_long_nested_shift_and_count =
            value & 63u;
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_nested_shift_and_rhs =
            ((unsigned long long)(value ^ 0xfedcba98u) << 13u) | 0x5678u;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_nested_shift_and_source;
    debug_inline_nonconstant_unsigned_long_long_rhs_escape =
        &debug_inline_nonconstant_unsigned_long_long_nested_shift_and_rhs;
    debug_inline_nonconstant_shift_count_escape =
        &debug_inline_nonconstant_unsigned_long_long_nested_shift_and_count;
    return (int)debug_nonconstant_unsigned_long_long_and_two_variables_inline(
        (debug_inline_nonconstant_unsigned_long_long_nested_shift_and_source <<
         debug_inline_nonconstant_unsigned_long_long_nested_shift_and_count) &
        debug_inline_nonconstant_unsigned_long_long_nested_shift_and_rhs);
}

int debug_inline_nonconstant_unsigned_long_long_nested_shift_xor_entry(
    unsigned int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_nested_shift_xor_source =
            ((unsigned long long)(value ^ 0x89abcdefu) << 32u) | 0x01234567u;
    unsigned int
        debug_inline_nonconstant_unsigned_long_long_nested_shift_xor_count =
            value & 63u;
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_nested_shift_xor_rhs =
            ((unsigned long long)(value + 0x76543210u) << 15u) | 0xfedcu;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_nested_shift_xor_source;
    debug_inline_nonconstant_unsigned_long_long_rhs_escape =
        &debug_inline_nonconstant_unsigned_long_long_nested_shift_xor_rhs;
    debug_inline_nonconstant_shift_count_escape =
        &debug_inline_nonconstant_unsigned_long_long_nested_shift_xor_count;
    return (int)debug_nonconstant_unsigned_long_long_xor_two_variables_inline(
        (debug_inline_nonconstant_unsigned_long_long_nested_shift_xor_source <<
         debug_inline_nonconstant_unsigned_long_long_nested_shift_xor_count) ^
        debug_inline_nonconstant_unsigned_long_long_nested_shift_xor_rhs);
}

int debug_inline_nonconstant_unsigned_long_long_scope_mul_two_variables_entry(
    unsigned int value)
{
    {
        {
            unsigned long long
                debug_inline_nonconstant_unsigned_long_long_scope_mul_left_source =
                    ((unsigned long long)value << 32u) | 0xfffffff1ull;
            unsigned long long
                debug_inline_nonconstant_unsigned_long_long_scope_mul_rhs_source =
                    ((unsigned long long)value << 16u) | 0x12345ull;
            debug_inline_nonconstant_unsigned_long_long_escape =
                &debug_inline_nonconstant_unsigned_long_long_scope_mul_left_source;
            debug_inline_nonconstant_unsigned_long_long_rhs_escape =
                &debug_inline_nonconstant_unsigned_long_long_scope_mul_rhs_source;
            return (int)debug_nonconstant_unsigned_long_long_mul_two_variables_inline(
                (0u,
                 (0u,
                  debug_inline_nonconstant_unsigned_long_long_scope_mul_left_source +
                      0x100000001ull) *
                 (0u,
                  debug_inline_nonconstant_unsigned_long_long_scope_mul_rhs_source -
                      0x100000001ull)));
        }
    }
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

int debug_inline_nonconstant_unsigned_long_long_lshift_variable_entry(
    int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_lshift_variable_source =
            (unsigned long long)(unsigned int)value;
    unsigned int
        debug_inline_nonconstant_unsigned_long_long_lshift_variable_count =
            1u;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_lshift_variable_source;
    debug_inline_nonconstant_shift_count_escape =
        &debug_inline_nonconstant_unsigned_long_long_lshift_variable_count;
    return (int)
        debug_nonconstant_unsigned_long_long_lshift_variable_inline(
            debug_inline_nonconstant_unsigned_long_long_lshift_variable_source
                << debug_inline_nonconstant_unsigned_long_long_lshift_variable_count);
}

int debug_inline_nonconstant_unsigned_long_long_rshift_variable_entry(
    int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_rshift_variable_source =
            (unsigned long long)(unsigned int)value;
    unsigned int
        debug_inline_nonconstant_unsigned_long_long_rshift_variable_count =
            1u;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_rshift_variable_source;
    debug_inline_nonconstant_shift_count_escape =
        &debug_inline_nonconstant_unsigned_long_long_rshift_variable_count;
    return (int)
        debug_nonconstant_unsigned_long_long_rshift_variable_inline(
            debug_inline_nonconstant_unsigned_long_long_rshift_variable_source
                >> debug_inline_nonconstant_unsigned_long_long_rshift_variable_count);
}

int debug_inline_nonconstant_unsigned_long_long_lshift_computed_count_entry(
    int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_lshift_computed_count_source =
            (unsigned long long)(unsigned int)value;
    unsigned int
        debug_inline_nonconstant_unsigned_long_long_lshift_computed_count_count =
            1u;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_lshift_computed_count_source;
    debug_inline_nonconstant_shift_count_escape =
        &debug_inline_nonconstant_unsigned_long_long_lshift_computed_count_count;
    return (int)
        debug_nonconstant_unsigned_long_long_lshift_variable_inline(
            debug_inline_nonconstant_unsigned_long_long_lshift_computed_count_source
                << (debug_inline_nonconstant_unsigned_long_long_lshift_computed_count_count +
                    1u));
}

int debug_inline_nonconstant_unsigned_long_long_lshift_wide_computed_count_entry(
    int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_lshift_wide_computed_count_source =
            (unsigned long long)(unsigned int)value;
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_lshift_wide_computed_count_count =
            1ull;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_lshift_wide_computed_count_source;
    debug_inline_nonconstant_unsigned_long_long_rhs_escape =
        &debug_inline_nonconstant_unsigned_long_long_lshift_wide_computed_count_count;
    return (int)
        debug_nonconstant_unsigned_long_long_lshift_variable_inline(
            debug_inline_nonconstant_unsigned_long_long_lshift_wide_computed_count_source
                << (debug_inline_nonconstant_unsigned_long_long_lshift_wide_computed_count_count +
                    1ull));
}

int debug_inline_nonconstant_unsigned_long_long_lshift_mod64_count_entry(
    int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_lshift_mod64_count_source =
            (unsigned long long)(unsigned int)value;
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_lshift_mod64_count_count =
            (unsigned long long)(unsigned int)value;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_lshift_mod64_count_source;
    debug_inline_nonconstant_unsigned_long_long_rhs_escape =
        &debug_inline_nonconstant_unsigned_long_long_lshift_mod64_count_count;
    return (int)
        debug_nonconstant_unsigned_long_long_lshift_variable_inline(
            debug_inline_nonconstant_unsigned_long_long_lshift_mod64_count_source
                << (debug_inline_nonconstant_unsigned_long_long_lshift_mod64_count_count %
                    64ull));
}

int debug_inline_nonconstant_signed_long_long_lshift_mod64_count_entry(
    int value)
{
    long long
        debug_inline_nonconstant_signed_long_long_lshift_mod64_count_source =
            (long long)value;
    long long
        debug_inline_nonconstant_signed_long_long_lshift_mod64_count_count =
            (long long)(unsigned int)value;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_lshift_mod64_count_source;
    debug_inline_nonconstant_signed_long_long_rhs_escape =
        &debug_inline_nonconstant_signed_long_long_lshift_mod64_count_count;
    return (int)
        debug_nonconstant_signed_long_long_lshift_variable_inline(
            debug_inline_nonconstant_signed_long_long_lshift_mod64_count_source
                << (debug_inline_nonconstant_signed_long_long_lshift_mod64_count_count %
                    64ll));
}

int debug_inline_nonconstant_signed_long_long_lshift_computed_mod64_count_entry(
    int value)
{
    long long
        debug_inline_nonconstant_signed_long_long_lshift_computed_mod64_count_source =
            (long long)value;
    long long
        debug_inline_nonconstant_signed_long_long_lshift_computed_mod64_count_count =
            (long long)(unsigned int)value;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_lshift_computed_mod64_count_source;
    debug_inline_nonconstant_signed_long_long_rhs_escape =
        &debug_inline_nonconstant_signed_long_long_lshift_computed_mod64_count_count;
    return (int)
        debug_nonconstant_signed_long_long_lshift_variable_inline(
            debug_inline_nonconstant_signed_long_long_lshift_computed_mod64_count_source
                << ((debug_inline_nonconstant_signed_long_long_lshift_computed_mod64_count_count +
                     1ll) % 64ll));
}

int debug_inline_nonconstant_signed_long_long_lshift_variable_entry(
    int value)
{
    long long
        debug_inline_nonconstant_signed_long_long_lshift_variable_source =
            (long long)value;
    unsigned int
        debug_inline_nonconstant_signed_long_long_lshift_variable_count = 1u;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_lshift_variable_source;
    debug_inline_nonconstant_shift_count_escape =
        &debug_inline_nonconstant_signed_long_long_lshift_variable_count;
    return (int)
        debug_nonconstant_signed_long_long_lshift_variable_inline(
            debug_inline_nonconstant_signed_long_long_lshift_variable_source
                << debug_inline_nonconstant_signed_long_long_lshift_variable_count);
}

int debug_inline_nonconstant_signed_long_long_rshift_variable_entry(
    int value)
{
    long long
        debug_inline_nonconstant_signed_long_long_rshift_variable_source =
            (long long)value;
    unsigned int
        debug_inline_nonconstant_signed_long_long_rshift_variable_count = 1u;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_rshift_variable_source;
    debug_inline_nonconstant_shift_count_escape =
        &debug_inline_nonconstant_signed_long_long_rshift_variable_count;
    return (int)
        debug_nonconstant_signed_long_long_rshift_variable_inline(
            debug_inline_nonconstant_signed_long_long_rshift_variable_source
                >> debug_inline_nonconstant_signed_long_long_rshift_variable_count);
}

int debug_inline_nonconstant_unsigned_long_long_lshift_wide_count_entry(
    int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_lshift_wide_count_source =
            (unsigned long long)(unsigned int)value;
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_lshift_wide_count_count =
            1ull;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_lshift_wide_count_source;
    debug_inline_nonconstant_unsigned_long_long_rhs_escape =
        &debug_inline_nonconstant_unsigned_long_long_lshift_wide_count_count;
    return (int)
        debug_nonconstant_unsigned_long_long_lshift_variable_inline(
            debug_inline_nonconstant_unsigned_long_long_lshift_wide_count_source
                << debug_inline_nonconstant_unsigned_long_long_lshift_wide_count_count);
}

int debug_inline_nonconstant_unsigned_long_long_rshift_wide_count_entry(
    int value)
{
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_rshift_wide_count_source =
            (unsigned long long)(unsigned int)value;
    unsigned long long
        debug_inline_nonconstant_unsigned_long_long_rshift_wide_count_count =
            1ull;
    debug_inline_nonconstant_unsigned_long_long_escape =
        &debug_inline_nonconstant_unsigned_long_long_rshift_wide_count_source;
    debug_inline_nonconstant_unsigned_long_long_rhs_escape =
        &debug_inline_nonconstant_unsigned_long_long_rshift_wide_count_count;
    return (int)
        debug_nonconstant_unsigned_long_long_rshift_variable_inline(
            debug_inline_nonconstant_unsigned_long_long_rshift_wide_count_source
                >> debug_inline_nonconstant_unsigned_long_long_rshift_wide_count_count);
}

int debug_inline_nonconstant_signed_long_long_lshift_wide_count_entry(
    int value)
{
    long long
        debug_inline_nonconstant_signed_long_long_lshift_wide_count_source =
            (long long)value;
    unsigned long long
        debug_inline_nonconstant_signed_long_long_lshift_wide_count_count =
            1ull;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_lshift_wide_count_source;
    debug_inline_nonconstant_unsigned_long_long_rhs_escape =
        &debug_inline_nonconstant_signed_long_long_lshift_wide_count_count;
    return (int)
        debug_nonconstant_signed_long_long_lshift_variable_inline(
            debug_inline_nonconstant_signed_long_long_lshift_wide_count_source
                << debug_inline_nonconstant_signed_long_long_lshift_wide_count_count);
}

int debug_inline_nonconstant_signed_long_long_rshift_wide_count_entry(
    int value)
{
    long long
        debug_inline_nonconstant_signed_long_long_rshift_wide_count_source =
            (long long)value;
    unsigned long long
        debug_inline_nonconstant_signed_long_long_rshift_wide_count_count =
            1ull;
    debug_inline_nonconstant_signed_long_long_escape =
        &debug_inline_nonconstant_signed_long_long_rshift_wide_count_source;
    debug_inline_nonconstant_unsigned_long_long_rhs_escape =
        &debug_inline_nonconstant_signed_long_long_rshift_wide_count_count;
    return (int)
        debug_nonconstant_signed_long_long_rshift_variable_inline(
            debug_inline_nonconstant_signed_long_long_rshift_wide_count_source
                >> debug_inline_nonconstant_signed_long_long_rshift_wide_count_count);
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

float debug_inline_nonconstant_float_entry(float input)
{
    float debug_inline_nonconstant_float_source = input + 0.25f;
    debug_inline_nonconstant_float_escape =
        &debug_inline_nonconstant_float_source;
    return debug_float_parameter_inline(
        debug_inline_nonconstant_float_source);
}

double debug_inline_nonconstant_double_entry(double input)
{
    double debug_inline_nonconstant_double_source = input + 0.25;
    debug_inline_nonconstant_double_escape =
        &debug_inline_nonconstant_double_source;
    return debug_double_parameter_inline(
        debug_inline_nonconstant_double_source);
}

float debug_inline_nonconstant_float_identity_cast_entry(float input)
{
    float debug_inline_nonconstant_float_cast_source = input + 0.5f;
    debug_inline_nonconstant_float_escape =
        &debug_inline_nonconstant_float_cast_source;
    return debug_float_parameter_inline(
        (float)debug_inline_nonconstant_float_cast_source);
}

double debug_inline_nonconstant_double_identity_cast_entry(double input)
{
    double debug_inline_nonconstant_double_cast_source = input + 0.5;
    debug_inline_nonconstant_double_escape =
        &debug_inline_nonconstant_double_cast_source;
    return debug_double_parameter_inline(
        (double)debug_inline_nonconstant_double_cast_source);
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
