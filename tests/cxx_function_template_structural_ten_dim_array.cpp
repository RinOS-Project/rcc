union TenDimensionalArrayUnion {
    int values[2][1][1][1][1][1][1][1][1][2];
    int scalar;
};

template<TenDimensionalArrayUnion Value>
constexpr int structural_ten_dimensional_array_value() {
    return Value.values[0][0][0][0][0][0][0][0][0][0] +
           2 * Value.values[0][0][0][0][0][0][0][0][0][1] +
           3 * Value.values[1][0][0][0][0][0][0][0][0][0] +
           4 * Value.values[1][0][0][0][0][0][0][0][0][1];
}

template<TenDimensionalArrayUnion Value>
struct TenDimensionalArrayTag {};

template<TenDimensionalArrayUnion Value>
constexpr int structural_ten_dimensional_array_deduced(
    TenDimensionalArrayTag<Value>) {
    return structural_ten_dimensional_array_value<Value>();
}

static_assert(structural_ten_dimensional_array_value<
                  TenDimensionalArrayUnion{
                      .values = {{{{{{{{{{1, 2}}}}}}}}}, {{{{{{{{{3, 4}}}}}}}}}}}>() == 30);
static_assert(structural_ten_dimensional_array_deduced(
                  TenDimensionalArrayTag<TenDimensionalArrayUnion{
                      {{{{{{{{{{5, 6}}}}}}}}}, {{{{{{{{{7, 8}}}}}}}}}}}>{}) == 70);

int main() {
    return structural_ten_dimensional_array_value<
               TenDimensionalArrayUnion{
                   .values = {{{{{{{{{{1, 2}}}}}}}}}, {{{{{{{{{3, 4}}}}}}}}}}}>() != 30 ||
           structural_ten_dimensional_array_deduced(
               TenDimensionalArrayTag<TenDimensionalArrayUnion{
                   {{{{{{{{{{5, 6}}}}}}}}}, {{{{{{{{{7, 8}}}}}}}}}}}>{}) != 70;
}