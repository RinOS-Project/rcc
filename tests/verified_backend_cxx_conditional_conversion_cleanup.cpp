static int conversion_cleanup_order;

struct ConvertedReferenceTemporary {
    int value;
    int cleanup_id;
    int* cleanup_order;

    ~ConvertedReferenceTemporary() noexcept {
        *cleanup_order = *cleanup_order * 10 + cleanup_id;
    }
};

struct ReferenceTemporarySource {
    int value;
    int cleanup_id;
    int* cleanup_order;

    operator ConvertedReferenceTemporary() const noexcept {
        return {value, cleanup_id, cleanup_order};
    }
};

int consume_converted_reference_temporary(
        const ConvertedReferenceTemporary& value) noexcept {
    return value.value;
}

static int verify_conversion_arm(bool choose_first) {
    conversion_cleanup_order = 0;
    int result = consume_converted_reference_temporary(
        choose_first
            ? static_cast<ConvertedReferenceTemporary>(
                  ReferenceTemporarySource{41, 1,
                                           &conversion_cleanup_order})
            : static_cast<ConvertedReferenceTemporary>(
                  ReferenceTemporarySource{82, 2,
                                           &conversion_cleanup_order}));
    if (result != (choose_first ? 41 : 82)) return 1;
    return conversion_cleanup_order == (choose_first ? 1 : 2) ? 0 : 2;
}

int main() {
    int result = verify_conversion_arm(true);
    if (result) return result;
    return verify_conversion_arm(false);
}
