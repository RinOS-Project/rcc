#include <assert.h>
#include <stdio.h>

int cxx_inherited_constructor_probe(void);

int main(void) {
    assert(cxx_inherited_constructor_probe() == 14);
    puts("C++ inherited constructor execution test passed");
    return 0;
}
