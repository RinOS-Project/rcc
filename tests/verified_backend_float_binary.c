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
