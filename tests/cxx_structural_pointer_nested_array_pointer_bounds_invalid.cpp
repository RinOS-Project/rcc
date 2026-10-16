int structural_pointer_nested_array_pointer_bounds_values[2][3] = {};

struct StructuralNestedArrayPointerBoundsKey {
    int (*pointer)[3];
};

template <StructuralNestedArrayPointerBoundsKey Value>
struct StructuralNestedArrayPointerBoundsOwner {};

StructuralNestedArrayPointerBoundsOwner<StructuralNestedArrayPointerBoundsKey{
    &structural_pointer_nested_array_pointer_bounds_values[1] + 2}> invalid;
