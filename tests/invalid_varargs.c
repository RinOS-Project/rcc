#include <stdarg.h>

struct Pair {
    int first;
    int second;
};

int invalid_start(int value)
{
    va_list arguments;
    va_start(arguments, value);
    va_end(arguments);
    return value;
}

int invalid_start_parameter(int first, int last, ...)
{
    va_list arguments;
    (void)last;
    va_start(arguments, first);
    va_end(arguments);
    return first;
}

int invalid_list(int marker, ...)
{
    va_list arguments;
    int value = 0;
    va_start(arguments, marker);
    va_copy(arguments, value);
    va_end(value);
    return va_arg(value, int);
}

int invalid_float(int marker, ...)
{
    va_list arguments;
    double value;
    va_start(arguments, marker);
    value = va_arg(arguments, double);
    va_end(arguments);
    return (int)value;
}

int invalid_aggregate(int marker, ...)
{
    va_list arguments;
    struct Pair value;
    va_start(arguments, marker);
    value = va_arg(arguments, struct Pair);
    va_end(arguments);
    return value.first;
}
