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
#ifdef RCC_TEST_STATIC_LOCAL_PARTIAL_SUBOBJECT
static int partial_member_initializer_calls;
static int partial_member_later_initializer_calls;
static int partial_member_destructions;
static int partial_member_unwind_order;
static int partial_member_final_cleanup_order;
#endif
#ifdef RCC_TEST_STATIC_LOCAL_PARTIAL_ARRAY_SUBOBJECT
static int partial_array_initializer_calls;
static int partial_array_later_initializer_calls;
static int partial_array_member_destructions;
static int partial_array_unwind_order;
static int partial_array_final_cleanup_order;
#endif
#ifdef RCC_TEST_STATIC_LOCAL_CONSTRUCTOR_SUBOBJECT
static int constructor_member_initializer_calls;
static int constructor_parent_body_calls;
static int constructor_mode_calls;
static int constructor_leaf_destructions;
static int constructor_child_destructions;
static int constructor_unwind_order;
static int constructor_final_cleanup_order;
#endif
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

#ifdef RCC_TEST_STATIC_LOCAL_PARTIAL_SUBOBJECT
static int make_static_aggregate_partial_member_later() {
    ++partial_member_later_initializer_calls;
    if (partial_member_later_initializer_calls == 1) throw 109;
    return 113;
}
#endif

#ifdef RCC_TEST_STATIC_LOCAL_PARTIAL_ARRAY_SUBOBJECT
static int make_static_aggregate_partial_array_later() {
    ++partial_array_later_initializer_calls;
    if (partial_array_later_initializer_calls == 1) throw 139;
    return 151;
}
#endif

#ifdef RCC_TEST_STATIC_LOCAL_CONSTRUCTOR_SUBOBJECT
static int make_static_aggregate_constructor_mode() {
    ++constructor_mode_calls;
    return constructor_mode_calls == 1 ? 1 : 0;
}
#endif

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

#ifdef RCC_TEST_STATIC_LOCAL_PARTIAL_SUBOBJECT
struct StaticLocalPartialMember {
    int value;

    ~StaticLocalPartialMember() {
        if (partial_member_later_initializer_calls == 1) {
            if (value == 107 && partial_member_destructions == 0) {
                partial_member_unwind_order = 1;
            }
        } else if (partial_member_later_initializer_calls == 2) {
            if (value == 113 && partial_member_destructions == 1) {
                partial_member_final_cleanup_order = 1;
            } else if (value == 107 && partial_member_destructions == 2 &&
                       partial_member_final_cleanup_order == 1) {
                partial_member_final_cleanup_order = 2;
            }
        }
        ++partial_member_destructions;
    }
};

struct StaticLocalNestedPartialMember {
    StaticLocalPartialMember first;
    StaticLocalPartialMember second;
};

struct StaticLocalPartialAggregate {
    StaticLocalNestedPartialMember member;
    int later;
};

#endif

#ifdef RCC_TEST_STATIC_LOCAL_PARTIAL_ARRAY_SUBOBJECT
struct StaticLocalArrayPartialMember {
    int value;

    ~StaticLocalArrayPartialMember() {
        if (partial_array_later_initializer_calls == 1) {
            if (value == 137 && partial_array_member_destructions == 0) {
                partial_array_unwind_order = 1;
            } else if (value == 131 &&
                       partial_array_member_destructions == 1 &&
                       partial_array_unwind_order == 1) {
                partial_array_unwind_order = 2;
            }
        } else if (partial_array_later_initializer_calls == 2) {
            if (value == 137 && partial_array_member_destructions == 2) {
                partial_array_final_cleanup_order = 1;
            } else if (value == 131 &&
                       partial_array_member_destructions == 3 &&
                       partial_array_final_cleanup_order == 1) {
                partial_array_final_cleanup_order = 2;
            }
        }
        ++partial_array_member_destructions;
    }
};

struct StaticLocalPartialArrayAggregate {
    StaticLocalArrayPartialMember members[2];
    int later;
};
#endif

