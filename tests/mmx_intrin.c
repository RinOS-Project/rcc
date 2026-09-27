/* Runtime coverage for the bounded MMX intrinsic lowering. */
#include <mmintrin.h>

int main(void) {
    __m64 a = _mm_set_pi16(4, 3, 2, 1);
    __m64 b = _mm_set_pi16(8, 7, 6, 5);
    __m64 sum = _mm_add_pi16(a, b);
    __m64 shifted = _mm_slli_pi16(a, 1);
    __m64 equal = _mm_cmpeq_pi16(a, a);
    __m64 zero = _mm_xor_si64(a, a);
    unsigned long long sum_bits = (unsigned long long)sum;
    unsigned long long shifted_bits = (unsigned long long)shifted;
    unsigned long long equal_bits = (unsigned long long)equal;
    unsigned long long zero_bits = (unsigned long long)zero;

    _mm_empty();
    if (sum_bits != 0x000c000a00080006ULL) return 1;
    if (shifted_bits != 0x0008000600040002ULL) return 2;
    if (equal_bits != 0xffffffffffffffffULL) return 3;
    return zero_bits == 0 ? 0 : 4;
}
