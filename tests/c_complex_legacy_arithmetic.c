typedef float _Complex RccLegacyArithmeticFloat;
typedef double _Complex RccLegacyArithmeticDouble;

extern int fegetround(void);
extern int fesetround(int mode);
extern int feclearexcept(int exceptions);
extern int fetestexcept(int exceptions);

#define RCC_LEGACY_FE_INEXACT 0x20
#define RCC_LEGACY_FE_ALL_EXCEPT 0x3d
#define RCC_LEGACY_FE_TONEAREST 0x0000
#define RCC_LEGACY_FE_DOWNWARD 0x0400
#define RCC_LEGACY_FE_UPWARD 0x0800
#define RCC_LEGACY_FE_TOWARDZERO 0x0c00

int rcc_complex_legacy_arithmetic_test(
    float ar, float ai, float br, float bi, float exceptional_float,
    float zero_float, double dar, double dai, double dbr, double dbi,
    double exceptional_double, double zero_double) {
    RccLegacyArithmeticFloat left_float = __builtin_complex(ar, ai);
    RccLegacyArithmeticFloat right_float = __builtin_complex(br, bi);
    RccLegacyArithmeticFloat float_result = left_float * right_float;
    RccLegacyArithmeticFloat float_quotient =
        left_float / __builtin_complex(1.0f, 1.0f);
    RccLegacyArithmeticDouble left_double = __builtin_complex(dar, dai);
    RccLegacyArithmeticDouble right_double = __builtin_complex(dbr, dbi);
    RccLegacyArithmeticDouble double_result = left_double * right_double;
    RccLegacyArithmeticDouble double_quotient =
        left_double / __builtin_complex(1.0, 1.0);
    volatile RccLegacyArithmeticFloat exceptional_float_product =
        __builtin_complex(exceptional_float, exceptional_float) *
        __builtin_complex(1.0f, 0.0f);
    volatile RccLegacyArithmeticFloat exceptional_float_quotient =
        left_float / __builtin_complex(zero_float, zero_float);
    volatile RccLegacyArithmeticDouble exceptional_double_product =
        __builtin_complex(exceptional_double, exceptional_double) *
        __builtin_complex(1.0, 0.0);
    volatile RccLegacyArithmeticDouble exceptional_double_quotient =
        left_double / __builtin_complex(zero_double, zero_double);

    if (__real__ float_result != 11.0f || __imag__ float_result != 10.0f)
        return 10;
    if (__real__ float_quotient != 2.5f) return 11;
    if (__imag__ float_quotient != 0.5f) return 12;
    if (__real__ double_result != 11.0 || __imag__ double_result != 10.0)
        return 20;
    if (__real__ double_quotient != 2.5 ||
        __imag__ double_quotient != 0.5) return 21;
    if (__real__ exceptional_float_product == 0.0f ||
        __imag__ exceptional_float_product == 0.0f) return 30;
    if (__real__ exceptional_float_quotient == 0.0f) return 31;
    if (__real__ exceptional_double_product == 0.0 ||
        __imag__ exceptional_double_product == 0.0) return 40;
    if (__real__ exceptional_double_quotient == 0.0) return 41;
    return 1;
}

int rcc_complex_legacy_rounding_test(void) {
    static const int modes[4] = {
        RCC_LEGACY_FE_TONEAREST, RCC_LEGACY_FE_UPWARD,
        RCC_LEGACY_FE_DOWNWARD, RCC_LEGACY_FE_TOWARDZERO
    };
    static const float expected_float[4] = {
        0x1.000004p1f, 0x1.000006p1f,
        0x1.000004p1f, 0x1.000004p1f
    };
    static const double expected_double[4] = {
        0x1.0000000000002p1, 0x1.0000000000003p1,
        0x1.0000000000002p1, 0x1.0000000000002p1
    };
    int original_rounding = fegetround();
    if (original_rounding == -1) return 1;

    for (unsigned int index = 0u; index < 4u; ++index) {
        float float_tie = 0x1.000002p0f;
        double double_tie = 0x1.0000000000001p0;
        RccLegacyArithmeticFloat float_value;
        RccLegacyArithmeticDouble double_value;
        if (fesetround(modes[index]) != 0) return 2 + (int)index;
        if (feclearexcept(RCC_LEGACY_FE_ALL_EXCEPT) != 0)
            return 6 + (int)index;
        float_value = __builtin_complex(float_tie, float_tie) *
            __builtin_complex(float_tie, float_tie);
        if (__real__ float_value != 0.0f) {
            return 10 + (int)index;
        }
        if (__imag__ float_value != expected_float[index])
            return 30 + (int)index;
        if ((fetestexcept(RCC_LEGACY_FE_INEXACT) &
             RCC_LEGACY_FE_INEXACT) == 0) return 50 + (int)index;

        if (feclearexcept(RCC_LEGACY_FE_ALL_EXCEPT) != 0)
            return 14 + (int)index;
        double_value = __builtin_complex(double_tie, double_tie) *
            __builtin_complex(double_tie, double_tie);
        if (__real__ double_value != 0.0) {
            return 18 + (int)index;
        }
        if (__imag__ double_value != expected_double[index])
            return 70 + (int)index;
        if ((fetestexcept(RCC_LEGACY_FE_INEXACT) &
             RCC_LEGACY_FE_INEXACT) == 0) return 90 + (int)index;
    }
    if (fesetround(original_rounding) != 0) return 22;
    return 0;
}

