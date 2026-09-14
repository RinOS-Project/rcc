int invalid_vla_initializer(int count)
{
    int values[count] = {1};
    return values[0];
}
