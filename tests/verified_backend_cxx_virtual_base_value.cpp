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

struct VerifiedVirtualCleanupLeaf : virtual VerifiedVirtualCleanupRoot {
    int* cleanup_count;

    explicit VerifiedVirtualCleanupLeaf(int* count) noexcept
        : cleanup_count(count) {}

    ~VerifiedVirtualCleanupLeaf() noexcept {
        ++*cleanup_count;
    }
};

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
    VerifiedVirtualValueLeaf value;
    value.value = 101;
    value.leaf_value = 53;
    if (consume_verified_virtual_value_root(value) != 101 ||
        value.read_leaf() != 53) {
        return 1;
    }
    if (consume_verified_virtual_cleanup_root(
            VerifiedVirtualCleanupLeaf(&cleanup_count)) != 101) {
        return 2;
    }
    return cleanup_count == 1 ? 0 : 3;
}
