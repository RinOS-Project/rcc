#if defined(__STDC_NO_COMPLEX__)
#error "the verified-backend complex test must advertise complex support"
#endif

#include <complex.h>
#include <fenv.h>

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
#define RCC_COMPLEX_API_BRANCH_FLOAT(call, expected_real, negative_imaginary) \
    do { \
        RccComplexFloat result = (call); \
        if (__real__ result != (expected_real) || __imag__ result == 0.0f || \
            !!__builtin_signbit(__imag__ result) != \
                (negative_imaginary)) return __LINE__; \
    } while (0)
#define RCC_COMPLEX_API_BRANCH_DOUBLE(call, expected_real, negative_imaginary) \
    do { \
        RccComplexDouble result = (call); \
        if (__real__ result != (expected_real) || __imag__ result == 0.0 || \
            !!__builtin_signbit(__imag__ result) != \
                (negative_imaginary)) return __LINE__; \
    } while (0)
#define RCC_COMPLEX_API_BRANCH_LONG_DOUBLE( \
    call, expected_real, negative_imaginary) \
    do { \
        RccComplexLongDouble result = (call); \
        if (__real__ result != (expected_real) || __imag__ result == 0.0L || \
            !!__builtin_signbit(__imag__ result) != \
                (negative_imaginary)) return __LINE__; \
    } while (0)
#define RCC_COMPLEX_API_PROJECTION_FLOAT(call, negative_imaginary) \
    do { \
        RccComplexFloat result = (call); \
        if (__real__ result != __builtin_huge_valf() || \
            __imag__ result != 0.0f || \
            !!__builtin_signbit(__imag__ result) != \
                (negative_imaginary)) return __LINE__; \
    } while (0)
#define RCC_COMPLEX_API_PROJECTION_DOUBLE(call, negative_imaginary) \
    do { \
        RccComplexDouble result = (call); \
        if (__real__ result != __builtin_huge_val() || \
            __imag__ result != 0.0 || \
            !!__builtin_signbit(__imag__ result) != \
                (negative_imaginary)) return __LINE__; \
    } while (0)
#define RCC_COMPLEX_API_PROJECTION_LONG_DOUBLE( \
    call, negative_imaginary) \
    do { \
        RccComplexLongDouble result = (call); \
        if (__real__ result != (long double)__builtin_huge_val() || \
            __imag__ result != 0.0L || \
            !!__builtin_signbit(__imag__ result) != \
                (negative_imaginary)) return __LINE__; \
    } while (0)
#define RCC_COMPLEX_API_SIGNED_RANGE_FLOAT( \
    call, real_low, real_high, negative_real, imag_low, imag_high, \
    negative_imaginary) \
    do { \
        RccComplexFloat result = (call); \
        float real = __real__ result; \
        float imaginary = __imag__ result; \
        if ((negative_real) ? \
                (real > -(real_low) || real < -(real_high)) : \
                (real < (real_low) || real > (real_high))) return __LINE__; \
        if ((negative_imaginary) ? \
                (imaginary > -(imag_low) || imaginary < -(imag_high)) : \
                (imaginary < (imag_low) || imaginary > (imag_high))) \
            return __LINE__; \
    } while (0)
#define RCC_COMPLEX_API_SIGNED_RANGE_DOUBLE( \
    call, real_low, real_high, negative_real, imag_low, imag_high, \
    negative_imaginary) \
    do { \
        RccComplexDouble result = (call); \
        double real = __real__ result; \
        double imaginary = __imag__ result; \
        if ((negative_real) ? \
                (real > -(real_low) || real < -(real_high)) : \
                (real < (real_low) || real > (real_high))) return __LINE__; \
        if ((negative_imaginary) ? \
                (imaginary > -(imag_low) || imaginary < -(imag_high)) : \
                (imaginary < (imag_low) || imaginary > (imag_high))) \
            return __LINE__; \
    } while (0)
