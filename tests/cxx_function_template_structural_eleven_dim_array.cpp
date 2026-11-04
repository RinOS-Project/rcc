union ElevenDimensionalArrayUnion {
    int values[2][1][1][1][1][1][1][1][1][1][2];
    int scalar;
};

template<ElevenDimensionalArrayUnion Value>
constexpr int structural_eleven_dimensional_array_value() {
    return Value.values[0][0][0][0][0][0][0][0][0][0][0] +
           2 * Value.values[0][0][0][0][0][0][0][0][0][0][1] +
           3 * Value.values[1][0][0][0][0][0][0][0][0][0][0] +
           4 * Value.values[1][0][0][0][0][0][0][0][0][0][1];
}

template<ElevenDimensionalArrayUnion Value>
struct ElevenDimensionalArrayTag {};

template<ElevenDimensionalArrayUnion Value>
constexpr int structural_eleven_dimensional_array_deduced(
    ElevenDimensionalArrayTag<Value>) {
    return structural_eleven_dimensional_array_value<Value>();
}

static_assert(structural_eleven_dimensional_array_value<
                  ElevenDimensionalArrayUnion{
                      .values = {{{{{{{{{{{1, 2}}}}}}}}}}, {{{{{{{{{{3, 4}}}}}}}}}}}}>() == 30);
static_assert(structural_eleven_dimensional_array_deduced(
                  ElevenDimensionalArrayTag<ElevenDimensionalArrayUnion{
                      {{{{{{{{{{{5, 6}}}}}}}}}}, {{{{{{{{{{7, 8}}}}}}}}}}}}>{}) == 70);

int main() {
    return structural_eleven_dimensional_array_value<
               ElevenDimensionalArrayUnion{
                   .values = {{{{{{{{{{{1, 2}}}}}}}}}}, {{{{{{{{{{3, 4}}}}}}}}}}}}>() != 30 ||
           structural_eleven_dimensional_array_deduced(
               ElevenDimensionalArrayTag<ElevenDimensionalArrayUnion{
                   {{{{{{{{{{{5, 6}}}}}}}}}}, {{{{{{{{{{7, 8}}}}}}}}}}}}>{}) != 70;
}