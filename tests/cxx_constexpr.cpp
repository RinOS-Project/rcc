/* Restricted integer constexpr folding coverage. */

constexpr int base_value = 12;

constexpr int select_value(int value) {
    return value > 10 ? value * 2 + 3 : value + 1;
}

constexpr int constexpr_global = select_value(base_value);

int main(void) {
    return constexpr_global == 27 ? 0 : 1;
}
