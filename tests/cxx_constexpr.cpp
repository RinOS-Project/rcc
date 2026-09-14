/* Restricted integer constexpr folding coverage. */

constexpr int base_value = 12;

constexpr int select_value(int value) {
    return value > 10 ? value * 2 + 3 : value + 1;
}

constexpr int local_value(int value) {
    int adjusted = value + 1;
    if (adjusted > 10) return adjusted * 2 + 1;
    return adjusted;
}

constexpr int constexpr_global = select_value(base_value);
constexpr int constexpr_local = local_value(base_value);

int main(void) {
    return constexpr_global == 27 && constexpr_local == 27 ? 0 : 1;
}
