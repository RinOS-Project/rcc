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

int verified_mul_signed(int left, int right, int* result)
{
    return __builtin_mul_overflow(left, right, result);
}

int verified_mul_unsigned(unsigned int left, unsigned int right,
                          unsigned int* result)
{
    return __builtin_mul_overflow(left, right, result);
}

#if defined(__x86_64__)
int verified_mul_signed64(long long left, long long right, long long* result)
{
    return __builtin_mul_overflow(left, right, result);
}

int verified_mul_unsigned64(unsigned long long left,
                            unsigned long long right,
                            unsigned long long* result)
{
    return __builtin_mul_overflow(left, right, result);
}
#endif
