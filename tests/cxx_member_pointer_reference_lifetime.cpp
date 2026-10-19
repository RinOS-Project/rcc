extern "C" {
int cxx_static_reference_events = 0;
int cxx_static_reference_expected_events = 3269875;
int cxx_static_reference_pointer_selections = 0;
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

struct PointerReferenceDerivedOwner : PointerReferenceOwner {
    int derived_tag;

    ~PointerReferenceDerivedOwner() {
        record_pointer_reference_event(derived_tag);
    }
};

PointerReferenceChild PointerReferenceOwner::*pointer_reference_child =
    &PointerReferenceOwner::child;

static PointerReferenceChild PointerReferenceOwner::*
select_pointer_reference_child(int select_child) {
    ++cxx_static_reference_pointer_selections;
    return select_child ? &PointerReferenceOwner::child : nullptr;
}

const PointerReferenceChild& pointer_reference_global =
    PointerReferenceOwner{5, {47}}.*pointer_reference_child;
const PointerReferenceChild& pointer_reference_cast_global =
    static_cast<PointerReferenceOwner&&>(
        PointerReferenceOwner{7, {61}}).*pointer_reference_child;
const PointerReferenceChild& pointer_reference_derived_global =
    PointerReferenceDerivedOwner{{8, {79}}, 9}.*
        select_pointer_reference_child(1);

static const PointerReferenceChild& pointer_reference_local() {
    static const PointerReferenceChild& value =
        PointerReferenceOwner{6, {53}}.*pointer_reference_child;
    return value;
}

static const PointerReferenceChild& pointer_reference_derived_local() {
    static const PointerReferenceChild& value =
        PointerReferenceDerivedOwner{{2, {83}}, 3}.*
            select_pointer_reference_child(1);
    return value;
}

int main() {
    if (pointer_reference_global.value != 47) return 1;
    if (pointer_reference_cast_global.value != 61) return 3;
    if (pointer_reference_derived_global.value != 79 ||
        cxx_static_reference_pointer_selections != 1) {
        return 4;
    }
    const PointerReferenceChild& first = pointer_reference_local();
    const PointerReferenceChild& second = pointer_reference_local();
    if (first.value != 53 || second.value != 53 || &first != &second ||
        cxx_static_reference_events != 0) {
        return 2;
    }
    const PointerReferenceChild& first_derived =
        pointer_reference_derived_local();
    const PointerReferenceChild& second_derived =
        pointer_reference_derived_local();
    if (first_derived.value != 83 || second_derived.value != 83 ||
        &first_derived != &second_derived ||
        cxx_static_reference_pointer_selections != 2 ||
        cxx_static_reference_events != 0) {
        return 5;
    }
    return 0;
}
