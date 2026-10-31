#include <math.h>

#pragma STDC FENV_ACCESS ON

extern "C" int fenv_header_compile_cpp_probe(unsigned int rounding,
                                              unsigned int exceptions)
{
    fenv_t environment;
    int result = fegetenv(&environment);
    if (result != 0) return result;
    result += fesetround(static_cast<int>(rounding));
    result += feraiseexcept(static_cast<int>(exceptions));
    result += fetestexcept(FE_ALL_EXCEPT);
    result += feclearexcept(static_cast<int>(exceptions));
    return result + fegetround() + fesetenv(&environment);
}

#pragma STDC FENV_ACCESS OFF
