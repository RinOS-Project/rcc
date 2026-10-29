static int verify_float_math_builtins() {
    if (__builtin_isnan(__builtin_nan("")) == 0) return 1;
    if (__builtin_isinf(__builtin_inf()) == 0) return 2;
    if (__builtin_isfinite(__builtin_inf()) != 0) return 3;
    if (__builtin_isnormal(1.0f) == 0) return 4;
    if (__builtin_signbit(-0.0L) == 0) return 5;
    if (__builtin_copysignf(2.0f, -0.0f) != -2.0f) return 6;
    if (__builtin_floorl(-1.25L) != -2.0L) return 7;
    if (__builtin_ceill(-1.25L) != -1.0L) return 8;
    if (__builtin_roundf(-2.5f) != -3.0f) return 9;
    if (__builtin_trunc(1.75) != 1.0) return 10;
    return 0;
}

int main() {
    return verify_float_math_builtins();
}
