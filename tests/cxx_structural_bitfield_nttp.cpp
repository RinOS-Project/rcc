enum class StructuralBitFieldMode : unsigned int {
    first = 1,
    second = 2
};

struct StructuralBitFieldKey {
    int bias;
    signed int displacement : 5;
    unsigned int category : 3;
    StructuralBitFieldMode mode : 2;
};

template <StructuralBitFieldKey Value>
struct StructuralBitFieldOwner {
    template <typename Element>
    struct Inner {
        int result() const { return Value.bias; }
    };
};

template <StructuralBitFieldKey Value>
template <typename Element>
struct StructuralBitFieldOwner<Value>::Inner<Element*> {
    int result() const {
        return Value.bias + Value.displacement * 10 + Value.category +
            (Value.mode == StructuralBitFieldMode::first ? 1 : 2);
    }
};

int main() {
    StructuralBitFieldOwner<StructuralBitFieldKey{
        100, -3, 5, StructuralBitFieldMode::first}>
        ::Inner<int*> first;
    StructuralBitFieldOwner<StructuralBitFieldKey{
        200, 7, 2, StructuralBitFieldMode::second}>
        ::Inner<int*> second;
    return first.result() == 76 && second.result() == 274 ? 0 : 1;
}
