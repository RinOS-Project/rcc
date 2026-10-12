#include <stdarg.h>

double verified_i686_read_variadic_double(int ignored, ...)
{
    va_list arguments;
    double first;
    double second;
    (void)ignored;
    va_start(arguments, ignored);
    first = va_arg(arguments, double);
    second = va_arg(arguments, double);
    va_end(arguments);
    return first + second;
}

int main(void)
{
    int first_integer = 1;
    int second_integer = -2;
    float first = (float)first_integer;
    float second = (float)second_integer;
    double value = verified_i686_read_variadic_double(0, first, second);
    int integer_value = (int)value;
    return integer_value >= 0;
}
