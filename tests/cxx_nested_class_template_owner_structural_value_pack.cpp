struct NestedStructuralOwnerKey {
    int value;
};

enum class NestedStructuralOwnerCode : int {
    three = 3,
    seven = 7,
    thirty_seven = 37,
};

struct NestedStructuralOwnerEnumKey {
    NestedStructuralOwnerCode value;
};

struct NestedStructuralOwnerNullPointerKey {
    const int* value;
};

struct NestedStructuralEmptyKey {};

struct NestedStructuralPairKey {
    int first;
    int second;
};

struct NestedStructuralLeafKey {
    signed char first;
    unsigned short second = 7;
};

struct NestedStructuralKey {
    int prefix;
    NestedStructuralLeafKey leaf;
};

struct NestedStructuralPointerLeafKey {
    int* pointer;
};

struct NestedStructuralPointerKey {
    NestedStructuralPointerLeafKey leaf;
};

template <NestedStructuralOwnerKey... Values>
struct NestedStructuralValueOwnerPack {
    template <typename Element>
    struct Inner {
        int result() const { return 10; }
    };
};

template <NestedStructuralOwnerKey... Values>
template <typename Element>
struct NestedStructuralValueOwnerPack<Values...>::Inner<Element*> {
    int result() const { return 22; }
};

template <NestedStructuralOwnerEnumKey... Values>
struct NestedStructuralEnumValueOwnerPack {
    template <typename Element>
    struct Inner {
        int result() const { return 11; }
    };
};

template <NestedStructuralOwnerEnumKey... Values>
template <typename Element>
struct NestedStructuralEnumValueOwnerPack<Values...>::Inner<Element*> {
    int result() const { return 24; }
};

template <NestedStructuralOwnerNullPointerKey... Values>
struct NestedStructuralNullPointerOwnerPack {
    template <typename Element>
    struct Inner {
        int result() const { return 12; }
    };
};

template <NestedStructuralOwnerNullPointerKey... Values>
template <typename Element>
struct NestedStructuralNullPointerOwnerPack<Values...>::Inner<Element*> {
    int result() const { return 25; }
};

template <NestedStructuralOwnerKey Value>
struct NestedStructuralValueOwner {
    template <typename Element>
    struct Inner {
        int result() const { return Value.value; }
    };
};

template <NestedStructuralOwnerEnumKey Value>
struct NestedStructuralEnumValueOwner {
    template <typename Element>
    struct Inner {
        int result() const { return static_cast<int>(Value.value); }
    };
};

template <NestedStructuralOwnerNullPointerKey Value>
struct NestedStructuralNullPointerOwner {
    template <typename Element>
    struct Inner {
        int result() const { return Value.value == nullptr ? 41 : 0; }
    };
};

template <NestedStructuralEmptyKey Value>
struct NestedStructuralEmptyOwner {
    template <typename Element>
    struct Inner {
        int result() const { return 19; }
    };
};

template <NestedStructuralPairKey Value>
struct NestedStructuralPairOwner {
    template <typename Element>
    struct Inner {
        int result() const { return Value.first * 100 + Value.second; }
    };
};

template <NestedStructuralKey Value>
struct NestedStructuralNestedOwner {
    template <typename Element>
    struct Inner {
        int result() const {
            return Value.prefix * 10000 + Value.leaf.first * 100 +
                   Value.leaf.second;
        }
    };
};

template <NestedStructuralPointerKey Value>
struct NestedStructuralPointerOwner {
    template <typename Element>
    struct Inner {
        int result() const { return Value.leaf.pointer == nullptr ? 17 : 0; }
    };
};

template <typename OwnerType, NestedStructuralOwnerKey OwnerValue>
struct NestedStructuralReorderedOwner {
    template <typename Element>
    struct Inner {
        int result() const { return 13; }
    };
};

template <NestedStructuralOwnerKey OwnerValue, typename OwnerType>
struct NestedStructuralReorderedOwner<OwnerType*, OwnerValue> {
    template <typename Element>
    struct Inner {
        int result() const { return 17; }
    };
};

template <NestedStructuralOwnerKey OwnerValue, typename OwnerType>
template <typename Element>
struct NestedStructuralReorderedOwner<OwnerType*, OwnerValue>::Inner<Element*> {
    int result() const { return OwnerValue.value; }
};

