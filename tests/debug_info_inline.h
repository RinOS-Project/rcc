inline int debug_nested_inline(int value)
{
    return value * 2;
}

inline int debug_declared_inline(int value)
{
    return debug_nested_inline(value) + 5;
}
