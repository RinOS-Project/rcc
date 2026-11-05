import "cxx_header_unit_macro_nested_definition.h";
import "cxx_header_unit_macro_nested_undef.h";
#ifdef RCC_HU_NESTED_UNDEF
#error nested header-unit undefinition did not reach the importer
#endif
int rcc_header_unit_nested_undef_value;
