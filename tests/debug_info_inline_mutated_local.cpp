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
