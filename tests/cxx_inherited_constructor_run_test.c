#include <assert.h>
#include <stdio.h>

#if defined(_WIN32) && defined(__x86_64__)
#define RIN_SYSV __attribute__((sysv_abi))
#else
#define RIN_SYSV
#endif

int RIN_SYSV cxx_inherited_constructor_probe(void);

int main(void) {
    int result = cxx_inherited_constructor_probe();
    if (result != 116) {
        fprintf(stderr, "inherited constructor probe returned %d\n", result);
        return 1;
    }
    puts("C++ inherited constructor execution test passed");
    return 0;
}
