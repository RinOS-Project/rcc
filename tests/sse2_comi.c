typedef double __m128d __attribute__((__vector_size__(16)));

#if defined(RCC_EXPECT_SSE2) && !defined(__SSE2__)
#error "-msse2 must define __SSE2__"
#endif

int ordered_equal(void) {
    __m128d value = {1.0, 0.0};
    return __builtin_ia32_comisdeq(value, value);
}

int unordered_equal(void) {
    __m128d value = {1.0, 0.0};
    return __builtin_ia32_ucomisdeq(value, value);
}

int main(void) {
    __m128d lower = {1.0, 0.0};
    __m128d higher = {2.0, 0.0};

    if (!__builtin_ia32_comisdlt(lower, higher)) return 1;
    if (!__builtin_ia32_ucomisdlt(lower, higher)) return 2;
    if (!ordered_equal()) return 3;
    if (!unordered_equal()) return 4;
    if (!__builtin_ia32_comisdgt(higher, lower)) return 5;
    if (!__builtin_ia32_ucomisdgt(higher, lower)) return 6;
    if (__builtin_ia32_comisdeq(lower, higher)) return 7;
    if (__builtin_ia32_ucomisdeq(lower, higher)) return 8;
    return 0;
}
