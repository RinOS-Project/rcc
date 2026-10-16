struct StructuralMultiplePointerTarget {
    int first;
    int second;
};

struct StructuralMultiplePointerPair {
    int StructuralMultiplePointerTarget::* first;
    int StructuralMultiplePointerTarget::* second;
};

struct StructuralMultiplePointerKey {
    unsigned tag;
    StructuralMultiplePointerPair pointers;
};

template <StructuralMultiplePointerKey Value>
struct StructuralMultiplePointerOwner {
    static int select(const StructuralMultiplePointerTarget& target,
                      bool second) {
        return static_cast<int>(Value.tag) +
               (second ? target.*Value.pointers.second
                       : target.*Value.pointers.first);
    }
};

int main() {
    const StructuralMultiplePointerTarget target{20, 22};
    return StructuralMultiplePointerOwner<
               StructuralMultiplePointerKey{
                   0,
                   {&StructuralMultiplePointerTarget::first,
                    &StructuralMultiplePointerTarget::second}}>::select(
                target, false) == 20 &&
            StructuralMultiplePointerOwner<
                StructuralMultiplePointerKey{
                    0,
                    {&StructuralMultiplePointerTarget::first,
                     &StructuralMultiplePointerTarget::second}}>::select(
                target, true) == 22 &&
            StructuralMultiplePointerOwner<
                StructuralMultiplePointerKey{
                    0,
                    {&StructuralMultiplePointerTarget::second,
                     &StructuralMultiplePointerTarget::first}}>::select(
                target, false) == 22
        ? 0 : 1;
}
