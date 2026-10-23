#if !defined(__STDC_HOSTED__)
#error "RCC must define __STDC_HOSTED__"
#endif
#if !defined(__FREESTANDING__)
#error "RCC must define __FREESTANDING__ with -ffreestanding"
#endif

int rcc_freestanding_hosted_value = __STDC_HOSTED__;

#if __STDC_HOSTED__ != 0
#error "freestanding preprocessing must report __STDC_HOSTED__ as 0"
#endif

typedef __SIZE_TYPE__ rcc_predefined_size_type;
typedef __PTRDIFF_TYPE__ rcc_predefined_ptrdiff_type;
typedef __INTPTR_TYPE__ rcc_predefined_intptr_type;
typedef __UINTPTR_TYPE__ rcc_predefined_uintptr_type;
_Static_assert(sizeof(rcc_predefined_size_type) == sizeof(void*),
               "__SIZE_TYPE__ must match the target pointer width");
_Static_assert(sizeof(rcc_predefined_ptrdiff_type) == sizeof(void*),
               "__PTRDIFF_TYPE__ must match the target pointer width");
_Static_assert(sizeof(rcc_predefined_intptr_type) == sizeof(void*),
               "__INTPTR_TYPE__ must match the target pointer width");
_Static_assert(sizeof(rcc_predefined_uintptr_type) == sizeof(void*),
               "__UINTPTR_TYPE__ must match the target pointer width");
_Static_assert(sizeof(__INT8_TYPE__) == 1 && sizeof(__UINT8_TYPE__) == 1 &&
                   sizeof(__INT16_TYPE__) == 2 &&
                   sizeof(__UINT16_TYPE__) == 2 &&
                   sizeof(__INT32_TYPE__) == 4 &&
                   sizeof(__UINT32_TYPE__) == 4 &&
                   sizeof(__INT64_TYPE__) == 8 &&
                   sizeof(__UINT64_TYPE__) == 8,
               "fixed-width predefined integer types must match their widths");
_Static_assert(sizeof(__WCHAR_TYPE__) == 4 &&
                   sizeof(__CHAR16_TYPE__) == 2 &&
                   sizeof(__CHAR32_TYPE__) == 4,
               "wide and UTF character predefined types must match the ABI");

int rcc_freestanding_macro_probe(void) {
    return rcc_freestanding_hosted_value == 0 ? 0 : 1;
}
