#if defined(RCC_HEADER_UNIT_REDEFINE_BASELINE)
#undef RCC_HEADER_UNIT_BASELINE_MACRO
#define RCC_HEADER_UNIT_BASELINE_MACRO 29
#endif

import "cxx_header_unit_macro_undef_header.h";

#if defined(RCC_HEADER_UNIT_REDEFINE_BASELINE)
#if !defined(RCC_HEADER_UNIT_BASELINE_MACRO) || \
    RCC_HEADER_UNIT_BASELINE_MACRO != 29
#error header-unit import replaced a translation-unit macro change
#endif
#else
#ifdef RCC_HEADER_UNIT_BASELINE_MACRO
#error header-unit import did not propagate the macro undefinition
#endif
#endif

#if !defined(RCC_HEADER_UNIT_IMPORTED_MACRO) || \
    RCC_HEADER_UNIT_IMPORTED_MACRO != 73
#error header-unit import did not propagate the macro definition
#endif

int rcc_header_unit_imported_macro_value = RCC_HEADER_UNIT_IMPORTED_MACRO;

#if defined(RCC_HEADER_UNIT_REDEFINE_BASELINE)
int rcc_header_unit_baseline_macro_value = RCC_HEADER_UNIT_BASELINE_MACRO;
#endif
