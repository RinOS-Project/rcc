int verified_builtin_clz32(unsigned int value)
{
    return __builtin_clz(value);
}

int verified_builtin_ctz32(unsigned int value)
{
    return __builtin_ctz(value);
}

int verified_builtin_popcount32(unsigned int value)
{
    return __builtin_popcount(value);
}

#if defined(__x86_64__)
int verified_builtin_clz64(unsigned long long value)
{
    return __builtin_clzll(value);
}

int verified_builtin_ctz64(unsigned long long value)
{
    return __builtin_ctzll(value);
}

int verified_builtin_popcount64(unsigned long long value)
{
    return __builtin_popcountll(value);
}
#endif
