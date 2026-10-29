struct VerifiedThrowingTemporaryNested {
    int value;
};

struct VerifiedThrowingTemporary {
    int* cleanup_order;
    int cleanup_id;
    VerifiedThrowingTemporaryNested nested;

    ~VerifiedThrowingTemporary() noexcept {
        *cleanup_order = *cleanup_order * 10 + cleanup_id;
    }
};

struct VerifiedThrowingTemporaryConversionSource {
    int* cleanup_order;
    int cleanup_id;
    int* receiver_cleanup_order;

    operator VerifiedThrowingTemporary() const {
        if (cleanup_id == 11) throw 73;
        return {cleanup_order, cleanup_id, {cleanup_id + 1}};
    }

    ~VerifiedThrowingTemporaryConversionSource() noexcept {
        if (receiver_cleanup_order) {
            *receiver_cleanup_order =
                *receiver_cleanup_order * 10 + cleanup_id;
        }
    }
};

struct VerifiedThrowingReferenceConversionSource {
    VerifiedThrowingTemporary* value;
    int cleanup_id;
    int* receiver_cleanup_order;

    operator VerifiedThrowingTemporary&() const {
        if (cleanup_id == 21) throw 74;
        return *value;
    }

    ~VerifiedThrowingReferenceConversionSource() noexcept {
        if (receiver_cleanup_order) {
            *receiver_cleanup_order =
                *receiver_cleanup_order * 10 + cleanup_id;
        }
    }
};

struct VerifiedThrowingTemporaryConversionReceiverInner {
    VerifiedThrowingTemporaryConversionSource source;
};

struct VerifiedThrowingTemporaryConversionReceiverEnvelope {
    VerifiedThrowingTemporaryConversionReceiverInner inner;
};

static VerifiedThrowingTemporaryConversionReceiverEnvelope
make_verified_temporary_conversion_receiver_envelope(
    int* cleanup_order, int cleanup_id,
    int* receiver_cleanup_order) noexcept {
    return VerifiedThrowingTemporaryConversionReceiverEnvelope{
        VerifiedThrowingTemporaryConversionReceiverInner{
            VerifiedThrowingTemporaryConversionSource{
                cleanup_order, cleanup_id, receiver_cleanup_order}}};
}

struct VerifiedThrowingTemporaryConversionReceiverDerived :
    VerifiedThrowingTemporaryConversionSource {
    int* complete_cleanup_order;

    ~VerifiedThrowingTemporaryConversionReceiverDerived() noexcept {
        *complete_cleanup_order = *complete_cleanup_order * 10 + cleanup_id;
    }
};

static VerifiedThrowingTemporaryConversionReceiverDerived
make_verified_temporary_conversion_receiver_derived(
    int* cleanup_order, int cleanup_id,
    int* receiver_cleanup_order, int* complete_cleanup_order) noexcept {
    return VerifiedThrowingTemporaryConversionReceiverDerived{
        VerifiedThrowingTemporaryConversionSource{
            cleanup_order, cleanup_id, receiver_cleanup_order},
        complete_cleanup_order};
}

struct VerifiedVirtualBaseConversionSource {
    int* cleanup_order;
    int cleanup_id;

    operator VerifiedThrowingTemporary() const noexcept {
        return {cleanup_order, cleanup_id, {cleanup_id + 1}};
    }
};

struct VerifiedVirtualBaseConversionDerived :
    virtual VerifiedVirtualBaseConversionSource {};

struct VerifiedConvertedBase {
    int* cleanup_order;
    int value;

    ~VerifiedConvertedBase() noexcept {
        *cleanup_order = *cleanup_order * 10 + 1;
    }
};

struct VerifiedConvertedPrefix {
    int value;
};

struct VerifiedConvertedDerived :
    VerifiedConvertedPrefix, VerifiedConvertedBase {
    int* cleanup_order;
    int value;

    ~VerifiedConvertedDerived() noexcept {
        *cleanup_order = *cleanup_order * 10 + 2;
    }
};

