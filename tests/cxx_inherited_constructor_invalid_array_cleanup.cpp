struct CleanupArrayElement {
    CleanupArrayElement() : value(1) {}
    ~CleanupArrayElement() {}
    int value;
};

struct CleanupArrayBase {
    explicit CleanupArrayBase(int value) : value(value) {}
    int value;
};

template <typename T>
int cleanup_array_probe(T value) {
    struct CleanupArrayDerived : CleanupArrayBase {
        using CleanupArrayBase::CleanupArrayBase;
        CleanupArrayElement elements[2];
    };

    CleanupArrayDerived object(1);
    return object.value + static_cast<int>(value);
}

int main() {
    return cleanup_array_probe(0);
}
