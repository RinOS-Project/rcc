union StructuralBitFieldUnion {
    unsigned int low : 5;
    signed int high : 6;
};

template<StructuralBitFieldUnion Value>
constexpr int structural_bitfield_union_value() {
    return Value.low;
}

template<StructuralBitFieldUnion Value>
constexpr int structural_bitfield_union_signed_value() {
    return Value.high;
}

static_assert(
    structural_bitfield_union_value<
        StructuralBitFieldUnion{.low = 17}>() == 17);
static_assert(
    structural_bitfield_union_signed_value<
        StructuralBitFieldUnion{.high = -7}>() == -7);

int main() {
    return structural_bitfield_union_value<
               StructuralBitFieldUnion{.low = 17}>() != 17 ||
           structural_bitfield_union_signed_value<
               StructuralBitFieldUnion{.high = -7}>() != -7;
}
