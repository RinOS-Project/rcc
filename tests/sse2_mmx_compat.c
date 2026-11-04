#include <emmintrin.h>

#if !defined(__SSE2__)
#error "-msse2 must define __SSE2__"
#endif

int main(void) {
    unsigned long long lanes[2];
    double doubles[2];
    unsigned char data[16];
    unsigned char mask[16];
    unsigned char destination[16];
    union {
        __m64 vector;
        unsigned long long bits;
    } mmx_result;
    union {
        double values[2];
        unsigned long long bits[2];
    } converted;
    __m128i set_vector = _mm_set_epi64(
        (__m64)0x1122334455667788ULL,
        (__m64)0x0123456789abcdefULL);
    __m128i set1_vector = _mm_set1_epi64((__m64)0x8877665544332211ULL);
    __m128i setr_vector = _mm_setr_epi64(
        (__m64)0x1020304050607080ULL,
        (__m64)0x90a0b0c0d0e0f000ULL);
    __m128i move_source = _mm_set_epi64x(
        0x1122334455667788LL, (long long)0xfedcba9876543210ULL);
    __m128i moved = _mm_movpi64_epi64(
        _mm_movepi64_pi64(move_source));
    int index;

    _mm_storeu_si128((__m128i*)lanes, set_vector);
    if (lanes[0] != 0x0123456789abcdefULL) return 1;
    if (lanes[1] != 0x1122334455667788ULL) return 11;

    _mm_storeu_si128((__m128i*)lanes, set1_vector);
    if (lanes[0] != 0x8877665544332211ULL ||
        lanes[1] != 0x8877665544332211ULL) return 2;

    _mm_storeu_si128((__m128i*)lanes, setr_vector);
    if (lanes[0] != 0x1020304050607080ULL ||
        lanes[1] != 0x90a0b0c0d0e0f000ULL) return 3;

    mmx_result.vector = _mm_mul_su32((__m64)0xffffffffULL,
                                     (__m64)2ULL);
    if (mmx_result.bits != 0x00000001fffffffeULL) return 4;
    _mm_empty();

    mmx_result.vector = _mm_movepi64_pi64(move_source);
    if (mmx_result.bits != 0xfedcba9876543210ULL) return 5;
    _mm_storeu_si128((__m128i*)lanes, moved);
    if (lanes[0] != 0xfedcba9876543210ULL || lanes[1] != 0) return 6;
    _mm_empty();

    doubles[0] = 1.5;
    doubles[1] = -2.5;
    mmx_result.vector = _mm_cvtpd_pi32(_mm_loadu_pd(doubles));
    if ((unsigned int)mmx_result.bits != 2U ||
        (int)(unsigned int)(mmx_result.bits >> 32) != -2) return 7;
    _mm_empty();

    doubles[0] = 1.9;
    doubles[1] = -2.9;
    mmx_result.vector = _mm_cvttpd_pi32(_mm_loadu_pd(doubles));
    if ((unsigned int)mmx_result.bits != 1U ||
        (int)(unsigned int)(mmx_result.bits >> 32) != -2) return 8;
    _mm_empty();

    mmx_result.bits = 0x499602d2fffffffeULL;
    _mm_storeu_pd(converted.values,
                  _mm_cvtpi32_pd(mmx_result.vector));
    if (converted.bits[0] != 0xc000000000000000ULL) return 19;
    if (converted.bits[1] != 0x41d26580b4800000ULL) return 29;
    _mm_empty();

    for (index = 0; index < 16; ++index) {
        data[index] = (unsigned char)(index + 1);
        mask[index] = (unsigned char)((index & 1) ? 0x80 : 0x00);
        destination[index] = 0xa5;
    }
    _mm_maskmoveu_si128(_mm_loadu_si128((const __m128i*)data),
                        _mm_loadu_si128((const __m128i*)mask),
                        (char*)destination);
    for (index = 0; index < 16; ++index) {
        unsigned char expected = (index & 1)
            ? (unsigned char)(index + 1) : 0xa5;
        if (destination[index] != expected) return 10;
    }
    _mm_empty();
    return 0;
}
