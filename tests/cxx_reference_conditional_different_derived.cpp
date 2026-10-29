static int destruction_events;

struct ConditionalReferenceBase {
    int value;

    explicit ConditionalReferenceBase(int initial_value)
        : value(initial_value) {}

    ~ConditionalReferenceBase() {
        destruction_events = destruction_events * 10 + 3;
    }
};

struct ConditionalReferenceFirst : ConditionalReferenceBase {
    explicit ConditionalReferenceFirst(int initial_value)
        : ConditionalReferenceBase(initial_value) {}

    ~ConditionalReferenceFirst() {
        destruction_events = destruction_events * 10 + 1;
    }
};

struct ConditionalReferenceSecond : ConditionalReferenceBase {
    explicit ConditionalReferenceSecond(int initial_value)
        : ConditionalReferenceBase(initial_value) {}

    ~ConditionalReferenceSecond() {
        destruction_events = destruction_events * 10 + 2;
    }
};

static int verify_selected_temporary(int select_first) {
    destruction_events = 0;
    {
        const ConditionalReferenceBase&& reference = select_first
            ? static_cast<ConditionalReferenceBase&&>(
                  ConditionalReferenceFirst{11})
            : static_cast<ConditionalReferenceBase&&>(
                  ConditionalReferenceSecond{22});
        if (reference.value != (select_first ? 11 : 22) ||
            destruction_events != 0) {
            return 1;
        }
    }
    return destruction_events == (select_first ? 13 : 23) ? 0 : 2;
}

int main() {
    int result = verify_selected_temporary(1);
    if (result) return result;
    return verify_selected_temporary(0);
}
