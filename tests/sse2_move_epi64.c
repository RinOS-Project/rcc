#include <emmintrin.h>

#if !defined(__SSE2__)
#error "-msse2 must define __SSE2__"
#endif

__m128i rcc_move_epi64(__m128i source) {
    return _mm_move_epi64(source);
}

int main(void) {
    _Alignas(16) unsigned long long lanes[2];
    __m128i source = _mm_set_epi64x(0x1122334455667788LL,
                                    0x0123456789abcdefLL);

    _mm_storeu_si128((__m128i*)lanes, rcc_move_epi64(source));
    if (lanes[0] != 0x0123456789abcdefULL || lanes[1] != 0) return 1;
    return 0;
}
