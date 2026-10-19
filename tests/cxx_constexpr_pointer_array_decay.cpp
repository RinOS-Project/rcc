int constexpr_pointer_array_decay_values[3] = {7, 11, 13};
constexpr int constexpr_pointer_matrix[2][3] = {{2, 3, 5}, {7, 11, 13}};

constexpr int constexpr_pointer_callback_increment(int value) {
    return value + 1;
}

constexpr int* constexpr_pointer_array_begin =
    constexpr_pointer_array_decay_values;
constexpr int* constexpr_pointer_array_middle =
    constexpr_pointer_array_decay_values + 1;
constexpr int* constexpr_pointer_array_end =
    constexpr_pointer_array_decay_values + 3;
constexpr const int* constexpr_pointer_matrix_row = constexpr_pointer_matrix[1];
constexpr const int* constexpr_pointer_matrix_row_end =
    constexpr_pointer_matrix[1] + 3;
constexpr const int (*constexpr_pointer_matrix_rows)[3] =
    constexpr_pointer_matrix;
constexpr const int (*constexpr_pointer_matrix_rows_end)[3] =
    constexpr_pointer_matrix_rows + 2;
constexpr int (*constexpr_pointer_callback)(int) =
    constexpr_pointer_callback_increment;

constexpr int constexpr_local_array_decay_value() {
    int values[3] = {17, 19, 23};
    int* begin = values;
    int* end = values + 3;
    return begin[1] + static_cast<int>(end - begin);
}

constexpr int constexpr_local_matrix_row_decay_value() {
    int values[2][3] = {{2, 3, 5}, {17, 19, 23}};
    int* begin = values[1];
    int* end = values[1] + 3;
    return begin[1] + static_cast<int>(end - begin);
}

static_assert(constexpr_pointer_array_begin ==
              &constexpr_pointer_array_decay_values[0]);
static_assert(constexpr_pointer_array_middle ==
              &constexpr_pointer_array_decay_values[1]);
static_assert(constexpr_pointer_array_end ==
              &constexpr_pointer_array_decay_values[3]);
static_assert(constexpr_pointer_array_end -
                  constexpr_pointer_array_begin == 3);
static_assert(constexpr_pointer_matrix_row[2] == 13);
static_assert(constexpr_pointer_matrix_row_end -
                  constexpr_pointer_matrix_row == 3);
static_assert(constexpr_pointer_matrix_rows_end -
                  constexpr_pointer_matrix_rows == 2);
static_assert(constexpr_pointer_callback != nullptr);
static_assert(constexpr_local_array_decay_value() == 22);
static_assert(constexpr_local_matrix_row_decay_value() == 22);

int main(void) {
    return constexpr_pointer_array_begin[0] == 7 &&
                   constexpr_pointer_array_middle[0] == 11 &&
                   constexpr_pointer_array_end -
                       constexpr_pointer_array_begin == 3 &&
                   constexpr_pointer_matrix_rows[1][2] == 13 &&
                   constexpr_pointer_matrix_rows_end -
                       constexpr_pointer_matrix_rows == 2 &&
                   constexpr_pointer_callback(7) == 8 &&
                   constexpr_local_array_decay_value() == 22 &&
                   constexpr_local_matrix_row_decay_value() == 22
               ? 0
               : 1;
}
