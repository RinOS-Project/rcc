union NineDimensionalArrayUnion {
    int values[2][1][1][1][1][1][1][1][2];
    int scalar;
};

template<NineDimensionalArrayUnion Value>
constexpr int structural_nine_dimensional_array_value() {
    return Value.values[0][0][0][0][0][0][0][0][0] +
           2 * Value.values[0][0][0][0][0][0][0][0][1] +
           3 * Value.values[1][0][0][0][0][0][0][0][0] +
           4 * Value.values[1][0][0][0][0][0][0][0][1];
}

template<NineDimensionalArrayUnion Value>
struct NineDimensionalArrayTag {};

template<NineDimensionalArrayUnion Value>
constexpr int structural_nine_dimensional_array_deduced(
    NineDimensionalArrayTag<Value>) {
    return structural_nine_dimensional_array_value<Value>();
}

static_assert(structural_nine_dimensional_array_value<
                  NineDimensionalArrayUnion{
                      .values = {{{{{{{{{1, 2}}}}}}}}, {{{{{{{{3, 4}}}}}}}}}}>() == 30);
static_assert(structural_nine_dimensional_array_deduced(
                  NineDimensionalArrayTag<NineDimensionalArrayUnion{
                      {{{{{{{{{5, 6}}}}}}}}, {{{{{{{{7, 8}}}}}}}}}}>{}) == 70);

int main() {
    return structural_nine_dimensional_array_value<
               NineDimensionalArrayUnion{
                   .values = {{{{{{{{{1, 2}}}}}}}}, {{{{{{{{3, 4}}}}}}}}}}>() != 30 ||
           structural_nine_dimensional_array_deduced(
               NineDimensionalArrayTag<NineDimensionalArrayUnion{
                   {{{{{{{{{5, 6}}}}}}}}, {{{{{{{{7, 8}}}}}}}}}}>{}) != 70;
}