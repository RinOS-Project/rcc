typedef unsigned int rin_word;

_Static_assert(
    _Generic((rin_word)0, int: 0, unsigned int: 1, default: 0),
    "typedef-compatible unsigned selection");

int generic_global = _Generic((rin_word)0,
                              int: 11,
                              unsigned int: 22,
                              default: 33);

int generic_int(void)
{
    return _Generic(3, int: 5, unsigned int: 6, default: 7);
}

int generic_pointer(int* pointer)
{
    return _Generic(pointer, int*: 8, void*: 9, default: 10);
}

int generic_default(char** pointer)
{
    return _Generic(pointer, int*: 11, default: 12);
}

int generic_string(void)
{
    return _Generic("RinOS", char*: 13, default: 14);
}

int generic_array(void)
{
    int values[2];
    return _Generic(values, int*: 14, default: 15);
}

int generic_side_effect(void)
{
    int value = 3;
    return _Generic((value = 99),
                    int: value + 4,
                    default: (value = 77));
}

int generic_lvalue(void)
{
    int selected = 3;
    int fallback = 4;
    _Generic(0, int: selected, default: fallback) = 9;
    return selected;
}

int generic_callee(void)
{
    return 0;
}

int generic_function(void)
{
    return _Generic(generic_callee,
                    int (*)(void): 16,
                    default: 17);
}
