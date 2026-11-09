#include <stdbit.h>

#if !defined(__STDC_VERSION_STDBIT_H__) || \
    __STDC_VERSION_STDBIT_H__ != 202311L
#error "stdbit.h must publish its C23 header version"
#endif

#define CHECK_UNSIGNED_TYPE(type, width, error) do { \
    type zero = (type)0; \
    type all = (type)~(type)0; \
    type pattern = (type)0x24U; \
    type top = (type)((type)1 << ((width) - 1U)); \
    if (stdc_leading_zeros(zero) != (width)) return (error); \
    if (stdc_leading_ones(zero) != 0U) return (error) + 1; \
    if (stdc_trailing_zeros(zero) != (width)) return (error) + 2; \
    if (stdc_trailing_ones(zero) != 0U) return (error) + 3; \
    if (stdc_first_leading_zero(zero) != 1U) return (error) + 4; \
    if (stdc_first_leading_one(zero) != 0U) return (error) + 5; \
    if (stdc_first_trailing_zero(zero) != 1U) return (error) + 6; \
    if (stdc_first_trailing_one(zero) != 0U) return (error) + 7; \
    if (stdc_count_zeros(zero) != (width)) return (error) + 8; \
    if (stdc_count_ones(zero) != 0U) return (error) + 9; \
    if (stdc_has_single_bit(zero)) return (error) + 10; \
    if (stdc_bit_width(zero) != 0U) return (error) + 11; \
    if (stdc_bit_floor(zero) != (type)0) return (error) + 12; \
    if (stdc_bit_ceil(zero) != (type)1) return (error) + 13; \
    if (stdc_leading_zeros(all) != 0U) return (error) + 14; \
    if (stdc_leading_ones(all) != (width)) return (error) + 15; \
    if (stdc_trailing_zeros(all) != 0U) return (error) + 16; \
    if (stdc_trailing_ones(all) != (width)) return (error) + 17; \
    if (stdc_first_leading_zero(all) != 0U) return (error) + 18; \
    if (stdc_first_leading_one(all) != 1U) return (error) + 19; \
    if (stdc_first_trailing_zero(all) != 0U) return (error) + 20; \
    if (stdc_first_trailing_one(all) != 1U) return (error) + 21; \
    if (stdc_count_zeros(all) != 0U) return (error) + 22; \
    if (stdc_count_ones(all) != (width)) return (error) + 23; \
    if (stdc_has_single_bit(all)) return (error) + 24; \
    if (stdc_bit_width(all) != (width)) return (error) + 25; \
    if (stdc_bit_floor(all) != top) return (error) + 26; \
    if (stdc_bit_ceil(all) != (type)0) return (error) + 27; \
    if (stdc_leading_zeros(pattern) != (width) - 6U) return (error) + 28; \
    if (stdc_trailing_zeros(pattern) != 2U) return (error) + 29; \
    if (stdc_count_ones(pattern) != 2U) return (error) + 30; \
    if (stdc_bit_width(pattern) != 6U) return (error) + 31; \
    if (stdc_bit_floor(pattern) != (type)0x20U) return (error) + 32; \
    if (stdc_bit_ceil(pattern) != (type)0x40U) return (error) + 33; \
    if (!stdc_has_single_bit(top) || stdc_bit_ceil(top) != top) \
        return (error) + 34; \
    if (!_Generic(stdc_leading_zeros(zero), unsigned int: 1, default: 0)) \
        return (error) + 35; \
    if (!_Generic(stdc_has_single_bit(zero), _Bool: 1, default: 0)) \
        return (error) + 36; \
    if (!_Generic(stdc_bit_floor(zero), type: 1, default: 0)) \
        return (error) + 37; \
    if (!_Generic(stdc_bit_ceil(zero), type: 1, default: 0)) \
        return (error) + 38; \
} while (0)

int main(void)
{
    CHECK_UNSIGNED_TYPE(unsigned char, sizeof(unsigned char) * 8U, 10);
    CHECK_UNSIGNED_TYPE(unsigned short, sizeof(unsigned short) * 8U, 60);
    CHECK_UNSIGNED_TYPE(unsigned int, sizeof(unsigned int) * 8U, 110);
    CHECK_UNSIGNED_TYPE(unsigned long, sizeof(unsigned long) * 8U, 160);
    CHECK_UNSIGNED_TYPE(unsigned long long,
                        sizeof(unsigned long long) * 8U, 210);
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
    CHECK_UNSIGNED_TYPE(unsigned _BitInt(8), 8U, 260);
    CHECK_UNSIGNED_TYPE(unsigned _BitInt(16), 16U, 310);
    CHECK_UNSIGNED_TYPE(unsigned _BitInt(32), 32U, 360);
    CHECK_UNSIGNED_TYPE(unsigned _BitInt(64), 64U, 410);
#endif
    return 0;
}
