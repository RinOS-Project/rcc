struct VerifiedThrowingTemporary {
    int* cleanup_order;
    int cleanup_id;

    ~VerifiedThrowingTemporary() noexcept {
        *cleanup_order = *cleanup_order * 10 + cleanup_id;
    }
};

struct VerifiedThrowingConstructorTemporary {
    int* cleanup_order;
    int marker;

    VerifiedThrowingConstructorTemporary(
        int* cleanup_order, int should_throw)
        : cleanup_order(cleanup_order), marker(7) {
        if (should_throw) throw 58;
    }

    ~VerifiedThrowingConstructorTemporary() noexcept {
        *cleanup_order = *cleanup_order * 10 + marker;
    }
};

VerifiedThrowingTemporary make_verified_throwing_temporary(
    int* cleanup_order, int cleanup_id) noexcept {
    return {cleanup_order, cleanup_id};
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
    /* Keep the observed destructor state outside the setjmp/longjmp frame. */
    *cleanup_order = 0;
    try {
        const VerifiedThrowingTemporary& value =
            make_verified_throwing_temporary(cleanup_order, 3);
        result = consume_verified_throwing_temporary(
            value, throw_verified_temporary_argument());
    } catch (int value) {
        result = value;
    }
    return result * 10 + *cleanup_order;
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
    {
        const VerifiedThrowingTemporary& value =
            make_verified_throwing_temporary(&cleanup_order, 5);
        result = consume_verified_throwing_temporary(value, 0);
    }
    return result * 10 + cleanup_order;
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
    int should_throw) {
    int cleanup_order = 0;
    int result = 0;
    try {
        const VerifiedThrowingConstructorTemporary& value =
            VerifiedThrowingConstructorTemporary(
                &cleanup_order, should_throw);
        result = value.marker;
    } catch (int value) {
        result = value;
    }
    return result * 100 + cleanup_order;
}

extern "C" int main() {
    return 0;
}
