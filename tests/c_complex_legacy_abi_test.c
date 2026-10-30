#include <assert.h>
#include <fenv.h>
#include <math.h>
#include <pthread.h>

typedef float _Complex RccLegacyComplexFloat;
typedef double _Complex RccLegacyComplexDouble;
typedef struct RccLegacyComplexBinary64Pair {
    double real;
    double imaginary;
} RccLegacyComplexBinary64Pair;
typedef long double _Complex RccLegacyHostLongDoubleComplex;

extern long double rcc_host_native_creall(RccLegacyHostLongDoubleComplex);
extern long double rcc_host_native_cimagl(RccLegacyHostLongDoubleComplex);
extern long double rcc_host_native_cabsl(RccLegacyHostLongDoubleComplex);
extern long double rcc_host_native_cargl(RccLegacyHostLongDoubleComplex);
extern RccLegacyHostLongDoubleComplex rcc_host_native_conjl(
    RccLegacyHostLongDoubleComplex);
extern RccLegacyHostLongDoubleComplex rcc_host_native_csqrtl(
    RccLegacyHostLongDoubleComplex);

static RccLegacyHostLongDoubleComplex rcc_legacy_pair_to_host(
    RccLegacyComplexBinary64Pair value) {
    return __builtin_complex((long double)value.real,
                             (long double)value.imaginary);
}

static RccLegacyComplexBinary64Pair rcc_legacy_host_to_pair(
    RccLegacyHostLongDoubleComplex value) {
    RccLegacyComplexBinary64Pair result;
    result.real = (double)__real__ value;
    result.imaginary = (double)__imag__ value;
    return result;
}

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
extern RccLegacyComplexFloat RINOS_ABI rcc_legacy_complex_float_add_assign(
    RccLegacyComplexFloat, RccLegacyComplexFloat);
extern RccLegacyComplexFloat RINOS_ABI
rcc_legacy_complex_float_subtract_assign(
    RccLegacyComplexFloat, RccLegacyComplexFloat);
extern RccLegacyComplexFloat RINOS_ABI
rcc_legacy_complex_float_multiply_assign(
    RccLegacyComplexFloat, RccLegacyComplexFloat);
extern RccLegacyComplexFloat RINOS_ABI
rcc_legacy_complex_float_divide_assign(
    RccLegacyComplexFloat, RccLegacyComplexFloat);