template <NestedStructuralOwnerKey... LeftValues>
struct NestedStructuralZipOwner {
    template <NestedStructuralOwnerEnumKey... RightValues>
    struct Zip {
        template <typename... Elements>
        int combined() const {
            return ((LeftValues.value +
                     static_cast<int>(RightValues.value) +
                     static_cast<int>(sizeof(Elements))) + ... + 0);
        }
    };
};

int main() {
    NestedStructuralValueOwnerPack<
        NestedStructuralOwnerKey{3}, NestedStructuralOwnerKey{7}>
        ::Inner<int*> partial;
    NestedStructuralValueOwner<NestedStructuralOwnerKey{37}>::Inner<int>
        parameter_object;
    NestedStructuralEnumValueOwnerPack<
        NestedStructuralOwnerEnumKey{NestedStructuralOwnerCode::three},
        NestedStructuralOwnerEnumKey{NestedStructuralOwnerCode::seven}>
        ::Inner<int*> enum_partial;
    NestedStructuralEnumValueOwner<
        NestedStructuralOwnerEnumKey{
            NestedStructuralOwnerCode::thirty_seven}>::Inner<int>
        enum_parameter_object;
    NestedStructuralNullPointerOwnerPack<
        NestedStructuralOwnerNullPointerKey{nullptr},
        NestedStructuralOwnerNullPointerKey{0}>::Inner<int*>
        null_pointer_partial;
    NestedStructuralNullPointerOwner<
        NestedStructuralOwnerNullPointerKey{nullptr}>::Inner<int>
        null_pointer_parameter_object;
    NestedStructuralEmptyOwner<NestedStructuralEmptyKey{}>::Inner<int>
        empty_parameter_object;
    NestedStructuralPairOwner<
        NestedStructuralPairKey{1, 2}>::Inner<int> pair_parameter_object;
    NestedStructuralPairOwner<
        NestedStructuralPairKey{-3, 4}>::Inner<int>
        signed_pair_parameter_object;
    NestedStructuralNestedOwner<
        NestedStructuralKey{3, {-2}}>::Inner<int>
        nested_parameter_object;
    NestedStructuralPointerOwner<
        NestedStructuralPointerKey{{nullptr}}>::Inner<int>
        nested_null_pointer_parameter_object;
    NestedStructuralReorderedOwner<
        int*, NestedStructuralOwnerKey{43}>::Inner<int*>
        reordered_type_first;
    NestedStructuralReorderedOwner<
        long*, NestedStructuralOwnerKey{47}>::Inner<long*>
        reordered_type_second;
    NestedStructuralReorderedOwner<
        int, NestedStructuralOwnerKey{43}>::Inner<int*>
        reordered_primary_fallback;
    NestedStructuralZipOwner<
        NestedStructuralOwnerKey{1}, NestedStructuralOwnerKey{2}>::Zip<
            NestedStructuralOwnerEnumKey{
                NestedStructuralOwnerCode::three},
            NestedStructuralOwnerEnumKey{
                NestedStructuralOwnerCode::seven}>
        zipped_structural_values;
    NestedStructuralZipOwner<>::Zip<> empty_structural_values;
    if (partial.result() != 22) return 1;
    if (parameter_object.result() != 37) return 2;
    if (enum_partial.result() != 24) return 3;
    if (enum_parameter_object.result() != 37) return 4;
    if (null_pointer_partial.result() != 25) return 5;
    if (null_pointer_parameter_object.result() != 41) return 6;
    if (empty_parameter_object.result() != 19) return 7;
    if (pair_parameter_object.result() != 102) return 8;
    if (signed_pair_parameter_object.result() != -296) return 9;
    if (nested_parameter_object.result() != 29807) return 10;
    if (nested_null_pointer_parameter_object.result() != 17) return 11;
    if (reordered_type_first.result() != 43) return 12;
    if (reordered_type_second.result() != 47) return 13;
    if (reordered_primary_fallback.result() != 13) return 14;
    if (zipped_structural_values.combined<char, int>() != 18) return 15;
    return empty_structural_values.combined<>() == 0 ? 0 : 16;
}
