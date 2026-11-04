struct StructuralForwardingSource {
    int value;
};

template<int Value>
constexpr int structural_forwarding_integer_target() {
    return Value;
}

template<StructuralForwardingSource Value>
constexpr int structural_forwarding_invalid_source() {
    return structural_forwarding_integer_target<Value>();
}

static_assert(structural_forwarding_invalid_source<
                  StructuralForwardingSource{19}>() == 19);

int main() {
    return structural_forwarding_invalid_source<
               StructuralForwardingSource{19}>() != 19;
}
