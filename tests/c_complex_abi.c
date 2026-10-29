#if defined(__STDC_NO_COMPLEX__)
#error "the verified-backend complex test must advertise complex support"
#endif

#include <complex.h>

typedef float complex RccComplexFloat;
typedef double complex RccComplexDouble;
typedef long double complex RccComplexLongDouble;

RccComplexDouble rcc_complex_static_value = CMPLX(7.0, -8.0);
_Thread_local RccComplexDouble rcc_complex_tls_value =
    CMPLX(9.0, 10.0);

RccComplexFloat rcc_complex_float_identity(RccComplexFloat value) {
    return value;
}

RccComplexFloat rcc_complex_float_add(RccComplexFloat left,
                                      RccComplexFloat right) {
    return left + right;
}

RccComplexFloat rcc_complex_float_multiply(RccComplexFloat left,
                                           RccComplexFloat right) {
    return left * right;
}

RccComplexFloat rcc_complex_float_divide(RccComplexFloat left,
                                         RccComplexFloat right) {
    return left / right;
}

RccComplexFloat rcc_complex_float_negate(RccComplexFloat value) {
    return -value;
}

float rcc_complex_float_real(RccComplexFloat value) {
    return (float)value;
}

int rcc_complex_float_truth(RccComplexFloat value) {
    return !!value;
}

int rcc_complex_float_equal(RccComplexFloat left, RccComplexFloat right) {
    return left == right;
}

RccComplexDouble rcc_complex_float_to_double(RccComplexFloat value) {
    return (RccComplexDouble)value;
}

RccComplexDouble rcc_complex_standard_imaginary_unit(void) {
    return 2.0 + 3.0 * I;
}

RccComplexDouble rcc_complex_standard_constructor(void) {
    return CMPLX(2.0, 3.0);
}

RccComplexDouble rcc_complex_double_identity(RccComplexDouble value) {
    return value;
}

RccComplexDouble rcc_complex_double_add(RccComplexDouble left,
                                        RccComplexDouble right) {
    return left + right;
}

RccComplexDouble rcc_complex_double_multiply(RccComplexDouble left,
                                             RccComplexDouble right) {
    return left * right;
}

RccComplexDouble rcc_complex_double_divide(RccComplexDouble left,
                                           RccComplexDouble right) {
    return left / right;
}

int rcc_complex_local_scalar_initializer(void) {
    RccComplexFloat value = 3.0f;
    return value != __builtin_complex(3.0f, 0.0f);
}

int rcc_complex_local_constructor_initializer(void) {
    RccComplexFloat value = __builtin_complex(2.0f, 3.0f);
    return value != __builtin_complex(2.0f, 3.0f);
}

int rcc_complex_local_arithmetic(void) {
    RccComplexFloat left = __builtin_complex(2.0f, 3.0f);
    RccComplexFloat right = __builtin_complex(4.0f, -1.0f);
    RccComplexFloat value = left * right + left;
    return value != __builtin_complex(13.0f, 13.0f);
}

int rcc_complex_static_initializer(void) {
    return rcc_complex_static_value != CMPLX(7.0, -8.0);
}

RccComplexLongDouble rcc_complex_long_double_identity(
    RccComplexLongDouble value) {
    return value;
}

RccComplexLongDouble rcc_complex_long_double_add(
    RccComplexLongDouble left, RccComplexLongDouble right) {
    return left + right;
}

RccComplexLongDouble rcc_complex_long_double_multiply(
    RccComplexLongDouble left, RccComplexLongDouble right) {
    return left * right;
}

RccComplexLongDouble rcc_complex_long_double_divide(
    RccComplexLongDouble left, RccComplexLongDouble right) {
    return left / right;
}
