static inline long long debug_inline_volatile_reference_read(
    const volatile long long& value)
{
    return value;
}

static inline unsigned long long
debug_nonconstant_unsigned_long_long_parameter_inline(
    unsigned long long value)
{
    return value;
}

extern "C" {
volatile long long debug_inline_volatile_reference_alias_target =
    -81985529216486889LL;
const volatile long long& debug_inline_volatile_reference_global_alias =
    debug_inline_volatile_reference_alias_target;

long long debug_inline_volatile_reference_global_alias_entry()
{
    return debug_inline_volatile_reference_read(
        debug_inline_volatile_reference_global_alias);
}

long long debug_inline_volatile_reference_local_alias_entry()
{
    const volatile long long& debug_inline_volatile_reference_local_alias =
        debug_inline_volatile_reference_global_alias;
    return debug_inline_volatile_reference_read(
        debug_inline_volatile_reference_local_alias);
}

unsigned long long
debug_inline_unsigned_reference_alias_target = 0xfedcba9876543217ULL;
unsigned long long debug_inline_unsigned_reference_alias_divisor =
    0x8000000000000001ULL;
const unsigned long long& debug_inline_unsigned_reference_global_alias =
    debug_inline_unsigned_reference_alias_target;
unsigned long long debug_inline_unsigned_reference_static_alias_target =
    0xfedcba9876543217ULL;
unsigned long long debug_inline_unsigned_reference_static_alias_divisor =
    0x8000000000000001ULL;
static const unsigned long long& debug_inline_unsigned_reference_static_alias =
    debug_inline_unsigned_reference_static_alias_target;
thread_local unsigned long long
    debug_inline_unsigned_reference_tls_alias_target = 0xfedcba9876543217ULL;
thread_local unsigned long long debug_inline_unsigned_reference_tls_divisor =
    0x8000000000000001ULL;

unsigned long long debug_inline_unsigned_reference_alias_global_divide_entry()
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_unsigned_reference_global_alias ^ 0x100000001ULL) /
        debug_inline_unsigned_reference_alias_divisor);
}

unsigned long long
debug_inline_unsigned_reference_alias_global_remainder_entry()
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_unsigned_reference_global_alias ^ 0x100000001ULL) %
        debug_inline_unsigned_reference_alias_divisor);
}

unsigned long long
debug_inline_unsigned_reference_alias_static_divide_entry()
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_unsigned_reference_static_alias ^ 0x100000001ULL) /
        debug_inline_unsigned_reference_static_alias_divisor);
}

unsigned long long
debug_inline_unsigned_reference_alias_static_remainder_entry()
{
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_unsigned_reference_static_alias ^ 0x100000001ULL) %
        debug_inline_unsigned_reference_static_alias_divisor);
}

unsigned long long debug_inline_unsigned_reference_alias_tls_divide_entry()
{
    const unsigned long long&
        debug_inline_unsigned_reference_tls_divide_alias =
        debug_inline_unsigned_reference_tls_alias_target;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_unsigned_reference_tls_divide_alias ^ 0x100000001ULL) /
        debug_inline_unsigned_reference_tls_divisor);
}

unsigned long long
debug_inline_unsigned_reference_alias_tls_remainder_entry()
{
    const unsigned long long&
        debug_inline_unsigned_reference_tls_remainder_alias =
        debug_inline_unsigned_reference_tls_alias_target;
    return debug_nonconstant_unsigned_long_long_parameter_inline(
        (debug_inline_unsigned_reference_tls_remainder_alias ^ 0x100000001ULL) %
        debug_inline_unsigned_reference_tls_divisor);
}
}
