struct StructuralBasePointerBase {
    int value;
};

struct StructuralBasePointerDerived : StructuralBasePointerBase {};

const StructuralBasePointerDerived structural_base_pointer_const_object{};

struct StructuralBasePointerKey {
    StructuralBasePointerBase* pointer;
};

template <StructuralBasePointerKey Value>
struct StructuralBasePointerOwner {};

StructuralBasePointerOwner<StructuralBasePointerKey{
    &structural_base_pointer_const_object}> invalid;
