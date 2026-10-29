#if defined(__x86_64__) && !defined(RCC_TEST_VERIFIED)
#include <math.h>
#endif

static int classification_builtins(void) {
    double infinity = __builtin_inf();
    double nan_value = __builtin_nan("");
#if !defined(RCC_TEST_VERIFIED)
    double subnormal = 0x0.0000000000001p-1022;
#endif
    if (!__builtin_isnan(nan_value) || __builtin_isnan(infinity)) return 1;
    if (!__builtin_isinf(infinity) || __builtin_isinf(1.0)) return 2;
    if (!__builtin_isfinite(-0.0) || __builtin_isfinite(infinity)) return 3;
#if !defined(RCC_TEST_VERIFIED)
    if (!__builtin_isnormal(1.0) || __builtin_isnormal(subnormal)) return 4;
#else
    if (!__builtin_isnormal(1.0)) return 4;
#endif
    if (!__builtin_signbit(-0.0) || __builtin_signbit(0.0)) return 5;
    if (__builtin_fpclassify(0, 1, 2, 3, 4, nan_value) != 0)
        return 6;
    if (__builtin_fpclassify(0, 1, 2, 3, 4, infinity) != 1)
        return 7;
#if !defined(RCC_TEST_VERIFIED)
    if (__builtin_fpclassify(0, 1, 2, 3, 4, subnormal) != 3)
        return 8;
#endif
#if defined(__x86_64__) && !defined(RCC_TEST_VERIFIED)
    if (!issubnormal(subnormal) || !iszero(-0.0)) return 9;
#endif
    return 0;
}

static int sign_and_rounding_builtins(void) {
    if (__builtin_copysign(2.0, -0.0) != -2.0) return 1;
    if (!__builtin_signbit(__builtin_copysignf(1.0f, -0.0f))) return 2;
    if (__builtin_copysignl(3.0L, -0.0L) != -3.0L) return 3;
    if (__builtin_floor(-2.25) != -3.0) return 4;
    if (__builtin_ceil(-2.25) != -2.0) return 5;
    if (__builtin_trunc(-2.75) != -2.0) return 6;
    if (__builtin_round(2.5) != 3.0 ||
        __builtin_round(-2.5) != -3.0) return 7;
    if (!__builtin_signbit(__builtin_round(-0.25))) return 8;
    if (__builtin_floorf(-1.25f) != -2.0f ||
        __builtin_ceill(1.25L) != 2.0L) return 9;
    if (__builtin_roundl(-2.5L) != -3.0L ||
        __builtin_truncl(-1.75L) != -1.0L) return 10;
    if (!__builtin_signbit(__builtin_trunc(-0.75))) return 11;
#if !defined(RCC_TEST_VERIFIED)
    if (__builtin_rint(2.5) != 2.0 ||
        __builtin_rintf(-2.5f) != -2.0f ||
        __builtin_rintl(3.5L) != 4.0L) return 12;
#endif
    return 0;
}

#if defined(__x86_64__) && !defined(RCC_TEST_VERIFIED)
static int dynamic_rounding_builtin(void) {
    volatile double value = 1.25;
    volatile long double extended_value = 1.25L;
    if (fesetround(FE_DOWNWARD) != 0) return 1;
    if (__builtin_rint(value) != 1.0 ||
        __builtin_rintl(extended_value) != 1.0L) return 2;
    if (fesetround(FE_UPWARD) != 0) return 3;
    if (__builtin_rint(value) != 2.0 ||
        __builtin_rintl(extended_value) != 2.0L) return 4;
    if (fesetround(FE_TOWARDZERO) != 0) return 5;
    if (__builtin_rint(-value) != -1.0 ||
        __builtin_rintl(-extended_value) != -1.0L) return 6;
    if (__builtin_round(value) != 1.0 ||
        __builtin_round(-value) != -1.0) return 7;
    return fesetround(FE_TONEAREST) == 0 ? 0 : 8;
}
#endif

int main(void) {
    int result = classification_builtins();
    if (result) return result;
    result = sign_and_rounding_builtins();
#if defined(__x86_64__) && !defined(RCC_TEST_VERIFIED)
    if (!result) result = dynamic_rounding_builtin();
#endif
    return result ? result + 16 : 0;
}