struct VerifiedBaseConversionResultSource {
    int* cleanup_order;
    int value;

    operator VerifiedConvertedDerived() const noexcept {
        return VerifiedConvertedDerived{
            {value - 1}, {cleanup_order, value + 1}, cleanup_order, value};
    }
};

extern "C" {
int verified_converted_virtual_cleanup_order;
}

struct VerifiedConvertedVirtualBase {
    int value = 35;

    ~VerifiedConvertedVirtualBase() noexcept {
        verified_converted_virtual_cleanup_order =
            verified_converted_virtual_cleanup_order * 10 + 1;
    }
};

struct VerifiedConvertedVirtualPrefix {
    int value;
};

struct VerifiedConvertedVirtualDerived :
    VerifiedConvertedVirtualPrefix, virtual VerifiedConvertedVirtualBase {
    int value = 33;

    ~VerifiedConvertedVirtualDerived() noexcept {
        verified_converted_virtual_cleanup_order =
            verified_converted_virtual_cleanup_order * 10 + 2;
    }
};

struct VerifiedVirtualBaseResultSource {
    operator VerifiedConvertedVirtualDerived() const noexcept {
        return {};
    }
};

struct VerifiedFirstConstructorMember {
    int* cleanup_order;

    explicit VerifiedFirstConstructorMember(int* cleanup_order) noexcept
        : cleanup_order(cleanup_order) {}

    ~VerifiedFirstConstructorMember() noexcept {
        *cleanup_order = *cleanup_order * 10 + 1;
    }
};

struct VerifiedInnerConstructorMember {
    int* cleanup_order;

    explicit VerifiedInnerConstructorMember(int* cleanup_order) noexcept
        : cleanup_order(cleanup_order) {}

    ~VerifiedInnerConstructorMember() noexcept {
        *cleanup_order = *cleanup_order * 10 + 3;
    }
};

struct VerifiedSecondConstructorMember {
    int* cleanup_order;
    VerifiedInnerConstructorMember inner;
    int marker;

    VerifiedSecondConstructorMember(int* cleanup_order, int should_throw)
        : cleanup_order(cleanup_order), inner(cleanup_order), marker(2) {
        if (should_throw) throw 59;
    }

    ~VerifiedSecondConstructorMember() noexcept {
        *cleanup_order = *cleanup_order * 10 + 2;
    }
};

struct VerifiedThrowingConstructorTemporary {
    VerifiedFirstConstructorMember first;
    VerifiedSecondConstructorMember second;

    VerifiedThrowingConstructorTemporary(int* cleanup_order, int stage)
        : first(cleanup_order), second(cleanup_order, stage == 1) {
        if (stage == 0) return;
        if (stage == 2) throw 58;
    }

    ~VerifiedThrowingConstructorTemporary() noexcept {}
};

struct VerifiedPotentiallyThrowingBase {
    int* cleanup_order;
    int first_value;

    explicit VerifiedPotentiallyThrowingBase(
        int* cleanup_order, int initial)
        : cleanup_order(cleanup_order), first_value(0) {
        if (initial < 0) throw 61;
        first_value = initial;
    }

    ~VerifiedPotentiallyThrowingBase() noexcept {
        *cleanup_order = *cleanup_order * 10 + 1;
    }
};

struct VerifiedReferenceBoundPartialPrefix {
    int prefix_value;
};

struct VerifiedReferenceBoundPartialAggregate :
        VerifiedReferenceBoundPartialPrefix, VerifiedThrowingTemporary {
    VerifiedPotentiallyThrowingBase pending;
};

struct VerifiedPotentiallyThrowingCleanupCallback {
    int* cleanup_order;

    ~VerifiedPotentiallyThrowingCleanupCallback() noexcept(false) {
        *cleanup_order = *cleanup_order * 10 + 9;
        throw 88;
    }
};

struct VerifiedSecondPotentiallyThrowingBase {
    int* cleanup_order;
    int second_value;

