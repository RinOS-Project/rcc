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

struct VerifiedSysvVaAligned32Aggregate {
#ifdef __cplusplus
    alignas(32) long long first;
#else
    _Alignas(32) long long first;
#endif
    long long second;
    long long third;
    long long fourth;
};

struct VerifiedSysvVaAligned64Aggregate {
#ifdef __cplusplus
    alignas(64) long long first;
#else
    _Alignas(64) long long first;
#endif
    long long second;
    long long third;
    long long fourth;
    long long fifth;
    long long sixth;
    long long seventh;
    long long eighth;
};

struct VerifiedSysvVaAligned4096Aggregate {
#ifdef __cplusplus
    alignas(4096) long long first;
#else
    _Alignas(4096) long long first;
#endif
    long long second;
};

struct VerifiedSysvVaAligned16384Aggregate {
#ifdef __cplusplus
    alignas(16384) long long first;
#else
    _Alignas(16384) long long first;
#endif
    long long second;
};

struct VerifiedSysvVaAligned128Aggregate {
#ifdef __cplusplus
    alignas(128) long long first;
#else
    _Alignas(128) long long first;
#endif
    long long second;
};

struct VerifiedSysvVaAligned256Aggregate {
#ifdef __cplusplus
    alignas(256) long long first;
#else
    _Alignas(256) long long first;
#endif
    long long second;
};

struct VerifiedSysvVaAligned512Aggregate {
#ifdef __cplusplus
    alignas(512) long long first;
#else
    _Alignas(512) long long first;
#endif
    long long second;
};

struct VerifiedSysvVaAligned1024Aggregate {
#ifdef __cplusplus
    alignas(1024) long long first;
#else
    _Alignas(1024) long long first;
#endif
    long long second;
};

struct VerifiedSysvVaAligned2048Aggregate {
#ifdef __cplusplus
    alignas(2048) long long first;
#else
    _Alignas(2048) long long first;
#endif
    long long second;
};

#ifdef __cplusplus
static_assert(alignof(VerifiedSysvVaAligned32Aggregate) == 32,
              "C++ variadic MEMORY aggregate alignment");
static_assert(sizeof(VerifiedSysvVaAligned32Aggregate) == 32,
              "C++ variadic MEMORY aggregate size");
#else
_Static_assert(_Alignof(struct VerifiedSysvVaAligned32Aggregate) == 32,
               "C variadic MEMORY aggregate alignment");
_Static_assert(sizeof(struct VerifiedSysvVaAligned32Aggregate) == 32,
               "C variadic MEMORY aggregate size");
#endif

#ifdef __cplusplus
static_assert(alignof(VerifiedSysvVaAligned64Aggregate) == 64,
              "C++ over-aligned variadic MEMORY aggregate alignment");
static_assert(sizeof(VerifiedSysvVaAligned64Aggregate) == 64,
              "C++ over-aligned variadic MEMORY aggregate size");
#else
_Static_assert(_Alignof(struct VerifiedSysvVaAligned64Aggregate) == 64,
               "C over-aligned variadic MEMORY aggregate alignment");
_Static_assert(sizeof(struct VerifiedSysvVaAligned64Aggregate) == 64,
               "C over-aligned variadic MEMORY aggregate size");
#endif

#ifdef __cplusplus
static_assert(alignof(VerifiedSysvVaAligned4096Aggregate) == 4096,
              "C++ maximum variadic MEMORY aggregate alignment");
static_assert(sizeof(VerifiedSysvVaAligned4096Aggregate) == 4096,
              "C++ maximum variadic MEMORY aggregate size");
#else
_Static_assert(_Alignof(struct VerifiedSysvVaAligned4096Aggregate) == 4096,
               "C maximum variadic MEMORY aggregate alignment");
_Static_assert(sizeof(struct VerifiedSysvVaAligned4096Aggregate) == 4096,
               "C maximum variadic MEMORY aggregate size");
#endif

