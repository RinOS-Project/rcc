#include <stdarg.h>

extern "C" double verified_sysv_va_double_first(int marker, ...)
{
    va_list arguments;
    va_start(arguments, marker);
    double value = va_arg(arguments, double);
    va_end(arguments);
    return value;
}

extern "C" double verified_sysv_va_double_second(int marker, ...)
{
    va_list arguments;
    va_start(arguments, marker);
    va_arg(arguments, double);
    double value = va_arg(arguments, double);
    va_end(arguments);
    return value;
}

extern "C" double verified_sysv_va_double_ninth(int marker, ...)
{
    va_list arguments;
    va_start(arguments, marker);
    for (int index = 0; index < 8; ++index) {
        va_arg(arguments, double);
    }
    double value = va_arg(arguments, double);
    va_end(arguments);
    return value;
}

extern "C" int verified_sysv_va_int_first(int marker, ...)
{
    va_list arguments;
    va_start(arguments, marker);
    int value = va_arg(arguments, int);
    va_end(arguments);
    return value;
}

extern "C" double verified_sysv_va_named_double(double named, ...)
{
    va_list arguments;
    va_start(arguments, named);
    double value = va_arg(arguments, double);
    va_end(arguments);
    return value;
}

extern "C" double verified_sysv_va_mixed_overflow(
    int i0, int i1, int i2, int i3, int i4, int i5,
    double d0, double d1, double d2, double d3,
    double d4, double d5, double d6, double d7, ...)
{
    va_list arguments;
    va_start(arguments, d7);
    int ignored = va_arg(arguments, int);
    double value = va_arg(arguments, double);
    va_end(arguments);
    return value;
}

extern "C" double verified_sysv_named_double_first(double value)
{
    return value;
}

extern "C" double verified_sysv_named_double_after_int(
    int marker, double value)
{
    return value;
}

extern "C" float verified_sysv_named_float_first(float value)
{
    return value;
}

extern "C" double verified_sysv_named_double_ninth(
    double first, double second, double third, double fourth,
    double fifth, double sixth, double seventh, double eighth,
    double ninth)
{
    return ninth;
}

extern "C" double verified_sysv_named_mixed_stack(
    int i0, int i1, int i2, int i3, int i4, int i5, int i6,
    double d0, double d1, double d2, double d3, double d4,
    double d5, double d6, double d7, double d8)
{
    return d8;
}

extern "C" double verified_sysv_fp_call_target(double value, int marker)
{
    return value;
}

extern "C" double verified_sysv_fp_call_double(double value)
{
    return verified_sysv_fp_call_target(value, 17);
}

extern "C" double verified_sysv_fp_call_mixed(int marker, double value)
{
    return verified_sysv_fp_call_target(value, marker);
}

extern "C" double verified_sysv_fp_call_ninth(
    double first, double second, double third, double fourth,
    double fifth, double sixth, double seventh, double eighth,
    double ninth)
{
    return verified_sysv_named_double_ninth(
        first, second, third, fourth, fifth, sixth, seventh, eighth,
        ninth);
}

extern "C" double verified_sysv_fp_call_mixed_stack(
    int i0, int i1, int i2, int i3, int i4, int i5, int i6,
    double d0, double d1, double d2, double d3, double d4,
    double d5, double d6, double d7, double d8)
{
    return verified_sysv_named_mixed_stack(
        i0, i1, i2, i3, i4, i5, i6,
        d0, d1, d2, d3, d4, d5, d6, d7, d8);
}

extern "C" double verified_sysv_fp_call_variadic_double(double value)
{
    return verified_sysv_va_double_first(31, value);
}

extern "C" double verified_sysv_fp_call_variadic_float(float value)
{
    return verified_sysv_va_double_first(32, value);
}

extern "C" double verified_sysv_fp_call_variadic_ninth_float(
    float f0, float f1, float f2, float f3, float f4,
    float f5, float f6, float f7, float f8, float f9,
    float f10, float f11, float f12, float f13, float f14,
    float f15, float f16)
{
    return verified_sysv_va_double_ninth(
        33, f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11,
        f12, f13, f14, f15, f16);
}

extern "C" double verified_sysv_fp_call_variadic_named_double(
    double named, double value)
{
    return verified_sysv_va_named_double(named, value);
}

extern "C" double verified_sysv_fp_call_mixed_variadic_overflow(
    int i0, int i1, int i2, int i3, int i4, int i5,
    double d0, double d1, double d2, double d3,
    double d4, double d5, double d6, double d7,
    int extra_integer, double extra_double)
{
    return verified_sysv_va_mixed_overflow(
        i0, i1, i2, i3, i4, i5,
        d0, d1, d2, d3, d4, d5, d6, d7,
        extra_integer, extra_double);
}

extern "C" int verified_sysv_fp_call_variadic_int(int value)
{
    return verified_sysv_va_int_first(35, value);
}

extern "C" double verified_sysv_fp_call_variadic_ninth(
    double d0, double d1, double d2, double d3, double d4,
    double d5, double d6, double d7, double d8)
{
    return verified_sysv_va_double_ninth(
        41, d0, d1, d2, d3, d4, d5, d6, d7, d8);
}
