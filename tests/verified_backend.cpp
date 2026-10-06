extern "C" int verified_cxx(int value)
{
    return value + 2;
}

extern "C" unsigned long long verified_cxx_wide_scalar_conditional_assign(
    int condition, unsigned long long value)
{
    unsigned long long local = 0ULL;
    local = (condition && value != 0ULL) ? value + 1ULL : 7ULL;
    return local;
}

extern "C" unsigned long long verified_cxx_wide_scalar_conditional_compound(
    int condition, unsigned long long value)
{
    unsigned long long local = value;
    local += condition ? 1ULL : 2ULL;
    return local;
}

extern "C" unsigned long long verified_cxx_wide_scalar_pure_comma_compound(
    int condition, unsigned long long value)
{
    unsigned long long local = value;
    local += (value, condition ? 1ULL : 2ULL);
    return local;
}
