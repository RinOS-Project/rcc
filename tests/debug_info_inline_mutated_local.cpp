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
