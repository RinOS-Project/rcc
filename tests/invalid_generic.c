int duplicate_types(void)
{
    return _Generic(0, int: 1, signed int: 2, default: 3);
}

int duplicate_defaults(void)
{
    return _Generic(0, default: 1, int: 2, default: 3);
}

int incomplete_association(void)
{
    return _Generic(0, void: 1, default: 2);
}
