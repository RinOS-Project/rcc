/* SPDX-License-Identifier: MIT */
/* C23 bit and byte utilities for RinOS supported integer types. */
#ifndef RCC_STDBIT_H
#define RCC_STDBIT_H

#include <stddef.h>
#include <stdint.h>

#define __STDC_VERSION_STDBIT_H__ 202311L

/* RinOS supports little-endian x86 execution environments. */
#define __STDC_ENDIAN_LITTLE__ 1234
#define __STDC_ENDIAN_BIG__ 4321
#define __STDC_ENDIAN_NATIVE__ __STDC_ENDIAN_LITTLE__

static inline unsigned int __rin_stdbit_width(unsigned long long value) {
    unsigned int width = 0;
    while (value != 0ULL) {
        ++width;
        value >>= 1;
    }
    return width;
}

static inline unsigned int __rin_stdbit_prefix(
    unsigned long long value, unsigned int width, unsigned int bit) {
    unsigned int count = 0;
    while (width != 0U) {
        --width;
        if (((value >> width) & 1ULL) != bit) break;
        ++count;
    }
    return count;
}

static inline unsigned int __rin_stdbit_suffix(
    unsigned long long value, unsigned int width, unsigned int bit) {
    unsigned int count = 0;
    while (count < width && ((value >> count) & 1ULL) == bit) ++count;
    return count;
}

static inline unsigned int __rin_stdbit_ones(unsigned long long value,
                                              unsigned int width) {
    unsigned int count = 0;
    unsigned int bit;
    for (bit = 0; bit < width; ++bit) count += (unsigned int)((value >> bit) & 1ULL);
    return count;
}

static inline unsigned long long __rin_stdbit_mask(unsigned int width) {
    if (width >= 64U) return ~0ULL;
    return (1ULL << width) - 1ULL;
}

#define __RIN_STDBIT_DEFINE_TYPE(suffix, type) \
static inline unsigned int stdc_leading_zeros_##suffix(type value) { \
    return __rin_stdbit_prefix((unsigned long long)value, \
                               (unsigned int)(sizeof(type) * 8U), 0U); \
} \
static inline unsigned int stdc_leading_ones_##suffix(type value) { \
    return __rin_stdbit_prefix((unsigned long long)value, \
                               (unsigned int)(sizeof(type) * 8U), 1U); \
} \
static inline unsigned int stdc_trailing_zeros_##suffix(type value) { \
    return __rin_stdbit_suffix((unsigned long long)value, \
                               (unsigned int)(sizeof(type) * 8U), 0U); \
} \
static inline unsigned int stdc_trailing_ones_##suffix(type value) { \
    return __rin_stdbit_suffix((unsigned long long)value, \
                               (unsigned int)(sizeof(type) * 8U), 1U); \
} \
static inline unsigned int stdc_first_leading_zero_##suffix(type value) { \
    unsigned int count = stdc_leading_ones_##suffix(value); \
    return count == (unsigned int)(sizeof(type) * 8U) ? 0U : count + 1U; \
} \
static inline unsigned int stdc_first_leading_one_##suffix(type value) { \
    unsigned int count = stdc_leading_zeros_##suffix(value); \
    return count == (unsigned int)(sizeof(type) * 8U) ? 0U : count + 1U; \
} \
static inline unsigned int stdc_first_trailing_zero_##suffix(type value) { \
    unsigned int count = stdc_trailing_ones_##suffix(value); \
    return count == (unsigned int)(sizeof(type) * 8U) ? 0U : count + 1U; \
} \
static inline unsigned int stdc_first_trailing_one_##suffix(type value) { \
    unsigned int count = stdc_trailing_zeros_##suffix(value); \
    return count == (unsigned int)(sizeof(type) * 8U) ? 0U : count + 1U; \
} \
static inline unsigned int stdc_count_zeros_##suffix(type value) { \
    return (unsigned int)(sizeof(type) * 8U) - \
        __rin_stdbit_ones((unsigned long long)value, \
                          (unsigned int)(sizeof(type) * 8U)); \
} \
static inline unsigned int stdc_count_ones_##suffix(type value) { \
    return __rin_stdbit_ones((unsigned long long)value, \
                             (unsigned int)(sizeof(type) * 8U)); \
} \
static inline _Bool stdc_has_single_bit_##suffix(type value) { \
    unsigned long long bits = (unsigned long long)value; \
    return bits != 0ULL && (bits & (bits - 1ULL)) == 0ULL; \
} \
static inline unsigned int stdc_bit_width_##suffix(type value) { \
    return __rin_stdbit_width((unsigned long long)value); \
} \
static inline type stdc_bit_floor_##suffix(type value) { \
    unsigned int width = __rin_stdbit_width((unsigned long long)value); \
    return width == 0U ? (type)0 : (type)(1ULL << (width - 1U)); \
} \
static inline type stdc_bit_ceil_##suffix(type value) { \
    unsigned long long bits = (unsigned long long)value; \
    unsigned int width = (unsigned int)(sizeof(type) * 8U); \
    unsigned int used = __rin_stdbit_width(bits); \
    unsigned long long rounded; \
    if (bits == 0ULL) return (type)1; \
    if (__rin_stdbit_ones(bits, width) == 1U) return value; \
    if (used >= width) return (type)0; \
    rounded = 1ULL << used; \
    return rounded > __rin_stdbit_mask(width) ? (type)0 : (type)rounded; \
}

