static int verified_debug_static(int value)
{
    return value + 3;
}

int verified_debug_entry(int value)
{
    return verified_debug_static(value) + 1;
}
