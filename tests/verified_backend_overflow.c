int verified_add_signed(int left, int right, int* result)
{
    return __builtin_add_overflow(left, right, result);
}

int verified_add_unsigned(unsigned int left, unsigned int right,
                          unsigned int* result)
{
    return __builtin_add_overflow(left, right, result);
}

int verified_sub_signed(int left, int right, int* result)
{
    return __builtin_sub_overflow(left, right, result);
}

int verified_sub_unsigned(unsigned int left, unsigned int right,
                          unsigned int* result)
{
    return __builtin_sub_overflow(left, right, result);
}
