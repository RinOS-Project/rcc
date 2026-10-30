#include <assert.h>
#include <math.h>

typedef float _Complex RccLegacyComplexFloat;
typedef double _Complex RccLegacyComplexDouble;
typedef struct RccLegacyComplexBinary64Pair {
    double real;
    double imaginary;
} RccLegacyComplexBinary64Pair;

#if defined(_WIN64)
#define RINOS_ABI __attribute__((sysv_abi))
#else
#define RINOS_ABI
#endif

extern RccLegacyComplexFloat RINOS_ABI rcc_legacy_complex_float_add(
    RccLegacyComplexFloat, RccLegacyComplexFloat);
extern RccLegacyComplexFloat RINOS_ABI rcc_legacy_complex_float_call(
    RccLegacyComplexFloat, RccLegacyComplexFloat);
extern RccLegacyComplexFloat RINOS_ABI rcc_legacy_complex_float_negate(
    RccLegacyComplexFloat);
extern RccLegacyComplexFloat RINOS_ABI rcc_legacy_complex_float_from_real(
    float);
extern RccLegacyComplexDouble RINOS_ABI rcc_legacy_complex_widen_float(
    RccLegacyComplexFloat);
extern RccLegacyComplexFloat RINOS_ABI rcc_legacy_complex_float_conditional(
    int, RccLegacyComplexFloat, RccLegacyComplexFloat);
extern RccLegacyComplexFloat RINOS_ABI rcc_legacy_complex_float_comma(
    RccLegacyComplexFloat, RccLegacyComplexFloat);
extern RccLegacyComplexFloat RINOS_ABI rcc_legacy_complex_float_assignment(
    RccLegacyComplexFloat, RccLegacyComplexFloat);
extern RccLegacyComplexFloat RINOS_ABI rcc_legacy_complex_float_subtract(
    RccLegacyComplexFloat, RccLegacyComplexFloat);
extern RccLegacyComplexFloat RINOS_ABI rcc_legacy_complex_float_multiply(
    RccLegacyComplexFloat, RccLegacyComplexFloat);
extern RccLegacyComplexFloat RINOS_ABI rcc_legacy_complex_float_divide(
    RccLegacyComplexFloat, RccLegacyComplexFloat);
extern RccLegacyComplexFloat RINOS_ABI rcc_legacy_complex_float_add_real(
    RccLegacyComplexFloat, float);
