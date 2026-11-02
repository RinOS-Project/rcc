#include <fenv.h>

static double rint(double value)
{
    volatile double half_ulp = 0x1p-53;
    return value + half_ulp;
}

#pragma STDC FENV_ROUND FE_UPWARD
double fenv_round_up_constant(void)
{
    return 1.0 + 0x1p-53;
}

double fenv_round_up_runtime(double value)
{
    return value + 0x1p-53;
}

float fenv_round_up_float_constant(void)
{
    return 1.0 + 0x1p-24;
}

float fenv_round_up_float_runtime(double value)
{
    return value;
}

double fenv_round_up_math(void)
{
    return rint(1.0);
}

#pragma STDC FENV_ROUND FE_DOWNWARD
double fenv_round_down_constant(void)
{
    return 1.0 + 0x1p-53;
}

double fenv_round_down_runtime(double value)
{
    return value + 0x1p-53;
}

float fenv_round_down_float_constant(void)
{
    return 1.0 + 0x1p-24;
}

float fenv_round_down_float_runtime(double value)
{
    return value;
}

double fenv_round_down_math(void)
{
    return rint(1.0);
}

#pragma STDC FENV_ROUND FE_TONEAREST
double fenv_round_nearest_constant(void)
{
    return 1.0 + 0x1p-53;
}

#pragma STDC FENV_ROUND FE_TOWARDZERO
double fenv_round_zero_negative_constant(void)
{
    return -1.0 - 0x1p-53;
}

double fenv_round_zero_negative_runtime(double value)
{
    return value - 0x1p-53;
}

#pragma STDC FENV_ROUND FE_DYNAMIC
double fenv_round_dynamic_runtime(double value)
{
    return value + 0x1p-53;
}

void fenv_round_nested(double value, volatile double* results)
{
    results[0] = value + 0x1p-53;
    {
#pragma STDC FENV_ROUND FE_DOWNWARD
        results[1] = value + 0x1p-53;
        {
#pragma STDC FENV_ROUND FE_UPWARD
            results[2] = value + 0x1p-53;
        }
        results[3] = value + 0x1p-53;
    }
    results[4] = value + 0x1p-53;
}

#pragma STDC FENV_ROUND FE_UPWARD
double fenv_round_after_nested_scope(void)
{
    return 1.0 + 0x1p-53;
}

static void set_rounding(unsigned short x87_rounding,
                         unsigned int mxcsr_rounding)
{
    unsigned short control;
    unsigned int mxcsr;
    __asm__ volatile("fnstcw %0\n\tstmxcsr %1"
                     : "=m"(control), "=m"(mxcsr));
    control = (unsigned short)((control & (unsigned short)~0x0c00u) |
                               x87_rounding);
    mxcsr = (mxcsr & ~0x6000u) | mxcsr_rounding;
    __asm__ volatile("fldcw %0\n\tldmxcsr %1"
                     : : "m"(control), "m"(mxcsr) : "memory");
}

int main(void)
{
    unsigned short original_control;
    unsigned int original_mxcsr;
    volatile double results[5];
    int status = 0;
    __asm__ volatile("fnstcw %0\n\tstmxcsr %1"
                     : "=m"(original_control), "=m"(original_mxcsr));

    set_rounding(FE_DOWNWARD, FE_DOWNWARD << 3);
    if (fenv_round_up_constant() <= 1.0) status = 11;
    else if (fenv_round_up_runtime(1.0) <= 1.0) status = 12;
    else if (fenv_round_up_float_constant() <= 1.0f) status = 13;
    else if (fenv_round_up_float_runtime(1.0 + 0x1p-24) <= 1.0f) status = 14;
    else if (fenv_round_up_math() <= 1.0) status = 15;

    set_rounding(FE_UPWARD, FE_UPWARD << 3);
    if (status == 0 && fenv_round_down_constant() != 1.0) status = 21;
    else if (status == 0 && fenv_round_down_runtime(1.0) != 1.0) status = 22;
    else if (status == 0 && fenv_round_down_float_constant() != 1.0f) status = 23;
    else if (status == 0 && fenv_round_down_float_runtime(1.0 + 0x1p-24) != 1.0f) status = 24;
    else if (status == 0 && fenv_round_down_math() != 1.0) status = 25;
    else if (status == 0 && fenv_round_nearest_constant() != 1.0) status = 26;
    else if (status == 0 && fenv_round_zero_negative_constant() != -1.0) status = 27;
    else if (status == 0 && fenv_round_zero_negative_runtime(-1.0) != -1.0) status = 28;

    set_rounding(FE_DOWNWARD, FE_DOWNWARD << 3);
    if (status == 0 && fenv_round_dynamic_runtime(1.0) != 1.0) status = 31;
    fenv_round_nested(1.0, results);
    if (status == 0 && (results[0] != 1.0 || results[1] != 1.0 ||
                        results[2] <= 1.0 || results[3] != 1.0 ||
                        results[4] != 1.0)) {
        status = 41;
    }

    set_rounding(FE_UPWARD, FE_UPWARD << 3);
    if (status == 0 && fenv_round_dynamic_runtime(1.0) <= 1.0) status = 51;
    fenv_round_nested(1.0, results);
    if (status == 0 && (results[0] <= 1.0 || results[1] != 1.0 ||
                        results[2] <= 1.0 || results[3] != 1.0 ||
                        results[4] <= 1.0 ||
                        fenv_round_after_nested_scope() <= 1.0)) {
        status = 61;
    }

    __asm__ volatile("ldmxcsr %0\n\tfldcw %1"
                     : : "m"(original_mxcsr), "m"(original_control)
                     : "memory");
    return status;
}
