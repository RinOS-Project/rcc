#include <stdarg.h>

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
