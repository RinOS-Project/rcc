struct StructuralUnnamedBitField {
    signed int left : 5;
    unsigned int : 11;
    unsigned int : 0;
    int right;
};

class StructuralPrivateUnnamedBitField {
    unsigned int : 11;

public:
    int value;

    constexpr explicit StructuralPrivateUnnamedBitField(int initial_value)
        : value(initial_value) {}
};

union StructuralUnnamedUnionBitField {
    unsigned int : 8;
    signed int value;
    unsigned int other : 8;
};

template<StructuralUnnamedBitField Value>
constexpr int structural_unnamed_bitfield_value() {
    return Value.left + Value.right;
}

template<StructuralPrivateUnnamedBitField Value>
constexpr int structural_private_unnamed_bitfield_value() {
    return Value.value;
}

template<StructuralUnnamedUnionBitField Value>
constexpr int structural_unnamed_union_signed_value() {
    return Value.value;
}

template<StructuralUnnamedUnionBitField Value>
constexpr int structural_unnamed_union_unsigned_value() {
    return static_cast<int>(Value.other);
}

template<StructuralUnnamedBitField Value>
struct StructuralUnnamedTypeInfo {};

template<StructuralUnnamedUnionBitField Value>
struct StructuralUnnamedUnionTypeInfo {};

static_assert(sizeof(StructuralUnnamedBitField) == 8);
static_assert(sizeof(StructuralPrivateUnnamedBitField) == 8);
static_assert(sizeof(StructuralUnnamedUnionBitField) == 4);
static_assert(structural_unnamed_bitfield_value<
                  StructuralUnnamedBitField{-7, 13}>() == 6);
static_assert(structural_private_unnamed_bitfield_value<
                  StructuralPrivateUnnamedBitField{37}>() == 37);
static_assert(structural_unnamed_union_signed_value<
                  StructuralUnnamedUnionBitField{.value = -19}>() == -19);
static_assert(structural_unnamed_union_unsigned_value<
                  StructuralUnnamedUnionBitField{.other = 23}>() == 23);

#if defined(RCC_STRUCTURAL_UNNAMED_BITFIELD_VERIFIED)
int main() {
    return structural_unnamed_bitfield_value<
               StructuralUnnamedBitField{-7, 13}>() != 6 ||
           structural_private_unnamed_bitfield_value<
               StructuralPrivateUnnamedBitField{37}>() != 37 ||
           structural_unnamed_union_signed_value<
               StructuralUnnamedUnionBitField{.value = -19}>() != -19 ||
           structural_unnamed_union_unsigned_value<
               StructuralUnnamedUnionBitField{.other = 23}>() != 23;
}
#else
int main() {
    StructuralUnnamedBitField value{-11, 17};
    if (value.left != -11 || value.right != 17 || sizeof(value) != 8 ||
        structural_unnamed_bitfield_value<
            StructuralUnnamedBitField{-7, 13}>() != 6) {
        return 1;
    }

    if (structural_private_unnamed_bitfield_value<
            StructuralPrivateUnnamedBitField{37}>() != 37) {
        return 2;
    }

    StructuralUnnamedUnionBitField union_value{};
    union_value.value = -29;
    if (union_value.value != -29 || sizeof(union_value) != 4 ||
        structural_unnamed_union_signed_value<
            StructuralUnnamedUnionBitField{.value = -19}>() != -19) {
        return 3;
    }
    union_value.other = 31;
    if (union_value.other != 31 ||
        structural_unnamed_union_unsigned_value<
            StructuralUnnamedUnionBitField{.other = 23}>() != 23) {
        return 4;
    }
    if (&typeid(StructuralUnnamedTypeInfo<
            StructuralUnnamedBitField{-7, 13}>) !=
            &typeid(StructuralUnnamedTypeInfo<
                StructuralUnnamedBitField{-7, 13}>) ||
        &typeid(StructuralUnnamedTypeInfo<
            StructuralUnnamedBitField{-7, 13}>) ==
            &typeid(StructuralUnnamedTypeInfo<
                StructuralUnnamedBitField{-7, 14}>)) {
        return 5;
    }
    if (&typeid(StructuralUnnamedUnionTypeInfo<
            StructuralUnnamedUnionBitField{.value = -19}>) !=
            &typeid(StructuralUnnamedUnionTypeInfo<
                StructuralUnnamedUnionBitField{.value = -19}>) ||
        &typeid(StructuralUnnamedUnionTypeInfo<
            StructuralUnnamedUnionBitField{.value = -19}>) ==
            &typeid(StructuralUnnamedUnionTypeInfo<
                StructuralUnnamedUnionBitField{.other = 23}>)) {
        return 6;
    }
    return 0;
}
#endif
