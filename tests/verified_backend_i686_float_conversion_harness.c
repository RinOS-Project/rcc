extern float verified_i686_s32_to_f32(int);
extern double verified_i686_s32_to_f64(int);
extern float verified_i686_u32_to_f32(unsigned);
extern double verified_i686_u32_to_f64(unsigned);
extern float verified_i686_s64_to_f32(long long);
extern double verified_i686_s64_to_f64(long long);
extern float verified_i686_u64_to_f32(unsigned long long);
extern double verified_i686_u64_to_f64(unsigned long long);
extern int verified_i686_f32_to_s32(float);
extern int verified_i686_f64_to_s32(double);
extern unsigned verified_i686_f32_to_u32(float);
extern unsigned verified_i686_f64_to_u32(double);
extern long long verified_i686_f32_to_s64(float);
extern long long verified_i686_f64_to_s64(double);
extern unsigned long long verified_i686_f32_to_u64(float);
extern unsigned long long verified_i686_f64_to_u64(double);
extern double verified_i686_f32_to_f64(float);
extern float verified_i686_f64_to_f32(double);

#if defined(__MINGW32__)
void rcc_mingw_main_hook(void) __asm__("__main");
void rcc_mingw_main_hook(void) {}
void rcc_mingw_generated_hook(void) __asm__("___main");
void rcc_mingw_generated_hook(void) {}
#endif

int main(void) {
    if (verified_i686_s32_to_f32(-123456789) != (float)-123456789) return 1;
    if (verified_i686_s32_to_f64(-123456789) != (double)-123456789) return 2;
    if (verified_i686_u32_to_f32(0xffffffffu) != 4294967296.0f) return 3;
    if (verified_i686_u32_to_f64(0xffffffffu) != 4294967295.0) return 4;
    if (verified_i686_s64_to_f32(-9223372036854775807LL - 1LL) !=
        -9223372036854775808.0f) return 5;
    if (verified_i686_s64_to_f64(-9223372036854775807LL - 1LL) !=
        -9223372036854775808.0) return 6;
    if (verified_i686_u64_to_f32(18446744073709551615ULL) !=
        18446744073709551616.0f) return 7;
    if (verified_i686_u64_to_f64(18446744073709551615ULL) !=
        18446744073709551616.0) return 8;

    if (verified_i686_f32_to_s32(-123.875f) != -123) return 9;
    if (verified_i686_f64_to_s32(-2147483647.75) != -2147483647) return 10;
    if (verified_i686_f32_to_u32(2147483648.0f) != 2147483648u) return 11;
    if (verified_i686_f64_to_u32(4294967295.0) != 0xffffffffu) return 12;
    if (verified_i686_f32_to_s64(-4294967296.0f) != -4294967296LL) return 13;
    if (verified_i686_f64_to_s64(-4294967296.75) != -4294967296LL) return 14;
    if (verified_i686_f32_to_s64(3221225472.0f) != 3221225472LL) return 15;
    if (verified_i686_f64_to_s64(9223372036854774784.0) !=
        9223372036854774784LL) return 16;
    if (verified_i686_f32_to_u64(4294967296.0f) != 4294967296ULL) return 17;
    if (verified_i686_f64_to_u64(18446744073709549568.0) !=
        18446744073709549568ULL) return 18;

    if (verified_i686_f32_to_f64(1.5f) != 1.5) return 19;
    if (verified_i686_f64_to_f32(1.1) != (float)1.1) return 20;
    return 0;
}
