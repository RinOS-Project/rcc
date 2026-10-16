struct StructuralFloatingKey {
    float ratio;
    double offset;
    int payload;
};

template <StructuralFloatingKey Value>
struct StructuralFloatingOwner {
    template <typename Element>
    struct Inner {
        int result() const { return 11; }
    };
};

template <StructuralFloatingKey Value>
template <typename Element>
struct StructuralFloatingOwner<Value>::Inner<Element*> {
    int result() const {
        return (Value.ratio == 1.25f && Value.offset == -2.5) ||
                (Value.ratio == -0.5f && Value.offset == 3.25) ||
                (Value.ratio == 1.0f && Value.offset == 0.25)
            ? Value.payload : -1;
    }
};

int main() {
    StructuralFloatingOwner<StructuralFloatingKey{1.25, -2.5, 42}>
        ::Inner<int*> first;
    StructuralFloatingOwner<StructuralFloatingKey{-0.5, 3.25, 73}>
        ::Inner<int*> second;
    StructuralFloatingOwner<StructuralFloatingKey{1, 0.25f, 5}>
        ::Inner<int*> exact_conversions;
    return first.result() == 42 && second.result() == 73 &&
            exact_conversions.result() == 5 ? 0 : 1;
}
