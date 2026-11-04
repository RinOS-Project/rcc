static int destruction_events;
static int comma_evaluations;

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

struct ConditionalReferencePayload {
    int value;

    explicit ConditionalReferencePayload(int initial_value)
        : value(initial_value) {}

    ~ConditionalReferencePayload() {
        destruction_events = destruction_events * 10 + 4;
    }
};

struct ConditionalReferenceEnvelope {
    ConditionalReferencePayload payload;

    explicit ConditionalReferenceEnvelope(int initial_value)
        : payload(initial_value) {}

    ~ConditionalReferenceEnvelope() {
        destruction_events = destruction_events * 10 + 5;
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
    if (destruction_events != (select_first ? 13 : 23)) return 2;

    for (int choose_outer = 0; choose_outer != 2; ++choose_outer) {
        for (int choose_inner = 0; choose_inner != 2; ++choose_inner) {
            for (int use_wrapper = 0; use_wrapper != 2; ++use_wrapper) {
                int expected_value = choose_outer
                    ? choose_inner ? 51 : 52
                    : 53;
                int expected_destruction =
                    choose_outer && choose_inner ? 13 : 23;
                destruction_events = 0;
                comma_evaluations = 0;
                {
                    const ConditionalReferenceBase&& reference =
                        choose_outer
                        ? (use_wrapper
                            ? (++comma_evaluations,
                               static_cast<ConditionalReferenceBase&&>(
                                   choose_inner
                                   ? static_cast<ConditionalReferenceBase&&>(
                                         ConditionalReferenceFirst{51})
                                   : static_cast<ConditionalReferenceBase&&>(
                                         ConditionalReferenceSecond{52})))
                            : (choose_inner
                                ? static_cast<ConditionalReferenceBase&&>(
                                      ConditionalReferenceFirst{51})
                                : static_cast<ConditionalReferenceBase&&>(
                                      ConditionalReferenceSecond{52})))
                        : static_cast<ConditionalReferenceBase&&>(
                              ConditionalReferenceSecond{53});
                    if (reference.value != expected_value ||
                        destruction_events != 0 ||
                        comma_evaluations !=
                            (choose_outer && use_wrapper ? 1 : 0)) {
                        return 3;
                    }
                }
                if (destruction_events != expected_destruction) return 4;
            }
        }
    }

    for (int choose_outer = 0; choose_outer != 2; ++choose_outer) {
        for (int choose_inner = 0; choose_inner != 2; ++choose_inner) {
            int expected_value = choose_outer
                ? choose_inner ? 61 : 62
                : 63;
            destruction_events = 0;
            {
                const ConditionalReferencePayload&& reference =
                    choose_outer
                    ? (choose_inner
                        ? static_cast<ConditionalReferenceEnvelope&&>(
                              ConditionalReferenceEnvelope{61})
                        : static_cast<ConditionalReferenceEnvelope&&>(
                              ConditionalReferenceEnvelope{62})).payload
                    : static_cast<ConditionalReferencePayload&&>(
                          ConditionalReferencePayload{63});
                if (reference.value != expected_value ||
                    destruction_events != 0) {
                    return 5;
                }
            }
            if (destruction_events != (choose_outer ? 54 : 4)) return 6;
        }
    }
    return 0;
}

int main() {
    int result = verify_selected_temporary(1);
    if (result) return result;
    return verify_selected_temporary(0);
}
