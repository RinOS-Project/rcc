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

constexpr int mutate_value(int value) {
    int result = value;
    result += 2;
    ++result;
    return result;
}

constexpr int sum_value(int value) {
    int total = 0;
    for (int index = 0; index < value; ++index) {
        total += index;
    }
    return total;
}

constexpr int while_value(int limit) {
    int index = 0;
    int total = 0;
    while (index < limit) {
        ++index;
        if (index == 2) continue;
        if (index == 5) break;
        total += index;
    }
    return total;
}

constexpr int do_value(int limit) {
    int index = 0;
    do {
        ++index;
    } while (index < limit);
    return index;
}

constexpr int constexpr_global = select_value(base_value);
constexpr int constexpr_local = local_value(base_value);
constexpr int constexpr_mutated = mutate_value(base_value);
constexpr int constexpr_loop = sum_value(5);
constexpr int constexpr_while = while_value(8);
constexpr int constexpr_do = do_value(5);

int main(void) {
    return constexpr_global == 27 && constexpr_local == 27 &&
                   constexpr_mutated == 15 && constexpr_loop == 10 &&
                   constexpr_while == 8 && constexpr_do == 5 ? 0 : 1;
}
