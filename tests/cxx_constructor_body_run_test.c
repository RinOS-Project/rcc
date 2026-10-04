#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32) && defined(__x86_64__)
#define RIN_SYSV __attribute__((sysv_abi))
#else
#define RIN_SYSV
#endif

int RIN_SYSV cxx_constructor_body(void) __asm__("_Z20cxx_constructor_bodyv");
int RIN_SYSV cxx_local_constructor(void) __asm__("_Z21cxx_local_constructorv");

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
    assert(cxx_constructor_body() == 0);
    assert(cxx_local_constructor() == 0);
    puts("C++ constructor body execution test passed");
    return 0;
}
