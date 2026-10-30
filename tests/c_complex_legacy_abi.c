#include <complex.h>

typedef float _Complex RccLegacyComplexFloat;
typedef double _Complex RccLegacyComplexDouble;
typedef long double _Complex RccLegacyComplexLongDouble;

_Thread_local RccLegacyComplexDouble rcc_legacy_complex_tls =
    CMPLX(9.0, 10.0);

RccLegacyComplexDouble rcc_legacy_complex_tls_read(void) {
    return rcc_legacy_complex_tls;
}

float rcc_legacy_complex_api_crealf(RccLegacyComplexFloat value) {
    return crealf(value);
}

float rcc_legacy_complex_api_cimagf(RccLegacyComplexFloat value) {
    return cimagf(value);
}

float rcc_legacy_complex_api_cabsf(RccLegacyComplexFloat value) {
    return cabsf(value);
}

float rcc_legacy_complex_api_cargf(RccLegacyComplexFloat value) {
    return cargf(value);
}

RccLegacyComplexFloat rcc_legacy_complex_api_conjf(
    RccLegacyComplexFloat value) {
    return conjf(value);
}

RccLegacyComplexFloat rcc_legacy_complex_api_csqrtf(
    RccLegacyComplexFloat value) {
    return csqrtf(value);
}

double rcc_legacy_complex_api_creal(RccLegacyComplexDouble value) {
    return creal(value);
}

double rcc_legacy_complex_api_cimag(RccLegacyComplexDouble value) {
    return cimag(value);
}

double rcc_legacy_complex_api_cabs(RccLegacyComplexDouble value) {
    return cabs(value);
}

double rcc_legacy_complex_api_carg(RccLegacyComplexDouble value) {
    return carg(value);
}

RccLegacyComplexDouble rcc_legacy_complex_api_conj(
    RccLegacyComplexDouble value) {
    return conj(value);
}

RccLegacyComplexDouble rcc_legacy_complex_api_csqrt(
    RccLegacyComplexDouble value) {
    return csqrt(value);
}

long double rcc_legacy_complex_api_creall(
    RccLegacyComplexLongDouble value) {
    return creall(value);
}

long double rcc_legacy_complex_api_cimagl(
    RccLegacyComplexLongDouble value) {
    return cimagl(value);
}

long double rcc_legacy_complex_api_cabsl(
    RccLegacyComplexLongDouble value) {
    return cabsl(value);
}

long double rcc_legacy_complex_api_cargl(
    RccLegacyComplexLongDouble value) {
    return cargl(value);
}

RccLegacyComplexLongDouble rcc_legacy_complex_api_conjl(
    RccLegacyComplexLongDouble value) {
    return conjl(value);
}

RccLegacyComplexLongDouble rcc_legacy_complex_api_csqrtl(
    RccLegacyComplexLongDouble value) {
    return csqrtl(value);
}

_Static_assert(_Generic(
    (RccLegacyComplexFloat)0.0f + (double)0.0,
    RccLegacyComplexDouble: 1,
    default: 0),
    "float complex plus double must promote to double complex");
_Static_assert(_Generic(
    (RccLegacyComplexDouble)0.0 + (long double)0.0L,
    RccLegacyComplexLongDouble: 1,
    default: 0),
    "double complex plus long double must promote to long double complex");

RccLegacyComplexFloat rcc_legacy_complex_float_add(
    RccLegacyComplexFloat left, RccLegacyComplexFloat right) {
    return left + right;
}

RccLegacyComplexFloat rcc_legacy_complex_float_call(
    RccLegacyComplexFloat left, RccLegacyComplexFloat right) {
    return rcc_legacy_complex_float_add(left, right);
}

RccLegacyComplexFloat rcc_legacy_complex_float_negate(
    RccLegacyComplexFloat value) {
    return -value;
}

RccLegacyComplexFloat rcc_legacy_complex_float_from_real(float value) {
    return (RccLegacyComplexFloat)value;
}

RccLegacyComplexDouble rcc_legacy_complex_widen_float(
    RccLegacyComplexFloat value) {
    return (RccLegacyComplexDouble)value;
}

RccLegacyComplexFloat rcc_legacy_complex_float_conditional(
    int choose_left, RccLegacyComplexFloat left,
    RccLegacyComplexFloat right) {
    return choose_left ? left + right : right - left;
}

RccLegacyComplexFloat rcc_legacy_complex_float_comma(
    RccLegacyComplexFloat left, RccLegacyComplexFloat right) {
    return (left, right);
}

RccLegacyComplexFloat rcc_legacy_complex_float_assignment(
    RccLegacyComplexFloat left, RccLegacyComplexFloat right) {
    RccLegacyComplexFloat result;
    result = left * right;
    return result;
}

RccLegacyComplexFloat rcc_legacy_complex_float_subtract(
    RccLegacyComplexFloat left, RccLegacyComplexFloat right) {
    return left - right;
}

RccLegacyComplexFloat rcc_legacy_complex_float_multiply(
    RccLegacyComplexFloat left, RccLegacyComplexFloat right) {
    return left * right;
}

RccLegacyComplexFloat rcc_legacy_complex_float_divide(
    RccLegacyComplexFloat left, RccLegacyComplexFloat right) {
    return left / right;
}

RccLegacyComplexFloat rcc_legacy_complex_float_add_real(
    RccLegacyComplexFloat left, float right) {
    return left + right;
}

RccLegacyComplexDouble rcc_legacy_complex_double_add(
    RccLegacyComplexDouble left, RccLegacyComplexDouble right) {
    return left + right;
}

