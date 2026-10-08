static int array_initializer_construction_count;

struct InitializedArrayElement {
    InitializedArrayElement() {
        array_initializer_construction_count =
            array_initializer_construction_count + 1;
        order = array_initializer_construction_count;
    }

    int order;
};

struct InitializedArrayBase {
    explicit InitializedArrayBase(int value) : value(value) {}
    int value;
};

template <typename T>
int initialized_array_probe(T value) {
    struct OrdinaryInitializedArray {
        InitializedArrayElement elements[2] = {};
        int marker;

        explicit OrdinaryInitializedArray(int initial_marker)
            : marker(initial_marker) {}
    };

    struct InitializedArrayDerived : InitializedArrayBase {
        using InitializedArrayBase::InitializedArrayBase;
        InitializedArrayElement elements[2] = {};
    };

    array_initializer_construction_count = 0;
    OrdinaryInitializedArray ordinary(7);
    if (ordinary.marker != 7 || array_initializer_construction_count != 2 ||
        ordinary.elements[0].order != 1 || ordinary.elements[1].order != 2) {
        return 1;
    }

    array_initializer_construction_count = 0;
    InitializedArrayDerived object(1);
    if (object.value != 1 || array_initializer_construction_count != 2 ||
        object.elements[0].order != 1 || object.elements[1].order != 2) {
        return 2;
    }
    return static_cast<int>(value);
}

int main() {
    if (initialized_array_probe(0) != 0) return 1;
    return initialized_array_probe(0LL);
}
