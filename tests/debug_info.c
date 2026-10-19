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

int debug_inline_static_pointer_entry(void)
{
    static int* debug_inline_static_pointer_source;
    return debug_global_pointer_parameter_inline(
        debug_inline_static_pointer_source);
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
