import "cxx_header_unit_macro_nested_definition.h";
import "cxx_header_unit_macro_nested_redefine.h";
#if !defined(RCC_HU_NESTED_UNDEF) || RCC_HU_NESTED_UNDEF != 29
#error nested header-unit redefinition did not replace the imported definition
#endif
int rcc_header_unit_nested_redefine_value = RCC_HU_NESTED_UNDEF;
