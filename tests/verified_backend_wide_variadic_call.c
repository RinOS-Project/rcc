extern unsigned long long verified_wide_scalar_variadic_target(
    int increment, ...);
extern unsigned long long verified_wide_scalar_variadic_pointer_target(
    int increment, ...);

unsigned long long verified_wide_scalar_variadic_call(
    unsigned long long value)
{
    return verified_wide_scalar_variadic_target(5, (unsigned char)2, value);
}

unsigned long long verified_wide_scalar_variadic_pointer_call(
    unsigned long long value, const void* pointer)
{
    value += verified_wide_scalar_variadic_pointer_target(5, pointer);
    return value;
}
