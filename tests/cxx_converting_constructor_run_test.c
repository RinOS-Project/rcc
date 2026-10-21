#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32) && defined(__x86_64__)
#define RIN_SYSV __attribute__((sysv_abi))
#else
#define RIN_SYSV
#endif

int RIN_SYSV cxx_converting_constructor(void);
extern void RIN_SYSV __rcc_global_init(void) __attribute__((weak));

RIN_SYSV void* rin_malloc(unsigned long size)
{
    return malloc((size_t)size);
}

RIN_SYSV void rin_free(void* pointer)
{
    free(pointer);
}

int main(void)
{
    int result;
    if (__rcc_global_init) __rcc_global_init();
    result = cxx_converting_constructor();
    if (result != 0) {
        fprintf(stderr, "C++ converting constructor test returned %d\n", result);
    }
    assert(result == 0);
    puts("C++ converting constructor execution test passed");
    return 0;
}
