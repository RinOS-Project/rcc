int nested_computed_pointer_target_a = 47;
int nested_computed_pointer_target_b = 89;

int nested_computed_pointer_function_a(int value) {
    return value + 5;
}

int nested_computed_pointer_function_b(int value) {
    return value + 9;
}

double nested_computed_pointer_overloaded_target(double value) {
    return value + 1.5;
}

int nested_computed_pointer_overloaded_target(int value) {
    return value + 6;
}

template <typename Value>
int nested_computed_pointer_overloaded_template(Value value) {
    return static_cast<int>(value) + 10;
}

template <typename Value>
int nested_computed_pointer_overloaded_template(Value* value) {
    return static_cast<int>(*value) + 20;
}

int nested_computed_pointer_mixed_template_target(long value) {
    return static_cast<int>(value) + 30;
}

template <typename Value>
int nested_computed_pointer_mixed_template_target(Value value) {
    return static_cast<int>(value) + 10;
}

int nested_computed_pointer_mixed_non_template_target(int value) {
    return value + 6;
}

template <typename Value>
int nested_computed_pointer_mixed_non_template_target(Value value) {
    return static_cast<int>(value) + 20;
}

int nested_computed_pointer_nonviable_template_target(int value) {
    return value + 6;
}

template <typename Value>
int nested_computed_pointer_nonviable_template_target(Value* value) {
    return static_cast<int>(*value) + 20;
}

struct NestedComputedPointerLeafKey {
    int* pointer;
};

struct NestedComputedPointerKey {
    NestedComputedPointerLeafKey leaf;
};

using NestedComputedFunction = int (*)(int);

struct NestedComputedFunctionKey {
    NestedComputedFunction pointer;
};

template <NestedComputedPointerKey Value>
struct NestedComputedPointerOwner {
    template <typename Element>
    struct Inner {
        int result() const { return *Value.leaf.pointer; }
    };
};

template <NestedComputedPointerKey Value>
struct NestedComputedNullPointerOwner {
    template <typename Element>
    struct Inner {
        bool is_null() const { return Value.leaf.pointer == nullptr; }
    };
};

template <NestedComputedFunctionKey Value>
struct NestedComputedFunctionOwner {
    template <typename Element>
    struct Inner {
        int result() const { return Value.pointer(37); }
    };
};

int main() {
    NestedComputedPointerOwner<
        NestedComputedPointerKey{{&nested_computed_pointer_target_a + (1 - 1)}}>
        ::Inner<int> add_zero;
    NestedComputedPointerOwner<
        NestedComputedPointerKey{{0 + &nested_computed_pointer_target_a}}>
        ::Inner<int> reverse_add_zero;
    NestedComputedPointerOwner<
        NestedComputedPointerKey{{&nested_computed_pointer_target_a - (1 - 1)}}>
        ::Inner<int> subtract_zero;
    NestedComputedPointerOwner<
        NestedComputedPointerKey{{false
            ? &nested_computed_pointer_target_a
            : &nested_computed_pointer_target_b}}>::Inner<int> conditional;
    NestedComputedPointerOwner<
        NestedComputedPointerKey{{true
            ? static_cast<int*>(&nested_computed_pointer_target_a)
            : &nested_computed_pointer_target_b}}>::Inner<int> cast;
    NestedComputedNullPointerOwner<
        NestedComputedPointerKey{{true
            ? nullptr : &nested_computed_pointer_target_a}}>::Inner<int> null;
    NestedComputedNullPointerOwner<
        NestedComputedPointerKey{{true
            ? 0 : &nested_computed_pointer_target_a}}>::Inner<int> integer_null;
    NestedComputedFunctionOwner<NestedComputedFunctionKey{
        false ? &nested_computed_pointer_function_a
              : &nested_computed_pointer_function_b}>::Inner<int> function;
    NestedComputedFunctionOwner<NestedComputedFunctionKey{
        &nested_computed_pointer_overloaded_target}>::Inner<int> overloaded;
    NestedComputedFunctionOwner<NestedComputedFunctionKey{
        &nested_computed_pointer_overloaded_template}>::Inner<int>
        overloaded_template;
    NestedComputedFunctionOwner<NestedComputedFunctionKey{
        &nested_computed_pointer_mixed_template_target}>::Inner<int>
        mixed_template_wins;
    NestedComputedFunctionOwner<NestedComputedFunctionKey{
        &nested_computed_pointer_mixed_non_template_target}>::Inner<int>
        mixed_non_template_wins;
    NestedComputedFunctionOwner<NestedComputedFunctionKey{
        &nested_computed_pointer_nonviable_template_target}>::Inner<int>
        nonviable_template_uses_ordinary;
    return add_zero.result() == 47 &&
        reverse_add_zero.result() == 47 &&
        subtract_zero.result() == 47 && conditional.result() == 89 &&
        cast.result() == 47 && null.is_null() && integer_null.is_null() &&
        function.result() == 46 && overloaded.result() == 43 &&
        overloaded_template.result() == 47 &&
        mixed_template_wins.result() == 47 &&
        mixed_non_template_wins.result() == 43 &&
        nonviable_template_uses_ordinary.result() == 43 ? 0 : 1;
}
