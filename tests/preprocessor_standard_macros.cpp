#if !defined(__STDC_HOSTED__)
#error "RCC++ must define __STDC_HOSTED__"
#endif

constexpr int rcc_cpp_standard_hosted_value = __STDC_HOSTED__;

#if __STDC_HOSTED__ != 1
#error "hosted preprocessing must report __STDC_HOSTED__ as 1"
#endif

int rcc_cpp_standard_macro_probe() {
    return rcc_cpp_standard_hosted_value == 1 ? 0 : 1;
}
