static int destruction_events;
static int comma_evaluations;
static int conditional_temporary_constructions;

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
        : ConditionalReferenceBase(initial_value) {
        ++conditional_temporary_constructions;
    }

    ~ConditionalReferenceFirst() {
        destruction_events = destruction_events * 10 + 1;
    }
};

struct ConditionalReferenceSecond : ConditionalReferenceBase {
    explicit ConditionalReferenceSecond(int initial_value)
        : ConditionalReferenceBase(initial_value) {
        ++conditional_temporary_constructions;
    }

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
    result = verify_selected_temporary(0);
    if (result) return result;

    for (int deep_case = 0; deep_case != 3; ++deep_case) {
        int deep_take_first = deep_case == 0;
        int deep_take_inner = deep_case == 1;
        int expected_value = deep_case == 0 ? 100
                             : deep_case == 1 ? 101 : 165;
        int expected_destruction = deep_case == 0 ? 13 : 23;
        destruction_events = 0;
        conditional_temporary_constructions = 0;
        {
            const ConditionalReferenceBase&& deep_reference =
+            deep_take_first
                ? static_cast<ConditionalReferenceBase&&>(
                      ConditionalReferenceFirst{100})
                :
              deep_take_inner
                  ? static_cast<ConditionalReferenceBase&&>(
                        ConditionalReferenceSecond{101})
                  :
                deep_take_inner
                    ? static_cast<ConditionalReferenceBase&&>(
                          ConditionalReferenceFirst{102})
                    :
                  deep_take_inner
                      ? static_cast<ConditionalReferenceBase&&>(
                            ConditionalReferenceSecond{103})
                      :
                    deep_take_inner
                        ? static_cast<ConditionalReferenceBase&&>(
                              ConditionalReferenceFirst{104})
                        :
                      deep_take_inner
                          ? static_cast<ConditionalReferenceBase&&>(
                                ConditionalReferenceSecond{105})
                          :
                        deep_take_inner
                            ? static_cast<ConditionalReferenceBase&&>(
                                  ConditionalReferenceFirst{106})
                            :
                          deep_take_inner
                              ? static_cast<ConditionalReferenceBase&&>(
                                    ConditionalReferenceSecond{107})
                              :
                            deep_take_inner
                                ? static_cast<ConditionalReferenceBase&&>(
                                      ConditionalReferenceFirst{108})
                                :
                              deep_take_inner
                                  ? static_cast<ConditionalReferenceBase&&>(
                                        ConditionalReferenceSecond{109})
                                  :
                                deep_take_inner
                                    ? static_cast<ConditionalReferenceBase&&>(
                                          ConditionalReferenceFirst{110})
                                    :
                                  deep_take_inner
                                      ? static_cast<ConditionalReferenceBase&&>(
                                            ConditionalReferenceSecond{111})
                                      :
                                    deep_take_inner
                                        ? static_cast<ConditionalReferenceBase&&>(
                                              ConditionalReferenceFirst{112})
                                        :
                                      deep_take_inner
                                          ? static_cast<ConditionalReferenceBase&&>(
                                                ConditionalReferenceSecond{113})
                                          :
                                        deep_take_inner
                                            ? static_cast<ConditionalReferenceBase&&>(
                                                  ConditionalReferenceFirst{114})
                                            :
                                          deep_take_inner
                                              ? static_cast<ConditionalReferenceBase&&>(
                                                    ConditionalReferenceSecond{115})
                                              :
                                            deep_take_inner
                                                ? static_cast<ConditionalReferenceBase&&>(
                                                      ConditionalReferenceFirst{116})
                                                :
                                              deep_take_inner
                                                  ? static_cast<ConditionalReferenceBase&&>(
                                                        ConditionalReferenceSecond{117})
                                                  :
                                                deep_take_inner
                                                    ? static_cast<ConditionalReferenceBase&&>(
                                                          ConditionalReferenceFirst{118})
                                                    :
                                                  deep_take_inner
                                                      ? static_cast<ConditionalReferenceBase&&>(
                                                            ConditionalReferenceSecond{119})
                                                      :
                                                    deep_take_inner
                                                        ? static_cast<ConditionalReferenceBase&&>(
                                                              ConditionalReferenceFirst{120})
                                                        :
                                                      deep_take_inner
                                                          ? static_cast<ConditionalReferenceBase&&>(
                                                                ConditionalReferenceSecond{121})
                                                          :
                                                        deep_take_inner
                                                            ? static_cast<ConditionalReferenceBase&&>(
                                                                  ConditionalReferenceFirst{122})
                                                            :
                                                          deep_take_inner
                                                              ? static_cast<ConditionalReferenceBase&&>(
                                                                    ConditionalReferenceSecond{123})
                                                              :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceFirst{124})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceSecond{125})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceFirst{126})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceSecond{127})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceFirst{128})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceSecond{129})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceFirst{130})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceSecond{131})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceFirst{132})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceSecond{133})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceFirst{134})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceSecond{135})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceFirst{136})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceSecond{137})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceFirst{138})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceSecond{139})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceFirst{140})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceSecond{141})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceFirst{142})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceSecond{143})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceFirst{144})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceSecond{145})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceFirst{146})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceSecond{147})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceFirst{148})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceSecond{149})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceFirst{150})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceSecond{151})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceFirst{152})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceSecond{153})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceFirst{154})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceSecond{155})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceFirst{156})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceSecond{157})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceFirst{158})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceSecond{159})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceFirst{160})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceSecond{161})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceFirst{162})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceSecond{163})
                                                                :
                                                            deep_take_inner
                                                                ? static_cast<ConditionalReferenceBase&&>(
                                                                      ConditionalReferenceFirst{164})
                                                                :
            static_cast<ConditionalReferenceBase&&>(
                ConditionalReferenceSecond{165});
            if (deep_reference.value != expected_value ||
                conditional_temporary_constructions != 1 ||
                destruction_events != 0) {
                return 7;
            }
        }
        if (destruction_events != expected_destruction ||
            conditional_temporary_constructions != 1) {
            return 8;
        }
    }
    return 0;
}
