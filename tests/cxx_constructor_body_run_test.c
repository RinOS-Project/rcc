#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int cxx_constructor_body(void) __asm__("_Z20cxx_constructor_bodyv");

void* rin_malloc(unsigned long size)
{
    return malloc((size_t)size);
}

void rin_free(void* pointer)
{
    free(pointer);
}

int main(void)
{
    assert(cxx_constructor_body() == 0);
    puts("C++ constructor body execution test passed");
    return 0;
}
