inline int debug_nested_inline(int value)
{
    return value * 2;
}

inline int debug_declared_inline(int value)
{
    return debug_nested_inline(value) + 5;
}

inline int debug_nonconstant_integer_parameter_inline(int value)
{
    return value;
}

inline int debug_nonconstant_integer_subtract_parameter_inline(int value)
{
    return value;
}

inline int debug_nonconstant_integer_multiply_parameter_inline(int value)
{
    return value;
}

inline int debug_nonconstant_integer_negate_parameter_inline(int value)
{
    return value;
}

inline int debug_nonconstant_integer_bitand_parameter_inline(int value)
{
    return value;
}

inline int debug_nonconstant_integer_bitor_parameter_inline(int value)
{
    return value;
}

inline int debug_nonconstant_integer_bitxor_parameter_inline(int value)
{
    return value;
}

inline int debug_nonconstant_integer_lshift_parameter_inline(int value)
{
    return value;
}

inline int debug_nonconstant_integer_rshift_parameter_inline(int value)
{
    return value;
}

inline unsigned int debug_nonconstant_unsigned_integer_rshift_parameter_inline(
    unsigned int value)
{
    return value;
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

inline double debug_float_expression_parameter_inline(double value)
{
    return value;
}

inline int debug_null_pointer_parameter_inline(int* value, int* other)
{
    return value == other;
}

inline int debug_enum_parameter_inline(enum debug_enum value)
{
    return (int)value;
}

inline int debug_global_pointer_parameter_inline(int* value)
{
    if (value != (int*)0) {
        return 1;
    } else {
        return 0;
    }
}

inline int debug_const_global_pointer_parameter_inline(const int* value)
{
    return value != (const int*)0;
}

inline int debug_function_pointer_parameter_inline(int (*value)(void))
{
    return value != 0;
}

inline int debug_void_pointer_parameter_inline(const void* value)
{
    return value != (const void*)0;
}
