static int first_initializer_calls;
static int second_initializer_calls;
static int default_member_first_calls;
static int default_member_second_calls;
static int reference_initializer_calls;
static int reference_second_initializer_calls;
static int reference_class_initializer_calls;
static int reference_class_second_initializer_calls;
static int reference_nontrivial_initializer_calls;
static int reference_nontrivial_second_initializer_calls;
static int reference_nontrivial_later_initializer_calls;
static int reference_nontrivial_value_destructions;
static int reference_nontrivial_aggregate_saw_live_temporary;
static int reference_nontrivial_unwind_order;
static int reference_nontrivial_final_cleanup_order;
extern "C" {
#ifdef RCC_VERIFIED_STATIC_LOCAL_AGGREGATE_HOST
extern int verified_static_local_aggregate_destructions;
#else
int verified_static_local_aggregate_destructions;
#endif
}

static int make_static_aggregate_first() {
    ++first_initializer_calls;
    if (first_initializer_calls == 1) throw 29;
    return 17;
}

static int make_static_aggregate_second() {
    ++second_initializer_calls;
    return 23;
}

static int make_static_aggregate_default_first() {
    ++default_member_first_calls;
    return 31;
}

static int make_static_aggregate_default_second() {
    ++default_member_second_calls;
    return 41;
}

static int make_static_aggregate_reference_value() {
    ++reference_initializer_calls;
    if (reference_initializer_calls == 1) throw 59;
    return 47;
}

static int make_static_aggregate_reference_second() {
    ++reference_second_initializer_calls;
    return 53;
}

struct StaticLocalReferenceValue {
    int value;
};

static StaticLocalReferenceValue make_static_aggregate_reference_class_value() {
    ++reference_class_initializer_calls;
    return {67};
}

static int make_static_aggregate_reference_class_second() {
    ++reference_class_second_initializer_calls;
    return 71;
}

struct StaticLocalNontrivialReferenceValue {
    int value;

    ~StaticLocalNontrivialReferenceValue() {
        if (reference_nontrivial_aggregate_saw_live_temporary) {
            if (value == 97 && reference_nontrivial_value_destructions == 2) {
                reference_nontrivial_final_cleanup_order = 1;
            } else if (value == 79 &&
                       reference_nontrivial_value_destructions == 3 &&
                       reference_nontrivial_final_cleanup_order == 1) {
                reference_nontrivial_final_cleanup_order = 2;
            }
        } else if (value == 97 &&
                   reference_nontrivial_value_destructions == 0) {
            reference_nontrivial_unwind_order = 1;
        } else if (value == 79 &&
                   reference_nontrivial_value_destructions == 1 &&
                   reference_nontrivial_unwind_order == 1) {
            reference_nontrivial_unwind_order = 2;
        }
        ++reference_nontrivial_value_destructions;
    }
};

static StaticLocalNontrivialReferenceValue
make_static_aggregate_nontrivial_reference_value() {
    ++reference_nontrivial_initializer_calls;
    return {79};
}

static StaticLocalNontrivialReferenceValue
make_static_aggregate_nontrivial_reference_second_value() {
    ++reference_nontrivial_second_initializer_calls;
    return {97};
}

static int make_static_aggregate_nontrivial_reference_later() {
    ++reference_nontrivial_later_initializer_calls;
    if (reference_nontrivial_later_initializer_calls == 1) throw 83;
    return 89;
}

struct StaticLocalAggregate {
    int first;
    int second;

    ~StaticLocalAggregate() {
        ++verified_static_local_aggregate_destructions;
    }
};

static int read_static_local_aggregate() {
    static StaticLocalAggregate value{
        make_static_aggregate_first(), make_static_aggregate_second()};
    return value.first + value.second;
}

struct StaticLocalDefaultMemberAggregate {
    int first = make_static_aggregate_default_first();
    int second = make_static_aggregate_default_second();

    ~StaticLocalDefaultMemberAggregate() {
        ++verified_static_local_aggregate_destructions;
    }
};

static int read_static_local_default_member_aggregate() {
    static StaticLocalDefaultMemberAggregate value{};
    return value.first + value.second;
}

struct StaticLocalReferenceAggregate {
    const int& reference;
    int second;

    ~StaticLocalReferenceAggregate() {
        ++verified_static_local_aggregate_destructions;
    }
};

static const StaticLocalReferenceAggregate&
static_local_reference_aggregate() {
    static StaticLocalReferenceAggregate value{
        make_static_aggregate_reference_value(),
        make_static_aggregate_reference_second()};
    return value;
}

struct StaticLocalClassReferenceAggregate {
    const StaticLocalReferenceValue& reference;
    int second;

    ~StaticLocalClassReferenceAggregate() {
        ++verified_static_local_aggregate_destructions;
    }
};

static const StaticLocalClassReferenceAggregate&
static_local_class_reference_aggregate() {
    static StaticLocalClassReferenceAggregate value{
        make_static_aggregate_reference_class_value(),
        make_static_aggregate_reference_class_second()};
    return value;
}

struct StaticLocalNontrivialClassReferenceAggregate {
    const StaticLocalNontrivialReferenceValue& reference;
    const StaticLocalNontrivialReferenceValue& second_reference;
    int later;

