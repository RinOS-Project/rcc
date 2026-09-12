struct ReferenceValue final {
    int value;
};

namespace reference_test {

inline ReferenceValue mutable_reference(ReferenceValue& input) noexcept {
    return ReferenceValue{input.value};
}

}

extern "C" int reject_const_reference(void) {
    const ReferenceValue value{7};
    return reference_test::mutable_reference(value).value;
}
