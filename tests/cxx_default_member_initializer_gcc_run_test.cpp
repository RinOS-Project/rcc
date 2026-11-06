#include <cstdlib>

extern "C" int cxx_default_member_initializer(void);
extern "C" int cxx_default_member_initializer_tls_test(void);
extern "C" int cxx_default_member_static_initializations;
extern "C" int cxx_default_member_static_destructions;
extern "C" int cxx_default_member_tls_initializations;
extern "C" int cxx_default_member_tls_destructions;

static void verify_static_default_member_lifetime() {
    if (cxx_default_member_static_initializations != 1 ||
        cxx_default_member_static_destructions != 1 ||
        cxx_default_member_tls_initializations != 1 ||
        cxx_default_member_tls_destructions != 1) {
        std::abort();
    }
}

int main() {
    std::atexit(verify_static_default_member_lifetime);
    if (cxx_default_member_initializer() != 0 ||
        cxx_default_member_initializer_tls_test() != 0 ||
        cxx_default_member_tls_initializations != 1 ||
        cxx_default_member_tls_destructions != 0) {
        return 1;
    }
    return 0;
}
