struct NestedTemplateOwner {
    template <typename T>
    struct Value {
        T value;

        T get() const {
            return value;
        }
    };
};

int main() {
    NestedTemplateOwner::Value<int> value{};
    value.value = 37;
    return value.get() == 37 ? 0 : 1;
}
