static inline unsigned long long
debug_nonconstant_unsigned_long_long_parameter_inline(
    unsigned long long value)
{
    return value;
}

_Atomic unsigned long long debug_inline_nonconstant_unsigned_long_long_atomic_dividend;
volatile _Atomic unsigned long long debug_inline_nonconstant_unsigned_long_long_volatile_atomic_dividend;
_Thread_local _Atomic unsigned long long debug_inline_nonconstant_unsigned_long_long_tls_atomic_dividend;
_Thread_local volatile _Atomic unsigned long long debug_inline_nonconstant_unsigned_long_long_volatile_tls_atomic_dividend;

_Atomic unsigned long long
    debug_inline_nonconstant_unsigned_long_long_atomic_load_dividend =
        0xfedcba9876543217ULL;
_Atomic unsigned long long
    debug_inline_nonconstant_unsigned_long_long_atomic_load_divisor =
        0x8000000000000001ULL;
_Thread_local _Atomic unsigned long long
    debug_inline_nonconstant_unsigned_long_long_tls_atomic_load_dividend =
        0xfedcba9876543217ULL;
_Thread_local _Atomic unsigned long long
    debug_inline_nonconstant_unsigned_long_long_tls_atomic_load_divisor =
        0x8000000000000001ULL;
volatile _Atomic unsigned long long
    debug_inline_nonconstant_unsigned_long_long_volatile_atomic_load_dividend =
        0xfedcba9876543217ULL;
volatile _Atomic unsigned long long
    debug_inline_nonconstant_unsigned_long_long_volatile_atomic_load_divisor =
        0x8000000000000001ULL;
_Thread_local volatile _Atomic unsigned long long
    debug_inline_nonconstant_unsigned_long_long_volatile_tls_atomic_load_dividend =
        0xfedcba9876543217ULL;
_Thread_local volatile _Atomic unsigned long long
    debug_inline_nonconstant_unsigned_long_long_volatile_tls_atomic_load_divisor =
        0x8000000000000001ULL;

unsigned long long
debug_inline_nonconstant_unsigned_long_long_atomic_load_xor_divide_entry(void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_nonconstant_unsigned_long_long_atomic_load_dividend ^
         0x100000001ULL) /
        debug_inline_nonconstant_unsigned_long_long_atomic_load_divisor);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_atomic_load_xor_remainder_entry(
    void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_nonconstant_unsigned_long_long_atomic_load_dividend ^
         0x100000001ULL) %
        debug_inline_nonconstant_unsigned_long_long_atomic_load_divisor);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_atomic_load_xor_divide_entry(
    void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_nonconstant_unsigned_long_long_tls_atomic_load_dividend ^
         0x100000001ULL) /
        debug_inline_nonconstant_unsigned_long_long_tls_atomic_load_divisor);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_tls_atomic_load_xor_remainder_entry(
    void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_nonconstant_unsigned_long_long_tls_atomic_load_dividend ^
         0x100000001ULL) %
        debug_inline_nonconstant_unsigned_long_long_tls_atomic_load_divisor);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_volatile_atomic_load_xor_divide_entry(
    void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_nonconstant_unsigned_long_long_volatile_atomic_load_dividend ^
         0x100000001ULL) /
        debug_inline_nonconstant_unsigned_long_long_volatile_atomic_load_divisor);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_volatile_atomic_load_xor_remainder_entry(
    void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_nonconstant_unsigned_long_long_volatile_atomic_load_dividend ^
         0x100000001ULL) %
        debug_inline_nonconstant_unsigned_long_long_volatile_atomic_load_divisor);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_volatile_tls_atomic_load_xor_divide_entry(
    void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_nonconstant_unsigned_long_long_volatile_tls_atomic_load_dividend ^
         0x100000001ULL) /
        debug_inline_nonconstant_unsigned_long_long_volatile_tls_atomic_load_divisor);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_volatile_tls_atomic_load_xor_remainder_entry(
    void)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_nonconstant_unsigned_long_long_volatile_tls_atomic_load_dividend ^
         0x100000001ULL) %
        debug_inline_nonconstant_unsigned_long_long_volatile_tls_atomic_load_divisor);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_atomic_local_load_xor_divide_entry(
    unsigned long long computed_dividend,
    unsigned long long computed_divisor)
{
    _Atomic unsigned long long atomic_local_load_xor_divide_dividend =
        computed_dividend;
    _Atomic unsigned long long atomic_local_load_xor_divide_divisor =
        computed_divisor;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (atomic_local_load_xor_divide_dividend ^ 0x100000001ULL) /
        atomic_local_load_xor_divide_divisor);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_atomic_local_load_xor_remainder_entry(
    unsigned long long computed_dividend,
    unsigned long long computed_divisor)
{
    _Atomic unsigned long long atomic_local_load_xor_remainder_dividend =
        computed_dividend;
    _Atomic unsigned long long atomic_local_load_xor_remainder_divisor =
        computed_divisor;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (atomic_local_load_xor_remainder_dividend ^ 0x100000001ULL) %
        atomic_local_load_xor_remainder_divisor);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_volatile_atomic_local_load_xor_divide_entry(
    unsigned long long computed_dividend,
    unsigned long long computed_divisor)
{
    volatile _Atomic unsigned long long
        volatile_atomic_local_load_xor_divide_dividend = computed_dividend;
    volatile _Atomic unsigned long long
        volatile_atomic_local_load_xor_divide_divisor = computed_divisor;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (volatile_atomic_local_load_xor_divide_dividend ^ 0x100000001ULL) /
        volatile_atomic_local_load_xor_divide_divisor);
}

