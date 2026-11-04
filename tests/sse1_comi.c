#include <xmmintrin.h>

#if defined(RCC_EXPECT_SSE1) && !defined(__SSE__)
#error "-msse must define __SSE__"
#endif
#if defined(RCC_EXPECT_SSE1) && defined(__SSE2__)
#error "this fixture must compile without SSE2"
#endif

int main(void) {
    __m128 lower = _mm_set_ss(1.0f);
    __m128 higher = _mm_set_ss(2.0f);
    __m128 equal = _mm_set_ss(1.0f);

    if (!_mm_comilt_ss(lower, higher)) return 1;
    if (!_mm_ucomilt_ss(lower, higher)) return 2;
    if (!_mm_comieq_ss(lower, equal)) return 3;
    if (!_mm_ucomieq_ss(lower, equal)) return 4;
    if (!_mm_comigt_ss(higher, lower)) return 5;
    if (!_mm_ucomigt_ss(higher, lower)) return 6;
    if (_mm_comieq_ss(lower, higher)) return 7;
    if (_mm_ucomieq_ss(lower, higher)) return 8;
    return 0;
}