#ifdef RCC_TEST_STATIC_LOCAL_CONSTRUCTOR_SUBOBJECT
struct StaticLocalConstructorBuiltLeaf {
    int value;

    explicit StaticLocalConstructorBuiltLeaf(int initial) noexcept
        : value(initial) {}

    ~StaticLocalConstructorBuiltLeaf() {
        if (constructor_mode_calls == 1 &&
            constructor_leaf_destructions == 0) {
            constructor_unwind_order = 1;
        } else if (constructor_mode_calls == 2 &&
                   constructor_child_destructions == 1 &&
                   constructor_leaf_destructions == 1 &&
                   constructor_final_cleanup_order == 1) {
            constructor_final_cleanup_order = 2;
        }
        ++constructor_leaf_destructions;
    }
};

struct StaticLocalConstructorThrowingChild {
    int value;

    StaticLocalConstructorThrowingChild() : value(191) {
        if (make_static_aggregate_constructor_mode()) throw 167;
    }

    ~StaticLocalConstructorThrowingChild() {
        if (constructor_mode_calls == 2 &&
            constructor_leaf_destructions == 1 &&
            constructor_child_destructions == 0) {
            constructor_final_cleanup_order = 1;
        }
        ++constructor_child_destructions;
    }
};

struct StaticLocalConstructorBuiltMember {
    StaticLocalConstructorBuiltLeaf leaf;
    StaticLocalConstructorThrowingChild child;

    StaticLocalConstructorBuiltMember() : leaf(181), child() {
        ++constructor_parent_body_calls;
    }
};

#endif

static const StaticLocalNontrivialClassReferenceAggregate&
static_local_nontrivial_class_reference_aggregate() {
    static StaticLocalNontrivialClassReferenceAggregate value{
        make_static_aggregate_nontrivial_reference_value(),
        make_static_aggregate_nontrivial_reference_second_value(),
        make_static_aggregate_nontrivial_reference_later()};
    return value;
}

#ifdef RCC_TEST_STATIC_LOCAL_PARTIAL_SUBOBJECT
static const StaticLocalPartialAggregate&
static_local_partial_aggregate() {
    ++partial_member_initializer_calls;
    static StaticLocalPartialAggregate value{
        {StaticLocalPartialMember{107},
         StaticLocalPartialMember{
             make_static_aggregate_partial_member_later()}},
        127};
    return value;
}
#endif

#ifdef RCC_TEST_STATIC_LOCAL_PARTIAL_ARRAY_SUBOBJECT
static const StaticLocalPartialArrayAggregate&
static_local_partial_array_aggregate() {
    ++partial_array_initializer_calls;
    static StaticLocalPartialArrayAggregate value{
        {StaticLocalArrayPartialMember{131},
         StaticLocalArrayPartialMember{137}},
        make_static_aggregate_partial_array_later()};
    return value;
}
#endif

#ifdef RCC_TEST_STATIC_LOCAL_CONSTRUCTOR_SUBOBJECT
static const StaticLocalConstructorBuiltMember&
static_local_constructor_built_member() {
    ++constructor_member_initializer_calls;
    static StaticLocalConstructorBuiltMember value{};
    return value;
}
#endif

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
#ifdef RCC_TEST_STATIC_LOCAL_PARTIAL_SUBOBJECT
    try {
        (void)static_local_partial_aggregate();
        return 18;
    } catch (int error) {
        if (error != 109) return 19;
    }
    if (partial_member_initializer_calls != 1 ||
        partial_member_later_initializer_calls != 1 ||
        partial_member_destructions != 1 || partial_member_unwind_order != 1) {
        return 20;
    }
    const StaticLocalPartialAggregate& first_partial =
        static_local_partial_aggregate();
    const StaticLocalPartialAggregate& second_partial =
        static_local_partial_aggregate();
    if (&first_partial != &second_partial ||
        first_partial.member.first.value != 107 ||
        first_partial.member.second.value != 113 ||
        first_partial.later != 127 ||
        partial_member_initializer_calls != 3 ||
        partial_member_later_initializer_calls != 2 ||
        partial_member_destructions != 1 ||
        partial_member_final_cleanup_order != 0) {
        return 21;
    }