    explicit VerifiedSecondPotentiallyThrowingBase(
        int* cleanup_order, int initial)
        : cleanup_order(cleanup_order), second_value(0) {
        if (initial < 0) throw 62;
        second_value = initial;
    }

    ~VerifiedSecondPotentiallyThrowingBase() noexcept {
        *cleanup_order = *cleanup_order * 10 + 2;
    }
};

static int verified_multiple_base_member_initializer_mode;

static int verified_multiple_base_member_initializer_value() {
    if (verified_multiple_base_member_initializer_mode == 7) throw 67;
    return 13;
}

struct VerifiedPotentiallyThrowingMember {
    int* cleanup_order;
    int member_value;

    explicit VerifiedPotentiallyThrowingMember(
        int* cleanup_order, int initial)
        : cleanup_order(cleanup_order), member_value(0) {
        if (initial < 0) throw 71;
        member_value = initial;
    }

    ~VerifiedPotentiallyThrowingMember() noexcept {
        *cleanup_order = *cleanup_order * 10 + 3;
    }
};

struct VerifiedMultipleThrowingBaseDerived :
        VerifiedPotentiallyThrowingBase,
        VerifiedSecondPotentiallyThrowingBase {
    int tail;
    VerifiedPotentiallyThrowingMember member;
    int default_member_value =
        verified_multiple_base_member_initializer_value();

    explicit VerifiedMultipleThrowingBaseDerived(
        int* cleanup_order, int first, int second, int member_initial,
        int body_mode)
        : VerifiedPotentiallyThrowingBase(cleanup_order, first),
          VerifiedSecondPotentiallyThrowingBase(cleanup_order, second),
          member(cleanup_order, member_initial),
          tail(7) {
        switch (body_mode) {
        case 0:
            switch (first_value) {
            case 8:
                tail += 1;
            case 9:
                tail += 2;
                break;
            default:
                break;
            }
            break;
        case 2:
            tail += 20;
            break;
        case 5:
            tail += 5;
            break;
        case 6:
            for (int iteration = 0; iteration < 2; ++iteration) {
                if (iteration == 1) throw 66;
                tail += 1;
            }
            break;
        default:
            break;
        }
        if (body_mode == 2) throw 65;
    }
};

VerifiedThrowingTemporary make_verified_throwing_temporary(
    int* cleanup_order, int cleanup_id) noexcept {
    return {cleanup_order, cleanup_id, {cleanup_id + 1}};
}

VerifiedThrowingTemporary make_verified_throwing_temporary_that_may_throw(
    int* cleanup_order, int cleanup_id, int should_throw) {
    if (should_throw) throw 56;
    return {cleanup_order, cleanup_id};
}

int consume_verified_throwing_temporary(
    const VerifiedThrowingTemporary& value, int should_throw) {
    if (should_throw) throw 47;
    return value.cleanup_id;
}

int consume_verified_throwing_temporary_noexcept(
    const VerifiedThrowingTemporary& value, int result) noexcept {
    return value.cleanup_id + result;
}

int consume_two_verified_throwing_temporaries_noexcept(
    const VerifiedThrowingTemporary& first,
    const VerifiedThrowingTemporary& second, int result) noexcept {
    return first.cleanup_id + second.cleanup_id + result;
}

int throw_verified_temporary_argument() {
    throw 53;
}

extern "C" int verified_cxx_throwing_temporary_cleanup(int should_throw) {
    int cleanup_order = 0;
    int result = 0;
    try {
        result = consume_verified_throwing_temporary(
            make_verified_throwing_temporary(&cleanup_order, 2),
            should_throw);
    } catch (int value) {
        result = value;
    }
    return result * 10 + cleanup_order;
}

