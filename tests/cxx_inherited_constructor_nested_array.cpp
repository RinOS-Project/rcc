struct NestedArrayElement {
    NestedArrayElement() : value(1) {}
    int value;
};

typedef NestedArrayElement NestedArrayRow[2];

struct NestedArrayBase {
    explicit NestedArrayBase(int initial_value) : value(initial_value) {}
    int value;
};

int nested_array_probe() {
    struct NestedArrayDerived : NestedArrayBase {
        using NestedArrayBase::NestedArrayBase;
        NestedArrayRow elements[2];
    };

    NestedArrayDerived object(7);
    if (object.value != 7 || object.elements[0][0].value != 1 ||
        object.elements[0][1].value != 1 ||
        object.elements[1][0].value != 1 ||
        object.elements[1][1].value != 1) {
        return 1;
    }
    return 0;
}

int main() {
    return nested_array_probe();
}
