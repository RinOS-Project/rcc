extern "C" {
int cxx_static_reference_events = 0;
int cxx_static_reference_expected_events = 21;
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
