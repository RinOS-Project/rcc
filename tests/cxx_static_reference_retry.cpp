extern "C" int cxx_static_reference_retry_attempts;
extern "C" int cxx_static_reference_guard_aborts;
extern "C" int cxx_static_reference_guard_releases;
extern "C" int cxx_static_reference_caught_value;

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
    return 0;
}
