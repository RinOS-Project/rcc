static inline int debug_inline_mutated_parameter(int mutable_parameter)
{
    mutable_parameter += 3;
    return mutable_parameter * 2;
}

extern "C" void debug_inline_mutated_parameter_observe(int* value);

extern "C" int debug_inline_mutated_parameter_entry(int input_value)
{
    int stable_input = input_value;
    debug_inline_mutated_parameter_observe(&stable_input);
    return debug_inline_mutated_parameter(stable_input);
}
