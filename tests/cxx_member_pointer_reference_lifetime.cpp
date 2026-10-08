extern "C" {
int cxx_static_reference_events = 0;
int cxx_static_reference_expected_events = 65;
}

static void record_pointer_reference_event(int value) {
    cxx_static_reference_events =
        cxx_static_reference_events * 10 + value;
}

struct PointerReferenceChild {
    int value;
};

struct PointerReferenceOwner {
    int tag;
    PointerReferenceChild child;

    ~PointerReferenceOwner() {
        record_pointer_reference_event(tag);
    }
};

PointerReferenceChild PointerReferenceOwner::*pointer_reference_child =
    &PointerReferenceOwner::child;

const PointerReferenceChild& pointer_reference_global =
    PointerReferenceOwner{5, {47}}.*pointer_reference_child;

static const PointerReferenceChild& pointer_reference_local() {
    static const PointerReferenceChild& value =
        PointerReferenceOwner{6, {53}}.*pointer_reference_child;
    return value;
}

int main() {
    if (pointer_reference_global.value != 47) return 1;
    const PointerReferenceChild& first = pointer_reference_local();
    const PointerReferenceChild& second = pointer_reference_local();
    if (first.value != 53 || second.value != 53 || &first != &second ||
        cxx_static_reference_events != 0) {
        return 2;
    }
    return 0;
}
