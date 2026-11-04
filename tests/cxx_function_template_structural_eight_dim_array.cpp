union EightDimensionalArrayUnion {
    int values[2][1][1][1][1][1][1][2];
    int scalar;
};

template<EightDimensionalArrayUnion Value>
constexpr int structural_eight_dimensional_array_value() {
    return Value.values[0][0][0][0][0][0][0][0] +
           2 * Value.values[0][0][0][0][0][0][0][1] +
           3 * Value.values[1][0][0][0][0][0][0][0] +
           4 * Value.values[1][0][0][0][0][0][0][1];
}

template<EightDimensionalArrayUnion Value>
struct EightDimensionalArrayTag {};

template<EightDimensionalArrayUnion Value>
constexpr int structural_eight_dimensional_array_deduced(
    EightDimensionalArrayTag<Value>) {
    return structural_eight_dimensional_array_value<Value>();
}

static_assert(structural_eight_dimensional_array_value<
                  EightDimensionalArrayUnion{
                      .values = {{{{{{{{1, 2}}}}}}}, {{{{{{{3, 4}}}}}}}}}>() == 30);
static_assert(structural_eight_dimensional_array_deduced(
                  EightDimensionalArrayTag<EightDimensionalArrayUnion{
                      {{{{{{{{5, 6}}}}}}}, {{{{{{{7, 8}}}}}}}}}>{}) == 70);

int main() {
    return structural_eight_dimensional_array_value<
               EightDimensionalArrayUnion{
                   .values = {{{{{{{{1, 2}}}}}}}, {{{{{{{3, 4}}}}}}}}}>() != 30 ||
           structural_eight_dimensional_array_deduced(
               EightDimensionalArrayTag<EightDimensionalArrayUnion{
                   {{{{{{{{5, 6}}}}}}}, {{{{{{{7, 8}}}}}}}}}>{}) != 70;
}