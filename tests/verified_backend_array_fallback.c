int verified_nested_array_initializer_fallback(void)
{
    int values[2][2] = {{1, 2}, {3, 4}};
    return values[1][0];
}
