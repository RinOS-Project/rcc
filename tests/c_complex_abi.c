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

double rcc_complex_api_creal(RccComplexDouble value) {
    return creal(value);
}

double rcc_complex_api_cimag(RccComplexDouble value) {
    return cimag(value);
}

double rcc_complex_api_cabs(RccComplexDouble value) {
    return cabs(value);
}

double rcc_complex_api_carg(RccComplexDouble value) {
    return carg(value);
}

RccComplexDouble rcc_complex_api_conj(RccComplexDouble value) {
    return conj(value);
}

RccComplexDouble rcc_complex_api_csqrt(RccComplexDouble value) {
    return csqrt(value);
}

float rcc_complex_api_crealf(RccComplexFloat value) {
    return crealf(value);
}

float rcc_complex_api_cimagf(RccComplexFloat value) {
    return cimagf(value);
}

float rcc_complex_api_cabsf(RccComplexFloat value) {
    return cabsf(value);
}

float rcc_complex_api_cargf(RccComplexFloat value) {
    return cargf(value);
}

RccComplexFloat rcc_complex_api_conjf(RccComplexFloat value) {
    return conjf(value);
}

RccComplexFloat rcc_complex_api_csqrtf(RccComplexFloat value) {
    return csqrtf(value);
}

long double rcc_complex_api_creall(RccComplexLongDouble value) {
    return creall(value);
}

long double rcc_complex_api_cimagl(RccComplexLongDouble value) {
    return cimagl(value);
}

long double rcc_complex_api_cabsl(RccComplexLongDouble value) {
    return cabsl(value);
}

long double rcc_complex_api_cargl(RccComplexLongDouble value) {
    return cargl(value);
}

RccComplexLongDouble rcc_complex_api_conjl(RccComplexLongDouble value) {
    return conjl(value);
}