int rcc_complex_legacy_edge_cases_test(
    float infinity_float, float nan_float, float zero_float,
    float subnormal_float, double infinity_double, double nan_double,
    double zero_double, double subnormal_double) {
    volatile RccLegacyArithmeticFloat infinity_product_float =
        __builtin_complex(infinity_float, infinity_float) *
        __builtin_complex(1.0f, 0.0f);
    volatile RccLegacyArithmeticFloat nan_product_float =
        __builtin_complex(nan_float, 1.0f) *
        __builtin_complex(1.0f, 1.0f);
    volatile RccLegacyArithmeticFloat signed_zero_product_float =
        __builtin_complex(-zero_float, zero_float) *
        __builtin_complex(1.0f, -zero_float);
    volatile RccLegacyArithmeticFloat subnormal_product_float =
        __builtin_complex(subnormal_float, subnormal_float) *
        __builtin_complex(0.25f, 0.125f);
    volatile RccLegacyArithmeticFloat zero_quotient_float =
        __builtin_complex(1.0f, 1.0f) /
        __builtin_complex(zero_float, zero_float);
    volatile RccLegacyArithmeticFloat nan_quotient_float =
        __builtin_complex(1.0f, 1.0f) /
        __builtin_complex(nan_float, 1.0f);
    volatile RccLegacyArithmeticDouble infinity_product_double =
        __builtin_complex(infinity_double, infinity_double) *
        __builtin_complex(1.0, 0.0);
    volatile RccLegacyArithmeticDouble nan_product_double =
        __builtin_complex(nan_double, 1.0) *
        __builtin_complex(1.0, 1.0);
    volatile RccLegacyArithmeticDouble signed_zero_product_double =
        __builtin_complex(-zero_double, zero_double) *
        __builtin_complex(1.0, -zero_double);
    volatile RccLegacyArithmeticDouble subnormal_product_double =
        __builtin_complex(subnormal_double, subnormal_double) *
        __builtin_complex(0.25, 0.125);
    volatile RccLegacyArithmeticDouble zero_quotient_double =
        __builtin_complex(1.0, 1.0) /
        __builtin_complex(zero_double, zero_double);
    volatile RccLegacyArithmeticDouble nan_quotient_double =
        __builtin_complex(1.0, 1.0) /
        __builtin_complex(nan_double, 1.0);

    if (!__builtin_isinf(__real__ infinity_product_float) ||
        !__builtin_isinf(__imag__ infinity_product_float)) return 1;
    if (!__builtin_isnan(__real__ nan_product_float) ||
        !__builtin_isnan(__imag__ nan_product_float)) return 2;
    if (__builtin_signbit(__real__ signed_zero_product_float) ||
        __builtin_signbit(__imag__ signed_zero_product_float)) return 3;
    if (__real__ subnormal_product_float != 0.0f ||
        __imag__ subnormal_product_float != 0.0f) return 4;
    if (!__builtin_isinf(__real__ zero_quotient_float) ||
        !__builtin_isinf(__imag__ zero_quotient_float)) return 5;
    if (!__builtin_isnan(__real__ nan_quotient_float) ||
        !__builtin_isnan(__imag__ nan_quotient_float)) return 6;

    if (!__builtin_isinf(__real__ infinity_product_double) ||
        !__builtin_isinf(__imag__ infinity_product_double)) return 7;
    if (!__builtin_isnan(__real__ nan_product_double) ||
        !__builtin_isnan(__imag__ nan_product_double)) return 8;
    if (__builtin_signbit(__real__ signed_zero_product_double) ||
        __builtin_signbit(__imag__ signed_zero_product_double)) return 9;
    if (__real__ subnormal_product_double != 0.0 ||
        __imag__ subnormal_product_double != 0.0) return 10;
    if (!__builtin_isinf(__real__ zero_quotient_double) ||
        !__builtin_isinf(__imag__ zero_quotient_double)) return 11;
    if (!__builtin_isnan(__real__ nan_quotient_double) ||
        !__builtin_isnan(__imag__ nan_quotient_double)) return 12;
    return 0;
}
