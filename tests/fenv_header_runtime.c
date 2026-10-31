#include <fenv.h>

#pragma STDC FENV_ACCESS ON

static double fenv_access_rounding_probe(void)
{
    return 1.0 + 0x1p-54;
}

int main(void) {
    fenv_t saved_environment;
    volatile double input = 1.25;
    volatile long double extended_input = 1.25L;
    int requested = FE_DIVBYZERO | FE_INEXACT;
    int source_value = 149;
    int copied_value = 0;
    int* source_pointer = &source_value;
    int* destination_pointer = &copied_value;

    __asm__ __volatile__("movl %1, %%eax\n\tmovl %%eax, %0"
                         : "=m"(*destination_pointer)
                         : "m"(*source_pointer)
                         : "eax", "memory");
    if (copied_value != source_value) return 7;
    __asm__ __volatile__("addl $1, %0" : "+m"(*destination_pointer));
    if (copied_value != source_value + 1) return 8;

    if (fegetenv(&saved_environment) != 0) return 1;
    if (fesetround(FE_DOWNWARD) != 0 ||
        fegetround() != FE_DOWNWARD ||
        __builtin_rint(input) != 1.0 ||
        __builtin_rintl(extended_input) != 1.0L) return 2;
    if (fesetround(FE_UPWARD) != 0 ||
        fegetround() != FE_UPWARD ||
        __builtin_rint(input) != 2.0 ||
        __builtin_rintl(extended_input) != 2.0L) return 3;
    if (feclearexcept(FE_ALL_EXCEPT) != 0 ||
        fenv_access_rounding_probe() <= 1.0 ||
        (fetestexcept(FE_INEXACT) & FE_INEXACT) == 0) return 7;
    if (fesetround(FE_DOWNWARD) != 0 ||
        feclearexcept(FE_ALL_EXCEPT) != 0 ||
        fenv_access_rounding_probe() != 1.0 ||
        (fetestexcept(FE_INEXACT) & FE_INEXACT) == 0) return 8;
    if (feraiseexcept(requested) != 0 ||
        (fetestexcept(FE_ALL_EXCEPT) & requested) != requested) return 4;
    if (feclearexcept(requested) != 0 ||
        fetestexcept(FE_ALL_EXCEPT) != 0) return 5;
    if (fesetenv(&saved_environment) != 0) return 6;
    return 0;
}
