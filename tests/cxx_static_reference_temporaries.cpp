extern "C" {
int cxx_static_reference_events = 0;
int cxx_static_reference_expected_events = 21;
#ifdef RCC_STATIC_REFERENCE_TLS_TEST
int cxx_static_reference_tls_constructions = 0;
int cxx_static_reference_tls_destructions = 0;
int cxx_static_reference_tls_destruction_order = 0;
int cxx_static_reference_tls_base_constructions = 0;
int cxx_static_reference_tls_derived_constructions = 0;
int cxx_static_reference_tls_base_destructions = 0;
int cxx_static_reference_tls_derived_destructions = 0;
int cxx_static_reference_tls_conversions = 0;
#endif
}

struct StaticReferenceLifetime {
    int value;

    ~StaticReferenceLifetime() {
        cxx_static_reference_events =
                cxx_static_reference_events * 10 + value;
    }
};

const StaticReferenceLifetime& global_static_reference =
        StaticReferenceLifetime{1};

#ifdef RCC_STATIC_REFERENCE_TLS_TEST
struct ThreadLocalReferenceLifetime {
    int value;

    explicit ThreadLocalReferenceLifetime(int initial_value)
        : value(initial_value) {
        ++cxx_static_reference_tls_constructions;
    }

    ~ThreadLocalReferenceLifetime() {
        ++cxx_static_reference_tls_destructions;
        cxx_static_reference_tls_destruction_order =
            cxx_static_reference_tls_destruction_order * 10 + value;
    }
};

struct ThreadLocalReferenceBase {
    int value;

    explicit ThreadLocalReferenceBase(int initial_value)
        : value(initial_value) {
        ++cxx_static_reference_tls_base_constructions;
    }

    ~ThreadLocalReferenceBase() {
        ++cxx_static_reference_tls_base_destructions;
        cxx_static_reference_tls_destruction_order =
            cxx_static_reference_tls_destruction_order * 10 + 1;
    }
};

struct ThreadLocalReferenceDerived : ThreadLocalReferenceBase {
    explicit ThreadLocalReferenceDerived(int initial_value)
        : ThreadLocalReferenceBase(initial_value) {
        ++cxx_static_reference_tls_derived_constructions;
    }

    ~ThreadLocalReferenceDerived() {
        ++cxx_static_reference_tls_derived_destructions;
        cxx_static_reference_tls_destruction_order =
            cxx_static_reference_tls_destruction_order * 10 + 2;
    }
};

struct ThreadLocalReferenceConversionSource {
    int value;

    operator ThreadLocalReferenceDerived() {
        ++cxx_static_reference_tls_conversions;
        return ThreadLocalReferenceDerived{value + 20};
    }
};

thread_local int cxx_static_reference_thread_value;

thread_local const ThreadLocalReferenceLifetime& thread_local_reference =
    ThreadLocalReferenceLifetime{cxx_static_reference_thread_value};

thread_local const ThreadLocalReferenceBase& thread_local_base_reference =
    static_cast<ThreadLocalReferenceBase&&>(
        ThreadLocalReferenceDerived{cxx_static_reference_thread_value + 10});

thread_local const ThreadLocalReferenceBase&
    thread_local_converted_base_reference =
        ThreadLocalReferenceConversionSource{
            cxx_static_reference_thread_value + 20};

extern "C" int cxx_static_reference_tls_worker(int value) {
    cxx_static_reference_thread_value = value;
    const ThreadLocalReferenceLifetime* first = &thread_local_reference;
    const ThreadLocalReferenceLifetime* second = &thread_local_reference;
    if (first != second || first->value != value) return 1;
    const ThreadLocalReferenceBase* base_first = &thread_local_base_reference;
    const ThreadLocalReferenceBase* base_second = &thread_local_base_reference;
    if (base_first != base_second || base_first->value != value + 10) return 2;
    const ThreadLocalReferenceBase* converted_first =
        &thread_local_converted_base_reference;
    const ThreadLocalReferenceBase* converted_second =
        &thread_local_converted_base_reference;
    if (converted_first != converted_second ||
        converted_first->value != value + 40) return 3;
    return 0;
}
#endif

const StaticReferenceLifetime& local_static_reference() {
    static const StaticReferenceLifetime& value =
            StaticReferenceLifetime{2};
    return value;
}

static int scalar_reference_initializations = 0;

int make_static_reference_scalar(int value) {
    ++scalar_reference_initializations;
    return value;
}

const int& global_scalar_reference = make_static_reference_scalar(5);

const int& local_scalar_reference() {
    static const int& value = make_static_reference_scalar(6);
    return value;
}

int main() {
    if (global_scalar_reference != 5) return 1;
    const int& first_local_scalar = local_scalar_reference();
    const int& second_local_scalar = local_scalar_reference();
    if (first_local_scalar != 6) return 2;
    if (second_local_scalar != 6) return 3;
    if (scalar_reference_initializations != 2) return 4;
    if (global_static_reference.value != 1) return 5;
    if (local_static_reference().value != 2) return 6;
    if (local_static_reference().value != 2) return 7;
    return cxx_static_reference_events == 0 ? 0 : 8;
}
