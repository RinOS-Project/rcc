struct VerifiedAutomaticCleanupLeaf {
    int* cleanup_order;
    int cleanup_id;

    ~VerifiedAutomaticCleanupLeaf() noexcept {
        *cleanup_order = *cleanup_order * 10 + cleanup_id;
    }
};

struct VerifiedAutomaticCleanupValue {
    int* cleanup_order;
    int value;
    VerifiedAutomaticCleanupLeaf children[2][3];

    ~VerifiedAutomaticCleanupValue() noexcept {
        *cleanup_order = *cleanup_order * 10 + 9;
    }
};

int verify_automatic_array_cleanup(int* cleanup_order,
                                   int early_return) noexcept {
    int result = 0;
    {
        VerifiedAutomaticCleanupValue value{
            cleanup_order, 39,
            {{{cleanup_order, 1}, {cleanup_order, 2},
              {cleanup_order, 3}},
             {{cleanup_order, 4}, {cleanup_order, 5},
              {cleanup_order, 6}}}};
        if (early_return) return value.value;
        result = value.value;
    }
    return result;
}

int main() {
    int normal_order = 0;
    if (verify_automatic_array_cleanup(&normal_order, 0) != 39 ||
        normal_order != 9654321) {
        return 1;
    }
    int early_order = 0;
    if (verify_automatic_array_cleanup(&early_order, 1) != 39 ||
        early_order != 9654321) {
        return 2;
    }
    return 0;
}
