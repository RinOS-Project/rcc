#define RCC_INCLUDE_NEXT_FIRST 11

#if !defined(__has_include_next)
#error "__has_include_next must be reported as a supported operator"
#endif
#if !__has_include_next("rcc_include_next.h")
#error "__has_include_next did not search after the current header directory"
#endif
#if __has_include_next("rcc_missing_include_next.h")
#error "__has_include_next reported a missing later header"
#endif

#include_next <rcc_include_next.h>
