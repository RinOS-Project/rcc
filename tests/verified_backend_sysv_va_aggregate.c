#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

struct VerifiedSysvMixedAggregate {
    int integer;
    double floating;
};

float verified_sysv_va_float_literal(void)
{
    return 1.25f;
}

double verified_sysv_va_double_literal(void)
{
    return 5.25;
}

int verified_sysv_va_named_mixed_aggregate_gp_offset(
    struct VerifiedSysvMixedAggregate value, ...)
{
    va_list arguments;
    int tail;
    va_start(arguments, value);
    tail = va_arg(arguments, int);
    va_end(arguments);
    return value.integer * 100 + tail;
}

double verified_sysv_va_named_mixed_aggregate_sse_offset(
    struct VerifiedSysvMixedAggregate value, ...)
{
    va_list arguments;
    double tail;
    va_start(arguments, value);
    tail = va_arg(arguments, double);
    va_end(arguments);
    return tail;
}

int verified_sysv_va_named_mixed_aggregate_stack_offset(
    int a, int b, int c, int d, int e, int f,
    double fa, double fb, double fc, double fd,
    double fe, double ff, double fg, double fh,
    struct VerifiedSysvMixedAggregate value, ...)
{
    va_list arguments;
    int tail;
    va_start(arguments, value);
    tail = va_arg(arguments, int);
    va_end(arguments);
    return value.integer * 100 + tail;
}

int verified_sysv_va_named_mixed_aggregate_gp_offset_call(
    int integer, double floating, int tail)
{
    struct VerifiedSysvMixedAggregate value;
    value.integer = integer;
    value.floating = floating;
    return verified_sysv_va_named_mixed_aggregate_gp_offset(value, tail);
}

double verified_sysv_va_named_mixed_aggregate_sse_offset_call(
    int integer, double floating, double tail)
{
    struct VerifiedSysvMixedAggregate value;
    value.integer = integer;
    value.floating = floating;
    return verified_sysv_va_named_mixed_aggregate_sse_offset(value, tail);
}

int verified_sysv_va_named_mixed_aggregate_stack_offset_call(
    double fill, int integer, double floating, int tail)
{
    struct VerifiedSysvMixedAggregate value;
    value.integer = integer;
    value.floating = floating;
    return verified_sysv_va_named_mixed_aggregate_stack_offset(
        1, 2, 3, 4, 5, 6,
        fill, fill, fill, fill, fill, fill, fill, fill,
        value, tail);
}

double verified_sysv_named_mixed_aggregate_value(
    struct VerifiedSysvMixedAggregate value)
{
    return value.floating;
}

int verified_sysv_named_mixed_aggregate_integer_value(
    struct VerifiedSysvMixedAggregate value)
{
    return value.integer;
}

double verified_sysv_named_mixed_aggregate_call(int integer, double floating)
{
    struct VerifiedSysvMixedAggregate value;
    value.integer = integer;
    value.floating = floating;
    return verified_sysv_named_mixed_aggregate_value(value);
}

int verified_sysv_named_mixed_aggregate_integer_call(
    int integer, double floating)
{
    struct VerifiedSysvMixedAggregate value;
    value.integer = integer;
    value.floating = floating;
    return verified_sysv_named_mixed_aggregate_integer_value(value);
}

double verified_sysv_named_mixed_aggregate_after_full_banks(
    int a, int b, int c, int d, int e, int last,
    double fa, double fb, double fc, double fd,
    double fe, double ff, double fg, double flast,
    struct VerifiedSysvMixedAggregate value)
{
    return value.floating;
}

int verified_sysv_named_mixed_aggregate_integer_after_full_banks(
    int a, int b, int c, int d, int e, int last,
    double fa, double fb, double fc, double fd,
    double fe, double ff, double fg, double flast,
    struct VerifiedSysvMixedAggregate value)
{
    return value.integer;
}

double verified_sysv_named_mixed_aggregate_stack_call(
    int integer, double floating,
    double fa, double fb, double fc, double fd,
    double fe, double ff, double fg, double flast)
{
    struct VerifiedSysvMixedAggregate value;
    value.integer = integer;
    value.floating = floating;
    return verified_sysv_named_mixed_aggregate_after_full_banks(
        1, 2, 3, 4, 5, 6, fa, fb, fc, fd,
        fe, ff, fg, flast, value);
}

int verified_sysv_named_mixed_aggregate_stack_integer_call(
    int integer, double floating,
    double fa, double fb, double fc, double fd,
    double fe, double ff, double fg, double flast)
{
    struct VerifiedSysvMixedAggregate value;
    value.integer = integer;
    value.floating = floating;
    return verified_sysv_named_mixed_aggregate_integer_after_full_banks(
        1, 2, 3, 4, 5, 6, fa, fb, fc, fd,
        fe, ff, fg, flast, value);
}

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

struct VerifiedSysvVaTwoDoubleAggregate {
    double first;
    double second;
};

int verified_sysv_va_read_mixed_aggregate_integer(int marker, ...)
{
    va_list arguments;
    va_start(arguments, marker);
    marker = va_arg(arguments, struct VerifiedSysvMixedAggregate).integer;
    va_end(arguments);
    return marker;
}

double verified_sysv_va_read_mixed_aggregate_floating(int marker, ...)
{
    va_list arguments;
    double value;
    va_start(arguments, marker);
    value = va_arg(arguments, struct VerifiedSysvMixedAggregate).floating;
    va_end(arguments);
    return value;
}

int verified_sysv_va_mixed_aggregate_integer_call(int integer, double floating)
{
    struct VerifiedSysvMixedAggregate value;
    value.integer = integer;
    value.floating = floating;
    return verified_sysv_va_read_mixed_aggregate_integer(1, value);
}

