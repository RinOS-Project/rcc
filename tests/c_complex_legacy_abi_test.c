#include <assert.h>
#include <fenv.h>
#include <float.h>
#include <math.h>
#include <pthread.h>

typedef float _Complex RccLegacyComplexFloat;
typedef double _Complex RccLegacyComplexDouble;
typedef struct RccLegacyComplexBinary64Pair {
    double real;
    double imaginary;
} RccLegacyComplexBinary64Pair;
typedef RccLegacyComplexBinary64Pair ComplexBinary64Pair;
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

#define RCC_DECLARE_HOST_COMPLEX_UNARY_API(name) \
    extern RccLegacyHostLongDoubleComplex rcc_host_native_##name##l( \
        RccLegacyHostLongDoubleComplex)
RCC_DECLARE_HOST_COMPLEX_UNARY_API(cacos);
RCC_DECLARE_HOST_COMPLEX_UNARY_API(casin);
RCC_DECLARE_HOST_COMPLEX_UNARY_API(catan);
RCC_DECLARE_HOST_COMPLEX_UNARY_API(ccos);
RCC_DECLARE_HOST_COMPLEX_UNARY_API(csin);
RCC_DECLARE_HOST_COMPLEX_UNARY_API(ctan);
RCC_DECLARE_HOST_COMPLEX_UNARY_API(cacosh);
RCC_DECLARE_HOST_COMPLEX_UNARY_API(casinh);
RCC_DECLARE_HOST_COMPLEX_UNARY_API(catanh);
RCC_DECLARE_HOST_COMPLEX_UNARY_API(ccosh);
RCC_DECLARE_HOST_COMPLEX_UNARY_API(csinh);
RCC_DECLARE_HOST_COMPLEX_UNARY_API(ctanh);
RCC_DECLARE_HOST_COMPLEX_UNARY_API(cexp);
RCC_DECLARE_HOST_COMPLEX_UNARY_API(clog);
RCC_DECLARE_HOST_COMPLEX_UNARY_API(cproj);
#undef RCC_DECLARE_HOST_COMPLEX_UNARY_API
extern RccLegacyHostLongDoubleComplex rcc_host_native_cpowl(
    RccLegacyHostLongDoubleComplex, RccLegacyHostLongDoubleComplex);

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
extern RccLegacyComplexDouble RINOS_ABI
rcc_legacy_complex_double_add_float(double, RccLegacyComplexFloat);
extern RccLegacyComplexDouble RINOS_ABI
rcc_legacy_complex_float_subtract_double(RccLegacyComplexFloat, double);
extern RccLegacyComplexDouble RINOS_ABI
rcc_legacy_complex_double_subtract_float(double, RccLegacyComplexFloat);
extern RccLegacyComplexDouble RINOS_ABI
rcc_legacy_complex_float_multiply_double(RccLegacyComplexFloat, double);
extern RccLegacyComplexDouble RINOS_ABI
rcc_legacy_complex_double_multiply_float(double, RccLegacyComplexFloat);
extern RccLegacyComplexDouble RINOS_ABI
rcc_legacy_complex_float_divide_double(RccLegacyComplexFloat, double);
extern RccLegacyComplexDouble RINOS_ABI
rcc_legacy_complex_double_divide_float(double, RccLegacyComplexFloat);
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_double_add_long_double(RccLegacyComplexDouble, double);
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_long_double_identity(RccLegacyComplexDouble);
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_long_double_add(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_long_double_subtract(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_long_double_multiply(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_long_double_divide(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_long_double_add_assign(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_long_double_multiply_assign(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_long_double_divide_assign(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_long_double_subtract_assign(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_long_double_negate(RccLegacyComplexDouble);
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_long_double_prefix_increment(RccLegacyComplexDouble);
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_long_double_postfix_increment(RccLegacyComplexDouble);
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_long_double_prefix_decrement(RccLegacyComplexDouble);
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_long_double_postfix_decrement(RccLegacyComplexDouble);
extern void RINOS_ABI
rcc_legacy_complex_long_double_prefix_increment_pointer(
    RccLegacyComplexBinary64Pair*, RccLegacyComplexBinary64Pair*);
extern void RINOS_ABI
rcc_legacy_complex_long_double_postfix_increment_pointer(
    RccLegacyComplexBinary64Pair*, RccLegacyComplexBinary64Pair*);
extern void RINOS_ABI
rcc_legacy_complex_long_double_prefix_decrement_pointer(
    RccLegacyComplexBinary64Pair*, RccLegacyComplexBinary64Pair*);
extern void RINOS_ABI
rcc_legacy_complex_long_double_postfix_decrement_pointer(
    RccLegacyComplexBinary64Pair*, RccLegacyComplexBinary64Pair*);
extern int RINOS_ABI rcc_legacy_complex_long_double_equal(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern int RINOS_ABI rcc_legacy_complex_long_double_not_equal(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern int RINOS_ABI rcc_legacy_complex_long_double_truth(
    RccLegacyComplexDouble);
extern int RINOS_ABI rcc_legacy_complex_long_double_to_int(
    RccLegacyComplexDouble);
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_long_double_from_real(double);
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
extern _Thread_local RccLegacyComplexBinary64Pair
    rcc_legacy_complex_long_double_tls;
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_long_double_tls_read(void);
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_long_double_static_read(void);
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
#define RCC_DECLARE_LEGACY_COMPLEX_UNARY_API(name) \
    extern RccLegacyComplexFloat RINOS_ABI \
    rcc_legacy_complex_api_##name##f(RccLegacyComplexFloat); \
    extern RccLegacyComplexDouble RINOS_ABI \
    rcc_legacy_complex_api_##name(RccLegacyComplexDouble); \
    extern RccLegacyComplexBinary64Pair RINOS_ABI \
    rcc_legacy_complex_api_##name##l(RccLegacyComplexBinary64Pair)
RCC_DECLARE_LEGACY_COMPLEX_UNARY_API(cacos);
RCC_DECLARE_LEGACY_COMPLEX_UNARY_API(casin);
RCC_DECLARE_LEGACY_COMPLEX_UNARY_API(catan);
RCC_DECLARE_LEGACY_COMPLEX_UNARY_API(ccos);
RCC_DECLARE_LEGACY_COMPLEX_UNARY_API(csin);
RCC_DECLARE_LEGACY_COMPLEX_UNARY_API(ctan);
RCC_DECLARE_LEGACY_COMPLEX_UNARY_API(cacosh);
RCC_DECLARE_LEGACY_COMPLEX_UNARY_API(casinh);
RCC_DECLARE_LEGACY_COMPLEX_UNARY_API(catanh);
RCC_DECLARE_LEGACY_COMPLEX_UNARY_API(ccosh);
RCC_DECLARE_LEGACY_COMPLEX_UNARY_API(csinh);
RCC_DECLARE_LEGACY_COMPLEX_UNARY_API(ctanh);
RCC_DECLARE_LEGACY_COMPLEX_UNARY_API(cexp);
RCC_DECLARE_LEGACY_COMPLEX_UNARY_API(clog);
RCC_DECLARE_LEGACY_COMPLEX_UNARY_API(cproj);
#undef RCC_DECLARE_LEGACY_COMPLEX_UNARY_API
extern RccLegacyComplexFloat RINOS_ABI rcc_legacy_complex_api_cpowf(
    RccLegacyComplexFloat, RccLegacyComplexFloat);
extern RccLegacyComplexDouble RINOS_ABI rcc_legacy_complex_api_cpow(
    RccLegacyComplexDouble, RccLegacyComplexDouble);
extern RccLegacyComplexBinary64Pair RINOS_ABI
rcc_legacy_complex_api_cpowl(RccLegacyComplexBinary64Pair,
                             RccLegacyComplexBinary64Pair);

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

#define RCC_DEFINE_LONG_DOUBLE_COMPLEX_API_BRIDGE(name) \
    RccLegacyComplexBinary64Pair RINOS_ABI \
    rcc_legacy_bridge_##name##l( \
        RccLegacyComplexBinary64Pair value) __asm__(#name "l"); \
    RccLegacyComplexBinary64Pair RINOS_ABI \
    rcc_legacy_bridge_##name##l( \
        RccLegacyComplexBinary64Pair value) { \
        return rcc_legacy_host_to_pair( \
            rcc_host_native_##name##l(rcc_legacy_pair_to_host(value))); \
    }
RCC_DEFINE_LONG_DOUBLE_COMPLEX_API_BRIDGE(cacos)
RCC_DEFINE_LONG_DOUBLE_COMPLEX_API_BRIDGE(casin)
RCC_DEFINE_LONG_DOUBLE_COMPLEX_API_BRIDGE(catan)
RCC_DEFINE_LONG_DOUBLE_COMPLEX_API_BRIDGE(ccos)
RCC_DEFINE_LONG_DOUBLE_COMPLEX_API_BRIDGE(csin)
RCC_DEFINE_LONG_DOUBLE_COMPLEX_API_BRIDGE(ctan)
RCC_DEFINE_LONG_DOUBLE_COMPLEX_API_BRIDGE(cacosh)
RCC_DEFINE_LONG_DOUBLE_COMPLEX_API_BRIDGE(casinh)
RCC_DEFINE_LONG_DOUBLE_COMPLEX_API_BRIDGE(catanh)
RCC_DEFINE_LONG_DOUBLE_COMPLEX_API_BRIDGE(ccosh)
RCC_DEFINE_LONG_DOUBLE_COMPLEX_API_BRIDGE(csinh)
RCC_DEFINE_LONG_DOUBLE_COMPLEX_API_BRIDGE(ctanh)
RCC_DEFINE_LONG_DOUBLE_COMPLEX_API_BRIDGE(cexp)
RCC_DEFINE_LONG_DOUBLE_COMPLEX_API_BRIDGE(clog)
RCC_DEFINE_LONG_DOUBLE_COMPLEX_API_BRIDGE(cproj)
#undef RCC_DEFINE_LONG_DOUBLE_COMPLEX_API_BRIDGE

RccLegacyComplexBinary64Pair RINOS_ABI rcc_legacy_bridge_cpowl(
    RccLegacyComplexBinary64Pair base,
    RccLegacyComplexBinary64Pair exponent) __asm__("cpowl");
RccLegacyComplexBinary64Pair RINOS_ABI rcc_legacy_bridge_cpowl(
    RccLegacyComplexBinary64Pair base,
    RccLegacyComplexBinary64Pair exponent) {
    return rcc_legacy_host_to_pair(rcc_host_native_cpowl(
        rcc_legacy_pair_to_host(base), rcc_legacy_pair_to_host(exponent)));
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
    {
        RccLegacyComplexBinary64Pair wide =
            rcc_legacy_complex_long_double_tls_read();
        if (fabs(wide.real - 11.0) > 0.0000001 ||
            fabs(wide.imaginary - 12.0) > 0.0000001) {
            test_case->failed = 1;
            return NULL;
        }
        rcc_legacy_complex_long_double_tls.real = -5.0;
        rcc_legacy_complex_long_double_tls.imaginary = 7.0;
        wide = rcc_legacy_complex_long_double_tls_read();
        if (fabs(wide.real + 5.0) > 0.0000001 ||
            fabs(wide.imaginary - 7.0) > 0.0000001) {
            test_case->failed = 1;
        }
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

#define RCC_ASSERT_FLOAT_COMPLEX_API(name, input, expected_real, expected_imag) \
    do { \
        RccLegacyComplexFloat result = \
            rcc_legacy_complex_api_##name##f(input); \
        assert(near_float(__real__ result, expected_real) && \
               near_float(__imag__ result, expected_imag)); \
    } while (0)
#define RCC_ASSERT_DOUBLE_COMPLEX_API(name, input, expected_real, expected_imag) \
    do { \
        RccLegacyComplexDouble result = \
            rcc_legacy_complex_api_##name(input); \
        assert(near_double(__real__ result, expected_real) && \
               near_double(__imag__ result, expected_imag)); \
    } while (0)
#define RCC_ASSERT_LONG_DOUBLE_COMPLEX_API( \
    name, input, expected_real, expected_imag) \
    do { \
        RccLegacyComplexBinary64Pair result = \
            rcc_legacy_complex_api_##name##l(input); \
        assert(near_double(result.real, expected_real) && \
               near_double(result.imaginary, expected_imag)); \
    } while (0)

static void check_remaining_complex_api(void) {
    RCC_ASSERT_FLOAT_COMPLEX_API(cacos, __builtin_complex(1.0f, 0.0f),
                                 0.0f, 0.0f);
    RCC_ASSERT_DOUBLE_COMPLEX_API(cacos, __builtin_complex(1.0, 0.0),
                                  0.0, 0.0);
    RCC_ASSERT_LONG_DOUBLE_COMPLEX_API(cacos, ((ComplexBinary64Pair){1.0, 0.0}),
                                       0.0, 0.0);
    RCC_ASSERT_FLOAT_COMPLEX_API(casin, __builtin_complex(0.0f, 0.0f),
                                 0.0f, 0.0f);
    RCC_ASSERT_DOUBLE_COMPLEX_API(casin, __builtin_complex(0.0, 0.0),
                                  0.0, 0.0);
    RCC_ASSERT_LONG_DOUBLE_COMPLEX_API(casin, ((ComplexBinary64Pair){0.0, 0.0}),
                                       0.0, 0.0);
    RCC_ASSERT_FLOAT_COMPLEX_API(catan, __builtin_complex(0.0f, 0.0f),
                                 0.0f, 0.0f);
    RCC_ASSERT_DOUBLE_COMPLEX_API(catan, __builtin_complex(0.0, 0.0),
                                  0.0, 0.0);
    RCC_ASSERT_LONG_DOUBLE_COMPLEX_API(catan, ((ComplexBinary64Pair){0.0, 0.0}),
                                       0.0, 0.0);
    RCC_ASSERT_FLOAT_COMPLEX_API(ccos, __builtin_complex(0.0f, 0.0f),
                                 1.0f, 0.0f);
    RCC_ASSERT_DOUBLE_COMPLEX_API(ccos, __builtin_complex(0.0, 0.0),
                                  1.0, 0.0);
    RCC_ASSERT_LONG_DOUBLE_COMPLEX_API(ccos, ((ComplexBinary64Pair){0.0, 0.0}),
                                       1.0, 0.0);
    RCC_ASSERT_FLOAT_COMPLEX_API(csin, __builtin_complex(0.0f, 0.0f),
                                 0.0f, 0.0f);
    RCC_ASSERT_DOUBLE_COMPLEX_API(csin, __builtin_complex(0.0, 0.0),
                                  0.0, 0.0);
    RCC_ASSERT_LONG_DOUBLE_COMPLEX_API(csin, ((ComplexBinary64Pair){0.0, 0.0}),
                                       0.0, 0.0);
    RCC_ASSERT_FLOAT_COMPLEX_API(ctan, __builtin_complex(0.0f, 0.0f),
                                 0.0f, 0.0f);
    RCC_ASSERT_DOUBLE_COMPLEX_API(ctan, __builtin_complex(0.0, 0.0),
                                  0.0, 0.0);
    RCC_ASSERT_LONG_DOUBLE_COMPLEX_API(ctan, ((ComplexBinary64Pair){0.0, 0.0}),
                                       0.0, 0.0);
    RCC_ASSERT_FLOAT_COMPLEX_API(cacosh, __builtin_complex(1.0f, 0.0f),
                                 0.0f, 0.0f);
    RCC_ASSERT_DOUBLE_COMPLEX_API(cacosh, __builtin_complex(1.0, 0.0),
                                  0.0, 0.0);
    RCC_ASSERT_LONG_DOUBLE_COMPLEX_API(cacosh, ((ComplexBinary64Pair){1.0, 0.0}),
                                       0.0, 0.0);
    RCC_ASSERT_FLOAT_COMPLEX_API(casinh, __builtin_complex(0.0f, 0.0f),
                                 0.0f, 0.0f);
    RCC_ASSERT_DOUBLE_COMPLEX_API(casinh, __builtin_complex(0.0, 0.0),
                                  0.0, 0.0);
    RCC_ASSERT_LONG_DOUBLE_COMPLEX_API(casinh, ((ComplexBinary64Pair){0.0, 0.0}),
                                       0.0, 0.0);
    RCC_ASSERT_FLOAT_COMPLEX_API(catanh, __builtin_complex(0.0f, 0.0f),
                                 0.0f, 0.0f);
    RCC_ASSERT_DOUBLE_COMPLEX_API(catanh, __builtin_complex(0.0, 0.0),
                                  0.0, 0.0);
    RCC_ASSERT_LONG_DOUBLE_COMPLEX_API(catanh, ((ComplexBinary64Pair){0.0, 0.0}),
                                       0.0, 0.0);
    RCC_ASSERT_FLOAT_COMPLEX_API(ccosh, __builtin_complex(0.0f, 0.0f),
                                 1.0f, 0.0f);
    RCC_ASSERT_DOUBLE_COMPLEX_API(ccosh, __builtin_complex(0.0, 0.0),
                                  1.0, 0.0);
    RCC_ASSERT_LONG_DOUBLE_COMPLEX_API(ccosh, ((ComplexBinary64Pair){0.0, 0.0}),
                                       1.0, 0.0);
    RCC_ASSERT_FLOAT_COMPLEX_API(csinh, __builtin_complex(0.0f, 0.0f),
                                 0.0f, 0.0f);
    RCC_ASSERT_DOUBLE_COMPLEX_API(csinh, __builtin_complex(0.0, 0.0),
                                  0.0, 0.0);
    RCC_ASSERT_LONG_DOUBLE_COMPLEX_API(csinh, ((ComplexBinary64Pair){0.0, 0.0}),
                                       0.0, 0.0);
    RCC_ASSERT_FLOAT_COMPLEX_API(ctanh, __builtin_complex(0.0f, 0.0f),
                                 0.0f, 0.0f);
    RCC_ASSERT_DOUBLE_COMPLEX_API(ctanh, __builtin_complex(0.0, 0.0),
                                  0.0, 0.0);
    RCC_ASSERT_LONG_DOUBLE_COMPLEX_API(ctanh, ((ComplexBinary64Pair){0.0, 0.0}),
                                       0.0, 0.0);
    RCC_ASSERT_FLOAT_COMPLEX_API(cexp, __builtin_complex(0.0f, 0.0f),
                                 1.0f, 0.0f);
    RCC_ASSERT_DOUBLE_COMPLEX_API(cexp, __builtin_complex(0.0, 0.0),
                                  1.0, 0.0);
    RCC_ASSERT_LONG_DOUBLE_COMPLEX_API(cexp, ((ComplexBinary64Pair){0.0, 0.0}),
                                       1.0, 0.0);
    RCC_ASSERT_FLOAT_COMPLEX_API(clog, __builtin_complex(1.0f, 0.0f),
                                 0.0f, 0.0f);
    RCC_ASSERT_DOUBLE_COMPLEX_API(clog, __builtin_complex(1.0, 0.0),
                                  0.0, 0.0);
    RCC_ASSERT_LONG_DOUBLE_COMPLEX_API(clog, ((ComplexBinary64Pair){1.0, 0.0}),
                                       0.0, 0.0);
    RCC_ASSERT_FLOAT_COMPLEX_API(cproj, __builtin_complex(1.0f, 2.0f),
                                 1.0f, 2.0f);
    RCC_ASSERT_DOUBLE_COMPLEX_API(cproj, __builtin_complex(1.0, 2.0),
                                  1.0, 2.0);
    RCC_ASSERT_LONG_DOUBLE_COMPLEX_API(cproj, ((ComplexBinary64Pair){1.0, 2.0}),
                                       1.0, 2.0);
    {
        RccLegacyComplexFloat float_result = rcc_legacy_complex_api_cpowf(
            __builtin_complex(2.0f, 0.0f),
            __builtin_complex(3.0f, 0.0f));
        RccLegacyComplexDouble double_result = rcc_legacy_complex_api_cpow(
            __builtin_complex(2.0, 0.0), __builtin_complex(3.0, 0.0));
        RccLegacyComplexBinary64Pair long_double_result =
            rcc_legacy_complex_api_cpowl(
                (ComplexBinary64Pair){2.0, 0.0},
                (ComplexBinary64Pair){3.0, 0.0});
        assert(near_float(__real__ float_result, 8.0f) &&
               near_float(__imag__ float_result, 0.0f));
        assert(near_double(__real__ double_result, 8.0) &&
               near_double(__imag__ double_result, 0.0));
        assert(near_double(long_double_result.real, 8.0) &&
               near_double(long_double_result.imaginary, 0.0));
    }
}

#undef RCC_ASSERT_FLOAT_COMPLEX_API
#undef RCC_ASSERT_DOUBLE_COMPLEX_API
#undef RCC_ASSERT_LONG_DOUBLE_COMPLEX_API

static void check_complex_branch_cuts(void) {
#define RCC_ASSERT_FLOAT_BRANCH(call, expected_real, expected_imaginary, \
                                negative_imaginary) \
    do { \
        RccLegacyComplexFloat value = (call); \
        assert(near_float(__real__ value, expected_real)); \
        assert(near_float(fabsf(__imag__ value), expected_imaginary)); \
        assert(!!__builtin_signbit(__imag__ value) == \
               (negative_imaginary)); \
    } while (0)
#define RCC_ASSERT_DOUBLE_BRANCH(call, expected_real, expected_imaginary, \
                                 negative_imaginary) \
    do { \
        RccLegacyComplexDouble value = (call); \
        assert(near_double(__real__ value, expected_real)); \
        assert(near_double(fabs(__imag__ value), expected_imaginary)); \
        assert(!!__builtin_signbit(__imag__ value) == \
               (negative_imaginary)); \
    } while (0)
#define RCC_ASSERT_LONG_DOUBLE_BRANCH(call, expected_real, \
                                      expected_imaginary, negative_imaginary) \
    do { \
        RccLegacyComplexBinary64Pair value = (call); \
        assert(near_double(value.real, expected_real)); \
        assert(near_double(fabs(value.imaginary), expected_imaginary)); \
        assert(!!__builtin_signbit(value.imaginary) == \
               (negative_imaginary)); \
    } while (0)

    RccLegacyComplexBinary64Pair ld_negative_axis = {-1.0, 0.0};
    RccLegacyComplexBinary64Pair ld_negative_axis_below = {-1.0, -0.0};
    RccLegacyComplexBinary64Pair ld_acos_cut = {-2.0, 0.0};
    RccLegacyComplexBinary64Pair ld_acos_cut_below = {-2.0, -0.0};
    RccLegacyComplexBinary64Pair ld_asin_cut = {2.0, 0.0};
    RccLegacyComplexBinary64Pair ld_asin_cut_below = {2.0, -0.0};

    RCC_ASSERT_FLOAT_BRANCH(
        rcc_legacy_complex_api_clogf(__builtin_complex(-1.0f, 0.0f)),
        0.0f, 3.14159265358979323846f, 0);
    RCC_ASSERT_FLOAT_BRANCH(
        rcc_legacy_complex_api_clogf(__builtin_complex(-1.0f, -0.0f)),
        0.0f, 3.14159265358979323846f, 1);
    RCC_ASSERT_DOUBLE_BRANCH(
        rcc_legacy_complex_api_clog(__builtin_complex(-1.0, 0.0)),
        0.0, 3.14159265358979323846, 0);
    RCC_ASSERT_DOUBLE_BRANCH(
        rcc_legacy_complex_api_clog(__builtin_complex(-1.0, -0.0)),
        0.0, 3.14159265358979323846, 1);
    RCC_ASSERT_LONG_DOUBLE_BRANCH(
        rcc_legacy_complex_api_clogl(ld_negative_axis),
        0.0, 3.14159265358979323846, 0);
    RCC_ASSERT_LONG_DOUBLE_BRANCH(
        rcc_legacy_complex_api_clogl(ld_negative_axis_below),
        0.0, 3.14159265358979323846, 1);

    RCC_ASSERT_FLOAT_BRANCH(
        rcc_legacy_complex_api_cacosf(__builtin_complex(-2.0f, 0.0f)),
        3.14159265358979323846f, 1.3169578969248168f, 1);
    RCC_ASSERT_FLOAT_BRANCH(
        rcc_legacy_complex_api_cacosf(__builtin_complex(-2.0f, -0.0f)),
        3.14159265358979323846f, 1.3169578969248168f, 0);
    RCC_ASSERT_DOUBLE_BRANCH(
        rcc_legacy_complex_api_cacos(__builtin_complex(-2.0, 0.0)),
        3.14159265358979323846, 1.3169578969248168, 1);
    RCC_ASSERT_DOUBLE_BRANCH(
        rcc_legacy_complex_api_cacos(__builtin_complex(-2.0, -0.0)),
        3.14159265358979323846, 1.3169578969248168, 0);
    RCC_ASSERT_LONG_DOUBLE_BRANCH(
        rcc_legacy_complex_api_cacosl(ld_acos_cut),
        3.14159265358979323846, 1.3169578969248168, 1);
    RCC_ASSERT_LONG_DOUBLE_BRANCH(
        rcc_legacy_complex_api_cacosl(ld_acos_cut_below),
        3.14159265358979323846, 1.3169578969248168, 0);

    RCC_ASSERT_FLOAT_BRANCH(
        rcc_legacy_complex_api_casinf(__builtin_complex(2.0f, 0.0f)),
        1.57079632679489661923f, 1.3169578969248168f, 0);
    RCC_ASSERT_FLOAT_BRANCH(
        rcc_legacy_complex_api_casinf(__builtin_complex(2.0f, -0.0f)),
        1.57079632679489661923f, 1.3169578969248168f, 1);
    RCC_ASSERT_DOUBLE_BRANCH(
        rcc_legacy_complex_api_casin(__builtin_complex(2.0, 0.0)),
        1.57079632679489661923, 1.3169578969248168, 0);
    RCC_ASSERT_DOUBLE_BRANCH(
        rcc_legacy_complex_api_casin(__builtin_complex(2.0, -0.0)),
        1.57079632679489661923, 1.3169578969248168, 1);
    RCC_ASSERT_LONG_DOUBLE_BRANCH(
        rcc_legacy_complex_api_casinl(ld_asin_cut),
        1.57079632679489661923, 1.3169578969248168, 0);
    RCC_ASSERT_LONG_DOUBLE_BRANCH(
        rcc_legacy_complex_api_casinl(ld_asin_cut_below),
        1.57079632679489661923, 1.3169578969248168, 1);

    RCC_ASSERT_FLOAT_BRANCH(
        rcc_legacy_complex_api_csqrtf(__builtin_complex(-4.0f, 0.0f)),
        0.0f, 2.0f, 0);
    RCC_ASSERT_FLOAT_BRANCH(
        rcc_legacy_complex_api_csqrtf(__builtin_complex(-4.0f, -0.0f)),
        0.0f, 2.0f, 1);
    RCC_ASSERT_DOUBLE_BRANCH(
        rcc_legacy_complex_api_csqrt(__builtin_complex(-4.0, 0.0)),
        0.0, 2.0, 0);
    RCC_ASSERT_DOUBLE_BRANCH(
        rcc_legacy_complex_api_csqrt(__builtin_complex(-4.0, -0.0)),
        0.0, 2.0, 1);
    RCC_ASSERT_LONG_DOUBLE_BRANCH(
        rcc_legacy_complex_api_csqrtl((ComplexBinary64Pair){-4.0, 0.0}),
        0.0, 2.0, 0);
    RCC_ASSERT_LONG_DOUBLE_BRANCH(
        rcc_legacy_complex_api_csqrtl((ComplexBinary64Pair){-4.0, -0.0}),
        0.0, 2.0, 1);
    RCC_ASSERT_FLOAT_BRANCH(
        rcc_legacy_complex_api_catanhf(__builtin_complex(2.0f, 0.0f)),
        0.54930614f, 1.57079632679489661923f, 0);
    RCC_ASSERT_FLOAT_BRANCH(
        rcc_legacy_complex_api_catanhf(__builtin_complex(2.0f, -0.0f)),
        0.54930614f, 1.57079632679489661923f, 1);
    RCC_ASSERT_DOUBLE_BRANCH(
        rcc_legacy_complex_api_catanh(__builtin_complex(2.0, 0.0)),
        0.5493061443340548, 1.57079632679489661923, 0);
    RCC_ASSERT_DOUBLE_BRANCH(
        rcc_legacy_complex_api_catanh(__builtin_complex(2.0, -0.0)),
        0.5493061443340548, 1.57079632679489661923, 1);
    RCC_ASSERT_LONG_DOUBLE_BRANCH(
        rcc_legacy_complex_api_catanhl((ComplexBinary64Pair){2.0, 0.0}),
        0.5493061443340548, 1.57079632679489661923, 0);
    RCC_ASSERT_LONG_DOUBLE_BRANCH(
        rcc_legacy_complex_api_catanhl((ComplexBinary64Pair){2.0, -0.0}),
        0.5493061443340548, 1.57079632679489661923, 1);

    {
        RccLegacyComplexFloat value_float =
            rcc_legacy_complex_api_cpowf(
                __builtin_complex(-1.0f, 0.0f),
                __builtin_complex(0.5f, 0.0f));
        RccLegacyComplexDouble value_double = rcc_legacy_complex_api_cpow(
            __builtin_complex(-1.0, 0.0), __builtin_complex(0.5, 0.0));
        RccLegacyComplexBinary64Pair value_long_double =
            rcc_legacy_complex_api_cpowl(
                (ComplexBinary64Pair){-1.0, 0.0},
                (ComplexBinary64Pair){0.5, 0.0});
        assert(near_float(__real__ value_float, 0.0f) &&
               near_float(__imag__ value_float, 1.0f) &&
               !__builtin_signbit(__imag__ value_float));
        assert(near_double(__real__ value_double, 0.0) &&
               near_double(__imag__ value_double, 1.0) &&
               !__builtin_signbit(__imag__ value_double));
        assert(near_double(value_long_double.real, 0.0) &&
               near_double(value_long_double.imaginary, 1.0) &&
               !__builtin_signbit(value_long_double.imaginary));
        value_float = rcc_legacy_complex_api_cpowf(
            __builtin_complex(-1.0f, -0.0f),
            __builtin_complex(0.5f, 0.0f));
        value_double = rcc_legacy_complex_api_cpow(
            __builtin_complex(-1.0, -0.0), __builtin_complex(0.5, 0.0));
        value_long_double = rcc_legacy_complex_api_cpowl(
            (ComplexBinary64Pair){-1.0, -0.0},
            (ComplexBinary64Pair){0.5, 0.0});
        assert(__builtin_signbit(__imag__ value_float));
        assert(__builtin_signbit(__imag__ value_double));
        assert(__builtin_signbit(value_long_double.imaginary));
    }
    {
        RccLegacyComplexFloat projected_float;
        RccLegacyComplexDouble projected_double;
        RccLegacyComplexBinary64Pair projected_long_double;

        projected_float = rcc_legacy_complex_api_cprojf(
            __builtin_complex(-__builtin_huge_valf(), 3.0f));
        assert(__real__ projected_float == INFINITY &&
               __imag__ projected_float == 0.0f &&
               !__builtin_signbit(__imag__ projected_float));
        projected_float = rcc_legacy_complex_api_cprojf(
            __builtin_complex(3.0f, -__builtin_huge_valf()));
        assert(__real__ projected_float == INFINITY &&
               __imag__ projected_float == 0.0f &&
               __builtin_signbit(__imag__ projected_float));
        projected_float = rcc_legacy_complex_api_cprojf(
            __builtin_complex(__builtin_huge_valf(), __builtin_nanf("")));
        assert(__real__ projected_float == INFINITY &&
               __imag__ projected_float == 0.0f &&
               !__builtin_signbit(__imag__ projected_float));
        projected_float = rcc_legacy_complex_api_cprojf(
            __builtin_complex(__builtin_nanf(""), __builtin_huge_valf()));
        assert(__real__ projected_float == INFINITY &&
               __imag__ projected_float == 0.0f &&
               !__builtin_signbit(__imag__ projected_float));

        projected_double = rcc_legacy_complex_api_cproj(
            __builtin_complex(-__builtin_huge_val(), 3.0));
        assert(__real__ projected_double == INFINITY &&
               __imag__ projected_double == 0.0 &&
               !__builtin_signbit(__imag__ projected_double));
        projected_double = rcc_legacy_complex_api_cproj(
            __builtin_complex(3.0, -__builtin_huge_val()));
        assert(__real__ projected_double == INFINITY &&
               __imag__ projected_double == 0.0 &&
               __builtin_signbit(__imag__ projected_double));
        projected_double = rcc_legacy_complex_api_cproj(
            __builtin_complex(__builtin_huge_val(), __builtin_nan("")));
        assert(__real__ projected_double == INFINITY &&
               __imag__ projected_double == 0.0 &&
               !__builtin_signbit(__imag__ projected_double));
        projected_double = rcc_legacy_complex_api_cproj(
            __builtin_complex(__builtin_nan(""), __builtin_huge_val()));
        assert(__real__ projected_double == INFINITY &&
               __imag__ projected_double == 0.0 &&
               !__builtin_signbit(__imag__ projected_double));

        projected_long_double = rcc_legacy_complex_api_cprojl(
            (ComplexBinary64Pair){-(double)__builtin_huge_val(), 3.0});
        assert(projected_long_double.real == INFINITY &&
               projected_long_double.imaginary == 0.0 &&
               !__builtin_signbit(projected_long_double.imaginary));
        projected_long_double = rcc_legacy_complex_api_cprojl(
            (ComplexBinary64Pair){3.0, -(double)__builtin_huge_val()});
        assert(projected_long_double.real == INFINITY &&
               projected_long_double.imaginary == 0.0 &&
               __builtin_signbit(projected_long_double.imaginary));
        projected_long_double = rcc_legacy_complex_api_cprojl(
            (ComplexBinary64Pair){(double)__builtin_huge_val(),
                                  __builtin_nan("")});
        assert(projected_long_double.real == INFINITY &&
               projected_long_double.imaginary == 0.0 &&
               !__builtin_signbit(projected_long_double.imaginary));
        projected_long_double = rcc_legacy_complex_api_cprojl(
            (ComplexBinary64Pair){__builtin_nan(""),
                                  (double)__builtin_huge_val()});
        assert(projected_long_double.real == INFINITY &&
               projected_long_double.imaginary == 0.0 &&
               !__builtin_signbit(projected_long_double.imaginary));
    }

#undef RCC_ASSERT_FLOAT_BRANCH
#undef RCC_ASSERT_DOUBLE_BRANCH
#undef RCC_ASSERT_LONG_DOUBLE_BRANCH
}

int main(void) {
    pthread_t tls_thread;
    RccLegacyComplexTlsCase tls_case = {0};
    RccLegacyComplexFloat float_api_input = __builtin_complex(3.0f, 4.0f);
    RccLegacyComplexDouble double_api_input = __builtin_complex(3.0, 4.0);
    RccLegacyComplexBinary64Pair long_double_api_input = {3.0, 4.0};
    RccLegacyComplexFloat float_left = __builtin_complex(2.0f, 3.0f);
    RccLegacyComplexFloat float_right = __builtin_complex(4.0f, -1.0f);
    RccLegacyComplexFloat float_result;
    RccLegacyComplexDouble double_left = __builtin_complex(2.0, 3.0);
    RccLegacyComplexDouble double_right = __builtin_complex(4.0, -1.0);
    RccLegacyComplexDouble double_result;
    RccLegacyComplexBinary64Pair long_double_result;

    double_result = rcc_legacy_complex_tls_read();
    assert(fabs(__real__ double_result - 9.0) < 0.0000001 &&
           fabs(__imag__ double_result - 10.0) < 0.0000001);
    long_double_result = rcc_legacy_complex_long_double_static_read();
    assert(near_double(long_double_result.real, 5.0) &&
           near_double(long_double_result.imaginary, 6.0));
    long_double_result = rcc_legacy_complex_long_double_tls_read();
    assert(near_double(long_double_result.real, 11.0) &&
           near_double(long_double_result.imaginary, 12.0));
    __real__ rcc_legacy_complex_tls = 2.0;
    __imag__ rcc_legacy_complex_tls = -3.0;
    rcc_legacy_complex_long_double_tls.real = 2.0;
    rcc_legacy_complex_long_double_tls.imaginary = -3.0;
    assert(pthread_create(&tls_thread, NULL, rcc_legacy_complex_tls_worker,
                          &tls_case) == 0);
    assert(pthread_join(tls_thread, NULL) == 0);
    assert(!tls_case.failed);
    double_result = rcc_legacy_complex_tls_read();
    assert(fabs(__real__ double_result - 2.0) < 0.0000001 &&
           fabs(__imag__ double_result + 3.0) < 0.0000001);
    long_double_result = rcc_legacy_complex_long_double_tls_read();
    assert(near_double(long_double_result.real, 2.0) &&
           near_double(long_double_result.imaginary, -3.0));

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
    assert(near_double(
        rcc_legacy_complex_api_creall(long_double_api_input), 3.0));
    assert(near_double(
        rcc_legacy_complex_api_cimagl(long_double_api_input), 4.0));
    assert(near_double(
        rcc_legacy_complex_api_cabsl(long_double_api_input), 5.0));
    assert(near_double(
        rcc_legacy_complex_api_cargl(long_double_api_input),
        atan2(4.0, 3.0)));
    long_double_result = rcc_legacy_complex_api_conjl(
        long_double_api_input);
    assert(near_double(long_double_result.real, 3.0) &&
           near_double(long_double_result.imaginary, -4.0));
    long_double_result = rcc_legacy_complex_api_csqrtl(
        long_double_api_input);
    assert(near_double(long_double_result.real, 2.0) &&
           near_double(long_double_result.imaginary, 1.0));
    check_remaining_complex_api();
    check_complex_branch_cuts();

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
    assert(feclearexcept(FE_ALL_EXCEPT) == 0);
    float_result = rcc_legacy_complex_float_multiply(
        __builtin_complex(FLT_MAX, 0.0f),
        __builtin_complex(2.0f, 0.0f));
    assert(isinf(__real__ float_result));
    assert((fetestexcept(FE_ALL_EXCEPT) &
            (FE_OVERFLOW | FE_INEXACT)) ==
           (FE_OVERFLOW | FE_INEXACT));
    assert(feclearexcept(FE_ALL_EXCEPT) == 0);
    float_result = rcc_legacy_complex_float_multiply(
        __builtin_complex(FLT_MIN * FLT_EPSILON, 0.0f),
        __builtin_complex(0.5f, 0.0f));
    assert(__real__ float_result == 0.0f &&
           __imag__ float_result == 0.0f);
    assert((fetestexcept(FE_ALL_EXCEPT) &
            (FE_UNDERFLOW | FE_INEXACT)) ==
           (FE_UNDERFLOW | FE_INEXACT));
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
    assert(feclearexcept(FE_ALL_EXCEPT) == 0);
    double_result = rcc_legacy_complex_double_multiply(
        __builtin_complex(DBL_MAX, 0.0),
        __builtin_complex(2.0, 0.0));
    assert(isinf(__real__ double_result));
    assert((fetestexcept(FE_ALL_EXCEPT) &
            (FE_OVERFLOW | FE_INEXACT)) ==
           (FE_OVERFLOW | FE_INEXACT));
    assert(feclearexcept(FE_ALL_EXCEPT) == 0);
    double_result = rcc_legacy_complex_double_multiply(
        __builtin_complex(DBL_MIN * DBL_EPSILON, 0.0),
        __builtin_complex(0.5, 0.0));
    assert(__real__ double_result == 0.0 &&
           __imag__ double_result == 0.0);
    assert((fetestexcept(FE_ALL_EXCEPT) &
            (FE_UNDERFLOW | FE_INEXACT)) ==
           (FE_UNDERFLOW | FE_INEXACT));
    {
        RccLegacyComplexBinary64Pair long_double_result =
            rcc_legacy_complex_long_double_multiply(
                __builtin_complex(HUGE_VAL, HUGE_VAL),
                __builtin_complex(1.0, 0.0));
        assert(isinf(long_double_result.real) &&
               isinf(long_double_result.imaginary));
        assert(feclearexcept(FE_ALL_EXCEPT) == 0);
        long_double_result = rcc_legacy_complex_long_double_divide(
            double_left, __builtin_complex(0.0, 0.0));
        assert(isinf(long_double_result.real) &&
               isinf(long_double_result.imaginary));
        assert((fetestexcept(FE_ALL_EXCEPT) & FE_DIVBYZERO) != 0);
        assert(feclearexcept(FE_ALL_EXCEPT) == 0);
        long_double_result = rcc_legacy_complex_long_double_divide(
            __builtin_complex(0.0, 0.0), __builtin_complex(0.0, 0.0));
        assert(isnan(long_double_result.real) &&
               isnan(long_double_result.imaginary));
        assert((fetestexcept(FE_ALL_EXCEPT) &
                (FE_DIVBYZERO | FE_INVALID)) ==
               (FE_DIVBYZERO | FE_INVALID));
        long_double_result = rcc_legacy_complex_long_double_divide(
            __builtin_complex(1.0, 2.0),
            __builtin_complex(HUGE_VAL, HUGE_VAL));
        assert(long_double_result.real == 0.0 &&
               long_double_result.imaginary == 0.0);
        long_double_result = rcc_legacy_complex_long_double_divide(
            __builtin_complex(HUGE_VAL, HUGE_VAL),
            __builtin_complex(1.0, 0.0));
        assert(isinf(long_double_result.real) &&
               isinf(long_double_result.imaginary));
        assert(feclearexcept(FE_ALL_EXCEPT) == 0);
        long_double_result = rcc_legacy_complex_long_double_multiply(
            __builtin_complex(DBL_MAX, 0.0),
            __builtin_complex(2.0, 0.0));
        assert(isinf(long_double_result.real));
        assert((fetestexcept(FE_ALL_EXCEPT) &
                (FE_OVERFLOW | FE_INEXACT)) ==
               (FE_OVERFLOW | FE_INEXACT));
        assert(feclearexcept(FE_ALL_EXCEPT) == 0);
        long_double_result = rcc_legacy_complex_long_double_multiply(
            __builtin_complex(DBL_MIN * DBL_EPSILON, 0.0),
            __builtin_complex(0.5, 0.0));
        assert(long_double_result.real == 0.0 &&
               long_double_result.imaginary == 0.0);
        assert((fetestexcept(FE_ALL_EXCEPT) &
                (FE_UNDERFLOW | FE_INEXACT)) ==
               (FE_UNDERFLOW | FE_INEXACT));
    }
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
    double_result = rcc_legacy_complex_double_add_float(0.5, float_left);
    assert(near_double(__real__ double_result, 2.5) &&
           near_double(__imag__ double_result, 3.0));
    double_result = rcc_legacy_complex_float_subtract_double(float_left, 0.5);
    assert(near_double(__real__ double_result, 1.5) &&
           near_double(__imag__ double_result, 3.0));
    double_result = rcc_legacy_complex_double_subtract_float(0.5, float_left);
    assert(near_double(__real__ double_result, -1.5) &&
           near_double(__imag__ double_result, -3.0));
    double_result = rcc_legacy_complex_float_multiply_double(float_left, 0.5);
    assert(near_double(__real__ double_result, 1.0) &&
           near_double(__imag__ double_result, 1.5));
    double_result = rcc_legacy_complex_double_multiply_float(0.5, float_left);
    assert(near_double(__real__ double_result, 1.0) &&
           near_double(__imag__ double_result, 1.5));
    double_result = rcc_legacy_complex_float_divide_double(float_left, 0.5);
    assert(near_double(__real__ double_result, 4.0) &&
           near_double(__imag__ double_result, 6.0));
    double_result = rcc_legacy_complex_double_divide_float(0.5, float_left);
    assert(near_double(__real__ double_result, 1.0 / 13.0) &&
           near_double(__imag__ double_result, -3.0 / 26.0));
    {
        RccLegacyComplexBinary64Pair wide_result =
            rcc_legacy_complex_double_add_long_double(double_left, 0.5);
        assert(near_double(wide_result.real, 2.5) &&
               near_double(wide_result.imaginary, 3.0));
    }
    {
        RccLegacyComplexBinary64Pair wide_result =
            rcc_legacy_complex_long_double_identity(double_left);
        assert(near_double(wide_result.real, 2.0) &&
               near_double(wide_result.imaginary, 3.0));
        wide_result = rcc_legacy_complex_long_double_add(
            double_left, double_right);
        assert(near_double(wide_result.real, 6.0) &&
               near_double(wide_result.imaginary, 2.0));
        wide_result = rcc_legacy_complex_long_double_subtract(
            double_left, double_right);
        assert(near_double(wide_result.real, -2.0) &&
               near_double(wide_result.imaginary, 4.0));
        wide_result = rcc_legacy_complex_long_double_multiply(
            double_left, double_right);
        assert(near_double(wide_result.real, 11.0) &&
               near_double(wide_result.imaginary, 10.0));
        wide_result = rcc_legacy_complex_long_double_divide(
            double_left, __builtin_complex(1.0, 1.0));
        assert(near_double(wide_result.real, 2.5) &&
               near_double(wide_result.imaginary, 0.5));
        wide_result = rcc_legacy_complex_long_double_add_assign(
            double_left, double_right);
        assert(near_double(wide_result.real, 6.0) &&
               near_double(wide_result.imaginary, 2.0));
        wide_result = rcc_legacy_complex_long_double_multiply_assign(
            double_left, double_right);
        assert(near_double(wide_result.real, 11.0) &&
               near_double(wide_result.imaginary, 10.0));
        wide_result = rcc_legacy_complex_long_double_divide_assign(
            double_left, __builtin_complex(1.0, 1.0));
        assert(near_double(wide_result.real, 2.5) &&
               near_double(wide_result.imaginary, 0.5));
        wide_result = rcc_legacy_complex_long_double_subtract_assign(
            double_left, __builtin_complex(1.0, -1.0));
        assert(near_double(wide_result.real, 1.0) &&
               near_double(wide_result.imaginary, 4.0));
        wide_result = rcc_legacy_complex_long_double_negate(double_left);
        assert(near_double(wide_result.real, -2.0) &&
               near_double(wide_result.imaginary, -3.0));
        wide_result = rcc_legacy_complex_long_double_prefix_increment(
            double_left);
        assert(near_double(wide_result.real, 3.0) &&
               near_double(wide_result.imaginary, 3.0));
        wide_result = rcc_legacy_complex_long_double_postfix_increment(
            double_left);
        assert(near_double(wide_result.real, 2.0) &&
               near_double(wide_result.imaginary, 3.0));
        wide_result = rcc_legacy_complex_long_double_prefix_decrement(
            double_left);
        assert(near_double(wide_result.real, 1.0) &&
               near_double(wide_result.imaginary, 3.0));
        wide_result = rcc_legacy_complex_long_double_postfix_decrement(
            double_left);
        assert(near_double(wide_result.real, 2.0) &&
               near_double(wide_result.imaginary, 3.0));
        assert(rcc_legacy_complex_long_double_equal(
            double_left, double_left));
        assert(rcc_legacy_complex_long_double_not_equal(
            double_left, double_right));
        assert(!rcc_legacy_complex_long_double_equal(
            __builtin_complex((double)NAN, 0.0), double_left));
        assert(rcc_legacy_complex_long_double_not_equal(
            __builtin_complex((double)NAN, 0.0), double_left));
        assert(!rcc_legacy_complex_long_double_truth(
            __builtin_complex(0.0, 0.0)));
        assert(rcc_legacy_complex_long_double_truth(
            __builtin_complex(0.0, -1.0)));
        assert(rcc_legacy_complex_long_double_to_int(
            __builtin_complex(7.75, 9.0)) == 7);
        wide_result = rcc_legacy_complex_long_double_from_real(4.5);
        assert(near_double(wide_result.real, 4.5) &&
               near_double(wide_result.imaginary, 0.0));
    }
    {
        RccLegacyComplexBinary64Pair value = {2.0, 3.0};
        RccLegacyComplexBinary64Pair observed = {0.0, 0.0};
        rcc_legacy_complex_long_double_prefix_increment_pointer(
            &value, &observed);
        assert(near_double(value.real, 3.0) &&
               near_double(value.imaginary, 3.0));
        assert(near_double(observed.real, 3.0) &&
               near_double(observed.imaginary, 3.0));
        value = (RccLegacyComplexBinary64Pair){2.0, 3.0};
        rcc_legacy_complex_long_double_postfix_increment_pointer(
            &value, &observed);
        assert(near_double(value.real, 3.0) &&
               near_double(value.imaginary, 3.0));
        assert(near_double(observed.real, 2.0) &&
               near_double(observed.imaginary, 3.0));
        value = (RccLegacyComplexBinary64Pair){2.0, 3.0};
        rcc_legacy_complex_long_double_prefix_decrement_pointer(
            &value, &observed);
        assert(near_double(value.real, 1.0) &&
               near_double(value.imaginary, 3.0));
        assert(near_double(observed.real, 1.0) &&
               near_double(observed.imaginary, 3.0));
        value = (RccLegacyComplexBinary64Pair){2.0, 3.0};
        rcc_legacy_complex_long_double_postfix_decrement_pointer(
            &value, &observed);
        assert(near_double(value.real, 1.0) &&
               near_double(value.imaginary, 3.0));
        assert(near_double(observed.real, 2.0) &&
               near_double(observed.imaginary, 3.0));
    }
    return 0;
}
