int constexpr_pointer_array_decay_values[3] = {7, 11, 13};

constexpr int* constexpr_pointer_array_begin =
    constexpr_pointer_array_decay_values;
constexpr int* constexpr_pointer_array_middle =
    constexpr_pointer_array_decay_values + 1;
constexpr int* constexpr_pointer_array_end =
    constexpr_pointer_array_decay_values + 3;

constexpr int constexpr_local_array_decay_value() {
    int values[3] = {17, 19, 23};
    int* begin = values;
    int* end = values + 3;
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
static_assert(constexpr_local_array_decay_value() == 22);

int main(void) {
    return constexpr_pointer_array_begin[0] == 7 &&
                   constexpr_pointer_array_middle[0] == 11 &&
                   constexpr_pointer_array_end -
                       constexpr_pointer_array_begin == 3 &&
                   constexpr_local_array_decay_value() == 22
               ? 0
               : 1;
}
