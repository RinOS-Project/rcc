/* SPDX-License-Identifier: MIT */
/* C23 checked integer arithmetic for RinOS standard integer types. */
#ifndef RCC_STDCKDINT_H
#define RCC_STDCKDINT_H

#define __STDC_VERSION_STDCKDINT_H__ 202311L

struct __rin_ckd_value {
    unsigned long long low;
    unsigned long long high;
    _Bool negative;
};

static inline struct __rin_ckd_value __rin_ckd_zero(void) {
    struct __rin_ckd_value value;
    value.low = 0ULL;
    value.high = 0ULL;
    value.negative = 0;
    return value;
}

#define __RIN_CKD_SIGNED_CONVERTER(name, type) \
static inline struct __rin_ckd_value __rin_ckd_from_##name(type input) { \
    struct __rin_ckd_value value = __rin_ckd_zero(); \
    long long signed_input = (long long)input; \
    if (signed_input < 0LL) { \
        value.low = (unsigned long long)(-(signed_input + 1LL)) + 1ULL; \
        value.negative = 1; \
    } else { \
        value.low = (unsigned long long)signed_input; \
    } \
    return value; \
}

#define __RIN_CKD_UNSIGNED_CONVERTER(name, type) \
static inline struct __rin_ckd_value __rin_ckd_from_##name(type input) { \
    struct __rin_ckd_value value = __rin_ckd_zero(); \
    value.low = (unsigned long long)input; \
    return value; \
}

__RIN_CKD_SIGNED_CONVERTER(schar, signed char)
__RIN_CKD_UNSIGNED_CONVERTER(uchar, unsigned char)
__RIN_CKD_SIGNED_CONVERTER(short, short)
__RIN_CKD_UNSIGNED_CONVERTER(ushort, unsigned short)
__RIN_CKD_SIGNED_CONVERTER(int, int)
__RIN_CKD_UNSIGNED_CONVERTER(uint, unsigned int)
__RIN_CKD_SIGNED_CONVERTER(long, long)
__RIN_CKD_UNSIGNED_CONVERTER(ulong, unsigned long)
__RIN_CKD_SIGNED_CONVERTER(llong, long long)
__RIN_CKD_UNSIGNED_CONVERTER(ullong, unsigned long long)

#define __RIN_CKD_VALUE(input) \
    _Generic((input), \
        signed char: __rin_ckd_from_schar, \
        unsigned char: __rin_ckd_from_uchar, \
        short: __rin_ckd_from_short, \
        unsigned short: __rin_ckd_from_ushort, \
        int: __rin_ckd_from_int, \
        unsigned int: __rin_ckd_from_uint, \
        long: __rin_ckd_from_long, \
        unsigned long: __rin_ckd_from_ulong, \
        long long: __rin_ckd_from_llong, \
        unsigned long long: __rin_ckd_from_ullong)(input)

#define __RIN_CKD_RESULT_KIND(result) \
    _Generic((result), \
        signed char *: 1U, volatile signed char *: 1U, \
        unsigned char *: 0U, volatile unsigned char *: 0U, \
        short *: 3U, volatile short *: 3U, \
        unsigned short *: 2U, volatile unsigned short *: 2U, \
        int *: 5U, volatile int *: 5U, \
        unsigned int *: 4U, volatile unsigned int *: 4U, \
        long *: 7U, volatile long *: 7U, \
        unsigned long *: 6U, volatile unsigned long *: 6U, \
        long long *: 9U, volatile long long *: 9U, \
        unsigned long long *: 8U, volatile unsigned long long *: 8U)

static inline void __rin_ckd_multiply_wide(unsigned long long left,
                                           unsigned long long right,
                                           unsigned long long *low,
                                           unsigned long long *high) {
    unsigned long long left_low = left & 0xffffffffULL;
    unsigned long long left_high = left >> 32;
    unsigned long long right_low = right & 0xffffffffULL;
    unsigned long long right_high = right >> 32;
    unsigned long long product = left_low * right_low;
    unsigned long long carry;
    *low = product;
    *high = left_high * right_high;

    product = left_low * right_high;
    carry = *low;
    *low += product << 32;
    *high += (product >> 32) + (unsigned long long)(*low < carry);

    product = left_high * right_low;
    carry = *low;
    *low += product << 32;
    *high += (product >> 32) + (unsigned long long)(*low < carry);
}

