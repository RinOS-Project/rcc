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

int main(void)
{
    if (verified_builtin_bswap16(0x1234u) != 0x3412u) return 1;
    if (verified_builtin_bswap32(0x12345678u) != 0x78563412u) return 2;
    return verified_builtin_bswap64(0x0102030405060708ULL) ==
        0x0807060504030201ULL ? 0 : 3;
}