#ifdef __cplusplus
static_assert(alignof(VerifiedSysvVaAligned16384Aggregate) == 16384,
              "C++ extended variadic MEMORY aggregate alignment");
static_assert(sizeof(VerifiedSysvVaAligned16384Aggregate) == 16384,
              "C++ extended variadic MEMORY aggregate size");
#else
_Static_assert(_Alignof(struct VerifiedSysvVaAligned16384Aggregate) == 16384,
               "C extended variadic MEMORY aggregate alignment");
_Static_assert(sizeof(struct VerifiedSysvVaAligned16384Aggregate) == 16384,
               "C extended variadic MEMORY aggregate size");
#endif

#ifdef __cplusplus
static_assert(alignof(VerifiedSysvVaAligned128Aggregate) == 128 &&
                  sizeof(VerifiedSysvVaAligned128Aggregate) == 128,
              "C++ 128-byte variadic MEMORY aggregate layout");
static_assert(alignof(VerifiedSysvVaAligned256Aggregate) == 256 &&
                  sizeof(VerifiedSysvVaAligned256Aggregate) == 256,
              "C++ 256-byte variadic MEMORY aggregate layout");
static_assert(alignof(VerifiedSysvVaAligned512Aggregate) == 512 &&
                  sizeof(VerifiedSysvVaAligned512Aggregate) == 512,
              "C++ 512-byte variadic MEMORY aggregate layout");
static_assert(alignof(VerifiedSysvVaAligned1024Aggregate) == 1024 &&
                  sizeof(VerifiedSysvVaAligned1024Aggregate) == 1024,
              "C++ 1024-byte variadic MEMORY aggregate layout");
static_assert(alignof(VerifiedSysvVaAligned2048Aggregate) == 2048 &&
                  sizeof(VerifiedSysvVaAligned2048Aggregate) == 2048,
              "C++ 2048-byte variadic MEMORY aggregate layout");
#else
_Static_assert(_Alignof(struct VerifiedSysvVaAligned128Aggregate) == 128 &&
                   sizeof(struct VerifiedSysvVaAligned128Aggregate) == 128,
               "C 128-byte variadic MEMORY aggregate layout");
_Static_assert(_Alignof(struct VerifiedSysvVaAligned256Aggregate) == 256 &&
                   sizeof(struct VerifiedSysvVaAligned256Aggregate) == 256,
               "C 256-byte variadic MEMORY aggregate layout");
_Static_assert(_Alignof(struct VerifiedSysvVaAligned512Aggregate) == 512 &&
                   sizeof(struct VerifiedSysvVaAligned512Aggregate) == 512,
               "C 512-byte variadic MEMORY aggregate layout");
_Static_assert(_Alignof(struct VerifiedSysvVaAligned1024Aggregate) == 1024 &&
                   sizeof(struct VerifiedSysvVaAligned1024Aggregate) == 1024,
               "C 1024-byte variadic MEMORY aggregate layout");
_Static_assert(_Alignof(struct VerifiedSysvVaAligned2048Aggregate) == 2048 &&
                   sizeof(struct VerifiedSysvVaAligned2048Aggregate) == 2048,
               "C 2048-byte variadic MEMORY aggregate layout");
#endif

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

long long verified_sysv_va_read_aligned32_aggregate_after_nine_doubles(
    int marker, ...)
{
    va_list arguments;
    struct VerifiedSysvVaAligned32Aggregate value;
    struct VerifiedSysvVaLargeAggregate trailing;
    va_start(arguments, marker);
    for (int index = 0; index < 9; ++index) {
        (void)va_arg(arguments, double);
    }
    value = va_arg(arguments, struct VerifiedSysvVaAligned32Aggregate);
    trailing = va_arg(arguments, struct VerifiedSysvVaLargeAggregate);
    va_end(arguments);
    return value.fourth + trailing.third;
}

