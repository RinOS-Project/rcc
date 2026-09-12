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
