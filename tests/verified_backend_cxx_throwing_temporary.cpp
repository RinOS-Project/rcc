struct VerifiedThrowingTemporary {
    int* cleanup_order;
    int cleanup_id;

    ~VerifiedThrowingTemporary() noexcept {
        *cleanup_order = *cleanup_order * 10 + cleanup_id;
    }
};

VerifiedThrowingTemporary make_verified_throwing_temporary(
    int* cleanup_order, int cleanup_id) noexcept {
    return {cleanup_order, cleanup_id};
}

int consume_verified_throwing_temporary(
    const VerifiedThrowingTemporary& value, int should_throw) {
    if (should_throw) throw 47;
    return value.cleanup_id;
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

extern "C" int main() {
    return 0;
}