extern "C" int verified_cxx_throwing_argument_cleanup(int* cleanup_order) {
    int result = 0;
    int cleanup_before_throw = 0;
    /* Keep the observed destructor state outside the setjmp/longjmp frame. */
    *cleanup_order = 0;
    try {
        const int& value =
            make_verified_throwing_temporary(cleanup_order, 3).cleanup_id;
        cleanup_before_throw = *cleanup_order;
        result = value + throw_verified_temporary_argument();
    } catch (int value) {
        result = value;
    }
    return result * 100 + cleanup_before_throw * 10 + *cleanup_order;
}

extern "C" int verified_cxx_throwing_sibling_argument_cleanup(
    int* cleanup_order) {
    int result = 0;
    *cleanup_order = 0;
    try {
        result = consume_verified_throwing_temporary_noexcept(
            make_verified_throwing_temporary(cleanup_order, 6),
            throw_verified_temporary_argument());
    } catch (int value) {
        result = value;
    }
    return result * 10 + *cleanup_order;
}

extern "C" int verified_cxx_throwing_multiple_sibling_arguments_cleanup(
    int* cleanup_order) {
    int result = 0;
    *cleanup_order = 0;
    try {
        result = consume_two_verified_throwing_temporaries_noexcept(
            make_verified_throwing_temporary(cleanup_order, 6),
            make_verified_throwing_temporary(cleanup_order, 7),
            throw_verified_temporary_argument());
    } catch (int value) {
        result = value;
    }
    return result * 100 + *cleanup_order;
}

extern "C" int verified_cxx_throwing_conditional_cleanup(
    int select_outer, int select_inner, int* cleanup_order) {
    int result = 0;
    *cleanup_order = 0;
    try {
        const VerifiedThrowingTemporary& value = select_outer
            ? (select_inner
                   ? make_verified_throwing_temporary(cleanup_order, 4)
                   : make_verified_throwing_temporary(cleanup_order, 5))
            : make_verified_throwing_temporary(cleanup_order, 3);
        result = consume_verified_throwing_temporary(
            value, throw_verified_temporary_argument());
    } catch (int value) {
        result = value;
    }
    return result * 10 + *cleanup_order;
}

