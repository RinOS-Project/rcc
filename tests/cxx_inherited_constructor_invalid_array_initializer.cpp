struct InitializedArrayElement {
    InitializedArrayElement() : value(1) {}
    int value;
};

struct InitializedArrayBase {
    explicit InitializedArrayBase(int value) : value(value) {}
    int value;
};

template <typename T>
int initialized_array_probe(T value) {
    struct InitializedArrayDerived : InitializedArrayBase {
        using InitializedArrayBase::InitializedArrayBase;
        InitializedArrayElement elements[2] = {};
    };

    InitializedArrayDerived object(1);
    return object.value + static_cast<int>(value);
}

int main() {
    return initialized_array_probe(0);
}
