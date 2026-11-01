static inline long long debug_inline_volatile_reference_read(
    const volatile long long& value)
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
}
