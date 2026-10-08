#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

struct VerifiedSysvVaTwoIntegerAggregate {
    long long first;
    long long second;
};

struct VerifiedSysvVaTwoDoubleAggregate {
    double first;
    double second;
};

struct VerifiedSysvMixedAggregate {
    int integer;
    double floating;
};

struct VerifiedSysvVaLargeMemoryAggregate {
    long long first;
    long long second;
    long long third;
};

#ifdef __cplusplus
alignas(16) struct VerifiedSysvVaAlignedMemoryAggregate {
    long long first;
    long long second;
    long long third;
    long long fourth;
};
#endif

long long verified_sysv_va_read_large_memory_aggregate(int marker, ...)
{
    va_list arguments;
    struct VerifiedSysvVaLargeMemoryAggregate value;
    va_start(arguments, marker);
    value = va_arg(arguments, struct VerifiedSysvVaLargeMemoryAggregate);
    va_end(arguments);
    return value.third;
}

long long verified_sysv_va_large_memory_aggregate_call(
    long long first, long long second, long long third)
{
    struct VerifiedSysvVaLargeMemoryAggregate value;
    value.first = first;
    value.second = second;
    value.third = third;
    return verified_sysv_va_read_large_memory_aggregate(7, value);
}

#ifdef __cplusplus
long long verified_sysv_va_read_aligned_memory_aggregate(int marker, ...)
{
    va_list arguments;
    struct VerifiedSysvVaAlignedMemoryAggregate value;
    va_start(arguments, marker);
    value = va_arg(arguments, struct VerifiedSysvVaAlignedMemoryAggregate);
    va_end(arguments);
    return value.fourth;
}

long long verified_sysv_va_aligned_memory_aggregate_simple_call(
    long long first, long long second, long long third, long long fourth)
{
    struct VerifiedSysvVaAlignedMemoryAggregate value;
    value.first = first;
    value.second = second;
    value.third = third;
    value.fourth = fourth;
    return verified_sysv_va_read_aligned_memory_aggregate(7, value);
}

long long verified_sysv_va_read_aligned_memory_aggregate_after_nine_doubles(
    int marker, ...)
{
    va_list arguments;
    double ignored;
    struct VerifiedSysvVaAlignedMemoryAggregate value;
    va_start(arguments, marker);
    for (int index = 0; index < 9; ++index) {
        ignored = va_arg(arguments, double);
    }
    value = va_arg(arguments, struct VerifiedSysvVaAlignedMemoryAggregate);
    struct VerifiedSysvVaLargeMemoryAggregate trailing =
        va_arg(arguments, struct VerifiedSysvVaLargeMemoryAggregate);
    va_end(arguments);
    return value.fourth + trailing.third;
}

long long verified_sysv_va_aligned_memory_aggregate_call(
    double first, double second, double third, double fourth, double fifth,
    double sixth, double seventh, double eighth, double ninth,
    long long value_first, long long value_second,
    long long value_third, long long value_fourth,
    long long trailing_first, long long trailing_second,
    long long trailing_third)
{
    struct VerifiedSysvVaAlignedMemoryAggregate value;
    struct VerifiedSysvVaLargeMemoryAggregate trailing;
    value.first = value_first;
    value.second = value_second;
    value.third = value_third;
    value.fourth = value_fourth;
    trailing.first = trailing_first;
    trailing.second = trailing_second;
    trailing.third = trailing_third;
    return verified_sysv_va_read_aligned_memory_aggregate_after_nine_doubles(
        7, first, second, third, fourth, fifth, sixth, seventh, eighth,
        ninth, value, trailing);
}
#endif

long long verified_sysv_va_read_two_integer_aggregate_after_five(
    int a, int b, int c, int d, int last, ...)
{
    va_list arguments;
    struct VerifiedSysvVaTwoIntegerAggregate value;
    int tail;
    va_start(arguments, last);
    value = va_arg(arguments, struct VerifiedSysvVaTwoIntegerAggregate);
    tail = va_arg(arguments, int);
    va_end(arguments);
    return value.second + tail;
}

long long verified_sysv_va_two_integer_aggregate_straddle_call(
    int a, int b, int c, int d, int last,
    long long first, long long second, int tail)
{
    struct VerifiedSysvVaTwoIntegerAggregate value;
    value.first = first;
    value.second = second;
    return verified_sysv_va_read_two_integer_aggregate_after_five(
        a, b, c, d, last, value, tail);
}

double verified_sysv_va_read_two_double_aggregate_after_seven(
    double a, double b, double c, double d,
    double e, double f, double last, ...)
{
    va_list arguments;
    struct VerifiedSysvVaTwoDoubleAggregate value;
    va_start(arguments, last);
    value = va_arg(arguments, struct VerifiedSysvVaTwoDoubleAggregate);
    va_end(arguments);
    return value.second;
}

