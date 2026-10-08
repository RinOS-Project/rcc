struct CleanupArrayElement {
    CleanupArrayElement() : value(1) {}
    ~CleanupArrayElement() {}
    int value;
};

struct CleanupArrayBase {
    explicit CleanupArrayBase(int value) : value(value) {}
    int value;
};

struct CleanupArrayDerived : CleanupArrayBase {
    using CleanupArrayBase::CleanupArrayBase;
    CleanupArrayElement elements[2];
};

int main() {
    CleanupArrayDerived value(1);
    return value.value;
}
