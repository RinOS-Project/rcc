#include <xmmintrin.h>

__m128 cvtsi642ss_requires_x86_64(__m128 seed, long long value) {
    return __builtin_ia32_cvtsi642ss(seed, value);
}
