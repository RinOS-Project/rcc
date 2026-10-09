#ifdef __cplusplus
extern "C" {
#endif

float verified_i686_s32_to_f32(int value) { return (float)value; }
double verified_i686_s32_to_f64(int value) { return (double)value; }
float verified_i686_u32_to_f32(unsigned value) { return (float)value; }
double verified_i686_u32_to_f64(unsigned value) { return (double)value; }
float verified_i686_s64_to_f32(long long value) { return (float)value; }
double verified_i686_s64_to_f64(long long value) { return (double)value; }
float verified_i686_u64_to_f32(unsigned long long value) {
    return (float)value;
}
double verified_i686_u64_to_f64(unsigned long long value) {
    return (double)value;
}

int verified_i686_f32_to_s32(float value) { return (int)value; }
int verified_i686_f64_to_s32(double value) { return (int)value; }
unsigned verified_i686_f32_to_u32(float value) { return (unsigned)value; }
unsigned verified_i686_f64_to_u32(double value) { return (unsigned)value; }
long long verified_i686_f32_to_s64(float value) {
    return (long long)value;
}
long long verified_i686_f64_to_s64(double value) {
    return (long long)value;
}
unsigned long long verified_i686_f32_to_u64(float value) {
    return (unsigned long long)value;
}
unsigned long long verified_i686_f64_to_u64(double value) {
    return (unsigned long long)value;
}

double verified_i686_f32_to_f64(float value) { return (double)value; }
float verified_i686_f64_to_f32(double value) { return (float)value; }

#ifdef __cplusplus
}
#endif
