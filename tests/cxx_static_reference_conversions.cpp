extern "C" {
int cxx_static_reference_events = 0;
int cxx_static_reference_expected_events = 2211;
}

static void record_static_reference_event(int value) {
    cxx_static_reference_events =
            cxx_static_reference_events * 10 + value;
}

struct StaticReferenceConvertedValue {
    int value;

    explicit StaticReferenceConvertedValue(int input) : value(input) {}

    ~StaticReferenceConvertedValue() {
        record_static_reference_event(1);
    }
};

struct StaticReferenceConversionSource {
    int value;

    operator StaticReferenceConvertedValue() {
        record_static_reference_event(2);
        return StaticReferenceConvertedValue(value);
    }
};

const StaticReferenceConvertedValue& global_converted_reference =
        StaticReferenceConversionSource{17};

static const StaticReferenceConvertedValue& local_converted_reference() {
    static const StaticReferenceConvertedValue& value =
            StaticReferenceConversionSource{23};
    return value;
}

extern "C" int main() {
    if (global_converted_reference.value != 17 ||
            cxx_static_reference_events != 2) {
        return 1;
    }
    const StaticReferenceConvertedValue& first = local_converted_reference();
    const StaticReferenceConvertedValue& second = local_converted_reference();
    if (first.value != 23 || second.value != 23 || &first != &second ||
            cxx_static_reference_events != 22) {
        return 2;
    }
    return 0;
}
