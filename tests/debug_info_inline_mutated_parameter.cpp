static inline int debug_inline_mutated_parameter(int mutable_parameter)
{
    mutable_parameter += 3;
    mutable_parameter *= 2;
    return mutable_parameter * 2;
}

extern "C" void debug_inline_mutated_parameter_observe(int* value);

extern "C" int debug_inline_mutated_parameter_entry(int input_value)
{
    int stable_input = input_value;
    debug_inline_mutated_parameter_observe(&stable_input);
    return debug_inline_mutated_parameter(stable_input);
}

static inline unsigned int debug_inline_mutated_unsigned_parameter(
    unsigned int unsigned_parameter)
{
    unsigned_parameter += 3u;
    return unsigned_parameter * 2u;
}

extern "C" void debug_inline_mutated_unsigned_parameter_observe(
    unsigned int* value);

extern "C" unsigned int debug_inline_mutated_unsigned_parameter_entry(
    unsigned int input_value)
{
    unsigned int stable_unsigned_input = input_value;
    debug_inline_mutated_unsigned_parameter_observe(&stable_unsigned_input);
    return debug_inline_mutated_unsigned_parameter(stable_unsigned_input);
}
