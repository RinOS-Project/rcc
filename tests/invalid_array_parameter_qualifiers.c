int invalid_local_array_parameter_qualifier(void)
{
    int values[static 3];
    return values[0];
}

int invalid_missing_static_array_bound(int values[static])
{
    return values[0];
}

int invalid_unspecified_array_definition(int values[*])
{
    return values[0];
}
