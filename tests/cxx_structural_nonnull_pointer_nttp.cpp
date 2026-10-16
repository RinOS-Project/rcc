int nested_structural_pointer_target_a = 47;
int nested_structural_pointer_target_b = 89;
static int nested_structural_internal_pointer_target = 103;
int nested_structural_function_pointer_target(int value) {
    return value + 5;
}

struct NestedStructuralPointerLeafKey {
    int* pointer;
};

struct NestedStructuralPointerKey {
    NestedStructuralPointerLeafKey leaf;
};

using NestedStructuralFunction = int (*)(int);

struct NestedStructuralFunctionPointerKey {
    NestedStructuralFunction pointer;
};

template <NestedStructuralPointerKey Value>
struct NestedStructuralPointerOwner {
    template <typename Element>
    struct Inner {
        int result() const { return *Value.leaf.pointer; }
    };
};

template <NestedStructuralFunctionPointerKey Value>
struct NestedStructuralFunctionPointerOwner {
    template <typename Element>
    struct Inner {
        int result() const { return Value.pointer(37); }
    };
};

int main() {
    NestedStructuralPointerOwner<
        NestedStructuralPointerKey{{&nested_structural_pointer_target_a}}>
        ::Inner<int> first;
    NestedStructuralPointerOwner<
        NestedStructuralPointerKey{{&nested_structural_pointer_target_b}}>
        ::Inner<int> second;
    NestedStructuralFunctionPointerOwner<
        NestedStructuralFunctionPointerKey{
            &nested_structural_function_pointer_target}>
        ::Inner<int> function;
    NestedStructuralPointerOwner<
        NestedStructuralPointerKey{{
            &nested_structural_internal_pointer_target}}>
        ::Inner<int> internal;
    return first.result() == 47 && second.result() == 89 &&
        function.result() == 42 && internal.result() == 103 ? 0 : 1;
}
