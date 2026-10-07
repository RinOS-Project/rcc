typedef unsigned long long GoldenU64;
typedef signed long long GoldenI64;

GoldenU64 golden_wide_scalar_mix(GoldenU64 value, unsigned int count)
{
    GoldenU64 accumulator = value ^ 0x1122334455667788ULL;
    while (count != 0u) {
        accumulator += value;
        accumulator = (accumulator << 7u) | (accumulator >> 57u);
        --count;
    }
    return (accumulator * 3ULL) ^ (accumulator - value);
}

GoldenI64 golden_wide_scalar_signed_branch(GoldenI64 value)
{
    return value < -17LL ? value + 17LL : value - 17LL;
}
