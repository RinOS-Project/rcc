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

constexpr int mutate_pair() {
    Pair value{1, 2};
    value.first += 4;
    ++value.second;
    Pair copy = value;
    copy.second += 3;
    value = copy;
    return value.first * 10 + value.second;
}

constexpr int mutate_array() {
    int values[3] = {2, 4, 6};
    values[1] += 5;
    ++values[2];
    return values[0] + values[1] + values[2];
}

constexpr int mutated_pair_value = mutate_pair();
constexpr int mutated_array_value = mutate_array();

static_assert(pair.first == 3);
static_assert(nested.pair.second + nested.tail == 13);
static_assert(array_values[0] + array_values[1] == 17,
              "aggregate constexpr subscript evaluation");
static_assert(mutated_pair_value == 56);
static_assert(mutated_array_value == 18);

int main(void) {
        return first_value == 3 && second_value == 5 &&
                   nested_value == 13 && array_value == 19 &&
                   mutated_pair_value == 56 && mutated_array_value == 18
               ? 0 : 1;
}
