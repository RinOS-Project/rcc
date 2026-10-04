static unsigned int golden_mix(unsigned int value)
{
    return (value * 8u) + (value / 4u) + (value % 4u);
}

int golden_c17_entry(int value)
{
    int local = value + 3;
    return (int)golden_mix((unsigned int)local);
}
