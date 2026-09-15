/* Aggregate constexpr member and element access coverage. */

struct Pair {
    int first;
    int second;
};

struct Nested {
    Pair pair;
    int tail;
};

constexpr Pair make_pair(int first, int second) {
    return Pair{first, second};
}

constexpr Pair make_mutated_pair(int value) {
    Pair result{value, value + 1};
    result.first += 2;
    ++result.second;
    return result;
}

constexpr Pair pair{3, 4};
constexpr Nested nested{{5, 6}, 7};
constexpr Pair returned_pair = make_pair(11, 13);
constexpr Pair mutated_returned_pair = make_mutated_pair(20);
constexpr int first_value = pair.first;
constexpr int second_value = pair.second + 1;
constexpr int nested_value = nested.pair.second + nested.tail;
constexpr int returned_value = returned_pair.first + returned_pair.second;
constexpr int mutated_returned_value =
    mutated_returned_pair.first + mutated_returned_pair.second;
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
static_assert(returned_pair.first == 11 && returned_pair.second == 13);
static_assert(mutated_returned_pair.first == 22 &&
              mutated_returned_pair.second == 22);
static_assert(array_values[0] + array_values[1] == 17,
              "aggregate constexpr subscript evaluation");
static_assert(mutated_pair_value == 56);
static_assert(mutated_array_value == 18);

int main(void) {
    return first_value == 3 && second_value == 5 &&
                   nested_value == 13 && array_value == 19 &&
                   returned_value == 24 && mutated_returned_value == 44 &&
                   mutated_pair_value == 56 && mutated_array_value == 18
               ? 0 : 1;
}
