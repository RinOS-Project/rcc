extern float verified_i686_add_f32(float, float);
extern float verified_i686_sub_f32(float, float);
extern float verified_i686_mul_f32(float, float);
extern float verified_i686_div_f32(float, float);
extern double verified_i686_add_f64(double, double);
extern double verified_i686_sub_f64(double, double);
extern double verified_i686_mul_f64(double, double);
extern double verified_i686_div_f64(double, double);
extern float verified_i686_sum3_f32(float, float, float);
extern double verified_i686_sum3_f64(double, double, double);
extern float verified_i686_call_f32(float, float);
extern double verified_i686_call_f64(double, double);
extern double verified_i686_call_mix(int, double, int, double);

#if defined(__MINGW32__)
void rcc_mingw_main_hook(void) __asm__("__main");
void rcc_mingw_main_hook(void) {}
void rcc_mingw_generated_hook(void) __asm__("___main");
void rcc_mingw_generated_hook(void) {}
#endif

float verified_i686_external_add_f32(float lhs, float rhs) {
    return lhs + rhs;
}

double verified_i686_external_add_f64(double lhs, double rhs) {
    return lhs + rhs;
}

double verified_i686_external_mix(
    int first, double second, int third, double fourth) {
    return first + second + third + fourth;
}

int verified_i686_float_test(void) {
    if (verified_i686_add_f32(1.5f, 2.25f) != 3.75f) return 1;
    if (verified_i686_sub_f32(5.5f, 2.0f) != 3.5f) return 2;
    if (verified_i686_mul_f32(1.5f, 2.0f) != 3.0f) return 3;
    if (verified_i686_div_f32(7.5f, 2.5f) != 3.0f) return 4;
    if (verified_i686_add_f64(1.5, 2.25) != 3.75) return 5;
    if (verified_i686_sub_f64(5.5, 2.0) != 3.5) return 6;
    if (verified_i686_mul_f64(1.5, 2.0) != 3.0) return 7;
    if (verified_i686_div_f64(7.5, 2.5) != 3.0) return 8;
    if (verified_i686_sum3_f32(1.25f, 2.5f, 4.0f) != 7.75f) return 9;
    if (verified_i686_sum3_f64(1.25, 2.5, 4.0) != 7.75) return 10;
    if (verified_i686_call_f32(1.25f, 2.75f) != 4.0f) return 11;
    if (verified_i686_call_f64(1.25, 2.75) != 4.0) return 12;
    if (verified_i686_call_mix(3, 1.5, 4, 2.5) != 11.0) return 13;
    return 0;
}

int main(void) {
    return verified_i686_float_test();
}
