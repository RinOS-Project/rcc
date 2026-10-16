constexpr int structural_reference_add_five(int value) {
    return value + 5;
}

struct StructuralFunctionReferenceKey {
    int (&function)(int);
    int input;
};

template <StructuralFunctionReferenceKey Value>
struct StructuralFunctionReferenceOwner {
    static int select() {
        return Value.function(Value.input);
    }
};

int main() {
    return StructuralFunctionReferenceOwner<
               StructuralFunctionReferenceKey{
                   structural_reference_add_five, 37}>::select() == 42
        ? 0 : 1;
}