extern "C" int verified_cxx_local_reference_normal_cleanup() {
    int cleanup_order = 0;
    int result = 0;
    int nested_result;
    int conversion_observation;
    int converted_cleanup_order;
    int receiver_cleanup_order = 0;
    int receiver_observation;
    int converted_rvalue_receiver_cleanup_order = 0;
    int converted_rvalue_observation;
    int converted_const_rvalue_receiver_cleanup_order = 0;
    int converted_const_rvalue_observation;
    int throwing_conversion_receiver_cleanup_order = 0;
    int throwing_conversion_observation = 0;
    int conditional_receiver_cleanup_order = 0;
    int conditional_receiver_observation;
    int comma_receiver_cleanup_order = 0;
    int comma_receiver_evaluations = 0;
    int comma_receiver_observation;
    {
        const int& value =
            make_verified_throwing_temporary(
                &cleanup_order, 5).nested.value;
        result = value + cleanup_order * 10;
    }
    nested_result = result * 10 + cleanup_order;
    cleanup_order = 0;
    {
        VerifiedThrowingTemporaryConversionSource source{
            &cleanup_order, 7};
        const VerifiedThrowingTemporary& value = source;
        conversion_observation = value.cleanup_id * 10 + cleanup_order;
    }
    converted_cleanup_order = cleanup_order;
    cleanup_order = 0;
    receiver_cleanup_order = 0;
    {
        const VerifiedThrowingTemporary& value =
            VerifiedThrowingTemporaryConversionSource{
                &cleanup_order, 8, &receiver_cleanup_order};
        receiver_observation = value.cleanup_id * 100 +
            receiver_cleanup_order * 10 + cleanup_order;
    }
    if (nested_result != 65) return -1;
    if (conversion_observation != 70) return -2;
    if (converted_cleanup_order != 7) return -3;
    if (receiver_observation != 880) return receiver_observation;
    if (receiver_cleanup_order != 8) return -5;
    if (cleanup_order != 8) return -6;
    cleanup_order = 0;
    {
        VerifiedThrowingTemporary&& value =
            VerifiedThrowingTemporaryConversionSource{
                &cleanup_order, 9, &converted_rvalue_receiver_cleanup_order};
        converted_rvalue_observation = value.cleanup_id * 100 +
            converted_rvalue_receiver_cleanup_order * 10 + cleanup_order;
    }
    if (converted_rvalue_observation != 990) {
        return converted_rvalue_observation;
    }
    if (converted_rvalue_receiver_cleanup_order != 9) return -7;
    if (cleanup_order != 9) return -8;
    cleanup_order = 0;
    {
        const VerifiedThrowingTemporary&& value =
            VerifiedThrowingTemporaryConversionSource{
                &cleanup_order, 10,
                &converted_const_rvalue_receiver_cleanup_order};
        converted_const_rvalue_observation = value.cleanup_id * 100 +
            converted_const_rvalue_receiver_cleanup_order * 10 +
            cleanup_order;
    }
    if (converted_const_rvalue_observation != 1100) {
        return converted_const_rvalue_observation;
    }
    if (converted_const_rvalue_receiver_cleanup_order != 10) return -9;
    if (cleanup_order != 10) return -10;
    cleanup_order = 0;
    try {
        const VerifiedThrowingTemporary& value =
            VerifiedThrowingTemporaryConversionSource{
                &cleanup_order, 11,
                &throwing_conversion_receiver_cleanup_order};
        throwing_conversion_observation = value.cleanup_id;
    } catch (int value) {
        throwing_conversion_observation = value * 100 +
            throwing_conversion_receiver_cleanup_order * 10 + cleanup_order;
    }
    if (throwing_conversion_observation != 7410) {
        return throwing_conversion_observation;
    }
    if (throwing_conversion_receiver_cleanup_order != 11) return -11;
    if (cleanup_order != 0) return -12;
    cleanup_order = 0;
    {
        const VerifiedThrowingTemporary& value =
            1 ? VerifiedThrowingTemporaryConversionSource{
                    &cleanup_order, 12, &conditional_receiver_cleanup_order}
              : VerifiedThrowingTemporaryConversionSource{
                    &cleanup_order, 13, &conditional_receiver_cleanup_order};
        conditional_receiver_observation = value.cleanup_id * 100 +
            conditional_receiver_cleanup_order * 10 + cleanup_order;
    }
    if (conditional_receiver_observation != 1320) {
        return conditional_receiver_observation;
    }
    if (conditional_receiver_cleanup_order != 12) return -13;
    if (cleanup_order != 12) return -14;
    cleanup_order = 0;
    conditional_receiver_cleanup_order = 0;
    {
        const VerifiedThrowingTemporary& value =
            0 ? VerifiedThrowingTemporaryConversionSource{
                    &cleanup_order, 14, &conditional_receiver_cleanup_order}
              : VerifiedThrowingTemporaryConversionSource{
                    &cleanup_order, 15, &conditional_receiver_cleanup_order};
        conditional_receiver_observation = value.cleanup_id * 100 +
            conditional_receiver_cleanup_order * 10 + cleanup_order;
    }
    if (conditional_receiver_observation != 1650) {
        return conditional_receiver_observation;
    }
    if (conditional_receiver_cleanup_order != 15) return -15;
    if (cleanup_order != 15) return -16;
    cleanup_order = 0;
    conditional_receiver_cleanup_order = 0;
    {
        const VerifiedThrowingTemporary& value =
            1 ? (0 ? VerifiedThrowingTemporaryConversionSource{
                        &cleanup_order, 17,
                        &conditional_receiver_cleanup_order}
                  : VerifiedThrowingTemporaryConversionSource{
                        &cleanup_order, 18,
                        &conditional_receiver_cleanup_order})
              : VerifiedThrowingTemporaryConversionSource{
                    &cleanup_order, 19,
                    &conditional_receiver_cleanup_order};
        conditional_receiver_observation = value.cleanup_id * 100 +
            conditional_receiver_cleanup_order * 10 + cleanup_order;
    }
    if (conditional_receiver_observation != 1980) {
        return conditional_receiver_observation;
    }
    if (conditional_receiver_cleanup_order != 18) return -20;
    if (cleanup_order != 18) return -21;
    cleanup_order = 0;
    {
        const VerifiedThrowingTemporary& value =
            (++comma_receiver_evaluations,
             VerifiedThrowingTemporaryConversionSource{
                 &cleanup_order, 16, &comma_receiver_cleanup_order});
        comma_receiver_observation = value.cleanup_id * 100 +
            comma_receiver_cleanup_order * 10 + cleanup_order;
    }
    if (comma_receiver_observation != 1760) {
        return comma_receiver_observation;
    }
    if (comma_receiver_evaluations != 1) return -17;
    if (comma_receiver_cleanup_order != 16) return -18;
    if (cleanup_order != 16) return -19;
    int reference_conversion_receiver_cleanup_order = 0;
    int reference_conversion_target_cleanup_order = 0;
    int reference_conversion_observation = 0;
    VerifiedThrowingTemporary reference_conversion_target{
        &reference_conversion_target_cleanup_order, 20, {20}};
    try {
        const VerifiedThrowingTemporary& value =
            VerifiedThrowingReferenceConversionSource{
                &reference_conversion_target, 21,
                &reference_conversion_receiver_cleanup_order};
        reference_conversion_observation = value.cleanup_id;
    } catch (int value) {
        reference_conversion_observation = value * 100 +
            reference_conversion_receiver_cleanup_order * 10 +
            reference_conversion_target.cleanup_id;
    }
    if (reference_conversion_observation != 7630) {
        return reference_conversion_observation;
    }
    if (reference_conversion_receiver_cleanup_order != 21) return -22;
    if (reference_conversion_target_cleanup_order != 0) return -23;
    int subobject_receiver_cleanup_order = 0;
    int subobject_receiver_observation = 0;
    cleanup_order = 0;
    {
        const VerifiedThrowingTemporary& value =
            make_verified_temporary_conversion_receiver_envelope(
                &cleanup_order, 22, &subobject_receiver_cleanup_order)
                .inner.source;
        subobject_receiver_observation = value.cleanup_id * 100 +
            subobject_receiver_cleanup_order * 10 + cleanup_order;
    }
    if (subobject_receiver_observation != 2420) {
        return subobject_receiver_observation;
    }
    if (subobject_receiver_cleanup_order != 22) return -24;
    if (cleanup_order != 22) return -25;
    return 65716;
}

