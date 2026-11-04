union SevenDimensionalArrayUnion {
    int values[2][1][1][1][1][1][2];
    int scalar;
};

template<SevenDimensionalArrayUnion Value>
constexpr int structural_seven_dimensional_array_value() {
    return Value.values[0][0][0][0][0][0][0] +
           2 * Value.values[0][0][0][0][0][0][1] +
           3 * Value.values[1][0][0][0][0][0][0] +
           4 * Value.values[1][0][0][0][0][0][1];
}

template<SevenDimensionalArrayUnion Value>
struct SevenDimensionalArrayTag {};

template<SevenDimensionalArrayUnion Value>
constexpr int structural_seven_dimensional_array_deduced(
    SevenDimensionalArrayTag<Value>) {
    return structural_seven_dimensional_array_value<Value>();
}

static_assert(structural_seven_dimensional_array_value<
                  SevenDimensionalArrayUnion{
                      .values = {
                          {{{{{{1, 2}}}}}},
                          {{{{{{3, 4}}}}}}}}>() == 30);
static_assert(structural_seven_dimensional_array_deduced(
                  SevenDimensionalArrayTag<SevenDimensionalArrayUnion{
                      {{{{{{{5, 6}}}}}},
                       {{{{{{7, 8}}}}}}}}>{}) == 70);

int main() {
    return structural_seven_dimensional_array_value<
               SevenDimensionalArrayUnion{
                   .values = {
                       {{{{{{1, 2}}}}}},
                       {{{{{{3, 4}}}}}}}}>() != 30 ||
           structural_seven_dimensional_array_deduced(
               SevenDimensionalArrayTag<SevenDimensionalArrayUnion{
                   {{{{{{{5, 6}}}}}},
                    {{{{{{7, 8}}}}}}}}>{}) != 70;
}
