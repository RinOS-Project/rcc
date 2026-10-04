int shift_signed_comparison(int value, unsigned long long count)
{
    return (value >> count) < 0;
}

unsigned long long shift_unsigned_int_width(unsigned int value,
                                            unsigned long long count)
{
    return value << count;
}

int shift_unsigned_short(unsigned short value, unsigned long long count)
{
    return value << count;
}

unsigned int shift_unsigned_right(unsigned int value,
                                  unsigned long long count)
{
    return value >> count;
}

int shift_type_signed(void)
{
    return _Generic((-8 >> 2ULL), int: 1,
                    unsigned long long: 2, default: 3);
}

int shift_type_unsigned(void)
{
    return _Generic((1u << 1ULL), unsigned int: 1,
                    unsigned long long: 2, default: 3);
}

int shift_type_short(unsigned short value)
{
    return _Generic((value << 1ULL), int: 1, default: 2);
}

int unary_type_signed_char(signed char value)
{
    return _Generic((-value), int: 1, default: 2);
}

int unary_type_unsigned_short(unsigned short value)
{
    return _Generic((~value), int: 1, default: 2);
}

int bitnot_unsigned_short(unsigned short value)
{
    return ~value;
}

int main(void)
{
    return shift_signed_comparison(-8, 2ULL) == 1 &&
           shift_signed_comparison(8, 2ULL) == 0 &&
           shift_unsigned_int_width(0x80000000U, 1ULL) == 0ULL &&
           shift_unsigned_int_width(3U, 4ULL) == 48ULL &&
           shift_unsigned_short(0x8000U, 1ULL) == 65536 &&
           shift_unsigned_right(0x80000000U, 31ULL) == 1U &&
           shift_type_signed() == 1 && shift_type_unsigned() == 1 &&
           shift_type_short(3U) == 1 && unary_type_signed_char(-3) == 1 &&
           unary_type_unsigned_short(3U) == 1 &&
           bitnot_unsigned_short(0xffffU) == -65536
        ? 0 : 1;
}
