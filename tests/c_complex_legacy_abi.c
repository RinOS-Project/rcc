typedef float _Complex RccLegacyComplexFloat;
typedef double _Complex RccLegacyComplexDouble;

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
