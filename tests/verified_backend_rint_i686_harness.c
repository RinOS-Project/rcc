extern float verified_rintf(float value);
extern double verified_rint(double value);
extern double verified_rintl_as_double(double value);
extern void verified_rint_discard(double value);

static unsigned short get_x87_status(void) {
    unsigned short status;
    __asm__ __volatile__("fnstsw %0" : "=m"(status));
    return status;
}

static void set_x87_rounding(unsigned short mode) {
    unsigned short control;
    __asm__ __volatile__("fnstcw %0" : "=m"(control));
    control = (unsigned short)((control & (unsigned short)~0x0c00u) | mode);
    __asm__ __volatile__("fldcw %0" : : "m"(control) : "memory");
}

static void clear_x87_inexact(void) {
    struct { unsigned char bytes[32]; } environment;
    __asm__ __volatile__("fnstenv %0" : "=m"(environment) : : "memory");
    environment.bytes[4] = (unsigned char)(environment.bytes[4] & 0xdfu);
    __asm__ __volatile__("fldenv %0" : : "m"(environment) : "memory");
}

int main(void) {
    static const unsigned short modes[] = {
        0x0000u, 0x0400u, 0x0800u, 0x0c00u
    };
    static const float positive_f[] = {2.0f, 2.0f, 3.0f, 2.0f};
    static const float negative_f[] = {-2.0f, -3.0f, -2.0f, -2.0f};
    static const double positive[] = {2.0, 2.0, 3.0, 2.0};
    static const double negative[] = {-2.0, -3.0, -2.0, -2.0};
    unsigned short original_control;
    union { double floating; unsigned long long bits; } negative_zero;

    __asm__ __volatile__("fnstcw %0" : "=m"(original_control));
    for (unsigned int index = 0u; index < 4u; ++index) {
        set_x87_rounding(modes[index]);
        if (verified_rintf(2.5f) != positive_f[index] ||
            verified_rintf(-2.5f) != negative_f[index] ||
            verified_rint(2.5) != positive[index] ||
            verified_rint(-2.5) != negative[index] ||
            verified_rintl_as_double(2.5) != positive[index] ||
            verified_rintl_as_double(-2.5) != negative[index]) {
            return 1;
        }
    }

    set_x87_rounding(0x0000u);
    negative_zero.floating = verified_rint(-0.25);
    if (negative_zero.bits != 0x8000000000000000ULL) return 2;

    clear_x87_inexact();
    verified_rint_discard(1.25);
    if ((get_x87_status() & 0x0020u) == 0u) return 3;

    __asm__ __volatile__("fldcw %0" : : "m"(original_control) : "memory");
    return 0;
}
