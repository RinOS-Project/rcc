struct StructuralFloatingNarrowingKey {
    float ratio;
    int payload;
};

template <StructuralFloatingNarrowingKey Value>
struct StructuralFloatingNarrowingOwner {};

StructuralFloatingNarrowingOwner<
    StructuralFloatingNarrowingKey{1.1, 42}> invalid_narrowing;
