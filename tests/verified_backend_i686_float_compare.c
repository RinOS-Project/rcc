#ifdef __cplusplus
extern "C" {
#endif

int verified_i686_compare_f32(float lhs, float rhs) {
    return (lhs == rhs) | ((lhs != rhs) << 1) |
        ((lhs < rhs) << 2) | ((lhs <= rhs) << 3) |
        ((lhs > rhs) << 4) | ((lhs >= rhs) << 5);
}

int verified_i686_compare_f64(double lhs, double rhs) {
    return (lhs == rhs) | ((lhs != rhs) << 1) |
        ((lhs < rhs) << 2) | ((lhs <= rhs) << 3) |
        ((lhs > rhs) << 4) | ((lhs >= rhs) << 5);
}

int verified_i686_truth_f32(float value) {
    if (value) return 1;
    return 0;
}

int verified_i686_truth_f64(double value) {
    if (value) return 1;
    return 0;
}

#ifdef __cplusplus
}
#endif
