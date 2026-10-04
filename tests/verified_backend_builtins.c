int verified_builtin_expect(int value)
{
    return __builtin_expect(value, 1);
}

int verified_builtin_unreachable(int value)
{
    if (value != 0) return 17;
    __builtin_unreachable();
}

int verified_builtin_trap(int value)
{
    if (value != 0) return 29;
    __builtin_trap();
}

unsigned short verified_builtin_bswap16(unsigned short value)
{
    return __builtin_bswap16(value);
}

unsigned int verified_builtin_bswap32(unsigned int value)
{
    return __builtin_bswap32(value);
}

int verified_builtin_parity(unsigned int value)
{
    return __builtin_parity(value);
}

int verified_builtin_parityl(unsigned long value)
{
    return __builtin_parityl(value);
}

int main(void)
{
    if (verified_builtin_expect(23) != 23) return 1;
    if (verified_builtin_unreachable(1) != 17) return 2;
    if (verified_builtin_trap(1) != 29) return 3;
    if (verified_builtin_bswap16(0x1234u) != 0x3412u) return 4;
    if (verified_builtin_bswap32(0x12345678u) != 0x78563412u) return 5;
    if (verified_builtin_parity(0x80000003u) != 1) return 6;
    if (verified_builtin_parityl(0x80000003UL) != 1) return 7;
    return 0;
}
