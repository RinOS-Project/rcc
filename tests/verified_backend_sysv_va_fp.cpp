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
