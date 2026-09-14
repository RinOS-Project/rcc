/* Scalar constexpr folding coverage. */

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

constexpr float scale_float(float value) {
    return value * 2.0f + 0.5f;
}

constexpr double half_integer(int value) {
    return static_cast<double>(value) / 2.0 + 0.25;
}

constexpr int truncate_float(float value) {
    return static_cast<int>(value);
}

constexpr float local_float(int value) {
    float result = static_cast<float>(value);
    result += 0.5f;
    if (result > 2.0f) result *= 2.0f;
    return result;
}

constexpr double loop_float(int limit) {
    double total = 0.0;
    for (int index = 0; index < limit; ++index) {
        total += 0.5;
    }
    return total;
}

constexpr int constexpr_global = select_value(base_value);
constexpr int constexpr_local = local_value(base_value);
constexpr int constexpr_mutated = mutate_value(base_value);
constexpr int constexpr_loop = sum_value(5);
constexpr int constexpr_while = while_value(8);
constexpr int constexpr_do = do_value(5);
constexpr float constexpr_float = scale_float(1.5f);
constexpr double constexpr_double = half_integer(7);
constexpr int constexpr_truncated = truncate_float(3.75f);
constexpr float constexpr_local_float = local_float(2);
constexpr double constexpr_loop_float = loop_float(4);

int main(void) {
    return constexpr_global == 27 && constexpr_local == 27 &&
                   constexpr_mutated == 15 && constexpr_loop == 10 &&
                   constexpr_while == 8 && constexpr_do == 5 &&
                   constexpr_float == 3.5f && constexpr_double == 3.75 &&
                   constexpr_truncated == 3 && constexpr_local_float == 5.0f &&
                   constexpr_loop_float == 2.0
               ? 0 : 1;
}