RccComplexLongDouble rcc_complex_api_csqrtl(RccComplexLongDouble value) {
    return csqrtl(value);
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

int rcc_complex_standard_api_all(void) {
#define RCC_COMPLEX_API_CHECK_FLOAT(call, expected_real, expected_imaginary) \
    do { \
        RccComplexFloat result = (call); \
        if (__real__ result != (expected_real) || \
            __imag__ result != (expected_imaginary)) return __LINE__; \
    } while (0)
#define RCC_COMPLEX_API_CHECK_DOUBLE(call, expected_real, expected_imaginary) \
    do { \
        RccComplexDouble result = (call); \
        if (__real__ result != (expected_real) || \
            __imag__ result != (expected_imaginary)) return __LINE__; \
    } while (0)
#define RCC_COMPLEX_API_CHECK_LONG_DOUBLE( \
    call, expected_real, expected_imaginary) \
    do { \
        RccComplexLongDouble result = (call); \
        if (__real__ result != (expected_real) || \
            __imag__ result != (expected_imaginary)) return __LINE__; \
    } while (0)

    RCC_COMPLEX_API_CHECK_FLOAT(cacosf(CMPLXF(1.0f, 0.0f)), 0.0f, 0.0f);
    RCC_COMPLEX_API_CHECK_DOUBLE(cacos(CMPLX(1.0, 0.0)), 0.0, 0.0);
    RCC_COMPLEX_API_CHECK_LONG_DOUBLE(cacosl(CMPLXL(1.0L, 0.0L)),
                                      0.0L, 0.0L);
    RCC_COMPLEX_API_CHECK_FLOAT(casinf(CMPLXF(0.0f, 0.0f)), 0.0f, 0.0f);
    RCC_COMPLEX_API_CHECK_DOUBLE(casin(CMPLX(0.0, 0.0)), 0.0, 0.0);
    RCC_COMPLEX_API_CHECK_LONG_DOUBLE(casinl(CMPLXL(0.0L, 0.0L)), 0.0L, 0.0L);
    RCC_COMPLEX_API_CHECK_FLOAT(catanf(CMPLXF(0.0f, 0.0f)), 0.0f, 0.0f);
    RCC_COMPLEX_API_CHECK_DOUBLE(catan(CMPLX(0.0, 0.0)), 0.0, 0.0);
    RCC_COMPLEX_API_CHECK_LONG_DOUBLE(catanl(CMPLXL(0.0L, 0.0L)), 0.0L, 0.0L);
    RCC_COMPLEX_API_CHECK_FLOAT(ccosf(CMPLXF(0.0f, 0.0f)), 1.0f, 0.0f);
    RCC_COMPLEX_API_CHECK_DOUBLE(ccos(CMPLX(0.0, 0.0)), 1.0, 0.0);
    RCC_COMPLEX_API_CHECK_LONG_DOUBLE(ccosl(CMPLXL(0.0L, 0.0L)), 1.0L, 0.0L);
    RCC_COMPLEX_API_CHECK_FLOAT(csinf(CMPLXF(0.0f, 0.0f)), 0.0f, 0.0f);
    RCC_COMPLEX_API_CHECK_DOUBLE(csin(CMPLX(0.0, 0.0)), 0.0, 0.0);
    RCC_COMPLEX_API_CHECK_LONG_DOUBLE(csinl(CMPLXL(0.0L, 0.0L)), 0.0L, 0.0L);
    RCC_COMPLEX_API_CHECK_FLOAT(ctanf(CMPLXF(0.0f, 0.0f)), 0.0f, 0.0f);
    RCC_COMPLEX_API_CHECK_DOUBLE(ctan(CMPLX(0.0, 0.0)), 0.0, 0.0);
    RCC_COMPLEX_API_CHECK_LONG_DOUBLE(ctanl(CMPLXL(0.0L, 0.0L)), 0.0L, 0.0L);
    RCC_COMPLEX_API_CHECK_FLOAT(cacoshf(CMPLXF(1.0f, 0.0f)), 0.0f, 0.0f);
    RCC_COMPLEX_API_CHECK_DOUBLE(cacosh(CMPLX(1.0, 0.0)), 0.0, 0.0);
    RCC_COMPLEX_API_CHECK_LONG_DOUBLE(cacoshl(CMPLXL(1.0L, 0.0L)), 0.0L, 0.0L);
    RCC_COMPLEX_API_CHECK_FLOAT(casinhf(CMPLXF(0.0f, 0.0f)), 0.0f, 0.0f);
    RCC_COMPLEX_API_CHECK_DOUBLE(casinh(CMPLX(0.0, 0.0)), 0.0, 0.0);
    RCC_COMPLEX_API_CHECK_LONG_DOUBLE(casinhl(CMPLXL(0.0L, 0.0L)), 0.0L, 0.0L);
    RCC_COMPLEX_API_CHECK_FLOAT(catanhf(CMPLXF(0.0f, 0.0f)), 0.0f, 0.0f);
    RCC_COMPLEX_API_CHECK_DOUBLE(catanh(CMPLX(0.0, 0.0)), 0.0, 0.0);
    RCC_COMPLEX_API_CHECK_LONG_DOUBLE(catanhl(CMPLXL(0.0L, 0.0L)), 0.0L, 0.0L);
    RCC_COMPLEX_API_CHECK_FLOAT(ccoshf(CMPLXF(0.0f, 0.0f)), 1.0f, 0.0f);
    RCC_COMPLEX_API_CHECK_DOUBLE(ccosh(CMPLX(0.0, 0.0)), 1.0, 0.0);
    RCC_COMPLEX_API_CHECK_LONG_DOUBLE(ccoshl(CMPLXL(0.0L, 0.0L)), 1.0L, 0.0L);
    RCC_COMPLEX_API_CHECK_FLOAT(csinhf(CMPLXF(0.0f, 0.0f)), 0.0f, 0.0f);
    RCC_COMPLEX_API_CHECK_DOUBLE(csinh(CMPLX(0.0, 0.0)), 0.0, 0.0);
    RCC_COMPLEX_API_CHECK_LONG_DOUBLE(csinhl(CMPLXL(0.0L, 0.0L)), 0.0L, 0.0L);
    RCC_COMPLEX_API_CHECK_FLOAT(ctanhf(CMPLXF(0.0f, 0.0f)), 0.0f, 0.0f);
    RCC_COMPLEX_API_CHECK_DOUBLE(ctanh(CMPLX(0.0, 0.0)), 0.0, 0.0);
    RCC_COMPLEX_API_CHECK_LONG_DOUBLE(ctanhl(CMPLXL(0.0L, 0.0L)), 0.0L, 0.0L);
    RCC_COMPLEX_API_CHECK_FLOAT(cexpf(CMPLXF(0.0f, 0.0f)), 1.0f, 0.0f);
    RCC_COMPLEX_API_CHECK_DOUBLE(cexp(CMPLX(0.0, 0.0)), 1.0, 0.0);
    RCC_COMPLEX_API_CHECK_LONG_DOUBLE(cexpl(CMPLXL(0.0L, 0.0L)), 1.0L, 0.0L);
    RCC_COMPLEX_API_CHECK_FLOAT(clogf(CMPLXF(1.0f, 0.0f)), 0.0f, 0.0f);
    RCC_COMPLEX_API_CHECK_DOUBLE(clog(CMPLX(1.0, 0.0)), 0.0, 0.0);
    RCC_COMPLEX_API_CHECK_LONG_DOUBLE(clogl(CMPLXL(1.0L, 0.0L)), 0.0L, 0.0L);
    RCC_COMPLEX_API_CHECK_FLOAT(cprojf(CMPLXF(1.0f, 2.0f)), 1.0f, 2.0f);
    RCC_COMPLEX_API_CHECK_DOUBLE(cproj(CMPLX(1.0, 2.0)), 1.0, 2.0);
    RCC_COMPLEX_API_CHECK_LONG_DOUBLE(cprojl(CMPLXL(1.0L, 2.0L)), 1.0L, 2.0L);
    RCC_COMPLEX_API_CHECK_FLOAT(
        cpowf(CMPLXF(1.0f, 0.0f), CMPLXF(2.0f, 0.0f)), 1.0f, 0.0f);
    RCC_COMPLEX_API_CHECK_DOUBLE(
        cpow(CMPLX(1.0, 0.0), CMPLX(2.0, 0.0)), 1.0, 0.0);
    RCC_COMPLEX_API_CHECK_LONG_DOUBLE(
        cpowl(CMPLXL(1.0L, 0.0L), CMPLXL(2.0L, 0.0L)), 1.0L, 0.0L);

#undef RCC_COMPLEX_API_CHECK_FLOAT
#undef RCC_COMPLEX_API_CHECK_DOUBLE
#undef RCC_COMPLEX_API_CHECK_LONG_DOUBLE
    return 0;
}
