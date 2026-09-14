#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int cxx_new_array_constructor(void);
int cxx_new_array_default_constructor(void);
int cxx_new_scalar_default_constructor(void);

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

int main(void)
{
    assert(cxx_new_array_constructor() == 15);
    assert(cxx_new_array_default_constructor() == 14);
    assert(cxx_new_scalar_default_constructor() == 7);
    puts("C++ constructor array-new execution test passed");
    return 0;
}
