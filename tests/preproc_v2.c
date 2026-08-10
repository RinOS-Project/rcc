#include "include/preproc_v2_nested.h"

#define RCC_SUM(first, ...) ((first) + (__VA_ARGS__))

#if !defined(RCC_DISABLED) && (defined(__RCC__) || defined(RCC_FALLBACK))
int rcc_preproc_value = RCC_SUM(RCC_NESTED_VALUE, 2 + 3);
#else
int rcc_preproc_broken = 1;
#endif
