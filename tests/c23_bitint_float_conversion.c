typedef signed _BitInt(5) s5;
typedef signed _BitInt(9) s9;
typedef signed _BitInt(33) s33;
typedef unsigned _BitInt(33) u33;
typedef unsigned _BitInt(64) u64b;

static u64b from_float_to_unsigned64(float value)
{
    return (u64b)value;
}

static u64b from_double_to_unsigned64(double value)
{
    return (u64b)value;
}

static unsigned long long from_double_to_unsigned_long_long(double value)
{
    return (unsigned long long)value;
}

int main(void)
{
    s9 signed_narrow = -127;
    s33 signed_wide = -4294967296LL;
    u33 unsigned_wide = ((u33)1 << 32) | (u33)17;
    u64b unsigned_full = (u64b)18446744073709551615ULL;
    double signed_narrow_to_double = (double)signed_narrow;
    double signed_wide_to_double = (double)signed_wide;
    double unsigned_wide_to_double = (double)unsigned_wide;
    float unsigned_full_to_float = (float)unsigned_full;
    double unsigned_full_to_double = unsigned_full + 0.0;
    s5 double_to_signed_narrow = (s5)-15.75;
    u33 double_to_unsigned_wide = (u33)4294967295.75;

    if (signed_narrow_to_double != -127.0) {
        if ((int)signed_narrow_to_double == -127) return 11;
        if ((int)signed_narrow_to_double == 0) return 12;
        return 13;
    }
    if (signed_wide_to_double != -4294967296.0) return 2;
    if (unsigned_wide_to_double != 4294967313.0) return 3;
    if (unsigned_full_to_float != 18446744073709551616.0f) return 4;
    if (unsigned_full_to_double != 18446744073709551616.0) return 5;
    if (double_to_signed_narrow != -15) return 6;
    if (double_to_unsigned_wide != 4294967295u) return 7;
    if (from_double_to_unsigned64(18446744073709549568.0) !=
            (u64b)18446744073709549568ULL) return 8;
    if (from_double_to_unsigned64(9223372036854774784.0) !=
            (u64b)9223372036854774784ULL) return 11;
    if (from_double_to_unsigned64(9223372036854775808.0) !=
            (u64b)9223372036854775808ULL) return 12;
    if (from_float_to_unsigned64(18446742974197923840.0f) !=
            (u64b)18446742974197923840ULL) return 9;
    if (from_float_to_unsigned64(9223372036854775808.0f) !=
            (u64b)9223372036854775808ULL) return 13;
    if (from_double_to_unsigned_long_long(18446744073709549568.0) !=
            18446744073709549568ULL) return 10;
    return 0;
}
