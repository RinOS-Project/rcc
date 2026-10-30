struct VerifiedVirtualValueRoot {
    int value;
};

struct VerifiedVirtualValuePrefix {
    int prefix_value;
};

struct VerifiedVirtualValuePayload : VerifiedVirtualValuePrefix,
                                    VerifiedVirtualValueRoot {
    int payload_value;
};

struct VerifiedVirtualValueLeft : virtual VerifiedVirtualValuePayload {
    int left_value;
};

struct VerifiedVirtualValueRight : virtual VerifiedVirtualValuePayload {
    int right_value;
};

struct VerifiedVirtualValueLeaf : VerifiedVirtualValueLeft,
                                  VerifiedVirtualValueRight {
    int leaf_value;
    virtual int read_leaf() const noexcept { return leaf_value; }
};

struct VerifiedVirtualCleanupRoot {};

struct VerifiedFloatingValueInitialization {
    float single;
    double wide;
};

struct VerifiedConstructedVirtualBase {
    int value;
    int extra;

    VerifiedConstructedVirtualBase(
            int initial, int initial_extra) noexcept
        : value(initial), extra(initial_extra) {}
};

struct VerifiedConstructedVirtualDerived :
    virtual VerifiedConstructedVirtualBase {
    int derived_value;

    VerifiedConstructedVirtualDerived(
            int initial, int initial_extra, int derived_initial) noexcept
        : VerifiedConstructedVirtualBase(initial, initial_extra),
          derived_value(derived_initial) {}
};

struct VerifiedVirtualCleanupLeaf : virtual VerifiedVirtualCleanupRoot {
    int* cleanup_count;
    int cleanup_increment;
    int cleanup_increment_extra_a;
    double cleanup_increment_extra_b;

    explicit VerifiedVirtualCleanupLeaf(
            int* count, int ignored, int extra_a, int increment,
            int extra_b) noexcept
        : cleanup_count(count), cleanup_increment(increment),
          cleanup_increment_extra_a(extra_a),
          cleanup_increment_extra_b(extra_b) {}

    ~VerifiedVirtualCleanupLeaf() noexcept {
        *cleanup_count += cleanup_increment +
                          cleanup_increment_extra_a * 10 +
                          static_cast<int>(cleanup_increment_extra_b) * 100;
    }
};

static int* record_cleanup_pointer_argument(
        int* evaluations, int* value) noexcept {
    ++*evaluations;
    return value;
}

static int record_cleanup_scalar_argument(
        int* evaluations, int value) noexcept {
    ++*evaluations;
    return value;
}

int consume_verified_virtual_value_root(
        VerifiedVirtualValueRoot value) noexcept {
    return value.value;
}

int consume_verified_virtual_cleanup_root(
        VerifiedVirtualCleanupRoot value) noexcept {
    (void)value;
    return 101;
}

int main() {
    int cleanup_count = 0;
    int argument_evaluations = 0;
    VerifiedFloatingValueInitialization zero_floating{};
    VerifiedVirtualValueLeaf value;
    VerifiedConstructedVirtualDerived constructed_value(23, 19, 7);
    value.value = 101;
    value.leaf_value = 53;
    if (zero_floating.single != 0.0f || zero_floating.wide != 0.0) {
        return 5;
    }
    if (constructed_value.value != 23 || constructed_value.extra != 19 ||
        constructed_value.derived_value != 7) {
        return 6;
    }
    if (consume_verified_virtual_value_root(value) != 101 ||
        value.read_leaf() != 53) {
        return 1;
    }
    if (consume_verified_virtual_cleanup_root(
            VerifiedVirtualCleanupLeaf(
                record_cleanup_pointer_argument(
                    &argument_evaluations, &cleanup_count),
                record_cleanup_scalar_argument(&argument_evaluations, 97),
                record_cleanup_scalar_argument(&argument_evaluations, 3),
                record_cleanup_scalar_argument(&argument_evaluations, 2),
                record_cleanup_scalar_argument(&argument_evaluations, 5))) !=
            101) {
        return 2;
    }
    if (argument_evaluations != 5) return 3;
    return cleanup_count == 532 ? 0 : 4;
}
