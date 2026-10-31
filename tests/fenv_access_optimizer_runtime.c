#include <stdint.h>

#if defined(__MINGW32__)
/* The freestanding Windows test links without MinGW's CRT startup objects. */
void __main(void)
{
}
#endif

extern double rcc_fenv_access_rounding_probe(void);
extern double rcc_fenv_access_runtime_probe(double value);

static void fenv_set_rounding(uint16_t x87_rounding,
                              uint32_t mxcsr_rounding)
{
    uint16_t control;
    uint32_t mxcsr;
    __asm__ volatile ("fnstcw %0" : "=m"(control));
    __asm__ volatile ("stmxcsr %0" : "=m"(mxcsr));
    control = (uint16_t)((control & (uint16_t)~0x0C00u) |
                         x87_rounding);
    mxcsr = (mxcsr & ~UINT32_C(0x0000603F)) | mxcsr_rounding;
    __asm__ volatile ("fnclex\n\tldmxcsr %0\n\tfldcw %1"
                      : : "m"(mxcsr), "m"(control) : "memory");
}

static int fenv_inexact_is_set(void)
{
    uint16_t x87_status;
    uint32_t mxcsr;
    __asm__ volatile ("fnstsw %0\n\tstmxcsr %1"
                      : "=m"(x87_status), "=m"(mxcsr));
    return ((x87_status | mxcsr) & (1u << 5)) != 0u;
}

int main(void)
{
    uint16_t original_x87;
    uint32_t original_mxcsr;
    volatile double upward;
    volatile double upward_runtime;
    volatile double downward;
    volatile double downward_runtime;
    int valid;
    __asm__ volatile ("fnstcw %0" : "=m"(original_x87));
    __asm__ volatile ("stmxcsr %0" : "=m"(original_mxcsr));

    fenv_set_rounding(0x0800u, 0x00004000u);
    upward = rcc_fenv_access_rounding_probe();
    valid = upward > 1.0 && fenv_inexact_is_set();
    fenv_set_rounding(0x0800u, 0x00004000u);
    upward_runtime = rcc_fenv_access_runtime_probe(1.0);
    valid = valid && upward_runtime > 1.0 && fenv_inexact_is_set();

    fenv_set_rounding(0x0400u, 0x00002000u);
    downward = rcc_fenv_access_rounding_probe();
    valid = valid && downward == 1.0 && fenv_inexact_is_set();
    fenv_set_rounding(0x0400u, 0x00002000u);
    downward_runtime = rcc_fenv_access_runtime_probe(1.0);
    valid = valid && downward_runtime == 1.0 && fenv_inexact_is_set();

    __asm__ volatile ("ldmxcsr %0\n\tfldcw %1"
                      : : "m"(original_mxcsr), "m"(original_x87)
                      : "memory");
    return valid ? 0 : 1;
}
