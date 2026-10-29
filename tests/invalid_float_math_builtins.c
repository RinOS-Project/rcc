int invalid_float_math_builtins(void) {
    return __builtin_isnan(1) +
        __builtin_copysign(1.0) +
        __builtin_floor(1.0, 2.0) +
        __builtin_fpclassify(0.0, 1, 2, 3, 4, 1.0) +
        __builtin_fpclassify(0, 1, 2, 3, 4, 1);
}
