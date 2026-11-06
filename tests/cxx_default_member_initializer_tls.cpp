extern "C" {
int cxx_default_member_tls_initializations;
int cxx_default_member_tls_destructions;
}

static int initialize_thread_default_member() {
    ++cxx_default_member_tls_initializations;
    return 43;
}

struct ThreadDefaultMemberInitializer {
    int value = initialize_thread_default_member();

    ~ThreadDefaultMemberInitializer() {
        ++cxx_default_member_tls_destructions;
    }
};

static ThreadDefaultMemberInitializer& thread_default_member_initializer() {
    static thread_local ThreadDefaultMemberInitializer value;
    return value;
}

extern "C" int cxx_default_member_initializer_tls_test(void) {
    if (thread_default_member_initializer().value != 43 ||
        thread_default_member_initializer().value != 43 ||
        cxx_default_member_tls_initializations != 1 ||
        cxx_default_member_tls_destructions != 0) {
        return 1;
    }
    return 0;
}
