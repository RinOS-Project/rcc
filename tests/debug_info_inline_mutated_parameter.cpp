static inline int debug_inline_mutated_parameter(int mutable_parameter)
{
    mutable_parameter += 3;
    ++mutable_parameter;
    mutable_parameter *= 2;
    return mutable_parameter * 2;
}

extern "C" void debug_inline_mutated_parameter_observe(int* value);

extern "C" int debug_inline_mutated_parameter_entry(int input_value)
{
    int stable_input = input_value;
    debug_inline_mutated_parameter_observe(&stable_input);
    return debug_inline_mutated_parameter(stable_input + 2);
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

static inline unsigned long long debug_inline_mutated_wide_parameter(
    unsigned long long wide_parameter)
{
    wide_parameter += 3ULL;
    ++wide_parameter;
    return wide_parameter + 7ULL;
}

extern "C" void debug_inline_mutated_wide_parameter_observe(
    unsigned long long* value);

extern "C" unsigned long long debug_inline_mutated_wide_parameter_entry(
    unsigned long long input_value)
{
    unsigned long long stable_wide_input = input_value;
    debug_inline_mutated_wide_parameter_observe(&stable_wide_input);
    return debug_inline_mutated_wide_parameter(stable_wide_input + 2ULL);
}

static inline long long debug_inline_mutated_signed_wide_parameter(
    long long signed_wide_parameter)
{
    signed_wide_parameter += 3LL;
    ++signed_wide_parameter;
    return signed_wide_parameter + 7LL;
}

extern "C" void debug_inline_mutated_signed_wide_parameter_observe(
    long long* value);

extern "C" long long debug_inline_mutated_signed_wide_parameter_entry(
    long long input_value)
{
    long long stable_signed_wide_input = input_value;
    debug_inline_mutated_signed_wide_parameter_observe(
        &stable_signed_wide_input);
    return debug_inline_mutated_signed_wide_parameter(
        stable_signed_wide_input + 2LL);
}

static inline int debug_inline_mutated_reference_source_parameter(
    int reference_source_parameter)
{
    reference_source_parameter += 5;
    return reference_source_parameter * 2;
}

extern "C" void debug_inline_mutated_reference_source_observe(int* value);

extern "C" int debug_inline_mutated_reference_source_entry(int input_value)
{
    int stable_reference_source = input_value;
    debug_inline_mutated_reference_source_observe(&stable_reference_source);
    const int& stable_reference_alias = stable_reference_source;
    return debug_inline_mutated_reference_source_parameter(
        stable_reference_alias);
}

extern "C" void debug_inline_mutated_dereference_source_observe(int** value);

static inline int debug_inline_mutated_dereference_source_parameter(
    int dereference_source_parameter)
{
    dereference_source_parameter += 5;
    return dereference_source_parameter * 2;
}

extern "C" int debug_inline_mutated_dereference_source_entry(int input_value)
{
    int stable_dereference_source = input_value;
    int* stable_pointer_source = &stable_dereference_source;
    debug_inline_mutated_reference_source_observe(
        &stable_dereference_source);
    debug_inline_mutated_dereference_source_observe(&stable_pointer_source);
    return debug_inline_mutated_dereference_source_parameter(
        *stable_pointer_source);
}

extern "C" void debug_inline_mutated_pointer_chain_source_observe(
    int*** value);

static inline int debug_inline_mutated_pointer_chain_source_parameter(
    int pointer_chain_source_parameter)
{
    pointer_chain_source_parameter += 5;
    return pointer_chain_source_parameter * 2;
}

extern "C" int debug_inline_mutated_pointer_chain_source_entry(
    int input_value)
{
    int stable_pointer_chain_scalar = input_value;
    int* stable_pointer_chain_level_one = &stable_pointer_chain_scalar;
    int** stable_pointer_chain_level_two =
        &stable_pointer_chain_level_one;
    debug_inline_mutated_pointer_chain_source_observe(
        &stable_pointer_chain_level_two);
    return debug_inline_mutated_pointer_chain_source_parameter(
        **stable_pointer_chain_level_two);
}