extern RccLegacyComplexDouble RINOS_ABI
rcc_legacy_complex_double_add_assign(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern RccLegacyComplexDouble RINOS_ABI
rcc_legacy_complex_double_subtract_assign(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern RccLegacyComplexDouble RINOS_ABI
rcc_legacy_complex_double_multiply_assign(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern RccLegacyComplexDouble RINOS_ABI
rcc_legacy_complex_double_divide_assign(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern RccLegacyComplexFloat RINOS_ABI
rcc_legacy_complex_float_prefix_increment(RccLegacyComplexFloat);
extern RccLegacyComplexFloat RINOS_ABI
rcc_legacy_complex_float_postfix_decrement(RccLegacyComplexFloat);
extern RccLegacyComplexFloat RINOS_ABI
rcc_legacy_complex_float_prefix_decrement(RccLegacyComplexFloat);
extern RccLegacyComplexFloat RINOS_ABI
rcc_legacy_complex_float_postfix_increment(RccLegacyComplexFloat);
extern RccLegacyComplexDouble RINOS_ABI
rcc_legacy_complex_double_prefix_increment(RccLegacyComplexDouble);
extern RccLegacyComplexDouble RINOS_ABI
rcc_legacy_complex_double_postfix_decrement(RccLegacyComplexDouble);
extern RccLegacyComplexDouble RINOS_ABI
rcc_legacy_complex_double_prefix_decrement(RccLegacyComplexDouble);
extern RccLegacyComplexDouble RINOS_ABI
rcc_legacy_complex_double_postfix_increment(RccLegacyComplexDouble);
extern int RINOS_ABI rcc_legacy_complex_float_equal(
    RccLegacyComplexFloat, RccLegacyComplexFloat);
extern int RINOS_ABI rcc_legacy_complex_float_not_equal(
    RccLegacyComplexFloat, RccLegacyComplexFloat);
extern int RINOS_ABI rcc_legacy_complex_double_equal(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern int RINOS_ABI rcc_legacy_complex_double_not_equal(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern int RINOS_ABI rcc_legacy_complex_float_truth(RccLegacyComplexFloat);
extern int RINOS_ABI rcc_legacy_complex_double_truth(RccLegacyComplexDouble);
extern float RINOS_ABI rcc_legacy_complex_float_real(
    RccLegacyComplexFloat);
extern double RINOS_ABI rcc_legacy_complex_double_real(
    RccLegacyComplexDouble);
extern RccLegacyComplexFloat RINOS_ABI rcc_legacy_complex_double_narrow(
    RccLegacyComplexDouble);
extern int RINOS_ABI rcc_legacy_complex_float_equal_real(
    RccLegacyComplexFloat, float);
extern int RINOS_ABI rcc_legacy_complex_float_not_equal_real(
    RccLegacyComplexFloat, float);
extern int RINOS_ABI rcc_legacy_complex_float_to_int(
    RccLegacyComplexFloat);
extern _Bool RINOS_ABI rcc_legacy_complex_float_to_bool(
    RccLegacyComplexFloat);
extern RccLegacyComplexFloat RINOS_ABI
rcc_legacy_complex_float_add_assign_once(
    RccLegacyComplexFloat*, RccLegacyComplexFloat);
extern RccLegacyComplexFloat RINOS_ABI
rcc_legacy_complex_float_increment_once(RccLegacyComplexFloat*);
extern RccLegacyComplexFloat RINOS_ABI
rcc_legacy_complex_float_postfix_increment_once(RccLegacyComplexFloat*);
extern int RINOS_ABI rcc_legacy_complex_lvalue_evaluation_count(void);
extern _Thread_local RccLegacyComplexDouble rcc_legacy_complex_tls;
extern RccLegacyComplexDouble RINOS_ABI rcc_legacy_complex_tls_read(void);
extern float RINOS_ABI rcc_legacy_complex_api_crealf(
    RccLegacyComplexFloat);
extern float RINOS_ABI rcc_legacy_complex_api_cimagf(
    RccLegacyComplexFloat);
extern float RINOS_ABI rcc_legacy_complex_api_cabsf(
    RccLegacyComplexFloat);
extern float RINOS_ABI rcc_legacy_complex_api_cargf(
    RccLegacyComplexFloat);
extern RccLegacyComplexFloat RINOS_ABI rcc_legacy_complex_api_conjf(
    RccLegacyComplexFloat);
extern RccLegacyComplexFloat RINOS_ABI rcc_legacy_complex_api_csqrtf(
    RccLegacyComplexFloat);
extern double RINOS_ABI rcc_legacy_complex_api_creal(
    RccLegacyComplexDouble);
extern double RINOS_ABI rcc_legacy_complex_api_cimag(
    RccLegacyComplexDouble);
extern double RINOS_ABI rcc_legacy_complex_api_cabs(
    RccLegacyComplexDouble);
extern double RINOS_ABI rcc_legacy_complex_api_carg(
    RccLegacyComplexDouble);
extern RccLegacyComplexDouble RINOS_ABI rcc_legacy_complex_api_conj(
    RccLegacyComplexDouble);
extern RccLegacyComplexDouble RINOS_ABI rcc_legacy_complex_api_csqrt(
    RccLegacyComplexDouble);
extern double RINOS_ABI rcc_legacy_complex_api_creall(
    RccLegacyComplexBinary64Pair);
extern double RINOS_ABI rcc_legacy_complex_api_cimagl(
    RccLegacyComplexBinary64Pair);
extern double RINOS_ABI rcc_legacy_complex_api_cabsl(
    RccLegacyComplexBinary64Pair);
extern double RINOS_ABI rcc_legacy_complex_api_cargl(
    RccLegacyComplexBinary64Pair);
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_api_conjl(RccLegacyComplexBinary64Pair);
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_api_csqrtl(RccLegacyComplexBinary64Pair);

double RINOS_ABI rcc_legacy_bridge_creall(
    RccLegacyComplexBinary64Pair value) __asm__("creall");
double RINOS_ABI rcc_legacy_bridge_creall(
    RccLegacyComplexBinary64Pair value) {
    return (double)rcc_host_native_creall(rcc_legacy_pair_to_host(value));
}

double RINOS_ABI rcc_legacy_bridge_cimagl(
    RccLegacyComplexBinary64Pair value) __asm__("cimagl");
double RINOS_ABI rcc_legacy_bridge_cimagl(
    RccLegacyComplexBinary64Pair value) {
    return (double)rcc_host_native_cimagl(rcc_legacy_pair_to_host(value));
}

double RINOS_ABI rcc_legacy_bridge_cabsl(
    RccLegacyComplexBinary64Pair value) __asm__("cabsl");
double RINOS_ABI rcc_legacy_bridge_cabsl(
    RccLegacyComplexBinary64Pair value) {
    return (double)rcc_host_native_cabsl(rcc_legacy_pair_to_host(value));
}

double RINOS_ABI rcc_legacy_bridge_cargl(
    RccLegacyComplexBinary64Pair value) __asm__("cargl");
double RINOS_ABI rcc_legacy_bridge_cargl(
    RccLegacyComplexBinary64Pair value) {
    return (double)rcc_host_native_cargl(rcc_legacy_pair_to_host(value));
}

RccLegacyComplexBinary64Pair RINOS_ABI rcc_legacy_bridge_conjl(
    RccLegacyComplexBinary64Pair value) __asm__("conjl");
RccLegacyComplexBinary64Pair RINOS_ABI rcc_legacy_bridge_conjl(
    RccLegacyComplexBinary64Pair value) {
    return rcc_legacy_host_to_pair(
        rcc_host_native_conjl(rcc_legacy_pair_to_host(value)));
}

RccLegacyComplexBinary64Pair RINOS_ABI rcc_legacy_bridge_csqrtl(
    RccLegacyComplexBinary64Pair value) __asm__("csqrtl");
RccLegacyComplexBinary64Pair RINOS_ABI rcc_legacy_bridge_csqrtl(
    RccLegacyComplexBinary64Pair value) {
    return rcc_legacy_host_to_pair(
        rcc_host_native_csqrtl(rcc_legacy_pair_to_host(value)));
}

typedef struct RccLegacyComplexTlsCase {
    int failed;
} RccLegacyComplexTlsCase;

static void* rcc_legacy_complex_tls_worker(void* opaque) {
    RccLegacyComplexTlsCase* test_case =
        (RccLegacyComplexTlsCase*)opaque;
    RccLegacyComplexDouble value = rcc_legacy_complex_tls_read();
    if (fabs(__real__ value - 9.0) > 0.0000001 ||
        fabs(__imag__ value - 10.0) > 0.0000001) {
        test_case->failed = 1;
        return NULL;
    }
    __real__ rcc_legacy_complex_tls = -4.0;
    __imag__ rcc_legacy_complex_tls = 8.0;
    value = rcc_legacy_complex_tls_read();
    if (fabs(__real__ value + 4.0) > 0.0000001 ||
        fabs(__imag__ value - 8.0) > 0.0000001) {
        test_case->failed = 1;
    }
    return NULL;
}

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
    pthread_t tls_thread;
    RccLegacyComplexTlsCase tls_case = {0};
    RccLegacyComplexFloat float_api_input = __builtin_complex(3.0f, 4.0f);
    RccLegacyComplexDouble double_api_input = __builtin_complex(3.0, 4.0);
    RccLegacyComplexFloat float_left = __builtin_complex(2.0f, 3.0f);
    RccLegacyComplexFloat float_right = __builtin_complex(4.0f, -1.0f);
    RccLegacyComplexFloat float_result;
    RccLegacyComplexDouble double_left = __builtin_complex(2.0, 3.0);
    RccLegacyComplexDouble double_right = __builtin_complex(4.0, -1.0);
    RccLegacyComplexDouble double_result;

    double_result = rcc_legacy_complex_tls_read();
    assert(fabs(__real__ double_result - 9.0) < 0.0000001 &&
           fabs(__imag__ double_result - 10.0) < 0.0000001);
    __real__ rcc_legacy_complex_tls = 2.0;
    __imag__ rcc_legacy_complex_tls = -3.0;
    assert(pthread_create(&tls_thread, NULL, rcc_legacy_complex_tls_worker,
                          &tls_case) == 0);
    assert(pthread_join(tls_thread, NULL) == 0);
    assert(!tls_case.failed);
    double_result = rcc_legacy_complex_tls_read();
    assert(fabs(__real__ double_result - 2.0) < 0.0000001 &&
           fabs(__imag__ double_result + 3.0) < 0.0000001);

    assert(near_float(rcc_legacy_complex_api_crealf(float_api_input), 3.0f));
    assert(near_float(rcc_legacy_complex_api_cimagf(float_api_input), 4.0f));
    assert(near_float(rcc_legacy_complex_api_cabsf(float_api_input), 5.0f));
    assert(near_float(rcc_legacy_complex_api_cargf(float_api_input),
                      (float)atan2(4.0, 3.0)));
    float_result = rcc_legacy_complex_api_conjf(float_api_input);
    assert(near_float(__real__ float_result, 3.0f) &&
           near_float(__imag__ float_result, -4.0f));
    float_result = rcc_legacy_complex_api_csqrtf(float_api_input);
    assert(near_float(__real__ float_result, 2.0f) &&
           near_float(__imag__ float_result, 1.0f));
    assert(near_double(rcc_legacy_complex_api_creal(double_api_input), 3.0));
    assert(near_double(rcc_legacy_complex_api_cimag(double_api_input), 4.0));
    assert(near_double(rcc_legacy_complex_api_cabs(double_api_input), 5.0));
    assert(near_double(rcc_legacy_complex_api_carg(double_api_input),
                       atan2(4.0, 3.0)));
    double_result = rcc_legacy_complex_api_conj(double_api_input);
    assert(near_double(__real__ double_result, 3.0) &&
           near_double(__imag__ double_result, -4.0));
    double_result = rcc_legacy_complex_api_csqrt(double_api_input);
    assert(near_double(__real__ double_result, 2.0) &&
           near_double(__imag__ double_result, 1.0));

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
    float_result = rcc_legacy_complex_float_multiply(
        __builtin_complex(HUGE_VALF, HUGE_VALF),
        __builtin_complex(1.0f, 0.0f));
    assert(isinf(__real__ float_result) && isinf(__imag__ float_result));
    float_result = rcc_legacy_complex_float_divide(
        float_left, __builtin_complex(1.0f, 1.0f));
    assert(near_float(__real__ float_result, 2.5f) &&
           near_float(__imag__ float_result, 0.5f));
    assert(feclearexcept(FE_ALL_EXCEPT) == 0);
    float_result = rcc_legacy_complex_float_divide(
        float_left, __builtin_complex(0.0f, 0.0f));
    assert(isinf(__real__ float_result) && isinf(__imag__ float_result));
    assert((fetestexcept(FE_ALL_EXCEPT) & FE_DIVBYZERO) != 0);
    assert(feclearexcept(FE_ALL_EXCEPT) == 0);
    float_result = rcc_legacy_complex_float_divide(
        __builtin_complex(0.0f, 0.0f), __builtin_complex(0.0f, 0.0f));
    assert(isnan(__real__ float_result) && isnan(__imag__ float_result));
    assert((fetestexcept(FE_ALL_EXCEPT) & (FE_DIVBYZERO | FE_INVALID)) ==
           (FE_DIVBYZERO | FE_INVALID));
    float_result = rcc_legacy_complex_float_divide(
        __builtin_complex(1.0f, 2.0f),
        __builtin_complex(HUGE_VALF, HUGE_VALF));
    assert(__real__ float_result == 0.0f && __imag__ float_result == 0.0f);
    float_result = rcc_legacy_complex_float_divide(
        __builtin_complex(HUGE_VALF, HUGE_VALF),
        __builtin_complex(1.0f, 0.0f));
    assert(isinf(__real__ float_result) && isinf(__imag__ float_result));
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
    double_result = rcc_legacy_complex_double_multiply(
        __builtin_complex(HUGE_VAL, HUGE_VAL),
        __builtin_complex(1.0, 0.0));
    assert(isinf(__real__ double_result) && isinf(__imag__ double_result));
    double_result = rcc_legacy_complex_double_divide(
        double_left, __builtin_complex(1.0, 1.0));
    assert(near_double(__real__ double_result, 2.5) &&
           near_double(__imag__ double_result, 0.5));
    assert(feclearexcept(FE_ALL_EXCEPT) == 0);
    double_result = rcc_legacy_complex_double_divide(
        double_left, __builtin_complex(0.0, 0.0));
    assert(isinf(__real__ double_result) && isinf(__imag__ double_result));
    assert((fetestexcept(FE_ALL_EXCEPT) & FE_DIVBYZERO) != 0);
    assert(feclearexcept(FE_ALL_EXCEPT) == 0);
    double_result = rcc_legacy_complex_double_divide(
        __builtin_complex(0.0, 0.0), __builtin_complex(0.0, 0.0));
    assert(isnan(__real__ double_result) && isnan(__imag__ double_result));
    assert((fetestexcept(FE_ALL_EXCEPT) & (FE_DIVBYZERO | FE_INVALID)) ==
           (FE_DIVBYZERO | FE_INVALID));
    double_result = rcc_legacy_complex_double_divide(
        __builtin_complex(1.0, 2.0),
        __builtin_complex(HUGE_VAL, HUGE_VAL));
    assert(__real__ double_result == 0.0 && __imag__ double_result == 0.0);
    double_result = rcc_legacy_complex_double_divide(
        __builtin_complex(HUGE_VAL, HUGE_VAL),
        __builtin_complex(1.0, 0.0));
    assert(isinf(__real__ double_result) && isinf(__imag__ double_result));
    double_result = rcc_legacy_complex_double_add_integer(double_left, 3);
    assert(near_double(__real__ double_result, 5.0) &&
           near_double(__imag__ double_result, 3.0));
    float_result = rcc_legacy_complex_float_add_assign(
        float_left, float_right);
    assert(near_float(__real__ float_result, 6.0f) &&
           near_float(__imag__ float_result, 2.0f));
    float_result = rcc_legacy_complex_float_subtract_assign(
        float_left, float_right);
    assert(near_float(__real__ float_result, -2.0f) &&
           near_float(__imag__ float_result, 4.0f));
    float_result = rcc_legacy_complex_float_multiply_assign(
        float_left, float_right);
    assert(near_float(__real__ float_result, 11.0f) &&
           near_float(__imag__ float_result, 10.0f));
    float_result = rcc_legacy_complex_float_divide_assign(
        float_left, __builtin_complex(1.0f, 1.0f));
    assert(near_float(__real__ float_result, 2.5f) &&
           near_float(__imag__ float_result, 0.5f));
    double_result = rcc_legacy_complex_double_add_assign(
        double_left, double_right);
    assert(near_double(__real__ double_result, 6.0) &&
           near_double(__imag__ double_result, 2.0));
    double_result = rcc_legacy_complex_double_subtract_assign(
        double_left, double_right);
    assert(near_double(__real__ double_result, -2.0) &&
           near_double(__imag__ double_result, 4.0));
    double_result = rcc_legacy_complex_double_multiply_assign(
        double_left, double_right);
    assert(near_double(__real__ double_result, 11.0) &&
           near_double(__imag__ double_result, 10.0));
    double_result = rcc_legacy_complex_double_divide_assign(
        double_left, __builtin_complex(1.0, 1.0));
    assert(near_double(__real__ double_result, 2.5) &&
           near_double(__imag__ double_result, 0.5));
    float_result = rcc_legacy_complex_float_prefix_increment(float_left);
    assert(near_float(__real__ float_result, 3.0f) &&
           near_float(__imag__ float_result, 3.0f));
    float_result = rcc_legacy_complex_float_postfix_decrement(float_left);
    assert(near_float(__real__ float_result, 2.0f) &&
           near_float(__imag__ float_result, 3.0f));
    float_result = rcc_legacy_complex_float_prefix_decrement(float_left);
    assert(near_float(__real__ float_result, 1.0f) &&
           near_float(__imag__ float_result, 3.0f));
    float_result = rcc_legacy_complex_float_postfix_increment(float_result);
    assert(near_float(__real__ float_result, 1.0f) &&
           near_float(__imag__ float_result, 3.0f));
    double_result = rcc_legacy_complex_double_prefix_increment(double_left);
    assert(near_double(__real__ double_result, 3.0) &&
           near_double(__imag__ double_result, 3.0));
    double_result = rcc_legacy_complex_double_postfix_decrement(double_left);
    assert(near_double(__real__ double_result, 2.0) &&
           near_double(__imag__ double_result, 3.0));
    double_result = rcc_legacy_complex_double_prefix_decrement(double_left);
    assert(near_double(__real__ double_result, 1.0) &&
           near_double(__imag__ double_result, 3.0));
    double_result = rcc_legacy_complex_double_postfix_increment(double_result);
    assert(near_double(__real__ double_result, 1.0) &&
           near_double(__imag__ double_result, 3.0));
    assert(rcc_legacy_complex_float_equal(float_left, float_left));
    assert(!rcc_legacy_complex_float_equal(float_left, float_right));
    assert(!rcc_legacy_complex_float_not_equal(float_left, float_left));
    assert(rcc_legacy_complex_float_not_equal(float_left, float_right));
    assert(rcc_legacy_complex_double_equal(double_left, double_left));
    assert(!rcc_legacy_complex_double_equal(double_left, double_right));
    assert(!rcc_legacy_complex_double_not_equal(double_left, double_left));
    assert(rcc_legacy_complex_double_not_equal(double_left, double_right));
    assert(!rcc_legacy_complex_float_equal(
        __builtin_complex(NAN, 0.0f), __builtin_complex(NAN, 0.0f)));
    assert(rcc_legacy_complex_float_not_equal(
        __builtin_complex(NAN, 0.0f), __builtin_complex(NAN, 0.0f)));
    assert(rcc_legacy_complex_float_truth(float_left));
    assert(!rcc_legacy_complex_float_truth(__builtin_complex(0.0f, 0.0f)));
    assert(rcc_legacy_complex_double_truth(double_left));
    assert(!rcc_legacy_complex_double_truth(__builtin_complex(0.0, 0.0)));
    assert(rcc_legacy_complex_float_truth(
        __builtin_complex(0.0f, -1.0f)));
    assert(rcc_legacy_complex_float_truth(
        __builtin_complex(NAN, 0.0f)));
    assert(near_float(rcc_legacy_complex_float_real(float_left), 2.0f));
    assert(near_double(rcc_legacy_complex_double_real(double_left), 2.0));
    assert(rcc_legacy_complex_float_equal_real(
        __builtin_complex(2.0f, 0.0f), 2.0f));
    assert(!rcc_legacy_complex_float_equal_real(
        __builtin_complex(2.0f, 3.0f), 2.0f));
    assert(rcc_legacy_complex_float_not_equal_real(
        __builtin_complex(2.0f, 3.0f), 2.0f));
    assert(!rcc_legacy_complex_float_equal_real(
        __builtin_complex(NAN, 0.0f), NAN));
    assert(rcc_legacy_complex_float_not_equal_real(
        __builtin_complex(NAN, 0.0f), NAN));
    assert(rcc_legacy_complex_float_to_int(
        __builtin_complex(7.75f, 9.0f)) == 7);
    assert(!rcc_legacy_complex_float_to_bool(
        __builtin_complex(0.0f, 0.0f)));
    assert(rcc_legacy_complex_float_to_bool(
        __builtin_complex(0.0f, -1.0f)));
    float_result = rcc_legacy_complex_double_narrow(double_left);
    assert(near_float(__real__ float_result, 2.0f) &&
           near_float(__imag__ float_result, 3.0f));
    {
        RccLegacyComplexFloat once_value =
            __builtin_complex(2.0f, 3.0f);
        float_result = rcc_legacy_complex_float_add_assign_once(
            &once_value, __builtin_complex(1.0f, -1.0f));
        assert(near_float(__real__ float_result, 3.0f) &&
               near_float(__imag__ float_result, 2.0f));
        assert(near_float(__real__ once_value, 3.0f) &&
               near_float(__imag__ once_value, 2.0f));
        assert(rcc_legacy_complex_lvalue_evaluation_count() == 1);
        float_result = rcc_legacy_complex_float_increment_once(&once_value);
        assert(near_float(__real__ float_result, 4.0f) &&
               near_float(__imag__ float_result, 2.0f));
        assert(near_float(__real__ once_value, 4.0f) &&
               near_float(__imag__ once_value, 2.0f));
        assert(rcc_legacy_complex_lvalue_evaluation_count() == 2);
        float_result = rcc_legacy_complex_float_postfix_increment_once(
            &once_value);
        assert(near_float(__real__ float_result, 4.0f) &&
               near_float(__imag__ float_result, 2.0f));
        assert(near_float(__real__ once_value, 5.0f) &&
               near_float(__imag__ once_value, 2.0f));
        assert(rcc_legacy_complex_lvalue_evaluation_count() == 3);
    }
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
