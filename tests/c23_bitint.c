#include <limits.h>

#if BITINT_MAXWIDTH != 64
#error "RCC must publish the supported C23 bit-precise integer width"
#endif

typedef unsigned _BitInt(1) u1;
typedef _BitInt(2) s2;
typedef signed _BitInt(5) s5;
typedef unsigned _BitInt(5) u5;
typedef unsigned _BitInt(9) u9;
typedef unsigned _BitInt(33) u33;
typedef _BitInt(64) s64;
typedef unsigned _BitInt(64) u64;

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
    u9 nine_bit_max = 511;
    u9 nine_bit_wrapped = (u9)511 + (u9)1;
    u33 above_uint = (u33)UINT_MAX + (u33)1;
    s64 signed_literal = 9223372036854775807WB;
    s64 signed_min64 = -9223372036854775807LL - 1LL;
    u64 unsigned_literal = 18446744073709551615UWB;
    u64 unsigned_wrapped64 = unsigned_literal + (u64)1;

    if (one != 1u || zero != 0u || signed_min != -2 || signed_max != 1)
        return 1;
    if (global_signed_min != signed_low || signed_high != 15 ||
        signed_sum != -4) return 2;
    if (global_unsigned_max != unsigned_max || unsigned_max != 31u ||
        unsigned_wrapped != 2u || unsigned_cast != 3u) return 3;
    if (nine_bit_max != 511u || nine_bit_wrapped != 0u) return 4;
    if ((unsigned long long)above_uint != 4294967296ULL) return 5;
    if ((long long)signed_literal != LLONG_MAX ||
        (long long)signed_min64 != LLONG_MIN) return 6;
    if ((unsigned long long)unsigned_literal != ULLONG_MAX ||
        unsigned_wrapped64 != 0u) return 7;
    return 0;
}
