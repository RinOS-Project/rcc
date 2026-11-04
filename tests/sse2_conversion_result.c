#include <xmmintrin.h>
#include <emmintrin.h>

#if defined(RCC_EXPECT_SSE2) && !defined(__SSE2__)
#error "-msse2 must define __SSE2__"
#endif

typedef float test_v4sf __attribute__((__vector_size__(16)));
typedef double test_v2df __attribute__((__vector_size__(16)));
typedef int test_v4si __attribute__((__vector_size__(16)));

test_v4sf rcc_cvtpd2ps(test_v2df source) {
    return __builtin_ia32_cvtpd2ps(source);
}

test_v4si rcc_cvtpd2dq(test_v2df source) {
    return __builtin_ia32_cvtpd2dq(source);
}

test_v4si rcc_cvttpd2dq(test_v2df source) {
    return __builtin_ia32_cvttpd2dq(source);
}

test_v2df rcc_cvtps2pd(test_v4sf source) {
    return __builtin_ia32_cvtps2pd(source);
}

test_v2df rcc_cvtdq2pd(test_v4si source) {
    return __builtin_ia32_cvtdq2pd(source);
}

test_v4sf rcc_cvtdq2ps(test_v4si source) {
    return __builtin_ia32_cvtdq2ps(source);
}

test_v4si rcc_cvtps2dq(test_v4sf source) {
    return __builtin_ia32_cvtps2dq(source);
}

test_v4si rcc_cvttps2dq(test_v4sf source) {
    return __builtin_ia32_cvttps2dq(source);
}

test_v4sf rcc_cvtsd2ss(test_v4sf destination, test_v2df source) {
    return __builtin_ia32_cvtsd2ss(destination, source);
}

test_v2df rcc_cvtss2sd(test_v2df destination, test_v4sf source) {
    return __builtin_ia32_cvtss2sd(destination, source);
}

test_v2df rcc_cvtsi32sd(test_v2df destination, int source) {
    return __builtin_ia32_cvtsi2sd(destination, source);
}

int rcc_cvtsd2si(test_v2df source) {
    return __builtin_ia32_cvtsd2si(source);
}

int rcc_cvttsd2si(test_v2df source) {
    return __builtin_ia32_cvttsd2si(source);
}

#if defined(__x86_64__)
test_v2df rcc_cvtsi64sd(test_v2df destination, long long source) {
    return __builtin_ia32_cvtsi642sd(destination, source);
}

long long rcc_cvtsd2si64(test_v2df source) {
    return __builtin_ia32_cvtsd2si64(source);
}

long long rcc_cvttsd2si64(test_v2df source) {
    return __builtin_ia32_cvttsd2si64(source);
}
#endif

int main(void) {
    _Alignas(16) float single_input[4] = {1.6f, -1.6f, 2.5f, -2.5f};
    _Alignas(16) float single_output[4];
    _Alignas(16) double double_input[2] = {3.75, -3.75};
    _Alignas(16) double double_output[2];
    _Alignas(16) int integer_input[4] = {17, -23, 31, -47};
    _Alignas(16) int integer_output[4];
    unsigned int saved_mxcsr = _mm_getcsr();
    __m128 seed_single = _mm_set_ps(13.0f, 11.0f, 7.0f, 5.0f);
    __m128d seed_double = _mm_set_pd(91.5, 82.5);
    __m128d scalar_double = _mm_set_pd(123.0, 3.75);

    _mm_setcsr(0x1f80u);

    _mm_storeu_ps(single_output,
                  (__m128)rcc_cvtpd2ps(_mm_loadu_pd(double_input)));
    if (single_output[0] != 3.75f || single_output[1] != -3.75f ||
        single_output[2] != 0.0f || single_output[3] != 0.0f) return 1;

    _mm_storeu_si128((__m128i*)integer_output,
                     _mm_cvtpd_epi32(_mm_loadu_pd(double_input)));
    if (integer_output[0] != 4 || integer_output[1] != -4 ||
        integer_output[2] != 0 || integer_output[3] != 0) return 2;
    _mm_storeu_si128((__m128i*)integer_output,
                     (__m128i)rcc_cvtpd2dq(_mm_loadu_pd(double_input)));
    if (integer_output[0] != 4 || integer_output[1] != -4 ||
        integer_output[2] != 0 || integer_output[3] != 0) return 3;
    _mm_storeu_si128((__m128i*)integer_output,
                     (__m128i)rcc_cvttpd2dq(_mm_loadu_pd(double_input)));
    if (integer_output[0] != 3 || integer_output[1] != -3 ||
        integer_output[2] != 0 || integer_output[3] != 0) return 4;

    _mm_storeu_pd(double_output,
                  rcc_cvtps2pd(_mm_loadu_ps(single_input)));
    if (double_output[0] != (double)single_input[0] ||
        double_output[1] != (double)single_input[1]) return 5;
    _mm_storeu_pd(double_output,
                  rcc_cvtdq2pd((test_v4si)_mm_loadu_si128(
                      (const __m128i*)integer_input)));
    if (double_output[0] != 17.0 || double_output[1] != -23.0) return 6;
    _mm_storeu_ps(single_output,
                  rcc_cvtdq2ps((test_v4si)_mm_loadu_si128(
                      (const __m128i*)integer_input)));
    if (single_output[0] != 17.0f || single_output[1] != -23.0f ||
        single_output[2] != 31.0f || single_output[3] != -47.0f) return 7;

    _mm_storeu_si128((__m128i*)integer_output,
                     (__m128i)rcc_cvtps2dq(_mm_loadu_ps(single_input)));
    if (integer_output[0] != 2 || integer_output[1] != -2 ||
        integer_output[2] != 2 || integer_output[3] != -2) return 8;
    _mm_storeu_si128((__m128i*)integer_output,
                     (__m128i)rcc_cvttps2dq(_mm_loadu_ps(single_input)));
    if (integer_output[0] != 1 || integer_output[1] != -1 ||
        integer_output[2] != 2 || integer_output[3] != -2) return 9;

    _mm_storeu_ps(single_output,
                  (__m128)rcc_cvtsd2ss(seed_single,
                                      _mm_set_pd(123.0, -2.75)));
    if (single_output[0] != -2.75f || single_output[1] != 7.0f ||
        single_output[2] != 11.0f || single_output[3] != 13.0f) return 10;
    _mm_storeu_pd(double_output,
                  rcc_cvtss2sd(seed_double,
                               _mm_set_ps(17.0f, 19.0f, 0.0f, -1.375f)));
    if (double_output[0] != -1.375 || double_output[1] != 91.5) return 11;
    _mm_storeu_pd(double_output,
                  rcc_cvtsi32sd(seed_double, -1234567));
    if (double_output[0] != -1234567.0 || double_output[1] != 91.5) return 12;

    if (rcc_cvtsd2si(scalar_double) != 4 ||
        rcc_cvttsd2si(scalar_double) != 3) return 13;

#if defined(__x86_64__)
    _mm_storeu_pd(double_output,
                  rcc_cvtsi64sd(seed_double, 4294967297LL));
    if (double_output[0] != 4294967297.0 || double_output[1] != 91.5) return 14;
    scalar_double = _mm_set_pd(0.0, 1099511627779.75);
    if (rcc_cvtsd2si64(scalar_double) != 1099511627780LL ||
        rcc_cvttsd2si64(scalar_double) != 1099511627779LL) return 15;
#endif

    _mm_setcsr(saved_mxcsr);
    return 0;
}
