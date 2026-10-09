#ifdef __cplusplus
extern "C" {
#endif

float verified_i686_add_f32(float lhs, float rhs) { return lhs + rhs; }
float verified_i686_sub_f32(float lhs, float rhs) { return lhs - rhs; }
float verified_i686_mul_f32(float lhs, float rhs) { return lhs * rhs; }
float verified_i686_div_f32(float lhs, float rhs) { return lhs / rhs; }

double verified_i686_add_f64(double lhs, double rhs) { return lhs + rhs; }
double verified_i686_sub_f64(double lhs, double rhs) { return lhs - rhs; }
double verified_i686_mul_f64(double lhs, double rhs) { return lhs * rhs; }
double verified_i686_div_f64(double lhs, double rhs) { return lhs / rhs; }

float verified_i686_sum3_f32(float first, float second, float third) {
    return first + second + third;
}

double verified_i686_sum3_f64(
    double first, double second, double third) {
    return first + second + third;
}

extern float verified_i686_external_add_f32(float, float);
extern double verified_i686_external_add_f64(double, double);
extern double verified_i686_external_mix(int, double, int, double);

float verified_i686_call_f32(float lhs, float rhs) {
    return verified_i686_external_add_f32(lhs, rhs);
}

double verified_i686_call_f64(double lhs, double rhs) {
    return verified_i686_external_add_f64(lhs, rhs);
}

double verified_i686_call_mix(int first, double second,
                              int third, double fourth) {
    return verified_i686_external_mix(first, second, third, fourth);
}

#ifdef __cplusplus
}
#endif
