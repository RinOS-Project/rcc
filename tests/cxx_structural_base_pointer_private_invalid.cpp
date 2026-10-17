struct StructuralBasePointerBase {
    int value;
};

struct StructuralBasePointerDerived : private StructuralBasePointerBase {};

StructuralBasePointerDerived structural_base_pointer_private_object;

struct StructuralBasePointerKey {
    const StructuralBasePointerBase* pointer;
};

template <StructuralBasePointerKey Value>
struct StructuralBasePointerOwner {};

StructuralBasePointerOwner<StructuralBasePointerKey{
    &structural_base_pointer_private_object}> invalid;
