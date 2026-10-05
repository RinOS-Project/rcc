#include "rcc_include_next.h"

#if RCC_INCLUDE_NEXT_FIRST != 11 || RCC_INCLUDE_NEXT_SECOND != 31
#error "#include_next did not resume after the supplying include directory"
#endif

int preprocessor_include_next_c(void) {
    return RCC_INCLUDE_NEXT_FIRST + RCC_INCLUDE_NEXT_SECOND;
}
