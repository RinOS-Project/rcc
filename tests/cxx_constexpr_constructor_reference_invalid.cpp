struct UninitializedConstructorReferenceTarget {
    int value;

    constexpr explicit UninitializedConstructorReferenceTarget() {
        int local;
        const int& reference = local;
        this->value = reference;
    }
};

constexpr UninitializedConstructorReferenceTarget invalid_reference_target{};

int main() {
    return invalid_reference_target.value;
}
