int structural_pointer_nested_values[2][3] = {
    {11, 23, 37},
    {41, 53, 67},
};

struct StructuralNestedIntPointerKey {
    int* pointer;
};

template <StructuralNestedIntPointerKey Value>
struct StructuralNestedIntPointerOwner {
    static int get() {
        return *Value.pointer;
    }

    static int* address() {
        return Value.pointer;
    }
};

int main() {
    int row_first = StructuralNestedIntPointerOwner<
        StructuralNestedIntPointerKey{
            structural_pointer_nested_values[1]}>::get();
    int row_start = StructuralNestedIntPointerOwner<
        StructuralNestedIntPointerKey{
            structural_pointer_nested_values[1]}>::address() ==
        &structural_pointer_nested_values[1][0];
    int first = StructuralNestedIntPointerOwner<
        StructuralNestedIntPointerKey{
            &structural_pointer_nested_values[0][1]}>::get();
    int last = StructuralNestedIntPointerOwner<
        StructuralNestedIntPointerKey{
            &structural_pointer_nested_values[1][2]}>::get();
    int row_one_past = StructuralNestedIntPointerOwner<
        StructuralNestedIntPointerKey{
            &structural_pointer_nested_values[0][2] + 1}>::address() ==
        &structural_pointer_nested_values[0][3];
    int final_one_past = StructuralNestedIntPointerOwner<
        StructuralNestedIntPointerKey{
            &structural_pointer_nested_values[1][2] + 1}>::address() ==
        &structural_pointer_nested_values[1][3];
    int final_decay_one_past = StructuralNestedIntPointerOwner<
        StructuralNestedIntPointerKey{
            structural_pointer_nested_values[1] + 3}>::address() ==
        &structural_pointer_nested_values[1][3];
    return row_first == 41 && row_start && first == 23 && last == 67 &&
        row_one_past && final_one_past && final_decay_one_past ? 0 : 1;
}
