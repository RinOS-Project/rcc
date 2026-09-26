#if !defined(__STDC_HOSTED__)
#error "RCC++ must define __STDC_HOSTED__"
#endif
#if !defined(__FREESTANDING__)
#error "RCC++ must define __FREESTANDING__ with -ffreestanding"
#endif

constexpr int rcc_cpp_freestanding_hosted_value = __STDC_HOSTED__;

#if __STDC_HOSTED__ != 0
#error "freestanding preprocessing must report __STDC_HOSTED__ as 0"
#endif

int rcc_cpp_freestanding_macro_probe() {
    return rcc_cpp_freestanding_hosted_value == 0 ? 0 : 1;
}
