struct StructuralIntegerToFloatingNarrowingKey {
    float ratio;
    int payload;
};

template <StructuralIntegerToFloatingNarrowingKey Value>
struct StructuralIntegerToFloatingNarrowingOwner {};

StructuralIntegerToFloatingNarrowingOwner<
    StructuralIntegerToFloatingNarrowingKey{16777217, 42}> invalid_narrowing;
