union SixDimensionalMixedExtentUnion {
    int values[1][2][2][1][2][1];
    int scalar;
};

template<SixDimensionalMixedExtentUnion Value>
constexpr int structural_six_dimensional_mixed_extent_value() {
    return Value.values[0][0][0][0][0][0] +
           2 * Value.values[0][0][0][0][1][0] +
           3 * Value.values[0][0][1][0][0][0] +
           4 * Value.values[0][0][1][0][1][0] +
           5 * Value.values[0][1][0][0][0][0] +
           6 * Value.values[0][1][0][0][1][0] +
           7 * Value.values[0][1][1][0][0][0] +
           8 * Value.values[0][1][1][0][1][0];
}

template<SixDimensionalMixedExtentUnion Value>
struct SixDimensionalMixedExtentTag {};

template<SixDimensionalMixedExtentUnion Value>
constexpr int structural_six_dimensional_mixed_extent_deduced(
    SixDimensionalMixedExtentTag<Value>) {
    return structural_six_dimensional_mixed_extent_value<Value>();
}

static_assert(structural_six_dimensional_mixed_extent_value<
                  SixDimensionalMixedExtentUnion{.values =
                      {{{{{{1}, {2}}}, {{{3}, {4}}}},
                       {{{{5}, {6}}}, {{{7}, {8}}}}}}}>() == 204);
static_assert(structural_six_dimensional_mixed_extent_deduced(
                  SixDimensionalMixedExtentTag<SixDimensionalMixedExtentUnion{
                      .values = {{{{{{8}, {9}}}, {{{10}, {11}}}},
                                 {{{{12}, {13}}}, {{{14}, {15}}}}}}}>{}) ==
              456);

int main() {
    return structural_six_dimensional_mixed_extent_value<
               SixDimensionalMixedExtentUnion{.values =
                   {{{{{{1}, {2}}}, {{{3}, {4}}}},
                    {{{{5}, {6}}}, {{{7}, {8}}}}}}}>() != 204 ||
           structural_six_dimensional_mixed_extent_deduced(
               SixDimensionalMixedExtentTag<SixDimensionalMixedExtentUnion{
                   .values = {{{{{{8}, {9}}}, {{{10}, {11}}}},
                              {{{{12}, {13}}}, {{{14}, {15}}}}}}}>{}) != 456;
}
