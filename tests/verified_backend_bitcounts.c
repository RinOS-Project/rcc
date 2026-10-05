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

int verified_builtin_ffs32(unsigned int value)
{
    return __builtin_ffs((int)value);
}

int verified_builtin_clrsb32(int value)
{
    return __builtin_clrsb(value);
}

int verified_builtin_clrsbl(long value)
{
    return __builtin_clrsbl(value);
}

int verified_builtin_prefetch_read(int* value)
{
    __builtin_prefetch(value, 0, 3);
    return *value;
}

int verified_builtin_prefetch_write(int* value)
{
    __builtin_prefetch(value, 1, 0);
    return *value;
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

int verified_builtin_ffs64(unsigned long long value)
{
    return __builtin_ffsll(value);
}

int verified_builtin_clrsbll(long long value)
{
    return __builtin_clrsbll(value);
}
#endif
