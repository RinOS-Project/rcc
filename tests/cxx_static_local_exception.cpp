extern "C" {
extern int cxx_static_local_attempts;
extern int cxx_static_local_guard_aborts;
extern int cxx_static_local_guard_releases;
extern int cxx_static_local_registrations;
extern int cxx_static_local_finalize_calls;
extern int cxx_static_local_dso_mismatches;
extern int cxx_static_local_registration_dso_mismatches;
extern int cxx_static_local_destructors;
extern int cxx_static_local_destruction_order;
void cxx_static_local_finalize();
}

class RetryStatic final {
public:
    int value;

    RetryStatic() : value(71) {
        ++cxx_static_local_attempts;
        if (cxx_static_local_attempts == 1) {
            throw 37;
        }
    }

    ~RetryStatic() {
        ++cxx_static_local_destructors;
        cxx_static_local_destruction_order =
            cxx_static_local_destruction_order * 10 + 1;
    }
};

class LaterStatic final {
public:
    int value;

    LaterStatic() : value(29) {}

    ~LaterStatic() {
        ++cxx_static_local_destructors;
        cxx_static_local_destruction_order =
            cxx_static_local_destruction_order * 10 + 2;
    }
};

static RetryStatic& retry_static() {
    static RetryStatic value;
    return value;
}

static LaterStatic& later_static() {
    static LaterStatic value;
    return value;
}

extern "C" int main() {
    int caught = 0;
    try {
        (void)retry_static().value;
    } catch (int value) {
        caught = value;
    }
    if (caught != 37 || cxx_static_local_attempts != 1 ||
        cxx_static_local_guard_aborts != 1 ||
        cxx_static_local_guard_releases != 0 ||
        cxx_static_local_registrations != 0 ||
        cxx_static_local_destructors != 0) {
        return 1;
    }

    if (retry_static().value != 71 || cxx_static_local_attempts != 2 ||
        cxx_static_local_guard_aborts != 1 ||
        cxx_static_local_guard_releases != 1 ||
        cxx_static_local_registrations != 1 ||
        cxx_static_local_destructors != 0) {
        return 2;
    }
    if (later_static().value != 29 || later_static().value != 29 ||
        cxx_static_local_guard_releases != 2 ||
        cxx_static_local_registrations != 2 ||
        cxx_static_local_attempts != 2 ||
        cxx_static_local_destructors != 0) {
        return 3;
    }
    if (cxx_static_local_registration_dso_mismatches != 0) return 3;

    cxx_static_local_finalize();
    if (cxx_static_local_finalize_calls != 1 ||
        cxx_static_local_dso_mismatches != 0 ||
        cxx_static_local_destructors != 2 ||
        cxx_static_local_destruction_order != 21) {
        return 4;
    }
    cxx_static_local_finalize();
    if (cxx_static_local_finalize_calls != 2 ||
        cxx_static_local_dso_mismatches != 0 ||
        cxx_static_local_destructors != 2 ||
        cxx_static_local_destruction_order != 21) {
        return 5;
    }
    return 0;
}
