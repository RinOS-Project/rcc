struct WideStructuralBitField {
    signed int negative : 40;
    unsigned int positive : 40;
    int tail;
};

union WideStructuralUnionBitField {
    signed int negative : 40;
    unsigned int positive : 40;
};

template<WideStructuralBitField Value>
constexpr int wide_structural_bitfield_value() {
    return Value.negative + static_cast<int>(Value.positive) + Value.tail;
}

template<WideStructuralUnionBitField Value>
constexpr int wide_structural_union_signed_value() {
    return Value.negative;
}

template<WideStructuralUnionBitField Value>
constexpr int wide_structural_union_unsigned_value() {
    return static_cast<int>(Value.positive);
}

static_assert(sizeof(WideStructuralBitField) == 20);
static_assert(sizeof(WideStructuralUnionBitField) == 8);
static_assert(wide_structural_bitfield_value<
                  WideStructuralBitField{.negative = -17,
                                         .positive = 23,
                                         .tail = 5}>() == 11);
static_assert(wide_structural_union_signed_value<
                  WideStructuralUnionBitField{.negative = -29}>() == -29);
static_assert(wide_structural_union_unsigned_value<
                  WideStructuralUnionBitField{.positive = 41}>() == 41);

#if defined(RCC_STRUCTURAL_WIDE_BITFIELD_VERIFIED)
int main() {
    return wide_structural_bitfield_value<
               WideStructuralBitField{.negative = -17,
                                      .positive = 23,
                                      .tail = 5}>() != 11 ||
           wide_structural_union_signed_value<
               WideStructuralUnionBitField{.negative = -29}>() != -29 ||
           wide_structural_union_unsigned_value<
               WideStructuralUnionBitField{.positive = 41}>() != 41;
}
#else
int main() {
    WideStructuralBitField value{};
    value.negative = -31;
    value.positive = 43;
    value.tail = 7;
    if (value.negative != -31 || value.positive != 43 || value.tail != 7 ||
        sizeof(value) != 20 ||
        wide_structural_bitfield_value<
            WideStructuralBitField{.negative = -17,
                                   .positive = 23,
                                   .tail = 5}>() != 11) {
        return 1;
    }

    WideStructuralUnionBitField union_value{};
    union_value.negative = -37;
    if (union_value.negative != -37 || sizeof(union_value) != 8 ||
        wide_structural_union_signed_value<
            WideStructuralUnionBitField{.negative = -29}>() != -29) {
        return 2;
    }
    union_value.positive = 47;
    if (union_value.positive != 47 ||
        wide_structural_union_unsigned_value<
            WideStructuralUnionBitField{.positive = 41}>() != 41) {
        return 3;
    }
    return 0;
}
#endif