extern "C" int verified_cxx_base_conversion_receiver_cleanup() {
    int cleanup_order = 0;
    int base_receiver_cleanup_order = 0;
    int derived_complete_cleanup_order = 0;
    int base_receiver_observation = 0;
    {
        const VerifiedThrowingTemporary& value =
            static_cast<VerifiedThrowingTemporaryConversionSource&&>(
                make_verified_temporary_conversion_receiver_derived(
                    &cleanup_order, 23, &base_receiver_cleanup_order,
                    &derived_complete_cleanup_order));
        base_receiver_observation = value.cleanup_id * 100 +
            base_receiver_cleanup_order * 10 + cleanup_order;
    }
    if (base_receiver_observation != 2530) {
        return base_receiver_observation;
    }
    if (base_receiver_cleanup_order != 23) return -26;
    if (derived_complete_cleanup_order != 23) return -27;
    if (cleanup_order != 23) return -28;
    return 65717;
}

extern "C" int verified_cxx_virtual_base_conversion_receiver_cleanup() {
    int cleanup_order = 0;
    int conversion_observation = 0;
    {
        VerifiedVirtualBaseConversionDerived receiver;
        receiver.cleanup_order = &cleanup_order;
        receiver.cleanup_id = 24;
        {
            const VerifiedThrowingTemporary& value =
                static_cast<VerifiedVirtualBaseConversionSource&&>(receiver);
            conversion_observation = value.cleanup_id * 100 + cleanup_order;
        }
    }
    if (conversion_observation != 2400) return conversion_observation;
    if (cleanup_order != 24) return -29;
    return 65718;
}

