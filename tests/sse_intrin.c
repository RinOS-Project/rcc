/* Runtime coverage for the bounded SSE/SSE2 intrinsic lowering. */
#include <xmmintrin.h>
#include <emmintrin.h>

int main(void) {
    float a[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float b[4] = {5.0f, 6.0f, 7.0f, 8.0f};
    float sum[4];
    __m128 va = _mm_loadu_ps(a);
    __m128 vb = _mm_loadu_ps(b);
    _mm_storeu_ps(sum, _mm_add_ps(va, vb));
    if (sum[0] != 6.0f || sum[1] != 8.0f ||
        sum[2] != 10.0f || sum[3] != 12.0f) return 1;

    __m128 signs = _mm_set_ps(-1.0f, 0.0f, -2.0f, 3.0f);
    if (_mm_movemask_ps(signs) != 10) return 2;

    int add_values[4];
    int equal_values[4];
    __m128i ia = _mm_set_epi32(4, 3, 2, 1);
    __m128i ib = _mm_set_epi32(8, 7, 6, 5);
    __m128i ic = _mm_set_epi32(4, 9, 2, 0);
    _mm_storeu_si128((__m128i*)add_values, _mm_add_epi32(ia, ib));
    _mm_storeu_si128((__m128i*)equal_values, _mm_cmpeq_epi32(ia, ic));
    if (add_values[0] != 6 || add_values[1] != 8 ||
        add_values[2] != 10 || add_values[3] != 12) return 3;
    if (equal_values[0] != 0 || equal_values[1] != -1 ||
        equal_values[2] != 0 || equal_values[3] != -1) return 4;
    return 0;
}
