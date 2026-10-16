inline constexpr int structural_array_reference_left[3] = {13, 29, 41};
inline constexpr int structural_array_reference_right[3] = {17, 31, 47};

struct StructuralArrayReferenceKey {
    const int (&values)[3];
    unsigned int index;
};

template <StructuralArrayReferenceKey Value>
struct StructuralArrayReferenceOwner {
    static int get() {
        return Value.values[Value.index];
    }

    static const int* address() {
        return &Value.values[0];
    }
};

int main() {
    return StructuralArrayReferenceOwner<
               StructuralArrayReferenceKey{
                   structural_array_reference_left, 1}>::get() == 29 &&
            StructuralArrayReferenceOwner<
                StructuralArrayReferenceKey{
                    structural_array_reference_right, 2}>::get() == 47 &&
            StructuralArrayReferenceOwner<
                StructuralArrayReferenceKey{
                    structural_array_reference_left, 1}>::address() ==
                structural_array_reference_left &&
            StructuralArrayReferenceOwner<
                StructuralArrayReferenceKey{
                    structural_array_reference_right, 2}>::address() ==
                structural_array_reference_right
        ? 0 : 1;
}
