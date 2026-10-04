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

int verified_wide_scalar_equal(unsigned long long value)
{
    return value == 0x1122334455667788ULL;
}

int verified_wide_scalar_not_equal(unsigned long long value)
{
    return value != 0x1122334455667788ULL;
}

int verified_wide_scalar_unsigned_less(unsigned long long value)
{
    return value < 0x0000000100000000ULL;
}

int verified_wide_scalar_signed_less(long long value)
{
    return value < 0;
}

int verified_wide_scalar_unsigned_le(unsigned long long value)
{
    return value <= 0x0000000100000000ULL;
}

int verified_wide_scalar_unsigned_ge(unsigned long long value)
{
    return value >= 0x0000000100000000ULL;
}

int verified_wide_scalar_signed_le(long long value)
{
    return value <= 0;
}

int verified_wide_scalar_signed_ge(long long value)
{
    return value >= 0;
}

unsigned long long verified_wide_scalar_lshift(
    unsigned long long value, unsigned int count)
{
    return value << count;
}

unsigned long long verified_wide_scalar_lshr(
    unsigned long long value, unsigned int count)
{
    return value >> count;
}

long long verified_wide_scalar_ashr(long long value, unsigned int count)
{
    return value >> count;
}

unsigned long long verified_wide_scalar_conditional(int condition)
{
    return condition ? 0x1122334455667788ULL : 0x8877665544332211ULL;
}

unsigned long long verified_wide_scalar_truth_conditional(
    unsigned long long value)
{
    return value ? 0x1122334455667788ULL : 0x8877665544332211ULL;
}

unsigned long long verified_wide_scalar_mul(unsigned long long value)
{
    return value * 0x0000000100000001ULL;
}

unsigned long long verified_wide_scalar_identity(unsigned long long value)
{
    return value;
}

unsigned long long verified_wide_scalar_call(unsigned long long value)
{
    return verified_wide_scalar_identity(value);
}

unsigned long long verified_wide_scalar_udiv(unsigned long long value)
{
    return value / 0x0000000100000001ULL;
}

unsigned long long verified_wide_scalar_udiv_small(unsigned long long value)
{
    return value / 3ULL;
}

unsigned long long verified_wide_scalar_umod(unsigned long long value)
{
    return value % 0x0000000100000001ULL;
}

long long verified_wide_scalar_sdiv(long long value)
{
    return value / 3LL;
}

long long verified_wide_scalar_smod(long long value)
{
    return value % 3LL;
}
