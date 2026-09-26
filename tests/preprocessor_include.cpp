#define RCC_QUOTED_HEADER "preprocessor_include_header.h"
#define RCC_ANGLE_HEADER <preprocessor_include_header.h>

#include RCC_QUOTED_HEADER
#include RCC_ANGLE_HEADER

constexpr int preprocessor_include_cxx =
    RCC_PREPROCESSOR_INCLUDE_VALUE + RCC_PREPROCESSOR_INCLUDE_VALUE;
