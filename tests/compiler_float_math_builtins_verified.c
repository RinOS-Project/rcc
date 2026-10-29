static int verify_float_classification(void) {
    float infinity = (float)__builtin_inf();
    float nan_value = (float)__builtin_nan("");
    if (!__builtin_isnan(nan_value) || __builtin_isnan(infinity)) return 1;
    if (!__builtin_isinf(infinity) || __builtin_isinf(1.0f)) return 2;
    if (!__builtin_isfinite(-0.0f) || __builtin_isfinite(infinity)) return 3;
    if (!__builtin_isnormal(1.0f)) return 4;
    if (!__builtin_signbit(-0.0f) || __builtin_signbit(0.0f)) return 5;
    if (__builtin_fpclassify(0, 1, 2, 3, 4, nan_value) != 0) return 6;
    if (__builtin_fpclassify(0, 1, 2, 3, 4, infinity) != 1) return 7;
    if (__builtin_fpclassify(0, 1, 2, 3, 4, 1.0f) != 2) return 8;
    if (__builtin_fpclassify(0, 1, 2, 3, 4, 0.0f) != 4) return 9;
    return 0;
}

static int verify_float_sign_and_rounding(void) {
    if (__builtin_copysignf(2.0f, -0.0f) != -2.0f) return 1;
    if (__builtin_floorf(-2.25f) != -3.0f) return 2;
    if (__builtin_ceilf(-2.25f) != -2.0f) return 3;
    if (__builtin_truncf(-2.75f) != -2.0f) return 4;
    if (__builtin_roundf(2.5f) != 3.0f ||
        __builtin_roundf(-2.5f) != -3.0f) return 5;
    if (!__builtin_signbit(__builtin_roundf(-0.25f))) return 6;
    if (!__builtin_signbit(__builtin_truncf(-0.75f))) return 7;
    return 0;
}

static int verify_double_classification(void) {
    double infinity = __builtin_inf();
    double nan_value = __builtin_nan("");
    double subnormal = 0x0.0000000000001p-1022;
    if (!__builtin_isnan(nan_value) || __builtin_isnan(infinity)) return 1;
    if (!__builtin_isinf(infinity) || __builtin_isinf(1.0)) return 2;
    if (!__builtin_isfinite(-0.0) || __builtin_isfinite(infinity)) return 3;
    if (!__builtin_isnormal(1.0) || __builtin_isnormal(subnormal)) return 4;
    if (!__builtin_signbit(-0.0) || __builtin_signbit(0.0)) return 5;
    if (__builtin_fpclassify(0, 1, 2, 3, 4, nan_value) != 0) return 6;
    if (__builtin_fpclassify(0, 1, 2, 3, 4, infinity) != 1) return 7;
    if (__builtin_fpclassify(0, 1, 2, 3, 4, 1.0) != 2) return 8;
    if (__builtin_fpclassify(0, 1, 2, 3, 4, subnormal) != 3) return 9;
    if (__builtin_fpclassify(0, 1, 2, 3, 4, -0.0) != 4) return 10;
    return 0;
}

static int verify_double_sign_and_rounding(void) {
    if (__builtin_copysign(2.0, -0.0) != -2.0) return 1;
    if (__builtin_floor(-2.25) != -3.0) return 2;
    if (__builtin_ceil(-2.25) != -2.0) return 3;
    if (__builtin_trunc(-2.75) != -2.0) return 4;
    if (__builtin_round(2.5) != 3.0 ||
        __builtin_round(-2.5) != -3.0) return 5;
    if (__builtin_floor(-0.25) != -1.0 ||
        __builtin_ceil(0.25) != 1.0) return 6;
    if (__builtin_round(0.5) != 1.0 ||
        __builtin_round(-0.5) != -1.0 ||
        __builtin_round(0.499) != 0.0) return 7;
    if (!__builtin_signbit(__builtin_floor(-0.0)) ||
        !__builtin_signbit(__builtin_trunc(-0.75))) return 8;
    if (__builtin_floor(4503599627370496.0) != 4503599627370496.0 ||
        __builtin_ceil(-4503599627370496.0) != -4503599627370496.0) {
        return 9;
    }
    return 0;
}

int main(void) {
    int result = verify_float_classification();
    if (result) return result;
    result = verify_float_sign_and_rounding();
    if (result) return result + 16;
    result = verify_double_classification();
    if (result) return result + 32;
    result = verify_double_sign_and_rounding();
    return result ? result + 48 : 0;
}
