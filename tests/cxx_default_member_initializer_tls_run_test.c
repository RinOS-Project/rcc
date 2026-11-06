#include <assert.h>

int cxx_default_member_initializer_tls_test(void);
extern int cxx_default_member_tls_initializations;
extern int cxx_default_member_tls_destructions;
extern int cxx_static_reference_thread_atexit_registrations;
extern int cxx_static_reference_thread_atexit_finalizations;
void cxx_static_reference_thread_finalize(void);

int main(void) {
    assert(cxx_default_member_initializer_tls_test() == 0);
    assert(cxx_default_member_tls_initializations == 1);
    assert(cxx_default_member_tls_destructions == 0);
    assert(cxx_static_reference_thread_atexit_registrations == 1);
    assert(cxx_static_reference_thread_atexit_finalizations == 0);
    cxx_static_reference_thread_finalize();
    assert(cxx_default_member_tls_destructions == 1);
    assert(cxx_static_reference_thread_atexit_finalizations == 1);
    cxx_static_reference_thread_finalize();
    assert(cxx_default_member_tls_destructions == 1);
    assert(cxx_static_reference_thread_atexit_finalizations == 1);
    return 0;
}
