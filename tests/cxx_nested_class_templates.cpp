struct NestedTemplateOwner {
    template <typename T>
    struct Value {
        T value;

        T get() const {
            return value;
        }
    };
};

class NestedTemplateProtectedOwner {
protected:
    template <typename ValueType>
    struct Visible {
        ValueType value;
    };
};

struct NestedTemplateProtectedDerived
    : NestedTemplateProtectedOwner {
    using Value = typename NestedTemplateProtectedOwner::Visible<long>;
};

int main() {
    NestedTemplateOwner::Value<int> value{};
    value.value = 37;
    NestedTemplateProtectedDerived::Value protected_value{};
    protected_value.value = 42;
    return value.get() == 37 && protected_value.value == 42
        ? 0 : 1;
}
