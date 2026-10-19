inline int debug_nested_inline(int value)
{
    return value * 2;
}

inline int debug_declared_inline(int value)
{
    return debug_nested_inline(value) + 5;
}

inline int debug_local_inline(int value)
{
    const int inline_local_value = 7;
    return value + inline_local_value;
}

inline int debug_constant_inline(int value)
{
    return value + 3;
}

inline unsigned long long debug_unsigned_constant_inline(
    unsigned long long value)
{
    return value;
}

inline int debug_converted_signed_inline(int value)
{
    return value;
}

inline unsigned int debug_converted_unsigned_inline(unsigned int value)
{
    return value;
}

inline float debug_float_parameter_inline(float value)
{
    return value;
}

inline double debug_double_parameter_inline(double value)
{
    return value;
}

inline double debug_float_to_double_parameter_inline(double value)
{
    return value;
}

inline float debug_double_to_float_parameter_inline(float value)
{
    return value;
}
