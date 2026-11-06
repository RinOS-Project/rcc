static int cleanup_order;
static int copy_count;
static int move_count;

struct ConditionalThrowingMember {
    int* order;

    explicit ConditionalThrowingMember(int* cleanup_order) noexcept
        : order(cleanup_order) {}

    ~ConditionalThrowingMember() noexcept {
        *order = *order * 10 + 1;
    }
};

struct ConditionalThrowingValue {
    int* cleanup;
    ConditionalThrowingMember member;
    int value;

    explicit ConditionalThrowingValue(int initial, int* order) noexcept
        : cleanup(order), member(order), value(initial) {}

    ConditionalThrowingValue(const ConditionalThrowingValue& source)
        : cleanup(source.cleanup), member(source.cleanup), value(0) {
        ++copy_count;
        value = source.value + 100;
        if (source.value == 7) throw 71;
    }

    ConditionalThrowingValue(ConditionalThrowingValue&& source)
        : cleanup(source.cleanup), member(source.cleanup), value(0) {
        ++move_count;
        value = source.value + 200;
        if (source.value == 9) throw 72;
    }

    ~ConditionalThrowingValue() noexcept {
        cleanup_order = cleanup_order * 10 + 2;
    }
};

static int consume_conditional_throwing_value(
    const ConditionalThrowingValue& value) noexcept {
    return value.value;
}

static int verify_copy_constructor_throw() {
    cleanup_order = 0;
    copy_count = 0;
    int caught = 0;
    try {
        ConditionalThrowingValue source(7, &cleanup_order);
        (void)consume_conditional_throwing_value(
            true ? source : ConditionalThrowingValue(80, &cleanup_order));
    } catch (int exception) {
        caught = exception;
    }
    return caught == 71 && copy_count == 1 && cleanup_order == 121
        ? 0 : 11;
}

static int verify_move_constructor_throw() {
    cleanup_order = 0;
    move_count = 0;
    int caught = 0;
    try {
        ConditionalThrowingValue source(9, &cleanup_order);
        (void)consume_conditional_throwing_value(
            true ? static_cast<ConditionalThrowingValue&&>(source)
                 : ConditionalThrowingValue(80, &cleanup_order));
    } catch (int exception) {
        caught = exception;
    }
    return caught == 72 && move_count == 1 && cleanup_order == 121
        ? 0 : 21;
}

static int verify_copy_constructor_success() {
    cleanup_order = 0;
    copy_count = 0;
    {
        ConditionalThrowingValue source(5, &cleanup_order);
        const int result = consume_conditional_throwing_value(
            true ? source : ConditionalThrowingValue(80, &cleanup_order));
        if (result != 105) return 31;
        if (copy_count != 1) return 32;
        if (cleanup_order != 21) return 33;
    }
    return cleanup_order == 2121 ? 0 : 34;
}

static int verify_move_constructor_success() {
    cleanup_order = 0;
    move_count = 0;
    {
        ConditionalThrowingValue source(4, &cleanup_order);
        const int result = consume_conditional_throwing_value(
            true ? static_cast<ConditionalThrowingValue&&>(source)
                 : ConditionalThrowingValue(80, &cleanup_order));
        if (result != 204) return 41;
        if (move_count != 1) return 42;
        if (cleanup_order != 21) return 43;
    }
    return cleanup_order == 2121 ? 0 : 44;
}

static int verify_unselected_copy_arm() {
    cleanup_order = 0;
    copy_count = 0;
    {
        ConditionalThrowingValue source(7, &cleanup_order);
        const int result = consume_conditional_throwing_value(
            false ? source : ConditionalThrowingValue(80, &cleanup_order));
        if (result != 80) return 51;
        if (copy_count != 0) return 53;
        if (cleanup_order != 21) return 54;
    }
    return cleanup_order == 2121 ? 0 : 55;
}

static int verify_unselected_move_arm() {
    cleanup_order = 0;
    move_count = 0;
    {
        ConditionalThrowingValue source(9, &cleanup_order);
        const int result = consume_conditional_throwing_value(
            false
                ? static_cast<ConditionalThrowingValue&&>(source)
                : ConditionalThrowingValue(80, &cleanup_order));
        if (result != 80) return 61;
        if (move_count != 0) return 63;
        if (cleanup_order != 21) return 64;
    }
    return cleanup_order == 2121 ? 0 : 65;
}

int main() {
    int result = verify_copy_constructor_throw();
    if (result) return result;
    result = verify_move_constructor_throw();
    if (result) return result;
    result = verify_copy_constructor_success();
    if (result) return result;
    result = verify_move_constructor_success();
    if (result) return result;
    result = verify_unselected_copy_arm();
    if (result) return result;
    return verify_unselected_move_arm();
}
