#include <preproc_v2_nested.h>

#ifndef RCC_CXX_CLI_VALUE
#error "-D was not applied"
#endif

#ifdef RCC_CXX_REMOVE_ME
#error "-U was not applied"
#endif

static_assert(RCC_CXX_CLI_VALUE == 23, "command-line macro");
static_assert(RCC_NESTED_VALUE == 7, "-I include path");
static_assert(sizeof(long) == 8, "x86_64 target type widths");

int main() {
    return RCC_CXX_CLI_VALUE - 23;
}
