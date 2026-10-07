extern "C" {
int cxx_static_reference_events = 0;
int cxx_static_reference_expected_events = 45678231;
int cxx_static_reference_comma_calls = 0;
int cxx_static_reference_condition_calls = 0;
}

static void record_static_reference_event(int value) {
    cxx_static_reference_events =
            cxx_static_reference_events * 10 + value;
}

struct StaticReferenceSubobjectOwner {
    int value;

    ~StaticReferenceSubobjectOwner() {
        record_static_reference_event(value);
    }
};

struct StaticReferenceBase {
    int value;

    explicit StaticReferenceBase(int input) : value(input) {}

    ~StaticReferenceBase() {
        record_static_reference_event(3);
    }
};

struct StaticReferenceDerived : StaticReferenceBase {
    explicit StaticReferenceDerived(int input) : StaticReferenceBase(input) {}

    ~StaticReferenceDerived() {
        record_static_reference_event(2);
    }
};

struct StaticReferenceVirtualBase {
    int value;

    explicit StaticReferenceVirtualBase(int input) : value(input) {}

    ~StaticReferenceVirtualBase() {
        record_static_reference_event(6);
    }
};

struct StaticReferenceVirtualDerived : virtual StaticReferenceVirtualBase {
    explicit StaticReferenceVirtualDerived(int input)
        : StaticReferenceVirtualBase(input) {}

    ~StaticReferenceVirtualDerived() {
        record_static_reference_event(5);
    }
};

static int record_comma_operand() {
    ++cxx_static_reference_comma_calls;
    return 0;
}

static int select_conditional_operand() {
    ++cxx_static_reference_condition_calls;
    return 1;
}

const int& global_member_reference =
        StaticReferenceSubobjectOwner{1}.value;
const StaticReferenceBase& global_base_reference =
        static_cast<StaticReferenceBase&&>(
                StaticReferenceDerived{3});
const int& global_conditional_member_reference =
        (select_conditional_operand()
             ? StaticReferenceSubobjectOwner{8}
             : StaticReferenceSubobjectOwner{9}).value;
const int& global_comma_member_reference =
        (record_comma_operand(), StaticReferenceSubobjectOwner{7}).value;
const StaticReferenceVirtualBase& global_virtual_base_reference =
        static_cast<StaticReferenceVirtualBase&&>(
                StaticReferenceVirtualDerived{11});

const int& local_member_reference() {
    static const int& value = StaticReferenceSubobjectOwner{4}.value;
    return value;
}

extern "C" int main() {
    if (global_member_reference != 1 || global_base_reference.value != 3 ||
            global_conditional_member_reference != 8 ||
            global_comma_member_reference != 7 ||
            global_virtual_base_reference.value != 11 ||
            cxx_static_reference_comma_calls != 1 ||
            cxx_static_reference_condition_calls != 1) {
        return 1;
    }
    const int& first_local = local_member_reference();
    const int& second_local = local_member_reference();
    if (first_local != 4 || second_local != 4 ||
            &first_local != &second_local || cxx_static_reference_events != 0) {
        return 2;
    }
    return 0;
}
