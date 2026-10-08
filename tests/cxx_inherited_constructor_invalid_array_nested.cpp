struct NestedArrayElement {
    NestedArrayElement() : value(1) {}
    int value;
};
typedef NestedArrayElement NestedArrayRow[2];

struct NestedArrayBase {
    explicit NestedArrayBase(int value) : value(value) {}
    int value;
};

struct NestedArrayDerived : NestedArrayBase {
    using NestedArrayBase::NestedArrayBase;
    NestedArrayRow elements[2];
};

int main() {
    NestedArrayDerived value(1);
    return value.value;
}
