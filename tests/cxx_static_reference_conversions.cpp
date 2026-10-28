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

struct StaticReferenceConvertedBase {
    int* destruction_order;
    int* destruction_count;
    int value;

    StaticReferenceConvertedBase(int* order, int* count, int initial_value)
        : destruction_order(order), destruction_count(count),
          value(initial_value) {}

    ~StaticReferenceConvertedBase() {
        destruction_order[(*destruction_count)++] = 1;
    }
};

struct StaticReferenceConvertedDerived : StaticReferenceConvertedBase {
    StaticReferenceConvertedDerived(int* order, int* count, int initial_value)
        : StaticReferenceConvertedBase(order, count, initial_value) {}

    ~StaticReferenceConvertedDerived() {
        destruction_order[(*destruction_count)++] = 2;
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
    int destruction_order[2] = {0, 0};
    int destruction_count = 0;
    {
        StaticReferenceConvertedBase&& base =
                static_cast<StaticReferenceConvertedBase&&>(
                        StaticReferenceConvertedDerived(
                                destruction_order, &destruction_count, 42));
        if (base.value != 42 || destruction_count != 0) return 3;
    }
    if (destruction_count != 2 || destruction_order[0] != 2 ||
            destruction_order[1] != 1) {
        return 4;
    }
    destruction_count = 0;
    {
        StaticReferenceConvertedBase&& selected =
                static_cast<StaticReferenceConvertedBase&&>(
                        true
                                ? StaticReferenceConvertedDerived(
                                          destruction_order,
                                          &destruction_count, 55)
                                : StaticReferenceConvertedDerived(
                                          destruction_order,
                                          &destruction_count, 66));
        if (selected.value != 55 || destruction_count != 0) return 5;
    }
    if (destruction_count != 2 || destruction_order[0] != 2 ||
            destruction_order[1] != 1) {
        return 6;
    }
    return 0;
}
