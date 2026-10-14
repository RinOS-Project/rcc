template <typename OuterType>
struct NestedTemplateOwnerTemplate {
    template <typename InnerType>
    struct Value {
        OuterType outer;
        InnerType inner;

        int combined() const {
            return static_cast<int>(outer) + static_cast<int>(inner);
        }
    };
};

using IntegerOuterValue =
    typename NestedTemplateOwnerTemplate<int>::Value<long>;

int main() {
    IntegerOuterValue integer_outer{};
    NestedTemplateOwnerTemplate<long>::Value<long> long_outer{};
    integer_outer.outer = 19;
    integer_outer.inner = 23;
    long_outer.outer = 17;
    long_outer.inner = 25;
    return integer_outer.combined() == 42 &&
                   long_outer.combined() == 42
        ? 0 : 1;
}
