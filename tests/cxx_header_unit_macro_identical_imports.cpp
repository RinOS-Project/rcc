import "cxx_header_unit_macro_semantics_a.h";
import "cxx_header_unit_macro_same_y.h";
#if !defined(RCC_HU_SEMANTICS_Y) || RCC_HU_SEMANTICS_Y != 45
#error identical imported macro definitions were not retained
#endif
int rcc_header_unit_same_import_value = RCC_HU_SEMANTICS_Y;
#undef RCC_HU_SEMANTICS_Y
#ifdef RCC_HU_SEMANTICS_Y
#error undef did not retire every identical active macro definition
#endif
