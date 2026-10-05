extern "C" int verified_cxx_add_signed(int left, int right, int* result)
{
    return __builtin_add_overflow(left, right, result);
}

extern "C" int verified_cxx_add_unsigned(unsigned int left,
                                          unsigned int right,
                                          unsigned int* result)
{
    return __builtin_add_overflow(left, right, result);
}

extern "C" int verified_cxx_sub_signed(int left, int right, int* result)
{
    return __builtin_sub_overflow(left, right, result);
}

extern "C" int verified_cxx_sub_unsigned(unsigned int left,
                                          unsigned int right,
                                          unsigned int* result)
{
    return __builtin_sub_overflow(left, right, result);
}

extern "C" int verified_cxx_mul_signed(int left, int right, int* result)
{
    return __builtin_mul_overflow(left, right, result);
}

extern "C" int verified_cxx_mul_unsigned(unsigned int left,
                                          unsigned int right,
                                          unsigned int* result)
{
    return __builtin_mul_overflow(left, right, result);
}

#if defined(__i386__)
extern "C" int verified_cxx_add_signed64(long long left, long long right,
                                           long long* result)
{
    return __builtin_add_overflow(left, right, result);
}

extern "C" int verified_cxx_add_unsigned64(unsigned long long left,
                                             unsigned long long right,
                                             unsigned long long* result)
{
    return __builtin_add_overflow(left, right, result);
}

extern "C" int verified_cxx_sub_signed64(long long left, long long right,
                                           long long* result)
{
    return __builtin_sub_overflow(left, right, result);
}

extern "C" int verified_cxx_sub_unsigned64(unsigned long long left,
                                             unsigned long long right,
                                             unsigned long long* result)
{
    return __builtin_sub_overflow(left, right, result);
}

extern "C" int verified_cxx_mul_signed64(long long left, long long right,
                                           long long* result)
{
    return __builtin_mul_overflow(left, right, result);
}

extern "C" int verified_cxx_mul_unsigned64(unsigned long long left,
                                             unsigned long long right,
                                             unsigned long long* result)
{
    return __builtin_mul_overflow(left, right, result);
}
#endif

#if defined(__x86_64__)
extern "C" int verified_cxx_mul_signed64(long long left, long long right,
                                         long long* result)
{
    return __builtin_mul_overflow(left, right, result);
}

extern "C" int verified_cxx_mul_unsigned64(unsigned long long left,
                                           unsigned long long right,
                                           unsigned long long* result)
{
    return __builtin_mul_overflow(left, right, result);
}
#endif
