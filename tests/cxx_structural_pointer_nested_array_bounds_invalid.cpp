int structural_pointer_nested_bounds_values[2][3] = {};

struct StructuralNestedBoundsPointerKey {
    int* pointer;
};

template <StructuralNestedBoundsPointerKey Value>
struct StructuralNestedBoundsPointerOwner {};

StructuralNestedBoundsPointerOwner<StructuralNestedBoundsPointerKey{
    &structural_pointer_nested_bounds_values[0][2] + 2}> invalid;
