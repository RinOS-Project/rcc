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
};

int consume_verified_virtual_value_root(
        VerifiedVirtualValueRoot value) noexcept {
    return value.value;
}

int main() {
    VerifiedVirtualValueLeaf value;
    value.value = 101;
    return consume_verified_virtual_value_root(value) == 101 ? 0 : 1;
}
