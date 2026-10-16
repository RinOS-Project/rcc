inline constexpr int structural_class_pointer_pack_left = 13;
inline constexpr int structural_class_pointer_pack_right = 29;

struct StructuralClassPointerPackValue {
    const int* pointer;
    int bias;
};

template <StructuralClassPointerPackValue... Values>
struct StructuralClassPointerPack {
    static constexpr int sum_values() {
        return ((*Values.pointer + Values.bias) + ... + 0);
    }

    template <typename Element>
    struct Inner {
        int result() const { return 11; }
    };
};

template <StructuralClassPointerPackValue... Values>
template <typename Element>
struct StructuralClassPointerPack<Values...>::Inner<Element*> {
    int result() const { return 47; }
};

int main() {
    StructuralClassPointerPack<
        StructuralClassPointerPackValue{
            &structural_class_pointer_pack_left, 2},
        StructuralClassPointerPackValue{
            &structural_class_pointer_pack_right, 3}>::Inner<int*> instance;
    return instance.result() == 47 &&
           StructuralClassPointerPack<
               StructuralClassPointerPackValue{
                   &structural_class_pointer_pack_left, 2},
               StructuralClassPointerPackValue{
                   &structural_class_pointer_pack_right, 3}>::sum_values() == 47
        ? 0 : 1;
}
