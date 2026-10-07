static int verified_debug_multi_return(int value)
{
    if (value == 0) return 3;
    if (value < 0) return -value;
    return value + 1;
}

static int verified_debug_static(int value)
{
    int local = value + 3;
    {
        int nested = local + 1;
        return nested;
    }
}

int verified_debug_entry(int value)
{
    return verified_debug_static(value) + 1;
}
