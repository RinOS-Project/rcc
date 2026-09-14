/* Aggregate constexpr member and element access coverage. */

struct Pair {
    int first;
    int second;
};

struct Nested {
    Pair pair;
    int tail;
};

constexpr Pair pair{3, 4};
constexpr Nested nested{{5, 6}, 7};
constexpr int first_value = pair.first;
constexpr int second_value = pair.second + 1;
constexpr int nested_value = nested.pair.second + nested.tail;
constexpr int array_values[3] = {8, 9, 10};
constexpr int array_value = array_values[1] + array_values[2];

int main(void) {
    return first_value == 3 && second_value == 5 &&
                   nested_value == 13 && array_value == 19
               ? 0 : 1;
}
