#include <fenv.h>

int main(void) {
    volatile long double positive = 2.75L;
    volatile long double negative = -2.75L;
    volatile long double half = 2.5L;
    volatile long double negative_zero = -0.0L;
    int original_round = fegetround();

    if (__builtin_floorl(positive) != 2.0L ||
        __builtin_ceill(positive) != 3.0L ||
        __builtin_roundl(negative) != -3.0L ||
        __builtin_truncl(negative) != -2.0L ||
        __builtin_copysignl(1.0L, negative_zero) != -1.0L) {
        return 1;
    }

    if (original_round == -1 || fesetround(FE_DOWNWARD) != 0 ||
        fegetround() != FE_DOWNWARD || __builtin_rintl(half) != 2.0L ||
        __builtin_roundl(half) != 3.0L) {
        return 2;
    }
    if (feclearexcept(FE_ALL_EXCEPT) != 0 ||
        __builtin_rintl(half) != 2.0L ||
        (fetestexcept(FE_INEXACT) & FE_INEXACT) == 0) {
        return 3;
    }

    if (fesetround(FE_UPWARD) != 0 || fegetround() != FE_UPWARD ||
        __builtin_rintl(half) != 3.0L || __builtin_roundl(half) != 3.0L) {
        return 4;
    }
    if (feclearexcept(FE_ALL_EXCEPT) != 0 ||
        __builtin_rintl(half) != 3.0L ||
        (fetestexcept(FE_INEXACT) & FE_INEXACT) == 0 ||
        fesetround(original_round) != 0) {
        return 5;
    }

    return 0;
}