#define RCC_COMPLEX_API_SIGNED_RANGE_LONG_DOUBLE( \
    call, real_low, real_high, negative_real, imag_low, imag_high, \
    negative_imaginary) \
    do { \
        RccComplexLongDouble result = (call); \
        long double real = __real__ result; \
        long double imaginary = __imag__ result; \
        if ((negative_real) ? \
                (real > -(real_low) || real < -(real_high)) : \
                (real < (real_low) || real > (real_high))) return __LINE__; \
        if ((negative_imaginary) ? \
                (imaginary > -(imag_low) || imaginary < -(imag_high)) : \
                (imaginary < (imag_low) || imaginary > (imag_high))) \
            return __LINE__; \
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
    if (cargf(CMPLXF(-1.0f, 0.0f)) != 3.14159265358979323846f ||
        cargf(CMPLXF(-1.0f, -0.0f)) != -3.14159265358979323846f ||
        carg(CMPLX(-1.0, 0.0)) != 3.14159265358979323846 ||
        carg(CMPLX(-1.0, -0.0)) != -3.14159265358979323846 ||
        cargl(CMPLXL(-1.0L, 0.0L)) != 3.14159265358979323846L ||
        cargl(CMPLXL(-1.0L, -0.0L)) != -3.14159265358979323846L)
        return __LINE__;
    RCC_COMPLEX_API_PROJECTION_FLOAT(
        cprojf(CMPLXF(-__builtin_huge_valf(), 3.0f)), 0);
    RCC_COMPLEX_API_PROJECTION_FLOAT(
        cprojf(CMPLXF(-__builtin_huge_valf(), -3.0f)), 1);
    RCC_COMPLEX_API_PROJECTION_FLOAT(
        cprojf(CMPLXF(3.0f, __builtin_huge_valf())), 0);
    RCC_COMPLEX_API_PROJECTION_FLOAT(
        cprojf(CMPLXF(3.0f, -__builtin_huge_valf())), 1);
    RCC_COMPLEX_API_PROJECTION_FLOAT(
        cprojf(CMPLXF(__builtin_huge_valf(), __builtin_nanf(""))), 0);
    RCC_COMPLEX_API_PROJECTION_FLOAT(
        cprojf(CMPLXF(__builtin_nanf(""), __builtin_huge_valf())), 0);
    RCC_COMPLEX_API_PROJECTION_DOUBLE(
        cproj(CMPLX(-__builtin_huge_val(), 3.0)), 0);
    RCC_COMPLEX_API_PROJECTION_DOUBLE(
        cproj(CMPLX(-__builtin_huge_val(), -3.0)), 1);
    RCC_COMPLEX_API_PROJECTION_DOUBLE(
        cproj(CMPLX(3.0, __builtin_huge_val())), 0);
    RCC_COMPLEX_API_PROJECTION_DOUBLE(
        cproj(CMPLX(3.0, -__builtin_huge_val())), 1);
    RCC_COMPLEX_API_PROJECTION_DOUBLE(
        cproj(CMPLX(__builtin_huge_val(), __builtin_nan(""))), 0);
    RCC_COMPLEX_API_PROJECTION_DOUBLE(
        cproj(CMPLX(__builtin_nan(""), __builtin_huge_val())), 0);
    RCC_COMPLEX_API_PROJECTION_LONG_DOUBLE(
        cprojl(CMPLXL(-(long double)__builtin_huge_val(), 3.0L)), 0);
    RCC_COMPLEX_API_PROJECTION_LONG_DOUBLE(
        cprojl(CMPLXL(-(long double)__builtin_huge_val(), -3.0L)), 1);
    RCC_COMPLEX_API_PROJECTION_LONG_DOUBLE(
        cprojl(CMPLXL(3.0L, (long double)__builtin_huge_val())), 0);
    RCC_COMPLEX_API_PROJECTION_LONG_DOUBLE(
        cprojl(CMPLXL(3.0L, -(long double)__builtin_huge_val())), 1);
    RCC_COMPLEX_API_PROJECTION_LONG_DOUBLE(
        cprojl(CMPLXL((long double)__builtin_huge_val(),
                      (long double)__builtin_nan(""))), 0);
    RCC_COMPLEX_API_PROJECTION_LONG_DOUBLE(
        cprojl(CMPLXL((long double)__builtin_nan(""),
                      (long double)__builtin_huge_val())), 0);
    RCC_COMPLEX_API_CHECK_FLOAT(
        cpowf(CMPLXF(1.0f, 0.0f), CMPLXF(2.0f, 0.0f)), 1.0f, 0.0f);
    RCC_COMPLEX_API_CHECK_DOUBLE(
        cpow(CMPLX(1.0, 0.0), CMPLX(2.0, 0.0)), 1.0, 0.0);
    RCC_COMPLEX_API_CHECK_LONG_DOUBLE(
        cpowl(CMPLXL(1.0L, 0.0L), CMPLXL(2.0L, 0.0L)), 1.0L, 0.0L);

    RCC_COMPLEX_API_BRANCH_FLOAT(
        clogf(CMPLXF(-1.0f, 0.0f)), 0.0f, 0);
    RCC_COMPLEX_API_BRANCH_FLOAT(
        clogf(CMPLXF(-1.0f, -0.0f)), 0.0f, 1);
    RCC_COMPLEX_API_BRANCH_DOUBLE(
        clog(CMPLX(-1.0, 0.0)), 0.0, 0);
    RCC_COMPLEX_API_BRANCH_DOUBLE(
        clog(CMPLX(-1.0, -0.0)), 0.0, 1);
    RCC_COMPLEX_API_BRANCH_LONG_DOUBLE(
        clogl(CMPLXL(-1.0L, 0.0L)), 0.0L, 0);
    RCC_COMPLEX_API_BRANCH_LONG_DOUBLE(
        clogl(CMPLXL(-1.0L, -0.0L)), 0.0L, 1);
    RCC_COMPLEX_API_BRANCH_FLOAT(
        cacosf(CMPLXF(-2.0f, 0.0f)),
        3.14159265358979323846f, 1);
    RCC_COMPLEX_API_BRANCH_FLOAT(
        cacosf(CMPLXF(-2.0f, -0.0f)),
        3.14159265358979323846f, 0);
    RCC_COMPLEX_API_BRANCH_DOUBLE(
        cacos(CMPLX(-2.0, 0.0)), 3.14159265358979323846, 1);
    RCC_COMPLEX_API_BRANCH_DOUBLE(
        cacos(CMPLX(-2.0, -0.0)), 3.14159265358979323846, 0);
    RCC_COMPLEX_API_BRANCH_LONG_DOUBLE(
        cacosl(CMPLXL(-2.0L, 0.0L)), 3.14159265358979323846L, 1);
    RCC_COMPLEX_API_BRANCH_LONG_DOUBLE(
        cacosl(CMPLXL(-2.0L, -0.0L)), 3.14159265358979323846L, 0);
    RCC_COMPLEX_API_BRANCH_FLOAT(
        cacosf(CMPLXF(2.0f, 0.0f)), 0.0f, 1);
    RCC_COMPLEX_API_BRANCH_FLOAT(
        cacosf(CMPLXF(2.0f, -0.0f)), 0.0f, 0);
    RCC_COMPLEX_API_BRANCH_DOUBLE(
        cacos(CMPLX(2.0, 0.0)), 0.0, 1);
    RCC_COMPLEX_API_BRANCH_DOUBLE(
        cacos(CMPLX(2.0, -0.0)), 0.0, 0);
    RCC_COMPLEX_API_BRANCH_LONG_DOUBLE(
        cacosl(CMPLXL(2.0L, 0.0L)), 0.0L, 1);
    RCC_COMPLEX_API_BRANCH_LONG_DOUBLE(
        cacosl(CMPLXL(2.0L, -0.0L)), 0.0L, 0);
    RCC_COMPLEX_API_BRANCH_FLOAT(
        casinf(CMPLXF(2.0f, 0.0f)),
        1.57079632679489661923f, 0);
    RCC_COMPLEX_API_BRANCH_FLOAT(
        casinf(CMPLXF(2.0f, -0.0f)),
        1.57079632679489661923f, 1);
    RCC_COMPLEX_API_BRANCH_DOUBLE(
        casin(CMPLX(2.0, 0.0)), 1.57079632679489661923, 0);
    RCC_COMPLEX_API_BRANCH_DOUBLE(
        casin(CMPLX(2.0, -0.0)), 1.57079632679489661923, 1);
    RCC_COMPLEX_API_BRANCH_LONG_DOUBLE(
        casinl(CMPLXL(2.0L, 0.0L)), 1.57079632679489661923L, 0);
    RCC_COMPLEX_API_BRANCH_LONG_DOUBLE(
        casinl(CMPLXL(2.0L, -0.0L)), 1.57079632679489661923L, 1);
    RCC_COMPLEX_API_BRANCH_FLOAT(
        casinf(CMPLXF(-2.0f, 0.0f)), -1.57079632679489661923f, 0);
    RCC_COMPLEX_API_BRANCH_FLOAT(
        casinf(CMPLXF(-2.0f, -0.0f)), -1.57079632679489661923f, 1);
    RCC_COMPLEX_API_BRANCH_DOUBLE(
        casin(CMPLX(-2.0, 0.0)), -1.57079632679489661923, 0);
    RCC_COMPLEX_API_BRANCH_DOUBLE(
        casin(CMPLX(-2.0, -0.0)), -1.57079632679489661923, 1);
    RCC_COMPLEX_API_BRANCH_LONG_DOUBLE(
        casinl(CMPLXL(-2.0L, 0.0L)), -1.57079632679489661923L, 0);
    RCC_COMPLEX_API_BRANCH_LONG_DOUBLE(
        casinl(CMPLXL(-2.0L, -0.0L)), -1.57079632679489661923L, 1);
    RCC_COMPLEX_API_BRANCH_FLOAT(
        catanf(CMPLXF(0.0f, 2.0f)), 1.57079632679489661923f, 0);
    RCC_COMPLEX_API_BRANCH_FLOAT(
        catanf(CMPLXF(-0.0f, 2.0f)), -1.57079632679489661923f, 0);
    RCC_COMPLEX_API_BRANCH_FLOAT(
        catanf(CMPLXF(0.0f, -2.0f)), 1.57079632679489661923f, 1);
    RCC_COMPLEX_API_BRANCH_FLOAT(
        catanf(CMPLXF(-0.0f, -2.0f)), -1.57079632679489661923f, 1);
    RCC_COMPLEX_API_BRANCH_DOUBLE(
        catan(CMPLX(0.0, 2.0)), 1.57079632679489661923, 0);
    RCC_COMPLEX_API_BRANCH_DOUBLE(
        catan(CMPLX(-0.0, 2.0)), -1.57079632679489661923, 0);
    RCC_COMPLEX_API_BRANCH_DOUBLE(
        catan(CMPLX(0.0, -2.0)), 1.57079632679489661923, 1);
    RCC_COMPLEX_API_BRANCH_DOUBLE(
        catan(CMPLX(-0.0, -2.0)), -1.57079632679489661923, 1);
    RCC_COMPLEX_API_BRANCH_LONG_DOUBLE(
        catanl(CMPLXL(0.0L, 2.0L)), 1.57079632679489661923L, 0);
    RCC_COMPLEX_API_BRANCH_LONG_DOUBLE(
        catanl(CMPLXL(-0.0L, 2.0L)), -1.57079632679489661923L, 0);
    RCC_COMPLEX_API_BRANCH_LONG_DOUBLE(
        catanl(CMPLXL(0.0L, -2.0L)), 1.57079632679489661923L, 1);
    RCC_COMPLEX_API_BRANCH_LONG_DOUBLE(
        catanl(CMPLXL(-0.0L, -2.0L)), -1.57079632679489661923L, 1);
    RCC_COMPLEX_API_SIGNED_RANGE_FLOAT(
        casinhf(CMPLXF(0.0f, 2.0f)),
        1.3168f, 1.3171f, 0, 1.5707f, 1.5709f, 0);
    RCC_COMPLEX_API_SIGNED_RANGE_FLOAT(
        casinhf(CMPLXF(-0.0f, 2.0f)),
        1.3168f, 1.3171f, 1, 1.5707f, 1.5709f, 0);
    RCC_COMPLEX_API_SIGNED_RANGE_FLOAT(
        casinhf(CMPLXF(0.0f, -2.0f)),
        1.3168f, 1.3171f, 0, 1.5707f, 1.5709f, 1);
    RCC_COMPLEX_API_SIGNED_RANGE_FLOAT(
        casinhf(CMPLXF(-0.0f, -2.0f)),
        1.3168f, 1.3171f, 1, 1.5707f, 1.5709f, 1);
    RCC_COMPLEX_API_SIGNED_RANGE_DOUBLE(
        casinh(CMPLX(0.0, 2.0)),
        1.3168, 1.3171, 0, 1.5707, 1.5709, 0);
    RCC_COMPLEX_API_SIGNED_RANGE_DOUBLE(
        casinh(CMPLX(-0.0, 2.0)),
        1.3168, 1.3171, 1, 1.5707, 1.5709, 0);
    RCC_COMPLEX_API_SIGNED_RANGE_DOUBLE(
        casinh(CMPLX(0.0, -2.0)),
        1.3168, 1.3171, 0, 1.5707, 1.5709, 1);
    RCC_COMPLEX_API_SIGNED_RANGE_DOUBLE(
        casinh(CMPLX(-0.0, -2.0)),
        1.3168, 1.3171, 1, 1.5707, 1.5709, 1);
    RCC_COMPLEX_API_SIGNED_RANGE_LONG_DOUBLE(
        casinhl(CMPLXL(0.0L, 2.0L)),
        1.3168L, 1.3171L, 0, 1.5707L, 1.5709L, 0);
    RCC_COMPLEX_API_SIGNED_RANGE_LONG_DOUBLE(
        casinhl(CMPLXL(-0.0L, 2.0L)),
        1.3168L, 1.3171L, 1, 1.5707L, 1.5709L, 0);
    RCC_COMPLEX_API_SIGNED_RANGE_LONG_DOUBLE(
        casinhl(CMPLXL(0.0L, -2.0L)),
        1.3168L, 1.3171L, 0, 1.5707L, 1.5709L, 1);
    RCC_COMPLEX_API_SIGNED_RANGE_LONG_DOUBLE(
        casinhl(CMPLXL(-0.0L, -2.0L)),
        1.3168L, 1.3171L, 1, 1.5707L, 1.5709L, 1);
    RCC_COMPLEX_API_BRANCH_FLOAT(
        cacoshf(CMPLXF(0.5f, 0.0f)), 0.0f, 0);
    RCC_COMPLEX_API_BRANCH_FLOAT(
        cacoshf(CMPLXF(0.5f, -0.0f)), 0.0f, 1);
    RCC_COMPLEX_API_BRANCH_DOUBLE(
        cacosh(CMPLX(0.5, 0.0)), 0.0, 0);
    RCC_COMPLEX_API_BRANCH_DOUBLE(
        cacosh(CMPLX(0.5, -0.0)), 0.0, 1);
    RCC_COMPLEX_API_BRANCH_LONG_DOUBLE(
        cacoshl(CMPLXL(0.5L, 0.0L)), 0.0L, 0);
    RCC_COMPLEX_API_BRANCH_LONG_DOUBLE(
        cacoshl(CMPLXL(0.5L, -0.0L)), 0.0L, 1);
    RCC_COMPLEX_API_SIGNED_RANGE_FLOAT(
        cacoshf(CMPLXF(-2.0f, 0.0f)),
        1.3168f, 1.3171f, 0, 3.1414f, 3.1418f, 0);
    RCC_COMPLEX_API_SIGNED_RANGE_FLOAT(
        cacoshf(CMPLXF(-2.0f, -0.0f)),
        1.3168f, 1.3171f, 0, 3.1414f, 3.1418f, 1);
    RCC_COMPLEX_API_SIGNED_RANGE_DOUBLE(
        cacosh(CMPLX(-2.0, 0.0)),
        1.3168, 1.3171, 0, 3.1414, 3.1418, 0);
    RCC_COMPLEX_API_SIGNED_RANGE_DOUBLE(
        cacosh(CMPLX(-2.0, -0.0)),
        1.3168, 1.3171, 0, 3.1414, 3.1418, 1);
    RCC_COMPLEX_API_SIGNED_RANGE_LONG_DOUBLE(
        cacoshl(CMPLXL(-2.0L, 0.0L)),
        1.3168L, 1.3171L, 0, 3.1414L, 3.1418L, 0);
    RCC_COMPLEX_API_SIGNED_RANGE_LONG_DOUBLE(
        cacoshl(CMPLXL(-2.0L, -0.0L)),
        1.3168L, 1.3171L, 0, 3.1414L, 3.1418L, 1);
    RCC_COMPLEX_API_SIGNED_RANGE_FLOAT(
        catanhf(CMPLXF(-2.0f, 0.0f)),
        0.5492f, 0.5494f, 1, 1.5707f, 1.5709f, 0);
    RCC_COMPLEX_API_SIGNED_RANGE_FLOAT(
        catanhf(CMPLXF(-2.0f, -0.0f)),
        0.5492f, 0.5494f, 1, 1.5707f, 1.5709f, 1);
    RCC_COMPLEX_API_SIGNED_RANGE_DOUBLE(
        catanh(CMPLX(-2.0, 0.0)),
        0.5492, 0.5494, 1, 1.5707, 1.5709, 0);
    RCC_COMPLEX_API_SIGNED_RANGE_DOUBLE(
        catanh(CMPLX(-2.0, -0.0)),
        0.5492, 0.5494, 1, 1.5707, 1.5709, 1);
    RCC_COMPLEX_API_SIGNED_RANGE_LONG_DOUBLE(
        catanhl(CMPLXL(-2.0L, 0.0L)),
        0.5492L, 0.5494L, 1, 1.5707L, 1.5709L, 0);
    RCC_COMPLEX_API_SIGNED_RANGE_LONG_DOUBLE(
        catanhl(CMPLXL(-2.0L, -0.0L)),
        0.5492L, 0.5494L, 1, 1.5707L, 1.5709L, 1);
    RCC_COMPLEX_API_CHECK_FLOAT(
        csqrtf(CMPLXF(-4.0f, 0.0f)), 0.0f, 2.0f);
    RCC_COMPLEX_API_CHECK_FLOAT(
        csqrtf(CMPLXF(-4.0f, -0.0f)), 0.0f, -2.0f);
    RCC_COMPLEX_API_CHECK_DOUBLE(
        csqrt(CMPLX(-4.0, 0.0)), 0.0, 2.0);
    RCC_COMPLEX_API_CHECK_DOUBLE(
        csqrt(CMPLX(-4.0, -0.0)), 0.0, -2.0);
    RCC_COMPLEX_API_CHECK_LONG_DOUBLE(
        csqrtl(CMPLXL(-4.0L, 0.0L)), 0.0L, 2.0L);
    RCC_COMPLEX_API_CHECK_LONG_DOUBLE(
        csqrtl(CMPLXL(-4.0L, -0.0L)), 0.0L, -2.0L);
    {
        RccComplexFloat value_float =
            catanhf(CMPLXF(2.0f, 0.0f));
        RccComplexDouble value_double =
            catanh(CMPLX(2.0, 0.0));
        RccComplexLongDouble value_long_double =
            catanhl(CMPLXL(2.0L, 0.0L));
        if (__real__ value_float < 0.54920614f ||
            __real__ value_float > 0.54940614f ||
            (__imag__ value_float > -1.5f &&
             __imag__ value_float < 1.5f) ||
            __builtin_signbit(__imag__ value_float)) return __LINE__;
        if (__real__ value_double < 0.5493060443340548 ||
            __real__ value_double > 0.5493062443340548 ||
            (__imag__ value_double > -1.5 &&
             __imag__ value_double < 1.5) ||
            __builtin_signbit(__imag__ value_double)) return __LINE__;
        if ((double)__real__ value_long_double < 0.5493060443340548 ||
            (double)__real__ value_long_double > 0.5493062443340548 ||
            ((double)__imag__ value_long_double > -1.5 &&
             (double)__imag__ value_long_double < 1.5) ||
            __builtin_signbit(__imag__ value_long_double)) return __LINE__;
        value_float = catanhf(CMPLXF(2.0f, -0.0f));
        value_double = catanh(CMPLX(2.0, -0.0));
        value_long_double = catanhl(CMPLXL(2.0L, -0.0L));
        if (!__builtin_signbit(__imag__ value_float) ||
            !__builtin_signbit(__imag__ value_double) ||
            !__builtin_signbit(__imag__ value_long_double)) return __LINE__;
    }
    {
        RccComplexFloat value_float =
            cpowf(CMPLXF(-1.0f, 0.0f), CMPLXF(0.5f, 0.0f));
        RccComplexDouble value_double =
            cpow(CMPLX(-1.0, 0.0), CMPLX(0.5, 0.0));
        RccComplexLongDouble value_long_double =
            cpowl(CMPLXL(-1.0L, 0.0L), CMPLXL(0.5L, 0.0L));
        if (__real__ value_float < -0.0001f ||
            __real__ value_float > 0.0001f ||
            __imag__ value_float < 0.9999f ||
            __imag__ value_float > 1.0001f ||
            __builtin_signbit(__imag__ value_float)) return __LINE__;
        if (__real__ value_double < -0.0000001 ||
            __real__ value_double > 0.0000001 ||
            __imag__ value_double < 0.9999999 ||
            __imag__ value_double > 1.0000001 ||
            __builtin_signbit(__imag__ value_double)) return __LINE__;
        if ((double)__real__ value_long_double < -0.0000001 ||
            (double)__real__ value_long_double > 0.0000001 ||
            (double)__imag__ value_long_double < 0.9999999 ||
            (double)__imag__ value_long_double > 1.0000001 ||
            __builtin_signbit(__imag__ value_long_double)) return __LINE__;
        value_float = cpowf(CMPLXF(-1.0f, -0.0f), CMPLXF(0.5f, 0.0f));
        value_double = cpow(CMPLX(-1.0, -0.0), CMPLX(0.5, 0.0));
        value_long_double = cpowl(CMPLXL(-1.0L, -0.0L),
                                  CMPLXL(0.5L, 0.0L));
        if (!__builtin_signbit(__imag__ value_float) ||
            !__builtin_signbit(__imag__ value_double) ||
            !__builtin_signbit(__imag__ value_long_double)) return __LINE__;
    }
    {
        RccComplexFloat value_float;
        RccComplexDouble value_double;
        RccComplexLongDouble value_long_double;
        int endpoint, side;

        for (endpoint = 0; endpoint != 2; ++endpoint) {
            float endpoint_float = endpoint ? -1.0f : 1.0f;
            double endpoint_double = endpoint ? -1.0 : 1.0;
            long double endpoint_long_double = endpoint ? -1.0L : 1.0L;
            for (side = 0; side != 2; ++side) {
                float zero_float = side ? -0.0f : 0.0f;
                double zero_double = side ? -0.0 : 0.0;
                long double zero_long_double = side ? -0.0L : 0.0L;

                if (feclearexcept(FE_ALL_EXCEPT) != 0) return __LINE__;
                value_float = catanhf(CMPLXF(endpoint_float, zero_float));
                if (!__builtin_isinf(__real__ value_float) ||
                    !!__builtin_signbit(__real__ value_float) != endpoint ||
                    __imag__ value_float != 0.0f ||
                    !!__builtin_signbit(__imag__ value_float) != side ||
                    !(fetestexcept(FE_DIVBYZERO) & FE_DIVBYZERO) ||
                    (fetestexcept(FE_INVALID) & FE_INVALID)) return __LINE__;
                if (feclearexcept(FE_ALL_EXCEPT) != 0) return __LINE__;
                value_double = catanh(CMPLX(endpoint_double, zero_double));
                if (!__builtin_isinf(__real__ value_double) ||
                    !!__builtin_signbit(__real__ value_double) != endpoint ||
                    __imag__ value_double != 0.0 ||
                    !!__builtin_signbit(__imag__ value_double) != side ||
                    !(fetestexcept(FE_DIVBYZERO) & FE_DIVBYZERO) ||
                    (fetestexcept(FE_INVALID) & FE_INVALID)) return __LINE__;
                if (feclearexcept(FE_ALL_EXCEPT) != 0) return __LINE__;
                value_long_double = catanhl(
                    CMPLXL(endpoint_long_double, zero_long_double));
                if (!__builtin_isinf(__real__ value_long_double) ||
                    !!__builtin_signbit(__real__ value_long_double) != endpoint ||
                    __imag__ value_long_double != 0.0L ||
                    !!__builtin_signbit(__imag__ value_long_double) != side ||
                    !(fetestexcept(FE_DIVBYZERO) & FE_DIVBYZERO) ||
                    (fetestexcept(FE_INVALID) & FE_INVALID)) return __LINE__;

                if (feclearexcept(FE_ALL_EXCEPT) != 0) return __LINE__;
                value_float = catanf(CMPLXF(zero_float, endpoint_float));
                if (__real__ value_float != 0.0f ||
                    !!__builtin_signbit(__real__ value_float) != side ||
                    !__builtin_isinf(__imag__ value_float) ||
                    !!__builtin_signbit(__imag__ value_float) != endpoint ||
                    !(fetestexcept(FE_DIVBYZERO) & FE_DIVBYZERO) ||
                    (fetestexcept(FE_INVALID) & FE_INVALID)) return __LINE__;
                if (feclearexcept(FE_ALL_EXCEPT) != 0) return __LINE__;
                value_double = catan(CMPLX(zero_double, endpoint_double));
                if (__real__ value_double != 0.0 ||
                    !!__builtin_signbit(__real__ value_double) != side ||
                    !__builtin_isinf(__imag__ value_double) ||
                    !!__builtin_signbit(__imag__ value_double) != endpoint ||
                    !(fetestexcept(FE_DIVBYZERO) & FE_DIVBYZERO) ||
                    (fetestexcept(FE_INVALID) & FE_INVALID)) return __LINE__;
                if (feclearexcept(FE_ALL_EXCEPT) != 0) return __LINE__;
                value_long_double = catanl(
                    CMPLXL(zero_long_double, endpoint_long_double));
                if (__real__ value_long_double != 0.0L ||
                    !!__builtin_signbit(__real__ value_long_double) != side ||
                    !__builtin_isinf(__imag__ value_long_double) ||
                    !!__builtin_signbit(__imag__ value_long_double) != endpoint ||
                    !(fetestexcept(FE_DIVBYZERO) & FE_DIVBYZERO) ||
                    (fetestexcept(FE_INVALID) & FE_INVALID)) return __LINE__;
            }
        }

        value_float = catanf(
            CMPLXF(__builtin_nanf(""), __builtin_huge_valf()));
        value_double = catan(
            CMPLX(__builtin_nan(""), __builtin_huge_val()));
        value_long_double = catanl(
            CMPLXL((long double)__builtin_nan(""),
                   (long double)__builtin_huge_val()));
        if (!__builtin_isnan(__real__ value_float) ||
            __imag__ value_float != 0.0f ||
            !__builtin_isnan(__real__ value_double) ||
            __imag__ value_double != 0.0 ||
            !__builtin_isnan(__real__ value_long_double) ||
            __imag__ value_long_double != 0.0L) return __LINE__;
        if (__builtin_signbit(__imag__ value_float) ||
            __builtin_signbit(__imag__ value_double) ||
            __builtin_signbit(__imag__ value_long_double)) return __LINE__;

        value_float = catanf(
            CMPLXF(__builtin_nanf(""), -__builtin_huge_valf()));
        value_double = catan(
            CMPLX(__builtin_nan(""), -__builtin_huge_val()));
        value_long_double = catanl(
            CMPLXL((long double)__builtin_nan(""),
                   -(long double)__builtin_huge_val()));
        if (!__builtin_isnan(__real__ value_float) ||
            __imag__ value_float != 0.0f ||
            !__builtin_isnan(__real__ value_double) ||
            __imag__ value_double != 0.0 ||
            !__builtin_isnan(__real__ value_long_double) ||
            __imag__ value_long_double != 0.0L) return __LINE__;
        if (!__builtin_signbit(__imag__ value_float) ||
            !__builtin_signbit(__imag__ value_double) ||
            !__builtin_signbit(__imag__ value_long_double)) return __LINE__;

        value_float = catanf(CMPLXF(__builtin_nanf(""), 0.0f));
        value_double = catan(CMPLX(__builtin_nan(""), 0.0));
        value_long_double = catanl(
            CMPLXL((long double)__builtin_nan(""), 0.0L));
        if (!__builtin_isnan(__real__ value_float) ||
            __imag__ value_float != 0.0f ||
            __builtin_signbit(__imag__ value_float) ||
            !__builtin_isnan(__real__ value_double) ||
            __imag__ value_double != 0.0 ||
            __builtin_signbit(__imag__ value_double) ||
            !__builtin_isnan(__real__ value_long_double) ||
            __imag__ value_long_double != 0.0L ||
            __builtin_signbit(__imag__ value_long_double)) return __LINE__;

        value_float = catanf(CMPLXF(__builtin_nanf(""), -0.0f));
        value_double = catan(CMPLX(__builtin_nan(""), -0.0));
        value_long_double = catanl(
            CMPLXL((long double)__builtin_nan(""), -0.0L));
        if (!__builtin_isnan(__real__ value_float) ||
            __imag__ value_float != 0.0f ||
            !__builtin_signbit(__imag__ value_float) ||
            !__builtin_isnan(__real__ value_double) ||
            __imag__ value_double != 0.0 ||
            !__builtin_signbit(__imag__ value_double) ||
            !__builtin_isnan(__real__ value_long_double) ||
            __imag__ value_long_double != 0.0L ||
            !__builtin_signbit(__imag__ value_long_double)) return __LINE__;

        value_float = cacosf(CMPLXF(0.0f, __builtin_nanf("")));
        value_double = cacos(CMPLX(0.0, __builtin_nan("")));
        value_long_double = cacosl(
            CMPLXL(0.0L, (long double)__builtin_nan("")));
        if (__real__ value_float < 1.5707f ||
            __real__ value_float > 1.5709f ||
            !__builtin_isnan(__imag__ value_float) ||
            __real__ value_double < 1.5707 ||
            __real__ value_double > 1.5709 ||
            !__builtin_isnan(__imag__ value_double) ||
            __real__ value_long_double < 1.5707L ||
            __real__ value_long_double > 1.5709L ||
            !__builtin_isnan(__imag__ value_long_double)) return __LINE__;

        value_float = cacosf(CMPLXF(
            __builtin_nanf(""), __builtin_huge_valf()));
        value_double = cacos(CMPLX(
            __builtin_nan(""), __builtin_huge_val()));
        value_long_double = cacosl(CMPLXL(
            (long double)__builtin_nan(""),
            (long double)__builtin_huge_val()));
        if (!__builtin_isnan(__real__ value_float) ||
            !__builtin_isinf(__imag__ value_float) ||
            !__builtin_signbit(__imag__ value_float) ||
            !__builtin_isnan(__real__ value_double) ||
            !__builtin_isinf(__imag__ value_double) ||
            !__builtin_signbit(__imag__ value_double) ||
            !__builtin_isnan(__real__ value_long_double) ||
            !__builtin_isinf(__imag__ value_long_double) ||
            !__builtin_signbit(__imag__ value_long_double)) return __LINE__;

        value_float = cexpf(CMPLXF(
            __builtin_huge_valf(), __builtin_nanf("")));
        value_double = cexp(CMPLX(
            __builtin_huge_val(), __builtin_nan("")));
        value_long_double = cexpl(CMPLXL(
            (long double)__builtin_huge_val(),
            (long double)__builtin_nan("")));
        if (!__builtin_isinf(__real__ value_float) ||
            !__builtin_isnan(__imag__ value_float) ||
            !__builtin_isinf(__real__ value_double) ||
            !__builtin_isnan(__imag__ value_double) ||
            !__builtin_isinf(__real__ value_long_double) ||
            !__builtin_isnan(__imag__ value_long_double)) return __LINE__;

        value_float = clogf(CMPLXF(
            __builtin_nanf(""), __builtin_huge_valf()));
        value_double = clog(CMPLX(
            __builtin_nan(""), __builtin_huge_val()));
        value_long_double = clogl(CMPLXL(
            (long double)__builtin_nan(""),
            (long double)__builtin_huge_val()));
        if (!__builtin_isinf(__real__ value_float) ||
            __real__ value_float < 0.0f ||
            !__builtin_isnan(__imag__ value_float) ||
            !__builtin_isinf(__real__ value_double) ||
            __real__ value_double < 0.0 ||
            !__builtin_isnan(__imag__ value_double) ||
            !__builtin_isinf(__real__ value_long_double) ||
            __real__ value_long_double < 0.0L ||
            !__builtin_isnan(__imag__ value_long_double)) return __LINE__;

        value_float = csqrtf(CMPLXF(
            __builtin_nanf(""), __builtin_huge_valf()));
        value_double = csqrt(CMPLX(
            __builtin_nan(""), __builtin_huge_val()));
        value_long_double = csqrtl(CMPLXL(
            (long double)__builtin_nan(""),
            (long double)__builtin_huge_val()));
        if (!__builtin_isinf(__real__ value_float) ||
            __real__ value_float < 0.0f ||
            !__builtin_isinf(__imag__ value_float) ||
            __imag__ value_float < 0.0f ||
            !__builtin_isinf(__real__ value_double) ||
            __real__ value_double < 0.0 ||
            !__builtin_isinf(__imag__ value_double) ||
            __imag__ value_double < 0.0 ||
            !__builtin_isinf(__real__ value_long_double) ||
            __real__ value_long_double < 0.0L ||
            !__builtin_isinf(__imag__ value_long_double) ||
            __imag__ value_long_double < 0.0L) return __LINE__;

        value_float = casinhf(CMPLXF(0.0f, __builtin_nanf("")));
        value_double = casinh(CMPLX(0.0, __builtin_nan("")));
        value_long_double = casinhl(
            CMPLXL(0.0L, (long double)__builtin_nan("")));
        if (!__builtin_isnan(__real__ value_float) ||
            !__builtin_isnan(__imag__ value_float) ||
            !__builtin_isnan(__real__ value_double) ||
            !__builtin_isnan(__imag__ value_double) ||
            !__builtin_isnan(__real__ value_long_double) ||
            !__builtin_isnan(__imag__ value_long_double)) return __LINE__;

        value_float = cacoshf(CMPLXF(0.0f, __builtin_nanf("")));
        value_double = cacosh(CMPLX(0.0, __builtin_nan("")));
        value_long_double = cacoshl(
            CMPLXL(0.0L, (long double)__builtin_nan("")));
        if (!__builtin_isnan(__real__ value_float) ||
            !((__imag__ value_float > 1.5707f &&
               __imag__ value_float < 1.5709f) ||
              (__imag__ value_float < -1.5707f &&
               __imag__ value_float > -1.5709f)) ||
            !__builtin_isnan(__real__ value_double) ||
            !((__imag__ value_double > 1.5707 &&
               __imag__ value_double < 1.5709) ||
              (__imag__ value_double < -1.5707 &&
               __imag__ value_double > -1.5709)) ||
            !__builtin_isnan(__real__ value_long_double) ||
            !((__imag__ value_long_double > 1.5707L &&
               __imag__ value_long_double < 1.5709L) ||
              (__imag__ value_long_double < -1.5707L &&
               __imag__ value_long_double > -1.5709L))) return __LINE__;

        value_float = ctanhf(CMPLXF(0.0f, __builtin_huge_valf()));
        value_double = ctanh(CMPLX(0.0, __builtin_huge_val()));
        value_long_double = ctanhl(
            CMPLXL(0.0L, (long double)__builtin_huge_val()));
        if (__real__ value_float != 0.0f ||
            !__builtin_isnan(__imag__ value_float) ||
            __real__ value_double != 0.0 ||
            !__builtin_isnan(__imag__ value_double) ||
            __real__ value_long_double != 0.0L ||
            !__builtin_isnan(__imag__ value_long_double)) return __LINE__;

        value_float = ctanhf(CMPLXF(0.0f, __builtin_nanf("")));
        value_double = ctanh(CMPLX(0.0, __builtin_nan("")));
        value_long_double = ctanhl(
            CMPLXL(0.0L, (long double)__builtin_nan("")));
        if (__real__ value_float != 0.0f ||
            !__builtin_isnan(__imag__ value_float) ||
            __real__ value_double != 0.0 ||
            !__builtin_isnan(__imag__ value_double) ||
            __real__ value_long_double != 0.0L ||
            !__builtin_isnan(__imag__ value_long_double)) return __LINE__;
    }

#undef RCC_COMPLEX_API_CHECK_FLOAT
#undef RCC_COMPLEX_API_CHECK_DOUBLE
#undef RCC_COMPLEX_API_CHECK_LONG_DOUBLE
#undef RCC_COMPLEX_API_BRANCH_FLOAT
#undef RCC_COMPLEX_API_BRANCH_DOUBLE
#undef RCC_COMPLEX_API_BRANCH_LONG_DOUBLE
#undef RCC_COMPLEX_API_PROJECTION_FLOAT
#undef RCC_COMPLEX_API_PROJECTION_DOUBLE
#undef RCC_COMPLEX_API_PROJECTION_LONG_DOUBLE
#undef RCC_COMPLEX_API_SIGNED_RANGE_FLOAT
#undef RCC_COMPLEX_API_SIGNED_RANGE_DOUBLE
#undef RCC_COMPLEX_API_SIGNED_RANGE_LONG_DOUBLE
    return 0;
}
