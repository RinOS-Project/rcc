#include <stddef.h>
#include <stdlib.h>

#if defined(_WIN32) && defined(__x86_64__)
#define RIN_SYSV __attribute__((sysv_abi))
#else
#define RIN_SYSV
#endif

RIN_SYSV void* rin_malloc(unsigned long size)
{
    return malloc((size_t)size);
}

RIN_SYSV void rin_free(void* pointer)
{
    free(pointer);
}

extern int RIN_SYSV _rcc_entry(void);
extern void RIN_SYSV __rcc_global_init(void) __attribute__((weak));

int main(void)
{
    if (__rcc_global_init) __rcc_global_init();
    return _rcc_entry();
}
