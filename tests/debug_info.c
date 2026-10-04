static int debug_line_helper(void)
{
    return 3;
}

inline int debug_declared_inline(int value)
{
    return value + 5;
}

int debug_line_entry(void)
{
    return debug_line_helper() + debug_declared_inline(1);
}

int debug_info_parameters(int left, int right)
{
    int sum = left + right;
    int* pointer = &sum;
    return *pointer;
}
