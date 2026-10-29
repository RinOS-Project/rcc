static int destruction_events;

struct ConditionalStaticBase {
    int value;

    explicit ConditionalStaticBase(int initial_value)
        : value(initial_value) {}

    ~ConditionalStaticBase() {
        destruction_events = destruction_events * 10 + 3;
    }
};

struct ConditionalStaticFirst : ConditionalStaticBase {
    explicit ConditionalStaticFirst(int initial_value)
        : ConditionalStaticBase(initial_value) {}

    ~ConditionalStaticFirst() {
        destruction_events = destruction_events * 10 + 1;
    }
};

struct ConditionalStaticSecond : ConditionalStaticBase {
    explicit ConditionalStaticSecond(int initial_value)
        : ConditionalStaticBase(initial_value) {}

    ~ConditionalStaticSecond() {
        destruction_events = destruction_events * 10 + 2;
    }
};

static int first_static_reference(int select_first) {
    static const ConditionalStaticBase&& reference = select_first
        ? static_cast<ConditionalStaticBase&&>(
              ConditionalStaticFirst{31})
        : static_cast<ConditionalStaticBase&&>(
              ConditionalStaticSecond{32});
    return destruction_events == 0 && reference.value == 31 ? 0 : 1;
}

static int second_static_reference(int select_first) {
    static const ConditionalStaticBase&& reference = select_first
        ? static_cast<ConditionalStaticBase&&>(
              ConditionalStaticFirst{41})
        : static_cast<ConditionalStaticBase&&>(
              ConditionalStaticSecond{42});
    return destruction_events == 0 && reference.value == 42 ? 0 : 1;
}

extern "C" void cxx_static_reference_retry_finalize(void);

int main() {
    if (first_static_reference(1) || first_static_reference(0)) return 1;
    if (second_static_reference(0)) return 2;
    cxx_static_reference_retry_finalize();
    return destruction_events == 2313 ? 0 : 3;
}
