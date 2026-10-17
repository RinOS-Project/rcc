struct StructuralBasePointerBase {
    int value;
};

struct StructuralBasePointerLeft : StructuralBasePointerBase {};
struct StructuralBasePointerRight : StructuralBasePointerBase {};
struct StructuralBasePointerDerived :
    StructuralBasePointerLeft, StructuralBasePointerRight {};

StructuralBasePointerDerived structural_base_pointer_ambiguous_object;

struct StructuralBasePointerKey {
    const StructuralBasePointerBase* pointer;
};

template <StructuralBasePointerKey Value>
struct StructuralBasePointerOwner {};

StructuralBasePointerOwner<StructuralBasePointerKey{
    &structural_base_pointer_ambiguous_object}> invalid;
