#define RCC_QUOTED_HEADER "preprocessor_include_header.h"
#define RCC_ANGLE_HEADER <preprocessor_include_header.h>

#include RCC_QUOTED_HEADER
#include RCC_ANGLE_HEADER
#include "rcc_utf8_bom.h"

#define errno (*__errno_location())
#include <errno.h>
#ifndef RCC_BOOTSTRAP_ERRNO_H
#error "a direct header-name must not macro-expand its path"
#endif

int preprocessor_include_c =
    RCC_PREPROCESSOR_INCLUDE_VALUE + RCC_PREPROCESSOR_INCLUDE_VALUE;
int preprocessor_include_bom_c = RCC_UTF8_BOM_VALUE;
