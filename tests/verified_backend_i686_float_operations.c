#ifdef __cplusplus
extern "C" {
#endif

float verified_i686_negate_f32(float value) { return -value; }
double verified_i686_negate_f64(double value) { return -value; }

float verified_i686_update_add_f32(float* value, float rhs) {
    return *value += rhs;
}
float verified_i686_update_sub_f32(float* value, float rhs) {
    return *value -= rhs;
}
float verified_i686_update_mul_f32(float* value, float rhs) {
    return *value *= rhs;
}
float verified_i686_update_div_f32(float* value, float rhs) {
    return *value /= rhs;
}
double verified_i686_update_add_f64(double* value, double rhs) {
    return *value += rhs;
}
double verified_i686_update_sub_f64(double* value, double rhs) {
    return *value -= rhs;
}
double verified_i686_update_mul_f64(double* value, double rhs) {
    return *value *= rhs;
}
double verified_i686_update_div_f64(double* value, double rhs) {
    return *value /= rhs;
}

float verified_i686_preinc_f32(float* value) { return ++(*value); }
float verified_i686_predec_f32(float* value) { return --(*value); }
float verified_i686_postinc_f32(float* value) { return (*value)++; }
float verified_i686_postdec_f32(float* value) { return (*value)--; }
double verified_i686_preinc_f64(double* value) { return ++(*value); }
double verified_i686_predec_f64(double* value) { return --(*value); }
double verified_i686_postinc_f64(double* value) { return (*value)++; }
double verified_i686_postdec_f64(double* value) { return (*value)--; }

float verified_i686_select_f32(float condition, float yes, float no) {
    return condition ? yes : no;
}
double verified_i686_select_f64(double condition, double yes, double no) {
    return condition ? yes : no;
}

#ifdef __cplusplus
}
#endif
