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
    if (partial.result() != 22) return 1;
    if (parameter_object.result() != 37) return 2;
    if (enum_partial.result() != 24) return 3;
    return enum_parameter_object.result() == 37 ? 0 : 4;
}
