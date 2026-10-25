int update_lvalue_reference_local(int& value) {
    int& local = value;
    local += 1;
    return value;
}

int update_rvalue_reference_local(int&& value) {
    int&& local = static_cast<int&&>(value);
    local += 2;
    return value;
}

int assign_lvalue_reference(int& value, int replacement) {
    value = replacement;
    return value;
}

struct VerifiedBase {
    int* cleanup_order;
    int value;

    VerifiedBase(int* order, int initial) noexcept
        : cleanup_order(order), value(initial) {}

    ~VerifiedBase() noexcept {
        *cleanup_order = *cleanup_order * 10 + 1;
    }
};

struct VerifiedDerived : VerifiedBase {
    VerifiedDerived(int* order, int initial)
        : VerifiedBase(order, initial) {}

    ~VerifiedDerived() noexcept {
        *cleanup_order = *cleanup_order * 10 + 2;
    }
};

struct VerifiedConversionSource {
    int* cleanup_order;
    int value;

    operator VerifiedDerived() const noexcept {
        return {cleanup_order, value};
    }
};

struct VerifiedBodyConstructor {
    int value;

    explicit VerifiedBodyConstructor(int initial) noexcept
        : value(initial) {
        value = value * 3 + 2;
    }
};

int verified_body_constructor_result() {
    VerifiedBodyConstructor object(13);
    return object.value;
}

struct VerifiedBranchConstructor {
    int value;

    explicit VerifiedBranchConstructor(int initial) noexcept
        : value(initial) {
        int adjusted = initial + 1;
        if (adjusted > 10) {
            value = adjusted * 2;
        } else {
            value = adjusted + 3;
        }
    }
};

int verified_branch_constructor_result() {
    VerifiedBranchConstructor high(10);
    VerifiedBranchConstructor low(4);
    return high.value * 100 + low.value;
}

struct VerifiedLoopConstructor {
    int value;

    explicit VerifiedLoopConstructor(int count) noexcept : value(0) {
        for (int index = 0; index < count; ++index) {
            value += index + 1;
        }
    }
};

int verified_loop_constructor_result() {
    VerifiedLoopConstructor four(4);
    VerifiedLoopConstructor zero(0);
    return four.value * 100 + zero.value;
}

struct VerifiedSwitchConstructor {
    int value;

    explicit VerifiedSwitchConstructor(int selector) noexcept : value(0) {
        switch (selector) {
            case 1:
                value = 17;
                break;
            case 2:
                value = 29;
                break;
            default:
                value = 5;
                break;
        }
    }
};

int verified_switch_constructor_result() {
    VerifiedSwitchConstructor one(1);
    VerifiedSwitchConstructor two(2);
    VerifiedSwitchConstructor other(9);
    return one.value * 100 + two.value * 10 + other.value;
}

struct VerifiedBaseConstructor {
    int base_value;

    VerifiedBaseConstructor(int* order, int initial) noexcept
        : base_value(initial) {
        *order = *order * 10 + 2;
        base_value = base_value * 2 + 1;
    }
};

struct VerifiedPrefixConstructor {
    int prefix_value;

    VerifiedPrefixConstructor(int* order, int initial) noexcept
        : prefix_value(initial) {
        *order = *order * 10 + 1;
        prefix_value += 3;
    }
};

struct VerifiedDerivedConstructor : VerifiedPrefixConstructor,
                                    VerifiedBaseConstructor {
    int tail;

    VerifiedDerivedConstructor(int* order, int initial) noexcept
        : VerifiedPrefixConstructor(order, initial),
          VerifiedBaseConstructor(order, initial), tail(initial) {
        *order = *order * 10 + 3;
        tail = tail * 3 + 2;
    }
};

struct VerifiedTrivialCopyMember {
    int nested_value;
};

struct VerifiedTrivialCopyPrefix {
    int prefix_value;
};

struct VerifiedTrivialCopyBase {
    int base_value;
};

struct VerifiedTrivialCopy : VerifiedTrivialCopyPrefix,
                             VerifiedTrivialCopyBase {
    int value;
    int values[2];
    VerifiedTrivialCopyMember nested;
};

int verified_nontrivial_base_constructor_result() {
    int order = 0;
    VerifiedDerivedConstructor object(&order, 10);
    if (order != 123) return -1;
    return object.prefix_value * 10000 +
        object.base_value * 100 + object.tail;
}

int verified_trivial_copy_result() {
    VerifiedTrivialCopy source{{5}, {7}, 17, {21, 25}, {31}};
    VerifiedTrivialCopy copy(source);
    source.prefix_value = 37;
    source.base_value = 43;
    source.value = 29;
    source.values[1] = 35;
    source.nested.nested_value = 41;
    if (source.prefix_value != 37 || source.base_value != 43 ||
        source.value != 29 || source.values[1] != 35 ||
        source.nested.nested_value != 41) {
        return -1;
    }
    return copy.prefix_value * 100000000 + copy.base_value * 10000000 +
        copy.value * 100000 + copy.values[0] * 1000 +
        copy.values[1] * 10 + copy.nested.nested_value;
}

struct VerifiedExpressionConstructor {
    int sum;
    int weighted_difference;

    VerifiedExpressionConstructor(int first, int second) noexcept
        : sum(first + second),
          weighted_difference(first * 2 - second) {
        sum = sum + 0;
    }
};

int verified_initializer_effect(int* order, int value) noexcept {
    *order = *order * 10 + value;
    return value * 10;
}

struct VerifiedCallInitializer {
    int first;
    int second;

    VerifiedCallInitializer(int* order, int initial) noexcept
        : second(verified_initializer_effect(order, initial + 1)),
          first((verified_initializer_effect(order, initial), initial * 10)) {
        first = first + 0;
    }
};

int verified_expression_constructor_result() {
    VerifiedExpressionConstructor object(10, 3);
    int order = 0;
    VerifiedCallInitializer called(&order, 3);
    if (order != 34 || called.first != 30 || called.second != 40) {
        return -1;
    }
    return object.sum * 100 + object.weighted_difference;
}

int main() {
    int value = 40;
    if (update_lvalue_reference_local(value) != 41) {
        return 1;
    }
    if (update_rvalue_reference_local(static_cast<int&&>(value)) != 43) {
        return 2;
    }
    int assigned = 45;
    if (assign_lvalue_reference(assigned, 47) != 47 || assigned != 47) {
        return 10;
    }
    if (verified_body_constructor_result() != 41) {
        return 3;
    }
    if (verified_branch_constructor_result() != 2208) {
        return 4;
    }
    if (verified_loop_constructor_result() != 1000) {
        return 5;
    }
    if (verified_switch_constructor_result() != 1995) {
        return 6;
    }
    if (verified_nontrivial_base_constructor_result() != 132132) {
        return 7;
    }
    if (verified_trivial_copy_result() != 571721281) {
        return 8;
    }
    if (verified_expression_constructor_result() != 1317) {
        return 9;
    }
    return value != 43;
}
