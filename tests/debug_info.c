static int debug_line_helper(void)
{
    return 3;
}

int debug_line_entry(void)
{
    return debug_line_helper() + 6;
}

int debug_info_parameters(int left, int right)
{
    int sum = left + right;
    return sum;
}
