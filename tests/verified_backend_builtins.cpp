extern "C" int verified_cxx_builtin_expect(int value)
{
    return __builtin_expect(value, 1);
}

extern "C" int verified_cxx_builtin_unreachable(int value)
{
    if (value != 0) return 19;
    __builtin_unreachable();
}

extern "C" int verified_cxx_builtin_trap(int value)
{
    if (value != 0) return 23;
    __builtin_trap();
}

extern "C" int verified_cxx_builtin_clz(unsigned int value)
{
    return __builtin_clz(value);
}

extern "C" int verified_cxx_builtin_ctz(unsigned int value)
{
    return __builtin_ctz(value);
}

extern "C" int verified_cxx_builtin_popcount(unsigned int value)
{
    return __builtin_popcount(value);
}

extern "C" int verified_cxx_builtin_clzll(unsigned long long value)
{
    return __builtin_clzll(value);
}

extern "C" int verified_cxx_builtin_ctzll(unsigned long long value)
{
    return __builtin_ctzll(value);
}

extern "C" int verified_cxx_builtin_popcountll(unsigned long long value)
{
    return __builtin_popcountll(value);
}

extern "C" int verified_cxx_builtin_parity(unsigned int value)
{
    return __builtin_parity(value);
}

extern "C" int verified_cxx_builtin_parityl(unsigned long value)
{
    return __builtin_parityl(value);
}

extern "C" int verified_cxx_builtin_ffs(int value)
{
    return __builtin_ffs(value);
}

extern "C" int verified_cxx_builtin_ffsl(unsigned long value)
{
    return __builtin_ffsl(value);
}

extern "C" int verified_cxx_builtin_ffsll(unsigned long long value)
{
    return __builtin_ffsll(value);
}

extern "C" int verified_cxx_builtin_prefetch(int* value)
{
    __builtin_prefetch(value, 1, 0);
    return *value;
}

extern "C" int main(void)
{
    if (verified_cxx_builtin_expect(31) != 31) return 1;
    if (verified_cxx_builtin_unreachable(1) != 19) return 2;
    if (verified_cxx_builtin_trap(1) != 23) return 3;
    if (verified_cxx_builtin_clz(0x00100000u) != 11) return 4;
    if (verified_cxx_builtin_ctz(0x00001000u) != 12) return 5;
    if (verified_cxx_builtin_popcount(0xf0f00f0fu) != 16) return 6;
    if (verified_cxx_builtin_clzll(1ULL) != 63) return 7;
    if (verified_cxx_builtin_ctzll(1ULL << 40) != 40) return 8;
    if (verified_cxx_builtin_popcountll(0xf00000000000000FULL) != 8) return 9;
    if (verified_cxx_builtin_parity(0x80000003u) != 1) return 10;
    if (verified_cxx_builtin_parityl(0x80000003UL) != 1) return 11;
    if (verified_cxx_builtin_ffs(0) != 0) return 12;
    if (verified_cxx_builtin_ffs(0x100) != 9) return 13;
    if (verified_cxx_builtin_ffsl(0x100UL) != 9) return 14;
    if (verified_cxx_builtin_ffsll(1ULL << 40) != 41) return 15;
    {
        int value = 37;
        if (verified_cxx_builtin_prefetch(&value) != 37) return 16;
    }
    return 0;
}
