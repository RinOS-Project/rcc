#ifdef __cplusplus
extern "C" {
#endif

float verified_fp_add_f32(float left, float right) {
    return left + right;
}

float verified_fp_sub_f32(float left, float right) {
    return left - right;
}

float verified_fp_mul_f32(float left, float right) {
    return left * right;
}

float verified_fp_div_f32(float left, float right) {
    return left / right;
}

double verified_fp_add_f64(double left, double right) {
    return left + right;
}

double verified_fp_sub_f64(double left, double right) {
    return left - right;
}

double verified_fp_mul_f64(double left, double right) {
    return left * right;
}

double verified_fp_div_f64(double left, double right) {
    return left / right;
}

double verified_fp_promote_f32(double left, float right) {
    return left + right;
}

int verified_fp_predicates_f32(float left, float right) {
    return (left == right) | ((left != right) << 1) |
        ((left < right) << 2) | ((left <= right) << 3) |
        ((left > right) << 4) | ((left >= right) << 5);
}

int verified_fp_predicates_f64(double left, double right) {
    return (left == right) | ((left != right) << 1) |
        ((left < right) << 2) | ((left <= right) << 3) |
        ((left > right) << 4) | ((left >= right) << 5);
}

int verified_fp_truth_f32(float value) {
    if (value) return 1;
    return 0;
}

int verified_fp_truth_f64(double value) {
    if (value) return 1;
    return 0;
}

float verified_fp_neg_f32(float value) {
    return -value;
}

double verified_fp_neg_f64(double value) {
    return -value;
}

float verified_fp_compound_f32(float value, float rhs) {
    float plus = value;
    float minus = value;
    float multiply = value;
    float divide = value;
    plus += rhs;
    minus -= rhs;
    multiply *= rhs;
    divide /= rhs;
    return plus + minus * 2.0f + multiply * 4.0f + divide * 8.0f;
}

double verified_fp_compound_f64(double value, double rhs) {
    double plus = value;
    double minus = value;
    double multiply = value;
    double divide = value;
    plus += rhs;
    minus -= rhs;
    multiply *= rhs;
    divide /= rhs;
    return plus + minus * 2.0 + multiply * 4.0 + divide * 8.0;
}

float verified_fp_incdec_f32(float value) {
    float post_increment = value++;
    float pre_increment = ++value;
    float post_decrement = value--;
    float pre_decrement = --value;
    return post_increment * 1000.0f + pre_increment * 100.0f +
        post_decrement * 10.0f + pre_decrement;
}

double verified_fp_incdec_f64(double value) {
    double post_increment = value++;
    double pre_increment = ++value;
    double post_decrement = value--;
    double pre_decrement = --value;
    return post_increment * 1000.0 + pre_increment * 100.0 +
        post_decrement * 10.0 + pre_decrement;
}

float verified_fp_from_i32(int value) {
    return (float)value;
}

double verified_fp_from_i64(long long value) {
    return (double)value;
}

float verified_fp_from_u32(unsigned int value) {
    return (float)value;
}

int verified_fp_to_i32(float value) {
    return (int)value;
}

long long verified_fp_to_i64(double value) {
    return (long long)value;
}

unsigned int verified_fp_to_u32_f32(float value) {
    return (unsigned int)value;
}

unsigned int verified_fp_to_u32_f64(double value) {
    return (unsigned int)value;
}

float verified_fp_mixed_add_f32(float left, int right) {
    return left + right;
}

double verified_fp_mixed_add_f64(double left, unsigned int right) {
    return left + right;
}

int verified_fp_mixed_compare_i32(double left, int right) {
    return left < right;
}

int verified_fp_mixed_compare_u32(float left, unsigned int right) {
    return left != right;
}

float verified_fp_narrow_f64(double value) {
    return (float)value;
}

float verified_fp_from_u64_f32(unsigned long long value) {
    return (float)value;
}

double verified_fp_from_u64_f64(unsigned long long value) {
    return (double)value;
}

unsigned long long verified_fp_to_u64_f32(float value) {
    return (unsigned long long)value;
}

unsigned long long verified_fp_to_u64_f64(double value) {
    return (unsigned long long)value;
}

float verified_fp_pressure(
    float a1, float a2, float a3, float a4,
    float a5, float a6, float a7, float a8,
    float a9, float a10, float a11, float a12,
    float a13, float a14, float a15, float a16,
    float a17, float a18, float a19, float a20,
    float a21, float a22, float a23, float a24,
    float a25, float a26, float a27, float a28,
    float a29, float a30, float a31, float a32) {
    float p0 = a1 + a2;
    float p1 = a3 + a4;
    float p2 = a5 + a6;
    float p3 = a7 + a8;
    float p4 = a9 + a10;
    float p5 = a11 + a12;
    float p6 = a13 + a14;
    float p7 = a15 + a16;
    float p8 = a17 + a18;
    float p9 = a19 + a20;
    float p10 = a21 + a22;
    float p11 = a23 + a24;
    float p12 = a25 + a26;
    float p13 = a27 + a28;
    float p14 = a29 + a30;
    float p15 = a31 + a32;
    return p0 + p1 + p2 + p3 + p4 + p5 + p6 + p7 +
        p8 + p9 + p10 + p11 + p12 + p13 + p14 + p15;
}

#ifdef __cplusplus
}
#endif
