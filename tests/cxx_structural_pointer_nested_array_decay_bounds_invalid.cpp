int structural_pointer_nested_decay_bounds_values[2][3] = {};

struct StructuralNestedDecayBoundsPointerKey {
    int* pointer;
};

template <StructuralNestedDecayBoundsPointerKey Value>
struct StructuralNestedDecayBoundsPointerOwner {};

StructuralNestedDecayBoundsPointerOwner<StructuralNestedDecayBoundsPointerKey{
    structural_pointer_nested_decay_bounds_values[0] + 4}> invalid;
