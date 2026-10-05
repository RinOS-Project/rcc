unsigned short verified_builtin_bswap16(unsigned short value)
{
    return __builtin_bswap16(value);
}

unsigned int verified_builtin_bswap32(unsigned int value)
{
    return __builtin_bswap32(value);
}

unsigned long long verified_builtin_bswap64(unsigned long long value)
{
    return __builtin_bswap64(value);
}

void* verified_builtin_assume_aligned(void* value)
{
    return __builtin_assume_aligned(value, 16);
}

void* verified_builtin_assume_aligned_offset(void* value)
{
    return __builtin_assume_aligned(value, 16, 4);
}

int verified_builtin_constant_true(void)
{
    return __builtin_constant_p(40 + 2);
}

int verified_builtin_constant_false(unsigned int value)
{
    return __builtin_constant_p(value);
}

int main(void)
{
    if (verified_builtin_bswap16(0x1234u) != 0x3412u) return 1;
    if (verified_builtin_bswap32(0x12345678u) != 0x78563412u) return 2;
    return verified_builtin_bswap64(0x0102030405060708ULL) ==
        0x0807060504030201ULL ? 0 : 3;
}