unsigned long long
debug_inline_nonconstant_unsigned_long_long_volatile_atomic_local_load_xor_remainder_entry(
    unsigned long long computed_dividend,
    unsigned long long computed_divisor)
{
    volatile _Atomic unsigned long long
        volatile_atomic_local_load_xor_remainder_dividend = computed_dividend;
    volatile _Atomic unsigned long long
        volatile_atomic_local_load_xor_remainder_divisor = computed_divisor;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (volatile_atomic_local_load_xor_remainder_dividend ^
         0x100000001ULL) %
        volatile_atomic_local_load_xor_remainder_divisor);
}

static inline long long
debug_nonconstant_signed_long_long_parameter_inline(long long value)
{
    return value;
}

_Atomic long long debug_inline_nonconstant_signed_long_long_atomic_dividend =
    -0x7123456789abcdefll;
_Atomic long long debug_inline_nonconstant_signed_long_long_atomic_divisor =
    -0x100000001ll;
_Thread_local _Atomic long long debug_inline_nonconstant_signed_long_long_tls_atomic_dividend =
    -0x7123456789abcdefll;
_Thread_local _Atomic long long
    debug_inline_nonconstant_signed_long_long_tls_atomic_divisor =
        -0x100000001ll;

long long
debug_inline_nonconstant_signed_long_long_atomic_tls_load_xor_divide_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        (debug_inline_nonconstant_signed_long_long_tls_atomic_dividend ^
         0x100000001ll) /
        debug_inline_nonconstant_signed_long_long_tls_atomic_divisor);
}

long long
debug_inline_nonconstant_signed_long_long_atomic_tls_load_xor_remainder_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        (debug_inline_nonconstant_signed_long_long_tls_atomic_dividend ^
         0x100000001ll) %
        debug_inline_nonconstant_signed_long_long_tls_atomic_divisor);
}

long long
debug_inline_nonconstant_signed_long_long_atomic_local_load_xor_divide_entry(
    long long computed_atomic_local_dividend,
    long long computed_atomic_local_divisor)
{
    _Atomic long long debug_inline_nonconstant_signed_long_long_atomic_local_load_xor_dividend =
        computed_atomic_local_dividend;
    _Atomic long long debug_inline_nonconstant_signed_long_long_atomic_local_load_xor_divisor =
        computed_atomic_local_divisor;
    return debug_nonconstant_signed_long_long_parameter_inline(
        (debug_inline_nonconstant_signed_long_long_atomic_local_load_xor_dividend ^
         0x100000001ll) /
        debug_inline_nonconstant_signed_long_long_atomic_local_load_xor_divisor);
}