__RIN_STDBIT_DEFINE_TYPE(uc, unsigned char)
__RIN_STDBIT_DEFINE_TYPE(us, unsigned short)
__RIN_STDBIT_DEFINE_TYPE(ui, unsigned int)
__RIN_STDBIT_DEFINE_TYPE(ul, unsigned long)
__RIN_STDBIT_DEFINE_TYPE(ull, unsigned long long)

#define __RIN_STDBIT_GENERIC(name, value) \
    _Generic((value), \
        unsigned char: name##_uc, \
        unsigned short: name##_us, \
        unsigned int: name##_ui, \
        unsigned long: name##_ul, \
        unsigned long long: name##_ull)(value)

#define stdc_leading_zeros(value) __RIN_STDBIT_GENERIC(stdc_leading_zeros, value)
#define stdc_leading_ones(value) __RIN_STDBIT_GENERIC(stdc_leading_ones, value)
#define stdc_trailing_zeros(value) __RIN_STDBIT_GENERIC(stdc_trailing_zeros, value)
#define stdc_trailing_ones(value) __RIN_STDBIT_GENERIC(stdc_trailing_ones, value)
#define stdc_first_leading_zero(value) __RIN_STDBIT_GENERIC(stdc_first_leading_zero, value)
#define stdc_first_leading_one(value) __RIN_STDBIT_GENERIC(stdc_first_leading_one, value)
#define stdc_first_trailing_zero(value) __RIN_STDBIT_GENERIC(stdc_first_trailing_zero, value)
#define stdc_first_trailing_one(value) __RIN_STDBIT_GENERIC(stdc_first_trailing_one, value)
#define stdc_count_zeros(value) __RIN_STDBIT_GENERIC(stdc_count_zeros, value)
#define stdc_count_ones(value) __RIN_STDBIT_GENERIC(stdc_count_ones, value)
#define stdc_has_single_bit(value) __RIN_STDBIT_GENERIC(stdc_has_single_bit, value)
#define stdc_bit_width(value) __RIN_STDBIT_GENERIC(stdc_bit_width, value)
#define stdc_bit_floor(value) __RIN_STDBIT_GENERIC(stdc_bit_floor, value)
#define stdc_bit_ceil(value) __RIN_STDBIT_GENERIC(stdc_bit_ceil, value)

#undef __RIN_STDBIT_DEFINE_TYPE

#endif /* RCC_STDBIT_H */
