#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int cxx_base_constructor_initializer(void);

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
    assert(cxx_base_constructor_initializer() == 0);
    puts("C++ fixed-layout base constructor initializer test passed");
    return 0;
}
