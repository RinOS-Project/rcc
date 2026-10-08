#include <stdarg.h>

static int verified_cpp_va_list_pointer_parameter(va_list* source)
{
    va_list copy;
    int copied_first;
    int source_first;
    va_copy(copy, *source);
    copied_first = va_arg(copy, int);
    source_first = va_arg(*source, int);
    va_end(copy);
    return copied_first + source_first;
}

extern "C" int verified_cpp_va_list_pointer_forward_target(int marker, ...)
{
    va_list arguments;
    int result;
    va_start(arguments, marker);
    result = verified_cpp_va_list_pointer_parameter(&arguments);
    result += va_arg(arguments, int);
    va_end(arguments);
    return result;
}

extern "C" int verified_cpp_va_list_pointer_forward_call(void)
{
    return verified_cpp_va_list_pointer_forward_target(1, 10, 20, 30);
}
