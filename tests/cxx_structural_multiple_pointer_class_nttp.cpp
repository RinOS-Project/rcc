int structural_multi_pointer_first = 17;
int structural_multi_pointer_second = 31;
int structural_multi_pointer_third = 43;
int structural_multi_pointer_other = 59;

struct StructuralPointerPair {
    int* first;
    int* second;
};

struct StructuralNestedPointerValue {
    StructuralPointerPair pair;
    int* third;
    int bias;
};

template <StructuralNestedPointerValue Value>
struct StructuralNestedPointerOwner {
    static int sum() {
        return *Value.pair.first + *Value.pair.second + *Value.third +
            Value.bias;
    }

    static int matches(int* first, int* second, int* third) {
        return Value.pair.first == first && Value.pair.second == second &&
            Value.third == third;
    }
};

template <typename Left, typename Right>
struct StructuralSameType {
    char value[2];
};

template <typename Type>
struct StructuralSameType<Type, Type> {
    char value;
};

using StructuralPointerOwner = StructuralNestedPointerOwner<
    StructuralNestedPointerValue{
        {&structural_multi_pointer_first, &structural_multi_pointer_second},
        &structural_multi_pointer_third,
        7,
    }>;

using StructuralSamePointerOwner = StructuralNestedPointerOwner<
    StructuralNestedPointerValue{
        {&structural_multi_pointer_first, &structural_multi_pointer_second},
        &structural_multi_pointer_third,
        7,
    }>;

using StructuralDifferentPointerOwner = StructuralNestedPointerOwner<
    StructuralNestedPointerValue{
        {&structural_multi_pointer_first, &structural_multi_pointer_other},
        &structural_multi_pointer_third,
        7,
    }>;

static_assert(sizeof(StructuralSameType<StructuralPointerOwner,
                                        StructuralSamePointerOwner>) == 1);
static_assert(sizeof(StructuralSameType<StructuralPointerOwner,
                                        StructuralDifferentPointerOwner>) == 2);

int main() {
    return StructuralNestedPointerOwner<
                StructuralNestedPointerValue{
                    {&structural_multi_pointer_first,
                     &structural_multi_pointer_second},
                    &structural_multi_pointer_third,
                    7,
                }>::sum() == 98 &&
            StructuralNestedPointerOwner<
                StructuralNestedPointerValue{
                    {&structural_multi_pointer_first,
                     &structural_multi_pointer_second},
                    &structural_multi_pointer_third,
                    7,
                }>::matches(
                &structural_multi_pointer_first,
                &structural_multi_pointer_second,
                &structural_multi_pointer_third) &&
            StructuralNestedPointerOwner<
                StructuralNestedPointerValue{
                    {&structural_multi_pointer_first,
                     &structural_multi_pointer_other},
                    &structural_multi_pointer_third,
                    7,
                }>::sum() == 126
            && StructuralNestedPointerOwner<
                StructuralNestedPointerValue{
                    {&structural_multi_pointer_first,
                     &structural_multi_pointer_other},
                    &structural_multi_pointer_third,
                    7,
                }>::matches(
                &structural_multi_pointer_first,
                &structural_multi_pointer_other,
                &structural_multi_pointer_third)
        ? 0
        : 1;
}
