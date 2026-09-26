#if !defined(__STDC_HOSTED__)
#error "RCC must define __STDC_HOSTED__"
#endif
#if !defined(__STDC_NO_COMPLEX__)
#error "RCC must define __STDC_NO_COMPLEX__ when complex ABI is unavailable"
#endif
#if __STDC_NO_COMPLEX__ != 1
#error "RCC must report the unsupported complex ABI as 1"
#endif
#if !defined(__STDC_NO_THREADS__)
#error "RCC must define __STDC_NO_THREADS__ without C11 threads.h"
#endif
#if __STDC_NO_THREADS__ != 1
#error "RCC must report the unavailable C11 threads API as 1"
#endif

int rcc_standard_hosted_value = __STDC_HOSTED__;

#if __STDC_HOSTED__ != 1
#error "hosted preprocessing must report __STDC_HOSTED__ as 1"
#endif

int rcc_standard_macro_probe(void) {
    return rcc_standard_hosted_value == 1 ? 0 : 1;
}