extern RccLegacyComplexDouble RINOS_ABI rcc_legacy_complex_double_add(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern RccLegacyComplexDouble RINOS_ABI rcc_legacy_complex_double_call(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern RccLegacyComplexDouble RINOS_ABI rcc_legacy_complex_double_subtract(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern RccLegacyComplexDouble RINOS_ABI rcc_legacy_complex_double_multiply(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern RccLegacyComplexDouble RINOS_ABI rcc_legacy_complex_double_divide(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern RccLegacyComplexDouble RINOS_ABI rcc_legacy_complex_double_add_integer(
    RccLegacyComplexDouble, int);
extern RccLegacyComplexDouble RINOS_ABI
rcc_legacy_complex_float_add_double(RccLegacyComplexFloat, double);
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_double_add_long_double(RccLegacyComplexDouble, double);

extern void rcc_host_complex_mul_f32(
    float, float, float, float, float*, float*);
extern void rcc_host_complex_mul_f64(
    double, double, double, double, double*, double*);
extern void rcc_host_complex_div_f32(
    float, float, float, float, float*, float*);
extern void rcc_host_complex_div_f64(
    double, double, double, double, double*, double*);

void RINOS_ABI __rin_cmul_f32(float a, float b, float c, float d,
                              float* real, float* imaginary) {
    rcc_host_complex_mul_f32(a, b, c, d, real, imaginary);
}

void RINOS_ABI __rin_cmul_f64(double a, double b, double c, double d,
                              double* real, double* imaginary) {
    rcc_host_complex_mul_f64(a, b, c, d, real, imaginary);
}

void RINOS_ABI __rin_cdiv_f32(float a, float b, float c, float d,
                              float* real, float* imaginary) {
    rcc_host_complex_div_f32(a, b, c, d, real, imaginary);
}

void RINOS_ABI __rin_cdiv_f64(double a, double b, double c, double d,
                              double* real, double* imaginary) {
    rcc_host_complex_div_f64(a, b, c, d, real, imaginary);
}

static int near_float(float actual, float expected) {
    return fabsf(actual - expected) < 0.0001f;
}

static int near_double(double actual, double expected) {
    return fabs(actual - expected) < 0.0000001;
}

int main(void) {
    RccLegacyComplexFloat float_left = __builtin_complex(2.0f, 3.0f);
    RccLegacyComplexFloat float_right = __builtin_complex(4.0f, -1.0f);
    RccLegacyComplexFloat float_result;
    RccLegacyComplexDouble double_left = __builtin_complex(2.0, 3.0);
    RccLegacyComplexDouble double_right = __builtin_complex(4.0, -1.0);
    RccLegacyComplexDouble double_result;

    float_result = rcc_legacy_complex_float_add(float_left, float_right);
    assert(near_float(__real__ float_result, 6.0f) &&
           near_float(__imag__ float_result, 2.0f));
    float_result = rcc_legacy_complex_float_call(float_left, float_right);
    assert(near_float(__real__ float_result, 6.0f) &&
           near_float(__imag__ float_result, 2.0f));
    float_result = rcc_legacy_complex_float_negate(float_left);
    assert(near_float(__real__ float_result, -2.0f) &&
           near_float(__imag__ float_result, -3.0f));
    float_result = rcc_legacy_complex_float_from_real(2.5f);
    assert(near_float(__real__ float_result, 2.5f) &&
           near_float(__imag__ float_result, 0.0f));
    float_result = rcc_legacy_complex_float_conditional(
        1, float_left, float_right);
    assert(near_float(__real__ float_result, 6.0f) &&
           near_float(__imag__ float_result, 2.0f));
    float_result = rcc_legacy_complex_float_conditional(
        0, float_left, float_right);
    assert(near_float(__real__ float_result, 2.0f) &&
           near_float(__imag__ float_result, -4.0f));
    float_result = rcc_legacy_complex_float_comma(float_left, float_right);
    assert(near_float(__real__ float_result, 4.0f) &&
           near_float(__imag__ float_result, -1.0f));
    float_result = rcc_legacy_complex_float_assignment(
        float_left, float_right);
    assert(near_float(__real__ float_result, 11.0f) &&
           near_float(__imag__ float_result, 10.0f));
    float_result = rcc_legacy_complex_float_subtract(float_left, float_right);
    assert(near_float(__real__ float_result, -2.0f) &&
           near_float(__imag__ float_result, 4.0f));
    float_result = rcc_legacy_complex_float_multiply(float_left, float_right);
    assert(near_float(__real__ float_result, 11.0f) &&
           near_float(__imag__ float_result, 10.0f));
    float_result = rcc_legacy_complex_float_divide(
        float_left, __builtin_complex(1.0f, 1.0f));
    assert(near_float(__real__ float_result, 2.5f) &&
           near_float(__imag__ float_result, 0.5f));
    float_result = rcc_legacy_complex_float_add_real(float_left, 2.0f);
    assert(near_float(__real__ float_result, 4.0f) &&
           near_float(__imag__ float_result, 3.0f));

    double_result = rcc_legacy_complex_double_add(double_left, double_right);
    assert(near_double(__real__ double_result, 6.0) &&
           near_double(__imag__ double_result, 2.0));
    double_result = rcc_legacy_complex_double_call(double_left, double_right);
    assert(near_double(__real__ double_result, 6.0) &&
           near_double(__imag__ double_result, 2.0));
    double_result = rcc_legacy_complex_widen_float(float_left);
    assert(near_double(__real__ double_result, 2.0) &&
           near_double(__imag__ double_result, 3.0));
    double_result = rcc_legacy_complex_double_subtract(
        double_left, double_right);
    assert(near_double(__real__ double_result, -2.0) &&
           near_double(__imag__ double_result, 4.0));
    double_result = rcc_legacy_complex_double_multiply(
        double_left, double_right);
    assert(near_double(__real__ double_result, 11.0) &&
           near_double(__imag__ double_result, 10.0));
    double_result = rcc_legacy_complex_double_divide(
        double_left, __builtin_complex(1.0, 1.0));
    assert(near_double(__real__ double_result, 2.5) &&
           near_double(__imag__ double_result, 0.5));
    double_result = rcc_legacy_complex_double_add_integer(double_left, 3);
    assert(near_double(__real__ double_result, 5.0) &&
           near_double(__imag__ double_result, 3.0));
    double_result = rcc_legacy_complex_float_add_double(float_left, 0.5);
    assert(near_double(__real__ double_result, 2.5) &&
           near_double(__imag__ double_result, 3.0));
    {
        RccLegacyComplexBinary64Pair wide_result =
            rcc_legacy_complex_double_add_long_double(double_left, 0.5);
        assert(near_double(wide_result.real, 2.5) &&
               near_double(wide_result.imaginary, 3.0));
    }
    return 0;
}
