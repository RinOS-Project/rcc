int verified_builtin_expect(int value)
{
    return __builtin_expect(value, 1);
}

int verified_builtin_choose(void)
{
    int value = 3;
    int selected = __builtin_choose_expr(1, value + 4, value += 100);
    int other = __builtin_choose_expr(0, value += 100, value + 2);
    if (selected != 7 || other != 5 || value != 3) return 1;
    return __builtin_choose_expr(1, 31, 47);
}

typedef unsigned int verified_unsigned_alias;

int verified_builtin_types_compatible(void)
{
    if (!__builtin_types_compatible_p(int, int)) return 1;
    if (!__builtin_types_compatible_p(verified_unsigned_alias, unsigned int)) return 2;
    if (!__builtin_types_compatible_p(int *, int *)) return 3;
    if (__builtin_types_compatible_p(int, unsigned int)) return 4;
    if (__builtin_types_compatible_p(int *, const int *)) return 5;
    return 0;
}

int verified_builtin_expect_probability(int value)
{
    return __builtin_expect_with_probability(value, 1, 0.75);
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

int verified_builtin_ffs(int value)
{
    return __builtin_ffs(value);
}

int verified_builtin_ffsl(unsigned long value)
{
    return __builtin_ffsl(value);
}

int verified_builtin_strlen(const char *value)
{
    return (int)__builtin_strlen(value);
}

#if defined(__x86_64__)
int verified_builtin_ffsll(unsigned long long value)
{
    return __builtin_ffsll(value);
}
#endif

int main(void)
{
    if (verified_builtin_expect(23) != 23) return 1;
    if (verified_builtin_choose() != 31) return 2;
    if (verified_builtin_types_compatible() != 0) return 3;
    if (verified_builtin_expect_probability(23) != 23) return 4;
    if (verified_builtin_unreachable(1) != 17) return 5;
    if (verified_builtin_trap(1) != 29) return 6;
    if (verified_builtin_bswap16(0x1234u) != 0x3412u) return 7;
    if (verified_builtin_bswap32(0x12345678u) != 0x78563412u) return 8;
    if (verified_builtin_parity(0x80000003u) != 1) return 9;
    if (verified_builtin_parityl(0x80000003UL) != 1) return 10;
    if (verified_builtin_ffs(0) != 0) return 11;
    if (verified_builtin_ffs(0x100) != 9) return 12;
    if (verified_builtin_ffsl(0x100UL) != 9) return 13;
#if defined(__x86_64__)
    if (verified_builtin_ffsll(1ULL << 40) != 41) return 14;
#endif
    {
        const char value[] = "RinOS";
        if (verified_builtin_strlen(value + 1) != 4) return 15;
    }
    return 0;
}
