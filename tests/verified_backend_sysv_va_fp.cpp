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