    ~StaticLocalNontrivialClassReferenceAggregate() {
        if (reference_nontrivial_value_destructions == 2) {
            ++reference_nontrivial_aggregate_saw_live_temporary;
        }
        ++verified_static_local_aggregate_destructions;
    }
};

static const StaticLocalNontrivialClassReferenceAggregate&
static_local_nontrivial_class_reference_aggregate() {
    static StaticLocalNontrivialClassReferenceAggregate value{
        make_static_aggregate_nontrivial_reference_value(),
        make_static_aggregate_nontrivial_reference_second_value(),
        make_static_aggregate_nontrivial_reference_later()};
    return value;
}

int main() {
    try {
        (void)read_static_local_aggregate();
        return 1;
    } catch (int error) {
        if (error != 29) return 2;
    }
    if (first_initializer_calls != 1 || second_initializer_calls != 0) {
        return 3;
    }
    if (read_static_local_aggregate() != 40 ||
        first_initializer_calls != 2 || second_initializer_calls != 1) {
        return 4;
    }
    if (read_static_local_aggregate() != 40 ||
        first_initializer_calls != 2 || second_initializer_calls != 1) {
        return 5;
    }
    if (verified_static_local_aggregate_destructions != 0) return 6;
    if (read_static_local_default_member_aggregate() != 72 ||
        default_member_first_calls != 1 || default_member_second_calls != 1) {
        return 7;
    }
    if (read_static_local_default_member_aggregate() != 72 ||
        default_member_first_calls != 1 || default_member_second_calls != 1 ||
        verified_static_local_aggregate_destructions != 0) {
        return 8;
    }
    try {
        (void)static_local_reference_aggregate();
        return 10;
    } catch (int error) {
        if (error != 59) return 11;
    }
    if (reference_initializer_calls != 1 ||
        reference_second_initializer_calls != 0 ||
        verified_static_local_aggregate_destructions != 0) {
        return 12;
    }
    const StaticLocalReferenceAggregate& first_reference =
        static_local_reference_aggregate();
    const StaticLocalReferenceAggregate& second_reference =
        static_local_reference_aggregate();
    if (&first_reference != &second_reference ||
        &first_reference.reference != &second_reference.reference ||
        first_reference.reference != 47 || first_reference.second != 53 ||
        reference_initializer_calls != 2 ||
        reference_second_initializer_calls != 1 ||
        verified_static_local_aggregate_destructions != 0) {
        return 9;
    }
    const StaticLocalClassReferenceAggregate& first_class_reference =
        static_local_class_reference_aggregate();
    const StaticLocalClassReferenceAggregate& second_class_reference =
        static_local_class_reference_aggregate();
    if (&first_class_reference != &second_class_reference ||
        &first_class_reference.reference !=
            &second_class_reference.reference ||
        first_class_reference.reference.value != 67 ||
        first_class_reference.second != 71 ||
        reference_class_initializer_calls != 1 ||
        reference_class_second_initializer_calls != 1 ||
        verified_static_local_aggregate_destructions != 0) {
        return 13;
    }
    try {
        (void)static_local_nontrivial_class_reference_aggregate();
        return 14;
    } catch (int error) {
        if (error != 83) return 15;
    }
    if (reference_nontrivial_initializer_calls != 1 ||
        reference_nontrivial_second_initializer_calls != 1 ||
        reference_nontrivial_later_initializer_calls != 1 ||
        verified_static_local_aggregate_destructions != 0) {
        return 16;
    }
    const StaticLocalNontrivialClassReferenceAggregate& first_nontrivial =
        static_local_nontrivial_class_reference_aggregate();
    const StaticLocalNontrivialClassReferenceAggregate& second_nontrivial =
        static_local_nontrivial_class_reference_aggregate();
    if (&first_nontrivial != &second_nontrivial ||
        &first_nontrivial.reference != &second_nontrivial.reference ||
        &first_nontrivial.second_reference !=
            &second_nontrivial.second_reference ||
        first_nontrivial.later != 89 ||
        reference_nontrivial_initializer_calls != 2 ||
        reference_nontrivial_second_initializer_calls != 2 ||
        reference_nontrivial_later_initializer_calls != 2 ||
        verified_static_local_aggregate_destructions != 0) {
        return 17;
    }
    return 0;
}

extern "C" int verified_static_local_aggregate_destructor_count() {
    return verified_static_local_aggregate_destructions;
}

extern "C" int
verified_static_local_aggregate_reference_temp_destructor_count() {
    return reference_nontrivial_value_destructions;
}

extern "C" int
verified_static_local_aggregate_reference_temp_order_count() {
    return reference_nontrivial_aggregate_saw_live_temporary == 1 &&
        reference_nontrivial_final_cleanup_order == 2;
}

extern "C" int
verified_static_local_aggregate_reference_temp_unwind_order_count() {
    return reference_nontrivial_unwind_order;
}

extern "C" const void*
verified_static_local_aggregate_reference_temp_first_address() {
    const StaticLocalNontrivialClassReferenceAggregate& value =
        static_local_nontrivial_class_reference_aggregate();
    return &value.reference;
}

extern "C" const void*
verified_static_local_aggregate_reference_temp_second_address() {
    const StaticLocalNontrivialClassReferenceAggregate& value =
        static_local_nontrivial_class_reference_aggregate();
    return &value.second_reference;
}