long long
debug_inline_nonconstant_signed_long_long_atomic_local_load_xor_remainder_entry(
    long long computed_atomic_local_dividend,
    long long computed_atomic_local_divisor)
{
    _Atomic long long debug_inline_nonconstant_signed_long_long_atomic_local_load_xor_remainder_dividend =
        computed_atomic_local_dividend;
    _Atomic long long debug_inline_nonconstant_signed_long_long_atomic_local_load_xor_remainder_divisor =
        computed_atomic_local_divisor;
    return debug_nonconstant_signed_long_long_parameter_inline(
        (debug_inline_nonconstant_signed_long_long_atomic_local_load_xor_remainder_dividend ^
         0x100000001ll) %
        debug_inline_nonconstant_signed_long_long_atomic_local_load_xor_remainder_divisor);
}

long long
debug_inline_nonconstant_signed_long_long_volatile_atomic_local_load_xor_divide_entry(
    long long computed_volatile_atomic_local_dividend,
    long long computed_volatile_atomic_local_divisor)
{
    volatile _Atomic long long
        debug_inline_nonconstant_signed_long_long_volatile_atomic_local_load_xor_dividend =
            computed_volatile_atomic_local_dividend;
    volatile _Atomic long long
        debug_inline_nonconstant_signed_long_long_volatile_atomic_local_load_xor_divisor =
            computed_volatile_atomic_local_divisor;
    return debug_nonconstant_signed_long_long_parameter_inline(
        (debug_inline_nonconstant_signed_long_long_volatile_atomic_local_load_xor_dividend ^
         0x100000001ll) /
        debug_inline_nonconstant_signed_long_long_volatile_atomic_local_load_xor_divisor);
}

long long
debug_inline_nonconstant_signed_long_long_volatile_atomic_local_load_xor_remainder_entry(
    long long computed_volatile_atomic_local_dividend,
    long long computed_volatile_atomic_local_divisor)
{
    volatile _Atomic long long
        debug_inline_nonconstant_signed_long_long_volatile_atomic_local_load_xor_remainder_dividend =
            computed_volatile_atomic_local_dividend;
    volatile _Atomic long long
        debug_inline_nonconstant_signed_long_long_volatile_atomic_local_load_xor_remainder_divisor =
            computed_volatile_atomic_local_divisor;
    return debug_nonconstant_signed_long_long_parameter_inline(
        (debug_inline_nonconstant_signed_long_long_volatile_atomic_local_load_xor_remainder_dividend ^
         0x100000001ll) %
        debug_inline_nonconstant_signed_long_long_volatile_atomic_local_load_xor_remainder_divisor);
}

long long
debug_inline_nonconstant_signed_long_long_atomic_load_xor_divide_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        (debug_inline_nonconstant_signed_long_long_atomic_dividend ^
         0x100000001ll) /
        debug_inline_nonconstant_signed_long_long_atomic_divisor);
}

long long
debug_inline_nonconstant_signed_long_long_atomic_load_xor_remainder_entry(void)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        (debug_inline_nonconstant_signed_long_long_atomic_dividend ^
         0x100000001ll) %
        debug_inline_nonconstant_signed_long_long_atomic_divisor);
}

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

unsigned long long debug_inline_nonconstant_unsigned_long_long_atomic_store_pointer_volatile_divide_entry(
    volatile _Atomic unsigned long long *computed_atomic_store_volatile_pointer_destination,
    unsigned long long computed_atomic_store_pointer_division_dividend,
    unsigned long long computed_atomic_store_pointer_division_divisor)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        ((*computed_atomic_store_volatile_pointer_destination =
              computed_atomic_store_pointer_division_dividend) /
         (computed_atomic_store_pointer_division_divisor * 2ull + 1ull)));
}

unsigned long long debug_inline_nonconstant_unsigned_long_long_atomic_store_pointer_volatile_remainder_entry(
    volatile _Atomic unsigned long long *computed_atomic_store_volatile_pointer_destination,
    unsigned long long computed_atomic_store_pointer_division_dividend,
    unsigned long long computed_atomic_store_pointer_division_divisor)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        ((*computed_atomic_store_volatile_pointer_destination =
              computed_atomic_store_pointer_division_dividend) %
         (computed_atomic_store_pointer_division_divisor * 2ull + 1ull)));
}

