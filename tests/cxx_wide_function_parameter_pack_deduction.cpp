constexpr int wide_function_tail_code(int value) {
    return value;
}

constexpr int wide_function_tail_code(double) {
    return 100;
}

constexpr int wide_function_tail_code(char) {
    return 200;
}

template <
    typename T0, typename T1, typename T2, typename T3,
    typename T4, typename T5, typename T6, typename T7,
    typename T8, typename T9, typename T10, typename T11,
    typename T12, typename T13, typename T14, typename T15,
    typename T16, typename T17, typename T18, typename T19,
    typename T20, typename T21, typename T22, typename T23,
    typename T24, typename T25, typename T26, typename T27,
    typename T28, typename T29, typename T30, typename T31,
    typename T32, typename T33, typename T34, typename T35,
    typename T36, typename T37, typename T38, typename T39,
    typename... Tail
>
int count_wide_function_tail(int seed, Tail... values) {
    return seed + (wide_function_tail_code(values) + ...);
}

int main() {
    return count_wide_function_tail<
        int, int, int, int, int, int, int, int,
        int, int, int, int, int, int, int, int,
        int, int, int, int, int, int, int, int,
        int, int, int, int, int, int, int, int,
        int, int, int, int, int, int, int, int
    >(10, 1, 2.0, 'x') == 311 ? 0 : 1;
}
