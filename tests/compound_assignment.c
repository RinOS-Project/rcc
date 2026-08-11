unsigned int compound_unsigned_multiply(unsigned int* value,
                                        unsigned int operand)
{ return *value *= operand; }

unsigned int compound_unsigned_divide(unsigned int* value,
                                      unsigned int operand)
{ return *value /= operand; }

unsigned int compound_unsigned_modulo(unsigned int* value,
                                      unsigned int operand)
{ return *value %= operand; }

unsigned int compound_unsigned_divide_wide(unsigned int* value,
                                           unsigned long long operand)
{ return *value /= operand; }

unsigned int compound_unsigned_modulo_wide(unsigned int* value,
                                           unsigned long long operand)
{ return *value %= operand; }

unsigned int compound_unsigned_and(unsigned int* value,
                                   unsigned int operand)
{ return *value &= operand; }

unsigned int compound_unsigned_or(unsigned int* value,
                                  unsigned int operand)
{ return *value |= operand; }

unsigned int compound_unsigned_xor(unsigned int* value,
                                   unsigned int operand)
{ return *value ^= operand; }

unsigned int compound_unsigned_shift_left(unsigned int* value,
                                          unsigned int operand)
{ return *value <<= operand; }

unsigned int compound_unsigned_shift_right(unsigned int* value,
                                           unsigned int operand)
{ return *value >>= operand; }

int compound_signed_divide(int* value, int operand)
{ return *value /= operand; }

int compound_signed_modulo(int* value, int operand)
{ return *value %= operand; }

int compound_signed_shift_right(int* value, int operand)
{ return *value >>= operand; }

int compound_unsigned_char(unsigned char* value, unsigned int operand)
{
    return *value += operand;
}

int compound_signed_char(signed char* value, int operand)
{
    return *value *= operand;
}

int compound_unsigned_short(unsigned short* value, unsigned int operand)
{
    return *value <<= operand;
}

static unsigned int* compound_side_effect_pointer(unsigned int* value,
                                                  int* calls)
{
    ++*calls;
    return value;
}

unsigned int compound_lvalue_once(unsigned int* value, unsigned int operand,
                                  int* calls)
{
    return *compound_side_effect_pointer(value, calls) ^= operand;
}

unsigned int ordinary_unsigned_divide(unsigned int left, unsigned int right)
{
    return left / right;
}

unsigned int ordinary_unsigned_modulo(unsigned int left, unsigned int right)
{
    return left % right;
}
