union SixDimensionalArrayUnion {
    int values[2][1][1][1][1][2];
    int scalar;
};

template<SixDimensionalArrayUnion Value>
constexpr int structural_six_dimensional_array_value() {
    return Value.values[0][0][0][0][0][0] +
           2 * Value.values[0][0][0][0][0][1] +
           3 * Value.values[1][0][0][0][0][0] +
           4 * Value.values[1][0][0][0][0][1];
}

template<SixDimensionalArrayUnion Value>
struct SixDimensionalArrayTag {};

template<SixDimensionalArrayUnion Value>
constexpr int structural_six_dimensional_array_deduced(
    SixDimensionalArrayTag<Value>) {
    return structural_six_dimensional_array_value<Value>();
}

static_assert(structural_six_dimensional_array_value<
                  SixDimensionalArrayUnion{
                      .values = {
                          {{{{{1, 2}}}}},
                          {{{{{3, 4}}}}}}}>() == 30);
static_assert(structural_six_dimensional_array_deduced(
                  SixDimensionalArrayTag<SixDimensionalArrayUnion{
                      {{{{{{5, 6}}}}},
                       {{{{{7, 8}}}}}}}>{}) == 70);

int main() {
    return structural_six_dimensional_array_value<
               SixDimensionalArrayUnion{
                   .values = {
                       {{{{{1, 2}}}}},
                       {{{{{3, 4}}}}}}}>() != 30 ||
           structural_six_dimensional_array_deduced(
               SixDimensionalArrayTag<SixDimensionalArrayUnion{
                   {{{{{{5, 6}}}}},
                    {{{{{7, 8}}}}}}}>{}) != 70;
}
