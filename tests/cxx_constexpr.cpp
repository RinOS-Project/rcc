/* Restricted integer constexpr folding coverage. */

constexpr int select_value(int value) {
    return value > 10 ? value * 2 + 3 : value + 1;
}

int constexpr_global = select_value(12);

int main(void) {
    return constexpr_global == 27 ? 0 : 1;
}
