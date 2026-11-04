#include <xmmintrin.h>

#if defined(RCC_EXPECT_SSE1) && !defined(__SSE__)
#error "-msse must define __SSE__"
#endif
#if defined(RCC_EXPECT_SSE1) && defined(__SSE2__)
#error "this fixture must compile without SSE2"
#endif

int main(void) {
    float lanes[4];
    __m128 seed = _mm_set_ps(13.0f, 11.0f, 7.0f, 5.0f);

    _mm_storeu_ps(lanes, _mm_cvtsi32_ss(seed, -1234567));
    if (lanes[0] != -1234567.0f || lanes[1] != 7.0f ||
        lanes[2] != 11.0f || lanes[3] != 13.0f) {
        return 1;
    }

#if defined(__x86_64__)
    _mm_storeu_ps(lanes, _mm_cvtsi64_ss(seed, 4294967296LL));
    if (lanes[0] != 4294967296.0f || lanes[1] != 7.0f ||
        lanes[2] != 11.0f || lanes[3] != 13.0f) {
        return 2;
    }
#endif

    return 0;
}
