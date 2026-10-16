struct StructuralNullptrKey {
    decltype(nullptr) pointer;
    int payload;
};

template <StructuralNullptrKey Value>
struct StructuralNullptrOwner {
    template <typename Element>
    struct Inner {
        int result() const { return 11; }
    };
};

template <StructuralNullptrKey Value>
template <typename Element>
struct StructuralNullptrOwner<Value>::Inner<Element*> {
    int result() const {
        return Value.pointer == nullptr ? Value.payload : -1;
    }
};

int main() {
    StructuralNullptrOwner<StructuralNullptrKey{nullptr, 42}>
        ::Inner<int*> first;
    StructuralNullptrOwner<StructuralNullptrKey{nullptr, 73}>
        ::Inner<int*> second;
    return first.result() == 42 && second.result() == 73 ? 0 : 1;
}