static inline struct __rin_ckd_value __rin_ckd_add_wide(
    struct __rin_ckd_value left, struct __rin_ckd_value right) {
    struct __rin_ckd_value result;
    if (left.negative == right.negative) {
        result.low = left.low + right.low;
        result.high = left.high + right.high +
            (unsigned long long)(result.low < left.low);
        result.negative = left.negative;
    } else if (left.high > right.high ||
               (left.high == right.high && left.low >= right.low)) {
        result.low = left.low - right.low;
        result.high = left.high - right.high -
            (unsigned long long)(left.low < right.low);
        result.negative = left.negative;
    } else {
        result.low = right.low - left.low;
        result.high = right.high - left.high -
            (unsigned long long)(right.low < left.low);
        result.negative = right.negative;
    }
    if (result.low == 0ULL && result.high == 0ULL) result.negative = 0;
    return result;
}

static inline _Bool __rin_ckd_apply(void *destination, unsigned int kind,
                                    struct __rin_ckd_value left,
                                    struct __rin_ckd_value right,
                                    unsigned int operation) {
    struct __rin_ckd_value result;
    unsigned int width;
    _Bool is_signed = (kind & 1U) != 0U;
    unsigned long long mask;
    unsigned long long sign_bit;
    unsigned long long wrapped;
    _Bool overflow;

    switch (kind) {
        case 0U: case 1U: width = (unsigned int)(sizeof(unsigned char) * 8U); break;
        case 2U: case 3U: width = (unsigned int)(sizeof(unsigned short) * 8U); break;
        case 4U: case 5U: width = (unsigned int)(sizeof(unsigned int) * 8U); break;
        case 6U: case 7U: width = (unsigned int)(sizeof(unsigned long) * 8U); break;
        case 8U: case 9U: width = (unsigned int)(sizeof(unsigned long long) * 8U); break;
        default: return 1;
    }
    mask = width == 64U ? ~0ULL : ((1ULL << width) - 1ULL);
    sign_bit = 1ULL << (width - 1U);

    if (operation == 2U) {
        __rin_ckd_multiply_wide(left.low, right.low,
                                &result.low, &result.high);
        result.negative = left.negative != right.negative;
        if (result.low == 0ULL && result.high == 0ULL) result.negative = 0;
    } else {
        if (operation == 1U &&
            (right.low != 0ULL || right.high != 0ULL)) {
            right.negative = !right.negative;
        }
        result = __rin_ckd_add_wide(left, right);
    }

    if (is_signed) {
        overflow = result.high != 0ULL ||
            (result.negative ? result.low > sign_bit
                             : result.low >= sign_bit);
    } else {
        overflow = (result.negative &&
                    (result.low != 0ULL || result.high != 0ULL)) ||
                   result.high != 0ULL || result.low > mask;
    }

    wrapped = result.negative ? 0ULL - result.low : result.low;
    wrapped &= mask;
    switch (kind) {
        case 0U: *(volatile unsigned char *)destination = (unsigned char)wrapped; break;
        case 1U: *(volatile signed char *)destination = (signed char)wrapped; break;
        case 2U: *(volatile unsigned short *)destination = (unsigned short)wrapped; break;
        case 3U: *(volatile short *)destination = (short)wrapped; break;
        case 4U: *(volatile unsigned int *)destination = (unsigned int)wrapped; break;
        case 5U: *(volatile int *)destination = (int)wrapped; break;
        case 6U: *(volatile unsigned long *)destination = (unsigned long)wrapped; break;
        case 7U: *(volatile long *)destination = (long)wrapped; break;
        case 8U: *(volatile unsigned long long *)destination = wrapped; break;
        case 9U: *(volatile long long *)destination = (long long)wrapped; break;
        default: return 1;
    }
    return overflow;
}

#define ckd_add(result, a, b) \
    __rin_ckd_apply((void *)(result), __RIN_CKD_RESULT_KIND(result), \
                    __RIN_CKD_VALUE(a), __RIN_CKD_VALUE(b), 0U)
#define ckd_sub(result, a, b) \
    __rin_ckd_apply((void *)(result), __RIN_CKD_RESULT_KIND(result), \
                    __RIN_CKD_VALUE(a), __RIN_CKD_VALUE(b), 1U)
#define ckd_mul(result, a, b) \
    __rin_ckd_apply((void *)(result), __RIN_CKD_RESULT_KIND(result), \
                    __RIN_CKD_VALUE(a), __RIN_CKD_VALUE(b), 2U)

#undef __RIN_CKD_SIGNED_CONVERTER
#undef __RIN_CKD_UNSIGNED_CONVERTER

#endif /* RCC_STDCKDINT_H */
