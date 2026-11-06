static inline long long
debug_nonconstant_signed_long_long_parameter_inline(long long value)
{
    return value;
}

long long debug_inline_nonconstant_signed_long_long_atomic_local_rmw_add_divide_entry(
    long long computed_atomic_rmw_initial,
    long long computed_atomic_rmw_delta,
    long long computed_atomic_rmw_divisor)
{
    volatile _Atomic long long debug_inline_nonconstant_signed_long_long_atomic_local_rmw_add_dividend =
        computed_atomic_rmw_initial;
    return debug_nonconstant_signed_long_long_parameter_inline(
        ((debug_inline_nonconstant_signed_long_long_atomic_local_rmw_add_dividend +=
              computed_atomic_rmw_delta) ^ 0x100000001ll) /
        computed_atomic_rmw_divisor);
}

long long debug_inline_nonconstant_signed_long_long_atomic_local_rmw_sub_remainder_entry(
    long long computed_atomic_rmw_initial,
    long long computed_atomic_rmw_delta,
    long long computed_atomic_rmw_divisor)
{
    _Atomic long long debug_inline_nonconstant_signed_long_long_atomic_local_rmw_sub_dividend =
        computed_atomic_rmw_initial;
    return debug_nonconstant_signed_long_long_parameter_inline(
        ((debug_inline_nonconstant_signed_long_long_atomic_local_rmw_sub_dividend -=
              computed_atomic_rmw_delta) ^ 0x100000001ll) %
        computed_atomic_rmw_divisor);
}
