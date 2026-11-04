static inline unsigned long long
debug_nonconstant_unsigned_long_long_parameter_inline(
    unsigned long long value)
{
    return value;
}

_Atomic unsigned long long debug_inline_nonconstant_unsigned_long_long_atomic_dividend;
volatile _Atomic unsigned long long debug_inline_nonconstant_unsigned_long_long_volatile_atomic_dividend;

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

unsigned long long debug_inline_nonconstant_unsigned_long_long_atomic_store_local_divide_entry(
    unsigned long long computed_atomic_store_division_dividend,
    unsigned long long computed_atomic_store_division_divisor)
{
    _Atomic unsigned long long local_atomic_dividend;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (local_atomic_dividend = computed_atomic_store_division_dividend) /
        (computed_atomic_store_division_divisor * 2ull + 1ull));
}

unsigned long long debug_inline_nonconstant_unsigned_long_long_atomic_store_local_remainder_entry(
    unsigned long long computed_atomic_store_division_dividend,
    unsigned long long computed_atomic_store_division_divisor)
{
    _Atomic unsigned long long local_atomic_dividend;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (local_atomic_dividend = computed_atomic_store_division_dividend) %
        (computed_atomic_store_division_divisor * 2ull + 1ull));
}

unsigned long long debug_inline_nonconstant_unsigned_long_long_atomic_store_pointer_divide_entry(
    _Atomic unsigned long long *computed_atomic_store_division_destination,
    unsigned long long computed_atomic_store_pointer_division_dividend,
    unsigned long long computed_atomic_store_pointer_division_divisor)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        ((*computed_atomic_store_division_destination =
              computed_atomic_store_pointer_division_dividend) /
         (computed_atomic_store_pointer_division_divisor * 2ull + 1ull)));
}

unsigned long long debug_inline_nonconstant_unsigned_long_long_atomic_store_pointer_remainder_entry(
    _Atomic unsigned long long *computed_atomic_store_division_destination,
    unsigned long long computed_atomic_store_pointer_division_dividend,
    unsigned long long computed_atomic_store_pointer_division_divisor)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        ((*computed_atomic_store_division_destination =
              computed_atomic_store_pointer_division_dividend) %
         (computed_atomic_store_pointer_division_divisor * 2ull + 1ull)));
}

unsigned long long debug_inline_nonconstant_unsigned_long_long_atomic_store_volatile_divide_entry(
    unsigned long long computed_atomic_store_division_dividend,
    unsigned long long computed_atomic_store_division_divisor)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_nonconstant_unsigned_long_long_volatile_atomic_dividend =
             computed_atomic_store_division_dividend) /
        (computed_atomic_store_division_divisor * 2ull + 1ull));
}

unsigned long long debug_inline_nonconstant_unsigned_long_long_atomic_store_volatile_remainder_entry(
    unsigned long long computed_atomic_store_division_dividend,
    unsigned long long computed_atomic_store_division_divisor)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_nonconstant_unsigned_long_long_volatile_atomic_dividend =
             computed_atomic_store_division_dividend) %
        (computed_atomic_store_division_divisor * 2ull + 1ull));
}
