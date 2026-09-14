int invalid_parenthesized_array_new(void)
{
    int* values = new int[3](7);
    return values[0];
}
