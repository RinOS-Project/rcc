struct NestedStructuralOwnerKey {
    int value;
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

template <NestedStructuralOwnerKey Value>
struct NestedStructuralValueOwner {
    template <typename Element>
    struct Inner {
        int result() const { return Value.value; }
    };
};

int main() {
    NestedStructuralValueOwnerPack<
        NestedStructuralOwnerKey{3}, NestedStructuralOwnerKey{7}>
        ::Inner<int*> partial;
    NestedStructuralValueOwner<NestedStructuralOwnerKey{37}>::Inner<int>
        parameter_object;
    if (partial.result() != 22) return 1;
    return parameter_object.result() == 37 ? 0 : 2;
}