long long verified_sysv_va_aligned32_aggregate_call(
    double first, double second, double third, double fourth, double fifth,
    double sixth, double seventh, double eighth, double ninth,
    long long value_first, long long value_second,
    long long value_third, long long value_fourth,
    long long trailing_first, long long trailing_second,
    long long trailing_third)
{
    struct VerifiedSysvVaAligned32Aggregate value;
    struct VerifiedSysvVaLargeAggregate trailing;
    value.first = value_first;
    value.second = value_second;
    value.third = value_third;
    value.fourth = value_fourth;
    trailing.first = trailing_first;
    trailing.second = trailing_second;
    trailing.third = trailing_third;
    return verified_sysv_va_read_aligned32_aggregate_after_nine_doubles(
        7, first, second, third, fourth, fifth, sixth, seventh, eighth,
        ninth, value, trailing);
}

long long verified_sysv_va_read_aligned64_aggregate_after_nine_doubles(
    int marker, ...)
{
    va_list arguments;
    struct VerifiedSysvVaAligned64Aggregate value;
    struct VerifiedSysvVaLargeAggregate trailing;
    va_start(arguments, marker);
    for (int index = 0; index < 9; ++index) {
        (void)va_arg(arguments, double);
    }
    value = va_arg(arguments, struct VerifiedSysvVaAligned64Aggregate);
    trailing = va_arg(arguments, struct VerifiedSysvVaLargeAggregate);
    va_end(arguments);
    return value.eighth + trailing.third;
}

long long verified_sysv_va_aligned64_aggregate_call(
    double first, double second, double third, double fourth, double fifth,
    double sixth, double seventh, double eighth, double ninth,
    long long value_first, long long value_second,
    long long value_third, long long value_fourth,
    long long value_fifth, long long value_sixth,
    long long value_seventh, long long value_eighth,
    long long trailing_first, long long trailing_second,
    long long trailing_third)
{
    struct VerifiedSysvVaAligned64Aggregate value;
    struct VerifiedSysvVaLargeAggregate trailing;
    value.first = value_first;
    value.second = value_second;
    value.third = value_third;
    value.fourth = value_fourth;
    value.fifth = value_fifth;
    value.sixth = value_sixth;
    value.seventh = value_seventh;
    value.eighth = value_eighth;
    trailing.first = trailing_first;
    trailing.second = trailing_second;
    trailing.third = trailing_third;
    return verified_sysv_va_read_aligned64_aggregate_after_nine_doubles(
        7, first, second, third, fourth, fifth, sixth, seventh, eighth,
        ninth, value, trailing);
}

long long verified_sysv_va_read_aligned4096_aggregate_after_nine_doubles(
    int marker, ...)
{
    va_list arguments;
    struct VerifiedSysvVaAligned4096Aggregate value;
    struct VerifiedSysvVaLargeAggregate trailing;
    va_start(arguments, marker);
    for (int index = 0; index < 9; ++index) {
        (void)va_arg(arguments, double);
    }
    value = va_arg(arguments, struct VerifiedSysvVaAligned4096Aggregate);
    trailing = va_arg(arguments, struct VerifiedSysvVaLargeAggregate);
    va_end(arguments);
    return value.second + trailing.third;
}

long long verified_sysv_va_aligned4096_aggregate_call(
    double first, double second, double third, double fourth, double fifth,
    double sixth, double seventh, double eighth, double ninth,
    long long value_first, long long value_second,
    long long trailing_first, long long trailing_second,
    long long trailing_third)
{
    struct VerifiedSysvVaAligned4096Aggregate value;
    struct VerifiedSysvVaLargeAggregate trailing;
    value.first = value_first;
    value.second = value_second;
    trailing.first = trailing_first;
    trailing.second = trailing_second;
    trailing.third = trailing_third;
    return verified_sysv_va_read_aligned4096_aggregate_after_nine_doubles(
        7, first, second, third, fourth, fifth, sixth, seventh, eighth,
        ninth, value, trailing);
}

