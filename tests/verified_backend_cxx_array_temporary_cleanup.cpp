struct VerifiedArrayCleanupLeaf {
    int* cleanup_order;
    int cleanup_id;

    ~VerifiedArrayCleanupLeaf() noexcept {
        *cleanup_order = *cleanup_order * 10 + cleanup_id;
    }
};

struct VerifiedArrayCleanupValue {
    int* cleanup_order;
    int cleanup_id;
    int value;
    VerifiedArrayCleanupLeaf children[2][3];

    ~VerifiedArrayCleanupValue() noexcept {
        *cleanup_order = *cleanup_order * 10 + cleanup_id;
    }
};

VerifiedArrayCleanupValue make_verified_array_cleanup_value(
        int* cleanup_order) noexcept {
    return {cleanup_order, 9, 39,
            {{{cleanup_order, 1}, {cleanup_order, 2},
              {cleanup_order, 3}},
             {{cleanup_order, 4}, {cleanup_order, 5},
              {cleanup_order, 6}}}};
}

int consume_verified_array_cleanup_value(
        const VerifiedArrayCleanupValue& value) noexcept {
    return value.value;
}

int main() {
    int cleanup_order = 0;
    if (consume_verified_array_cleanup_value(
            make_verified_array_cleanup_value(&cleanup_order)) != 39) {
        return 1;
    }
    if (cleanup_order != 9654321) return 2;
    return 0;
}
