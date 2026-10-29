#include <fenv.h>

int fenv_header_compile_probe(unsigned int rounding, unsigned int exceptions)
{
    fenv_t environment;
    int result = fegetenv(&environment);
    if (result != 0) return result;
    result = fesetround((int)rounding);
    result += feraiseexcept((int)exceptions);
    result += fetestexcept(FE_ALL_EXCEPT);
    result += feclearexcept((int)exceptions);
    return result + fegetround() + fesetenv(&environment);
}