long long verified_sysv_va_read_aligned16384_aggregate_after_nine_doubles(
    int marker, ...)
{
    va_list arguments;
    struct VerifiedSysvVaAligned16384Aggregate value;
    struct VerifiedSysvVaLargeAggregate trailing;
    va_start(arguments, marker);
    for (int index = 0; index < 9; ++index) {
        (void)va_arg(arguments, double);
    }
    value = va_arg(arguments, struct VerifiedSysvVaAligned16384Aggregate);
    trailing = va_arg(arguments, struct VerifiedSysvVaLargeAggregate);
    va_end(arguments);
    return value.second + trailing.third;
}

long long verified_sysv_va_aligned16384_aggregate_call(
    double first, double second, double third, double fourth, double fifth,
    double sixth, double seventh, double eighth, double ninth,
    long long value_first, long long value_second,
    long long trailing_first, long long trailing_second,
    long long trailing_third)
{
    struct VerifiedSysvVaAligned16384Aggregate value;
    struct VerifiedSysvVaLargeAggregate trailing;
    value.first = value_first;
    value.second = value_second;
    trailing.first = trailing_first;
    trailing.second = trailing_second;
    trailing.third = trailing_third;
    return verified_sysv_va_read_aligned16384_aggregate_after_nine_doubles(
        7, first, second, third, fourth, fifth, sixth, seventh, eighth,
        ninth, value, trailing);
}

long long verified_sysv_va_read_aligned128_to2048_after_nine_doubles(
    int marker, ...)
{
    va_list arguments;
    struct VerifiedSysvVaAligned128Aggregate value128;
    struct VerifiedSysvVaAligned256Aggregate value256;
    struct VerifiedSysvVaAligned512Aggregate value512;
    struct VerifiedSysvVaAligned1024Aggregate value1024;
    struct VerifiedSysvVaAligned2048Aggregate value2048;
    struct VerifiedSysvVaLargeAggregate trailing;
    va_start(arguments, marker);
    for (int index = 0; index < 9; ++index) {
        (void)va_arg(arguments, double);
    }
    value128 = va_arg(arguments, struct VerifiedSysvVaAligned128Aggregate);
    value256 = va_arg(arguments, struct VerifiedSysvVaAligned256Aggregate);
    value512 = va_arg(arguments, struct VerifiedSysvVaAligned512Aggregate);
    value1024 = va_arg(arguments, struct VerifiedSysvVaAligned1024Aggregate);
    value2048 = va_arg(arguments, struct VerifiedSysvVaAligned2048Aggregate);
    trailing = va_arg(arguments, struct VerifiedSysvVaLargeAggregate);
    va_end(arguments);
    return value128.second + value256.second + value512.second +
           value1024.second + value2048.second + trailing.third;
}

long long verified_sysv_va_aligned128_to2048_aggregate_call(
    double first, double second, double third, double fourth, double fifth,
    double sixth, double seventh, double eighth, double ninth,
    long long value128_first, long long value128_second,
    long long value256_first, long long value256_second,
    long long value512_first, long long value512_second,
    long long value1024_first, long long value1024_second,
    long long value2048_first, long long value2048_second,
    long long trailing_first, long long trailing_second,
    long long trailing_third)
{
    struct VerifiedSysvVaAligned128Aggregate value128;
    struct VerifiedSysvVaAligned256Aggregate value256;
    struct VerifiedSysvVaAligned512Aggregate value512;
    struct VerifiedSysvVaAligned1024Aggregate value1024;
    struct VerifiedSysvVaAligned2048Aggregate value2048;
    struct VerifiedSysvVaLargeAggregate trailing;
    value128.first = value128_first;
    value128.second = value128_second;
    value256.first = value256_first;
    value256.second = value256_second;
    value512.first = value512_first;
    value512.second = value512_second;
    value1024.first = value1024_first;
    value1024.second = value1024_second;
    value2048.first = value2048_first;
    value2048.second = value2048_second;
    trailing.first = trailing_first;
    trailing.second = trailing_second;
    trailing.third = trailing_third;
    return verified_sysv_va_read_aligned128_to2048_after_nine_doubles(
        7, first, second, third, fourth, fifth, sixth, seventh, eighth,
        ninth, value128, value256, value512, value1024, value2048, trailing);
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
