/* SPDX-License-Identifier: MIT */
#include <stdio.h>

#if defined(__GNUC__) && defined(__x86_64__)
extern int rcc_test_main(void) __attribute__((sysv_abi));
#else
extern int rcc_test_main(void);
#endif

int main(void)
{
    int result = rcc_test_main();
    if (result != 0) fprintf(stderr, "atomic test failed at stage %d\n", result);
    return result;
}
