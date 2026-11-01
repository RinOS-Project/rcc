static inline int debug_inline_mutated_local(int initial_value)
{
    int evolving = initial_value;
    evolving += 7;
    ++evolving;
    evolving *= 2;
    return evolving + 5;
}

extern "C" void debug_inline_mutated_local_observe(int* value);

extern "C" int debug_inline_mutated_local_entry(int input_value)
{
    int stable_input = input_value;
    debug_inline_mutated_local_observe(&stable_input);
    return debug_inline_mutated_local(stable_input);
}

static inline unsigned long long debug_inline_mutated_wide_local(
    unsigned long long initial_value)
{
    unsigned long long wide_evolving = initial_value;
    wide_evolving += 7ULL;
    ++wide_evolving;
    wide_evolving *= 0x100000001ULL;
    wide_evolving ^= 0x8000000000000001ULL;
    wide_evolving &= 0x7fffffffffffffffULL;
    wide_evolving |= 0x0100000000000001ULL;
    wide_evolving <<= 1ULL;
    wide_evolving >>= 3ULL;
    return wide_evolving + 5ULL;
}

extern "C" void debug_inline_mutated_wide_local_observe(
    unsigned long long* value);

extern "C" unsigned long long debug_inline_mutated_wide_local_entry(
    unsigned long long input_value)
{
    unsigned long long stable_wide_input = input_value;
    debug_inline_mutated_wide_local_observe(&stable_wide_input);
    return debug_inline_mutated_wide_local(stable_wide_input);
}

static inline long long debug_inline_mutated_signed_wide_local(
    long long initial_value)
{
    long long signed_wide_evolving = initial_value;
    signed_wide_evolving += 7LL;
    ++signed_wide_evolving;
    signed_wide_evolving *= 2LL;
    signed_wide_evolving ^= -9223372036854775807LL;
    signed_wide_evolving >>= 3LL;
    signed_wide_evolving >>= 33LL;
    signed_wide_evolving &= 0x7fffffffffffffffLL;
    signed_wide_evolving |= 0x0100000000000001LL;
    return signed_wide_evolving + 5LL;
}

extern "C" void debug_inline_mutated_signed_wide_local_observe(
    long long* value);

extern "C" long long debug_inline_mutated_signed_wide_local_entry(
    long long input_value)
{
    long long stable_signed_wide_input = input_value;
    debug_inline_mutated_signed_wide_local_observe(&stable_signed_wide_input);
    return debug_inline_mutated_signed_wide_local(stable_signed_wide_input);
}
