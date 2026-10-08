#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

struct VerifiedSysvMixedAggregate {
    int integer;
    double floating;
};

struct VerifiedSysvIntegerAggregate {
    int first;
    int second;
};

struct VerifiedSysvVaLargeAggregate {
    long long first;
    long long second;
    long long third;
};

struct VerifiedSysvVaTwoIntegerAggregate {
    long long first;
    long long second;
};

struct VerifiedSysvVaDoubleAggregate {
    double value;
};

struct VerifiedSysvVaFloatAggregate {
    float value;
};

int verified_sysv_va_mixed_aggregate_integer(int marker, ...)
{
    va_list arguments;
    int value;
    va_start(arguments, marker);
    value = va_arg(arguments, struct VerifiedSysvMixedAggregate).integer;
    va_end(arguments);
    return value;
}

double verified_sysv_va_mixed_aggregate_floating(int marker, ...)
{
    va_list arguments;
    double value;
    va_start(arguments, marker);
    value = va_arg(arguments, struct VerifiedSysvMixedAggregate).floating;
    va_end(arguments);
    return value;
}

int verified_sysv_va_integer_aggregate_overflow(
    int a, int b, int c, int d, int e, int last, ...)
{
    va_list arguments;
    struct VerifiedSysvIntegerAggregate value;
    va_start(arguments, last);
    value = va_arg(arguments, struct VerifiedSysvIntegerAggregate);
    va_end(arguments);
    return value.first * 10 + value.second;
}

double verified_sysv_va_mixed_aggregate_overflow(
    int a, int b, int c, int d, int e, int last, ...)
{
    va_list arguments;
    double value;
    va_start(arguments, last);
    value = va_arg(arguments, struct VerifiedSysvMixedAggregate).floating;
    va_end(arguments);
    return value;
}

double verified_sysv_va_mixed_aggregate_sse_overflow(
    double a, double b, double c, double d,
    double e, double f, double g, double last, ...)
{
    va_list arguments;
    double value;
    va_start(arguments, last);
    value = va_arg(arguments, struct VerifiedSysvMixedAggregate).floating;
    va_end(arguments);
    return value;
}

long long verified_sysv_va_large_aggregate(int marker, ...)
{
    va_list arguments;
    long long value;
    va_start(arguments, marker);
    value = va_arg(arguments, struct VerifiedSysvVaLargeAggregate).third;
    va_end(arguments);
    return value;
}

int verified_sysv_va_read_integer_aggregate(int marker, ...)
{
    va_list arguments;
    struct VerifiedSysvIntegerAggregate value;
    va_start(arguments, marker);
    value = va_arg(arguments, struct VerifiedSysvIntegerAggregate);
    va_end(arguments);
    return value.first * 10 + value.second;
}

int verified_sysv_va_integer_aggregate_call(int first, int second)
{
    struct VerifiedSysvIntegerAggregate value;
    value.first = first;
    value.second = second;
    return verified_sysv_va_read_integer_aggregate(1, value);
}

long long verified_sysv_va_read_two_integer_aggregate(int marker, ...)
{
    va_list arguments;
    struct VerifiedSysvVaTwoIntegerAggregate value;
    va_start(arguments, marker);
    value = va_arg(arguments, struct VerifiedSysvVaTwoIntegerAggregate);
    va_end(arguments);
    return value.second;
}

long long verified_sysv_va_two_integer_aggregate_call(
    long long first, long long second)
{
    struct VerifiedSysvVaTwoIntegerAggregate value;
    value.first = first;
    value.second = second;
    return verified_sysv_va_read_two_integer_aggregate(1, value);
}

long long verified_sysv_va_read_two_integer_aggregate_after_six(
    int a, int b, int c, int d, int e, int last, ...)
{
    va_list arguments;
    struct VerifiedSysvVaTwoIntegerAggregate value;
    va_start(arguments, last);
    value = va_arg(arguments, struct VerifiedSysvVaTwoIntegerAggregate);
    va_end(arguments);
    return value.second;
}

long long verified_sysv_va_two_integer_aggregate_stack_call(
    int a, int b, int c, int d, int e, int last,
    long long first, long long second)
{
    struct VerifiedSysvVaTwoIntegerAggregate value;
    value.first = first;
    value.second = second;
    return verified_sysv_va_read_two_integer_aggregate_after_six(
        a, b, c, d, e, last, value);
}

long long verified_sysv_va_read_two_integer_aggregate_after_five(
    int a, int b, int c, int d, int last, ...)
{
    va_list arguments;
    struct VerifiedSysvVaTwoIntegerAggregate value;
    va_start(arguments, last);
    value = va_arg(arguments, struct VerifiedSysvVaTwoIntegerAggregate);
    va_end(arguments);
    return value.second;
}

long long verified_sysv_va_two_integer_aggregate_straddle_call(
    int a, int b, int c, int d, int last,
    long long first, long long second)
{
    struct VerifiedSysvVaTwoIntegerAggregate value;
    value.first = first;
    value.second = second;
    return verified_sysv_va_read_two_integer_aggregate_after_five(
        a, b, c, d, last, value);
}

double verified_sysv_va_read_double_aggregate(int marker, ...)
{
    va_list arguments;
    struct VerifiedSysvVaDoubleAggregate value;
    va_start(arguments, marker);
    value = va_arg(arguments, struct VerifiedSysvVaDoubleAggregate);
    va_end(arguments);
    return value.value;
}

double verified_sysv_va_double_aggregate_call(double floating)
{
    struct VerifiedSysvVaDoubleAggregate value;
    value.value = floating;
    return verified_sysv_va_read_double_aggregate(1, value);
}

double verified_sysv_va_read_double_aggregate_after_seven(
    double a, double b, double c, double d,
    double e, double f, double last, ...)
{
    va_list arguments;
    struct VerifiedSysvVaDoubleAggregate value;
    va_start(arguments, last);
    value = va_arg(arguments, struct VerifiedSysvVaDoubleAggregate);
    va_end(arguments);
    return value.value;
}

double verified_sysv_va_double_aggregate_last_xmm_call(
    double a, double b, double c, double d,
    double e, double f, double last, double floating)
{
    struct VerifiedSysvVaDoubleAggregate value;
    value.value = floating;
    return verified_sysv_va_read_double_aggregate_after_seven(
        a, b, c, d, e, f, last, value);
}

double verified_sysv_va_read_double_aggregate_after_eight(
    double a, double b, double c, double d,
    double e, double f, double g, double last, ...)
{
    va_list arguments;
    struct VerifiedSysvVaDoubleAggregate value;
    va_start(arguments, last);
    value = va_arg(arguments, struct VerifiedSysvVaDoubleAggregate);
    va_end(arguments);
    return value.value;
}

double verified_sysv_va_double_aggregate_stack_call(
    double a, double b, double c, double d,
    double e, double f, double g, double last, double floating)
{
    struct VerifiedSysvVaDoubleAggregate value;
    value.value = floating;
    return verified_sysv_va_read_double_aggregate_after_eight(
        a, b, c, d, e, f, g, last, value);
}

float verified_sysv_va_read_float_aggregate(int marker, ...)
{
    va_list arguments;
    struct VerifiedSysvVaFloatAggregate value;
    va_start(arguments, marker);
    value = va_arg(arguments, struct VerifiedSysvVaFloatAggregate);
    va_end(arguments);
    return value.value;
}

float verified_sysv_va_float_aggregate_call(float floating)
{
    struct VerifiedSysvVaFloatAggregate value;
    value.value = floating;
    return verified_sysv_va_read_float_aggregate(1, value);
}

#ifdef __cplusplus
}
#endif
