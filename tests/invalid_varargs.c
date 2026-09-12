#include <stdarg.h>

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

int invalid_void(int marker, ...)
{
    va_list arguments;
    va_start(arguments, marker);
    va_arg(arguments, void);
    va_end(arguments);
    return marker;
}