unsigned long long debug_inline_nonconstant_unsigned_long_long_atomic_store_volatile_local_divide_entry(
    unsigned long long computed_atomic_store_division_dividend,
    unsigned long long computed_atomic_store_division_divisor)
{
    volatile _Atomic unsigned long long local_atomic_dividend;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (local_atomic_dividend = computed_atomic_store_division_dividend) /
        (computed_atomic_store_division_divisor * 2ull + 1ull));
}

unsigned long long debug_inline_nonconstant_unsigned_long_long_atomic_store_volatile_local_remainder_entry(
    unsigned long long computed_atomic_store_division_dividend,
    unsigned long long computed_atomic_store_division_divisor)
{
    volatile _Atomic unsigned long long local_atomic_dividend;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (local_atomic_dividend = computed_atomic_store_division_dividend) %
        (computed_atomic_store_division_divisor * 2ull + 1ull));
}

unsigned long long debug_inline_nonconstant_unsigned_long_long_atomic_store_tls_divide_entry(
    unsigned long long computed_atomic_store_division_dividend,
    unsigned long long computed_atomic_store_division_divisor)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_nonconstant_unsigned_long_long_tls_atomic_dividend =
             computed_atomic_store_division_dividend) /
        (computed_atomic_store_division_divisor * 2ull + 1ull));
}

unsigned long long debug_inline_nonconstant_unsigned_long_long_atomic_store_tls_remainder_entry(
    unsigned long long computed_atomic_store_division_dividend,
    unsigned long long computed_atomic_store_division_divisor)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_nonconstant_unsigned_long_long_tls_atomic_dividend =
             computed_atomic_store_division_dividend) %
        (computed_atomic_store_division_divisor * 2ull + 1ull));
}

unsigned long long debug_inline_nonconstant_unsigned_long_long_atomic_store_volatile_tls_divide_entry(
    unsigned long long computed_atomic_store_division_dividend,
    unsigned long long computed_atomic_store_division_divisor)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_nonconstant_unsigned_long_long_volatile_tls_atomic_dividend =
             computed_atomic_store_division_dividend) /
        (computed_atomic_store_division_divisor * 2ull + 1ull));
}

unsigned long long debug_inline_nonconstant_unsigned_long_long_atomic_store_volatile_tls_remainder_entry(
    unsigned long long computed_atomic_store_division_dividend,
    unsigned long long computed_atomic_store_division_divisor)
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_nonconstant_unsigned_long_long_volatile_tls_atomic_dividend =
             computed_atomic_store_division_dividend) %
        (computed_atomic_store_division_divisor * 2ull + 1ull));
}

long long debug_inline_nonconstant_signed_long_long_atomic_store_global_divide_entry(
    long long computed_atomic_store_division_dividend,
    long long computed_atomic_store_division_divisor)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        (debug_inline_nonconstant_signed_long_long_atomic_dividend =
             computed_atomic_store_division_dividend + 17ll) /
        (computed_atomic_store_division_divisor * 2ll + 1ll));
}

long long debug_inline_nonconstant_signed_long_long_atomic_store_global_remainder_entry(
    long long computed_atomic_store_division_dividend,
    long long computed_atomic_store_division_divisor)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        (debug_inline_nonconstant_signed_long_long_atomic_dividend =
             computed_atomic_store_division_dividend + 17ll) %
        (computed_atomic_store_division_divisor * 2ll + 1ll));
}

long long debug_inline_nonconstant_signed_long_long_atomic_store_tls_divide_entry(
    long long computed_atomic_store_division_dividend,
    long long computed_atomic_store_division_divisor)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        (debug_inline_nonconstant_signed_long_long_tls_atomic_dividend =
             computed_atomic_store_division_dividend + 17ll) /
        (computed_atomic_store_division_divisor * 2ll + 1ll));
}

long long debug_inline_nonconstant_signed_long_long_atomic_store_tls_remainder_entry(
    long long computed_atomic_store_division_dividend,
    long long computed_atomic_store_division_divisor)
{
    return debug_nonconstant_signed_long_long_parameter_inline(
        (debug_inline_nonconstant_signed_long_long_tls_atomic_dividend =
             computed_atomic_store_division_dividend + 17ll) %
        (computed_atomic_store_division_divisor * 2ll + 1ll));
}
