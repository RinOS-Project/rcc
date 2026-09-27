#include <xmmintrin.h>

__m128 invalid_sse_arity(__m128 value) {
    return __builtin_ia32_addps(value);
}

__m128 invalid_sse_immediate(__m128 lhs, __m128 rhs) {
    return __builtin_ia32_shufps(lhs, rhs, lhs);
}
