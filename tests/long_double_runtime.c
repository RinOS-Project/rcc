#include <stdarg.h>
#include <math.h>

static int near_long_double(long double actual, long double expected) {
    long double difference = actual - expected;

    return difference >= -1e-8L && difference <= 1e-8L;
}

static long double read_long_double(double prefix, int marker, ...) {
    va_list args;
    long double value;
    double trailing;

    if (marker != 7)
        return -1.0L;

    va_start(args, marker);
    value = va_arg(args, long double);
    trailing = va_arg(args, double);
    va_end(args);
    return value + (prefix - 2.25) + (trailing - 4.5);
}

static long double return_long_double(long double value) {
    return value;
}

int main(void) {
    long double value = return_long_double(
        read_long_double(2.25, 7, 3.5L, 4.5));
    long double rounding_tie = 0x1.00000000000008p+0L;
    volatile long double zero_base = 0.0L;
    volatile long double negative_exponent = -1.0L;
    volatile int select_positive_infinity = 1;
    long double zero_negative_power;
    double conditional_infinity;
    long double integral = 0.0L;
    int exponent = 0;
    int original_round = fegetround();

    if (value != 3.5L)
        return 1;
    if (rounding_tie != 1.0L)
        return 2;

    if (fabsl(-2.5L) != 2.5L || fmodl(5.5L, 2.0L) != 1.5L)
        return 3;
    if (sinl(0.0L) != 0.0L || cosl(0.0L) != 1.0L ||
        tanl(0.0L) != 0.0L || sqrtl(2.25L) != 1.5L)
        return 4;
    if (powl(2.0L, 3.0L) != 8.0L || expl(0.0L) != 1.0L ||
        logl(1.0L) != 0.0L || log10l(1.0L) != 0.0L ||
        log2l(1.0L) != 0.0L || log1pl(0.0L) != 0.0L ||
        expm1l(0.0L) != 0.0L || exp2l(3.0L) != 8.0L)
        return 5;
    if (asinl(0.0L) != 0.0L || acosl(1.0L) != 0.0L ||
        atanl(0.0L) != 0.0L || atan2l(0.0L, 1.0L) != 0.0L ||
        sinhl(0.0L) != 0.0L || coshl(0.0L) != 1.0L ||
        tanhl(0.0L) != 0.0L || asinhl(0.0L) != 0.0L ||
        acoshl(1.0L) != 0.0L || atanhl(0.0L) != 0.0L)
        return 6;
    if (cbrtl(8.0L) != 2.0L || hypotl(3.0L, 4.0L) != 5.0L ||
        copysignl(2.0L, -1.0L) != -2.0L ||
        fminl(2.0L, 3.0L) != 2.0L || fmaxl(2.0L, 3.0L) != 3.0L ||
        fdiml(5.0L, 2.0L) != 3.0L || fmal(2.0L, 3.0L, 4.0L) != 10.0L)
        return 7;
    if (modfl(-3.25L, &integral) != -0.25L || integral != -3.0L ||
        frexpl(8.0L, &exponent) != 0.5L || exponent != 4 ||
        ldexpl(0.5L, 4) != 8.0L || scalbnl(0.5L, 4) != 8.0L ||
        scalblnl(0.5L, 4) != 8.0L || remainderl(7.0L, 2.0L) != -1.0L)
        return 8;
    if (!near_long_double(sinl(0.5L), 0.479425538604203L) ||
        !near_long_double(cosl(0.5L), 0.877582561890373L) ||
        !near_long_double(atan2l(1.0L, 0.0L), 1.570796326794897L))
        return 9;
    if (original_round == -1 ||
        fesetround(FE_DOWNWARD) != 0 ||
        fegetround() != FE_DOWNWARD || rintl(2.5L) != 2.0L ||
        fesetround(FE_UPWARD) != 0 ||
        fegetround() != FE_UPWARD || rintl(2.5L) != 3.0L ||
        fesetround(original_round) != 0)
        return 10;
    if (feclearexcept(FE_ALL_EXCEPT) != 0 ||
        !__builtin_isnan(sqrtl(-1.0L)) ||
        (fetestexcept(FE_INVALID) & FE_INVALID) == 0)
        return 11;
    if (feclearexcept(FE_ALL_EXCEPT) != 0 ||
        !__builtin_isnan(fmodl(1.0L, 0.0L)) ||
        (fetestexcept(FE_INVALID) & FE_INVALID) == 0)
        return 12;
    if (feclearexcept(FE_ALL_EXCEPT) != 0) return 13;
    value = logl(0.0L);
    if (!__builtin_isinf(value) || !__builtin_signbit(value) ||
        (fetestexcept(FE_DIVBYZERO) & FE_DIVBYZERO) == 0)
        return 14;
    if (feclearexcept(FE_ALL_EXCEPT) != 0 ||
        !__builtin_isnan(asinl(2.0L)) ||
        (fetestexcept(FE_INVALID) & FE_INVALID) == 0)
        return 15;
    if (feclearexcept(FE_ALL_EXCEPT) != 0 ||
        !__builtin_isnan(acoshl(0.5L)) ||
        (fetestexcept(FE_INVALID) & FE_INVALID) == 0)
        return 16;
    if (feclearexcept(FE_ALL_EXCEPT) != 0 ||
        !__builtin_isnan(powl(-2.0L, 0.5L)) ||
        (fetestexcept(FE_INVALID) & FE_INVALID) == 0)
        return 17;
    if (feclearexcept(FE_ALL_EXCEPT) != 0) return 18;
    zero_negative_power = powl(zero_base, negative_exponent);
    if (!__builtin_isinf(zero_negative_power)) return 24;
    if ((fetestexcept(FE_DIVBYZERO) & FE_DIVBYZERO) == 0) return 23;
    conditional_infinity = select_positive_infinity ? INFINITY : 0.0;
    if (!__builtin_isinf(conditional_infinity)) return 25;
    if (feclearexcept(FE_ALL_EXCEPT) != 0 ||
        !__builtin_isinf(expl(1000.0L)) ||
        (fetestexcept(FE_OVERFLOW | FE_INEXACT) &
         (FE_OVERFLOW | FE_INEXACT)) != (FE_OVERFLOW | FE_INEXACT))
        return 19;
    if (feclearexcept(FE_ALL_EXCEPT) != 0 || expl(-1000.0L) != 0.0L ||
        (fetestexcept(FE_UNDERFLOW | FE_INEXACT) &
         (FE_UNDERFLOW | FE_INEXACT)) != (FE_UNDERFLOW | FE_INEXACT))
        return 20;
    if (feclearexcept(FE_ALL_EXCEPT) != 0 || log1pl(-1.0L) != -INFINITY ||
        (fetestexcept(FE_DIVBYZERO) & FE_DIVBYZERO) == 0)
        return 21;
    if (feclearexcept(FE_ALL_EXCEPT) != 0 ||
        !__builtin_isnan(log1pl(-2.0L)) ||
        (fetestexcept(FE_INVALID) & FE_INVALID) == 0)
        return 22;
    if (feclearexcept(FE_ALL_EXCEPT) != 0 ||
        !__builtin_isnan(logl(-1.0L)) ||
        (fetestexcept(FE_INVALID) & FE_INVALID) == 0)
        return 26;
    if (feclearexcept(FE_ALL_EXCEPT) != 0) return 27;
    value = atanhl(1.0L);
    if (!__builtin_isinf(value) || __builtin_signbit(value) ||
        (fetestexcept(FE_DIVBYZERO) & FE_DIVBYZERO) == 0)
        return 28;
    if (feclearexcept(FE_ALL_EXCEPT) != 0) return 29;
    value = atanhl(-1.0L);
    if (!__builtin_isinf(value) || !__builtin_signbit(value) ||
        (fetestexcept(FE_DIVBYZERO) & FE_DIVBYZERO) == 0)
        return 30;
    if (feclearexcept(FE_ALL_EXCEPT) != 0 ||
        !__builtin_isnan(atanhl(2.0L)) ||
        (fetestexcept(FE_INVALID) & FE_INVALID) == 0)
        return 31;
    return 0;
}
