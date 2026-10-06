static int verified_debug_static(int value)
{
    int local = value + 3;
    return local;
}

int verified_debug_entry(int value)
{
    return verified_debug_static(value) + 1;
}
