static int destruction_events;
static int temporary_constructions;

struct MixedReferenceBase {
    int value;

    explicit MixedReferenceBase(int initial_value) : value(initial_value) {}

    ~MixedReferenceBase() {
        destruction_events = destruction_events * 10 + 3;
    }
};

struct MixedReferenceFirst : MixedReferenceBase {
    explicit MixedReferenceFirst(int initial_value)
        : MixedReferenceBase(initial_value) {
        ++temporary_constructions;
    }

    ~MixedReferenceFirst() {
        destruction_events = destruction_events * 10 + 1;
    }
};

struct MixedReferenceSecond : MixedReferenceBase {
    explicit MixedReferenceSecond(int initial_value)
        : MixedReferenceBase(initial_value) {
        ++temporary_constructions;
    }

    ~MixedReferenceSecond() {
        destruction_events = destruction_events * 10 + 2;
    }
};

static int verify_mixed_conditional(int temporary_is_then, int choose_then) {
    const bool selected_temporary = temporary_is_then == choose_then;
    const int expected_value = selected_temporary
        ? (temporary_is_then ? 81 : 82)
        : (temporary_is_then ? 72 : 71);
    const int selected_destruction = selected_temporary
        ? (temporary_is_then ? 13 : 23)
        : 0;
    destruction_events = 0;
    temporary_constructions = 0;
    {
        MixedReferenceFirst named_first{71};
        MixedReferenceSecond named_second{72};
        destruction_events = 0;
        temporary_constructions = 0;
        if (temporary_is_then) {
            {
                const MixedReferenceBase&& reference = choose_then
                    ? static_cast<MixedReferenceBase&&>(
                          MixedReferenceFirst{81})
                    : static_cast<MixedReferenceBase&&>(named_second);
                if (reference.value != expected_value ||
                    destruction_events != 0 ||
                    (!selected_temporary &&
                     &reference != static_cast<const MixedReferenceBase*>(
                                       &named_second))) {
                    return 1;
                }
            }
        } else {
            {
                const MixedReferenceBase&& reference = choose_then
                    ? static_cast<MixedReferenceBase&&>(named_first)
                    : static_cast<MixedReferenceBase&&>(
                          MixedReferenceSecond{82});
                if (reference.value != expected_value ||
                    destruction_events != 0 ||
                    (!selected_temporary &&
                     &reference != static_cast<const MixedReferenceBase*>(
                                       &named_first))) {
                    return 2;
                }
            }
        }
        if (destruction_events != selected_destruction ||
            temporary_constructions != (selected_temporary ? 1 : 0)) {
            return 3;
        }
    }
    if (destruction_events !=
        (selected_temporary ? selected_destruction * 10000 + 2313 : 2313)) {
        return 4;
    }
    return 0;
}

int main() {
    int result = verify_mixed_conditional(0, 0);
    if (result) return result;
    result = verify_mixed_conditional(0, 1);
    if (result) return result;
    result = verify_mixed_conditional(1, 0);
    if (result) return result;
    return verify_mixed_conditional(1, 1);
}
