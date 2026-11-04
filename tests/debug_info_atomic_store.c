static inline unsigned long long
debug_nonconstant_unsigned_long_long_parameter_inline(
    unsigned long long value)
{
    return value;
}

_Atomic unsigned long long debug_inline_nonconstant_unsigned_long_long_atomic_dividend;

unsigned long long debug_inline_nonconstant_unsigned_long_long_atomic_store_divide_entry(
    unsigned long long computed_atomic_store_division_dividend,
    unsigned long long computed_atomic_store_division_divisor)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_nonconstant_unsigned_long_long_atomic_dividend =
             computed_atomic_store_division_dividend) /
        (computed_atomic_store_division_divisor * 2ull + 1ull));
}

unsigned long long debug_inline_nonconstant_unsigned_long_long_atomic_store_remainder_entry(
    unsigned long long computed_atomic_store_division_dividend,
    unsigned long long computed_atomic_store_division_divisor)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_nonconstant_unsigned_long_long_atomic_dividend =
             computed_atomic_store_division_dividend) %
        (computed_atomic_store_division_divisor * 2ull + 1ull));
}

unsigned long long debug_inline_nonconstant_unsigned_long_long_atomic_store_computed_divide_entry(
    unsigned long long computed_atomic_store_division_dividend,
    unsigned long long computed_atomic_store_division_divisor)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_nonconstant_unsigned_long_long_atomic_dividend =
             computed_atomic_store_division_dividend +
             0x123456789abcdef0ull) /
        (computed_atomic_store_division_divisor * 2ull + 1ull));
}

unsigned long long debug_inline_nonconstant_unsigned_long_long_atomic_store_computed_remainder_entry(
    unsigned long long computed_atomic_store_division_dividend,
    unsigned long long computed_atomic_store_division_divisor)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_nonconstant_unsigned_long_long_atomic_dividend =
             computed_atomic_store_division_dividend +
             0x123456789abcdef0ull) %
        (computed_atomic_store_division_divisor * 2ull + 1ull));
}
