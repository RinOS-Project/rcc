#include <assert.h>
#include <stdlib.h>

#if defined(_WIN32) && defined(__x86_64__)
#define RIN_SYSV __attribute__((sysv_abi))
#else
#define RIN_SYSV
#endif

int RIN_SYSV cxx_default_member_initializer(void);

RIN_SYSV void* rin_malloc(unsigned long size) {
    return malloc((size_t)size);
}

RIN_SYSV void rin_free(void* pointer) {
    free(pointer);
}

int main(void) {
    assert(cxx_default_member_initializer() == 0);
    return 0;
}