#endif

#ifdef RCC_TEST_STATIC_LOCAL_PARTIAL_ARRAY_SUBOBJECT
    try {
        (void)static_local_partial_array_aggregate();
        return 22;
    } catch (int error) {
        if (error != 139) return 23;
    }
    if (partial_array_initializer_calls != 1 ||
        partial_array_later_initializer_calls != 1 ||
        partial_array_member_destructions != 2 ||
        partial_array_unwind_order != 2) {
        return 24;
    }
    const StaticLocalPartialArrayAggregate& first_partial_array =
        static_local_partial_array_aggregate();
    const StaticLocalPartialArrayAggregate& second_partial_array =
        static_local_partial_array_aggregate();
    if (&first_partial_array != &second_partial_array ||
        first_partial_array.members[0].value != 131 ||
        first_partial_array.members[1].value != 137 ||
        first_partial_array.later != 151 ||
        partial_array_initializer_calls != 3 ||
        partial_array_later_initializer_calls != 2 ||
        partial_array_member_destructions != 2 ||
        partial_array_final_cleanup_order != 0) {
        return 25;
    }
#endif

#ifdef RCC_TEST_STATIC_LOCAL_CONSTRUCTOR_SUBOBJECT
    try {
        (void)static_local_constructor_built_member();
        return 26;
    } catch (int error) {
        if (error != 167) return 27;
    }
    if (constructor_member_initializer_calls != 1 ||
        constructor_parent_body_calls != 0 || constructor_mode_calls != 1 ||
        constructor_leaf_destructions != 1 ||
        constructor_child_destructions != 0 || constructor_unwind_order != 1) {
        return 28;
    }
    const StaticLocalConstructorBuiltMember& first_constructor_member =
        static_local_constructor_built_member();
    const StaticLocalConstructorBuiltMember& second_constructor_member =
        static_local_constructor_built_member();
    if (&first_constructor_member != &second_constructor_member ||
        first_constructor_member.leaf.value != 181 ||
        first_constructor_member.child.value != 191 ||
        constructor_member_initializer_calls != 3 ||
        constructor_parent_body_calls != 1 || constructor_mode_calls != 2 ||
        constructor_leaf_destructions != 1 ||
        constructor_child_destructions != 0 ||
        constructor_final_cleanup_order != 0) {
        return 29;
    }
#endif
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

#ifdef RCC_TEST_STATIC_LOCAL_PARTIAL_SUBOBJECT
extern "C" int
verified_static_local_aggregate_partial_member_destructor_count() {
    return partial_member_destructions;
}

extern "C" int
verified_static_local_aggregate_partial_member_unwind_order() {
    return partial_member_unwind_order;
}

extern "C" int
verified_static_local_aggregate_partial_member_final_cleanup_order() {
    return partial_member_final_cleanup_order;
}
#endif

#ifdef RCC_TEST_STATIC_LOCAL_PARTIAL_ARRAY_SUBOBJECT
extern "C" int
verified_static_local_aggregate_partial_array_member_destructor_count() {
    return partial_array_member_destructions;
}

extern "C" int
verified_static_local_aggregate_partial_array_unwind_order() {
    return partial_array_unwind_order;
}

extern "C" int
verified_static_local_aggregate_partial_array_final_cleanup_order() {
    return partial_array_final_cleanup_order;
}
#endif

#ifdef RCC_TEST_STATIC_LOCAL_CONSTRUCTOR_SUBOBJECT
extern "C" int
verified_static_local_aggregate_constructor_leaf_destructor_count() {
    return constructor_leaf_destructions;
}

extern "C" int
verified_static_local_aggregate_constructor_child_destructor_count() {
    return constructor_child_destructions;
}

extern "C" int
verified_static_local_aggregate_constructor_unwind_order() {
    return constructor_unwind_order;
}

extern "C" int
verified_static_local_aggregate_constructor_final_cleanup_order() {
    return constructor_final_cleanup_order;
}
#endif
