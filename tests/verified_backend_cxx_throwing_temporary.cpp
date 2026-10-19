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

    operator VerifiedThrowingTemporary() const noexcept {
        return {cleanup_order, cleanup_id, {cleanup_id + 1}};
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
    int receiver_observation;
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
    {
        const VerifiedThrowingTemporary& value =
            VerifiedThrowingTemporaryConversionSource{&cleanup_order, 8};
        receiver_observation = value.cleanup_id * 10 + cleanup_order;
    }
    return nested_result == 65 && conversion_observation == 70 &&
            converted_cleanup_order == 7 && receiver_observation == 80 &&
            cleanup_order == 8
        ? 65708 : -1;
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

extern "C" int main() {
    return 0;
}
