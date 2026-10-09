extern int verified_i686_compare_f32(float, float);
extern int verified_i686_compare_f64(double, double);
extern int verified_i686_truth_f32(float);
extern int verified_i686_truth_f64(double);

static float verified_i686_f32_from_bits(unsigned int bits) {
    union {
        unsigned int integer;
        float floating;
    } value;
    value.integer = bits;
    return value.floating;
}

static double verified_i686_f64_from_bits(unsigned long long bits) {
    union {
        unsigned long long integer;
        double floating;
    } value;
    value.integer = bits;
    return value.floating;
}

int main(void) {
    float nan_f32 = verified_i686_f32_from_bits(0x7fc00001u);
    float negative_zero_f32 = verified_i686_f32_from_bits(0x80000000u);
    double nan_f64 = verified_i686_f64_from_bits(
        0x7ff8000000000001ULL);
    double negative_zero_f64 = verified_i686_f64_from_bits(
        0x8000000000000000ULL);

    if (verified_i686_compare_f32(1.0f, 2.0f) != 14) return 1;
    if (verified_i686_compare_f32(2.0f, 2.0f) != 41) return 2;
    if (verified_i686_compare_f32(3.0f, 2.0f) != 50) return 3;
    if (verified_i686_compare_f32(nan_f32, 2.0f) != 2) return 4;
    if (verified_i686_compare_f32(2.0f, nan_f32) != 2) return 5;
    if (verified_i686_compare_f32(negative_zero_f32, 0.0f) != 41) return 6;

    if (verified_i686_compare_f64(1.0, 2.0) != 14) return 7;
    if (verified_i686_compare_f64(2.0, 2.0) != 41) return 8;
    if (verified_i686_compare_f64(3.0, 2.0) != 50) return 9;
    if (verified_i686_compare_f64(nan_f64, 2.0) != 2) return 10;
    if (verified_i686_compare_f64(2.0, nan_f64) != 2) return 11;
    if (verified_i686_compare_f64(negative_zero_f64, 0.0) != 41) return 12;
    if (verified_i686_truth_f32(0.0f) != 0) return 13;
    if (verified_i686_truth_f32(negative_zero_f32) != 0) return 14;
    if (verified_i686_truth_f32(1.0f) != 1) return 15;
    if (verified_i686_truth_f32(nan_f32) != 1) return 16;
    if (verified_i686_truth_f64(0.0) != 0) return 17;
    if (verified_i686_truth_f64(negative_zero_f64) != 0) return 18;
    if (verified_i686_truth_f64(1.0) != 1) return 19;
    if (verified_i686_truth_f64(nan_f64) != 1) return 20;
    return 0;
}
