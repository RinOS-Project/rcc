struct NestedArrayElement {
    NestedArrayElement() : value(1) {}
    int value;
};
typedef NestedArrayElement NestedArrayRow[2];

struct NestedArrayBase {
    explicit NestedArrayBase(int value) : value(value) {}
    int value;
};

template <typename T>
int nested_array_probe(T value) {
    struct NestedArrayDerived : NestedArrayBase {
        using NestedArrayBase::NestedArrayBase;
        NestedArrayRow elements[2];
    };

    NestedArrayDerived object(1);
    return object.value + static_cast<int>(value);
}

int main() {
    return nested_array_probe(0);
}
