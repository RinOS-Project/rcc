unsigned long long verified_wide_scalar_constant_return(void)
{
    return ((unsigned long long)0x11223344ULL << 32) |
        0x55667788ULL;
}

unsigned long long verified_wide_scalar_parameter(
    unsigned long long value)
{
    return value;
}

unsigned long long verified_wide_scalar_add(unsigned long long value)
{
    return value + 0x0102030405060708ULL;
}

unsigned long long verified_wide_scalar_carry(unsigned long long value)
{
    return value + 0xffffffffULL;
}

unsigned long long verified_wide_scalar_subtract(unsigned long long value)
{
    return value - 1ULL;
}

unsigned long long verified_wide_scalar_local(unsigned long long value)
{
    unsigned long long copy = value;
    return copy;
}

unsigned long long verified_wide_scalar_narrow(unsigned int value)
{
    return (unsigned long long)value;
}
