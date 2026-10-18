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
    VerifiedDerived(int* order, int initial) noexcept
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

int main() {
    int value = 40;
    if (update_lvalue_reference_local(value) != 41) {
        return 1;
    }
    if (update_rvalue_reference_local(static_cast<int&&>(value)) != 43) {
        return 2;
    }
    return value != 43;
}
