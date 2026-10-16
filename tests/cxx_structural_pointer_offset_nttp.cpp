int structural_pointer_offset_values[3] = {17, 29, 43};
int structural_pointer_offset_scalar = 59;

struct StructuralPointerOffsetKey {
    int* pointer;
};

template <StructuralPointerOffsetKey Value>
struct StructuralPointerOffsetOwner {
    static int get() {
        return *Value.pointer;
    }

    static int* address() {
        return Value.pointer;
    }
};

int main() {
    int first = StructuralPointerOffsetOwner<
        StructuralPointerOffsetKey{structural_pointer_offset_values}>::get();
    int first_element = StructuralPointerOffsetOwner<
        StructuralPointerOffsetKey{&structural_pointer_offset_values[0]}>::get();
    int second = StructuralPointerOffsetOwner<
        StructuralPointerOffsetKey{structural_pointer_offset_values + 1}>::get();
    int second_address = StructuralPointerOffsetOwner<
        StructuralPointerOffsetKey{&structural_pointer_offset_values[1]}>::get();
    int reverse_add = StructuralPointerOffsetOwner<
        StructuralPointerOffsetKey{1 + structural_pointer_offset_values}>::get();
    int third = StructuralPointerOffsetOwner<
        StructuralPointerOffsetKey{structural_pointer_offset_values + (3 - 1)}>::get();
    int chained_add = StructuralPointerOffsetOwner<
        StructuralPointerOffsetKey{structural_pointer_offset_values + 1 + 1}>::get();
    int* one_past = StructuralPointerOffsetOwner<
        StructuralPointerOffsetKey{&structural_pointer_offset_values[3]}>::address();
    int* scalar_one_past = StructuralPointerOffsetOwner<
        StructuralPointerOffsetKey{&structural_pointer_offset_scalar + 1}>::address();
    return first == 17 && first_element == 17 && second == 29 &&
        second_address == 29 && reverse_add == 29 && third == 43 &&
        chained_add == 43 &&
        one_past == structural_pointer_offset_values + 3 &&
        scalar_one_past == &structural_pointer_offset_scalar + 1 ? 0 : 1;
}
