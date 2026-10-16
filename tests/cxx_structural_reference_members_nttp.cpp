struct StructuralReferenceTarget {
    int first;
    int second;
};

inline constexpr int structural_reference_direct_left = 5;
inline constexpr int structural_reference_direct_right = 7;
inline constexpr int structural_reference_other_direct_left = 23;
inline constexpr int structural_reference_other_direct_right = 29;
inline constexpr StructuralReferenceTarget structural_reference_left{
    11, 13};
inline constexpr StructuralReferenceTarget structural_reference_right{
    17, 19};

struct StructuralReferencePair {
    const int& first;
    const int& second;
};

struct StructuralReferenceKey {
    unsigned char tag;
    StructuralReferencePair direct_references;
    StructuralReferencePair subobject_references;
};

template <StructuralReferenceKey Value>
struct StructuralReferenceOwner {
    static int select(bool second) {
        return static_cast<int>(Value.tag) +
               (second ? Value.direct_references.second
                       : Value.direct_references.first) +
               100 * Value.subobject_references.first +
               1000 * Value.subobject_references.second;
    }
};

int main() {
    return StructuralReferenceOwner<StructuralReferenceKey{
               3,
               {structural_reference_direct_left,
                structural_reference_direct_right},
               {structural_reference_left.first,
                structural_reference_right.second}}>::select(false) == 20108 &&
            StructuralReferenceOwner<StructuralReferenceKey{
                3,
                {structural_reference_direct_left,
                 structural_reference_direct_right},
                {structural_reference_left.first,
                 structural_reference_right.second}}>::select(true) == 20110 &&
            StructuralReferenceOwner<StructuralReferenceKey{
                5,
                {structural_reference_other_direct_left,
                 structural_reference_other_direct_right},
                {structural_reference_right.first,
                 structural_reference_left.second}}>::select(false) == 14728 &&
            StructuralReferenceOwner<StructuralReferenceKey{
                5,
                {structural_reference_other_direct_left,
                 structural_reference_other_direct_right},
                {structural_reference_right.first,
                 structural_reference_left.second}}>::select(true) == 14734
        ? 0 : 1;
}