double verified_sysv_va_two_double_aggregate_straddle_call(
    double a, double b, double c, double d,
    double e, double f, double last, double first, double second)
{
    struct VerifiedSysvVaTwoDoubleAggregate value;
    value.first = first;
    value.second = second;
    return verified_sysv_va_read_two_double_aggregate_after_seven(
        a, b, c, d, e, f, last, value);
}

double verified_sysv_va_read_mixed_aggregate_after_gp_full(
    int a, int b, int c, int d, int e, int last, ...)
{
    va_list arguments;
    struct VerifiedSysvMixedAggregate value;
    va_start(arguments, last);
    value = va_arg(arguments, struct VerifiedSysvMixedAggregate);
    va_end(arguments);
    return (double)value.integer + value.floating;
}

double verified_sysv_va_mixed_aggregate_gp_straddle_call(
    int a, int b, int c, int d, int e, int last,
    int integer, double floating)
{
    struct VerifiedSysvMixedAggregate value;
    value.integer = integer;
    value.floating = floating;
    return verified_sysv_va_read_mixed_aggregate_after_gp_full(
        a, b, c, d, e, last, value);
}

double verified_sysv_va_read_mixed_aggregate_after_sse_full(
    double a, double b, double c, double d,
    double e, double f, double g, double last, ...)
{
    va_list arguments;
    struct VerifiedSysvMixedAggregate value;
    va_start(arguments, last);
    value = va_arg(arguments, struct VerifiedSysvMixedAggregate);
    va_end(arguments);
    return (double)value.integer + value.floating;
}

double verified_sysv_va_mixed_aggregate_sse_straddle_call(
    double a, double b, double c, double d,
    double e, double f, double g, double last,
    int integer, double floating)
{
    struct VerifiedSysvMixedAggregate value;
    value.integer = integer;
    value.floating = floating;
    return verified_sysv_va_read_mixed_aggregate_after_sse_full(
        a, b, c, d, e, f, g, last, value);
}

double verified_sysv_named_mixed_aggregate_after_gp_full(
    int a, int b, int c, int d, int e, int last,
    struct VerifiedSysvMixedAggregate value)
{
    return (double)value.integer + value.floating;
}

double verified_sysv_named_mixed_aggregate_gp_straddle_call(
    int a, int b, int c, int d, int e, int last,
    int integer, double floating)
{
    struct VerifiedSysvMixedAggregate value;
    value.integer = integer;
    value.floating = floating;
    return verified_sysv_named_mixed_aggregate_after_gp_full(
        a, b, c, d, e, last, value);
}

double verified_sysv_named_mixed_aggregate_after_sse_full(
    double a, double b, double c, double d,
    double e, double f, double g, double last,
    struct VerifiedSysvMixedAggregate value)
{
    return (double)value.integer + value.floating;
}

double verified_sysv_named_mixed_aggregate_sse_straddle_call(
    double a, double b, double c, double d,
    double e, double f, double g, double last,
    int integer, double floating)
{
    struct VerifiedSysvMixedAggregate value;
    value.integer = integer;
    value.floating = floating;
    return verified_sysv_named_mixed_aggregate_after_sse_full(
        a, b, c, d, e, f, g, last, value);
}

int verified_sysv_va_named_mixed_aggregate_after_gp_full(
    int a, int b, int c, int d, int e, int last,
    struct VerifiedSysvMixedAggregate value, ...)
{
    va_list arguments;
    int tail;
    va_start(arguments, value);
    tail = va_arg(arguments, int);
    va_end(arguments);
    return value.integer * 100 + tail;
}

int verified_sysv_va_named_mixed_aggregate_gp_straddle_call(
    int integer, double floating, int tail)
{
    struct VerifiedSysvMixedAggregate value;
    value.integer = integer;
    value.floating = floating;
    return verified_sysv_va_named_mixed_aggregate_after_gp_full(
        1, 2, 3, 4, 5, 6, value, tail);
}

double verified_sysv_va_named_mixed_aggregate_after_sse_full(
    double a, double b, double c, double d,
    double e, double f, double g, double last,
    struct VerifiedSysvMixedAggregate value, ...)
{
    va_list arguments;
    double tail;
    va_start(arguments, value);
    tail = va_arg(arguments, double);
    va_end(arguments);
    return tail;
}

double verified_sysv_va_named_mixed_aggregate_sse_straddle_call(
    double a, double b, double c, double d,
    double e, double f, double g, double last,
    int integer, double floating, double tail)
{
    struct VerifiedSysvMixedAggregate value;
    value.integer = integer;
    value.floating = floating;
    return verified_sysv_va_named_mixed_aggregate_after_sse_full(
        a, b, c, d, e, f, g, last, value, tail);
}

#ifdef __cplusplus
}
#endif
