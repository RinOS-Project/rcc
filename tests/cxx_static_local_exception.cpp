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
void cxx_static_local_thread_finalize();
extern int cxx_thread_local_attempts;
extern int cxx_thread_local_destructors;
extern int cxx_thread_local_registrations;
extern int cxx_thread_local_dso_mismatches;
extern int cxx_thread_local_destruction_order;
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

class RetryThreadLocal final {
public:
    int value;

    RetryThreadLocal() : value(83) {
        ++cxx_thread_local_attempts;
        if (cxx_thread_local_attempts == 1) {
            throw 43;
        }
    }

    ~RetryThreadLocal() {
        ++cxx_thread_local_destructors;
        cxx_thread_local_destruction_order =
            cxx_thread_local_destruction_order * 10 + 3;
    }
};

class FirstThreadLocal final {
public:
    int value;

    FirstThreadLocal() : value(4) {}

    ~FirstThreadLocal() {
        ++cxx_thread_local_destructors;
        cxx_thread_local_destruction_order =
            cxx_thread_local_destruction_order * 10 + 4;
    }
};

class SecondThreadLocal final {
public:
    int value;

    SecondThreadLocal() : value(5) {}

    ~SecondThreadLocal() {
        ++cxx_thread_local_destructors;
        cxx_thread_local_destruction_order =
            cxx_thread_local_destruction_order * 10 + 5;
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

static RetryThreadLocal& retry_thread_local() {
    static thread_local RetryThreadLocal value;
    return value;
}

static FirstThreadLocal& first_thread_local() {
    static thread_local FirstThreadLocal value;
    return value;
}

static SecondThreadLocal& second_thread_local() {
    static thread_local SecondThreadLocal value;
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

    caught = 0;
    try {
        (void)retry_thread_local().value;
    } catch (int value) {
        caught = value;
    }
    if (caught != 43) return 60;
    if (cxx_thread_local_attempts != 1) return 61;
    if (cxx_static_local_guard_aborts != 2) return 62;
    if (cxx_static_local_guard_releases != 2) return 63;
    if (cxx_thread_local_registrations != 0) return 64;
    if (cxx_thread_local_destructors != 0) return 65;
    if (retry_thread_local().value != 83 ||
        cxx_thread_local_attempts != 2 ||
        cxx_static_local_guard_aborts != 2 ||
        cxx_static_local_guard_releases != 3 ||
        cxx_thread_local_registrations != 1 ||
        cxx_thread_local_destructors != 0 ||
        cxx_thread_local_dso_mismatches != 0) return 70;
    if (first_thread_local().value != 4 ||
        second_thread_local().value != 5 ||
        cxx_thread_local_registrations != 3 ||
        cxx_thread_local_destructors != 0 ||
        cxx_thread_local_dso_mismatches != 0) return 71;
    cxx_static_local_thread_finalize();
    if (cxx_thread_local_destructors != 3 ||
        cxx_thread_local_destruction_order != 543) return 80;
    cxx_static_local_thread_finalize();
    if (cxx_thread_local_destructors != 3 ||
        cxx_thread_local_destruction_order != 543) return 90;
    return 0;
}
