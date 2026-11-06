#include <assert.h>
#include <stdlib.h>

#if defined(_WIN32) && defined(__x86_64__)
#define RIN_SYSV __attribute__((sysv_abi))
#else
#define RIN_SYSV
#endif

int RIN_SYSV cxx_default_member_initializer(void);
extern int cxx_default_member_static_initializations;
extern int cxx_default_member_static_destructions;
extern int cxx_static_reference_atexit_registrations;
extern int cxx_static_reference_atexit_finalizations;
void cxx_static_reference_retry_finalize(void);

RIN_SYSV void* rin_malloc(unsigned long size) {
    return malloc((size_t)size);
}

RIN_SYSV void rin_free(void* pointer) {
    free(pointer);
}

int main(void) {
    assert(cxx_default_member_initializer() == 0);
    assert(cxx_default_member_static_initializations == 1);
    assert(cxx_default_member_static_destructions == 0);
    assert(cxx_static_reference_atexit_registrations == 1);
    assert(cxx_static_reference_atexit_finalizations == 0);
    cxx_static_reference_retry_finalize();
    assert(cxx_default_member_static_destructions == 1);
    assert(cxx_static_reference_atexit_finalizations == 1);
    cxx_static_reference_retry_finalize();
    assert(cxx_default_member_static_destructions == 1);
    assert(cxx_static_reference_atexit_finalizations == 1);
    return 0;
}
