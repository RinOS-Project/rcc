#include <assert.h>
#include <stdio.h>

#if defined(_WIN32) && defined(__x86_64__)
#define RIN_SYSV __attribute__((sysv_abi))
#else
#define RIN_SYSV
#endif

int RIN_SYSV cxx_inherited_constructor_probe(void);

int main(void) {
    assert(cxx_inherited_constructor_probe() == 24);
    puts("C++ inherited constructor execution test passed");
    return 0;
}
