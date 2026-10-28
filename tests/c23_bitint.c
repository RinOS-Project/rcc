#include <limits.h>
#include <stdarg.h>

#if BITINT_MAXWIDTH != 64
#error "RCC must publish the supported C23 bit-precise integer width"
#endif

typedef unsigned _BitInt(1) u1;
typedef _BitInt(2) s2;
typedef signed _BitInt(5) s5;
typedef unsigned _BitInt(5) u5;
typedef unsigned _BitInt(9) u9;
typedef signed _BitInt(33) s33;
typedef unsigned _BitInt(33) u33;
typedef _BitInt(64) s64;
typedef unsigned _BitInt(64) u64;

struct bitint_fields {
    signed _BitInt(9) signed_narrow : 5;
    unsigned _BitInt(9) unsigned_narrow : 5;
    unsigned _BitInt(33) unsigned_wide : 33;
};

static int check_bitint_varargs(int marker, ...)
{
    va_list arguments;
    int signed_narrow;
    int unsigned_narrow;
    u33 unsigned_wide;
    u64 unsigned_full;

    va_start(arguments, marker);
    signed_narrow = va_arg(arguments, int);
    unsigned_narrow = va_arg(arguments, int);
    unsigned_wide = va_arg(arguments, u33);
    unsigned_full = va_arg(arguments, u64);
    va_end(arguments);

    return marker == 23 && signed_narrow == -7 && unsigned_narrow == 31 &&
           unsigned_wide == (((u33)1 << 32) | (u33)17) &&
           unsigned_full == ULLONG_MAX;
}

static s5 global_signed_min = -16;
static u5 global_unsigned_max = 31;

int main(void)
{
    u1 one = 1uwb;
    u1 zero = (u1)2u;
    s2 signed_min = -2;
    s2 signed_max = 1;
    s5 signed_low = -16;
    s5 signed_high = 15;
    s5 signed_sum = (s5)-7 + (s5)3;
    u5 unsigned_max = 31;
    u5 unsigned_wrapped = (u5)31 + (u5)3;
    u5 unsigned_cast = (u5)35u;
    u5 unsigned_shift = (u5)7 << 2;
    u5 unsigned_shift_wrapped = (u5)31 << 1;
    u9 nine_bit_max = 511;
    u9 nine_bit_wrapped = (u9)511 + (u9)1;
    u33 above_uint = (u33)UINT_MAX + (u33)1;
    s64 signed_literal = 9223372036854775807WB;
    s64 signed_min64 = -9223372036854775807LL - 1LL;
    u64 unsigned_literal = 18446744073709551615UWB;
    u64 unsigned_wrapped64 = unsigned_literal + (u64)1;
    struct bitint_fields fields = {0};

    fields.signed_narrow = -16;
    fields.unsigned_narrow = 31;
    fields.unsigned_wide = ((u33)1 << 32) | (u33)17;

    if (one != 1u || zero != 0u || signed_min != -2 || signed_max != 1)
        return 1;
    if (global_signed_min != signed_low || signed_high != 15 ||
        signed_sum != -4) return 2;
    if (global_unsigned_max != unsigned_max || unsigned_max != 31u ||
        unsigned_wrapped != 2u || unsigned_cast != 3u) return 3;
    if (unsigned_shift != 28u || unsigned_shift_wrapped != 30u) return 9;
    if (nine_bit_max != 511u || nine_bit_wrapped != 0u) return 4;
    if ((unsigned long long)above_uint != 4294967296ULL) return 5;
    if ((long long)signed_literal != LLONG_MAX ||
        (long long)signed_min64 != LLONG_MIN) return 6;
    if ((unsigned long long)unsigned_literal != ULLONG_MAX ||
        unsigned_wrapped64 != 0u) return 7;
    if (fields.signed_narrow != -16 || fields.unsigned_narrow != 31u ||
        fields.unsigned_wide != (((u33)1 << 32) | (u33)17)) return 8;
    if (!_Generic((s5)1, s5: 1, default: 0) ||
        !_Generic((u5)1, u5: 1, default: 0) ||
        !_Generic((s5)1 + 1, int: 1, default: 0) ||
        !_Generic((s33)1 + 1u, s33: 1, default: 0) ||
        !_Generic((u33)1 + 1, u33: 1, default: 0) ||
        !_Generic((u64)1 + 1ULL, unsigned long long: 1, default: 0))
        return 10;
    if (!check_bitint_varargs(23, (s5)-7, (u5)31,
                              ((u33)1 << 32) | (u33)17,
                              (u64)ULLONG_MAX)) return 11;
    return 0;
}
