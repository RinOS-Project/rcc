struct NestedAliasBase {
    using value_type = int;
};

struct NestedAliasDerived : NestedAliasBase {};

template <typename T>
struct NestedAliasBox {
    typename T::value_type value;

    typename T::value_type read() const {
        typename T::value_type result = value;
        return result;
    }
};

int main() {
    NestedAliasBox<NestedAliasDerived> box;
    box.value = 7;
    return box.read() == 7 ? 0 : 1;
}
