template <typename OuterType>
struct NestedTemplateDependentOwnerTemplate {
    template <typename InnerType>
    struct Value {
        OuterType outer;
        InnerType inner;

        int combined() const {
            return static_cast<int>(outer) + static_cast<int>(inner);
        }
    };
};

template <typename OuterType, typename InnerType>
struct NestedTemplateDependentUse {
    using Value = typename NestedTemplateDependentOwnerTemplate<
        OuterType>::template Value<InnerType>;
};

using IntegerOuterValue =
    NestedTemplateDependentUse<int, long>::Value;
using LongOuterValue =
    NestedTemplateDependentUse<long, int>::Value;

template <typename OuterType, typename InnerType>
int make_dependent_nested_value(OuterType outer, InnerType inner) {
    typename NestedTemplateDependentOwnerTemplate<OuterType>::template Value<
        InnerType> value{};
    value.outer = outer;
    value.inner = inner;
    return value.combined();
}

int main() {
    IntegerOuterValue integer_outer{};
    LongOuterValue long_outer{};
    integer_outer.outer = 19;
    integer_outer.inner = 23;
    long_outer.outer = 17;
    long_outer.inner = 25;
    return integer_outer.combined() == 42 &&
                   long_outer.combined() == 42 &&
                   make_dependent_nested_value<int, long>(19, 23) == 42 &&
                   make_dependent_nested_value<long, int>(17, 25) == 42
        ? 0 : 1;
}
