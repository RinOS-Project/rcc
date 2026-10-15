extern "C" {
extern int cxx_static_reference_retry_attempts;
extern int cxx_static_reference_guard_aborts;
extern int cxx_static_reference_guard_releases;
extern int cxx_static_reference_caught_value;
extern int cxx_static_reference_class_attempts;
extern int cxx_static_reference_class_constructions;
extern int cxx_static_reference_class_destructions;
extern int cxx_static_reference_class_destruction_value;
extern int cxx_static_reference_atexit_registrations;
extern int cxx_static_reference_atexit_finalizations;
extern void cxx_static_reference_retry_finalize();
}

static int failed_reference_value;

static int make_retry_value() {
    ++cxx_static_reference_retry_attempts;
    if (cxx_static_reference_retry_attempts == 1) {
        throw 37;
    }
    return 73;
}

const int& retry_static_reference() {
    try {
        static const int& value = make_retry_value();
        return value;
    } catch (int caught) {
        cxx_static_reference_caught_value = caught;
        return failed_reference_value;
    }
}

struct RetryReferenceTemporary {
    int value;

    explicit RetryReferenceTemporary(int initial_value) : value(initial_value) {
        ++cxx_static_reference_class_attempts;
        if (cxx_static_reference_class_attempts == 1) {
            throw 41;
        }
        ++cxx_static_reference_class_constructions;
    }

    ~RetryReferenceTemporary() {
        ++cxx_static_reference_class_destructions;
        cxx_static_reference_class_destruction_value = value;
    }
};

const RetryReferenceTemporary* retry_static_class_reference() {
    try {
        static const RetryReferenceTemporary& value =
            RetryReferenceTemporary{89};
        return &value;
    } catch (int caught) {
        cxx_static_reference_caught_value = caught;
        return nullptr;
    }
}

extern "C" int main() {
    const int& failed = retry_static_reference();
    if (failed != 0 || cxx_static_reference_caught_value != 37 ||
            cxx_static_reference_retry_attempts != 1 ||
            cxx_static_reference_guard_aborts != 1) {
        return 1;
    }

    const int& first = retry_static_reference();
    if (first != 73 || cxx_static_reference_retry_attempts != 2 ||
            cxx_static_reference_guard_releases != 1) {
        return 2;
    }

    const int& second = retry_static_reference();
    if (second != 73 || &first != &second ||
            cxx_static_reference_retry_attempts != 2 ||
            cxx_static_reference_guard_aborts != 1 ||
            cxx_static_reference_guard_releases != 1) {
        return 3;
    }

    if (retry_static_class_reference() != nullptr ||
            cxx_static_reference_caught_value != 41 ||
            cxx_static_reference_class_attempts != 1 ||
            cxx_static_reference_class_constructions != 0 ||
            cxx_static_reference_class_destructions != 0 ||
            cxx_static_reference_guard_aborts != 2 ||
            cxx_static_reference_guard_releases != 1 ||
            cxx_static_reference_atexit_registrations != 0) {
        return 4;
    }

    const RetryReferenceTemporary* class_first =
        retry_static_class_reference();
    if (!class_first || class_first->value != 89 ||
            cxx_static_reference_class_attempts != 2 ||
            cxx_static_reference_class_constructions != 1 ||
            cxx_static_reference_class_destructions != 0 ||
            cxx_static_reference_guard_aborts != 2 ||
            cxx_static_reference_guard_releases != 2 ||
            cxx_static_reference_atexit_registrations != 1) {
        return 5;
    }

    const RetryReferenceTemporary* class_second =
        retry_static_class_reference();
    if (class_second != class_first || class_second->value != 89 ||
            cxx_static_reference_class_attempts != 2 ||
            cxx_static_reference_atexit_registrations != 1 ||
            cxx_static_reference_class_destructions != 0) {
        return 6;
    }

    cxx_static_reference_retry_finalize();
    if (cxx_static_reference_class_destructions != 1 ||
            cxx_static_reference_class_destruction_value != 89 ||
            cxx_static_reference_atexit_finalizations != 1) {
        return 7;
    }
    cxx_static_reference_retry_finalize();
    if (cxx_static_reference_class_destructions != 1 ||
            cxx_static_reference_atexit_finalizations != 1) {
        return 8;
    }
    return 0;
}
