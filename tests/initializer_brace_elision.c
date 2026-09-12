struct Pair {
    int first;
    int second;
};

struct Nested {
    int values[2];
    int tail;
};

int matrix[2][2] = { 1, 2, 3, 4 };
struct Pair pairs[2] = { 5, 6, 7, 8 };
struct Nested global_nested = { 9, 10, 11 };

int check_brace_elision(void) {
    int local_matrix[2][2] = { 12, 13, 14, 15 };
    struct Pair local_pairs[2] = { 16, 17, 18, 19 };
    return matrix[1][0] + pairs[0].second + global_nested.tail +
           local_matrix[1][1] + local_pairs[1].first;
}
