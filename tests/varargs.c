#include <stdarg.h>

int sum_values(int count, ...)
{
    va_list arguments;
    int total = 0;
    int index;
    va_start(arguments, count);
    for (index = 0; index < count; ++index) {
        total += va_arg(arguments, int);
    }
    va_end(arguments);
    return total;
}

long long select_wide_value(int ignored, ...)
{
    va_list arguments;
    long long value;
    (void)ignored;
    va_start(arguments, ignored);
    value = va_arg(arguments, long long);
    va_end(arguments);
    return value;
}

double sum_floating(int count, ...)
{
    va_list arguments;
    double total = 0.0;
    int index;
    va_start(arguments, count);
    for (index = 0; index < count; ++index) {
        total += va_arg(arguments, double);
    }
    va_end(arguments);
    return total;
}

double named_floating(double first, ...)
{
    va_list arguments;
    double second;
    double third;
    va_start(arguments, first);
    second = va_arg(arguments, double);
    third = va_arg(arguments, double);
    va_end(arguments);
    return first + second + third;
}

double stack_floating(double first, ...)
{
    va_list arguments;
    double total = first;
    int index;
    va_start(arguments, first);
    for (index = 0; index < 9; ++index) {
        total += va_arg(arguments, double);
    }
    va_end(arguments);
    return total;
}

int copy_values(int ignored, ...)
{
    va_list arguments;
    va_list copied;
    int first;
    int copied_first;
    int copied_second;
    (void)ignored;
    va_start(arguments, ignored);
    va_copy(copied, arguments);
    first = va_arg(arguments, int);
    copied_first = va_arg(copied, int);
    copied_second = va_arg(copied, int);
    va_end(copied);
    va_end(arguments);
    return first * 100 + copied_first * 10 + copied_second;
}

int pointer_value(int ignored, ...)
{
    va_list arguments;
    int* value;
    (void)ignored;
    va_start(arguments, ignored);
    value = va_arg(arguments, int*);
    va_end(arguments);
    return *value;
}

int generated_register_varargs(void)
{
    return sum_values(5, 1, 2, 3, 4, 5);
}

int generated_stack_varargs(void)
{
    return sum_values(8, 1, 2, 3, 4, 5, 6, 7, 8);
}

long long generated_wide_varargs(void)
{
    return select_wide_value(0, 0x1122334455667788LL);
}

double generated_floating_varargs(void)
{
    return sum_floating(3, 1.25, 2.5, 3.75);
}

double generated_named_floating(void)
{
    return named_floating(1.5, 2.25, 3.25);
}

double generated_stack_floating(void)
{
    return stack_floating(0.0, 1.0, 2.0, 3.0, 4.0, 5.0,
                          6.0, 7.0, 8.0, 9.0);
}

int generated_copy_varargs(void)
{
    return copy_values(0, 4, 7);
}

int generated_pointer_varargs(void)
{
    int value = 73;
    return pointer_value(0, &value);
}
