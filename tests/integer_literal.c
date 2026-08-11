_Static_assert(_Generic(1, int: 1, default: 0),
               "unsuffixed small decimal must be int");
_Static_assert(_Generic(1U, unsigned int: 1, default: 0),
               "U suffix must select an unsigned type");
_Static_assert(_Generic(1L, long: 1, default: 0),
               "L suffix must start with long");
_Static_assert(_Generic(1UL, unsigned long: 1, default: 0),
               "UL suffix must select unsigned long");
_Static_assert(_Generic(1LL, long long: 1, default: 0),
               "LL suffix must start with long long");
_Static_assert(_Generic(1uLL, unsigned long long: 1, default: 0),
               "mixed-order ULL suffix must select unsigned long long");
_Static_assert(_Generic(0x80000000, unsigned int: 1, default: 0),
               "hex candidate list must include unsigned int");
_Static_assert(0xffffffffffffffffULL > 0,
               "unsigned constant comparison must not become signed");
_Static_assert(0xffffffffU + 1U == 0,
               "unsigned constant arithmetic must wrap at its type width");

#if defined(__x86_64__)
_Static_assert(_Generic(2147483648, long: 1, default: 0),
               "LP64 decimal boundary must select long");
_Static_assert(_Generic(0xffffffffffffffff, unsigned long: 1, default: 0),
               "LP64 maximum hex value must select unsigned long");
#else
_Static_assert(_Generic(2147483648, long long: 1, default: 0),
               "ILP32 decimal boundary must select long long");
_Static_assert(_Generic(0xffffffffffffffff, unsigned long long: 1, default: 0),
               "ILP32 maximum hex value must select unsigned long long");
#endif

unsigned long long literal_maximum_ull(void)
{
    return 18446744073709551615ULL;
}

unsigned long long literal_large_hex(void)
{
    return 0xfedcba9876543210;
}

unsigned long long literal_decimal_boundary(void)
{
    return 2147483648;
}

unsigned long long literal_unsigned_int(void)
{
    return 0xffffffffU;
}

unsigned long long literal_mixed_suffix(void)
{
    return 0x89abcdef01234567uLL;
}
