struct InitializedArrayElement {
    InitializedArrayElement() : value(1) {}
    int value;
};

struct InitializedArrayBase {
    explicit InitializedArrayBase(int value) : value(value) {}
    int value;
};

struct InitializedArrayDerived : InitializedArrayBase {
    using InitializedArrayBase::InitializedArrayBase;
    InitializedArrayElement elements[2] = {};
};

int main() {
    InitializedArrayDerived value(1);
    return value.value;
}
