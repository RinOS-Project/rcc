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