double verified_sysv_va_mixed_aggregate_floating_call(
    int integer, double floating)
{
    struct VerifiedSysvMixedAggregate value;
    value.integer = integer;
    value.floating = floating;
    return verified_sysv_va_read_mixed_aggregate_floating(1, value);
}

double verified_sysv_va_read_mixed_floating_after_full_banks(
    int marker, ...)
{
    va_list arguments;
    int ignored_integer;
    double ignored_floating;
    double value;
    va_start(arguments, marker);
    ignored_integer = va_arg(arguments, int);
    ignored_integer = va_arg(arguments, int);
    ignored_integer = va_arg(arguments, int);
    ignored_integer = va_arg(arguments, int);
    ignored_integer = va_arg(arguments, int);
    ignored_floating = va_arg(arguments, double);
    ignored_floating = va_arg(arguments, double);
    ignored_floating = va_arg(arguments, double);
    ignored_floating = va_arg(arguments, double);
    ignored_floating = va_arg(arguments, double);
    ignored_floating = va_arg(arguments, double);
    ignored_floating = va_arg(arguments, double);
    ignored_floating = va_arg(arguments, double);
    value = va_arg(arguments, struct VerifiedSysvMixedAggregate).floating;
    va_end(arguments);
    return value;
}

int verified_sysv_va_read_mixed_integer_after_full_banks(
    int marker, ...)
{
    va_list arguments;
    int ignored_integer;
    double ignored_floating;
    int value;
    va_start(arguments, marker);
    ignored_integer = va_arg(arguments, int);
    ignored_integer = va_arg(arguments, int);
    ignored_integer = va_arg(arguments, int);
    ignored_integer = va_arg(arguments, int);
    ignored_integer = va_arg(arguments, int);
    ignored_floating = va_arg(arguments, double);
    ignored_floating = va_arg(arguments, double);
    ignored_floating = va_arg(arguments, double);
    ignored_floating = va_arg(arguments, double);
    ignored_floating = va_arg(arguments, double);
    ignored_floating = va_arg(arguments, double);
    ignored_floating = va_arg(arguments, double);
    ignored_floating = va_arg(arguments, double);
    value = va_arg(arguments, struct VerifiedSysvMixedAggregate).integer;
    va_end(arguments);
    return value;
}

double verified_sysv_va_mixed_aggregate_stack_call(
    int integer, double floating,
    double fa, double fb, double fc, double fd,
    double fe, double ff, double fg, double flast)
{
    struct VerifiedSysvMixedAggregate value;
    value.integer = integer;
    value.floating = floating;
    return verified_sysv_va_read_mixed_floating_after_full_banks(
        1, 2, 3, 4, 5, 6, fa, fb, fc, fd,
        fe, ff, fg, flast, value);
}

int verified_sysv_va_mixed_aggregate_stack_integer_call(
    int integer, double floating,
    double fa, double fb, double fc, double fd,
    double fe, double ff, double fg, double flast)
{
    struct VerifiedSysvMixedAggregate value;
    value.integer = integer;
    value.floating = floating;
    return verified_sysv_va_read_mixed_integer_after_full_banks(
        1, 2, 3, 4, 5, 6, fa, fb, fc, fd,
        fe, ff, fg, flast, value);
}

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

long long verified_sysv_va_large_aggregate_call(
    long long first, long long second, long long third)
{
    struct VerifiedSysvVaLargeAggregate value;
    value.first = first;
    value.second = second;
    value.third = third;
    return verified_sysv_va_large_aggregate(7, value);
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

double verified_sysv_va_read_two_double_aggregate(int marker, ...)
{
    va_list arguments;
    struct VerifiedSysvVaTwoDoubleAggregate value;
    va_start(arguments, marker);
    value = va_arg(arguments, struct VerifiedSysvVaTwoDoubleAggregate);
    va_end(arguments);
    return value.second;
}

double verified_sysv_va_two_double_aggregate_call(
    double first, double second)
{
    struct VerifiedSysvVaTwoDoubleAggregate value;
    value.first = first;
    value.second = second;
    return verified_sysv_va_read_two_double_aggregate(1, value);
}

double verified_sysv_va_read_two_double_aggregate_after_six(
    double a, double b, double c, double d, double e, double last, ...)
{
    va_list arguments;
    struct VerifiedSysvVaTwoDoubleAggregate value;
    va_start(arguments, last);
    value = va_arg(arguments, struct VerifiedSysvVaTwoDoubleAggregate);
    va_end(arguments);
    return value.second;
}

double verified_sysv_va_two_double_aggregate_last_xmm_call(
    double a, double b, double c, double d, double e, double last,
    double first, double second)
{
    struct VerifiedSysvVaTwoDoubleAggregate value;
    value.first = first;
    value.second = second;
    return verified_sysv_va_read_two_double_aggregate_after_six(
        a, b, c, d, e, last, value);
}

double verified_sysv_va_read_two_double_aggregate_after_eight(
    double a, double b, double c, double d,
    double e, double f, double g, double last, ...)
{
    va_list arguments;
    struct VerifiedSysvVaTwoDoubleAggregate value;
    va_start(arguments, last);
    value = va_arg(arguments, struct VerifiedSysvVaTwoDoubleAggregate);
    va_end(arguments);
    return value.second;
}

double verified_sysv_va_two_double_aggregate_stack_call(
    double a, double b, double c, double d,
    double e, double f, double g, double last,
    double first, double second)
{
    struct VerifiedSysvVaTwoDoubleAggregate value;
    value.first = first;
    value.second = second;
    return verified_sysv_va_read_two_double_aggregate_after_eight(
        a, b, c, d, e, f, g, last, value);
}

#ifdef __cplusplus
}
#endif