extern "C" int verified_cxx_converted_base_lifetime() {
    int cleanup_order = 0;
    int observed_value = 0;
    {
        const VerifiedConvertedBase& value =
            VerifiedBaseConversionResultSource{&cleanup_order, 34};
        observed_value = value.value;
        if (cleanup_order != 0) return -30;
    }
    if (observed_value != 35) return observed_value;
    if (cleanup_order != 21) return cleanup_order * 100;
    return 65719;
}

extern "C" int verified_cxx_converted_virtual_base_lifetime() {
    verified_converted_virtual_cleanup_order = 0;
    {
        const VerifiedConvertedVirtualBase& value =
            VerifiedVirtualBaseResultSource{};
        VerifiedConvertedVirtualBase& mutable_value =
            const_cast<VerifiedConvertedVirtualBase&>(value);
        mutable_value.value = 35;
        if (value.value != 35) return -32;
        if (verified_converted_virtual_cleanup_order != 0) return -31;
    }
    if (verified_converted_virtual_cleanup_order != 21) {
        return verified_converted_virtual_cleanup_order * 100;
    }
    return 65720;
}

extern "C" int verified_cxx_throwing_initializer_cleanup(int should_throw) {
    int cleanup_order = 0;
    int result = 0;
    try {
        const VerifiedThrowingTemporary& value =
            make_verified_throwing_temporary_that_may_throw(
                &cleanup_order, 6, should_throw);
        result = consume_verified_throwing_temporary(value, 0);
    } catch (int value) {
        result = value;
    }
    return result * 10 + cleanup_order;
}

extern "C" int verified_cxx_reference_bound_partial_aggregate_cleanup(
    int* cleanup_order, int should_throw) {
    int result = 0;
    *cleanup_order = 0;
    try {
        {
            const VerifiedReferenceBoundPartialAggregate& value =
                VerifiedReferenceBoundPartialAggregate{
                    {3},
                    {cleanup_order, 8, {9}},
                    VerifiedPotentiallyThrowingBase(
                        cleanup_order, should_throw ? -1 : 5)};
            result = value.pending.first_value;
        }
    } catch (int exception) {
        return exception * 10 + *cleanup_order;
    }
    return result * 100 + *cleanup_order;
}

extern "C" int verified_cxx_throwing_cleanup_callback(int* cleanup_order) {
    *cleanup_order = 0;
    try {
        {
            VerifiedPotentiallyThrowingCleanupCallback object{cleanup_order};
            (void)object;
        }
    } catch (int exception) {
        return exception * 10 + *cleanup_order;
    }
    return -1;
}

extern "C" int verified_cxx_throwing_constructor_reference_cleanup(
    int stage) {
    int cleanup_order = 0;
    int result = 0;
    try {
        const VerifiedThrowingConstructorTemporary& value =
            VerifiedThrowingConstructorTemporary(
                &cleanup_order, stage);
        result = value.second.marker;
    } catch (int value) {
        result = value;
    }
    return result * 1000 + cleanup_order;
}

extern "C" int verified_cxx_single_throwing_base_constructor(
    int mode) {
    int cleanup_order = 0;
    int result = 0;
    verified_multiple_base_member_initializer_mode = mode;
    try {
        {
            VerifiedMultipleThrowingBaseDerived object(
                &cleanup_order, mode == 3 ? -1 : 8,
                mode == 1 ? -1 : 9, mode == 8 ? -1 : 3,
                mode == 9 ? 2 : mode);
            if (mode == 4) throw 64;
            result = object.first_value * 100 +
                object.second_value * 10 + object.tail;
        }
    } catch (int value) {
        return value * 100 + cleanup_order;
    }
    return result * 100 + cleanup_order;
}

extern "C" int main() {
    return 0;
}
