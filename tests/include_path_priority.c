#include <limits.h>

#if defined(__cplusplus)
static_assert(RCC_INCLUDE_PATH_PRIORITY_VALUE == 0x5A,
              "command-line include directories precede system headers");
#else
_Static_assert(RCC_INCLUDE_PATH_PRIORITY_VALUE == 0x5A,
               "command-line include directories precede system headers");
#endif

int include_path_priority(void) {
    return RCC_INCLUDE_PATH_PRIORITY_VALUE;
}