RccLegacyComplexDouble rcc_legacy_complex_double_call(
    RccLegacyComplexDouble left, RccLegacyComplexDouble right) {
    return rcc_legacy_complex_double_add(left, right);
}

RccLegacyComplexDouble rcc_legacy_complex_double_subtract(
    RccLegacyComplexDouble left, RccLegacyComplexDouble right) {
    return left - right;
}

RccLegacyComplexDouble rcc_legacy_complex_double_multiply(
    RccLegacyComplexDouble left, RccLegacyComplexDouble right) {
    return left * right;
}

RccLegacyComplexDouble rcc_legacy_complex_double_divide(
    RccLegacyComplexDouble left, RccLegacyComplexDouble right) {
    return left / right;
}

RccLegacyComplexDouble rcc_legacy_complex_double_add_integer(
    RccLegacyComplexDouble left, int right) {
    return left + right;
}

RccLegacyComplexDouble rcc_legacy_complex_float_add_double(
    RccLegacyComplexFloat left, double right) {
    return left + right;
}

RccLegacyComplexLongDouble rcc_legacy_complex_double_add_long_double(
    RccLegacyComplexDouble left, long double right) {
    return left + right;
}

RccLegacyComplexFloat rcc_legacy_complex_float_add_assign(
    RccLegacyComplexFloat left, RccLegacyComplexFloat right) {
    left += right;
    return left;
}

RccLegacyComplexFloat rcc_legacy_complex_float_subtract_assign(
    RccLegacyComplexFloat left, RccLegacyComplexFloat right) {
    left -= right;
    return left;
}

RccLegacyComplexFloat rcc_legacy_complex_float_multiply_assign(
    RccLegacyComplexFloat left, RccLegacyComplexFloat right) {
    left *= right;
    return left;
}

RccLegacyComplexFloat rcc_legacy_complex_float_divide_assign(
    RccLegacyComplexFloat left, RccLegacyComplexFloat right) {
    left /= right;
    return left;
}

RccLegacyComplexDouble rcc_legacy_complex_double_add_assign(
    RccLegacyComplexDouble left, RccLegacyComplexDouble right) {
    left += right;
    return left;
}

RccLegacyComplexDouble rcc_legacy_complex_double_subtract_assign(
    RccLegacyComplexDouble left, RccLegacyComplexDouble right) {
    left -= right;
    return left;
}

RccLegacyComplexDouble rcc_legacy_complex_double_multiply_assign(
    RccLegacyComplexDouble left, RccLegacyComplexDouble right) {
    left *= right;
    return left;
}

RccLegacyComplexDouble rcc_legacy_complex_double_divide_assign(
    RccLegacyComplexDouble left, RccLegacyComplexDouble right) {
    left /= right;
    return left;
}

RccLegacyComplexFloat rcc_legacy_complex_float_prefix_increment(
    RccLegacyComplexFloat value) {
    return ++value;
}

RccLegacyComplexFloat rcc_legacy_complex_float_postfix_decrement(
    RccLegacyComplexFloat value) {
    return value--;
}

RccLegacyComplexDouble rcc_legacy_complex_double_prefix_increment(
    RccLegacyComplexDouble value) {
    return ++value;
}

RccLegacyComplexDouble rcc_legacy_complex_double_postfix_decrement(
    RccLegacyComplexDouble value) {
    return value--;
}

int rcc_legacy_complex_float_equal(
    RccLegacyComplexFloat left, RccLegacyComplexFloat right) {
    return left == right;
}

int rcc_legacy_complex_double_equal(
    RccLegacyComplexDouble left, RccLegacyComplexDouble right) {
    return left == right;
}

int rcc_legacy_complex_float_truth(RccLegacyComplexFloat value) {
    return !!value;
}

int rcc_legacy_complex_double_truth(RccLegacyComplexDouble value) {
    return !!value;
}

float rcc_legacy_complex_float_real(RccLegacyComplexFloat value) {
    return (float)value;
}

double rcc_legacy_complex_double_real(RccLegacyComplexDouble value) {
    return (double)value;
}

RccLegacyComplexFloat rcc_legacy_complex_double_narrow(
    RccLegacyComplexDouble value) {
    return (RccLegacyComplexFloat)value;
}

int rcc_legacy_complex_float_equal_real(
    RccLegacyComplexFloat value, float real) {
    return value == real;
}

int rcc_legacy_complex_float_not_equal_real(
    RccLegacyComplexFloat value, float real) {
    return value != real;
}

int rcc_legacy_complex_float_to_int(RccLegacyComplexFloat value) {
    return (int)value;
}

_Bool rcc_legacy_complex_float_to_bool(RccLegacyComplexFloat value) {
    return (_Bool)value;
}

static int rcc_legacy_complex_lvalue_evaluations;

static RccLegacyComplexFloat* rcc_legacy_complex_counted_lvalue(
    RccLegacyComplexFloat* value) {
    ++rcc_legacy_complex_lvalue_evaluations;
    return value;
}

RccLegacyComplexFloat rcc_legacy_complex_float_add_assign_once(
    RccLegacyComplexFloat* value, RccLegacyComplexFloat right) {
    return *rcc_legacy_complex_counted_lvalue(value) += right;
}

RccLegacyComplexFloat rcc_legacy_complex_float_increment_once(
    RccLegacyComplexFloat* value) {
    return ++*rcc_legacy_complex_counted_lvalue(value);
}

int rcc_legacy_complex_lvalue_evaluation_count(void) {
    return rcc_legacy_complex_lvalue_evaluations;
}
