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
