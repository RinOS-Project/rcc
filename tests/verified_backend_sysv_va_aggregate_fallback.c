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

#ifdef __cplusplus
}
#endif
