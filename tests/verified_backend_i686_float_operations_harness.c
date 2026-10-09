extern float verified_i686_negate_f32(float);
extern double verified_i686_negate_f64(double);
extern float verified_i686_update_add_f32(float*, float);
extern float verified_i686_update_sub_f32(float*, float);
extern float verified_i686_update_mul_f32(float*, float);
extern float verified_i686_update_div_f32(float*, float);
extern double verified_i686_update_add_f64(double*, double);
extern double verified_i686_update_sub_f64(double*, double);
extern double verified_i686_update_mul_f64(double*, double);
extern double verified_i686_update_div_f64(double*, double);
extern float verified_i686_preinc_f32(float*);
extern float verified_i686_predec_f32(float*);
extern float verified_i686_postinc_f32(float*);
extern float verified_i686_postdec_f32(float*);
extern double verified_i686_preinc_f64(double*);
extern double verified_i686_predec_f64(double*);
extern double verified_i686_postinc_f64(double*);
extern double verified_i686_postdec_f64(double*);
extern float verified_i686_select_f32(float, float, float);
extern double verified_i686_select_f64(double, double, double);

static float float_from_bits(unsigned int bits) {
    union { unsigned int integer; float floating; } value;
    value.integer = bits;
    return value.floating;
}

static unsigned int float_bits(float floating) {
    union { unsigned int integer; float floating; } value;
    value.floating = floating;
    return value.integer;
}

static double double_from_bits(unsigned long long bits) {
    union { unsigned long long integer; double floating; } value;
    value.integer = bits;
    return value.floating;
}

static unsigned long long double_bits(double floating) {
    union { unsigned long long integer; double floating; } value;
    value.floating = floating;
    return value.integer;
}

int main(void) {
    float f32;
    double f64;

    if (float_bits(verified_i686_negate_f32(0.0f)) != 0x80000000u)
        return 21;
    if (float_bits(verified_i686_negate_f32(float_from_bits(0x80000000u)))
        != 0u) return 22;
    if (float_bits(verified_i686_negate_f32(float_from_bits(0x7fc12345u)))
        != 0xffc12345u) return 23;
    if (double_bits(verified_i686_negate_f64(0.0)) !=
        0x8000000000000000ULL) return 24;
    if (double_bits(verified_i686_negate_f64(double_from_bits(
            0x8000000000000000ULL))) != 0ULL) return 25;
    if (double_bits(verified_i686_negate_f64(double_from_bits(
            0x7ff8123456789abcULL))) != 0xfff8123456789abcULL) return 26;

    f32 = 4.0f;
    if (verified_i686_update_add_f32(&f32, 1.5f) != 5.5f || f32 != 5.5f)
        return 27;
    f32 = 4.0f;
    if (verified_i686_update_sub_f32(&f32, 1.25f) != 2.75f || f32 != 2.75f)
        return 28;
    f32 = 4.0f;
    if (verified_i686_update_mul_f32(&f32, 1.5f) != 6.0f || f32 != 6.0f)
        return 29;
    f32 = 4.0f;
    if (verified_i686_update_div_f32(&f32, 2.0f) != 2.0f || f32 != 2.0f)
        return 30;
    f64 = 4.0;
    if (verified_i686_update_add_f64(&f64, 1.5) != 5.5 || f64 != 5.5)
        return 31;
    f64 = 4.0;
    if (verified_i686_update_sub_f64(&f64, 1.25) != 2.75 || f64 != 2.75)
        return 32;
    f64 = 4.0;
    if (verified_i686_update_mul_f64(&f64, 1.5) != 6.0 || f64 != 6.0)
        return 33;
    f64 = 4.0;
    if (verified_i686_update_div_f64(&f64, 2.0) != 2.0 || f64 != 2.0)
        return 34;

    f32 = 4.0f;
    if (verified_i686_preinc_f32(&f32) != 5.0f || f32 != 5.0f) return 35;
    f32 = 4.0f;
    if (verified_i686_predec_f32(&f32) != 3.0f || f32 != 3.0f) return 36;
    f32 = 4.0f;
    if (verified_i686_postinc_f32(&f32) != 4.0f || f32 != 5.0f) return 37;
    f32 = 4.0f;
    if (verified_i686_postdec_f32(&f32) != 4.0f || f32 != 3.0f) return 38;
    f64 = 4.0;
    if (verified_i686_preinc_f64(&f64) != 5.0 || f64 != 5.0) return 39;
    f64 = 4.0;
    if (verified_i686_predec_f64(&f64) != 3.0 || f64 != 3.0) return 40;
    f64 = 4.0;
    if (verified_i686_postinc_f64(&f64) != 4.0 || f64 != 5.0) return 41;
    f64 = 4.0;
    if (verified_i686_postdec_f64(&f64) != 4.0 || f64 != 3.0) return 42;

    if (verified_i686_select_f32(1.0f, 3.25f, 7.5f) != 3.25f) return 43;
    if (verified_i686_select_f32(0.0f, 3.25f, 7.5f) != 7.5f) return 44;
    if (verified_i686_select_f32(verified_i686_negate_f32(0.0f),
                                 3.25f, 7.5f) != 7.5f) return 45;
    if (verified_i686_select_f64(1.0, 3.25, 7.5) != 3.25) return 46;
    if (verified_i686_select_f64(0.0, 3.25, 7.5) != 7.5) return 47;
    if (float_bits(verified_i686_select_f32(
            1.0f, float_from_bits(0x80000000u), 0.0f)) != 0x80000000u)
        return 48;
    if (double_bits(verified_i686_select_f64(
            1.0, double_from_bits(0x8000000000000000ULL), 0.0)) !=
        0x8000000000000000ULL) return 49;
    return 0;
}
